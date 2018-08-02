/* drivers/sharp/shkeyled/shkeyled_kerl.c  (Key LED Driver)
 *
 * Copyright (C) 2010-2013 SHARP CORPORATION
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/module.h>

#include <mach/pmic.h>

#define SHKEYLED_LOG_TAG "SHKEYLEDkerl"
#define  SHKEYLED_DEBUG_LOG_ENABLE_1
//#define  SHKEYLED_DEBUG_LOG_ENABLE_2

#ifdef SHKEYLED_DEBUG_LOG_ENABLE_1
#define SHKEYLED_DEBUG_LOG_1(fmt, args...)	printk(KERN_INFO "[%s][%s(%d)] " fmt"\n", SHKEYLED_LOG_TAG, __func__, __LINE__, ## args)
#else
#define SHKEYLED_DEBUG_LOG_1(fmt, args...)
#endif

#ifdef SHKEYLED_DEBUG_LOG_ENABLE_2
#define SHKEYLED_DEBUG_LOG_2(fmt, args...)	printk(KERN_INFO "[%s][%s(%d)] " fmt"\n", SHKEYLED_LOG_TAG, __func__, __LINE__, ## args)
#else
#define SHKEYLED_DEBUG_LOG_2(fmt, args...)
#endif

#define KEYLED_FULL			2
#define KEYLED_CURRENT_MAX	40
#define KEYLED_CURRENT_UP	2

/* LED trigger */
DEFINE_LED_TRIGGER(buttonbacklight_trigger);

uint	shkeyled_ma = 0;
uint	shkeyled_full        = KEYLED_FULL;
uint	shkeyled_current_max = KEYLED_CURRENT_MAX;
uint	shkeyled_current_up  = KEYLED_CURRENT_UP;

static void shkeyled_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	
	if(value == LED_OFF)
	{
		shkeyled_ma = 0;
	}
	else if(value == LED_FULL)
	{
		shkeyled_ma = shkeyled_full;
	}
	else if(value == 2)
	{
		if(shkeyled_ma < shkeyled_current_max)
		{
			shkeyled_ma += shkeyled_current_up;
		}
	}
	else if(value == 4)
	{
		if(shkeyled_ma > 0)
		{
			shkeyled_ma -= shkeyled_current_up;
		}
	}
	else
	{
		shkeyled_ma = 0;
	}
	
	SHKEYLED_DEBUG_LOG_2("value = %d, mA = %d", value, shkeyled_ma);

	led_trigger_event(buttonbacklight_trigger, (shkeyled_ma * LED_FULL / KEYLED_CURRENT_MAX));
}

static struct led_classdev shkeyled_dev =
{
	.name			= "button-backlight",
	.brightness_set	= shkeyled_set,
	.brightness		= LED_OFF,
};

static int shkeyled_probe(struct platform_device *pdev)
{
	int rc;

	led_trigger_register_simple("keybacklight", &buttonbacklight_trigger);

	rc = led_classdev_register(&pdev->dev, &shkeyled_dev);
	if (rc)
	{
		SHKEYLED_DEBUG_LOG_1("led_classdev_register Error");
		return rc;
	}

	shkeyled_full        = KEYLED_FULL;
	shkeyled_current_max = KEYLED_CURRENT_MAX;
	shkeyled_current_up	 = KEYLED_CURRENT_UP;

	shkeyled_set(&shkeyled_dev, LED_OFF);

	return rc;
}

static int __devexit shkeyled_remove(struct platform_device *pdev)
{
	led_trigger_unregister_simple(buttonbacklight_trigger);
	led_classdev_unregister(&shkeyled_dev);

	return 0;
}

#ifdef CONFIG_PM
static int shkeyled_suspend(struct platform_device *dev, pm_message_t state)
{
	led_classdev_suspend(&shkeyled_dev);

	return 0;
}

static int shkeyled_resume(struct platform_device *dev)
{
	led_classdev_resume(&shkeyled_dev);

	return 0;
}
#else
#define shkeyled_suspend NULL
#define shkeyled_resume NULL
#endif

static struct platform_driver shkeyled_driver = {
	.probe		= shkeyled_probe,
	.remove		= __devexit_p(shkeyled_remove),
	.suspend	= shkeyled_suspend,
	.resume		= shkeyled_resume,
	.driver		= {
		.name	= "shkeyled",
		.owner	= THIS_MODULE,
	},
};

static int __init shkeyled_init(void)
{
	return platform_driver_register(&shkeyled_driver);
}

static void __exit shkeyled_exit(void)
{
	platform_driver_unregister(&shkeyled_driver);
}

module_exit(shkeyled_exit);
module_init(shkeyled_init);

MODULE_DESCRIPTION("SHARP KEYLED DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.0");
