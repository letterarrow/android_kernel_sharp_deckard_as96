/* drivers/sharp/btpm/btpm.c  (BT Power Management)
 *
 * Copyright (C) 2009 SHARP CORPORATION
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
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <mach/../../board-8064.h>
#include <linux/uaccess.h>

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Definition

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
#define _BTPM_NAME_ "btpm"

#define disp_err( format, args... ) \
	printk( KERN_ERR "[%s] " format, _BTPM_NAME_ , ##args )

#ifdef BTPM_DEBUG
  #define disp_inf( format, args... ) \
	printk( KERN_ERR "[%s] " format, _BTPM_NAME_ , ##args )
#else
  #define disp_inf( format, args... ) 
#endif /* BTPM_DEBUG */

enum btpm_gpio_state {
	BTPM_GPIO_LOW,
	BTPM_GPIO_HIGH,
};

/* private data */
typedef struct {
	int bluetooth;								/* power status of bluetooth */
} btpm_data_t;

typedef enum {
	BTPM_STATE_OFF,
	BTPM_STATE_ON,
	BTPM_STATE_RESET,
	BTPM_STATE_SEM_RELEASE,
	BTPM_DIAG_STATE_OFF = 10,
	BTPM_DIAG_STATE_ON,
	BTPM_DIAG_STATE_RESET,
} eBTPM_STATE;

enum btpm_lock_state {
	BTPM_NOT_LOCK,
	BTPM_LOCK,
};

/* BT lock waiting timeout, in second */
#define BTLOCK_TIMEOUT	2

static struct semaphore btlock;
static int count = 1;
static int owner_cookie = -1;
int bcm_bt_lock(int cookie);
void bcm_bt_unlock(int cookie);

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Configuration

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
/* GPIO mapping for Wireless LAN device */
#define BTPM_HOST_WAKE				  5			/* BT_HOST_WAKE */
#define BTPM_DEV_WAKE				  6			/* BT_DEV_WAKE	*/
#define BTPM_REG_ON					 21			/* BT_REG_ON	*/
/* for GSBI5 */
#define BTPM_UART_TX_GSBI5			 51 		/* BT_UART_Tx	*/
#define BTPM_UART_RX_GSBI5			 52 		/* BT_UART_Rx	*/
#define BTPM_UART_CTS_GSBI5			 53 		/* BT_UART_CTS	*/
#define BTPM_UART_RTS_GSBI5			 54 		/* BT_UART_RTS	*/

/* GPIO Initial value */
static struct pm_gpio bt_reg_on = {
	.direction		= PM_GPIO_DIR_OUT,
	.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
	.output_value	= 0,
	.pull			= PM_GPIO_PULL_NO,
	.vin_sel		= PM_GPIO_VIN_S4,
	.out_strength	= PM_GPIO_STRENGTH_HIGH,
	.function		= PM_GPIO_FUNC_NORMAL,
	.inv_int_pol	= 0,
	.disable_pin	= 0,
};

static struct pm_gpio bt_host_wake = {
	.direction		= PM_GPIO_DIR_IN,
	.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
	.output_value	= 0,
	.pull			= PM_GPIO_PULL_DN,
	.vin_sel		= PM_GPIO_VIN_S4,
	.out_strength	= PM_GPIO_STRENGTH_NO,
	.function		= PM_GPIO_FUNC_NORMAL,
	.inv_int_pol	= 0,
	.disable_pin	= 0,
};

static struct pm_gpio bt_dev_wake = {
	.direction		= PM_GPIO_DIR_OUT,
	.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
	.output_value	= 0,
	.pull			= PM_GPIO_PULL_NO,
	.vin_sel		= PM_GPIO_VIN_S4,
	.out_strength	= PM_GPIO_STRENGTH_HIGH,
	.function		= PM_GPIO_FUNC_NORMAL,
	.inv_int_pol	= 0,
	.disable_pin	= 0,
};

/* GPIO TLMM: Function */
#define BTPM_FUNC_GPIO				0 			/* FUNC_SEL GPIO */
#define BTPM_FUNC_UART				2 			/* FUNC_SEL UART */

/* Port Configuration Table */
#define BTPM_POWER_NUM				2			/* 0 : BT OFF / 1 : BT ON */
#define BTPM_PORT_NUM				4			/* 0 : TX / 1 : RX / 2 : CTS / 3 : RTS */

static unsigned uart_gpio_config[BTPM_POWER_NUM][BTPM_PORT_NUM] = {
/*					  gpio,					func,			dir,				pull,				drvstr		*/
	{	/* [0][x] : BT OFF */
		GPIO_CFG( BTPM_UART_TX_GSBI5,	BTPM_FUNC_GPIO,	GPIO_CFG_INPUT,		GPIO_CFG_PULL_UP,	GPIO_CFG_6MA ),	/* [x][0] : TX  */
		GPIO_CFG( BTPM_UART_RX_GSBI5,	BTPM_FUNC_GPIO,	GPIO_CFG_INPUT,		GPIO_CFG_PULL_UP,	GPIO_CFG_2MA ),	/* [x][1] : RX  */
		GPIO_CFG( BTPM_UART_CTS_GSBI5,	BTPM_FUNC_GPIO,	GPIO_CFG_INPUT,		GPIO_CFG_PULL_UP,	GPIO_CFG_2MA ),	/* [x][2] : CTS */
		GPIO_CFG( BTPM_UART_RTS_GSBI5,	BTPM_FUNC_GPIO,	GPIO_CFG_INPUT,		GPIO_CFG_PULL_UP,	GPIO_CFG_6MA ) 	/* [x][3] : RTS */
	},
	{	/* [1][x] : BT ON */
		GPIO_CFG( BTPM_UART_TX_GSBI5,	BTPM_FUNC_UART,	GPIO_CFG_OUTPUT,	GPIO_CFG_NO_PULL,	GPIO_CFG_6MA ),	/* [x][0] : TX  */
		GPIO_CFG( BTPM_UART_RX_GSBI5,	BTPM_FUNC_UART,	GPIO_CFG_INPUT,		GPIO_CFG_PULL_UP,	GPIO_CFG_2MA ),	/* [x][1] : RX  */
		GPIO_CFG( BTPM_UART_CTS_GSBI5,	BTPM_FUNC_UART,	GPIO_CFG_INPUT,		GPIO_CFG_PULL_UP,	GPIO_CFG_2MA ),	/* [x][2] : CTS */
		GPIO_CFG( BTPM_UART_RTS_GSBI5,	BTPM_FUNC_UART,	GPIO_CFG_OUTPUT,	GPIO_CFG_NO_PULL,	GPIO_CFG_6MA ) 	/* [x][3] : RTS */
	}
};

#ifndef BTPM_INI_BT
	#define BTPM_INI_BT 0
#endif

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Resource

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
struct platform_device *p_btpm_dev = NULL;

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Local

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static void btpm_uart_gpio_config( int on )
{
	int pin;
	int result;

	/* UART Configuration */
	for ( pin = 0; pin < ARRAY_SIZE(uart_gpio_config[on]); pin++ ) {
		result = gpio_tlmm_config( uart_gpio_config[on][pin], GPIO_CFG_ENABLE );
		if ( result ) {
			disp_err( "error : gpio_tlmm_config( uart_gpio_config[%d][%d] )<-%#x : %d\n", on, pin, uart_gpio_config[on][pin], result );
		} else {
			disp_inf( "gpio_tlmm_config( uart_gpio_config[%d][%d] )<-%#x \n", on, pin, uart_gpio_config[on][pin] );
		}
	}
}

static int btpm_bluetooth_off( void )
{
	struct timespec tu;
	int ret;
	
	/* UART Configuration */
	btpm_uart_gpio_config( BTPM_STATE_OFF );

	gpio_set_value( PM8921_GPIO_PM_TO_SYS(BTPM_DEV_WAKE), BTPM_GPIO_HIGH );				/* BT_DEV_WAKE HIGH(SLEEP) */
	disp_inf( "%s: BT_DEV_WAKE HIGH\n" , __func__ );
	tu.tv_sec = ( time_t )0;
	tu.tv_nsec = 1000000;																/* over 1ms */
	hrtimer_nanosleep( &tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC );
	/* BC7 is always turned on */
	gpio_set_value( PM8921_GPIO_PM_TO_SYS( BTPM_REG_ON ), BTPM_GPIO_LOW );				/* BT_REG_ON LOW */
	disp_inf( "%s: BT_REG_ON LOW\n" , __func__ );
	gpio_set_value( PM8921_GPIO_PM_TO_SYS( BTPM_DEV_WAKE ), BTPM_GPIO_LOW );			/* BT_DEV_WAKE LOW(WAKE) */
	disp_inf( "%s: BT_DEV_WAKE LOW\n" , __func__ );

	/* BT WAKE Configuration */
	bt_host_wake.pull = PM_GPIO_PULL_DN;
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS( BTPM_HOST_WAKE ), &bt_host_wake );
	if ( ret ) {
		disp_err( "error : uart pm8xxx_gpio_config( BTPM_HOST_WAKE ) : %d\n", ret );
		return -EIO;
	}
	disp_inf( "%s: BT_HOST_WAKE PULL DOWN\n" , __func__ );

	return 0;
}

static int btpm_reset( struct device *pdev )
{
	btpm_data_t *p_sts;

	if ( pdev == NULL ){
		disp_err( "device not found\n" );
		return -1;
	}

	p_sts = ( btpm_data_t * )dev_get_drvdata( pdev );
	if ( p_sts == NULL ){
		disp_err( "driver infomation not found\n" );
		return -1;
	}

	disp_inf( "%s: HARDWARE START\n" , __func__ );
	btpm_bluetooth_off();
	disp_inf( "%s: HARDWARE END\n" , __func__ );

	/* init status */
	p_sts->bluetooth = 0;

	return 0;
}

static int btpm_power_control( struct device *pdev , int on , int lock )
{
	btpm_data_t *p_sts;
	struct timespec tu;
	int ret;
	int lock_cookie_bt = 'B' | 'T'<<8 | '3'<<16 | '5'<<24;								/* cookie is "BT35" */

	if ( pdev == NULL ){
		disp_err( "device not found\n" );
		return -1;
	}

	p_sts = (btpm_data_t *)dev_get_drvdata( pdev );
	if ( p_sts == NULL ){
		disp_err( "driver infomation not found\n" );
		return -1;
	}

	if ( p_sts->bluetooth == on ){
		disp_inf( "%s: no need to change status ( %d->%d )\n" , __func__, p_sts->bluetooth , on );
		return 0;
	}

	if ( on ){
		disp_inf( "%s: BT ON START\n" , __func__ );

		if (lock == BTPM_LOCK) {
			if (bcm_bt_lock(lock_cookie_bt) != 0) {
				disp_err("** BT AXI WAR timeout in acquiring bt lock**\n");
			} else {
				disp_inf("** AXI WAR locked **\n");
			}
		}

		/* BT WAKE Configuration */
		bt_host_wake.pull = PM_GPIO_PULL_NO;
		ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS(BTPM_HOST_WAKE), &bt_host_wake );
		if ( ret ) {
			disp_err( "error : uart pm8xxx_gpio_config(BTPM_HOST_WAKE) : %d\n", ret );
			return -EIO;
		}
		disp_inf( "%s: BT_HOST_WAKE NONE DOWN\n" , __func__ );

		/* Turn ON RF/WLAN_IO-3.0V */
		gpio_set_value( PM8921_GPIO_PM_TO_SYS( BTPM_REG_ON ), BTPM_GPIO_LOW );			/* BT_REG_ON LOW */
		disp_inf( "%s: BT_REG_ON LOW\n" , __func__ );
		/* BC7 is always turned on */
		tu.tv_sec = ( time_t )0;
		tu.tv_nsec = 2000000;															/* over 2ms */
		hrtimer_nanosleep( &tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC );
		gpio_set_value( PM8921_GPIO_PM_TO_SYS( BTPM_REG_ON ), BTPM_GPIO_HIGH );			/* BT_REG_ON HIGH */
		disp_inf( "%s: BT_REG_ON HIGH\n" , __func__ );
		gpio_set_value( PM8921_GPIO_PM_TO_SYS( BTPM_DEV_WAKE ), BTPM_GPIO_HIGH );		/* BT_DEV_WAKE HIGH(SLEEP) */
		disp_inf( "%s: BT_DEV_WAKE HIGH\n" , __func__ );
		tu.tv_sec = ( time_t )0;
		tu.tv_nsec = 20000000;															/* over 20ms */
		hrtimer_nanosleep( &tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC );
		gpio_set_value( PM8921_GPIO_PM_TO_SYS( BTPM_DEV_WAKE ), BTPM_GPIO_LOW );		/* BT_DEV_WAKE LOW(WAKE) */
		disp_inf( "%s: BT_DEV_WAKE LOW\n" , __func__ );
		tu.tv_sec = ( time_t )0;
		tu.tv_nsec = 200000000;															/* over 200ms */
		hrtimer_nanosleep( &tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC );

		/* UART Configuration */
		btpm_uart_gpio_config( BTPM_STATE_ON );
		disp_inf( "%s: BT ON END\n" , __func__ );
	} else {
		disp_inf( "%s: BT OFF START\n" , __func__ );
		btpm_bluetooth_off();
		disp_inf( "%s: BT OFF END\n" , __func__ );
	}

	if ( p_sts->bluetooth < 0 ){
		disp_inf( "Bluetooth power on reset\n" );
	} else {
		disp_inf( "%s: change status ( %d->%d )\n" , __func__, p_sts->bluetooth , on );
	}
	p_sts->bluetooth = on;

	return 0;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Global

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
int bcm_bt_lock(int cookie)
{
	int ret;
	char cookie_msg[5] = {0};

	ret = down_timeout(&btlock, msecs_to_jiffies(BTLOCK_TIMEOUT * 1000));
	if (ret == 0) {
		memcpy(cookie_msg, &cookie, sizeof(cookie));
		owner_cookie = cookie;
		count--;
		disp_inf("btlock acquired cookie: %s\n", cookie_msg);
	}
	return ret;
}

void bcm_bt_unlock(int cookie)
{
	char owner_msg[5] = {0};
	char cookie_msg[5] = {0};

	memcpy(cookie_msg, &cookie, sizeof(cookie));
	if (owner_cookie == cookie) {
		owner_cookie = -1;
		if (count++ > 1) {
			disp_inf("error, release a lock that was not acquired**\n");
		}
		up(&btlock);
		disp_inf("btlock released, cookie: %s\n", cookie_msg);
	} else {
		memcpy(owner_msg, &owner_cookie, sizeof(owner_cookie));
		disp_inf("ignore lock release,  cookie mismatch: %s owner %s \n", cookie_msg, 
				owner_cookie == 0 ? "NULL" : owner_msg);
	}
}


/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Device attribute

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static
ssize_t show_bluetooth_power( struct device *pdev, struct device_attribute *pattr, char *buf )
{
	btpm_data_t *p_priv = ( btpm_data_t * )dev_get_drvdata( pdev );

	buf[0] = ( char )( p_priv->bluetooth );
	
	return( 1 );
}

static
ssize_t set_bluetooth_power( struct device *pdev, struct device_attribute *pattr, const char *buf, size_t count )
{
	if ( ( buf[0] == BTPM_STATE_OFF ) || ( buf[0] == BTPM_STATE_ON ) ){
		btpm_power_control( pdev, ( int )buf[0], BTPM_LOCK );
		return( count );
	} else if ( ( buf[0] == BTPM_DIAG_STATE_OFF ) || ( buf[0] == BTPM_DIAG_STATE_ON ) ){
		btpm_power_control( pdev, ( int )buf[0], BTPM_NOT_LOCK );
		return( count );
	} else if ( ( buf[0] == BTPM_STATE_RESET ) || ( buf[0] == BTPM_DIAG_STATE_RESET ) ){
		btpm_reset( pdev );
		return( count );
	} else if ( buf[0] == BTPM_STATE_SEM_RELEASE ){
		int lock_cookie_bt = 'B' | 'T'<<8 | '3'<<16 | '5'<<24;							/* cookie is "BT35" */
		bcm_bt_unlock(lock_cookie_bt);
		return( count );
	}

	return( 0 );
}


/* device attribute structure */
static DEVICE_ATTR(
	bluetooth,
	( ( S_IRUGO | S_IWUGO ) & ( ~S_IWOTH ) ),	/* R:user/group/other W:user/group */
	show_bluetooth_power,						/* read */
	set_bluetooth_power							/* write */
);


static struct attribute *btpm_device_attributes[] = {
	&dev_attr_bluetooth.attr,
	NULL,
};

static struct attribute_group btpm_device_attributes_gourp = {
	.attrs = btpm_device_attributes,
};

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Driver Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int __init btpm_driver_probe( struct platform_device *pdev )
{
	int ret;
	btpm_data_t *p_priv;

	int ini;

	bt_reg_on.output_value = 0;
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS( BTPM_REG_ON ), &bt_reg_on );
	if ( ret ) {
		disp_err( "uart pm8xxx_gpio_config(BTPM_REG_ON) : %d\n", ret );
		return -EIO;
	}
	disp_inf( "%s: BT_REG_ON INIT\n" , __func__ );

	/* Initialize private data */
	p_priv = kmalloc( sizeof( *p_priv ) , GFP_KERNEL );
	if ( p_priv == NULL ){
		disp_err( "memory allocation for private data failed\n" );
		return -ENOMEM;
	}
	/* set drive data */
	platform_set_drvdata( pdev , p_priv );

	/* BT WAKE Configuration */
	bt_dev_wake.output_value = 0;
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS( BTPM_DEV_WAKE ), &bt_dev_wake );
	if ( ret ) {
		disp_err( "uart pm8xxx_gpio_config( BTPM_DEV_WAKE ) : %d\n", ret );
		return -EIO;
	}
	disp_inf( "%s: BT_DEV_WAKE INIT\n" , __func__ );

	bt_host_wake.pull = PM_GPIO_PULL_DN;
	ret = pm8xxx_gpio_config( PM8921_GPIO_PM_TO_SYS( BTPM_HOST_WAKE ), &bt_host_wake );
	if ( ret ) {
		disp_err( "uart pm8xxx_gpio_config( BTPM_HOST_WAKE ) : %d\n", ret );
		return -EIO;
	}
	disp_inf( "%s: BT_HOST_WAKE INIT\n" , __func__ );

	/* power on reset */
	btpm_reset( &( pdev->dev ) );

	/* power on */
	ini = BTPM_INI_BT;
	if ( ini ) {
		btpm_power_control( &( pdev->dev ), 1, BTPM_NOT_LOCK );
	}

	/* create sysfs interface */
	ret = sysfs_create_group( &( pdev->dev.kobj ), &btpm_device_attributes_gourp );
	if ( ret ){
		disp_err( "Sysfs attribute export failed with error %d.\n" , ret );
	}

	return ret;
}

static int btpm_driver_remove( struct platform_device *pdev )
{
	btpm_data_t *p_priv;

	sysfs_remove_group( &( pdev->dev.kobj ), &btpm_device_attributes_gourp );
	
	p_priv = platform_get_drvdata( pdev );
	platform_set_drvdata( pdev , NULL );
	if ( p_priv != NULL ){
		kfree( p_priv );
	}

	return 0;
}

static void btpm_driver_shutdown( struct platform_device *pdev )
{
	btpm_reset( &( pdev->dev ) );

	return;
}

/* driver structure */
static struct platform_driver btpm_driver = {
	.remove = __devexit_p( btpm_driver_remove ),
	.shutdown = __devexit_p( btpm_driver_shutdown ),
	.driver = {
		.name = _BTPM_NAME_,
		.owner = THIS_MODULE,
	},
};

static int btpm_driver_init( void )
{
	int ret;
	
	/* regist driver */
	ret = platform_driver_probe( &btpm_driver, btpm_driver_probe );
	if ( ret != 0 ){
		disp_err( "driver register failed ( %d )\n" , ret );
	}

	return ret;
}

static void btpm_driver_exit( void )
{
	platform_driver_unregister( &btpm_driver );
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Device Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int btpm_device_init( void )
{
	int ret;
	
	/* allocate device structure */
	p_btpm_dev = platform_device_alloc( _BTPM_NAME_ , -1 );
	if ( p_btpm_dev == NULL ){
		disp_err( "device allocation failed\n" );
		return -ENOMEM;
	}

	/* regist device */
	ret = platform_device_add( p_btpm_dev );
	if ( ret != 0 ){
		disp_err( "device register failed ( %d )\n" , ret );
	}

	return ret;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Module Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int __init btpm_module_init( void )
{
	int ret;
	
	disp_inf( "Bluetooth Power Management\n" );

	ret = btpm_device_init();
	if ( ret == 0 ){
		ret = btpm_driver_init();
	}

	sema_init(&btlock, 1);
	return ret;
}

static void __exit btpm_module_exit( void )
{
	btpm_driver_exit();
}


EXPORT_SYMBOL( btpm_power_control );

MODULE_DESCRIPTION( "Bluetooth Power Management" );
MODULE_LICENSE( "GPL v2" );
MODULE_AUTHOR( "SHARP CORPORATION" );
MODULE_VERSION( "0.10" );

module_init( btpm_module_init );
module_exit( btpm_module_exit );

