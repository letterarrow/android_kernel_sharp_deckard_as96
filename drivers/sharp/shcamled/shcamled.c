/* drivers/sharp/shcamled/shcamled.c
 *
 * Copyright (C) 2011 SHARP CORPORATION
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

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/leds-pm8xxx.h>
#include "linux/leds.h"
#include <sharp/tps_driver.h>

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
int shcamled_pmic_set_torch_led_1_current(unsigned mA);
static int shcamled_pmic_set_torch_led_2_current(unsigned mA);
static ssize_t shcamled_torch_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size);
static int shcamled_torch_probe(struct platform_device *pdev);
static int shcamled_torch_remove(struct platform_device *pdev);

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define FALSE 0
#define TRUE 1

#define SHCAMLED_TORCH_DRV_NAME	"shcamled_torch"
#define SHCAMLED_TORCH_PERMISSION	(S_IRUSR | S_IWUSR) | (S_IRGRP | S_IWGRP)

#define SHCAMLED_FULL_MA		160

#define SHCAMLED_TORCH_MA		 40
#if defined(CONFIG_MACH_DECKARD_AS96)
#define SHCAMLED_FLASH_MA		140
#else
#define SHCAMLED_FLASH_MA		160
#endif

//#define SHCAMLED_ENABLE_DEBUG

#ifdef SHCAMLED_ENABLE_DEBUG
#define SHCAMLED_INFO(x...) 	printk(KERN_INFO "[SHCAMLED] " x)
#define SHCAMLED_TRACE(x...)	printk(KERN_DEBUG "[SHCAMLED] " x)
#define SHCAMLED_ERROR(x...) 	printk(KERN_ERR "[SHCAMLED] " x)
#else
#define SHCAMLED_INFO(x...)		do {} while(0)
#define SHCAMLED_TRACE(x...)	do {} while(0)
#define SHCAMLED_ERROR(x...)	printk(KERN_ERR "[SHCAMLED] " x)
#endif

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static struct platform_device shcamled_torch_dev = {
	.name   = "shcamled_torch",
};

static struct platform_driver shcamled_torch_driver = {
	.probe = shcamled_torch_probe,
	.remove = shcamled_torch_remove,
	.shutdown = NULL,
	.driver = {
		.name = SHCAMLED_TORCH_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static DEVICE_ATTR(shcamled_torch, SHCAMLED_TORCH_PERMISSION, NULL, shcamled_torch_store);

/* LED trigger (declare cam_led_x_trigger) */
DEFINE_LED_TRIGGER(cam_torch_led_1_trigger);

/* mutex */
static DEFINE_MUTEX(shcamled_torch_mut);

/* user info */
static int shcamled_use_torch_led_cam = FALSE;
static int shcamled_use_torch_led_comm = FALSE;
static unsigned shcamled_use_torch_led_mA = 0;

/* ------------------------------------------------------------------------- */
/* CODE                                                                      */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* shcamled_pmic_set_torch_led_1_current                                     */
/* ------------------------------------------------------------------------- */
int shcamled_pmic_set_torch_led_1_current(unsigned mA)
{
	int rc = 0;
	mutex_lock(&shcamled_torch_mut);

	SHCAMLED_TRACE("%s in mA:%d ucam:%d ucom:%d\n", __FUNCTION__, mA,
				   shcamled_use_torch_led_cam, shcamled_use_torch_led_comm);

	if(mA == 0){
		if((shcamled_use_torch_led_cam == FALSE && shcamled_use_torch_led_comm == TRUE) ||
		   (shcamled_use_torch_led_cam == TRUE && shcamled_use_torch_led_comm == FALSE)) {
			/* request turning off */
			led_trigger_event(cam_torch_led_1_trigger, LED_OFF);
			#if defined(CONFIG_TPS_DRIVER)
			tps_driver_unregister_user(TPS_DRIVER_USER_CAMLED);
			#endif /* defined(CONFIG_TPS_DRIVER) */
			shcamled_use_torch_led_cam = FALSE;
		}
	} else {
		if(shcamled_use_torch_led_cam == FALSE && shcamled_use_torch_led_comm == FALSE) {
			shcamled_use_torch_led_mA = mA;
			/* request turning on */
			#if defined(CONFIG_TPS_DRIVER)
			rc = tps_driver_register_user(TPS_DRIVER_USER_CAMLED);
			#endif /* defined(CONFIG_TPS_DRIVER) */
			if (rc == 0) {
				led_trigger_event(cam_torch_led_1_trigger, (mA * LED_FULL / SHCAMLED_FULL_MA));
				shcamled_use_torch_led_cam = TRUE;
			}
		} else if(shcamled_use_torch_led_cam == FALSE && shcamled_use_torch_led_comm == TRUE) {
			/* change user info only */
			shcamled_use_torch_led_comm = FALSE;
			shcamled_use_torch_led_cam = TRUE;
		}
	}

	SHCAMLED_TRACE("%s done ucam:%d ucom:%d\n", __FUNCTION__,
				   shcamled_use_torch_led_cam, shcamled_use_torch_led_comm);

	mutex_unlock(&shcamled_torch_mut);

	return 0;
}
EXPORT_SYMBOL(shcamled_pmic_set_torch_led_1_current);

/* ------------------------------------------------------------------------- */
/* shcamled_pmic_set_torch_led_2_current                                     */
/* ------------------------------------------------------------------------- */
static int shcamled_pmic_set_torch_led_2_current(unsigned mA)
{
	int rc = 0;
	mutex_lock(&shcamled_torch_mut);

	SHCAMLED_TRACE("%s in mA:%d ucam:%d ucom:%d\n", __FUNCTION__, mA,
				   shcamled_use_torch_led_cam, shcamled_use_torch_led_comm);

	if(mA == 0) {
		if(shcamled_use_torch_led_cam == FALSE && shcamled_use_torch_led_comm == TRUE) {
			/* request turning off */
			led_trigger_event(cam_torch_led_1_trigger, LED_OFF);
			#if defined(CONFIG_TPS_DRIVER)
			tps_driver_unregister_user(TPS_DRIVER_USER_CAMLED);
			#endif /* defined(CONFIG_TPS_DRIVER) */
			shcamled_use_torch_led_comm = FALSE;
		}
	}
	else if(SHCAMLED_TORCH_MA == mA) {
		if(shcamled_use_torch_led_cam == FALSE && shcamled_use_torch_led_comm == FALSE) {
			shcamled_use_torch_led_mA = mA;
			/* request turning on */
			#if defined(CONFIG_TPS_DRIVER)
			tps_driver_register_user(TPS_DRIVER_USER_CAMLED);
			#endif /* defined(CONFIG_TPS_DRIVER) */
			led_trigger_event(cam_torch_led_1_trigger, (mA * LED_FULL / SHCAMLED_FULL_MA));
			shcamled_use_torch_led_comm = TRUE;
		}
	}
	else if(SHCAMLED_FLASH_MA == mA) {
		if(shcamled_use_torch_led_cam == FALSE && shcamled_use_torch_led_comm == FALSE) {
			shcamled_use_torch_led_mA = mA;
			#if defined(CONFIG_TPS_DRIVER)
			rc = tps_driver_register_user(TPS_DRIVER_USER_CAMLED);
			#endif /* defined(CONFIG_TPS_DRIVER) */
			if (rc == 0) {
				led_trigger_event(cam_torch_led_1_trigger, (mA * LED_FULL / SHCAMLED_FULL_MA));
				shcamled_use_torch_led_comm = TRUE;
			}
		}
	}
	else {
		shcamled_use_torch_led_comm = FALSE;
	}

	SHCAMLED_TRACE("%s done ucam:%d ucom:%d\n", __FUNCTION__,
				   shcamled_use_torch_led_cam, shcamled_use_torch_led_comm);

	mutex_unlock(&shcamled_torch_mut);

	return 0;
}

/* ------------------------------------------------------------------------- */
/* shcamled_pmic_req_torch_led_current                                       */
/* ------------------------------------------------------------------------- */
int shcamled_pmic_req_torch_led_current_off(void)
{

	int ret = FALSE;

	SHCAMLED_TRACE("%s in ucam:%d ucom:%d\n", __FUNCTION__,
				   shcamled_use_torch_led_cam, shcamled_use_torch_led_comm);

	if((shcamled_use_torch_led_cam == FALSE && shcamled_use_torch_led_comm == TRUE) ||
	   (shcamled_use_torch_led_cam == TRUE && shcamled_use_torch_led_comm == FALSE)) {
		SHCAMLED_TRACE("%s %d mA\n", __FUNCTION__, shcamled_use_torch_led_mA);

		if (shcamled_use_torch_led_mA == SHCAMLED_FLASH_MA) {
			/* request strobe off */
			led_trigger_event(cam_torch_led_1_trigger, LED_OFF);
			shcamled_use_torch_led_cam = FALSE;
			shcamled_use_torch_led_comm = FALSE;
			ret = TRUE;
		}
	}

	SHCAMLED_TRACE("%s done\n", __FUNCTION__);
	return ret;
}
EXPORT_SYMBOL(shcamled_pmic_req_torch_led_current_off);

/* ------------------------------------------------------------------------- */
/* shcamled_pmic_get_led_conflict                                       */
/* ------------------------------------------------------------------------- */
int shcamled_pmic_get_led_conflict(void)
{
	int ret = FALSE;

	if (shcamled_use_torch_led_mA == SHCAMLED_FLASH_MA) {
		ret = TRUE;
	}
	return ret;
}
EXPORT_SYMBOL(shcamled_pmic_get_led_conflict);

/* ------------------------------------------------------------------------- */
/* shcamled_torch_store                                                      */
/* ------------------------------------------------------------------------- */
static ssize_t shcamled_torch_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	ssize_t ret = -EINVAL;
	int proc_ret;
	char *after;
	unsigned long val = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	SHCAMLED_TRACE("%s in\n", __FUNCTION__);

	/* count written bytes size and check size */
	if (isspace(*after))
		count++;

	if(count == size) {
		ret = count;
		proc_ret = shcamled_pmic_set_torch_led_2_current(val);
		if(proc_ret) {
			ret = -EFAULT;
			SHCAMLED_ERROR("%s failed ret:%d\n", __FUNCTION__, proc_ret);
		}
		SHCAMLED_TRACE("%s done ret:%d\n", __FUNCTION__, proc_ret);
	}

	SHCAMLED_TRACE("%s done ret:%d\n", __FUNCTION__, ret);
	return ret;
}

/* ------------------------------------------------------------------------- */
/* shcamled_torch_prove                                                      */
/* ------------------------------------------------------------------------- */
static int shcamled_torch_probe(struct platform_device *pdev)
{

	int ret;

	SHCAMLED_TRACE("%s in\n", __FUNCTION__);

	led_trigger_register_simple("cam-led1", &cam_torch_led_1_trigger);

	ret = device_create_file(&pdev->dev, &dev_attr_shcamled_torch);
	if (ret) {
		SHCAMLED_ERROR("%s failed create file \n", __FUNCTION__);
		return ret;
	}

	SHCAMLED_TRACE("%s done ret:%d\n", __FUNCTION__, ret);
	return ret;
}

/* ------------------------------------------------------------------------- */
/* shcamled_torch_remove                                                     */
/* ------------------------------------------------------------------------- */
static int shcamled_torch_remove(struct platform_device *pdev)
{

	SHCAMLED_TRACE("%s in\n", __FUNCTION__);

	led_trigger_unregister_simple(cam_torch_led_1_trigger);

	device_remove_file(&pdev->dev, &dev_attr_shcamled_torch);

	SHCAMLED_TRACE("%s done\n", __FUNCTION__);
	return 0;
}

/* ------------------------------------------------------------------------- */
/* shcamled_driver_init                                                      */
/* ------------------------------------------------------------------------- */
static int __init shcamled_torch_driver_init(void)
{
	int ret;

	SHCAMLED_TRACE("%s in\n", __FUNCTION__);

	/* torch 1. register platform-driver for device */
	ret = platform_driver_register(&shcamled_torch_driver);
	if (ret) {
		SHCAMLED_ERROR("%s failed! L.%d ret=%d\n", __FUNCTION__, __LINE__, ret);
		return ret;
	}

	/* torch 2. register platform-device */
	ret = platform_device_register(&shcamled_torch_dev);
	if (ret) {
		SHCAMLED_ERROR("%s failed! L.%d ret=%d\n", __FUNCTION__, __LINE__, ret);
		platform_driver_unregister(&shcamled_torch_driver);
		return ret;
	}

	SHCAMLED_TRACE("%s done\n", __FUNCTION__);
	return 0;
}
module_init(shcamled_torch_driver_init);

/* ------------------------------------------------------------------------- */
/* shcamled_driver_exit                                                      */
/* ------------------------------------------------------------------------- */
static void __exit shcamled_torch_driver_exit(void)
{
	SHCAMLED_TRACE("%s in\n", __FUNCTION__);

	platform_driver_unregister(&shcamled_torch_driver);
	platform_device_unregister(&shcamled_torch_dev);

	SHCAMLED_TRACE("%s done\n", __FUNCTION__);
}
module_exit(shcamled_torch_driver_exit);

MODULE_DESCRIPTION("SHARP CAMERA TORCH LED DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.1");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
