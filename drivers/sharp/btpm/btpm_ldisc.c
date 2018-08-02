/* drivers/sharp/btpm/btpm_ldisc.c  (Bluetooth Low Power Management)
 *
 * Copyright (C) 2012 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/ioctl.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include <asm/irq.h>
#include <asm/uaccess.h>

#include <mach/gpio.h>
#include <mach/../../board-8064.h>
#include <mach/msm_serial_hs.h>

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Definition

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
#define _BTPM_LDISC_NAME_ "btpm_ldisc"

#define disp_err( format, args... ) \
    printk( KERN_ERR "[%s] " format, _BTPM_LDISC_NAME_ , ##args )

#ifdef BTPM_LDISC_DEBUG
  #define disp_inf( format, args... ) \
    printk( KERN_ERR "[%s] " format, _BTPM_LDISC_NAME_ , ##args )
#else
  #define disp_inf( format, args... )
#endif /* BTPM_LDISC_DEBUG */

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Configuration

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
/* GPIO mapping */
#define BTPM_LDISC_HOST_WAKE 				5	 /* BT_HOST_WAKE */
#define BTPM_LDISC_DEV_WAKE					6	 /* BT_DEV_WAKE */

#define BTPM_LDISC_PM8921_HOST_WAKE 		PM8921_GPIO_PM_TO_SYS(BTPM_LDISC_HOST_WAKE)
#define BTPM_LDISC_PM8921_DEV_WAKE			PM8921_GPIO_PM_TO_SYS(BTPM_LDISC_DEV_WAKE)

/* Wake up irq */
#define BTPM_LDISC_WAKEUP_IRQ 				PM8921_GPIO_IRQ(PM8921_IRQ_BASE, BTPM_LDISC_HOST_WAKE)

/* Bluetooth ioctl command */
#define BTPM_LDISC_WAKE_ASSERT				(0x8003)
#define BTPM_LDISC_WAKE_DEASSERT 			(0x8004)
#define BTPM_LDISC_WAKE_GET_STATE			(0x8005)

/* status */
enum btpm_ldisc_gpio_state {
	BTPM_LDISC_GPIO_LOW,
	BTPM_LDISC_GPIO_HIGH,
};

enum btpm_ldisc_clk_state {
	BTPM_LDISC_CLK_OFF,
	BTPM_LDISC_CLK_ON,
};

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Resource

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int  (*alias_n_tty_ioctl)(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg);
static int  (*alias_n_tty_open)(struct tty_struct *tty);
static void (*alias_n_tty_close)(struct tty_struct *tty);

struct platform_device *p_btpm_ldisc_dev = NULL;


/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Local

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static irqreturn_t btpm_ldisc_wakeup_isr(int irq, void *dev)
{
	struct uart_port *uport = (struct uart_port *)dev;

	/* WAKE */
	if (gpio_get_value(BTPM_LDISC_PM8921_HOST_WAKE) == BTPM_LDISC_GPIO_LOW) {
		disp_inf("UART Clock ON from Bluetooth chip\n");
		msm_hs_request_clock_on(uport);
		msm_hs_set_mctrl(uport, TIOCM_RTS);
	}
	/* SLEEP */
	else {
		disp_inf("UART Clock OFF from Bluetooth chip (BT_DEV_WAKE:%s)\n", (gpio_get_value(BTPM_LDISC_PM8921_DEV_WAKE) ? "HIGH" : "LOW"));
		if (gpio_get_value(BTPM_LDISC_PM8921_DEV_WAKE) == BTPM_LDISC_GPIO_HIGH) {
			msm_hs_set_mctrl(uport, 0);
			msm_hs_request_clock_off(uport);
		}
	}
	return IRQ_HANDLED;
}

/* btpm_ldisc_tty_open()
 *    Process Open system call for the tty device.
 *
 * Arguments:
 *
 *    tty        pointer to tty instance data
 *
 * Return Value:    
 */
static int btpm_ldisc_tty_open(struct tty_struct *tty)
{
	int ret;

	struct uart_state *state = tty->driver_data;
	struct uart_port *uport = state->uart_port;

	ret = irq_set_irq_wake(BTPM_LDISC_WAKEUP_IRQ, 1);
	if (unlikely(ret)) {
		disp_err("Err setting btpm_ldisc wakeup_irq\n");
		return ret;
	}
	ret = request_threaded_irq(BTPM_LDISC_WAKEUP_IRQ, NULL,
				btpm_ldisc_wakeup_isr,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"btpm_ldisc_wakeup", uport);

	if (unlikely(ret)) {
		disp_err("Err getting btpm_ldisc wakeup_irq\n");
		irq_set_irq_wake(BTPM_LDISC_WAKEUP_IRQ, 0);
		return ret;
	}

	disp_inf("UART open (/dev/ttyHS%d) \n", uport->line);
	ret = alias_n_tty_open(tty);
	return ret;
}

/* btpm_ldisc_tty_close()
 *    Process Close system call for the tty device.
 *
 * Arguments:
 *
 *    tty        pointer to tty instance data
 *
 * Return Value:    
 */
static void btpm_ldisc_tty_close(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *uport = state->uart_port;

	irq_set_irq_wake(BTPM_LDISC_WAKEUP_IRQ, 0);
	free_irq(BTPM_LDISC_WAKEUP_IRQ, uport);

	disp_inf("UART close (/dev/ttyHS%d) \n", uport->line);
	alias_n_tty_close(tty);
	return ;
}

/* btpm_ldisc_tty_ioctl()
 *    Process IOCTL system call for the tty device.
 *
 * Arguments:
 *    tty        pointer to tty instance data
 *    file       pointer to open file object for device
 *    cmd        IOCTL command code
 *    arg        argument for IOCTL call (cmd dependent)
 *
 * Return Value:    Command dependent
 */
static int btpm_ldisc_tty_ioctl(struct tty_struct *tty, struct file * file,
								unsigned int cmd, unsigned long arg)
{
	int ret = -ENOIOCTLCMD;
	void __user *uarg = (void __user *)arg;
	unsigned int  cur_state = 0;

	struct uart_state *state = tty->driver_data;
	struct uart_port *uport = state->uart_port;

	switch (cmd) {
	case BTPM_LDISC_WAKE_ASSERT:
		disp_inf("UART Clock ON from Host\n");
		gpio_set_value( BTPM_LDISC_PM8921_DEV_WAKE, BTPM_LDISC_GPIO_LOW );	/* BT_DEV_WAKE LOW(WAKE) */
		msm_hs_request_clock_on(uport);
		msm_hs_set_mctrl(uport, TIOCM_RTS);
		ret = 0;
		break;

	case BTPM_LDISC_WAKE_DEASSERT:
		disp_inf("UART Clock OFF from Host (BT_HOST_WAKE:%s)\n", (gpio_get_value(BTPM_LDISC_PM8921_HOST_WAKE) ? "HIGH" : "LOW"));
		gpio_set_value(BTPM_LDISC_PM8921_DEV_WAKE, BTPM_LDISC_GPIO_HIGH);	/* BT_DEV_WAKE HIGH(SLEEP) */
		if (gpio_get_value(BTPM_LDISC_PM8921_HOST_WAKE) == BTPM_LDISC_GPIO_HIGH) {
			msm_hs_set_mctrl(uport, 0);
			msm_hs_request_clock_off(uport);
		}
		ret = 0;
		break;

	case BTPM_LDISC_WAKE_GET_STATE:
		if ((gpio_get_value(BTPM_LDISC_PM8921_DEV_WAKE ) == BTPM_LDISC_GPIO_HIGH)
		&&  (gpio_get_value(BTPM_LDISC_PM8921_HOST_WAKE) == BTPM_LDISC_GPIO_HIGH)) {
			cur_state = BTPM_LDISC_CLK_OFF;
		} else {
			cur_state = BTPM_LDISC_CLK_ON;
		}
		disp_inf("UART status is %s \n", cur_state ? " ON":"OFF");
		if (copy_to_user(uarg, &cur_state, sizeof(unsigned int *))) {
			ret = -EFAULT;
		} else {
			ret = 0;
		}
		break;

	default:
		ret = alias_n_tty_ioctl(tty, file, cmd, arg);
		break;
	};

	return ret;
}

static int btpm_ldisc_tty_init(void)
{
	static struct tty_ldisc_ops btpm_ldisc;
	int ret = 0;

	/* Register the tty discipline */

	memset(&btpm_ldisc, 0, sizeof (btpm_ldisc));

	/* Inherit the N_TTY's ops */
	n_tty_inherit_ops(&btpm_ldisc);

	/* Save N_TTY's ioctl() methods */
    alias_n_tty_open  = btpm_ldisc.open;
    alias_n_tty_close = btpm_ldisc.close;
	alias_n_tty_ioctl = btpm_ldisc.ioctl;

	btpm_ldisc.magic = TTY_LDISC_MAGIC;
	btpm_ldisc.name  = "n_btpm_hci";
    btpm_ldisc.open  = btpm_ldisc_tty_open;
    btpm_ldisc.close = btpm_ldisc_tty_close;
	btpm_ldisc.ioctl = btpm_ldisc_tty_ioctl;
	btpm_ldisc.owner = THIS_MODULE;

	disp_inf("btpm_ldisc is registered in tty\n");
	if ((ret = tty_register_ldisc(N_BTPM_HCI, &btpm_ldisc))) {
		disp_err("Bluetooth UART line discipline registration failed. (%d)\n", ret);
		return ret;
	}

	return ret;
}

static int btpm_ldisc_tty_exit(void)
{
	int ret = 0;

	/* Release tty registration of line discipline */
	disp_inf("btpm_ldisc is unregistered in tty\n");
	if ((ret = tty_unregister_ldisc(N_BTPM_HCI))) {
		disp_err("Can't unregister Bluetooth UART line discipline (%d)\n", ret);
	}

	return ret;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Driver Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int btpm_ldisc_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = btpm_ldisc_tty_init();

	return ret;
}

static int btpm_ldisc_remove(struct platform_device *pdev)
{
	int ret = 0;

	ret = btpm_ldisc_tty_exit();

	return ret;
}

static struct platform_driver btpm_ldisc_driver = {
	.remove = __devexit_p(btpm_ldisc_remove),
	.driver = {
			.name = _BTPM_LDISC_NAME_,
			.owner = THIS_MODULE,
			},
};

static int btpm_ldisc_driver_init(void)
{
	int ret = 0;

	/* regist driver */
	ret = platform_driver_probe(&btpm_ldisc_driver, btpm_ldisc_probe);
	if (ret != 0) {
		disp_err("driver register failed (%d)\n" , ret);
	}

	return ret;
}

static void btpm_ldisc_driver_exit(void)
{
	platform_driver_unregister(&btpm_ldisc_driver);
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Device Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int btpm_ldisc_device_init(void)
{
	int ret = 0;

	/* allocate device structure */
	p_btpm_ldisc_dev = platform_device_alloc(_BTPM_LDISC_NAME_ , -1);
	if (p_btpm_ldisc_dev == NULL) {
		disp_err("device allocation failed\n");
		return -ENOMEM;
	}

	/* regist device */
	ret = platform_device_add(p_btpm_ldisc_dev);
	if (ret != 0) {
		disp_err("device register failed (%d)\n" , ret);
	}

	return ret;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Module Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int __init btpm_ldisc_init(void)
{
	int ret = 0;

	disp_inf("BT Low Power support\n");
	ret = btpm_ldisc_device_init();
	if (ret == 0) {
		ret = btpm_ldisc_driver_init();
	}

	return ret;
}

static void __exit btpm_ldisc_exit(void)
{
	btpm_ldisc_driver_exit();
}

module_init(btpm_ldisc_init);
module_exit(btpm_ldisc_exit);

MODULE_DESCRIPTION("Bluetooth Low Power Management");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("0.10");
MODULE_ALIAS_LDISC(N_BTPM_HCI);
