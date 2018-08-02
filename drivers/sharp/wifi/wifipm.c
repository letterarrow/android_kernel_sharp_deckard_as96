
/* drivers/sharp/wifi/wifipm.c  (WiFi Power Management)
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <mach/pmic.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <mach/../../board-8064.h>

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Definition

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
#define _WIFIPM_NAME_	"wifipm"

#define disp_err( format, args... ) \
	printk( KERN_ERR "[%s] " format, _WIFIPM_NAME_ , ##args )
#define disp_war( format, args... ) \
	printk( KERN_WARNING "[%s] " format, _WIFIPM_NAME_ , ##args )

#ifdef WIFIPM_DEBUG
  #define disp_inf( format, args... ) \
	printk( KERN_ERR "[%s] " format, _WIFIPM_NAME_ , ##args )
  #define disp_dbg( format, args... ) \
	printk( KERN_ERR "[%s] " format, _WIFIPM_NAME_ , ##args )
  #define disp_trc( format, args... ) \
	printk( KERN_ERR "[%s] trace:%s " format, _WIFIPM_NAME_ , __func__ , ##args )
#else
  #define disp_inf( format, args... ) 
  #define disp_dbg( format, args... ) 
  #define disp_trc( format, args... )
#endif /* WIFIPM_DEBUG */

/* private data */
typedef struct {
	int active; /* power status of WiFi */
	int bcmsdh; /* init/cleanup bcmsdh */
} wifipm_data_t;

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Configuration

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
/* GPIO mapping for Wireless LAN device */
#define WIFIPM_PORT_WL_REG		18	      /* WLAN REG */

struct pm_gpio wl_reg_pm_gpio_cfg = {
    .direction = PM_GPIO_DIR_OUT,
    .output_buffer = PM_GPIO_OUT_BUF_CMOS,
    .output_value = 0,
    .pull = PM_GPIO_PULL_NO,
    .vin_sel = PM_GPIO_VIN_S4,
    .out_strength = PM_GPIO_STRENGTH_HIGH,
    .function = PM_GPIO_FUNC_NORMAL,
    .inv_int_pol = 0,
    .disable_pin = 0
};

/* for OOB port setting */
#define WIFIPM_PORT_WL_HOST_WAKE	3		/* WLAN int */

struct pm_gpio wl_host_wake_gpio_cfg = {
    .direction = PM_GPIO_DIR_IN,
    .output_buffer = PM_GPIO_OUT_BUF_CMOS,
    .output_value = 0,
    .pull = PM_GPIO_PULL_DN,
    .vin_sel = PM_GPIO_VIN_S4,
    .out_strength = PM_GPIO_STRENGTH_NO,
    .function = PM_GPIO_FUNC_NORMAL,
    .inv_int_pol = 0,
    .disable_pin = 0
};

#ifndef WIFIPM_INI
  #define WIFIPM_INI 0
#endif

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Proto type

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
#ifdef CONFIG_BTPM
int bcm_bt_lock(int cookie);
void bcm_bt_unlock(int cookie);
#endif

/* control bcmsdh from user space */
#ifdef SH_CONFIG_WLAN_STATIC
void sdio_function_cleanup(void);
int sdio_function_init(void);
#endif
/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Resource

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
struct platform_device *p_wifipm_dev = NULL;


/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Global

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
void wifipm_dev_reset( int reset ){

    if(reset){

	gpio_set_value(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_REG), 0);
        msleep(20);
        disp_dbg( "%s: Device Reset ON\n" , __func__);
    } else{
	gpio_set_value(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_REG), 1);
        msleep(110);
        disp_dbg( "%s: Device Reset OFF\n" , __func__);
    }

}


/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Local

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int wifipm_reset( struct device *pdev )
{
	wifipm_data_t *p_priv;

	if ( pdev == NULL ){
		disp_err( "device for wifipm not found\n" );
		return -1;
	}

	p_priv = (wifipm_data_t *)dev_get_drvdata(pdev);
	if ( p_priv == NULL ){
		disp_err( "driver infomation for wifipm not found\n" );
		return -1;
	}

	gpio_set_value(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_REG), 0);

	/* init status */
	p_priv->active = 0;
	p_priv->bcmsdh = 0; 

	return 0;
}



static int wifipm_power( struct device *pdev , int on )
{
	wifipm_data_t *p_priv;
	int ret;

	if ( pdev == NULL ){
		disp_err( "device for wifipm not found\n" );
		return -1;
	}

	p_priv = (wifipm_data_t *)dev_get_drvdata(pdev);
	if ( p_priv == NULL ){
		disp_err( "driver infomation for wifipm not found\n" );
		return -1;
	}

	if ( p_priv->active == on ){
		disp_dbg( "%s: no need to change status (%d->%d)\n" , __func__, p_priv->active , on );
		return 0;
	}

	if ( on ){

		/* for OOB port setting */
	    wl_host_wake_gpio_cfg.pull = PM_GPIO_PULL_NO;
	    ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_HOST_WAKE), &wl_host_wake_gpio_cfg);
	    if (ret) {
		disp_err( "pm8xxx_gpio_config: %d\n", ret);
		return -EIO;
	    }

	    /* WL_REG Enable */
	    gpio_set_value(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_REG), 1);
	    disp_dbg( "%s: WL_REG ON\n" , __func__);



	} else {

	    /* WL_REG Disable */
	    gpio_set_value(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_REG), 0);
	    disp_dbg( "%s: WL_REG OFF\n" , __func__);

		/* for OOB port setting */
	    wl_host_wake_gpio_cfg.pull = PM_GPIO_PULL_DN;
	    ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_HOST_WAKE), &wl_host_wake_gpio_cfg);
	    if (ret) {
		disp_err( "pm8xxx_gpio_config: %d\n", ret);
		return -EIO;
	    }
	}

	if ( p_priv->active < 0 ){
		disp_inf( "WiFi power on reset\n" );
	} else {
		disp_dbg( "%s: change status (%d->%d)\n" , __func__, p_priv->active , on );
	}

	p_priv->active = on;

	return 0;
}


/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Device attribute

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static
ssize_t show_wifi_power(struct device *pdev, struct device_attribute *pattr, char *buf)
{
	wifipm_data_t *p_priv = (wifipm_data_t *)dev_get_drvdata(pdev);
	int status;

	status = p_priv->active;
	
	return snprintf( buf, PAGE_SIZE, "%d\n" , status );
}

static
ssize_t set_wifi_power(struct device *pdev, struct device_attribute *pattr, const char *buf, size_t count)
{
	int new_status;

	sscanf( buf, "%1d", &new_status );

	if ( (new_status==0) || (new_status==1) ){
#ifdef CONFIG_BTPM
		int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24;	/* cookie is "WiFi" */
		disp_dbg("WiFi: trying to acquire BT lock\n");
		if (bcm_bt_lock(lock_cookie_wifi) != 0) {
			disp_err("** WiFi: timeout in acquiring bt lock**\n");
		}
		disp_dbg("%s: btlock acquired\n", __func__);
#endif
		wifipm_power( pdev, new_status );
#ifdef CONFIG_BTPM
		disp_dbg("%s: btlock release\n", __func__);
		bcm_bt_unlock(lock_cookie_wifi);
#endif
	}

	return count;
}



/* control bcmsdh from user space */
static
ssize_t ctl_bcmsdh(struct device *pdev, struct device_attribute *pattr, const char *buf, size_t count)
{
	int enable;
	wifipm_data_t *p_priv = (wifipm_data_t *)dev_get_drvdata(pdev);

	sscanf( buf, "%1d", &enable );

	if ( p_priv->bcmsdh == enable ){
		disp_err("%s: Forbid double process %d\n ", __FUNCTION__, enable);
		return count;
	}

	if ( enable == 1 ){
#ifdef SH_CONFIG_WLAN_STATIC
		sdio_function_init();
#endif
		p_priv->bcmsdh = enable;
	} else if (enable == 0 ){
		p_priv->bcmsdh = enable;
#ifdef SH_CONFIG_WLAN_STATIC
		sdio_function_cleanup();
#endif
	} else{
		/* Do nothing except 1 and 0   */
		disp_err("%s: Illegal State %d\n",__FUNCTION__, enable);
	}
	return count;

}


/* device attribute structure */
static DEVICE_ATTR(
	active,
	S_IRUGO | S_IWUGO,
	show_wifi_power,
	set_wifi_power
);


/* control bcmsdh from user space */
static DEVICE_ATTR(
	bcmsdh,
	S_IWUGO,
	NULL,
	ctl_bcmsdh
);


static struct attribute *wifipm_device_attributes[] = {
	&dev_attr_active.attr,
	&dev_attr_bcmsdh.attr,
	NULL,
};

static struct attribute_group wifipm_device_attributes_gourp = {
	.attrs = wifipm_device_attributes,
};

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Driver Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int __init wifipm_driver_probe( struct platform_device *pdev )
{
/*	int pin;*/
	int ret;
	wifipm_data_t *p_priv;

	int ini;

	/* Port Configuration */
	ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_REG), &wl_reg_pm_gpio_cfg);

	ret = gpio_request(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_HOST_WAKE), "wl_host_wake");	/* for OOB port setting */
	ret = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_HOST_WAKE), &wl_host_wake_gpio_cfg);	/* for OOB port setting */

	if (ret) {
	    disp_err( "pm8xxx_gpio_config: %d\n", ret);
	    return -EIO;
	}

	/* Initialize private data */
	p_priv = kmalloc( sizeof(*p_priv) , GFP_KERNEL );
	if ( p_priv == NULL ){
		disp_err( "memory allocation for private data failed\n" );
		return -ENOMEM;
	}
	platform_set_drvdata( pdev , p_priv );

	/* power on reset */
	wifipm_reset( &(pdev->dev) );

	/* power on */
	ini = WIFIPM_INI;
	if ( ini ){
		wifipm_power( &(pdev->dev) , 1 );
	}

	/* create sysfs interface */
	ret = sysfs_create_group( &(pdev->dev.kobj), &wifipm_device_attributes_gourp);
	if ( ret ){
		disp_err( "Sysfs attribute export failed with error %d.\n" , ret );
	}

	return ret;
}

static int wifipm_driver_remove( struct platform_device *pdev )
{
	wifipm_data_t *p_priv;

	sysfs_remove_group( &(pdev->dev.kobj), &wifipm_device_attributes_gourp);
	
	p_priv = platform_get_drvdata( pdev );
	platform_set_drvdata( pdev , NULL );
	if ( p_priv != NULL ){
		kfree( p_priv );
	}
	
	return 0;
}

static void wifipm_driver_shutdown( struct platform_device *pdev )
{
	wifipm_data_t *p_priv;
	p_priv = platform_get_drvdata( pdev );

	printk("%s:enter\n",__FUNCTION__);

	sysfs_remove_group( &(pdev->dev.kobj), &wifipm_device_attributes_gourp);

	if( p_priv->bcmsdh ){
		printk("%s:sdio_function_cleanup\n",__FUNCTION__);
		p_priv->bcmsdh = 0;
#ifdef SH_CONFIG_WLAN_STATIC
		sdio_function_cleanup();
#endif
	}
	printk("%s:wifipm_power\n",__FUNCTION__);
	wifipm_power( &(pdev->dev), 0 );

/* [WLAN][SHARP] 2012.12.25 for OOB setting Start */
	gpio_free(PM8921_GPIO_PM_TO_SYS(WIFIPM_PORT_WL_HOST_WAKE));	/* for OOB port setting */
/* [WLAN][SHARP] 2012.12.25 for OOB setting End */

	return;
}

/* driver structure */
static struct platform_driver wifipm_driver = {
	.remove = __devexit_p(wifipm_driver_remove),
	.shutdown = __devexit_p(wifipm_driver_shutdown),
	.driver = {
		.name = _WIFIPM_NAME_,
		.owner = THIS_MODULE,
	},
};

static int wifipm_driver_init( void )
{
	int ret;
	
	/* regist driver */
	ret = platform_driver_probe( &wifipm_driver, wifipm_driver_probe );
	if ( ret != 0 ){
		disp_err( "driver register failed (%d)\n" , ret );
	}

	return ret;
}

static void wifipm_driver_exit( void )
{
	platform_driver_unregister( &wifipm_driver );
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Device Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int wifipm_device_init( void )
{
	int ret;
	
	/* allocate device structure */
	p_wifipm_dev = platform_device_alloc( _WIFIPM_NAME_ , -1 );
	if ( p_wifipm_dev == NULL ){
		disp_err( "device allocation for wifipm failed\n" );
		return -ENOMEM;
	}

	/* regist device */
	ret = platform_device_add( p_wifipm_dev );
	if ( ret != 0 ){
		disp_err( "device register for wifipm failed (%d)\n" , ret );
	}

	return ret;
}

/*::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	Module Description

 *:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/
static int __init wifipm_module_init( void )
{
	int ret;
	
	disp_inf( "Wifi Power Management\n" );


	ret = wifipm_device_init();
	if ( ret == 0 ){
		ret = wifipm_driver_init();
	}

	return ret;
}

static void __exit wifipm_module_exit( void )
{
	wifipm_driver_exit();
}



EXPORT_SYMBOL(wifipm_dev_reset);

MODULE_DESCRIPTION("WiFi Power Management");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("0.10");

module_init(wifipm_module_init);
module_exit(wifipm_module_exit);

