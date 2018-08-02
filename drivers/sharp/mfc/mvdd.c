/* drivers/sharp/mfc/mvdd.c (NFC driver)
 *
 * Copyright (C) 2012-2013 SHARP CORPORATION
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

#ifdef CONFIG_SHFELICA

/***************header***************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include "mfc.h"

#include <linux/mfd/pm8xxx/pm8921.h>
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_MPP_BASE			(PM8921_GPIO_BASE + PM8921_NR_GPIOS)
#define PM8921_MPP_PM_TO_SYS(pm_mpp)	(pm_mpp - 1 + PM8921_MPP_BASE)
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + NR_GPIO_IRQS)

#define D_MVDD_ENABLE 			0x01
#define D_MVDD_DISABLE 			0x00

#define D_MVDD_DEVS 			(1)
#define D_MVDD_GPIO_NO 			61
#define D_MVDD_DEV_LOW 			(0)
#define D_MVDD_DEV_HIGH 		(1)
#define D_MVDD_DEV_NAME 		("snfc_mvdd")
#define D_MVDD_DELAY_MSEC 		(0)

/* SNFC Enable */
#define D_SNFC_EN_DEVS			(1)
#define D_SNFC_EN_DEV_NAME		("snfc_en")

/* for snfc_output_disable */
#define D_HSEL_GPIO_NO			PM8921_MPP_PM_TO_SYS(1)
#define D_INTU_GPIO_NO			PM8921_GPIO_PM_TO_SYS(20)
#define D_PON_GPIO_NO			(2)
#define D_RFS_GPIO_NO			(29)
#define D_INT_GPIO_NO			(26)
#define D_UART_TX_GPIO_NO		(22)
#define D_UART_RX_GPIO_NO		(23)

#define D_INTU_PULL_DN			(0)
#define D_INTU_PULL_UP			(1)

//#define D_MVDD_REF_INT
#ifdef D_MVDD_REF_INT
#define D_MVDD_DEV_FIX	 		(2)
#define D_INT_DEV_HIGH			(1)

#endif /* D_MVDD_REF_INT */
/*
 * prototype
 */
static __init int mvdd_init(void);
static void __exit mvdd_exit(void);

/*
 * global variable
 */
static struct class *mvdd_class = NULL;

#ifdef CONFIG_SHSNFC_BATTERY_FIXATION
static struct poll_data g_mvdd_data;
static struct poll_data *g_mvdd_d = &g_mvdd_data;
#else /* CONFIG_SHSNFC_BATTERY_FIXATION */
static struct cdev enable_cdev;
static int g_mvdd_enable = D_MVDD_DISABLE;
#endif /* CONFIG_SHSNFC_BATTERY_FIXATION */

static struct pm_gpio gpio_cfg = {
	.direction        = PM_GPIO_DIR_IN,
	.output_buffer    = PM_GPIO_OUT_BUF_CMOS,
	.output_value     = 0,
	.pull             = PM_GPIO_PULL_UP_31P5,
	.vin_sel          = PM_GPIO_VIN_S4,
	.out_strength     = PM_GPIO_STRENGTH_NO,
	.function         = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol      = 0,
	.disable_pin      = 0,
};

unsigned int snfc_available(void)
{
#ifdef CONFIG_SHSNFC_BATTERY_FIXATION

	unsigned int ret;
	ret = (g_mvdd_d->device_status != D_MVDD_DEV_LOW) ? D_MVDD_ENABLE : D_MVDD_DISABLE;

	return ret;

#else	//CONFIG_SHSNFC_BATTERY_FIXATION.

	return g_mvdd_enable;

#endif	//CONFIG_SHSNFC_BATTERY_FIXATION.
}


unsigned int snfc_available_wake_up(void)
{
#ifdef CONFIG_SHSNFC_BATTERY_FIXATION

	unsigned int ret;
#ifdef D_MVDD_REF_INT
	ret = ((g_mvdd_d->device_status == D_MVDD_DEV_FIX) || (gpio_get_value(D_MVDD_GPIO_NO) == D_MVDD_DEV_HIGH)) ? D_MVDD_ENABLE : D_MVDD_DISABLE;
#else
	ret = (gpio_get_value(D_MVDD_GPIO_NO) == D_MVDD_DEV_HIGH) ? D_MVDD_ENABLE : D_MVDD_DISABLE;
#endif /* D_MVDD_REF_INT */

	return ret;

#else	//CONFIG_SHSNFC_BATTERY_FIXATION.

	return g_mvdd_enable;

#endif	//CONFIG_SHSNFC_BATTERY_FIXATION.
}

#ifdef CONFIG_SHSNFC
static void set_intu_gpio_pull(int pull_type)
{
	int ret;
	
	gpio_cfg.pull = (pull_type == D_INTU_PULL_UP) ? PM_GPIO_PULL_UP_31P5 : PM_GPIO_PULL_DN;
	ret = pm8xxx_gpio_config(D_INTU_GPIO_NO, &gpio_cfg);
	if(ret) {
		MFC_DRV_ERR_LOG("pm8xxx_gpio_config ret = %d", ret);
	}
}
#endif /* CONFIG_SHSNFC */

static void snfc_output_disable(void)
{
	MFC_DRV_DBG_LOG("START");
	
	/* UART-TX */
	gpio_free(D_UART_TX_GPIO_NO);

	/* UART-RX */
	gpio_free(D_UART_RX_GPIO_NO);

	/* RFS */
	gpio_free(D_RFS_GPIO_NO);

	/* INT */
	gpio_free(D_INT_GPIO_NO);

#ifdef CONFIG_SHSNFC
	/* INTU */
	set_intu_gpio_pull(D_INTU_PULL_DN);

	/* HSEL */
	gpio_set_value_cansleep(D_HSEL_GPIO_NO, D_MVDD_DEV_LOW);
#endif /* CONFIG_SHSNFC */
	
	/* PON */
	gpio_set_value(D_PON_GPIO_NO, D_MVDD_DEV_LOW);

#ifndef CONFIG_SHSNFC_BATTERY_FIXATION
	g_mvdd_enable = D_MVDD_DISABLE;
#endif /* !CONFIG_SHSNFC_BATTERY_FIXATION */

	MFC_DRV_DBG_LOG("END");
}

static void snfc_output_enable(void)
{
	int ret;
	
	MFC_DRV_DBG_LOG("START");

	/* UART-TX */
	ret = gpio_request(D_UART_TX_GPIO_NO,"UART_TX_GPIO request");
	if(ret) {
		MFC_DRV_ERR_LOG("UART_TX_GPIO ret = %d", ret);
	}
	
	/* UART-RX */
	ret = gpio_request(D_UART_RX_GPIO_NO,"UART_RX_GPIO request");
	if(ret) {
		MFC_DRV_ERR_LOG("UART_RX_GPIO ret = %d", ret);
	}

	/* RFS */
	ret = gpio_request(D_RFS_GPIO_NO,"RFS_GPIO request");
	if(ret) {
		MFC_DRV_ERR_LOG("RFS_GPIO ret = %d", ret);
	}

	/* INT */
	ret = gpio_request(D_INT_GPIO_NO,"INT_GPIO request");
	if(ret) {
		MFC_DRV_ERR_LOG("INT_GPIO ret = %d", ret);
	}

#ifdef CONFIG_SHSNFC
	/* INTU */
	set_intu_gpio_pull(D_INTU_PULL_UP);
#endif /* CONFIG_SHSNFC */

#ifndef CONFIG_SHSNFC_BATTERY_FIXATION
	g_mvdd_enable = D_MVDD_ENABLE;
#endif /* !CONFIG_SHSNFC_BATTERY_FIXATION */

	MFC_DRV_DBG_LOG("END");
}

#ifdef CONFIG_SHSNFC_BATTERY_FIXATION
/*
 * function_mvdd_irq
 */
static void mvdd_work_func(struct work_struct *work)
{
	struct poll_data *mvdd_d = g_mvdd_d;
	int read_value = 0, old_value = 0;
	unsigned long irqflag = 0;
	
	MFC_DRV_DBG_LOG("START");
	
	old_value = mvdd_d->device_status;
	read_value = gpio_get_value(D_MVDD_GPIO_NO);
	
	MFC_DRV_DBG_LOG("read_value = %d, old_value = %d", read_value, old_value);

	/* read error */
	if (read_value < 0) {
		mvdd_d->read_error = read_value;
	} else if (read_value != old_value) {

		if (read_value == D_MVDD_DEV_LOW) {
			snfc_output_disable();
			mvdd_d->device_status = read_value;
			mvdd_d->read_error = 0;
			irqflag = IRQF_TRIGGER_HIGH;
			
		} else {
			msleep(100);
			
			read_value = gpio_get_value(D_MVDD_GPIO_NO);
			if(read_value == D_MVDD_DEV_HIGH) {
				snfc_output_enable();
			}
			mvdd_d->device_status = read_value;
			mvdd_d->read_error = 0;
			irqflag = (read_value == D_MVDD_DEV_HIGH) ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH;
		}

		irqflag |= IRQF_SHARED;
		if (irq_set_irq_type(gpio_to_irq(D_MVDD_GPIO_NO), irqflag))
			MFC_DRV_ERR_LOG("set_irq_type irqflag = %ld", irqflag);

	}

	/* enable irq handler */
	enable_irq(gpio_to_irq(D_MVDD_GPIO_NO));

	MFC_DRV_DBG_LOG("END read_value = %d, old_value = %d, mvdd_d->read_error = %d"
					, read_value, old_value, mvdd_d->read_error);
}


static irqreturn_t mvdd_irq_handler(int irq, void *dev_id)
{	
	MFC_DRV_DBG_LOG("START irq = %d", irq);
	
	disable_irq_nosync(gpio_to_irq(D_MVDD_GPIO_NO));
	/* set workqueue */
	schedule_delayed_work(&g_mvdd_d->work, msecs_to_jiffies(D_MVDD_DELAY_MSEC));
	
	MFC_DRV_DBG_LOG("END");
	
	return IRQ_HANDLED;
}

static int mvdd_irq_init(void)
{
	int ret = 0;
	struct poll_data *mvdd_d = g_mvdd_d;
	unsigned long irqflag = 0;
	
	MFC_DRV_DBG_LOG("START");
	
	/* initialize poll_data */
	memset(g_mvdd_d, 0x00, sizeof(struct poll_data));
	/* initialize workqueue */
	INIT_DELAYED_WORK(&g_mvdd_d->work, mvdd_work_func);
	/* initialize waitqueue */
	init_waitqueue_head(&g_mvdd_d->read_wait);

	ret = gpio_get_value(D_MVDD_GPIO_NO);
	if (ret < 0) {
		MFC_DRV_ERR_LOG("gpio_get_value ret = %d", ret);
		return -EIO;
	}

	MFC_DRV_DBG_LOG("MVDD gpio_get_value[%u] = %d", D_MVDD_GPIO_NO, ret);

	mvdd_d->device_status = ret;

#ifdef D_MVDD_REF_INT
	if (mvdd_d->device_status == D_MVDD_DEV_LOW) {
		ret = gpio_get_value(D_INT_GPIO_NO);
		if (ret == D_INT_DEV_HIGH) {
			mvdd_d->device_status = D_MVDD_DEV_FIX;
		}
	}
#endif /* D_MVDD_REF_INT */

	irqflag = (mvdd_d->device_status == D_MVDD_DEV_HIGH) ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH;
	irqflag |= IRQF_SHARED;

	if (request_irq(gpio_to_irq(D_MVDD_GPIO_NO),
	                mvdd_irq_handler,
	                irqflag,
	                D_MVDD_DEV_NAME,
	                (void*)mvdd_d)) {

		MFC_DRV_ERR_LOG("request_irq irqflag = %ld", irqflag);
		return -EIO;
	}
		
	if(enable_irq_wake(gpio_to_irq(D_MVDD_GPIO_NO))) {
		MFC_DRV_ERR_LOG("enable_irq_wake");
		free_irq(gpio_to_irq(D_MVDD_GPIO_NO), (void *)mvdd_d);
		return -EIO;
	}
	mvdd_d->irq_handler_done = 0;
	mvdd_d->open_flag = 1;

	ret = gpio_request(D_INTU_GPIO_NO, "mvdd_intu");
	if(ret) {
		MFC_DRV_ERR_LOG("gpio_request ret = %d", ret);
	}

	if(mvdd_d->device_status != D_MVDD_DEV_LOW) {
		snfc_output_enable();
	}

	MFC_DRV_DBG_LOG("END");
	
	return 0;
}

static void mvdd_irq_exit(void)
{
	struct poll_data *mvdd_d = g_mvdd_d;
	
	MFC_DRV_DBG_LOG("START");
	
	/* clear workqueue */
	cancel_delayed_work(&mvdd_d->work);
	
	if(mvdd_d->open_flag) {
		if(disable_irq_wake(gpio_to_irq(D_MVDD_GPIO_NO)))
			MFC_DRV_ERR_LOG("disable_irq_wake");
		
		free_irq(gpio_to_irq(D_MVDD_GPIO_NO), (void *)mvdd_d);
	}
	
	gpio_free(D_INTU_GPIO_NO);
	
	MFC_DRV_DBG_LOG("END");
}

#else /* CONFIG_SHSNFC_BATTERY_FIXATION */
/*
 * function_enable
 */
static ssize_t enable_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	char on[2];

	MFC_DRV_DBG_LOG("START");

	/* length check */
	if (len < 1) {
		MFC_DRV_ERR_LOG("length check len = %d", len);
		return -EIO;
	}

	on[0] = snfc_available();
	on[1] = 0x00;

	if (len > 2) {
		len = 2;
	}

	if (copy_to_user(buf, on, len)) {
		MFC_DRV_ERR_LOG("copy_to_user");
		return -EFAULT;
	}

	MFC_DRV_DBG_LOG("END on = %d, len = %d", on[0], len);

	return len;
}

static ssize_t enable_write(struct file *filp, const char __user *data, size_t len, loff_t *ppos)
{
	char on;

	MFC_DRV_DBG_LOG("START");

	/* length check */
	if (len < 1) {
		MFC_DRV_ERR_LOG("length check len = %d", len);
		return -EIO;
	}

	if (copy_from_user(&on, data, 1)) {
		MFC_DRV_ERR_LOG("copy_from_user");
		return -EFAULT;
	}

	if (on == g_mvdd_enable) {
		MFC_DRV_DBG_LOG("g_mvdd_enable equals to on, do nothing");
	} else if (on == D_MVDD_ENABLE) {
		snfc_output_enable();
	} else if (on == D_MVDD_DISABLE) {
		snfc_output_disable();
	} else {
		MFC_DRV_ERR_LOG("on = %d", on);
		return -EFAULT;
	}

	MFC_DRV_DBG_LOG("END on = %d g_mvdd_enable = %d", on, g_mvdd_enable);

	return len;
}

static int enable_open(struct inode *inode, struct file *file)
{
	MFC_DRV_DBG_LOG("");
	return 0;
}

static int enable_release(struct inode *inode, struct file *file)
{
	MFC_DRV_DBG_LOG("");
	return 0;
}

static const struct file_operations enable_fileops = {
	.owner   = THIS_MODULE,
	.write   = enable_write,
	.read    = enable_read,
	.open    = enable_open,
	.release = enable_release,
};

static int enable_init(void)
{
	int ret = 0;
	struct device *class_dev;

	dev_t dev = MKDEV(MISC_MAJOR, 0);

	MFC_DRV_DBG_LOG("START");

	ret = alloc_chrdev_region(&dev, 0, D_SNFC_EN_DEVS, D_SNFC_EN_DEV_NAME);
	if (ret) {
		MFC_DRV_ERR_LOG("alloc_chrdev_region ret = %d", ret);
		return ret;
	}

	cdev_init(&enable_cdev, &enable_fileops);
	enable_cdev.owner = THIS_MODULE;

	ret = cdev_add(&enable_cdev, dev, D_SNFC_EN_DEVS);
	if (ret) {
		unregister_chrdev_region(dev, D_SNFC_EN_DEVS);
		MFC_DRV_ERR_LOG("cdev_add ret = %d", ret);
		return ret;
	}

	class_dev = device_create(mvdd_class, NULL, dev, NULL, D_SNFC_EN_DEV_NAME);
	if (IS_ERR(class_dev)) {
		cdev_del(&enable_cdev);
		unregister_chrdev_region(dev, D_SNFC_EN_DEVS);
		ret = PTR_ERR(class_dev);
		MFC_DRV_ERR_LOG("device_create ret = %d", ret);
		return ret;
	}

	MFC_DRV_DBG_LOG("END");

	return ret;
}

static void enable_exit(void)
{
	dev_t dev = MKDEV(MISC_MAJOR, 0);

	MFC_DRV_DBG_LOG("START");

	cdev_del(&enable_cdev);
	unregister_chrdev_region(dev, D_SNFC_EN_DEVS);

	MFC_DRV_DBG_LOG("END");
}
#endif /* CONFIG_SHSNFC_BATTERY_FIXATION */

/*
 * mvdd_init
 */
static __init int mvdd_init(void)
{
	int ret;

	mvdd_class = class_create(THIS_MODULE, "mvdd");

#ifdef CONFIG_SHSNFC_BATTERY_FIXATION
	ret = mvdd_irq_init();
	if( ret < 0 )
		return ret;
#else /* CONFIG_SHSNFC_BATTERY_FIXATION */
	ret = enable_init();
	if( ret < 0 )
		return ret;
#endif /* CONFIG_SHSNFC_BATTERY_FIXATION */

	MFC_DRV_DBG_LOG("END");
	
	return 0;
}

/*
 * mvdd_exit
 */
static void __exit mvdd_exit(void)
{
	class_destroy(mvdd_class);

#ifdef CONFIG_SHSNFC_BATTERY_FIXATION
	mvdd_irq_exit();
#else /* CONFIG_SHSNFC_BATTERY_FIXATION */
	enable_exit();
#endif /* CONFIG_SHSNFC_BATTERY_FIXATION */
}

MODULE_LICENSE("GPL v2");

module_init(mvdd_init);
module_exit(mvdd_exit);

#endif	//CONFIG_SHFELICA

