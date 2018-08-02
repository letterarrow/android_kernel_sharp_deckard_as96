/*
  * Copyright (C) 2011 SHARP CORPORATION All rights reserved.
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

/*+-----------------------------------------------------------------------------+*/
/*| @ DEFINE COMPILE SWITCH :													|*/
/*+-----------------------------------------------------------------------------+*/

	/* NONE.. */

/*| ######## ZANTEI ######## */
#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
//#define WLCHG_BOOTLOADER_V1_0
//#define WLCHG_DO_VERIFY
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */


/*+-----------------------------------------------------------------------------+*/
/*| @ INCLUDE FILE :															|*/
/*+-----------------------------------------------------------------------------+*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/android_alarm.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/poll.h>
#include <asm/uaccess.h>

#include "sharp/shbatt_kerl.h"
#include "shbatt_type.h"

#ifdef CONFIG_SHTMP_SENSOR
#include "shbatt_tmp.h"
#endif /* CONFIG_SHTMP_SENSOR */

#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/mfd/pm8xxx/ccadc.h>

#include <linux/sched.h>

#include <linux/irq.h>
#include <linux/hrtimer.h>

#include <linux/mfd/pm8xxx/batt-alarm.h>

#include <linux/time.h>

#include "sharp/sh_smem.h"
#include "sharp/shchg_kerl.h"

#include <mach/gpio.h>
#include <linux/regulator/consumer.h>
#include <mach/vreg.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <mach/msm_xo.h>
#include <linux/workqueue.h>
#include <linux/usb/otg.h>

#include <sharp/sh_boot_manager.h>

#define TRACE  /*printk(KERN_DEBUG "(%s,%d)\n",__FUNCTION__, __LINE__)*/

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL MACRO DECLARE : 													|*/
/*+-----------------------------------------------------------------------------+*/
//#define SHBATT_ENABLE_DEBUG

#ifdef SHBATT_ENABLE_DEBUG

#ifdef SHBATT_ENABLE_LOGGING

#define SHBATT_INFO(x...)	shlogger_api_post_log(x)
#define SHBATT_TRACE(x...)	shlogger_api_post_log(x)
#define SHBATT_ERROR(x...)	shlogger_api_post_log(x)

#else

#define SHBATT_INFO(x...)	printk(KERN_INFO "[SHBATT] " x)
#define SHBATT_TRACE(x...)	printk(KERN_DEBUG "[SHBATT] " x)
#define SHBATT_ERROR(x...)	printk(KERN_ERR "[SHBATT] " x)

#endif /* SHBATT_ENABLE_LOGGING */

#else

#define SHBATT_INFO(x...)	do {} while(0)
#define SHBATT_TRACE(x...)	do {} while(0)
#ifdef SHBATT_ENABLE_LOGGING
#define SHBATT_ERROR(x...)	shlogger_api_post_log(x)
#else
#define SHBATT_ERROR(x...)	printk(KERN_ERR "[SHBATT] " x)
#endif /* SHBATT_ENABLE_LOGGING */

#endif /* SHBATT_ENABLE_DEBUG */

#define SHBATT_DEV_NAME						"shbatt"
#define SHBATT_PM8921_POWER_SUPPLY_NAME		"battery"
#define SHBATT_WAKE_LOCK_NAME				"shbatt_wake_0x%08x"

#define PM8921_GPIO_BASE					NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)		(pm_gpio - 1 + PM8921_GPIO_BASE)

#define SHBATT_GPIO_NUM_WIRELESS_STATUS		PM8921_GPIO_PM_TO_SYS(33)
#define SHBATT_GPIO_NUM_WIRELESS_SWITCH		PM8921_GPIO_PM_TO_SYS(22)
#define SHBATT_GPIO_NUM_LED_SHUT			PM8921_GPIO_PM_TO_SYS(43)

#define GPIO_SET(gpio,v)	((__gpio_cansleep(gpio) == 0) ? gpio_set_value(gpio, v) : gpio_set_value_cansleep(gpio, v))
#define GPIO_GET(gpio)		((__gpio_cansleep(gpio) == 0) ? gpio_get_value(gpio)    : gpio_get_value_cansleep(gpio)   )

#if defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE1)
	#define SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH		49
	#define SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH		40
	#define SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH		30
	#define SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH		20
	#define SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH		13
	#define SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH		6
	#define SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH		0
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE2)
	#define SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH		40
	#define SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH		30
	#define SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH		20
	#define SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH		15
	#define SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH		10
	#define SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH		6
	#define SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH		1
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE3)
	#define SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH		40
	#define SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH		32
	#define SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH		23
	#define SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH		17
	#define SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH		10
	#define SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH		6
	#define SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH		1

	#define SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH_USBUSE	40
	#define SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH_USBUSE	30
	#define SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH_USBUSE	1

	#define SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH_TLOW	40	
	#define SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH_TLOW	30	
	#define SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH_TLOW	20
	#define SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH_TLOW	1
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE4)
	#define SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH		40
	#define SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH		30
	#define SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH		20
	#define SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH		15
	#define SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH		10
	#define SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH		6
	#define SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH		1
#else
	#define SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH		40
	#define SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH		30
	#define SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH		20
	#define SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH		13
	#define SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH		6
	#define SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH		0
#endif



#define SHBATT_WAKE_CTL(x)										\
{																\
	do															\
	{															\
		if(x == 0)												\
		{														\
			SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(0) call shbatt_wake_lock_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wake_lock_num));	\
			if(atomic_dec_return(&shbatt_wake_lock_num) == 0)	\
			{													\
				wake_unlock(&shbatt_wake_lock); 				\
				SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(0) done shbatt_wake_lock_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wake_lock_num));	\
			}													\
		}														\
		else													\
		{														\
			SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(1) call shbatt_wake_lock_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wake_lock_num));	\
			if(atomic_inc_return(&shbatt_wake_lock_num) == 1)	\
			{													\
				SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(1) done shbatt_wake_lock_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wake_lock_num));	\
				wake_lock(&shbatt_wake_lock);					\
			}													\
		}														\
	} while(0); 												\
}
#define SHBATT_CABLE_STATUS_ATTR(_name)									\
{																		\
	.attr  = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP},	\
	.show  = shbatt_drv_show_cable_status_property,						\
	.store = shbatt_drv_store_property, 								\
}

#define SHBATT_BATTERY_ATTR(_name)										\
{																		\
	.attr  = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP},	\
	.show  = shbatt_drv_show_battery_property,							\
	.store = shbatt_drv_store_property, 								\
}

#define SHBATT_FG_ATTR(_name)											\
{																		\
	.attr  = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP},	\
	.show  = shbatt_drv_show_fuelgauge_property,						\
	.store = shbatt_drv_store_property, 								\
}

#define SHBATT_PMIC_ATTR(_name)											\
{																		\
	.attr  = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP},	\
	.show  = shbatt_drv_show_pmic_property,						\
	.store = shbatt_drv_store_property, 								\
}


#define SHBATT_ADC_ATTR(_name)											\
{																		\
	.attr  = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP},	\
	.show  = shbatt_drv_show_adc_property,								\
	.store = shbatt_drv_store_property, 								\
}
#define		SHBATT_IOC_MAGIC 's'
#define		SHBATT_DRV_IOCTL_CMD_PULL_USSE_PACKET						_IOR(SHBATT_IOC_MAGIC, 0, shbatt_usse_packet_t)
#define		SHBATT_DRV_IOCTL_CMD_DONE_USSE_PACKET						_IOW(SHBATT_IOC_MAGIC, 1, shbatt_usse_packet_t)
#define		SHBATT_DRV_IOCTL_CMD_INVALID								_IO(SHBATT_IOC_MAGIC, 2)
#define		SHBATT_DRV_IOCTL_CMD_SET_TIMER								_IOW(SHBATT_IOC_MAGIC, 8, shbatt_poll_timer_info_t)
#define		SHBATT_DRV_IOCTL_CMD_CLR_TIMER								_IOW(SHBATT_IOC_MAGIC, 9, shbatt_poll_timer_info_t)
#define		SHBATT_DRV_IOCTL_CMD_SET_VOLTAGE_ALARM						_IOW(SHBATT_IOC_MAGIC, 10, shbatt_voltage_alarm_info_t)
#define		SHBATT_DRV_IOCTL_CMD_CLR_VOLTAGE_ALARM						_IOW(SHBATT_IOC_MAGIC, 11, shbatt_voltage_alarm_info_t)
#define		SHBATT_DRV_IOCTL_CMD_GET_BATTERY_SAFETY						_IOR(SHBATT_IOC_MAGIC, 20, shbatt_safety_t)
#define		SHBATT_DRV_IOCTL_CMD_GET_TERMINAL_TEMPERATURE				_IOR(SHBATT_IOC_MAGIC, 22, int)
#define		SHBATT_DRV_IOCTL_CMD_GET_MODEM_TEMPERATURE					_IOR(SHBATT_IOC_MAGIC, 23, int)
#define		SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_CURRENT					_IOR(SHBATT_IOC_MAGIC, 24, int)
#define		SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_VOLTAGE					_IOR(SHBATT_IOC_MAGIC, 25, int)
#define		SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_DEVICE_ID				_IOR(SHBATT_IOC_MAGIC, 26, unsigned int)
#define		SHBATT_DRV_IOCTL_CMD_SET_FUELGAUGE_MODE						_IOW(SHBATT_IOC_MAGIC, 27, shbatt_fuelgauge_mode_t)
#define		SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_ACCUMULATE_CURRENT		_IOR(SHBATT_IOC_MAGIC, 28, shbatt_accumulate_current_t)
#define		SHBATT_DRV_IOCTL_CMD_CLR_FUELGAUGE_ACCUMULATE_CURRENT		_IO(SHBATT_IOC_MAGIC, 29)
#define		SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_TEMPERATURE				_IOR(SHBATT_IOC_MAGIC, 30, int)
#define		SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_CURRENT_AD				_IOR(SHBATT_IOC_MAGIC, 31, int)
#define		SHBATT_DRV_IOCTL_CMD_READ_ADC_CHANNEL_BUFFERED				_IOWR(SHBATT_IOC_MAGIC, 36, shbatt_adc_t)
#define		SHBATT_DRV_IOCTL_CMD_GET_BATTERY_LOG_INFO					_IOR(SHBATT_IOC_MAGIC, 40, shbatt_batt_log_info_t)
#define		SHBATT_DRV_IOCTL_CMD_POST_BATTERY_LOG_INFO					_IOW(SHBATT_IOC_MAGIC, 41, shbatt_batt_log_info_t)
#define		SHBATT_DRV_IOCTL_CMD_NOTIFY_CHARGER_CABLE_STATUS			_IOW(SHBATT_IOC_MAGIC, 42, shbatt_cable_status_t)
#define		SHBATT_DRV_IOCTL_CMD_NOTIFY_BATTERY_CHARGING_STATUS			_IOW(SHBATT_IOC_MAGIC, 43, shbatt_chg_status_t)
#define		SHBATT_DRV_IOCTL_CMD_NOTIFY_BATTERY_CAPACITY				_IOW(SHBATT_IOC_MAGIC, 44, int)
#define		SHBATT_DRV_IOCTL_CMD_NOTIFY_CHARGING_STATE_MACHINE_ENABLE	_IOW(SHBATT_IOC_MAGIC, 45, shbatt_boolean_t)
#define		SHBATT_DRV_IOCTL_CMD_NOTIFY_POWER_SUPPLY_CHANGED			_IOW(SHBATT_IOC_MAGIC, 46, shbatt_ps_category_t)
#define		SHBATT_DRV_IOCTL_CMD_INITIALIZE								_IO(SHBATT_IOC_MAGIC, 47)
#define		SHBATT_DRV_IOCTL_CMD_EXEC_LOW_BATTERY_CHECK_SEQUENCE		_IOW(SHBATT_IOC_MAGIC, 48, int)
#define		SHBATT_DRV_IOCTL_CMD_GET_KERNEL_TIME						_IOR(SHBATT_IOC_MAGIC, 49, struct timeval)
#define		SHBATT_DRV_IOCTL_CMD_READ_ADC_CHANNEL_NO_CONVERSION			_IOWR(SHBATT_IOC_MAGIC, 51, shbatt_adc_t*)
#define		SHBATT_DRV_IOCTL_CMD_GET_HW_REVISION						_IOR(SHBATT_IOC_MAGIC,  52, uint*)
#define		SHBATT_DRV_IOCTL_CMD_GET_SMEM_INFO							_IOR(SHBATT_IOC_MAGIC,  53, shbatt_smem_info_t*)

#ifdef CONFIG_PM_WIRELESS_CHARGE
#define SHBATT_DRV_IOCTL_CMD_ENABLE_WIRELESS_CHARGER		_IO(SHBATT_IOC_MAGIC,  54)
#define SHBATT_DRV_IOCTL_CMD_DISABLE_WIRELESS_CHARGER		_IO(SHBATT_IOC_MAGIC,  55)
#define SHBATT_DRV_IOCTL_CMD_GET_WIRELESS_DISABLE_STATE		_IOR(SHBATT_IOC_MAGIC,  56, shbatt_wireless_disable_state_t)
#define SHBATT_DRV_IOCTL_CMD_GET_WIRELESS_CHARGING_STATUS	_IOR(SHBATT_IOC_MAGIC,  57, shbatt_wireless_charging_state_t)
#endif

#define		SHBATT_DRV_IOCTL_CMD_SET_LOG_ENABLE							_IOR(SHBATT_IOC_MAGIC,  58, int)
#define		SHBATT_DRV_IOCTL_CMD_GET_CPU_CLOCK_LEVEL					_IOR(  SHBATT_IOCTL_MAGIC, 59, shbatt_cpu_clock_level_t*)
#define		SHBATT_DRV_IOCTL_CMD_READ_HDQ								_IOR(SHBATT_IOC_MAGIC, 60,  shbatt_hdq_read_t)
#define		SHBATT_DRV_IOCTL_CMD_WRITE_HDQ								_IOW(SHBATT_IOC_MAGIC, 61, shbatt_hdq_write_t)
#define		SHBATT_DRV_IOCTL_CMD_GET_DEPLETED_CAPACITY					_IOR(  SHBATT_IOCTL_MAGIC, 62, int)
#define		SHBATT_DRV_IOCTL_CMD_SET_DEPLETED_BATTERY_FLG				_IOW(  SHBATT_IOCTL_MAGIC, 63, int)
#define		SHBATT_DRV_IOCTL_CMD_GET_DEPLETED_BATTERY_FLG				_IOR(  SHBATT_IOCTL_MAGIC, 64, int)
#define		SHBATT_DRV_IOCTL_CMD_CHECK_BATTERY_AUTH						_IOR(  SHBATT_IOCTL_MAGIC, 65, unsigned char*)
#define		SHBATT_DRV_IOCTL_CMD_SET_BATTERY_HEALTH						_IOW(SHBATT_IOC_MAGIC, 66, shbatt_health_t)
#define 	SHBATT_DRV_IOCTL_CMD_SET_BATTERY_AUTH						_IOW(  SHBATT_IOCTL_MAGIC, 67, int)
#define SHBATT_DRV_IOCTL_CMD_SET_ANDROID_SHUTDOWN_TIMER					_IOR(SHBATT_IOC_MAGIC,  68, int)

#define		SHBATT_DRV_IOCTL_CMD_GET_AVE_CURRENT_AND_AVE_VOLTAGE			_IOR(  SHBATT_IOCTL_MAGIC, 69, shbatt_current_voltage_t)
#define		SHBATT_DRV_IOCTL_CMD_READ_ADC_CHANNELS					_IOR(  SHBATT_IOCTL_MAGIC, 70, shbatt_adc_read_request_t)

#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
#define		SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_UPDATE		_IO( SHBATT_IOC_MAGIC, 71)
#define 	SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_FVER		_IOR(  SHBATT_IOCTL_MAGIC, 72, int)
#define 	SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_HWVER	_IOR(  SHBATT_IOCTL_MAGIC, 73, int)

/*calibration*/
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_GO_TO_CALIBRATION_MODE				_IO( SHBATT_IOCTL_MAGIC, 74)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_VOLTAGE_CALIBRATION				_IOWR( SHBATT_IOCTL_MAGIC, 75, shbatt_wlchg_voltage_calibration_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_VOLTAGE_AND_CURRENT_CALIBRATION	_IOWR( SHBATT_IOCTL_MAGIC, 76, shbatt_wlchg_voltage_and_current_calibration_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_CURRENT_CALIBRATION				_IOWR( SHBATT_IOCTL_MAGIC, 77, shbatt_wlchg_current_calibration_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_WRITE_WPC_SERIAL_NO				_IOW( SHBATT_IOCTL_MAGIC, 78, uint16_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_GET_CALIBRATED_VALUE				_IOR( SHBATT_IOCTL_MAGIC, 79, shbatt_wlchg_calibrated_value_t)

#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_SET_BATT_TEMP	_IOW( SHBATT_IOCTL_MAGIC, 80, uint8_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_BATT_TEMP	_IOR( SHBATT_IOCTL_MAGIC, 81, uint8_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_SET_CPU_TEMP		_IOR( SHBATT_IOCTL_MAGIC, 82, uint8_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_CPU_TEMP		_IOW( SHBATT_IOCTL_MAGIC, 83, uint8_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_RECTIFIER_IC_TEMP	_IOR( SHBATT_IOCTL_MAGIC, 84, uint8_t)
#define SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_UPDATE_FROM_BUFFER	_IOW( SHBATT_IOCTL_MAGIC, 85, shbatt_wlchg_firmware_buffer_t)

#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */

#define SHBATT_DRV_IOCTL_CMD_GET_AVERAGE_CURRENT						_IOR( SHBATT_IOCTL_MAGIC,  86, int)
#define SHBATT_DRV_IOCTL_CMD_CALIB_CCADC								_IO( SHBATT_IOCTL_MAGIC,  87)
#define SHBATT_DRV_IOCTL_CMD_GET_SYSTEM_TIME							_IOR(SHBATT_IOC_MAGIC, 88, struct timeval)

#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C

#define SHBATT_DRV_IOCTL_CMD_WLCHG_READ_I2C_DATA						_IOR( SHBATT_IOCTL_MAGIC, 89, shbatt_wlchg_i2c_data_t)
#define SHBATT_DRV_IOCTL_CMD_WLCHG_WRITE_I2C_DATA						_IOW( SHBATT_IOCTL_MAGIC, 90, shbatt_wlchg_i2c_data_t)

#endif
#define	SHBATT_DRV_IOCTL_CMD_RESET_CC									_IO(  SHBATT_IOCTL_MAGIC, 91)
#define SHBATT_DRV_IOCTL_CMD_SET_DEPLETED_CALC_ENABLE					_IOR(SHBATT_IOC_MAGIC,  92, int)

#define GISPRODUCT_F_MASK	0x00000080
#define GISSOFTUP_F_MASK	0x00000010
#define GISABNORMAL_F_MASK	0x00001000
#define GISRESTRAIN_F_MASK	0x00000800


#define MAX(a,b)	((a>b)? a : b)
#define MIN(a,b)	((a<b)? a : b)

/*+-----------------------------------------------------------------------------+*/
/*| @ EXTERN VARIABLE : 														|*/
/*+-----------------------------------------------------------------------------+*/

	/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC VARIABLE : 														|*/
/*+-----------------------------------------------------------------------------+*/

	/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ STATIC VARIABLE : 														|*/
/*+-----------------------------------------------------------------------------+*/

static int shbatt_cur_bootinfo = 0;
static bool shbatt_task_is_initialized = false;
static struct wake_lock shbatt_wake_lock;
static bool shbatt_task_is_initialized_multi = false;

static atomic_t shbatt_wake_lock_num;
static shbatt_cable_status_t shbatt_cur_boot_time_cable_status = SHBATT_CABLE_STATUS_NONE;
static shbatt_chg_status_t shbatt_cur_boot_time_chg_status = SHBATT_CHG_STATUS_DISCHARGING;
static spinlock_t shbatt_pkt_lock;
static shbatt_packet_t shbatt_pkt[16];
static shbatt_usse_packet_t shbatt_usse_pkt;
static shbatt_timer_t shbatt_tim_soc_poll;
static shbatt_timer_t shbatt_tim_soc_poll_sleep;
static shbatt_timer_t shbatt_tim_soc_poll_multi;
static shbatt_timer_t shbatt_tim_soc_poll_sleep_multi;
static shbatt_timer_t shbatt_tim_low_battery;
static shbatt_timer_t shbatt_tim_fatal_battery;
static shbatt_timer_t shbatt_tim_low_battery_shutdown;
static shbatt_timer_t shbatt_tim_charging_fatal_battery;
static shbatt_timer_t shbatt_tim_overcurr;
static shbatt_timer_t shbatt_tim_battery_present;
static shbatt_timer_t shbatt_tim_wireless_charge;

static struct power_supply shbatt_power_supplies[] =
{
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
	},

	{
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
	},

	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
	},

	{
		.name = "shbatt_fuelgauge",
		.type = POWER_SUPPLY_TYPE_BATTERY,
	},

	{
		.name = "shbatt_pmic",
		.type = POWER_SUPPLY_TYPE_BATTERY,
	},

	{
		.name = "shbatt_adc",
		.type = POWER_SUPPLY_TYPE_BATTERY,
	},

	{
		.name = "wireless",
		.type = POWER_SUPPLY_TYPE_WIRELESS,
	},
};
static enum power_supply_property shbatt_ps_props[] = 
{
	POWER_SUPPLY_PROP_TYPE,
};
static ssize_t shbatt_drv_store_property( struct device* dev_p, struct device_attribute* attr_p, const char* buf_p, size_t size );

static ssize_t shbatt_drv_show_cable_status_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p );
static ssize_t shbatt_drv_show_battery_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p );
static ssize_t shbatt_drv_show_fuelgauge_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p );
static ssize_t shbatt_drv_show_pmic_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p );
static ssize_t shbatt_drv_show_adc_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p );

#ifdef CONFIG_BATTERY_SH
static struct device_attribute shbatt_cable_status_attributes[] = 
{
	SHBATT_CABLE_STATUS_ATTR(online),						/*SHBATT_PS_PROPERTY_ONLINE,*/
};

static struct device_attribute shbatt_battery_attributes[] = 
{
	SHBATT_BATTERY_ATTR(status), 					/*SHBATT_PS_PROPERTY_STATUS,*/
	SHBATT_BATTERY_ATTR(health), 					/*SHBATT_PS_PROPERTY_HEALTH,*/
	SHBATT_BATTERY_ATTR(present),					/*SHBATT_PS_PROPERTY_PRESENT,*/
	SHBATT_BATTERY_ATTR(capacity),					/*SHBATT_PS_PROPERTY_CAPACITY,*/
	SHBATT_BATTERY_ATTR(batt_vol),					/*SHBATT_PS_PROPERTY_BATT_VOL,*/
	SHBATT_BATTERY_ATTR(batt_temp),					/*SHBATT_PS_PROPERTY_BATT_TEMP,*/
	SHBATT_BATTERY_ATTR(technology), 				/*SHBATT_PS_PROPERTY_TECHNOLOGY,*/
	SHBATT_BATTERY_ATTR(safety), 					/*SHBATT_PS_PROPERTY_SAFETY,*/
	SHBATT_BATTERY_ATTR(camera_temp),				/*SHBATT_PS_PROPERTY_CAMERA_TEMP,*/
	SHBATT_BATTERY_ATTR(terminal_temp),				/*SHBATT_PS_PROPERTY_TERMINAL_TEMP,*/
	SHBATT_BATTERY_ATTR(modem_temp), 				/*SHBATT_PS_PROPERTY_MODEM_TEMP,*/
	SHBATT_BATTERY_ATTR(current_now),				/*SHBATT_PS_PROPERTY_CURRENT_NOW,*/
	SHBATT_BATTERY_ATTR(voltage_now),				/*SHBATT_PS_PROPERTY_VOLTAGE_NOW,*/
	SHBATT_BATTERY_ATTR(voltage_max_design),		/*SHBATT_PS_PROPERTY_VOLTAGE_MAX_DESIGN,*/
	SHBATT_BATTERY_ATTR(voltage_min_design),		/*SHBATT_PS_PROPERTY_VOLTAGE_MIN_DESIGN,*/
	SHBATT_BATTERY_ATTR(energy_full),				/*SHBATT_PS_PROPERTY_ENERGY_FULL,*/

};

static struct device_attribute shbatt_fuelgauge_attributes[] = 
{
	SHBATT_FG_ATTR(current),					/*SHBATT_PS_PROPERTY_CURRENT,*/
	SHBATT_FG_ATTR(voltage),					/*SHBATT_PS_PROPERTY_VOLTAGE,*/
	SHBATT_FG_ATTR(device_id),					/*SHBATT_PS_PROPERTY_DEVICE_ID,*/
	SHBATT_FG_ATTR(mode),						/*SHBATT_PS_PROPERTY_MODE,*/
	SHBATT_FG_ATTR(accumulate_current), 		/*SHBATT_PS_PROPERTY_ACCUMULATE_CURRENT,*/
	SHBATT_FG_ATTR(fgic_temp),					/*SHBATT_PS_PROPERTY_FGIC_TEMP,*/
	SHBATT_FG_ATTR(current_ad), 				/*SHBATT_PS_PROPERTY_CURRENT_AD,*/
};
static struct device_attribute shbatt_pmic_attributes[] = 
{
	SHBATT_PMIC_ATTR(charger_transistor_switch),	/*SHBATT_PS_PROPERTY_CHARGER_TRANSISTOR_S*/
	SHBATT_PMIC_ATTR(vmaxsel),					/*SHBATT_PS_PROPERTY_VMAXSEL,*/
	SHBATT_PMIC_ATTR(imaxsel),					/*SHBATT_PS_PROPERTY_IMAXSEL,*/
};


static struct device_attribute shbatt_adc_attributes[] = 
{
	SHBATT_ADC_ATTR(gpadc_in0),					/*SHBATT_PS_PROPERTY_GPADC_IN0,*/
	SHBATT_ADC_ATTR(gpadc_in1),					/*SHBATT_PS_PROPERTY_GPADC_IN1,*/
	SHBATT_ADC_ATTR(gpadc_in2),					/*SHBATT_PS_PROPERTY_GPADC_IN2,*/
	SHBATT_ADC_ATTR(gpadc_in3),					/*SHBATT_PS_PROPERTY_GPADC_IN3,*/
	SHBATT_ADC_ATTR(gpadc_in4),					/*SHBATT_PS_PROPERTY_GPADC_IN4,*/
	SHBATT_ADC_ATTR(gpadc_in5),					/*SHBATT_PS_PROPERTY_GPADC_IN5,*/
	SHBATT_ADC_ATTR(gpadc_in6),					/*SHBATT_PS_PROPERTY_GPADC_IN6,*/
	SHBATT_ADC_ATTR(vbat),						/*SHBATT_PS_PROPERTY_VBAT,*/
	SHBATT_ADC_ATTR(vbkp),						/*SHBATT_PS_PROPERTY_VBKP,*/
	SHBATT_ADC_ATTR(vac),						/*SHBATT_PS_PROPERTY_VAC,*/
	SHBATT_ADC_ATTR(vbus),						/*SHBATT_PS_PROPERTY_VBUS,*/
	SHBATT_ADC_ATTR(ichg),						/*SHBATT_PS_PROPERTY_ICHG,*/
	SHBATT_ADC_ATTR(hotdie1),					/*SHBATT_PS_PROPERTY_HOTDIE1,*/
	SHBATT_ADC_ATTR(hotdie2),					/*SHBATT_PS_PROPERTY_HOTDIE2,*/
	SHBATT_ADC_ATTR(id), 						/*SHBATT_PS_PROPERTY_ID,*/
	SHBATT_ADC_ATTR(testv),						/*SHBATT_PS_PROPERTY_TESTV,*/
	SHBATT_ADC_ATTR(chrg_led_test),				/*SHBATT_PS_PROPERTY_CHRG_LED_TEST,*/
	SHBATT_ADC_ATTR(gpadc_in0_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN0_BUF,*/
	SHBATT_ADC_ATTR(gpadc_in1_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN1_BUF,*/
	SHBATT_ADC_ATTR(gpadc_in2_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN2_BUF,*/
	SHBATT_ADC_ATTR(gpadc_in3_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN3_BUF,*/
	SHBATT_ADC_ATTR(gpadc_in4_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN4_BUF,*/
	SHBATT_ADC_ATTR(gpadc_in5_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN5_BUF,*/
	SHBATT_ADC_ATTR(gpadc_in6_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN6_BUF,*/
	SHBATT_ADC_ATTR(vbat_buf),					/*SHBATT_PS_PROPERTY_VBAT_BUF,*/
	SHBATT_ADC_ATTR(vbkp_buf),					/*SHBATT_PS_PROPERTY_VBKP_BUF,*/
	SHBATT_ADC_ATTR(vac_buf),					/*SHBATT_PS_PROPERTY_VAC_BUF,*/
	SHBATT_ADC_ATTR(vbus_buf),					/*SHBATT_PS_PROPERTY_VBUS_BUF,*/
	SHBATT_ADC_ATTR(ichg_buf),					/*SHBATT_PS_PROPERTY_ICHG_BUF,*/
	SHBATT_ADC_ATTR(hotdie1_buf),				/*SHBATT_PS_PROPERTY_HOTDIE1_BUF,*/
	SHBATT_ADC_ATTR(hotdie2_buf),				/*SHBATT_PS_PROPERTY_HOTDIE2_BUF,*/
	SHBATT_ADC_ATTR(id_buf), 					/*SHBATT_PS_PROPERTY_ID_BUF,*/
	SHBATT_ADC_ATTR(testv_buf),					/*SHBATT_PS_PROPERTY_TESTV_BUF,*/
	SHBATT_ADC_ATTR(chrg_led_test_buf),			/*SHBATT_PS_PROPERTY_CHRG_LED_TEST_BUF,*/
	SHBATT_ADC_ATTR(vbatt_calibration),			/*SHBATT_PS_PROPERTY_VBATT_CALIBRATION,*/
	SHBATT_ADC_ATTR(cpu_temp),					/*SHBATT_PS_PROPERTY_CPU_TEMP,*/
};

#else
static struct device_attribute shbatt_ps_attrs[NUM_SHBATT_PS_PROPERTY] =
{
	SHBATT_PS_ATTR(online),						/*SHBATT_PS_PROPERTY_ONLINE,*/
	SHBATT_PS_ATTR(status), 					/*SHBATT_PS_PROPERTY_STATUS,*/
	SHBATT_PS_ATTR(health), 					/*SHBATT_PS_PROPERTY_HEALTH,*/
	SHBATT_PS_ATTR(present),					/*SHBATT_PS_PROPERTY_PRESENT,*/
	SHBATT_PS_ATTR(capacity),					/*SHBATT_PS_PROPERTY_CAPACITY,*/
	SHBATT_PS_ATTR(batt_vol),					/*SHBATT_PS_PROPERTY_BATT_VOL,*/
	SHBATT_PS_ATTR(batt_temp),					/*SHBATT_PS_PROPERTY_BATT_TEMP,*/
	SHBATT_PS_ATTR(technology), 				/*SHBATT_PS_PROPERTY_TECHNOLOGY,*/
	SHBATT_PS_ATTR(safety), 					/*SHBATT_PS_PROPERTY_SAFETY,*/
	SHBATT_PS_ATTR(camera_temp),				/*SHBATT_PS_PROPERTY_CAMERA_TEMP,*/
	SHBATT_PS_ATTR(terminal_temp),				/*SHBATT_PS_PROPERTY_TERMINAL_TEMP,*/
	SHBATT_PS_ATTR(modem_temp), 				/*SHBATT_PS_PROPERTY_MODEM_TEMP,*/

	SHBATT_PS_ATTR(current),					/*SHBATT_PS_PROPERTY_CURRENT,*/
	SHBATT_PS_ATTR(voltage),					/*SHBATT_PS_PROPERTY_VOLTAGE,*/
	SHBATT_PS_ATTR(device_id),					/*SHBATT_PS_PROPERTY_DEVICE_ID,*/
	SHBATT_PS_ATTR(mode),						/*SHBATT_PS_PROPERTY_MODE,*/
	SHBATT_PS_ATTR(accumulate_current), 		/*SHBATT_PS_PROPERTY_ACCUMULATE_CURRENT,*/
	SHBATT_PS_ATTR(fgic_temp),					/*SHBATT_PS_PROPERTY_FGIC_TEMP,*/
	SHBATT_PS_ATTR(current_ad), 				/*SHBATT_PS_PROPERTY_CURRENT_AD,*/

	SHBATT_PS_ATTR(charger_transistor_switch),	/*SHBATT_PS_PROPERTY_CHARGER_TRANSISTOR_S*/
	SHBATT_PS_ATTR(vmaxsel),					/*SHBATT_PS_PROPERTY_VMAXSEL,*/
	SHBATT_PS_ATTR(imaxsel),					/*SHBATT_PS_PROPERTY_IMAXSEL,*/

	SHBATT_PS_ATTR(gpadc_in0),					/*SHBATT_PS_PROPERTY_GPADC_IN0,*/
	SHBATT_PS_ATTR(gpadc_in1),					/*SHBATT_PS_PROPERTY_GPADC_IN1,*/
	SHBATT_PS_ATTR(gpadc_in2),					/*SHBATT_PS_PROPERTY_GPADC_IN2,*/
	SHBATT_PS_ATTR(gpadc_in3),					/*SHBATT_PS_PROPERTY_GPADC_IN3,*/
	SHBATT_PS_ATTR(gpadc_in4),					/*SHBATT_PS_PROPERTY_GPADC_IN4,*/
	SHBATT_PS_ATTR(gpadc_in5),					/*SHBATT_PS_PROPERTY_GPADC_IN5,*/
	SHBATT_PS_ATTR(gpadc_in6),					/*SHBATT_PS_PROPERTY_GPADC_IN6,*/
	SHBATT_PS_ATTR(vbat),						/*SHBATT_PS_PROPERTY_VBAT,*/
	SHBATT_PS_ATTR(vbkp),						/*SHBATT_PS_PROPERTY_VBKP,*/
	SHBATT_PS_ATTR(vac),						/*SHBATT_PS_PROPERTY_VAC,*/
	SHBATT_PS_ATTR(vbus),						/*SHBATT_PS_PROPERTY_VBUS,*/
	SHBATT_PS_ATTR(ichg),						/*SHBATT_PS_PROPERTY_ICHG,*/
	SHBATT_PS_ATTR(hotdie1),					/*SHBATT_PS_PROPERTY_HOTDIE1,*/
	SHBATT_PS_ATTR(hotdie2),					/*SHBATT_PS_PROPERTY_HOTDIE2,*/
	SHBATT_PS_ATTR(id), 						/*SHBATT_PS_PROPERTY_ID,*/
	SHBATT_PS_ATTR(testv),						/*SHBATT_PS_PROPERTY_TESTV,*/
	SHBATT_PS_ATTR(chrg_led_test),				/*SHBATT_PS_PROPERTY_CHRG_LED_TEST,*/
	SHBATT_PS_ATTR(gpadc_in0_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN0_BUF,*/
	SHBATT_PS_ATTR(gpadc_in1_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN1_BUF,*/
	SHBATT_PS_ATTR(gpadc_in2_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN2_BUF,*/
	SHBATT_PS_ATTR(gpadc_in3_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN3_BUF,*/
	SHBATT_PS_ATTR(gpadc_in4_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN4_BUF,*/
	SHBATT_PS_ATTR(gpadc_in5_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN5_BUF,*/
	SHBATT_PS_ATTR(gpadc_in6_buf),				/*SHBATT_PS_PROPERTY_GPADC_IN6_BUF,*/
	SHBATT_PS_ATTR(vbat_buf),					/*SHBATT_PS_PROPERTY_VBAT_BUF,*/
	SHBATT_PS_ATTR(vbkp_buf),					/*SHBATT_PS_PROPERTY_VBKP_BUF,*/
	SHBATT_PS_ATTR(vac_buf),					/*SHBATT_PS_PROPERTY_VAC_BUF,*/
	SHBATT_PS_ATTR(vbus_buf),					/*SHBATT_PS_PROPERTY_VBUS_BUF,*/
	SHBATT_PS_ATTR(ichg_buf),					/*SHBATT_PS_PROPERTY_ICHG_BUF,*/
	SHBATT_PS_ATTR(hotdie1_buf),				/*SHBATT_PS_PROPERTY_HOTDIE1_BUF,*/
	SHBATT_PS_ATTR(hotdie2_buf),				/*SHBATT_PS_PROPERTY_HOTDIE2_BUF,*/
	SHBATT_PS_ATTR(id_buf), 					/*SHBATT_PS_PROPERTY_ID_BUF,*/
	SHBATT_PS_ATTR(testv_buf),					/*SHBATT_PS_PROPERTY_TESTV_BUF,*/
	SHBATT_PS_ATTR(chrg_led_test_buf),			/*SHBATT_PS_PROPERTY_CHRG_LED_TEST_BUF,*/
	SHBATT_PS_ATTR(vbatt_calibration)			/*SHBATT_PS_PROPERTY_VBATT_CALIBRATION,*/
};
#endif


static ssize_t shbatt_drv_store_property( struct device* dev_p, struct device_attribute* attr_p, const char* buf_p, size_t size )
{
	SHBATT_INFO("%s,dev_p=0x%08x,attr_p=0x%08x,buf_p=%s,size=%x", __FUNCTION__, (unsigned)dev_p, (unsigned)attr_p, buf_p, size);
	return size;
}


static struct workqueue_struct* shbatt_task_workqueue_p;

static struct mutex shbatt_api_lock;
static struct mutex shbatt_task_lock;
static struct completion shbatt_api_cmp;
static atomic_t shbatt_wake_lock_num;

static dev_t shbatt_dev;
static int shbatt_major;
static int shbatt_minor;
static struct cdev shbatt_cdev;
static struct class* shbatt_dev_class;

static atomic_t shbatt_usse_op_cnt;
static wait_queue_head_t shbatt_usse_wait;
static struct completion shbatt_usse_cmp;

static bool shbatt_charge_check_flg = false;

static bool shbatt_low_battery_interrupt = false;

static int64_t shbatt_before_batt_therm = -255;
static int shbatt_before_batt_therm_coefficient = 3;
static int shbatt_now_batt_therm_coefficient = 1;

struct timespec shbatt_last_timer_expire_time;
static bool shbatt_timer_restarted = false;

static const char *l22_vreg_name = "8921_l22";
static struct regulator *l22_vreg = NULL;
static int l22_vreg_on = 0;

#define L22_LOAD_UA 0
#define L22_MIN_UV 2850000
#define L22_MAX_UV 2850000

static struct msm_xo_voter* xo_handle;
#define WAIT_TIME_AFTER_XO_VOTE_ON	(10 * 1000)	/*us*/

static const char *l17_vreg_name = "8921_l17";
static struct regulator *l17_vreg = NULL;
static int l17_vreg_on = 0;

#define L17_LOAD_UA 0
#define L17_MIN_UV 3000000
#define L17_MAX_UV 3000000

/*+-----------------------------------------------------------------------------+*/
/*| @ EXTERN FUNCTION PROTO TYPE DECLARE :										|*/
/*+-----------------------------------------------------------------------------+*/

	/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL FUNCTION PROTO TYPE DECLARE :										|*/
/*+-----------------------------------------------------------------------------+*/


/*task.*/
static void shbatt_task_cmd_get_battery_safety( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_terminal_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_modem_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_fuelgauge_voltage( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_fuelgauge_device_id( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_fuelgauge_mode( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_fuelgauge_accumulate_current( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_clr_fuelgauge_accumulate_current( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_fuelgauge_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_fuelgauge_current_ad( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_adc_channel_buffered( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_log_info( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_post_battery_log_info( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_notify_charger_cable_status( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_notify_battery_charging_status( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_notify_battery_capacity( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_notify_charging_state_machine_enable( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_calibrate_battery_voltage( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_initialize( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_exec_low_battery_check_sequence( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_post_battery_log_info( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_exec_fuelgauge_soc_poll_sequence( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_exec_overcurr_check_sequence( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_exec_battery_present_check_sequence( shbatt_packet_t* pkt_p );
static shbatt_result_t shbatt_seq_exec_fuelgauge_soc_poll_sequence( shbatt_soc_t soc );
static shbatt_result_t shbatt_seq_exec_overcurr_check_sequence( void );
static shbatt_result_t shbatt_seq_exec_battery_present_check_sequence( int seq );
/*ioctrl.*/
static int shbatt_drv_ioctl_cmd_invalid( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_pull_usse_packet( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_done_usse_packet( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_timer( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_clr_timer( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_voltage_alarm( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_clr_voltage_alarm( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_charger_cable_status( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_charging_status( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_health( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_present( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_capacity( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_voltage( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_technology( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_safety( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_camera_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_terminal_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_modem_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_fuelgauge_current( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_fuelgauge_voltage( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_fuelgauge_device_id( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_fuelgauge_mode( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_fuelgauge_accumulate_current( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_clr_fuelgauge_accumulate_current( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_fuelgauge_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_fuelgauge_current_ad( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_charger_transistor_switch( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_vmaxsel( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_imaxsel( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_adc_channel( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_adc_channel_buffered( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_vbatt_calibration_data( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_refresh_vbatt_calibration_data( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_check_startup_voltage( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_log_info( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_post_battery_log_info( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_notify_charger_cable_status( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_notify_battery_charging_status( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_notify_battery_capacity( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_notify_charging_state_machine_enable( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_notify_power_supply_changed( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_calibrate_battery_voltage( struct file* fi_p, unsigned long arg );

static int shbatt_drv_ioctl_cmd_get_wireless_status( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_wireless_switch( struct file* fi_p, unsigned long arg );

static int shbatt_drv_ioctl_cmd_initialize( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_exec_low_battery_check_sequence( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_kernel_time( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_adc_channel_no_conversion( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_hw_revision( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_smem_info( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_log_enable( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_cpu_clock_level( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_backlight_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_battery_health( struct file* fi_p, unsigned long arg );
shbatt_result_t shbatt_api_set_battery_health( shbatt_health_t health );
static shbatt_result_t shbatt_seq_set_battery_health( shbatt_health_t health );
static void shbatt_task_cmd_set_battery_health( shbatt_packet_t* pkt_p );
static int shbatt_drv_ioctl_cmd_set_android_shutdown_timer( struct file* fi_p, unsigned long arg );

static int shbatt_drv_ioctl_cmd_set_depleted_calc_enable( struct file* fi_p, unsigned long arg );
static shbatt_result_t shbatt_api_set_depleted_calc_enable( int val );
static void shbatt_task_cmd_set_depleted_calc_enable( shbatt_packet_t* pkt_p );
static shbatt_result_t shbatt_seq_set_depleted_calc_enable( int val );

static shbatt_packet_t* shbatt_task_get_packet( void );
static void shbatt_task_free_packet( shbatt_packet_t* pkt );
//seq
static shbatt_result_t shbatt_seq_post_battery_log_info( shbatt_batt_log_info_t bli );
static shbatt_result_t shbatt_seq_get_fuelgauge_temperature( int* tmp_p );
static int shbatt_seq_get_bootinfo( void );
static shbatt_result_t shbatt_seq_call_user_space_sequence_executor( void );
static shbatt_result_t shbatt_seq_get_battery_safety( shbatt_safety_t* saf_p );
static shbatt_result_t shbatt_seq_exec_low_battery_check_sequence( int evt );
static shbatt_result_t shbatt_seq_initialize( void );
static shbatt_result_t shbatt_seq_notify_charging_state_machine_enable( shbatt_boolean_t ena );
static shbatt_result_t shbatt_seq_notify_battery_capacity( int cap );
static shbatt_result_t shbatt_seq_notify_battery_charging_status( shbatt_chg_status_t cgs );
static shbatt_result_t shbatt_seq_notify_charger_cable_status( shbatt_cable_status_t cbs );
static shbatt_result_t shbatt_seq_get_battery_log_info( shbatt_batt_log_info_t* bli_p );
static shbatt_result_t shbatt_seq_read_adc_channel_buffered( shbatt_adc_t* adc_p );
static shbatt_result_t shbatt_seq_set_fuelgauge_mode( shbatt_fuelgauge_mode_t mode );
static shbatt_result_t shbatt_seq_get_fuelgauge_current_ad( int* raw_p );
static shbatt_result_t shbatt_seq_clr_fuelgauge_accumulate_current( void );
static shbatt_result_t shbatt_seq_get_fuelgauge_accumulate_current( shbatt_accumulate_current_t* acc_p );
static shbatt_result_t shbatt_seq_get_terminal_temperature( int* tmp_p );
static shbatt_result_t shbatt_seq_get_modem_temperature( int* tmp_p );
static shbatt_result_t shbatt_seq_get_fuelgauge_voltage( int* vol_p );
static shbatt_result_t shbatt_seq_enable_battery_voltage_alarm_int( unsigned short max,unsigned short min,void* cb_p );
static shbatt_result_t shbatt_seq_get_fuelgauge_device_id( unsigned int* dev_p );
static void shbatt_seq_fuelgauge_soc_poll_timer_expire_cb( struct alarm* alm_p );
static void shbatt_seq_fuelgauge_soc_poll_timer_expire_multi_cb( struct alarm* alm_p );
static void shbatt_seq_low_battery_poll_timer_expire_cb( struct alarm* alm_p );
static void shbatt_seq_fatal_battery_poll_timer_expire_cb( struct alarm* alm_p );
static void shbatt_seq_charging_fatal_battery_poll_timer_expire_cb( struct alarm* alm_p );
static void shbatt_seq_low_battery_shutdown_poll_timer_expire_cb( struct alarm* alm_p );
static void shbatt_seq_overcurr_timer_expire_cb( struct alarm* alm_p );
static void shbatt_seq_battery_present_poll_timer_expire_cb( struct alarm* alm_p );
static shbatt_result_t shbatt_api_exec_fuelgauge_soc_poll_sequence( shbatt_soc_t soc );
static shbatt_result_t shbatt_api_exec_overcurr_check_sequence( void );
static shbatt_result_t shbatt_api_exec_battery_present_check_sequence( int seq );
static shbatt_result_t shbatt_seq_disable_battery_voltage_alarm_int( void );
static shbatt_result_t shbatt_api_initialize( void );
static shbatt_result_t shbatt_api_exec_low_battery_check_sequence( int evt );
static int shbatt_drv_register_irq( struct platform_device* dev_p );
static int shbatt_drv_create_ps_attrs( struct platform_device* dev_p );
static int shbatt_drv_set_ps_property( struct power_supply* psy_p, enum power_supply_property psp, const union power_supply_propval* val_p);
static int shbatt_drv_get_ps_property( struct power_supply* psy_p, enum power_supply_property psp, union power_supply_propval* val_p);
static void shbatt_seq_low_battery_voltage_alarm_isr(struct notifier_block *nb, unsigned long status, void *unused);
static void shbatt_seq_fatal_battery_voltage_alarm_isr(struct notifier_block *nb, unsigned long status, void *unused);
static void shbatt_seq_charging_fatal_battery_voltage_alarm_isr(struct notifier_block *nb, unsigned long status, void *unused);
shbatt_result_t shbatt_api_get_calibrate_battery_voltage( int* vol_p );
shbatt_result_t shbatt_api_get_wireless_status( int* vol_p );
shbatt_result_t shbatt_api_set_wireless_switch( int val );
shbatt_result_t shbatt_api_set_log_enable( int val );
shbatt_result_t shbatt_api_get_cpu_clock_level( shbatt_cpu_clock_level_t* val );
shbatt_result_t shbatt_api_get_real_battery_capacity( int* cap_p );
shbatt_result_t shbatt_api_get_depleted_capacity(int* depleted_capacity_per);
shbatt_result_t shbatt_api_set_depleted_battery_flg(int depleted_battery_flg);
shbatt_result_t shbatt_api_get_depleted_battery_flg(int* depleted_battery_flg);

extern struct delayed_work lowbatt_shutdown_struct;
extern struct delayed_work android_shutdown_struct;

static void shbatt_seq_wireless_charge_poll_timer_expire_cb( struct alarm* alm_p );
static shbatt_result_t shbatt_api_notify_wireless_charge_timer_expire( void );
static void shbatt_task_cmd_notify_wireless_charge_timer_expire( shbatt_packet_t* pkt_p );

/* TODO New API add point */

static int shbatt_drv_ioctl_cmd_get_charger_cable_status( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_charging_status( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_capacity( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_voltage( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_health( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_technology( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_current( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_adc_channel( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_vbatt_calibration_data( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_refresh_vbatt_calibration_data( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_charger_transistor_switch( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_battery_transistor_switch( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_vmaxsel( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_imaxsel( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_vtrickle( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_pmic_itrickle( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_check_startup_voltage( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_start_battery_present_check( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_battery_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_charger_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_camera_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_pa_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_xo_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_pmic_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_smbc_charger( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_meas_freq( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_ocv_for_rbatt( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_vsense_for_rbatt( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_vbatt_for_rbatt( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_cc( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_last_good_ocv( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_vsense_avg( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_vbatt_avg( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_auto_enable( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_recalib_adc_device( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_calibrate_battery_voltage( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_vsense_avg_calibration_data( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_refresh_vsense_avg_calibration_data( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_backlight_temperature( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_depleted_capacity( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_depleted_battery_flg( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_depleted_battery_flg( struct file* fi_p, unsigned long arg );
#ifdef CONFIG_SHTMP_SENSOR
static int shbatt_drv_ioctl_cmd_get_apq_temperature( struct file* fi_p, unsigned long arg );
#endif

static int shbatt_drv_ioctl_cmd_get_ave_current_and_ave_voltage( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_read_adc_channels( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_reset_cc( struct file* fi_p, unsigned long arg );

#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_set_batt_temp( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_batt_temp( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_set_cpu_temp( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_cpu_temp( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_rectifier_ic_temp( struct file* fi_p, unsigned long arg );
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */

/* TODO New API add point */

static void shbatt_task( struct work_struct* work_p );
static void shbatt_task_cmd_invalid( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_charger_cable_status( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_charging_status( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_capacity( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_voltage( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_health( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_technology( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_current( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_adc_channel( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_vbatt_calibration_data( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_refresh_vbatt_calibration_data( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_pmic_charger_transistor_switch( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_pmic_battery_transistor_switch( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_pmic_vmaxsel( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_pmic_imaxsel( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_pmic_vtrickle( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_pmic_itrickle( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_check_startup_voltage( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_start_battery_present_check( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_charger_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_camera_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_pa_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_xo_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_pmic_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_smbc_charger( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_meas_freq( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_ocv_for_rbatt( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_vsense_for_rbatt( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_vbatt_for_rbatt( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_cc( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_last_good_ocv( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_vsense_avg( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_read_vbatt_avg( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_auto_enable( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_recalib_adc_device( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_calibrate_battery_voltage( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_wireless_status( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_wireless_switch( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_vsense_avg_calibration_data( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_refresh_vsense_avg_calibration_data( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_battery_present( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_notify_wireless_charger_state_changed(shbatt_packet_t* pkt_p);
static void shbatt_task_cmd_set_log_enable( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_real_battery_capacity( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_backlight_temperature( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_get_depleted_capacity( shbatt_packet_t* pkt_p );
static void shbatt_task_cmd_set_depleted_battery_flg( shbatt_packet_t* pkt_p);
static void shbatt_task_cmd_get_depleted_battery_flg( shbatt_packet_t* pkt_p);

/* TODO New API add point */

static shbatt_result_t shbatt_seq_get_charger_cable_status( shbatt_cable_status_t* cbs_p );
static shbatt_result_t shbatt_seq_get_battery_charging_status( shbatt_chg_status_t* cgs_p );
static shbatt_result_t shbatt_seq_get_battery_capacity( int* cap_p );
static shbatt_result_t shbatt_seq_get_battery_voltage( int* vol_p );
static shbatt_result_t shbatt_seq_get_battery_present( shbatt_present_t* pre_p );
static shbatt_result_t shbatt_seq_set_log_enable( int val );
static shbatt_result_t shbatt_seq_get_battery_health( shbatt_health_t* hea_p );
static shbatt_result_t shbatt_seq_get_battery_technology( shbatt_technology_t* tec_p );
static shbatt_result_t shbatt_seq_get_battery_current( int* cur_p );
static shbatt_result_t shbatt_seq_read_adc_channel( shbatt_adc_t* adc_p );
static shbatt_result_t shbatt_seq_set_vbatt_calibration_data( shbatt_vbatt_cal_t cal );
static shbatt_result_t shbatt_seq_refresh_vbatt_calibration_data( void );
static shbatt_result_t shbatt_seq_set_pmic_charger_transistor_switch( int val );
static shbatt_result_t shbatt_seq_set_pmic_battery_transistor_switch( int val );
static shbatt_result_t shbatt_seq_set_pmic_vmaxsel( int val );
static shbatt_result_t shbatt_seq_set_pmic_imaxsel( int val );
static shbatt_result_t shbatt_seq_set_pmic_vtrickle( int val );
static shbatt_result_t shbatt_seq_set_pmic_itrickle( int val );
static shbatt_result_t shbatt_seq_check_startup_voltage( int* chk_p );
static shbatt_result_t shbatt_seq_start_battery_present_check( void );
static shbatt_result_t shbatt_seq_get_battery_temperature( int* tmp_p );
static shbatt_result_t shbatt_seq_get_charger_temperature( int* tmp_p );
static shbatt_result_t shbatt_seq_get_camera_temperature( int* tmp_p );
static shbatt_result_t shbatt_seq_get_pa_temperature( int* tmp_p );
static shbatt_result_t shbatt_seq_get_xo_temperature( int* tmp_p );
static shbatt_result_t shbatt_seq_get_pmic_temperature( int* tmp_p );
static shbatt_result_t shbatt_seq_set_smbc_charger( shbatt_smbc_chg_t chg );
static shbatt_result_t shbatt_seq_set_meas_freq( int val );
static shbatt_result_t shbatt_seq_read_ocv_for_rbatt( shbatt_adc_conv_uint_t* ocv_p );
static shbatt_result_t shbatt_seq_read_vsense_for_rbatt( shbatt_adc_conv_uint_t* adc_convu_p );
static shbatt_result_t shbatt_seq_read_vbatt_for_rbatt( shbatt_adc_conv_uint_t* vbatt_p );
static shbatt_result_t shbatt_seq_read_cc( shbatt_adc_conv_offset_t* adc_offset_p );
static shbatt_result_t shbatt_seq_read_last_good_ocv( shbatt_adc_conv_uint_t* ocv_p );
static shbatt_result_t shbatt_seq_read_vsense_avg( shbatt_adc_conv_int_t* adc_convi_p );
static shbatt_result_t shbatt_seq_read_vbatt_avg( shbatt_adc_conv_int_t* vbatt_avg_p );
static shbatt_result_t shbatt_seq_auto_enable( int val );
static shbatt_result_t shbatt_seq_recalib_adc_device( void );
static shbatt_result_t shbatt_seq_get_calibrate_battery_voltage( int* vol_p );
static shbatt_result_t shbatt_seq_get_wireless_status( int* vol_p );
static shbatt_result_t shbatt_seq_set_wireless_switch( int val );
static shbatt_result_t shbatt_seq_set_vsense_avg_calibration_data( shbatt_vsense_avg_cal_t vac );
static shbatt_result_t shbatt_seq_refresh_vsense_avg_calibration_data( void );
static shbatt_result_t shbatt_seq_get_cpu_clock_level( shbatt_cpu_clock_level_t* cpu_clock_level_p );
#if defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE1)
static shbatt_result_t shbatt_seq_get_cpu_clock_level_type1( shbatt_cpu_clock_level_t* cpu_clock_level_p );
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE2)
static shbatt_result_t shbatt_seq_get_cpu_clock_level_type2( shbatt_cpu_clock_level_t* cpu_clock_level_p );
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE3)
static shbatt_result_t shbatt_seq_get_cpu_clock_level_type3( shbatt_cpu_clock_level_t* cpu_clock_level_p );
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE4)
static shbatt_result_t shbatt_seq_get_cpu_clock_level_type4( shbatt_cpu_clock_level_t* cpu_clock_level_p );
#else
static shbatt_result_t shbatt_seq_get_cpu_clock_level_default( shbatt_cpu_clock_level_t* cpu_clock_level_p );
#endif
static shbatt_result_t shbatt_seq_get_real_battery_capacity( int* battery_capacity_p );
#ifdef CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM
static shbatt_result_t shbatt_seq_get_backlight_temperature( int* tmp_p );
#endif
#ifdef CONFIG_SHTMP_SENSOR
static shbatt_result_t shbatt_seq_get_apq_temperature( int* tmp_p );
#endif

static void shbatt_task_cmd_notify_charge_switch_status( shbatt_packet_t* pkt_p );
static shbatt_result_t shbatt_seq_notify_charge_switch_status( shbatt_chg_switch_status_t switch_status );
static shbatt_result_t shbatt_seq_get_depleted_capacity( int* depleted_capacity_per );
static shbatt_result_t shbatt_seq_set_depleted_battery_flg( int depleted_battery_flg );
static shbatt_result_t shbatt_seq_get_depleted_battery_flg( int* depleted_battery_flg );

static int shbatt_seq_get_average_current_and_average_voltage(int* cur, int* vol);

static int shbatt_drv_ioctl_cmd_get_average_current( struct file* fi_p, unsigned long arg );
shbatt_result_t shbatt_api_get_average_current( int* average_current );
static void shbatt_task_cmd_get_average_current( shbatt_packet_t* pkt_p );
static shbatt_result_t shbatt_seq_get_average_current( int* average_current );
static int shbatt_drv_ioctl_cmd_calib_ccadc( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_get_system_time( struct file* fi_p, unsigned long arg );

/* TODO New API add point */

static shbatt_result_t shbatt_get_battery_current(int* cur_p);
static int shbatt_drv_open( struct inode* in_p, struct file* fi_p );
static int shbatt_drv_release( struct inode* in_p, struct file* fi_p );
static unsigned int shbatt_drv_poll( struct file* fi_p, poll_table* wait_p );
static long shbatt_drv_ioctl( struct file* fi_p, unsigned int cmd, unsigned long arg );
static int shbatt_drv_create_device( void );
static int shbatt_drv_probe( struct platform_device* dev_p );
static int __devexit shbatt_drv_remove( struct platform_device* dev_p );
static void shbatt_drv_shutdown( struct platform_device* dev_p );
static int __init shbatt_drv_module_init( void );
static void __exit shbatt_drv_module_exit( void );

static int shbatt_drv_resume(struct platform_device * dev_p);
static int shbatt_drv_suspend(struct platform_device * dev_p, pm_message_t state);


#if defined(CONFIG_PM_WIRELESS_CHARGE)

typedef enum
{
	WIRELESS_UNKNOWN,
	WIRELESS_ENABLED,
	WIRELESS_BUSY,
	WIRELESS_DISABLED,
	WIRELESS_FIRMWARE_IS_DEAD,
}shbatt_wireless_disable_state_t;

typedef enum
{
	WIRELESS_NOT_CHARGING,
	WIRELESS_CHARGING,
	WIRELESS_PRE_FULL_CHARGE,
}shbatt_wireless_charging_state_t;

static struct wake_lock shbatt_wireless_wake_lock;

static int shbatt_is_wireless_charging(void);
static shbatt_result_t shbatt_disable_wireless_charger(void);
static shbatt_result_t shbatt_enable_wireless_charger(void);
static int shbatt_drv_ioctl_cmd_get_wireless_disable_phase( struct file* fi_p, unsigned long arg );
static shbatt_result_t shbatt_get_wireless_disable_phase(shbatt_wireless_disable_state_t* state);
static int shbatt_drv_ioctl_cmd_get_wireless_charging_status( struct file* fi_p, unsigned long arg );
static shbatt_result_t shbatt_get_wireless_charging_status(shbatt_wireless_charging_state_t* state);
#endif
static void initialize_wireless_charger(void);

/*+-----------------------------------------------------------------------------+*/
/*| @ FUNCTION TABLE PROTO TYPE DECLARE :										|*/
/*+-----------------------------------------------------------------------------+*/

static struct file_operations shbatt_fops =
{
	.owner			= THIS_MODULE,
	.open			= shbatt_drv_open,
	.release		= shbatt_drv_release,
	.poll			= shbatt_drv_poll,
	.unlocked_ioctl	= shbatt_drv_ioctl,
};

static void (*const shbatt_task_cmd_func[])( shbatt_packet_t* pkt_p ) =
{
	shbatt_task_cmd_invalid,									/*SHBATT_TASK_CMD_INVALID,*/
	shbatt_task_cmd_get_charger_cable_status,					/*SHBATT_TASK_CMD_GET_CHARGER_CABLE_STATUS,*/
	shbatt_task_cmd_get_battery_charging_status,				/*SHBATT_TASK_CMD_GET_BATTERY_CHARGING_STATUS,*/
	shbatt_task_cmd_get_battery_capacity,						/*SHBATT_TASK_CMD_GET_BATTERY_CAPACITY,*/
	shbatt_task_cmd_get_battery_voltage,						/*SHBATT_TASK_CMD_GET_BATTERY_VOLTAGE,*/
	shbatt_task_cmd_get_battery_health, 						/*SHBATT_TASK_CMD_GET_BATTERY_HEALTH,*/
	shbatt_task_cmd_get_battery_technology, 					/*SHBATT_TASK_CMD_GET_BATTERY_TECHNOLOGY,*/
	shbatt_task_cmd_get_battery_current,						/*SHBATT_TASK_CMD_GET_BATTERY_CURRENT,*/
	shbatt_task_cmd_read_adc_channel,							/*SHBATT_TASK_CMD_READ_ADC_CHANNEL,*/
	shbatt_task_cmd_set_vbatt_calibration_data, 				/*SHBATT_TASK_CMD_SET_VBATT_CALIBRATION_DATA,*/
	shbatt_task_cmd_refresh_vbatt_calibration_data, 			/*SHBATT_TASK_CMD_REFRESH_VBATT_CALIBRATION_DATA,*/
	shbatt_task_cmd_set_pmic_charger_transistor_switch, 		/*SHBATT_TASK_CMD_SET_PMIC_CHARGER_TRANSISTOR_SWITCH,*/
	shbatt_task_cmd_set_pmic_battery_transistor_switch, 		/*SHBATT_TASK_CMD_SET_PMIC_BATTERY_TRANSISTOR_SWITCH,*/
	shbatt_task_cmd_set_pmic_vmaxsel,							/*SHBATT_TASK_CMD_SET_PMIC_VMAXSEL,*/
	shbatt_task_cmd_set_pmic_imaxsel,							/*SHBATT_TASK_CMD_SET_PMIC_IMAXSEL,*/
	shbatt_task_cmd_set_pmic_vtrickle,							/*SHBATT_TASK_CMD_SET_PMIC_VTRICKLE,*/
	shbatt_task_cmd_set_pmic_itrickle,							/*SHBATT_TASK_CMD_SET_PMIC_ITRICKLE,*/
	shbatt_task_cmd_check_startup_voltage,						/*SHBATT_TASK_CMD_CHECK_STARTUP_VOLTAGE,*/
	shbatt_task_cmd_start_battery_present_check,				/*SHBATT_TASK_CMD_CHECK_STARTUP_BATTERY_PRESENT_CHECK,*/
	shbatt_task_cmd_get_battery_temperature,					/*SHBATT_TASK_CMD_GET_BATTERY_TEMPERATURE,*/
	shbatt_task_cmd_get_charger_temperature,					/*SHBATT_TASK_CMD_GET_CHARGER_TEMPERATURE,*/
	shbatt_task_cmd_get_camera_temperature, 					/*SHBATT_TASK_CMD_GET_CAMERA_TEMPERATURE,*/
	shbatt_task_cmd_get_pa_temperature, 						/*SHBATT_TASK_CMD_GET_PA_TEMPERATURE,*/
	shbatt_task_cmd_get_xo_temperature, 						/*SHBATT_TASK_CMD_GET_XO_TEMPERATURE,*/
	shbatt_task_cmd_get_pmic_temperature,						/*SHBATT_TASK_CMD_GET_PMIC_TEMPERATURE,*/
	shbatt_task_cmd_set_smbc_charger,							/*SHBATT_TASK_CMD_SET_SMBC_CHARGER,*/
	shbatt_task_cmd_set_meas_freq,								/*SHBATT_TASK_CMD_SET_MEAS_FREQ,*/
	shbatt_task_cmd_read_ocv_for_rbatt, 						/*SHBATT_TASK_CMD_READ_OCV_FOR_RBATT,*/
	shbatt_task_cmd_read_vsense_for_rbatt,						/*SHBATT_TASK_CMD_READ_VSENSE_FOR_RBATT,*/
	shbatt_task_cmd_read_vbatt_for_rbatt,						/*SHBATT_TASK_CMD_READ_VBATT_FOR_RBATT,*/
	shbatt_task_cmd_read_cc,									/*SHBATT_TASK_CMD_READ_CC,*/
	shbatt_task_cmd_read_last_good_ocv, 						/*SHBATT_TASK_CMD_READ_LAST_GOOD_OCV,*/
	shbatt_task_cmd_read_vsense_avg,							/*SHBATT_TASK_CMD_READ_VSENSE_AVG,*/
	shbatt_task_cmd_read_vbatt_avg, 							/*SHBATT_TASK_CMD_READ_VBATT_AVG,*/
	shbatt_task_cmd_auto_enable,								/*SHBATT_TASK_CMD_AUTO_ENABLE,*/
	shbatt_task_cmd_recalib_adc_device, 						/*SHBATT_TASK_CMD_RECALIB_ADC_DEVICE,*/
	shbatt_task_cmd_get_terminal_temperature,					/*SHBATT_TASK_CMD_GET_TERMINAL_TEMPERATURE,*/
	shbatt_task_cmd_get_modem_temperature,						/*SHBATT_TASK_CMD_GET_MODEM_TEMPERATURE,*/
	shbatt_task_cmd_get_fuelgauge_voltage,						/*SHBATT_TASK_CMD_GET_FUELGAUGE_VOLTAGE,*/
	shbatt_task_cmd_get_fuelgauge_device_id,					/*SHBATT_TASK_CMD_GET_FUELGAUGE_DEVICE_ID,*/
	shbatt_task_cmd_set_fuelgauge_mode, 						/*SHBATT_TASK_CMD_SET_FUELGAUGE_MODE,*/
	shbatt_task_cmd_get_fuelgauge_accumulate_current,			/*SHBATT_TASK_CMD_GET_FUELGAUGE_ACCUMULATE_CURRENT,*/
	shbatt_task_cmd_clr_fuelgauge_accumulate_current,			/*SHBATT_TASK_CMD_CLR_FUELGAUGE_ACCUMULATE_CURRENT,*/
	shbatt_task_cmd_get_fuelgauge_temperature,					/*SHBATT_TASK_CMD_GET_FUELGAUGE_TEMPERATURE,*/
	shbatt_task_cmd_get_fuelgauge_current_ad,					/*SHBATT_TASK_CMD_GET_FUELGAUGE_CURRENT_AD,*/
	shbatt_task_cmd_read_adc_channel_buffered,					/*SHBATT_TASK_CMD_READ_ADC_CHANNEL_BUFFERED,*/
	shbatt_task_cmd_get_battery_log_info,						/*SHBATT_TASK_CMD_GET_BATTERY_LOG_INFO,*/
	shbatt_task_cmd_notify_charger_cable_status,				/*SHBATT_TASK_CMD_NOTIFY_CHARGER_CABLE_STATUS,*/
	shbatt_task_cmd_notify_battery_charging_status, 			/*SHBATT_TASK_CMD_NOTIFY_BATTERY_CHARGING_STATUS,*/
	shbatt_task_cmd_notify_battery_capacity,					/*SHBATT_TASK_CMD_NOTIFY_BATTERY_CAPACITY,*/
	shbatt_task_cmd_notify_charging_state_machine_enable,		/*SHBATT_TASK_CMD_NOTIFY_CHARGING_STATE_MACHINE_ENABLE,*/
	shbatt_task_cmd_get_battery_safety, 						/*SHBATT_TASK_CMD_GET_BATTERY_SAFETY,*/
	shbatt_task_cmd_initialize, 								/*SHBATT_TASK_CMD_INITIALIZE,*/
	shbatt_task_cmd_exec_low_battery_check_sequence,			/*SHBATT_TASK_CMD_EXEC_LOW_BATTERY_CHECK_SEQUENCE,*/
	shbatt_task_cmd_post_battery_log_info,						/*SHBATT_TASK_CMD_POST_BATTERY_LOG_INFO,*/
	shbatt_task_cmd_exec_battery_present_check_sequence,		/*SHBATT_TASK_CMD_EXEC_BATTERY_PRESENT_CHECK_SEQUENCE,*/
	shbatt_task_cmd_exec_overcurr_check_sequence,				/*SHBATT_TASK_CMD_EXEC_OVERCURR_CHECK_SEQUENCE,*/
	shbatt_task_cmd_exec_fuelgauge_soc_poll_sequence,			/*SHBATT_TASK_CMD_EXEC_FUELGAUGE_SOC_POLL_SEQUENCE,*/
	shbatt_task_cmd_get_calibrate_battery_voltage,				/*SHBATT_TASK_CMD_GET_CALIBRATE_BATTERY_VOLTAGE*/
	shbatt_task_cmd_get_wireless_status,
	shbatt_task_cmd_set_wireless_switch,
	shbatt_task_cmd_set_vsense_avg_calibration_data,			/*SHBATT_TASK_CMD_SET_VSENSE_AVG_CALIBRATION_DATA,*/
	shbatt_task_cmd_refresh_vsense_avg_calibration_data,		/*SHBATT_TASK_CMD_REFRESH_VSENSE_AVG_CALIBRATION_DATA,*/
	shbatt_task_cmd_get_battery_present,						/*SHBATT_TASK_CMD_GET_BATTERY_PRESENT*/
	shbatt_task_cmd_notify_wireless_charger_state_changed,		/*SHBATT_TASK_CMD_NOTIFY_WIRELESS_CHARGER_STATE_CHANGED*/
	shbatt_task_cmd_set_log_enable,								/*SHBATT_TASK_CMD_SET_LOG_ENABLE*/
	shbatt_task_cmd_get_real_battery_capacity,					/*SHBATT_TASK_CMD_GET_REAL_BATTERY_CAPACITY*/
	shbatt_task_cmd_get_backlight_temperature, 					/*SHBATT_TASK_CMD_GET_BACKLIGHT_TEMPERATURE,*/
	shbatt_task_cmd_notify_charge_switch_status,				/*SHBATT_TASK_CMD_NOTIFY_CHARGE_SWITCH_STATUS*/
	shbatt_task_cmd_get_depleted_capacity,						/*SHBATT_TASK_CMD_GET_DEPLETED_CAPACITY*/
	shbatt_task_cmd_set_depleted_battery_flg,						/*SHBATT_TASK_CMD_SET_DEPLETED_BATTERY_FLG*/
	shbatt_task_cmd_get_depleted_battery_flg,						/*SHBATT_TASK_CMD_GET_DEPLETED_BATTERY_FLG*/
	shbatt_task_cmd_set_battery_health,							/*SHBATT_TASK_CMD_SET_BATTERY_HEALTH*/
	shbatt_task_cmd_notify_wireless_charge_timer_expire,		/*SHBATT_TASK_CMD_NOTIFY_WIRELESS_CHARGE_TIMER_EXPIRE*/
	shbatt_task_cmd_get_average_current,						/*SHBATT_TASK_CMD_GET_AVERAGE_CURRENT,*/
	shbatt_task_cmd_set_depleted_calc_enable,					/*SHBATT_TASK_CMD_SET_DEPLETED_CALC_ENABLE*/
/* TODO New API add point */
};

typedef enum hdq_bit_state_tag
{
	HDQ_BIT_STATE_LO = 0x00,
	HDQ_BIT_STATE_HI = 0x01
} hdq_bit_state_t;

#define HDQ_WAIT_INTERVAL						5		// 5us
#define HDQ_WAIT_MS_CYCLE_TIME					190		// 190us
#define HDQ_WAIT_MS_WRITE_LO					14		// 14us
#define HDQ_WAIT_MS_WRITE_LO_HI					110		// 110us
#define HDQ_WAIT_MS_WRITE_HI					190		// 190us
#define HDQ_WAIT_MS_READ_TIMEOUT				500		// 250us
#define HDQ_WAIT_MS_DEVICE_WRITE_1				65		// 65us
#define HDQ_WAIT_MS_DEVICE_WRITE_0				155		// 155us
#define HDQ_WAIT_MS_BREAK_TIME					190		// 190us
#define HDQ_WAIT_MS_BREAK_TIME_RECOVERY			40		// 40us
#define HDQ_WAIT_MS_WAKEUP_TIME					20*1000	// 20ms

#define HDQ_GPIO_PORT_NUM						PM8921_GPIO_PM_TO_SYS(10)		// GPIO port

#define HDQ_REGISTER_BIT_MASK					0x40
#define HDQ_RW_BIT_MASK							0x80

#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
static shbatt_result_t hdq_initialize(void);
static shbatt_result_t hdq_initialize_in_lock(void);
static shbatt_result_t hdq_read_byte(const unsigned char address, unsigned char* read_byte);
static shbatt_result_t hdq_write_byte(const unsigned char address, const unsigned char write_byte);
static shbatt_result_t hdq_tx(const unsigned char byte);
static shbatt_result_t hdq_rx(unsigned char* byte);
static shbatt_result_t hdq_bit_out(const hdq_bit_state_t bit);
static shbatt_result_t hdq_bit_in(hdq_bit_state_t* bit);
static shbatt_result_t sleep_wakeup(void);
static shbatt_result_t hdq_break(void);
static shbatt_result_t wait_ms_from_tv(struct timeval* tv_start, const int ms_wait);
static void timersub(struct timeval *tv_start, unsigned long* diff);
static int is_hdq_port_hi(void);
static int is_hdq_port_lo(void);
static int shbatt_drv_ioctl_cmd_read_hdq( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_write_hdq( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_check_battery_auth( struct file* fi_p, unsigned long arg );
static int shbatt_drv_ioctl_cmd_set_battery_auth( struct file* fi_p, unsigned long arg );
static shbatt_result_t shbatt_seq_initialize_battery_trace_data( void );

#define SHBATT_BATT_TRACE_PACK_DATE_LENGTH			4
#define SHBATT_BATT_TRACE_CELL_DATE_LENGTH			4
#define SHBATT_BATT_TRACE_PACK_LINE_INFO_LENGTH		3
#define SHBATT_BATT_TRACE_CELL_LINE_INFO_LENGTH		3
#define SHBATT_BATT_TRACE_MANUFACTUR_NUM_LENGTH		5
#define SHBATT_BATT_TRACE_MAKER_NAME_LENGTH			2

unsigned char pack_date_param[SHBATT_BATT_TRACE_PACK_DATE_LENGTH];
unsigned char cell_date_param[SHBATT_BATT_TRACE_CELL_DATE_LENGTH];
unsigned char pack_line_info_param[SHBATT_BATT_TRACE_PACK_LINE_INFO_LENGTH];
unsigned char cell_line_info_param[SHBATT_BATT_TRACE_CELL_LINE_INFO_LENGTH];
unsigned char manufactur_num_param[SHBATT_BATT_TRACE_MANUFACTUR_NUM_LENGTH];
unsigned char maker_name_param[SHBATT_BATT_TRACE_MAKER_NAME_LENGTH];
static int shbatt_cur_battery_auth = -1;
static int shbatt_battery_trace_data_read_finish = 0;

module_param_string(pack_date, pack_date_param, 			SHBATT_BATT_TRACE_PACK_DATE_LENGTH,			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param_string(cell_date, cell_date_param, 			SHBATT_BATT_TRACE_CELL_DATE_LENGTH,			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param_string(pack_line, pack_line_info_param,		SHBATT_BATT_TRACE_PACK_LINE_INFO_LENGTH,	S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param_string(cell_line, cell_line_info_param,		SHBATT_BATT_TRACE_CELL_LINE_INFO_LENGTH,	S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param_string(pack_manu_num, manufactur_num_param,	SHBATT_BATT_TRACE_MANUFACTUR_NUM_LENGTH,	S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param_string(maker_name, maker_name_param,			SHBATT_BATT_TRACE_MAKER_NAME_LENGTH,		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param_named(batt_auth, shbatt_cur_battery_auth, int,		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

typedef enum
{
	HDQ_INITIAL_NONE,
	HDQ_INITIALIZED,
	HDQ_INITIALIZING,
	HDQ_INITIAL_FAIL,
}shbatt_initial_state_t;

static volatile shbatt_initial_state_t shbatt_hdq_initial_state = HDQ_INITIAL_NONE;

#else /* CONFIG_PM_SUPPORT_BATT_AUTH */
static shbatt_result_t hdq_initialize(void);
static shbatt_result_t shbatt_seq_initialize_battery_trace_data( void );
#endif /* CONFIG_PM_SUPPORT_BATT_AUTH */

#define SHBATT_BATTERY_LOG_CACHE_PRE_INITIALIZE_MAX (20)
static int shbatt_battery_log_cache_pre_initialize_count = 0;
static shbatt_batt_log_info_t shbatt_battery_log_cache_pre_initialize[SHBATT_BATTERY_LOG_CACHE_PRE_INITIALIZE_MAX];

#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C

static atomic_t wlchg_disable_charge_count;
static atomic_t wlchg_rewrite_atomic;

#if defined(CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C) || defined(CONFIG_PM_SUPPORT_BATT_AUTH)
int wlchg_ic_power_on(void);
int wlchg_ic_power_off(void);
#endif


/************************************************
			Firmware rewrite
************************************************/
#include <linux/firmware.h>
#include <linux/i2c.h>


#define WLCHG_FIRMWARE_NAME "pwm/wirelesschg/WLCHG_FIRMWARE.bin"

static const uint8_t WLCHG_STATUS_DONE = 0x00;
static const uint8_t WLCHG_STATUS_BUSY = 0x55;
static const uint8_t WLCHG_STATUS_SUM_ERROR = 0xAA;
static const uint8_t WLCHG_STATUS_FAIL = 0xFF;
#define WLCHG_FLASH_DATA_SIZE 16
#ifdef WLCHG_BOOTLOADER_V1_0
#define WLCHG_SUM_SIZE 0
#else
#define WLCHG_SUM_SIZE 1
#endif	/* WLCHG_BOOTLOADER_V1_0 */
#define WLCHG_FLASH_WRITE_SIZE  (WLCHG_FLASH_DATA_SIZE + WLCHG_SUM_SIZE)
#define WLCHG_I2C_MAX_SIZE_PER_ACCESS 20

static struct device* shbatt_dev_p = NULL;

typedef struct
{
	struct i2c_client *client;
}wlchg_i2c_client_t;

static wlchg_i2c_client_t *wlchg_i2c_client;

static int wlchg_i2c_probe(struct i2c_client* clt_p, const struct i2c_device_id* id_p);
static int wlchg_i2c_remove(struct i2c_client* clt_p);

static const struct i2c_device_id wlchg_i2c_gsbi1_id[] = {
	{ "wlchg_i2c_gsbi1", 0},
	{ }
};
static const struct i2c_device_id wlchg_i2c_gsbi2_id[] = {
	{ "wlchg_i2c_gsbi2", 0},
	{ }
};

static struct i2c_driver wlchg_i2c_driver = {
	.probe	= wlchg_i2c_probe,
	.remove	= wlchg_i2c_remove,
};


static int wlchg_i2c_read(uint8_t sub_address, uint8_t* read_buf, int size);
static int wlchg_i2c_write(uint8_t sub_address, uint8_t *write_buf, int size);
static int wlchg_firmware_update(void);
static int wlchg_firmware_update_from_buffer(const u8* data, size_t size);

static void do_enable_wireless_charger(void);
static void do_disable_wireless_charger(void);

inline int wlchg_i2c_read_byte(uint8_t sub_address, uint8_t* rd_data)
{
	return wlchg_i2c_read(sub_address, rd_data, 1);
}
inline int wlchg_i2c_write_byte(uint8_t sub_address, uint8_t wr_data)
{
	return wlchg_i2c_write(sub_address, &wr_data, 1);
}

static int wlchg_get_revision_from_ic(uint8_t* fw_revision)
{
	return wlchg_i2c_read_byte(0x41, fw_revision);
}

static int wlchg_get_revision_from_fw(const struct firmware *fw, uint8_t* fw_revision)
{
	const uint8_t* last_16_bytes;
	last_16_bytes = fw->data + fw->size - 16;
	
	*fw_revision = last_16_bytes[1] ^ 0xF4;
	
	return 0;
}
static int wlchg_get_product_flg(int *is_procudt_flg_on)
{
	sharp_smem_common_type *smem = NULL;
	smem = sh_smem_get_common_address();
	if(smem == NULL)
	{
		return -ENOMEM;
	}
	
	*is_procudt_flg_on = (smem->shdiag_FlagData & GISPRODUCT_F_MASK) != 0;

	return 0;
}
static struct workqueue_struct* wlchg_fw_rewrite_workque = NULL;
typedef struct
{
	struct work_struct work_struct_header;
	const struct firmware* fw;
}wlchg_fw_rewrite_work_t;
static wlchg_fw_rewrite_work_t wlchg_fw_rewrite_work;
static void wlchg_fw_rewrite_task( struct work_struct* work_p )
{
	const struct firmware *fw;
	int result;

	static const int retry_max = 10;
	int i;

	wlchg_fw_rewrite_work_t* work = (wlchg_fw_rewrite_work_t*)work_p;
	
	fw = work->fw;

	printk(KERN_INFO"begin wlchg fw rewrite\n");
	for(i = 0; i < retry_max; i++)
	{
		result = wlchg_firmware_update_from_buffer(fw->data, fw->size);
		if(result == 0)
		{
			break;
		}
	
		if(result != 0)
		{
			SHBATT_ERROR("wlchg_firmware_update_from_buffer() failed at %s.result=%d\n", __FUNCTION__, result);
		}
	}
	
	printk(KERN_INFO"finish firmware rewrite.\n");

	release_firmware(fw);
	
	flush_workqueue(wlchg_fw_rewrite_workque);
	
	destroy_workqueue(wlchg_fw_rewrite_workque);
}


inline int wlchg_boot_bootloader(void)
{
#ifdef WLCHG_BOOTLOADER_V1_0
	return 0;
#else
	return wlchg_i2c_write(0xF0, NULL, 0);
#endif	/* WLCHG_BOOTLOADER_V1_0 */
}

inline int wlchg_set_write_address(uint16_t flash_address)
{
	int ret;
	static const int wait_after_address_write = 20 *1000;

	uint8_t write_buf[2];

	uint8_t* hi_byte = &write_buf[0];
	uint8_t* lo_byte = &write_buf[1];
	
	*hi_byte = (flash_address >> 8)& 0xFF;
	*lo_byte = flash_address & 0xFF;
	
	ret = wlchg_i2c_write(0xF1, write_buf, sizeof(write_buf));
	if(ret < 0)return ret;
	
	usleep(wait_after_address_write);
	
	return 0;
	
}

typedef struct
{
	uint8_t buf[WLCHG_FLASH_WRITE_SIZE];
}wlchg_flash_data_t;

inline void wlchg_append_sum(wlchg_flash_data_t* data)
{
	uint8_t sum = 0;
	int i;
	for( i = 0; i < ARRAY_SIZE(data->buf) - 1/*sum*/; i++)
	{
		sum += data->buf[i];
	}
	data->buf[ARRAY_SIZE(data->buf) - 1] = sum;
}

inline int wlchg_write_flash(wlchg_flash_data_t* write_data)
{
	int ret;

	ret = wlchg_i2c_write(0xF2, write_data->buf, sizeof(write_data->buf));
	if(ret < 0)return ret;
	
	return 0;
}

inline int wlchg_read_flash(wlchg_flash_data_t* read_data)
{
	return wlchg_i2c_read(0xF3, read_data->buf, WLCHG_FLASH_DATA_SIZE);
}

inline int wlchg_finish_rewrite(uint8_t* hw_rev, uint8_t* sw_rev)
{
	uint8_t read_buf[2];
	uint8_t *h = &read_buf[0];
	uint8_t *s = &read_buf[1];
	int ret;
	ret = wlchg_i2c_read(0xF4, read_buf, sizeof(read_buf));
	if(ret < 0)return ret;
	
	*hw_rev = *h;
	*sw_rev = *s;
	
	return 0;
}

inline int wlchg_read_status(uint8_t* status)
{
	return wlchg_i2c_read_byte(0xF5, status);
}

inline int wlchg_firmware_get_fver(uint8_t* fver)
{
	return wlchg_i2c_read_byte(0x41, fver);
}

inline int wlchg_firmware_get_hwver(uint8_t* hwver)
{
	return wlchg_i2c_read_byte(0x40, hwver);
}

/************************************************
			Calibration
************************************************/
#define COMMAND_REG 0xA4
#define RESULT_REG 0xA5

#define COMMAND_TO_ANSWER(command)	(0xA0 | ((command)&0x0F))
#define IS_COMMAND_ANSWER(data)	((data & 0xF0) == 0xA0)

inline int wlchg_goto_calibration_mode(void)
{
	return wlchg_i2c_write_byte(COMMAND_REG, 0x20);
}
inline int wlchg_read_command_result_reg(uint8_t *result)
{
	uint8_t reg_data = 0;
	int err;
	err = wlchg_i2c_read_byte(RESULT_REG, &reg_data);
	if(err == 0)
	{
		*result = reg_data;
	}
	return err;
}
inline int wlchg_wait_for_calibration_answer(void)
{
	static const int retry_max = 10;
	static const int wait_time = 100 * 1000;
	uint8_t reg_data = 0;
	int result;
	int try_count = 0;

	do
	{
		if(try_count++ > retry_max)
		{
			SHBATT_ERROR("%s timed out\n", __FUNCTION__);
			result = -ETIMEDOUT;
			break;
		}
		usleep(wait_time);
		result = wlchg_i2c_read_byte(COMMAND_REG, &reg_data);
		if(result != 0)
		{
			return result;
		}
	}
	while(!IS_COMMAND_ANSWER(reg_data));
	return result;
}

inline int wlchg_write_wpc_serial_no(uint16_t serial_no)
{
	uint8_t write_data[10];
	uint8_t* command = &write_data[0];
	/*1-5 is dummy zero.*/
	uint8_t* serial_hi = &write_data[6];
	uint8_t* serial_lo = &write_data[7];
	uint8_t* command_hi = &write_data[8];
	uint8_t* command_lo = &write_data[9];
	
	memset(write_data, 0x00, sizeof(write_data));
	
	*command = 0x28;
	*serial_hi = (serial_no >> 8) & 0xFF;
	*serial_lo = serial_no & 0xFF;
	*command_hi = 0x00;
	*command_lo = *command;

	return wlchg_i2c_write(COMMAND_REG, write_data, sizeof(write_data));
}

static int shbatt_wlchg_go_to_calibration_mode(void)
{
	int result;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		uint8_t command_result = 0;
		
		wlchg_ic_power_on();
		
		result = wlchg_goto_calibration_mode();
		if(result != 0)
		{
			SHBATT_ERROR("%s wlchg_goto_calibration_mode() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		result = wlchg_wait_for_calibration_answer();
		if(result != 0)
		{
			SHBATT_ERROR("%s wlchg_wait_for_calibration_answer() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		result = wlchg_read_command_result_reg(&command_result);
		if(result != 0)
		{
			SHBATT_ERROR("%s wlchg_read_command_result_reg() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		if(command_result==0)
		{
			SHBATT_ERROR("%s wlchg_read_command_result_reg() failed.command_result=%02x\n", __FUNCTION__, command_result);
			result = -1;
			break;
		}
	}while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}
static int shbatt_wlchg_voltage_calibration(shbatt_wlchg_voltage_calibration_t* cal)
{
	int result;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		uint8_t write_data[10];
		uint8_t *command = &write_data[0];
		/*1-5 is dummy zero.*/
		uint8_t *setting_voltage_hi = &write_data[6];
		uint8_t *setting_voltage_lo = &write_data[7];
		uint8_t *command_hi		= &write_data[8];
		uint8_t *command_lo		= &write_data[9];

		uint8_t read_data[4];
		uint16_t temp;
		uint8_t* result_voltage_hi = &read_data[0];
		uint8_t* result_voltage_lo = &read_data[1];
		uint8_t* result_set_voltage_hi = &read_data[2];
		uint8_t* result_set_voltage_lo = &read_data[3];
		
		memset(write_data, 0x00, sizeof(write_data));

		*command = 0x21;
		*setting_voltage_hi = (cal->setting_voltage_mv >> 8) & 0xFF;
		*setting_voltage_lo = cal->setting_voltage_mv & 0xFF;
		*command_hi = 0x00;
		*command_lo = *command;
		result = wlchg_i2c_write(COMMAND_REG, write_data, sizeof(write_data));
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_i2c_write() failed.result=%d\n", __FUNCTION__, result);
			break;
		}

		result = wlchg_wait_for_calibration_answer();
		if(result != 0)
		{
			SHBATT_ERROR("%s wlchg_wait_for_calibration_answer() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		result = wlchg_i2c_read(0xAA, read_data, sizeof(read_data));
		if(result != 0)
		{
			SHBATT_ERROR("%s wlchg_i2c_read() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		temp = *result_voltage_hi;
		temp <<= 8;
		temp |= *result_voltage_lo;
		cal->result_voltage_mv = temp;
		
		temp = *result_set_voltage_hi;
		temp <<= 8;
		temp |= *result_set_voltage_lo;
		cal->result_set_voltage_mv = temp;
		
	}while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

static int shbatt_wlchg_voltage_and_current_calibration(shbatt_wlchg_voltage_and_current_calibration_t* cal)
{
	int result;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		uint8_t write_data[12];
		uint8_t *command = &write_data[0];
		/*1-5 is dummy zero.*/
		uint8_t* setting_voltage_hi = &write_data[6];
		uint8_t* setting_voltage_lo = &write_data[7];
		uint8_t* setting_current_hi = &write_data[8];
		uint8_t* setting_current_lo = &write_data[9];
		uint8_t* command_hi = &write_data[10];
		uint8_t* command_lo = &write_data[11];

		uint8_t read_data[8];
		uint8_t* result_voltage_hi = &read_data[0];
		uint8_t* result_voltage_lo = &read_data[1];
		uint8_t* result_current_hi = &read_data[2];
		uint8_t* result_current_lo = &read_data[3];
		uint8_t* result_set_voltage_hi = &read_data[4];
		uint8_t* result_set_voltage_lo = &read_data[5];
		uint8_t* result_set_current_hi = &read_data[6];
		uint8_t* result_set_current_lo = &read_data[7];
		uint16_t temp;

		*command = 0x22;
		*setting_voltage_hi = (cal->setting_voltage_mv >> 8) & 0xFF;
		*setting_voltage_lo = cal->setting_voltage_mv & 0xFF;
		*setting_current_hi = (cal->setting_current_ma >> 8) & 0xFF;
		*setting_current_lo = cal->setting_current_ma & 0xFF;
		*command_hi = 0x00;
		*command_lo = *command;

		result = wlchg_i2c_write(COMMAND_REG, write_data, sizeof(write_data));
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_i2c_write() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		result = wlchg_wait_for_calibration_answer();
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_wait_for_calibration_answer() failed.result=%d\n", __FUNCTION__, result);
			break;
		}

		result = wlchg_i2c_read(0xAA, read_data, sizeof(read_data));
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_i2c_read() failed.result=%d\n", __FUNCTION__, result);
			break;
		}

		temp = *result_voltage_hi;
		temp <<= 8;
		temp |= *result_voltage_lo;
		cal->result_voltage_mv = temp;
		
		temp = *result_current_hi;
		temp <<= 8;
		temp |= *result_current_lo;
		cal->result_current_ma = temp;
		
		temp = *result_set_voltage_hi;
		temp <<= 8;
		temp |= *result_set_voltage_lo;
		cal->result_set_voltage_mv = temp;
		
		temp = *result_set_current_hi;
		temp <<= 8;
		temp |= *result_set_current_lo;
		cal->result_set_current_ma = temp;
	}while(0);	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

static int shbatt_wlchg_current_calibration(shbatt_wlchg_current_calibration_t* cal)
{
	int result;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		uint8_t write_data[10];
		uint8_t *command = &write_data[0];
		/*1-5 is dummy zero.*/
		uint8_t* setting_current_hi = &write_data[6];
		uint8_t* setting_current_lo = &write_data[7];
		uint8_t* command_hi = &write_data[8];
		uint8_t* command_lo = &write_data[9];

		uint8_t read_data[4];
		uint8_t* result_current_hi = &read_data[0];
		uint8_t* result_current_lo = &read_data[1];
		uint8_t* result_set_current_hi = &read_data[2];
		uint8_t* result_set_current_lo = &read_data[3];
		uint16_t temp;

		memset(write_data, 0x00, sizeof(write_data));
		*command = 0x23;
		*setting_current_hi = (cal->setting_current_ma >> 8) & 0xFF;
		*setting_current_lo = cal->setting_current_ma & 0xFF;
		*command_hi = 0x00;
		*command_lo = *command;

		result = wlchg_i2c_write(COMMAND_REG, write_data, sizeof(write_data));
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_i2c_write() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		result = wlchg_wait_for_calibration_answer();
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_wait_for_calibration_answer() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		result = wlchg_i2c_read(0xAA, read_data, sizeof(read_data));
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_i2c_read() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		temp = *result_current_hi;
		temp <<= 8;
		temp |= *result_current_lo;
		cal->result_current_ma = temp;
		
		temp = *result_set_current_hi;
		temp <<= 8;
		temp |= *result_set_current_lo;
		cal->result_set_current_ma = temp;
	}while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

static int shbatt_wlchg_write_wpc_serial_no(uint16_t serial_no)
{
	int result;
	uint8_t command_result = 0;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = wlchg_write_wpc_serial_no(serial_no);
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_write_wpc_serial_no() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		usleep(200*1000 + 50*1000);
		result = wlchg_wait_for_calibration_answer();
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_wait_for_calibration_answer() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		result = wlchg_read_command_result_reg(&command_result);
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_read_command_result_reg() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		if(command_result == 0)
		{
			result = -1;
			SHBATT_ERROR("%s  wlchg_read_command_result_reg() failed.command_result=%d\n", __FUNCTION__, command_result);
			break;
		}

	}while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

static int shbatt_wlchg_get_calibrated_value(shbatt_wlchg_calibrated_value_t* calibrated_value)
{
	int result;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		uint8_t read_data[1 + 1 + 2 + 2 + 2 + 2 + 2 + 2 + 2];
		
		uint8_t* battery_temp	= &read_data[0];
		uint8_t* ic_temp		= &read_data[1];
		uint8_t* voltage_hi		= &read_data[2];
		uint8_t* voltage_lo		= &read_data[3];
		uint8_t* current_hi		= &read_data[4];
		uint8_t* current_lo		= &read_data[5];
		uint8_t* serial_hi		= &read_data[6];
		uint8_t* serial_lo		= &read_data[7];
		uint8_t* v_gain_hi		= &read_data[8];
		uint8_t* v_gain_lo		= &read_data[9];
		uint8_t* v_offset_hi	= &read_data[10];
		uint8_t* v_offset_lo	= &read_data[11];
		uint8_t* i_gain_hi		= &read_data[12];
		uint8_t* i_gain_lo		= &read_data[13];
		uint8_t* i_offset_hi	= &read_data[14];
		uint8_t* i_offset_lo	= &read_data[15];
		
		uint16_t temp;

		
		
		result = wlchg_i2c_write_byte(COMMAND_REG, 0x29);
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_i2c_write() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		result = wlchg_wait_for_calibration_answer();
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_wait_for_calibration_answer() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		result = wlchg_i2c_read(0xA8, read_data, sizeof(read_data));
		if(result != 0)
		{
			SHBATT_ERROR("%s  wlchg_i2c_read() failed.result=%d\n", __FUNCTION__, result);
			break;
		}
		
		calibrated_value->battery_temp	= *battery_temp;
		calibrated_value->ic_temp		= *ic_temp;
		
		temp = *voltage_hi;
		temp <<= 8;
		temp |= *voltage_lo;
		calibrated_value->voltage_mv	= temp;
		
		temp = *current_hi;
		temp <<= 8;
		temp |= *current_lo;
		calibrated_value->current_ma	= temp;
		
		temp = *serial_hi;
		temp <<= 8;
		temp |= *serial_lo;
		calibrated_value->wpc_serial_no = temp;
		
		temp = *v_gain_hi;
		temp <<= 8;
		temp |= *v_gain_lo;
		calibrated_value->v_gain = temp;
		
		temp = *v_offset_hi;
		temp <<= 8;
		temp |= *v_offset_lo;
		calibrated_value->v_offset = (int16_t)temp;
		
		temp = *i_gain_hi;
		temp <<= 8;
		temp |= *i_gain_lo;
		calibrated_value->i_gain = temp;
		
		temp = *i_offset_hi;
		temp <<= 8;
		temp |= *i_offset_lo;
		calibrated_value->i_offset = (int16_t)temp;

		
	}while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}



#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */



/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC FUNCTION'S CODE AREA : 											|*/
/*+-----------------------------------------------------------------------------+*/
shbatt_result_t shbatt_api_get_charger_cable_status( shbatt_cable_status_t* cbs_p )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();
		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_CHARGER_CABLE_STATUS;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.cbs_p = cbs_p;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_battery_charging_status( shbatt_chg_status_t* cgs_p )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		pkt_p = shbatt_task_get_packet();
		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_BATTERY_CHARGING_STATUS;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.cgs_p = cgs_p;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_battery_capacity( int* cap_p )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_BATTERY_CAPACITY;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.cap_p = cap_p;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_battery_voltage( int* vol_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_get_battery_voltage(vol_p);	
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_battery_present( shbatt_present_t* pre_p )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_BATTERY_PRESENT;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.pre_p = pre_p;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_battery_health( shbatt_health_t* hea_p )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_BATTERY_HEALTH;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.hea_p = hea_p;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_battery_technology( shbatt_technology_t* tec_p )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();
		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_BATTERY_TECHNOLOGY;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.tec_p = tec_p;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);

		SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	}
	while(0);

	return result;
}

shbatt_result_t shbatt_api_get_battery_current( int* cur_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		result = shbatt_get_battery_current(cur_p);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_adc_channel( shbatt_adc_t* adc_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_read_adc_channel(adc_p);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_vbatt_calibration_data( shbatt_vbatt_cal_t cal )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		shbatt_seq_set_vbatt_calibration_data(cal);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_refresh_vbatt_calibration_data( void )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{

		result = shbatt_seq_refresh_vbatt_calibration_data();
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_pmic_charger_transistor_switch( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_set_pmic_charger_transistor_switch(val);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_pmic_battery_transistor_switch( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_set_pmic_battery_transistor_switch(val);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_pmic_vmaxsel( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_set_pmic_vmaxsel(val);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_pmic_imaxsel( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_set_pmic_imaxsel(val);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_pmic_vtrickle( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_set_pmic_vtrickle(val);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_pmic_itrickle( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_set_pmic_itrickle(val);
	}
	while(0);
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_check_startup_voltage( int* chk_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		result = shbatt_seq_check_startup_voltage(chk_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_start_battery_present_check( void )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();
		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_CHECK_STARTUP_BATTERY_PRESENT_CHECK;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.val	 = 0;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_battery_temperature( int* tmp_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		result = shbatt_seq_get_battery_temperature(tmp_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_charger_temperature( int* tmp_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_get_charger_temperature(tmp_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_camera_temperature( int* tmp_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_get_camera_temperature(tmp_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_backlight_temperature( int* tmp_p )
{
#ifndef CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM
	SHBATT_ERROR("%s : no support function.\n",__FUNCTION__);
	return -EPERM;
#else  /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_get_backlight_temperature(tmp_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
#endif /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
}

shbatt_result_t shbatt_api_get_pa_temperature( int* tmp_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_get_pa_temperature(tmp_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_xo_temperature( int* tmp_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		result = shbatt_seq_get_xo_temperature(tmp_p);
	}while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_pmic_temperature( int* tmp_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		result = shbatt_seq_get_pmic_temperature(tmp_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_smbc_charger( shbatt_smbc_chg_t chg )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_set_smbc_charger(chg);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_meas_freq( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_set_meas_freq(val);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_ocv_for_rbatt( shbatt_adc_conv_uint_t* ocv_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_read_ocv_for_rbatt(ocv_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_vsense_for_rbatt ( shbatt_adc_conv_uint_t* adc_convu_p)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_read_vsense_for_rbatt(adc_convu_p);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_vbatt_for_rbatt( shbatt_adc_conv_uint_t* vbatt_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_read_vbatt_for_rbatt(vbatt_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_cc( shbatt_adc_conv_long_t* adc_convl_p )
{
	return shbatt_api_read_cc_offset( adc_convl_p, 0 );
}

shbatt_result_t shbatt_api_read_cc_offset( shbatt_adc_conv_long_t* adc_convl_p, int offset )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	shbatt_adc_conv_offset_t adc_conv_offset;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		adc_conv_offset.offset = offset;

		result = shbatt_seq_read_cc(&adc_conv_offset);

		if(result == SHBATT_RESULT_SUCCESS)
		{
			adc_convl_p->adc_code = adc_conv_offset.adc_convl.adc_code;
			adc_convl_p->measurement = adc_conv_offset.adc_convl.measurement;
		}
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_last_good_ocv( shbatt_adc_conv_uint_t* ocv_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_read_last_good_ocv(ocv_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_vsense_avg ( shbatt_adc_conv_int_t* adc_convi_p)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_read_vsense_avg(adc_convi_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_vbatt_avg( shbatt_adc_conv_int_t* vbatt_avg_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_read_vbatt_avg(vbatt_avg_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_auto_enable( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_auto_enable(val);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_recalib_adc_device( void )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_recalib_adc_device();
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_calibrate_battery_voltage( int* vol_p )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();
		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_CALIBRATE_BATTERY_VOLTAGE;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.vol_p = vol_p;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_wireless_status( int* vol_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}	
		result = shbatt_seq_get_wireless_status(vol_p);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;

}

shbatt_result_t shbatt_api_set_wireless_switch( int val )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_set_wireless_switch(val);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_vsense_avg_calibration_data( shbatt_vsense_avg_cal_t vac )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_set_vsense_avg_calibration_data(vac);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_refresh_vsense_avg_calibration_data( void )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_refresh_vsense_avg_calibration_data();
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_log_enable( int val )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();
		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_SET_LOG_ENABLE;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.val	 = val;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_apq_temperature( int* tmp_p )
{
#ifndef CONFIG_SHTMP_SENSOR
	SHBATT_ERROR("%s : no support function.\n",__FUNCTION__);
	return -EPERM;
#else
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		result = shbatt_seq_get_apq_temperature(tmp_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
#endif /* CONFIG_SHTMP_SENSOR */
}
/* TODO New API add point */

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL FUNCTION'S CODE AREA :												|*/
/*+-----------------------------------------------------------------------------+*/
static int shbatt_drv_open( struct inode* in_p, struct file* fi_p )
{
	return 0;
}

static int shbatt_drv_release( struct inode* in_p, struct file* fi_p )
{
	return 0;
}

int l22_vreg_power_on(bool on)
{
	int rc = 0;

	if (!l22_vreg)
	{
		pr_err("%s: failed to set voltage for %s, l22_vreg is NULL\n", __FUNCTION__, l22_vreg_name);
		return -EINVAL;
	}

	if (on && !l22_vreg_on)
	{
		rc = regulator_set_voltage(l22_vreg, L22_MIN_UV, L22_MAX_UV);
		if (rc)
		{
			pr_err("%s: failed to set voltage for %s, rc=%d\n", __FUNCTION__, l22_vreg_name, rc);
			return rc;
		}

		rc = regulator_set_optimum_mode(l22_vreg, L22_LOAD_UA);
		if (rc < 0)
		{
			pr_err("%s: failed to set optimum mode for %s, rc=%d\n", __FUNCTION__, l22_vreg_name, rc);
			return rc;
		}

		rc = regulator_enable(l22_vreg);
		if (rc)
		{
			pr_err("%s: failed to enable %s, rc=%d\n", __FUNCTION__, l22_vreg_name, rc);
			return rc;
		}

		l22_vreg_on = 1;
		
		usleep(200);
	}
	else if (!on && l22_vreg_on)
	{
		/* disable the regulator after setting voltage and current */
		rc = regulator_set_voltage(l22_vreg, 0, L22_MAX_UV);
		if (rc) {
			pr_err("%s: failed to set voltage for %s, rc=%d\n", __FUNCTION__, l22_vreg_name, rc);
			return rc;
		}

		rc = regulator_set_optimum_mode(l22_vreg, 0);
		if (rc < 0) {
			pr_err("%s: failed to set optimum mode for %s, rc=%d\n", __FUNCTION__, l22_vreg_name, rc);
			return rc;
		}

		rc = regulator_disable(l22_vreg);
		if (rc) {
			pr_err("%s: failed to disable %s, rc=%d\n", __FUNCTION__, l22_vreg_name, rc);
			return rc;
		}

		l22_vreg_on = 0;
	}

	return rc;
}

#if defined(CONFIG_PM_SUPPORT_BATT_AUTH) || defined(CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C)
int l17_vreg_power_on(bool on)
{
	int rc = 0;

	SHBATT_TRACE("[S] %s on=%d \n",__FUNCTION__, on);

	if (!l17_vreg)
	{
		pr_err("%s: failed to set voltage for %s, l17_vreg is NULL\n", __FUNCTION__, l17_vreg_name);
		return -EINVAL;
	}

#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	SHBATT_TRACE("[P] %s shbatt_cur_battery_auth=%d shbatt_battery_trace_data_read_finish=%d \n",__FUNCTION__, shbatt_cur_battery_auth, shbatt_battery_trace_data_read_finish);
	if((shbatt_cur_battery_auth != 1) || (shbatt_battery_trace_data_read_finish != 1))
	{
		SHBATT_TRACE("[P] %s l17 power change not ready \n",__FUNCTION__);
		return 0;
	}
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */
	
	if (on && !l17_vreg_on)
	{
		rc = regulator_set_voltage(l17_vreg, L17_MIN_UV, L17_MAX_UV);
		if (rc)
		{
			pr_err("%s: failed to set voltage for %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
			return rc;
		}

		rc = regulator_set_optimum_mode(l17_vreg, L17_LOAD_UA);
		if (rc < 0)
		{
			pr_err("%s: failed to set optimum mode for %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
			return rc;
		}

		rc = regulator_enable(l17_vreg);
		if (rc)
		{
			pr_err("%s: failed to enable %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
			return rc;
		}

		l17_vreg_on = 1;
		
		usleep(20000);
	}
	else if (!on && l17_vreg_on)
	{
		/* disable the regulator after setting voltage and current */
		rc = regulator_set_voltage(l17_vreg, 0, L17_MAX_UV);
		if (rc) {
			pr_err("%s: failed to set voltage for %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
			return rc;
		}

		rc = regulator_set_optimum_mode(l17_vreg, 0);
		if (rc < 0) {
			pr_err("%s: failed to set optimum mode for %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
			return rc;
		}

#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
		rc = regulator_force_disable(l17_vreg);
#else
		rc = regulator_disable(l17_vreg);
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */
		if (rc) {
			pr_err("%s: failed to disable %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
			return rc;
		}

		l17_vreg_on = 0;
		
	}

	SHBATT_TRACE("[E] %s rc=%d \n",__FUNCTION__, rc);
	return rc;
}

int l17_vreg_power_force_off( void )
{
	int rc;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	SHBATT_TRACE("[P] %s shbatt_cur_battery_auth=%d shbatt_battery_trace_data_read_finish=%d \n",__FUNCTION__, shbatt_cur_battery_auth, shbatt_battery_trace_data_read_finish);
	if((shbatt_cur_battery_auth != 1) || (shbatt_battery_trace_data_read_finish != 1))
	{
		SHBATT_TRACE("[P] %s l17 power off not ready \n",__FUNCTION__);
		return 0;
	}
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */

	/* disable the regulator after setting voltage and current */
	rc = regulator_set_voltage(l17_vreg, 0, L17_MAX_UV);
	if (rc) {
		pr_err("%s: failed to set voltage for %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
		return rc;
	}

	rc = regulator_set_optimum_mode(l17_vreg, 0);
	if (rc < 0) {
		pr_err("%s: failed to set optimum mode for %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
		return rc;
	}

	rc = regulator_force_disable(l17_vreg);
	if (rc) {
		pr_err("%s: failed to disable %s, rc=%d\n", __FUNCTION__, l17_vreg_name, rc);
		return rc;
	}

	l17_vreg_on = 0;
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return rc;
}

#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
int wlchg_ic_power_on( void )
{
	int ret = 0;

	ret = l17_vreg_power_on(true);
	if(ret != 0)
	{
		SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
		return ret;
	}

	usleep(80*1000);
	return ret;
}

int wlchg_ic_power_off( void )
{
	int result = l17_vreg_power_on(false);
	usleep(200 * 1000);
	return result;
}
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */

#else /* CONFIG_PM_SUPPORT_BATT_AUTH */
int l17_vreg_power_on(bool on){ return 0; }
int l17_vreg_power_force_off( void ){ return 0; }
#endif /* CONFIG_PM_SUPPORT_BATT_AUTH */

static unsigned int shbatt_drv_poll( struct file* fi_p, poll_table* wait_p )
{
	unsigned int mask = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(atomic_read(&shbatt_usse_op_cnt) > 0)
	{
	  atomic_dec(&shbatt_usse_op_cnt);

	  mask = POLLIN;
	}
	else
	{
	  poll_wait(fi_p,&shbatt_usse_wait,wait_p);
	  complete(&shbatt_usse_cmp);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return mask;
}

static long shbatt_drv_ioctl( struct file* fi_p, unsigned int cmd, unsigned long arg )
{
	int ret = -EPERM;


	SHBATT_TRACE("[S] %s cmd=%x \n",__FUNCTION__,cmd);
	switch(cmd) {
	case SHBATT_DRV_IOCTL_CMD_GET_CHARGER_CABLE_STATUS:
		ret = shbatt_drv_ioctl_cmd_get_charger_cable_status(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_CHARGING_STATUS:
		ret = shbatt_drv_ioctl_cmd_get_battery_charging_status(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_CAPACITY:
		ret = shbatt_drv_ioctl_cmd_get_battery_capacity(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_VOLTAGE:
		ret = shbatt_drv_ioctl_cmd_get_battery_voltage(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_PRESENT:
		ret = shbatt_drv_ioctl_cmd_get_battery_present(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_HEALTH:
		ret = shbatt_drv_ioctl_cmd_get_battery_health(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_TECHNOLOGY:
		ret = shbatt_drv_ioctl_cmd_get_battery_technology(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_CURRENT:
		ret = shbatt_drv_ioctl_cmd_get_battery_current(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_ADC_CHANNEL:
		ret = shbatt_drv_ioctl_cmd_read_adc_channel(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_VBATT_CALIBRATION_DATA:
		ret = shbatt_drv_ioctl_cmd_set_vbatt_calibration_data(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_REFRESH_VBATT_CALIBRATION_DATA:
		ret = shbatt_drv_ioctl_cmd_refresh_vbatt_calibration_data(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_PMIC_CHARGER_TRANSISTOR_SWITCH:
		ret = shbatt_drv_ioctl_cmd_set_pmic_charger_transistor_switch(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_PMIC_BATTERY_TRANSISTOR_SWITCH:
		ret = shbatt_drv_ioctl_cmd_set_pmic_battery_transistor_switch(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_PMIC_VMAXSEL:
		ret = shbatt_drv_ioctl_cmd_set_pmic_vmaxsel(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_PMIC_IMAXSEL:
		ret = shbatt_drv_ioctl_cmd_set_pmic_imaxsel(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_PMIC_VTRICKLE:
		ret = shbatt_drv_ioctl_cmd_set_pmic_vtrickle(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_PMIC_ITRICKLE:
		ret = shbatt_drv_ioctl_cmd_set_pmic_itrickle(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_CHECK_STARTUP_VOLTAGE:
		ret = shbatt_drv_ioctl_cmd_check_startup_voltage(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_CHECK_STARTUP_BATTERY_PRESENT_CHECK:
		ret = shbatt_drv_ioctl_cmd_start_battery_present_check(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_battery_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_CHARGER_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_charger_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_CAMERA_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_camera_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_PA_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_pa_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_XO_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_xo_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_PMIC_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_pmic_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_SMBC_CHARGER:
		ret = shbatt_drv_ioctl_cmd_set_smbc_charger(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_MEAS_FREQ:
		ret = shbatt_drv_ioctl_cmd_set_meas_freq(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_OCV_FOR_RBATT:
		ret = shbatt_drv_ioctl_cmd_read_ocv_for_rbatt(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_VSENSE_FOR_RBATT:
		ret = shbatt_drv_ioctl_cmd_read_vsense_for_rbatt(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_VBATT_FOR_RBATT:
		ret = shbatt_drv_ioctl_cmd_read_vbatt_for_rbatt(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_CC:
		ret = shbatt_drv_ioctl_cmd_read_cc(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_LAST_GOOD_OCV:
		ret = shbatt_drv_ioctl_cmd_read_last_good_ocv(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_VSENSE_AVG:
		ret = shbatt_drv_ioctl_cmd_read_vsense_avg(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_VBATT_AVG:
		ret = shbatt_drv_ioctl_cmd_read_vbatt_avg(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_AUTO_ENABLE:
		ret = shbatt_drv_ioctl_cmd_auto_enable(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_RECALIB_ADC_DEVICE:
		ret = shbatt_drv_ioctl_cmd_recalib_adc_device(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_CALIBRATE_BATTERY_VOLTAGE:
		ret = shbatt_drv_ioctl_cmd_get_calibrate_battery_voltage(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_WIRELESS_STATUS:
		ret = shbatt_drv_ioctl_cmd_get_wireless_status(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_WIRELESS_SWITCH:
		ret = shbatt_drv_ioctl_cmd_set_wireless_switch(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_VSENSE_AVG_CALIBRATION_DATA:
		ret = shbatt_drv_ioctl_cmd_set_vsense_avg_calibration_data(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_REFRESH_VSENSE_AVG_CALIBRATION_DATA:
		ret = shbatt_drv_ioctl_cmd_refresh_vsense_avg_calibration_data(fi_p,arg);
		break;
/* TODO New API add point */
	case SHBATT_DRV_IOCTL_CMD_PULL_USSE_PACKET:
		ret = shbatt_drv_ioctl_cmd_pull_usse_packet(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_DONE_USSE_PACKET:
		ret = shbatt_drv_ioctl_cmd_done_usse_packet(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_INVALID:
		ret = shbatt_drv_ioctl_cmd_invalid(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_TIMER:
		ret = shbatt_drv_ioctl_cmd_set_timer(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_CLR_TIMER:
		ret = shbatt_drv_ioctl_cmd_clr_timer(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_VOLTAGE_ALARM:
		ret = shbatt_drv_ioctl_cmd_set_voltage_alarm(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_CLR_VOLTAGE_ALARM:
		ret = shbatt_drv_ioctl_cmd_clr_voltage_alarm(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_SAFETY:
		ret = shbatt_drv_ioctl_cmd_get_battery_safety(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_TERMINAL_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_terminal_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_MODEM_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_modem_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_CURRENT:
		ret = shbatt_drv_ioctl_cmd_get_fuelgauge_current(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_VOLTAGE:
		ret = shbatt_drv_ioctl_cmd_get_fuelgauge_voltage(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_DEVICE_ID:
		ret = shbatt_drv_ioctl_cmd_get_fuelgauge_device_id(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_FUELGAUGE_MODE:
		ret = shbatt_drv_ioctl_cmd_set_fuelgauge_mode(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_ACCUMULATE_CURRENT:
		ret = shbatt_drv_ioctl_cmd_get_fuelgauge_accumulate_current(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_CLR_FUELGAUGE_ACCUMULATE_CURRENT:
		ret = shbatt_drv_ioctl_cmd_clr_fuelgauge_accumulate_current(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_fuelgauge_temperature(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_FUELGAUGE_CURRENT_AD:
		ret = shbatt_drv_ioctl_cmd_get_fuelgauge_current_ad(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_ADC_CHANNEL_BUFFERED:
		ret = shbatt_drv_ioctl_cmd_read_adc_channel_buffered(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BATTERY_LOG_INFO:
		ret = shbatt_drv_ioctl_cmd_get_battery_log_info(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_POST_BATTERY_LOG_INFO:
		ret = shbatt_drv_ioctl_cmd_post_battery_log_info(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_NOTIFY_CHARGER_CABLE_STATUS:
		ret = shbatt_drv_ioctl_cmd_notify_charger_cable_status(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_NOTIFY_BATTERY_CHARGING_STATUS:
		ret = shbatt_drv_ioctl_cmd_notify_battery_charging_status(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_NOTIFY_BATTERY_CAPACITY:
		ret = shbatt_drv_ioctl_cmd_notify_battery_capacity(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_NOTIFY_CHARGING_STATE_MACHINE_ENABLE:
		ret = shbatt_drv_ioctl_cmd_notify_charging_state_machine_enable(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_NOTIFY_POWER_SUPPLY_CHANGED:
		ret = shbatt_drv_ioctl_cmd_notify_power_supply_changed(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_INITIALIZE:
		ret = shbatt_drv_ioctl_cmd_initialize(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_EXEC_LOW_BATTERY_CHECK_SEQUENCE:
		ret = shbatt_drv_ioctl_cmd_exec_low_battery_check_sequence(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_KERNEL_TIME:
		ret = shbatt_drv_ioctl_cmd_get_kernel_time(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_ADC_CHANNEL_NO_CONVERSION:
		ret = shbatt_drv_ioctl_cmd_read_adc_channel_no_conversion(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_HW_REVISION:
		ret = shbatt_drv_ioctl_cmd_get_hw_revision(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_SMEM_INFO:
		ret = shbatt_drv_ioctl_cmd_get_smem_info(fi_p,arg);
		break;
#if defined(CONFIG_PM_WIRELESS_CHARGE)
	case SHBATT_DRV_IOCTL_CMD_ENABLE_WIRELESS_CHARGER:
		ret = shbatt_enable_wireless_charger();
		break;
	case SHBATT_DRV_IOCTL_CMD_DISABLE_WIRELESS_CHARGER:
		ret = shbatt_disable_wireless_charger();
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_WIRELESS_DISABLE_STATE:
		ret = shbatt_drv_ioctl_cmd_get_wireless_disable_phase(fi_p, arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_WIRELESS_CHARGING_STATUS:
		ret = shbatt_drv_ioctl_cmd_get_wireless_charging_status(fi_p, arg);
		break;
#endif
	case SHBATT_DRV_IOCTL_CMD_SET_LOG_ENABLE:
		ret = shbatt_drv_ioctl_cmd_set_log_enable(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_CPU_CLOCK_LEVEL:
		ret = shbatt_drv_ioctl_cmd_get_cpu_clock_level(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_BACKLIGHT_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_backlight_temperature(fi_p,arg);
		break;
#ifdef CONFIG_SHTMP_SENSOR
	case SHBATT_DRV_IOCTL_CMD_GET_APQ_TEMPERATURE:
		ret = shbatt_drv_ioctl_cmd_get_apq_temperature(fi_p,arg);
		break;
#endif
#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	case SHBATT_DRV_IOCTL_CMD_READ_HDQ:
		ret = shbatt_drv_ioctl_cmd_read_hdq(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_WRITE_HDQ:
		ret = shbatt_drv_ioctl_cmd_write_hdq(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_CHECK_BATTERY_AUTH:
		ret = shbatt_drv_ioctl_cmd_check_battery_auth(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_BATTERY_AUTH:
		ret = shbatt_drv_ioctl_cmd_set_battery_auth(fi_p,arg);
		break;
#endif /* CONFIG_PM_SUPPORT_BATT_AUTH */
	case SHBATT_DRV_IOCTL_CMD_GET_DEPLETED_CAPACITY:
		ret = shbatt_drv_ioctl_cmd_get_depleted_capacity(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_DEPLETED_BATTERY_FLG:
		ret = shbatt_drv_ioctl_cmd_set_depleted_battery_flg(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_DEPLETED_BATTERY_FLG:
		ret = shbatt_drv_ioctl_cmd_get_depleted_battery_flg(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_BATTERY_HEALTH:
		ret = shbatt_drv_ioctl_cmd_set_battery_health(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_ANDROID_SHUTDOWN_TIMER:
		ret = shbatt_drv_ioctl_cmd_set_android_shutdown_timer(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_RESET_CC:
		ret = shbatt_drv_ioctl_cmd_reset_cc(fi_p, arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_SET_DEPLETED_CALC_ENABLE:
		ret = shbatt_drv_ioctl_cmd_set_depleted_calc_enable(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_AVE_CURRENT_AND_AVE_VOLTAGE:
		ret = shbatt_drv_ioctl_cmd_get_ave_current_and_ave_voltage(fi_p, arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_READ_ADC_CHANNELS:
		ret = shbatt_drv_ioctl_cmd_read_adc_channels(fi_p, arg);
		break;
#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_UPDATE:
		{
			ret = wlchg_firmware_update();
		}
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_FVER:
		{
			uint8_t fver = 0;
			int retry = 10;

			ret = wlchg_ic_power_on();
			if(ret != 0)
			{
				SHBATT_TRACE("[E] %s ret=%d \n",__FUNCTION__,ret);
				break;
			}

			for(; retry>0; retry--)
			{
				ret = wlchg_firmware_get_fver(&fver);
				if(ret == 0)
				{
					int result = fver;
					ret = copy_to_user((void*)arg, &result, sizeof(result));
					break;
				}
				else
				{
					SHBATT_TRACE("[E] %s ret=%d retry=%d\n",__FUNCTION__,ret,retry);
					usleep(20000);
				}
			}

			wlchg_ic_power_off();
		}
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_HWVER:
		{
			uint8_t hwver = 0;
			int retry = 10;

			ret = wlchg_ic_power_on();
			if(ret != 0)
			{
				SHBATT_TRACE("[E] %s ret=%d \n",__FUNCTION__,ret);
				break;
			}

			for(; retry>0; retry--)
			{
				ret = wlchg_firmware_get_hwver(&hwver);
				if(ret == 0)
				{
					int result = hwver;
					ret = copy_to_user((void*)arg, &result, sizeof(result));
				}
				else
				{
					SHBATT_TRACE("[E] %s ret=%d retry=%d\n",__FUNCTION__,ret,retry);
					usleep(20000);
				}
			}

			wlchg_ic_power_off();
		}
		break;
/*calibration commands.*/
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_GO_TO_CALIBRATION_MODE:
		ret = shbatt_wlchg_go_to_calibration_mode(); 
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_VOLTAGE_CALIBRATION:
		{
			shbatt_wlchg_voltage_calibration_t cal;
			ret = copy_from_user(&cal, (void*)arg, sizeof(cal));
			if(ret == 0)
			{
				ret = shbatt_wlchg_voltage_calibration(&cal);
				if(ret == 0)
				{
					ret = copy_to_user((void*)arg, &cal, sizeof(cal));
				}
			}
		}
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_VOLTAGE_AND_CURRENT_CALIBRATION:
		{
			shbatt_wlchg_voltage_and_current_calibration_t cal;
			ret = copy_from_user(&cal, (void*)arg, sizeof(cal));
			if(ret == 0)
			{
				ret = shbatt_wlchg_voltage_and_current_calibration(&cal);
				if(ret == 0)
				{
					ret = copy_to_user((void*)arg, &cal, sizeof(cal));
				}
			}
		}
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_CURRENT_CALIBRATION:
		{
			shbatt_wlchg_current_calibration_t cal;
			ret = copy_from_user(&cal, (void*)arg, sizeof(cal));
			if(ret == 0)
			{
				ret = shbatt_wlchg_current_calibration(&cal);
				if(ret == 0)
				{
					ret = copy_to_user((void*)arg, &cal, sizeof(cal));
				}
			}
		}
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_WRITE_WPC_SERIAL_NO:
		{
			uint16_t serial_no;
			ret = copy_from_user(&serial_no, (void*)arg, sizeof(serial_no));
			if(ret == 0)
			{
				ret = shbatt_wlchg_write_wpc_serial_no(serial_no);
			}
		}
		break;

	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGE_GET_CALIBRATED_VALUE:
		{
			shbatt_wlchg_calibrated_value_t calibrated_value;
			ret = shbatt_wlchg_get_calibrated_value(&calibrated_value);
			if(ret == 0)
			{
				ret = copy_to_user((void*)arg, &calibrated_value, sizeof(calibrated_value));
			}
		}
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_SET_BATT_TEMP:
		ret = shbatt_drv_ioctl_cmd_wireless_charger_firmware_set_batt_temp(fi_p, arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_BATT_TEMP:
		ret = shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_batt_temp(fi_p, arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_SET_CPU_TEMP:
		ret = shbatt_drv_ioctl_cmd_wireless_charger_firmware_set_cpu_temp(fi_p, arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_CPU_TEMP:
		ret = shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_cpu_temp(fi_p, arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_GET_RECTIFIER_IC_TEMP:
		ret = shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_rectifier_ic_temp(fi_p, arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_WIRELESS_CHARGER_FIRMWARE_UPDATE_FROM_BUFFER:
		{
			shbatt_wlchg_firmware_buffer_t firmware;
			ret = copy_from_user(&firmware, (void*)arg, sizeof(firmware));
			if(ret == 0)
			{
				size_t size = firmware.size;
				
				u8* buffer = kzalloc(size,GFP_KERNEL);
				if(buffer != NULL)
				{
					ret = copy_from_user(buffer, firmware.data, size);
					if(ret == 0)
					{
						ret = wlchg_firmware_update_from_buffer(buffer, size);
					}
					kfree(buffer);
				}
				else
				{
					ret = -ENOMEM;
				}
			}
		}
		break;
	case SHBATT_DRV_IOCTL_CMD_WLCHG_READ_I2C_DATA:
		{
			shbatt_wlchg_i2c_data_t i2c_data;
			ret = copy_from_user(&i2c_data, (void*)arg, sizeof(i2c_data));
			if(ret == 0)
			{
				uint8_t *read_buffer = (uint8_t*)kzalloc(i2c_data.size,GFP_KERNEL);
				if(read_buffer != NULL)
				{
					ret = wlchg_i2c_read(i2c_data.address, read_buffer, i2c_data.size);
					if(ret == 0)
					{
						ret = copy_to_user(i2c_data.data, read_buffer, i2c_data.size);
					}
					kfree(read_buffer);
				}
				else
				{
					ret = -ENOMEM;
				}
			}
		}
		break;
	case SHBATT_DRV_IOCTL_CMD_WLCHG_WRITE_I2C_DATA:
		{
			shbatt_wlchg_i2c_data_t i2c_data;
			ret = copy_from_user(&i2c_data, (void*)arg, sizeof(i2c_data));
			if(ret == 0)
			{
				uint8_t* write_buffer = (uint8_t*)kzalloc(i2c_data.size,GFP_KERNEL);
				if(write_buffer != NULL)
				{
					ret = copy_from_user(write_buffer, i2c_data.data, i2c_data.size);
					if(ret == 0)
					{
						ret =wlchg_i2c_write(i2c_data.address, write_buffer, i2c_data.size);
					}
					kfree(write_buffer);
				}
				else
				{
					ret = -ENOMEM;
				}
			}
		}
		break;
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */
	case SHBATT_DRV_IOCTL_CMD_GET_AVERAGE_CURRENT:
		ret = shbatt_drv_ioctl_cmd_get_average_current(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_CALIB_CCADC:
		ret = shbatt_drv_ioctl_cmd_calib_ccadc(fi_p,arg);
		break;
	case SHBATT_DRV_IOCTL_CMD_GET_SYSTEM_TIME:
		ret = shbatt_drv_ioctl_cmd_get_system_time(fi_p,arg);
		break;
	default:
		SHBATT_ERROR("%s : bad cmd. 0x%x\n",__FUNCTION__, cmd);
		ret = shbatt_drv_ioctl_cmd_invalid(fi_p,arg);
		break;
	}
	
	SHBATT_TRACE("[E] %s ret=%d \n",__FUNCTION__,ret);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_charger_cable_status( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_cable_status_t cbs;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_charger_cable_status( &cbs ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_charger_cable_status failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_cable_status_t __user *)arg, &cbs, sizeof(shbatt_cable_status_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_charging_status( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_chg_status_t cgs;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_battery_charging_status( &cgs ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_battery_charging_status failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_chg_status_t __user *)arg, &cgs, sizeof(shbatt_chg_status_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_capacity( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int cap;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_battery_capacity( &cap ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_battery_capacity failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &cap, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_voltage( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int vol;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_battery_voltage( &vol ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_battery_voltage failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &vol, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_present( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_present_t pre;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_battery_present( &pre ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_battery_present failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_present_t __user *)arg, &pre, sizeof(shbatt_present_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_health( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_health_t hea;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_battery_health( &hea ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_battery_health failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
		}

	if(copy_to_user((shbatt_health_t __user *)arg, &hea, sizeof(shbatt_health_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_technology( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_technology_t tec;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_battery_technology( &tec ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_battery_technology failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_technology_t __user *)arg, &tec, sizeof(shbatt_technology_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_current( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int cur;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_battery_current( &cur ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_battery_current failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &cur, sizeof(int)) != 0)
	{
	  SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
	  ret = -EPERM;
	  goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_adc_channel( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_adc_t adc;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&adc,(shbatt_adc_t __user *)arg, sizeof(shbatt_adc_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_read_adc_channel( &adc ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_read_adc_channel failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_adc_t __user *)arg, &adc, sizeof(shbatt_adc_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_vbatt_calibration_data( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_vbatt_cal_t cal;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&cal,(shbatt_vbatt_cal_t __user *)arg, sizeof(shbatt_vbatt_cal_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_vbatt_calibration_data( cal ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_vbatt_calibration_data failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_refresh_vbatt_calibration_data( struct file* fi_p, unsigned long arg )
{
	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_refresh_vbatt_calibration_data( ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_refresh_vbatt_calibration_data failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_pmic_charger_transistor_switch( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_pmic_charger_transistor_switch( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_pmic_charger_transistor_switch failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_pmic_battery_transistor_switch( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_pmic_battery_transistor_switch( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_pmic_battery_transistor_switch failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_pmic_vmaxsel( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_pmic_vmaxsel( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_pmic_vmaxsel failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_pmic_imaxsel( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_pmic_imaxsel( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_pmic_imaxsel failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_pmic_vtrickle( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_pmic_vtrickle( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_pmic_vtrickle failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_pmic_itrickle( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_pmic_itrickle( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_pmic_itrickle failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_check_startup_voltage( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int chk;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_check_startup_voltage( &chk ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_check_startup_voltage failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &chk, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_start_battery_present_check( struct file* fi_p, unsigned long arg )
{
	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_start_battery_present_check( ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_start_battery_present_check failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_temperature( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int tmp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_battery_temperature( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_battery_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_charger_temperature( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int tmp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_charger_temperature( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_charger_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_camera_temperature( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int tmp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_camera_temperature( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_camera_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_backlight_temperature( struct file* fi_p, unsigned long arg )
{
#ifndef CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM
	SHBATT_ERROR("%s : no support function.\n",__FUNCTION__);
	return -EPERM;
#else  /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
	int ret = 0;
	int tmp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_backlight_temperature( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_backlight_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
#endif /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
}

static int shbatt_drv_ioctl_cmd_get_pa_temperature( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int tmp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_pa_temperature( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_pa_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_xo_temperature( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int tmp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_xo_temperature( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_xo_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_pmic_temperature( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int tmp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_pmic_temperature( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_pmic_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_smbc_charger( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_smbc_chg_t chg;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&chg,(shbatt_smbc_chg_t __user *)arg, sizeof(shbatt_smbc_chg_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_smbc_charger( chg ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_smbc_charger failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_meas_freq( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_meas_freq( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_meas_freq failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_ocv_for_rbatt( struct file* fi_p, unsigned long arg )
{
	int  ret = 0;
	shbatt_adc_conv_uint_t ocv;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_read_ocv_for_rbatt( &ocv ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_read_ocv_for_rbatt failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_adc_conv_uint_t __user *)arg, &ocv, sizeof(shbatt_adc_conv_uint_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_vsense_for_rbatt( struct file* fi_p, unsigned long arg )
{
	int  ret = 0;
	shbatt_adc_conv_uint_t vsense;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_read_vsense_for_rbatt( &vsense ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_read_vsense_for_rbatt failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_adc_conv_uint_t __user *)arg, &vsense, sizeof(shbatt_adc_conv_uint_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_vbatt_for_rbatt( struct file* fi_p, unsigned long arg )
{
	int  ret = 0;
	shbatt_adc_conv_uint_t vbatt;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_read_vbatt_for_rbatt( &vbatt ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_read_vbatt_for_rbatt failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_adc_conv_uint_t __user *)arg, &vbatt, sizeof(shbatt_adc_conv_uint_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_cc( struct file* fi_p, unsigned long arg )
{
	int  ret = 0;
	shbatt_adc_conv_offset_t cc_mah;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&cc_mah,(shbatt_adc_conv_offset_t __user *)arg, sizeof(shbatt_adc_conv_offset_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_read_cc_offset( &cc_mah.adc_convl, cc_mah.offset ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_read_cc failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_adc_conv_offset_t __user *)arg, &cc_mah, sizeof(shbatt_adc_conv_offset_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_last_good_ocv( struct file* fi_p, unsigned long arg )
{
	int  ret = 0;
	shbatt_adc_conv_uint_t ocv;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_read_last_good_ocv( &ocv ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_read_last_good_ocv failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_adc_conv_uint_t __user *)arg, &ocv, sizeof(shbatt_adc_conv_uint_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_vsense_avg( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_adc_conv_int_t vsense_avg;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_read_vsense_avg( &vsense_avg ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_read_vsense_avg failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_adc_conv_int_t __user *)arg, &vsense_avg, sizeof(shbatt_adc_conv_int_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_vbatt_avg( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_adc_conv_int_t vbatt_avg;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_read_vbatt_avg( &vbatt_avg ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_read_vbatt_avg failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_adc_conv_int_t __user *)arg, &vbatt_avg, sizeof(shbatt_adc_conv_int_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_auto_enable( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_auto_enable( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_auto_enable failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_recalib_adc_device( struct file* fi_p, unsigned long arg )
{
	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_recalib_adc_device( ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_recalib_adc_device failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_calibrate_battery_voltage( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int vol;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_calibrate_battery_voltage( &vol ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_battery_voltage failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &vol, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_wireless_status( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int tmp = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_wireless_status( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_pmic_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_wireless_switch( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_wireless_switch( val ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_meas_freq failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_vsense_avg_calibration_data( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	shbatt_vsense_avg_cal_t vac;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&vac,(shbatt_vsense_avg_cal_t __user *)arg, sizeof(shbatt_vsense_avg_cal_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_vsense_avg_calibration_data( vac ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_set_vsense_avg_calibration_data failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_refresh_vsense_avg_calibration_data( struct file* fi_p, unsigned long arg )
{
	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_refresh_vsense_avg_calibration_data( ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_refresh_vsense_avg_calibration_data failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_log_enable( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(shbatt_api_set_log_enable( val) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_set_log_enable failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
	if(shchg_api_set_log_enable( (val>>1)&0x01 ) != SHCHG_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shchg_api_set_log_enable failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

#ifdef CONFIG_SHTMP_SENSOR
static int shbatt_drv_ioctl_cmd_get_apq_temperature( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int tmp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_apq_temperature( &tmp ) != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("%s : shbatt_api_get_apq_temperature failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &tmp, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return ret;
}
#endif /* CONFIG_SHTMP_SENSOR */

/* TODO New API add point */

static void shbatt_task( struct work_struct* work_p )
{
	shbatt_packet_t* pkt_p;

	mutex_lock(&shbatt_task_lock);

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	pkt_p = (shbatt_packet_t*)work_p;

	SHBATT_TRACE("shbatt_task %d \n",pkt_p->hdr.cmd);

	if(pkt_p->hdr.cmd < NUM_SHBATT_TASK_CMD)
	{
		SHBATT_TRACE("shbatt_task OK \n");
		shbatt_task_cmd_func[pkt_p->hdr.cmd](pkt_p);
	}
	else
	{
		SHBATT_TRACE("shbatt_task NG \n");
		shbatt_task_cmd_invalid(pkt_p);
	}

	SHBATT_WAKE_CTL(0);

	shbatt_task_free_packet(pkt_p);

	mutex_unlock(&shbatt_task_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static void shbatt_task_cmd_invalid( shbatt_packet_t* pkt_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_charger_cable_status( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_cable_status_t cbs = SHBATT_CABLE_STATUS_UNKNOWN;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_charger_cable_status( &cbs );

	if(pkt_p->prm.cbs_p != NULL)
	{
	  *(pkt_p->prm.cbs_p) = cbs;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_charging_status( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_chg_status_t cgs = SHBATT_CHG_STATUS_UNKNOWN;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_charging_status( &cgs );

	if(pkt_p->prm.cgs_p != NULL)
	{
		*(pkt_p->prm.cgs_p) = cgs;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_present( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_present_t present = SHBATT_PRESENT_BATT_NONE;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_present( &present );

	if(pkt_p->prm.pre_p != NULL)
	{
		*(pkt_p->prm.pre_p) = present;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_log_enable( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_log_enable( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_capacity( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int cap = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_capacity( &cap );

	if(pkt_p->prm.cap_p != NULL)
	{
		*(pkt_p->prm.cap_p) = cap;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_voltage( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int vol = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_voltage( &vol );

	if(pkt_p->prm.vol_p != NULL)
	{
		*(pkt_p->prm.vol_p) = vol;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_health( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_health_t hea = SHBATT_HEALTH_BATT_UNKNOWN;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_health( &hea );

	if(pkt_p->prm.hea_p != NULL)
	{
		*(pkt_p->prm.hea_p) = hea;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_technology( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_technology_t tec = SHBATT_TECHNOLOGY_BATT_UNKNOWN;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_technology( &tec );

	if(pkt_p->prm.tec_p != NULL)
	{
		*(pkt_p->prm.tec_p) = tec;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_current( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int cur = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_current( &cur );

	if(pkt_p->prm.cur_p != NULL)
	{
		*(pkt_p->prm.cur_p) = cur;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_adc_channel( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_adc_t adc;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(!pkt_p)
	{
		SHBATT_ERROR("error pkt_p is NULL = %x\n", (unsigned int)pkt_p);
		goto error;
	}

	if(!pkt_p->prm.adc_p)
	{
		SHBATT_ERROR("error pkt_p->prm.adc_p is NULL = %x\n", (unsigned int)pkt_p->prm.adc_p);
		goto error;
	}

	adc.channel		= pkt_p->prm.adc_p->channel;
	adc.adc_code	= 0;
	adc.measurement	= 0;
	adc.physical	= 0;

	result = shbatt_seq_read_adc_channel( &adc );

	pkt_p->prm.adc_p->adc_code		= adc.adc_code;
	pkt_p->prm.adc_p->measurement	= adc.measurement;
	pkt_p->prm.adc_p->physical		= adc.physical;

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

error:

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_vbatt_calibration_data( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_vbatt_calibration_data( pkt_p->prm.cal );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_refresh_vbatt_calibration_data( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_refresh_vbatt_calibration_data( );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_pmic_charger_transistor_switch( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_pmic_charger_transistor_switch( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_pmic_battery_transistor_switch( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_pmic_battery_transistor_switch( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
	  *(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
	  complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_pmic_vmaxsel( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_pmic_vmaxsel( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_pmic_imaxsel( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_pmic_imaxsel( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_pmic_vtrickle( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_pmic_vtrickle( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_pmic_itrickle( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_pmic_itrickle( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_check_startup_voltage( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int chk = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_check_startup_voltage( &chk );

	if(pkt_p->prm.chk_p != NULL)
	{
		*(pkt_p->prm.chk_p) = chk;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_start_battery_present_check( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_start_battery_present_check( );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int tmp = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_temperature( &tmp );

	if(pkt_p->prm.tmp_p != NULL)
	{
		*(pkt_p->prm.tmp_p) = tmp;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_charger_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int tmp = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_charger_temperature( &tmp );

	if(pkt_p->prm.tmp_p != NULL)
	{
		*(pkt_p->prm.tmp_p) = tmp;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_camera_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int tmp = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_camera_temperature( &tmp );

	if(pkt_p->prm.tmp_p != NULL)
	{
		*(pkt_p->prm.tmp_p) = tmp;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
	  *(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
	  complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_backlight_temperature( shbatt_packet_t* pkt_p )
{
#ifndef CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM
	SHBATT_ERROR("%s : no support function.\n",__FUNCTION__);
#else  /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
	shbatt_result_t result;
	int tmp = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_backlight_temperature( &tmp );

	if(pkt_p->prm.tmp_p != NULL)
	{
		*(pkt_p->prm.tmp_p) = tmp;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
	  *(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
	  complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
#endif /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
}

static void shbatt_task_cmd_get_pa_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int tmp = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_pa_temperature( &tmp );

	if(pkt_p->prm.tmp_p != NULL)
	{
		*(pkt_p->prm.tmp_p) = tmp;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_xo_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int tmp = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_xo_temperature( &tmp );

	if(pkt_p->prm.tmp_p != NULL)
	{
		*(pkt_p->prm.tmp_p) = tmp;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_pmic_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int tmp = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_pmic_temperature( &tmp );

	if(pkt_p->prm.tmp_p != NULL)
	{
		*(pkt_p->prm.tmp_p) = tmp;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_smbc_charger( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_smbc_charger( pkt_p->prm.chg );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_meas_freq( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_meas_freq( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_ocv_for_rbatt( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_adc_conv_uint_t ocv = {0,};

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_read_ocv_for_rbatt( &ocv );

	if(pkt_p->prm.ocv_p != NULL)
	{
		*(pkt_p->prm.ocv_p) = ocv;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_vsense_for_rbatt( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_adc_conv_uint_t vsense = {0,};

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_read_vsense_for_rbatt( &vsense );

	if(pkt_p->prm.vsense_p != NULL)
	{
		*(pkt_p->prm.vsense_p) = vsense;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_vbatt_for_rbatt( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_adc_conv_uint_t vbatt = {0,};

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_read_vbatt_for_rbatt( &vbatt );

	if(pkt_p->prm.vbatt_p != NULL)
	{
		*(pkt_p->prm.vbatt_p) = vbatt;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_cc( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_adc_conv_offset_t cc_mah = {0,};

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(pkt_p->prm.cc_mah_p != NULL)
	{
		cc_mah = *(pkt_p->prm.cc_mah_p);
	}

	result = shbatt_seq_read_cc( &cc_mah );

	if(pkt_p->prm.cc_mah_p != NULL)
	{
		*(pkt_p->prm.cc_mah_p) = cc_mah;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
	  *(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
	  complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_last_good_ocv( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_adc_conv_uint_t ocv = {0,};

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_read_last_good_ocv( &ocv );

	if(pkt_p->prm.ocv_p != NULL)
	{
		*(pkt_p->prm.ocv_p) = ocv;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
	  *(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
	  complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_vsense_avg( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_adc_conv_int_t vsense_avg = {0,};

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_read_vsense_avg( &vsense_avg );

	if(pkt_p->prm.vsense_avg_p != NULL)
	{
		*(pkt_p->prm.vsense_avg_p) = vsense_avg;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
	  *(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
	  complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_vbatt_avg( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_adc_conv_int_t vbatt_avg = {0,};

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_read_vbatt_avg( &vbatt_avg );

	if(pkt_p->prm.vbatt_avg_p != NULL)
	{
		*(pkt_p->prm.vbatt_avg_p) = vbatt_avg;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
	  *(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
	  complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_auto_enable( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_auto_enable( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_recalib_adc_device( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_recalib_adc_device();

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_calibrate_battery_voltage( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int vol = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_calibrate_battery_voltage( &vol );

	if(pkt_p->prm.vol_p != NULL)
	{
		*(pkt_p->prm.vol_p) = vol;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_wireless_status( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int vol = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_wireless_status( &vol );
	
	if(pkt_p->prm.vol_p != NULL)
	{
		*(pkt_p->prm.vol_p) = vol;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return;
}

static void shbatt_task_cmd_set_wireless_switch( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_wireless_switch( pkt_p->prm.val );
	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return;
}

static void shbatt_task_cmd_set_vsense_avg_calibration_data( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_vsense_avg_calibration_data( pkt_p->prm.vac );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_refresh_vsense_avg_calibration_data( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_refresh_vsense_avg_calibration_data( );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

/* TODO New API add point */


static shbatt_result_t shbatt_seq_get_charger_cable_status( shbatt_cable_status_t* cbs_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_CHARGER_CABLE_STATUS;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*cbs_p = shbatt_usse_pkt.prm.cbs;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_battery_charging_status( shbatt_chg_status_t* cgs_p )
{
	  shbatt_result_t result;

	  SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	  shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_BATTERY_CHARGING_STATUS;
	  shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	  shbatt_usse_pkt.prm.val = 0;

	  result = shbatt_seq_call_user_space_sequence_executor();

	  if(result == SHBATT_RESULT_SUCCESS)
	  {
	    *cgs_p = shbatt_usse_pkt.prm.cgs;
	  }

	  SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	  return result;
}

static shbatt_result_t shbatt_seq_get_battery_capacity( int* cap_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_BATTERY_CAPACITY;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*cap_p = shbatt_usse_pkt.prm.cap;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_battery_voltage( int* vol_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	shbatt_adc_t voltage;
	do
	{
		SHBATT_TRACE("[S] %s \n",__FUNCTION__);

		voltage.channel = SHBATT_ADC_CHANNEL_VBAT;
		
		result = shbatt_api_read_adc_channel_no_conversion(&voltage);
		if(result == SHBATT_RESULT_SUCCESS)
		{
			*vol_p = voltage.physical;
			break;
		}

	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_battery_present( shbatt_present_t* pre_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_BATTERY_PRESENT;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*pre_p = shbatt_usse_pkt.prm.pre;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_log_enable( int val )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_SET_LOG_ENABLE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = val;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_battery_health( shbatt_health_t* hea_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_BATTERY_HEALTH;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*hea_p = shbatt_usse_pkt.prm.hea;
	}

	SHBATT_TRACE("[E] %s hea_p=%d \n",__FUNCTION__, *hea_p);

	return result;
}

static shbatt_result_t shbatt_seq_get_battery_technology( shbatt_technology_t* tec_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_BATTERY_TECHNOLOGY;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.tec = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*tec_p = shbatt_usse_pkt.prm.tec;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_battery_current( int* cur_p )
{
	shbatt_result_t result = SHBATT_RESULT_FAIL;
	int ibat;
	int rc;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc =pm8921_bms_get_battery_current(&ibat);
	if(rc == 0)
	{
		*cur_p = ibat;
		result = SHBATT_RESULT_SUCCESS;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_read_adc_channel( shbatt_adc_t* adc_p )
{
	static	const shbatt_drv_adc_channel_t kernel_side_channel_ids[NUM_SHBATT_ADC_CHANNEL]=
		{										/*User side channel ids.*/
		SHBATT_DRV_ADC_CHANNEL_VCOIN,			/*SHBATT_ADC_CHANNEL_VCOIN*/
		SHBATT_DRV_ADC_CHANNEL_VBAT,			/*SHBATT_ADC_CHANNEL_VBAT*/
		SHBATT_DRV_ADC_CHANNEL_DCIN,			/*SHBATT_ADC_CHANNEL_DCIN*/
		SHBATT_DRV_ADC_CHANNEL_ICHG,			/*SHBATT_ADC_CHANNEL_ICHG*/
		SHBATT_DRV_ADC_CHANNEL_VPH_PWR,			/*SHBATT_ADC_CHANNEL_VPH_PWR*/
		SHBATT_DRV_ADC_CHANNEL_IBAT,			/*SHBATT_ADC_CHANNEL_IBAT*/
		SHBATT_DRV_ADC_CHANNEL_CAM_TEMP,		/*SHBATT_ADC_CHANNEL_CAM_TEMP*/
		SHBATT_DRV_ADC_CHANNEL_BAT_THERM,		/*SHBATT_ADC_CHANNEL_BAT_THERM*/
		SHBATT_DRV_ADC_CHANNEL_BAT_ID,			/*SHBATT_ADC_CHANNEL_BAT_ID*/
		SHBATT_DRV_ADC_CHANNEL_USBIN,			/*SHBATT_ADC_CHANNEL_USBIN*/
		SHBATT_DRV_ADC_CHANNEL_PMIC_TEMP,		/*SHBATT_ADC_CHANNEL_PMIC_TEMP*/
		SHBATT_DRV_ADC_CHANNEL_625MV,			/*SHBATT_ADC_CHANNEL_625MV*/
		SHBATT_DRV_ADC_CHANNEL_125V,			/*SHBATT_ADC_CHANNEL_125V*/
		SHBATT_DRV_ADC_CHANNEL_CHG_TEMP,		/*SHBATT_ADC_CHANNEL_CHG_TEMP*/
		SHBATT_DRV_ADC_CHANNEL_XO_THERM,		/*SHBATT_ADC_CHANNEL_XO_THERM*/
		SHBATT_DRV_ADC_CHANNEL_PA_THERM,		/*SHBATT_ADC_CHANNEL_PA_THERM*/
		SHBATT_DRV_ADC_CHANNEL_USB_ID,			/*SHBATT_ADC_CHANNEL_USB_ID*/
		SHBATT_DRV_ADC_CHANNEL_BAT_THERM_NO_MOVING_AVERAGE,		/* SHBATT_ADC_CHANNEL_BAT_THERM_NO_MOVING_AVERAGE */
		SHBATT_DRV_ADC_CHANNEL_BACKLIGHT_TEMP,	/*SHBATT_ADC_CHANNEL_BACKLIGHT_TEMP*/
		};
	
	shbatt_drv_adc_channel_t kernel_side_channel_id = kernel_side_channel_ids[adc_p->channel];	/*user id->kernel */
	int rc, ret;
	int ibat_result,ibat_adc_result;
	struct pm8xxx_adc_chan_result adc_result = {0,};
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[adc channel] %d \n",adc_p->channel);
	


	if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_CAM_TEMP)
	{
		rc = pm8xxx_adc_mpp_config_read(3, kernel_side_channel_id, &adc_result);
	}
	else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BACKLIGHT_TEMP)
	{
#ifdef CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM
		rc = pm8xxx_adc_mpp_config_read(12, kernel_side_channel_id, &adc_result);
#else  /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
		SHBATT_ERROR("%s : no support channel id. SHBATT_DRV_ADC_CHANNEL_BACKLIGHT_TEMP\n",__FUNCTION__);
		rc = -EPERM;
#endif /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
	}
	else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_ICHG)
	{
		l22_vreg_power_on(true);
		rc = pm8xxx_adc_mpp_config_read(9, kernel_side_channel_id, &adc_result);
		l22_vreg_power_on(false);
	}
	else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_USB_ID)
	{
		rc = pm8xxx_adc_mpp_config_read(11, kernel_side_channel_id, &adc_result);
	}
	else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BAT_THERM_NO_MOVING_AVERAGE)
	{
		rc = pm8xxx_adc_read(SHBATT_DRV_ADC_CHANNEL_BAT_THERM, &adc_result);
	}
#ifdef CONFIG_SHTMP_SENSOR
	else if((kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_CHG_TEMP) || (kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_XO_THERM) || (kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PA_THERM))
	{
		/* CHG_TEMP,XO_THERM,PA_THERM can't read */
		rc = 0;
		adc_p->adc_code		= 0;
		adc_p->measurement	= 0;
	}
#endif /*CONFIG_SHTMP_SENSOR*/
	else
	{
		if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PMIC_TEMP)
		{
			ret = msm_xo_mode_vote(xo_handle, MSM_XO_MODE_ON);
			if(ret)
			{
				printk(KERN_ERR "%s msm_xo_mode_vote ON err=%d", __FUNCTION__, ret);
			}
			usleep(WAIT_TIME_AFTER_XO_VOTE_ON);
		}
		rc = pm8xxx_adc_read(kernel_side_channel_id, &adc_result);
		if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PMIC_TEMP)
		{
			ret = msm_xo_mode_vote(xo_handle, MSM_XO_MODE_OFF);
			if(ret)
			{
				printk(KERN_ERR "%s msm_xo_mode_vote OFF err=%d", __FUNCTION__, ret);
			}
		}
	}

	if (kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_IBAT) {
//		rc = pm8921_bms_get_battery_current(&ibat_result);
		rc = pm8921_bms_get_vsense_avg_read(&ibat_adc_result, &ibat_result);

	}

	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error reading adc channel = %d, rc = %d\n", adc_p->channel, rc);
		goto error;
	}
	adc_p->adc_code		= adc_result.adc_code;
	adc_p->measurement	= adc_result.measurement;

	if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_CAM_TEMP || kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BAT_THERM ||
		kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_XO_THERM || kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PA_THERM ||
		kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BACKLIGHT_TEMP || kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BAT_THERM_NO_MOVING_AVERAGE
	)
	{
		adc_p->physical		= (int64_t)adc_result.microvolts / 1000;
	}
	else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PMIC_TEMP)
	{
		adc_p->physical		= adc_result.physical / 1024;
	}
	else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_ICHG)
	{
		adc_p->physical = (int32_t)adc_result.physical / 500;
	}
#ifdef CONFIG_SHTMP_SENSOR
	else if((kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_CHG_TEMP) || (kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_XO_THERM) || (kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PA_THERM))
	{
		/* CHG_TEMP,XO_THERM,PA_THERM can't read */
		adc_p->physical		= 0;
	}
#endif /*CONFIG_SHTMP_SENSOR*/
	else
	{
		adc_p->physical = (int32_t)adc_result.physical / 1000;
	}

	if (kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_IBAT) {
		adc_p->physical = (int32_t)ibat_result;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_vbatt_calibration_data( shbatt_vbatt_cal_t cal )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[calibration min]  %d \n", cal.min);
	SHBATT_TRACE("[calibration max]  %d \n", cal.max);
	SHBATT_TRACE("[calibration vmin] %d \n", cal.vmin);
	SHBATT_TRACE("[calibration vmax] %d \n", cal.vmax);

	pm8xxx_adc_set_vbatt_calibration_data(cal.min, cal.max, cal.vmin, cal.vmax);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_refresh_vbatt_calibration_data( void )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	pm8xxx_adc_refresh_vbatt_calibration_data();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_pmic_charger_transistor_switch( int val )
{
	int rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[switch] %d \n", !val);
/*	rc = pm8921_charger_charge_dis( !val );	*/ /*control usb suspend.*/
	 rc = pm8921_disable_source_current(!val);	/*control charge transistor.*/
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_charge_dis = %d, rc = %d\n", !val, rc);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_pmic_battery_transistor_switch( int val )
{
	int rc;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[switch] %d \n", val);

	rc = pm8921_charger_batt_trans_switch(val);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_batt_trans_switch = %d, rc = %d\n", val, rc);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_pmic_vmaxsel( int val )
{
	int rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[vmax] %dmV \n", val);
	rc = pm8921_charger_vddmax_set( val );
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_vddmax_set = %d, rc = %d\n", val, rc);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_pmic_imaxsel( int val )
{
	int rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[imax] %dmA \n", val);
	rc = pm8921_charger_ibatmax_set( val );
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_ibatmax_set = %d, rc = %d\n", val, rc);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_pmic_vtrickle( int val )
{
	int rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[vtrickle] %dmV \n", val);
	rc = pm8921_charger_vtrkl_low_set( val );
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm_chg_vtrkl_low_set = %d, rc = %d\n", val, rc);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_pmic_itrickle( int val )
{
	int rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[itrickle] %dmA \n", val);
	rc = pm8921_charger_itrkl_set( val );
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_itrkl_set = %d, rc = %d\n", val, rc);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_check_startup_voltage( int* chk_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	*chk_p = 1;
	/* TBD */

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_start_battery_present_check( void )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	/* TBD */

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_battery_temperature( int* tmp_p )
{
	int rc;
	struct pm8xxx_adc_chan_result adc_result;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8xxx_adc_read(CHANNEL_BATT_THERM, &adc_result);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error reading adc channel = %d, rc = %d\n", CHANNEL_BATT_THERM, rc);
		goto error;
	}
	*tmp_p	= adc_result.physical;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_charger_temperature( int* tmp_p )
{
	int rc;
	struct pm8xxx_adc_chan_result adc_result;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8xxx_adc_read(CHANNEL_CHG_TEMP, &adc_result);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error reading adc channel = %d, rc = %d\n", CHANNEL_CHG_TEMP, rc);
		goto error;
	}
	*tmp_p	= adc_result.physical;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_camera_temperature( int* tmp_p )
{
	int rc;
	struct pm8xxx_adc_chan_result adc_result;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8xxx_adc_mpp_config_read(3, ADC_MPP_1_AMUX6, &adc_result);

	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error reading adc channel = %d, rc = %d\n", ADC_MPP_1_AMUX6, rc);
		goto error;
	}
	*tmp_p	= adc_result.physical;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

#ifdef CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM
static shbatt_result_t shbatt_seq_get_backlight_temperature( int* tmp_p )
{
	int rc;
	struct pm8xxx_adc_chan_result adc_result;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8xxx_adc_mpp_config_read(12, ADC_MPP_1_BACKLIGHT_AMUX6, &adc_result);

	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error reading adc channel = %d, rc = %d\n", ADC_MPP_1_BACKLIGHT_AMUX6, rc);
		goto error;
	}
	*tmp_p	= adc_result.physical;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
#endif /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */

static shbatt_result_t shbatt_seq_get_pa_temperature( int* tmp_p )
{
	int rc;
	struct pm8xxx_adc_chan_result adc_result;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8xxx_adc_read(ADC_MPP_1_AMUX3, &adc_result);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error reading adc channel = %d, rc = %d\n", ADC_MPP_1_AMUX3, rc);
		goto error;
	}
	*tmp_p	= adc_result.physical;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_xo_temperature( int* tmp_p )
{
	int rc;
	struct pm8xxx_adc_chan_result adc_result;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	rc = pm8xxx_adc_read(CHANNEL_MUXOFF, &adc_result);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error reading adc channel = %d, rc = %d\n", CHANNEL_MUXOFF, rc);
		goto error;
	}
	*tmp_p	= adc_result.physical / 1024;
	
error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_pmic_temperature( int* tmp_p )
{
	int rc, ret;
	struct pm8xxx_adc_chan_result adc_result;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = msm_xo_mode_vote(xo_handle, MSM_XO_MODE_ON);
	if(ret)
	{
		printk(KERN_ERR "%s msm_xo_mode_vote ON err=%d", __FUNCTION__, ret);
	}

	usleep(WAIT_TIME_AFTER_XO_VOTE_ON);

	rc = pm8xxx_adc_read(CHANNEL_DIE_TEMP, &adc_result);

	ret = msm_xo_mode_vote(xo_handle, MSM_XO_MODE_OFF);
	if(ret)
	{
		printk(KERN_ERR "%s msm_xo_mode_vote OFF err=%d", __FUNCTION__, ret);
	}

	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error reading adc channel = %d, rc = %d\n", CHANNEL_DIE_TEMP, rc);
		goto error;
	}
	*tmp_p	= adc_result.physical / 1024;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_smbc_charger( shbatt_smbc_chg_t chg )
{
	int rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[vol] %dmV [cur] %dmA\n", chg.vol, chg.cur);
	rc = pm8921_charger_vinmin_set( chg.vol );
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_vinmin_set = %d, rc = %d\n", chg.vol, rc);
		goto error;
	}

	rc = pm8921_charger_iusbmax_set( chg.cur );
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_iusbmax_set = %d, rc = %d\n", chg.cur, rc);
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_set_meas_freq( int val )
{
	int  rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[meas freq] %dms \n", val);
	rc = pm8921_charger_set_meas_freq(val);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_set_meas_freq rc = %d\n", rc);
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_read_ocv_for_rbatt( shbatt_adc_conv_uint_t* ocv_p )
{
	int  rc;
	uint measurement;
	uint adc_code;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8921_bms_get_ocv_for_rbatt(&measurement,&adc_code);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_bms_get_ocv_for_rbatt rc = %d\n", rc);
		goto error;
	}
	ocv_p->measurement = measurement;
	ocv_p->adc_code = adc_code;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_read_vsense_for_rbatt( shbatt_adc_conv_uint_t* vsense_p )
{
	int  rc;
	uint vsense;
	uint vsense_mA;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8921_bms_get_vsense_for_rbatt(&vsense, &vsense_mA);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_bms_get_vsense_for_rbatt rc = %d\n", rc);
		goto error;
	}
	vsense_p->adc_code = vsense;
	vsense_p->measurement = vsense_mA;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_read_vbatt_for_rbatt( shbatt_adc_conv_uint_t* vbatt_p )
{
	int  rc;
	uint measurement;
	uint adc_code;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8921_bms_get_vbatt_for_rbatt(&measurement,&adc_code);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_bms_get_vbatt_for_rbatt rc = %d\n", rc);
		goto error;
	}
	vbatt_p->measurement = measurement;
	vbatt_p->adc_code = adc_code;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_read_cc( shbatt_adc_conv_offset_t* cc_mah_p )
{
	int  rc;
	int  cc;
	int64_t  cc_mAs;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8921_bms_get_cc(cc_mah_p->offset, &cc, &cc_mAs);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_bms_get_cc rc = %d\n", rc);
		goto error;
	}
	cc_mah_p->adc_convl.adc_code = cc;
	cc_mah_p->adc_convl.measurement = cc_mAs;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_read_last_good_ocv( shbatt_adc_conv_uint_t* ocv_p )
{
	int  rc;
	uint measurement;
	uint adc_code;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8921_bms_get_last_good_ocv(&measurement,&adc_code);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_bms_get_last_good_ocv rc = %d\n", rc);
		goto error;
	}
	ocv_p->measurement = measurement;
	ocv_p->adc_code = adc_code;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_read_vsense_avg( shbatt_adc_conv_int_t* vsense_avg_p )
{
	int rc;
	int vsense_avg;
	int vsense_avg_mA;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8921_bms_get_vsense_avg_read(&vsense_avg, &vsense_avg_mA);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_bms_get_vsense_avg_read rc = %d\n", rc);
		goto error;
	}
	vsense_avg_p->adc_code = vsense_avg;
	vsense_avg_p->measurement = vsense_avg_mA;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_read_vbatt_avg( shbatt_adc_conv_int_t* vbatt_avg_p )
{
	int  rc;
	int  measurement;
	int  adc_code;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8921_bms_get_vbatt_avg(&measurement,&adc_code);
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_bms_get_vbatt_avg rc = %d\n", rc);
		goto error;
	}
	vbatt_avg_p->measurement = measurement;
	vbatt_avg_p->adc_code = adc_code;

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_auto_enable( int val )
{
	int rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[auto enable] %d \n", val);
	rc = pm8921_charger_auto_enable( val );
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8921_charger_auto_enable = %d, rc = %d\n", val, rc);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_recalib_adc_device( void )
{
	int  rc;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8xxx_adc_recalib_device();
	if (rc) {
		result = SHBATT_RESULT_FAIL;
		SHBATT_ERROR("error pm8xxx_adc_recalib_device rc = %d\n", rc);
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_calibrate_battery_voltage( int* vol_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_CALIBRATE_BATTERY_VOLTAGE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*vol_p = shbatt_usse_pkt.prm.vol;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_wireless_status( int* vol_p )
{
#if defined(CONFIG_PM_WIRELESS_CHARGE)
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	*vol_p = GPIO_GET(SHBATT_GPIO_NUM_WIRELESS_STATUS) ? 1 : 0;

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
#else
	return SHBATT_RESULT_FAIL;
#endif
}

static shbatt_result_t shbatt_seq_set_wireless_switch( int val )
{
#if defined(CONFIG_PM_WIRELESS_CHARGE)
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(val)
		GPIO_SET(SHBATT_GPIO_NUM_WIRELESS_SWITCH, 1);
	else
		GPIO_SET(SHBATT_GPIO_NUM_WIRELESS_SWITCH, 0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
#else
	return SHBATT_RESULT_FAIL;
#endif
}

static shbatt_result_t shbatt_seq_set_vsense_avg_calibration_data( shbatt_vsense_avg_cal_t vac )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[calibration adjmin] %d \n", vac.adjmin);
	SHBATT_TRACE("[calibration adjmax] %d \n", vac.adjmax);
	SHBATT_TRACE("[calibration curmin] %d \n", vac.curmin);
	SHBATT_TRACE("[calibration curmax] %d \n", vac.curmax);

	if ((vac.adjmin == -1) &&
		(vac.adjmax == -1) &&
		(vac.curmin == -1) &&
		(vac.curmax == -1))
	{
		SHBATT_TRACE("pm8xxx_calib_ccadc() call \n");
		pm8xxx_calib_ccadc();
	}

	pm8xxx_cc_set_vsense_avg_calibration_data(vac.adjmin, vac.adjmax, vac.curmin, vac.curmax);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_refresh_vsense_avg_calibration_data( void )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	pm8xxx_cc_refresh_vsense_avg_calibration_data();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

#ifdef CONFIG_SHTMP_SENSOR
static shbatt_result_t shbatt_seq_get_apq_temperature( int* tmp_p )
{
	shtmp_result_t shtmp_ret;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	shtmp_ret = shtmp_get_temperature( tmp_p );
	
	if(shtmp_ret != SHTMP_RESULT_SUCCESS)
	{
		SHBATT_ERROR("error, cannot get shtmp, shtmp_ret=%d\n", shtmp_ret);
		result = SHBATT_RESULT_FAIL;
	}
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	return result;
}
#endif /* CONFIG_SHTMP_SENSOR */

/* TODO New API add point */

/*+-----------------------------------------------------------------------------+*/
/*| @ PLATFORM DRIVER MODULE CODE AREA :										|*/
/*+-----------------------------------------------------------------------------+*/
static int shbatt_drv_create_device( void )
{
	struct device* dev_p;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = alloc_chrdev_region(&shbatt_dev,0,1,SHBATT_DEV_NAME);

	if(ret < 0)
	{
		SHBATT_ERROR("%s : alloc_chrdev_region failed. ret = %d\n",__FUNCTION__,ret);
		goto create_device_exit_0;
	}

	shbatt_major = MAJOR(shbatt_dev);
	shbatt_minor = MINOR(shbatt_dev);

	cdev_init(&shbatt_cdev,&shbatt_fops);

	shbatt_cdev.owner = THIS_MODULE;

	ret = cdev_add(&shbatt_cdev,shbatt_dev,1);

	if(ret < 0)
	{
		SHBATT_ERROR("%s : cdev_add failed. ret = %d\n",__FUNCTION__,ret);
		goto create_device_exit_1;
	}

	shbatt_dev_class = class_create(THIS_MODULE,SHBATT_DEV_NAME);

	if(IS_ERR(shbatt_dev_class))
	{
		ret = PTR_ERR(shbatt_dev_class);
		SHBATT_ERROR("%s : class_create failed. ret = %d\n",__FUNCTION__,ret);
		goto create_device_exit_2;
	}

	dev_p = device_create(shbatt_dev_class,NULL,shbatt_dev,&shbatt_cdev,SHBATT_DEV_NAME);

	if(IS_ERR(dev_p))
	{
		ret = PTR_ERR(dev_p);
		SHBATT_ERROR("%s : device_create failed. ret = %d\n",__FUNCTION__,ret);
		goto create_device_exit_3;
	}

#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
	shbatt_dev_p = dev_p;
	atomic_set(&wlchg_disable_charge_count, 0);
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */

	l22_vreg = regulator_get(dev_p, l22_vreg_name);
	if (!l22_vreg || IS_ERR(l22_vreg))
	{
		ret = PTR_ERR(l22_vreg);
		l22_vreg = NULL;
		SHBATT_ERROR("%s: regulator_get L22 = %d\n", __FUNCTION__,ret);
	}
	l22_vreg_on = 0;

	l17_vreg = regulator_get(dev_p, l17_vreg_name);
	if (!l17_vreg || IS_ERR(l17_vreg))
	{
		ret = PTR_ERR(l17_vreg);
		l17_vreg = NULL;
		SHBATT_ERROR("%s: regulator_get L17 = %d\n", __FUNCTION__,ret);
	}

#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	l17_vreg_on = 1;
#else /* CONFIG_PM_SUPPORT_BATT_AUTH */
	l17_vreg_on = 0;
#endif /* CONFIG_PM_SUPPORT_BATT_AUTH */

	atomic_set(&shbatt_usse_op_cnt,0);

	init_waitqueue_head(&shbatt_usse_wait);

	init_completion(&shbatt_usse_cmp);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;

create_device_exit_3:
	class_destroy(shbatt_dev_class);

create_device_exit_2:
	cdev_del(&shbatt_cdev);

create_device_exit_1:
	unregister_chrdev_region(shbatt_dev,1);

create_device_exit_0:

	return ret;
}
#ifdef CONFIG_BATTERY_SH
static int shbatt_drv_probe( struct platform_device* dev_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_drv_create_ps_attrs(dev_p) < 0)
	{
		SHBATT_ERROR("%s : create attrs failed.\n",__FUNCTION__);
		return -EPERM;
	}

	if(shbatt_drv_create_device() < 0)
	{
		SHBATT_ERROR("%s : create device failed.\n",__FUNCTION__);
		return -EPERM;
	}

	if(shbatt_drv_register_irq(dev_p) < 0)
	{
		SHBATT_ERROR("%s : register irq failed.\n",__FUNCTION__);
		return -EPERM;
	}

	xo_handle = msm_xo_get(MSM_XO_TCXO_D0, "usb");

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return 0;
}
#else
static int shbatt_drv_probe( struct platform_device* dev_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_drv_create_device() < 0)
	{
		SHBATT_ERROR("%s : create device failed.\n",__FUNCTION__);
		return -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return 0;
}
#endif


#ifdef CONFIG_BATTERY_SH
static int __devexit shbatt_drv_remove( struct platform_device* dev_p )
{
#ifdef CONFIG_BATTERY_SH
	msm_xo_put(xo_handle);
	
	return 0;
#else
	shbatt_pm_device_info_t* di_p;

	int irq, idx;

	for(idx = 0; idx < ARRAY_SIZE(shbatt_power_supplies); idx++)
	{
		power_supply_unregister(&shbatt_power_supplies[idx]);
	}

	device_destroy(shbatt_dev_class,shbatt_dev);

	class_destroy(shbatt_dev_class);

	cdev_del(&shbatt_cdev);

	unregister_chrdev_region(shbatt_dev,1);

	di_p = platform_get_drvdata(dev_p);

	twl6030_interrupt_mask(0x04,REG_INT_MSK_LINE_A);
	twl6030_interrupt_mask(0x04,REG_INT_MSK_STS_A);

	irq = platform_get_irq(dev_p,0);

	free_irq(irq,di_p);

	platform_set_drvdata(dev_p,NULL);

	kfree(di_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return 0;
#endif
}
#else
static int __devexit shbatt_drv_remove( struct platform_device* dev_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	device_destroy(shbatt_dev_class,shbatt_dev);

	class_destroy(shbatt_dev_class);

	cdev_del(&shbatt_cdev);

	unregister_chrdev_region(shbatt_dev,1);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return 0;
}
#endif

#ifdef CONFIG_BATTERY_SH
static void shbatt_drv_shutdown( struct platform_device* dev_p )
{

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_task_is_initialized = false;

	alarm_cancel(&shbatt_tim_soc_poll.alm);

	alarm_cancel(&shbatt_tim_soc_poll_sleep.alm);

	alarm_cancel(&shbatt_tim_soc_poll_multi.alm);

	alarm_cancel(&shbatt_tim_soc_poll_sleep_multi.alm);

	alarm_cancel(&shbatt_tim_low_battery.alm);

	alarm_cancel(&shbatt_tim_fatal_battery.alm);

	alarm_cancel(&shbatt_tim_charging_fatal_battery.alm);

	alarm_cancel(&shbatt_tim_low_battery_shutdown.alm);

	alarm_cancel(&shbatt_tim_overcurr.alm);

	alarm_cancel(&shbatt_tim_battery_present.alm);

	alarm_cancel(&shbatt_tim_wireless_charge.alm);

	flush_workqueue(shbatt_task_workqueue_p);

	destroy_workqueue(shbatt_task_workqueue_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
#else
static void shbatt_drv_shutdown( struct platform_device* dev_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	flush_workqueue(shbatt_task_workqueue_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
#endif

static int shbatt_drv_resume(struct platform_device * dev_p)
{
	struct timespec ts;
	struct timespec now_time;
	int diff_time;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	now_time = ktime_to_timespec(alarm_get_elapsed_realtime());
	diff_time = now_time.tv_sec - shbatt_last_timer_expire_time.tv_sec;
	
	SHBATT_TRACE("[P] %s shbatt_last_timer_expire_time.tv_sec=%010lu shbatt_last_timer_expire_time.tv_nsec=%09lu \n",__FUNCTION__, shbatt_last_timer_expire_time.tv_sec, shbatt_last_timer_expire_time.tv_nsec);
	SHBATT_TRACE("[P] %s now_time.tv_sec=%010lu now_time.tv_nsec=%09lu \n",__FUNCTION__, now_time.tv_sec, now_time.tv_nsec);
	SHBATT_TRACE("[P] %s shbatt_timer_restarted=%d diff_time=%d \n",__FUNCTION__, shbatt_timer_restarted, diff_time);
	
	if((shbatt_timer_restarted == false) && (diff_time > 2400))
	{
		ts.tv_sec = now_time.tv_sec + 10;
		ts.tv_nsec = now_time.tv_nsec;
		
		SHBATT_TRACE("[P] %s timer restart ts.tv_sec=%010lu now_time.ts.tv_nsec=%09lu \n",__FUNCTION__, ts.tv_sec, ts.tv_nsec);
		
		shbatt_timer_restarted = true;
		shbatt_tim_soc_poll.prm = 0;
		alarm_cancel(&shbatt_tim_soc_poll.alm);
		alarm_start_range(&shbatt_tim_soc_poll.alm,
							timespec_to_ktime(ts),
							timespec_to_ktime(ts));
	}
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return 0;
}

static int shbatt_drv_suspend(struct platform_device * dev_p, pm_message_t state)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return 0;

}

static struct notifier_block alarm_notifier;

static struct platform_driver shbatt_platform_driver = {
	.probe		= shbatt_drv_probe,
	.remove		= __devexit_p(shbatt_drv_remove),
	.shutdown	= shbatt_drv_shutdown,
	.driver		= {
		.name	= SHBATT_DEV_NAME,
		.owner	= THIS_MODULE,
	},
    .resume       = shbatt_drv_resume,
	.suspend      = shbatt_drv_suspend,
};

static int __init shbatt_drv_module_init( void )
{
	int rc;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
{
	static const unsigned short hw_rev_pp2 = 4;

	if(GPIO_GET(31) == 0 || sh_boot_get_hw_revision() >= hw_rev_pp2)
	{
		wlchg_i2c_driver.driver.name = "wlchg_i2c_gsbi1";
		wlchg_i2c_driver.id_table = wlchg_i2c_gsbi1_id;
	}
	else
	{
		wlchg_i2c_driver.driver.name = "wlchg_i2c_gsbi2";
		wlchg_i2c_driver.id_table = wlchg_i2c_gsbi2_id;
	}

	i2c_add_driver(&wlchg_i2c_driver);
}
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */


	shbatt_task_workqueue_p = create_singlethread_workqueue("shbatt_task");

	mutex_init(&shbatt_api_lock);

	mutex_init(&shbatt_task_lock);

	init_completion(&shbatt_api_cmp);
	spin_lock_init(&shbatt_pkt_lock);


	wake_lock_init(&shbatt_wake_lock,WAKE_LOCK_SUSPEND,"shbatt_wake");

#if defined(CONFIG_PM_WIRELESS_CHARGE)
	wake_lock_init(&shbatt_wireless_wake_lock,WAKE_LOCK_SUSPEND,"shbatt_wireless_wake");
#endif /* CONFIG_PM_WIRELESS_CHARGE */

	atomic_set(&shbatt_wake_lock_num,0);

	alarm_init(&shbatt_tim_soc_poll.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_fuelgauge_soc_poll_timer_expire_cb);

	alarm_init(&shbatt_tim_soc_poll_sleep.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME,
						  shbatt_seq_fuelgauge_soc_poll_timer_expire_cb);

	alarm_init(&shbatt_tim_soc_poll_multi.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_fuelgauge_soc_poll_timer_expire_multi_cb);

	alarm_init(&shbatt_tim_soc_poll_sleep_multi.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME,
						  shbatt_seq_fuelgauge_soc_poll_timer_expire_multi_cb);

	alarm_init(&shbatt_tim_low_battery.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_low_battery_poll_timer_expire_cb);

	alarm_init(&shbatt_tim_fatal_battery.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_fatal_battery_poll_timer_expire_cb);

	alarm_init(&shbatt_tim_charging_fatal_battery.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_charging_fatal_battery_poll_timer_expire_cb);

	alarm_init(&shbatt_tim_low_battery_shutdown.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_low_battery_shutdown_poll_timer_expire_cb);

	alarm_init(&shbatt_tim_overcurr.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_overcurr_timer_expire_cb);

	alarm_init(&shbatt_tim_battery_present.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_battery_present_poll_timer_expire_cb);

	alarm_init(&shbatt_tim_wireless_charge.alm,
						  ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
						  shbatt_seq_wireless_charge_poll_timer_expire_cb);

	platform_driver_register(&shbatt_platform_driver);

	rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (!rc)
		rc = pm8xxx_batt_alarm_disable(
			PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	if (rc) {
		pr_err("%s: unable to set batt alarm state\n", __func__);
	}

	rc = pm8xxx_batt_alarm_hold_time_set(
				PM8XXX_BATT_ALARM_HOLD_TIME_16_MS);
	if (rc) {
		pr_err("%s: unable to set batt alarm hold time\n", __func__);
	}

	/* PWM enabled at 2Hz */
	rc = pm8xxx_batt_alarm_pwm_rate_set(1, 7, 4);
	if (rc) {
		pr_err("%s: unable to set batt alarm pwm rate\n", __func__);
	}
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return 0;
}

static void __exit shbatt_drv_module_exit( void )
{
	int rc;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	platform_driver_unregister(&shbatt_platform_driver);

	rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (!rc)
		rc = pm8xxx_batt_alarm_disable(
			PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	if (rc)
		pr_err("%s: unable to set batt alarm state\n", __func__);

	rc |= pm8xxx_batt_alarm_unregister_notifier(&alarm_notifier);
	if (rc)
		pr_err("%s: unable to register alarm notifier\n", __func__);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

module_init(shbatt_drv_module_init);
module_exit(shbatt_drv_module_exit);

MODULE_DESCRIPTION("SH Battery Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.0");

shbatt_result_t shbatt_api_get_terminal_temperature( int* tmp_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_TERMINAL_TEMPERATURE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.tmp_p = tmp_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_modem_temperature( int* tmp_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_MODEM_TEMPERATURE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.tmp_p = tmp_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_fuelgauge_current( int* cur_p )
{

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		result = shbatt_seq_get_battery_current(cur_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
shbatt_result_t shbatt_api_get_fuelgauge_voltage( int* vol_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_FUELGAUGE_VOLTAGE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.vol_p = vol_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_fuelgauge_device_id( unsigned int* dev_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_FUELGAUGE_DEVICE_ID;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.dev_p = dev_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_set_fuelgauge_mode( shbatt_fuelgauge_mode_t mode )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_set_fuelgauge_mode(mode);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_fuelgauge_accumulate_current( shbatt_accumulate_current_t* acc_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_FUELGAUGE_ACCUMULATE_CURRENT;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.acc_p = acc_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_clr_fuelgauge_accumulate_current( void )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_CLR_FUELGAUGE_ACCUMULATE_CURRENT;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.val = 0;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_fuelgauge_temperature( int* tmp_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_FUELGAUGE_TEMPERATURE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.tmp_p = tmp_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_fuelgauge_current_ad( int* raw_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_FUELGAUGE_CURRENT_AD;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.raw_p = raw_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_read_adc_channel_buffered( shbatt_adc_t* adc_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		SHBATT_INFO("shbatt_task_is_initialized == false:%s(%s,%d)", __FUNCTION__, __FILE__, __LINE__);
		result = SHBATT_RESULT_REJECTED;
		goto end;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		SHBATT_INFO("pkt_p == NULL:%s(%s,%d)", __FUNCTION__, __FILE__, __LINE__);
		result = SHBATT_RESULT_REJECTED;
		goto end;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_READ_ADC_CHANNEL_BUFFERED;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.adc_p = adc_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);
end:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_get_battery_log_info( shbatt_batt_log_info_t* bli_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_BATTERY_LOG_INFO;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.bli_p = bli_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

shbatt_result_t shbatt_api_notify_charger_cable_status( shbatt_cable_status_t cbs )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		shbatt_cur_boot_time_cable_status = cbs;

		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_NOTIFY_CHARGER_CABLE_STATUS;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.cbs = cbs;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_notify_battery_charging_status( shbatt_chg_status_t cgs )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		shbatt_cur_boot_time_chg_status = cgs;

		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_NOTIFY_BATTERY_CHARGING_STATUS;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.cgs = cgs;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_notify_battery_capacity( int cap )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_NOTIFY_BATTERY_CAPACITY;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.cap = cap;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_notify_charging_state_machine_enable( shbatt_boolean_t ena )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_NOTIFY_CHARGING_STATE_MACHINE_ENABLE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.ena = ena;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_get_battery_safety( shbatt_safety_t* saf_p )
{
	shbatt_packet_t* pkt_p;

	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	mutex_lock(&shbatt_api_lock);

	INIT_COMPLETION(shbatt_api_cmp);

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_GET_BATTERY_SAFETY;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = &shbatt_api_cmp;
	pkt_p->hdr.ret_p = &result;
	pkt_p->prm.saf_p = saf_p;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	wait_for_completion_killable(&shbatt_api_cmp);

	mutex_unlock(&shbatt_api_lock);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_api_initialize( void )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == true)
	{
		shbatt_task_is_initialized = false;
		shbatt_task_is_initialized_multi = true;
	}

	if(shbatt_task_is_initialized_multi == false)
	{
		initialize_wireless_charger();
		hdq_initialize();
		shbatt_seq_initialize_battery_trace_data();
	}	

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_INITIALIZE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.val = 0;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

static shbatt_result_t shbatt_api_exec_low_battery_check_sequence( int evt )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_EXEC_LOW_BATTERY_CHECK_SEQUENCE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.evt = evt;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_post_battery_log_info( shbatt_batt_log_info_t bli )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_POST_BATTERY_LOG_INFO;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.bli = bli;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_get_hw_revision( uint * p_hw_rev )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	sharp_smem_common_type * p_smem = 0;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	p_smem = sh_smem_get_common_address();

	if( p_hw_rev && p_smem )
	{
		*p_hw_rev = p_smem->sh_hw_revision;
	}
	else
	{
		SHBATT_ERROR("%s : invalid pointer, p_hw_rev=0x%08x, p_smem=0x%08x\n",__FUNCTION__,(uint)p_hw_rev,(uint)p_smem);
		result = SHBATT_RESULT_FAIL;
	}
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

shbatt_result_t shbatt_api_get_smem_info( shbatt_smem_info_t * p_smem_info )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	sharp_smem_common_type * p_smem = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	p_smem = sh_smem_get_common_address();

	if( p_smem_info && p_smem )
	{
		p_smem_info->battery_present		= p_smem->shpwr_battery_present;
		p_smem_info->battery_voltage		= p_smem->shpwr_battery_voltage;
		p_smem_info->battery_temperature	= p_smem->shpwr_battery_temperature;
		p_smem_info->xo_temperature			= p_smem->shpwr_xo_temperature;
		p_smem_info->cable_status			= p_smem->shpwr_cable_status;
		p_smem_info->product_flg			= (p_smem->shdiag_FlagData & GISPRODUCT_F_MASK)  ? 1 : 0;
		p_smem_info->softupdate_flg			= (p_smem->shdiag_FlagData & GISSOFTUP_F_MASK)   ? 1 : 0;
		p_smem_info->abnormal_flg			= (p_smem->shdiag_FlagData & GISABNORMAL_F_MASK) ? 1 : 0;
		p_smem_info->restrain_flg			= (p_smem->shdiag_FlagData & GISRESTRAIN_F_MASK) ? 1 : 0;
		p_smem_info->fullchg_flg			= p_smem->shdiag_fullchgflg;
#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
		p_smem_info->batauthflg				= p_smem->shpwr_batauthflg;
#endif /* CONFIG_PM_SUPPORT_BATT_AUTH */
		memcpy(p_smem_info->shbatt_fuel_data, p_smem->shpwr_fuel_data, sizeof(p_smem_info->shbatt_fuel_data));
		memcpy(p_smem_info->shbatt_vbat_data, p_smem->shpwr_vbat_data, sizeof(p_smem_info->shbatt_vbat_data));
		memcpy(p_smem_info->charge_th_high_array, p_smem->shdiag_charge_th_high, sizeof(p_smem->shdiag_charge_th_high));
		memcpy(p_smem_info->charge_th_low_array, p_smem->shdiag_charge_th_low, sizeof(p_smem->shdiag_charge_th_low));
	}
	else
	{
		SHBATT_ERROR("%s : invalid pointer, p_smem_info=0x%08x, p_smem=0x%08x\n",__FUNCTION__,(uint)p_smem_info,(uint)p_smem);
		result = SHBATT_RESULT_FAIL;
	}
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

/*from task.*/
static void shbatt_task_cmd_get_terminal_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t2 cb_func;

	int tmp = 0;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_terminal_temperature(&tmp);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t2)pkt_p->hdr.cb_p;

		cb_func(tmp,result);
	}
	else
	{
		if(pkt_p->prm.tmp_p != NULL)
		{
			*(pkt_p->prm.tmp_p) = tmp;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static void shbatt_task_cmd_get_modem_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t2 cb_func;

	int tmp = 0;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_modem_temperature(&tmp);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t2)pkt_p->hdr.cb_p;

		cb_func(tmp,result);
	}
	else
	{
		if(pkt_p->prm.tmp_p != NULL)
		{
			*(pkt_p->prm.tmp_p) = tmp;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static void shbatt_task_cmd_get_fuelgauge_voltage( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t2 cb_func;

	int vol = 0;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_fuelgauge_voltage(&vol);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t2)pkt_p->hdr.cb_p;

		cb_func(vol,result);
	}
	else
	{
		if(pkt_p->prm.vol_p != NULL)
		{
			*(pkt_p->prm.vol_p) = vol;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_fuelgauge_device_id( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t10 cb_func;

	unsigned int dev = 0;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_fuelgauge_device_id(&dev);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t10)pkt_p->hdr.cb_p;

		cb_func(dev,result);
	}
	else
	{
		if(pkt_p->prm.dev_p != NULL)
		{
			*(pkt_p->prm.dev_p) = dev;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_set_fuelgauge_mode( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = 
	shbatt_seq_set_fuelgauge_mode(pkt_p->prm.mode);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_fuelgauge_accumulate_current( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t11 cb_func;

	shbatt_accumulate_current_t acc;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_fuelgauge_accumulate_current(&acc);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t11)pkt_p->hdr.cb_p;

		cb_func(acc,result);
	}
	else
	{
		if(pkt_p->prm.acc_p != NULL)
		{
			*(pkt_p->prm.acc_p) = acc;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_clr_fuelgauge_accumulate_current( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_clr_fuelgauge_accumulate_current();

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_fuelgauge_current_ad( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t10 cb_func;

	int raw = 0;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_fuelgauge_current_ad(&raw);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t10)pkt_p->hdr.cb_p;

		cb_func(raw,result);
	}
	else
	{
		if(pkt_p->prm.raw_p != NULL)
		{
			*(pkt_p->prm.raw_p) = raw;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_read_adc_channel_buffered( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t9 cb_func;

	shbatt_adc_t adc;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(!pkt_p)
	{
		SHBATT_ERROR("error pkt_p is NULL = %x\n", (unsigned int)pkt_p);
		goto error;
	}

	if(!pkt_p->prm.adc_p)
	{
		SHBATT_ERROR("error pkt_p->prm.adc_p is NULL = %x\n", (unsigned int)pkt_p->prm.adc_p);
		goto error;
	}

	adc.channel = pkt_p->prm.adc_p->channel;

	result = shbatt_seq_read_adc_channel_buffered(&adc);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t9)pkt_p->hdr.cb_p;

		cb_func(adc,result);
	}
	else
	{
		*(pkt_p->prm.adc_p) = adc;

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

error:

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_battery_log_info( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t12 cb_func;

	shbatt_batt_log_info_t bli;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_log_info(&bli);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t12)pkt_p->hdr.cb_p;

		cb_func(bli,result);
	}
	else
	{
		if(pkt_p->prm.bli_p != NULL)
		{
			*(pkt_p->prm.bli_p) = bli;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static void shbatt_task_cmd_notify_charger_cable_status( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_notify_charger_cable_status(pkt_p->prm.cbs);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_notify_battery_charging_status( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_notify_battery_charging_status(pkt_p->prm.cgs);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_notify_battery_capacity( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_notify_battery_capacity(pkt_p->prm.cap);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_notify_charging_state_machine_enable( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_notify_charging_state_machine_enable(pkt_p->prm.ena);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_initialize( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_initialize();

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_exec_low_battery_check_sequence( shbatt_packet_t* pkt_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_seq_exec_low_battery_check_sequence(pkt_p->prm.evt);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}


static void shbatt_task_cmd_get_battery_safety( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t6 cb_func;

	shbatt_safety_t saf = SHBATT_SAFETY_BATT_UNKNOWN;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_battery_safety(&saf);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t6)pkt_p->hdr.cb_p;

		cb_func(saf,result);
	}
	else
	{
		if(pkt_p->prm.saf_p != NULL)
		{
			*(pkt_p->prm.saf_p) = saf;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_get_fuelgauge_temperature( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t2 cb_func;

	int tmp = 0;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_fuelgauge_temperature(&tmp);

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t2)pkt_p->hdr.cb_p;

		cb_func(tmp,result);
	}
	else
	{
		if(pkt_p->prm.tmp_p != NULL)
		{
			*(pkt_p->prm.tmp_p) = tmp;
		}

		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static void shbatt_task_cmd_post_battery_log_info( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;

	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		if(shbatt_battery_log_cache_pre_initialize_count < SHBATT_BATTERY_LOG_CACHE_PRE_INITIALIZE_MAX)
		{
			shbatt_battery_log_cache_pre_initialize[shbatt_battery_log_cache_pre_initialize_count] = pkt_p->prm.bli;
			shbatt_battery_log_cache_pre_initialize_count++;
			result = SHBATT_RESULT_SUCCESS;
		}
		else
		{
			SHBATT_ERROR("%s, cannot cache, cnt=%d/%d, event=%d\n",__FUNCTION__, shbatt_battery_log_cache_pre_initialize_count, SHBATT_BATTERY_LOG_CACHE_PRE_INITIALIZE_MAX, (int)pkt_p->prm.bli.event_num);
			result = SHBATT_RESULT_FAIL;
		}
	}
	else
	{
		result = shbatt_seq_post_battery_log_info(pkt_p->prm.bli);
		if(result)
		{
			SHBATT_ERROR("%s, post battery log fail, event=%d, ret=%d\n",__FUNCTION__, (int)pkt_p->prm.bli.event_num, (int)result);
		}
	}

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_exec_fuelgauge_soc_poll_sequence( shbatt_packet_t* pkt_p )
{
  SHBATT_TRACE("[S] %s \n",__FUNCTION__);

  shbatt_seq_exec_fuelgauge_soc_poll_sequence(pkt_p->prm.soc);

  SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_exec_overcurr_check_sequence( shbatt_packet_t* pkt_p )
{
  SHBATT_TRACE("[S] %s \n",__FUNCTION__);

  shbatt_seq_exec_overcurr_check_sequence();

  SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_task_cmd_exec_battery_present_check_sequence( shbatt_packet_t* pkt_p )
{
  SHBATT_TRACE("[S] %s \n",__FUNCTION__);

  shbatt_seq_exec_battery_present_check_sequence(pkt_p->prm.seq);

  SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}




static shbatt_packet_t* shbatt_task_get_packet( void )
{
	int idx;

	unsigned long flags;

	shbatt_packet_t* ret = NULL;

	spin_lock_irqsave(&shbatt_pkt_lock,flags);

	for(idx = 0; idx < 16; idx++)
	{
		if(shbatt_pkt[idx].is_used == false)
		{
			shbatt_pkt[idx].is_used = true;

			ret = &shbatt_pkt[idx];

			break;
		}
	}

	spin_unlock_irqrestore(&shbatt_pkt_lock,flags);

	return ret;
}
static void shbatt_task_free_packet( shbatt_packet_t* pkt )
{
	unsigned long flags;

	spin_lock_irqsave(&shbatt_pkt_lock,flags);

	pkt->is_used = false;

	spin_unlock_irqrestore(&shbatt_pkt_lock,flags);

	return;
}



static int shbatt_drv_ioctl_cmd_invalid( struct file* fi_p, unsigned long arg )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return -EINVAL;
}

static int shbatt_drv_ioctl_cmd_pull_usse_packet( struct file* fi_p, unsigned long arg )
{
	shbatt_usse_packet_t* pkt_p;

	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	pkt_p = &shbatt_usse_pkt;

	if(copy_to_user((shbatt_usse_packet_t*)arg,pkt_p,sizeof(shbatt_usse_packet_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_done_usse_packet( struct file* fi_p, unsigned long arg )
{
	shbatt_usse_packet_t* pkt_p;

	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	pkt_p = &shbatt_usse_pkt;

	if(copy_from_user(pkt_p,(shbatt_usse_packet_t*)arg,sizeof(shbatt_usse_packet_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_timer( struct file* fi_p, unsigned long arg )
{
	shbatt_poll_timer_info_t pti;

	struct timespec ts;

	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&pti,(shbatt_poll_timer_info_t*)arg,sizeof(shbatt_poll_timer_info_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ts = ktime_to_timespec(alarm_get_elapsed_realtime());

		ts.tv_sec += pti.ms / 1000;
		ts.tv_nsec += (pti.ms % 1000) * 1000000;

		if(ts.tv_nsec >= 1000000000)
		{
			ts.tv_sec += 1;
			ts.tv_nsec -= 1000000000;
		}

		if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FUELGAUGE_SOC)
		{
			shbatt_tim_soc_poll.prm = pti.prm;

			alarm_cancel(&shbatt_tim_soc_poll.alm);

			alarm_start_range(&shbatt_tim_soc_poll.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FUELGAUGE_SOC_SLEEP)
		{
			shbatt_tim_soc_poll_sleep.prm = pti.prm;

			alarm_cancel(&shbatt_tim_soc_poll_sleep.alm);

			alarm_start_range(&shbatt_tim_soc_poll_sleep.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FUELGAUGE_SOC_MULTI)
		{
			shbatt_tim_soc_poll_multi.prm = pti.prm;

			alarm_cancel(&shbatt_tim_soc_poll_multi.alm);

			alarm_start_range(&shbatt_tim_soc_poll_multi.alm,
								timespec_to_ktime(ts),
								timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FUELGAUGE_SOC_SLEEP_MULTI)
		{
			shbatt_tim_soc_poll_sleep_multi.prm = pti.prm;

			alarm_cancel(&shbatt_tim_soc_poll_sleep_multi.alm);

			alarm_start_range(&shbatt_tim_soc_poll_sleep_multi.alm,
								timespec_to_ktime(ts),
								timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_LOW_BATTERY)
		{
			cancel_delayed_work(&lowbatt_shutdown_struct);
			schedule_delayed_work_on(0, &lowbatt_shutdown_struct, msecs_to_jiffies(60000));
			shbatt_tim_low_battery.prm = pti.prm;

			alarm_cancel(&shbatt_tim_low_battery.alm);

			alarm_start_range(&shbatt_tim_low_battery.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FATAL_BATTERY)
		{
			shbatt_tim_fatal_battery.prm = pti.prm;

			alarm_cancel(&shbatt_tim_fatal_battery.alm);

			alarm_start_range(&shbatt_tim_fatal_battery.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_CHARGING_FATAL_BATTERY)
		{
			cancel_delayed_work(&lowbatt_shutdown_struct);
			schedule_delayed_work_on(0, &lowbatt_shutdown_struct, msecs_to_jiffies(60000));
			shbatt_tim_charging_fatal_battery.prm = pti.prm;

			alarm_cancel(&shbatt_tim_charging_fatal_battery.alm);

			alarm_start_range(&shbatt_tim_charging_fatal_battery.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_LOW_BATTERY_SHUTDOWN)
		{
			cancel_delayed_work(&lowbatt_shutdown_struct);
			schedule_delayed_work_on(0, &lowbatt_shutdown_struct, msecs_to_jiffies(60000));
			shbatt_tim_low_battery_shutdown.prm = pti.prm;

			alarm_cancel(&shbatt_tim_low_battery_shutdown.alm);

			alarm_start_range(&shbatt_tim_low_battery_shutdown.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_OVERCURR)
		{
			shbatt_tim_overcurr.prm = pti.prm;

			alarm_cancel(&shbatt_tim_overcurr.alm);

			alarm_start_range(&shbatt_tim_overcurr.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_BATTERY_PRESENT)
		{
			shbatt_tim_battery_present.prm = pti.prm;

			alarm_cancel(&shbatt_tim_battery_present.alm);

			alarm_start_range(&shbatt_tim_battery_present.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_WIRELESS_CHARGE)
		{
			shbatt_tim_wireless_charge.prm = pti.prm;

			alarm_cancel(&shbatt_tim_wireless_charge.alm);

			alarm_start_range(&shbatt_tim_wireless_charge.alm,
												timespec_to_ktime(ts),
												timespec_to_ktime(ts));
		}
		else
		{
			SHBATT_ERROR("%s : timer type invalid.\n",__FUNCTION__);
			ret = -EPERM;
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_clr_timer( struct file* fi_p, unsigned long arg )
{
	shbatt_poll_timer_info_t pti;

	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&pti,(shbatt_poll_timer_info_t*)arg,sizeof(shbatt_poll_timer_info_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FUELGAUGE_SOC)
		{
			alarm_cancel(&shbatt_tim_soc_poll.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FUELGAUGE_SOC_SLEEP)
		{
			alarm_cancel(&shbatt_tim_soc_poll_sleep.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FUELGAUGE_SOC_MULTI)
		{
			alarm_cancel(&shbatt_tim_soc_poll_multi.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FUELGAUGE_SOC_SLEEP_MULTI)
		{
			alarm_cancel(&shbatt_tim_soc_poll_sleep_multi.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_LOW_BATTERY)
		{
			alarm_cancel(&shbatt_tim_low_battery.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_FATAL_BATTERY)
		{
			alarm_cancel(&shbatt_tim_fatal_battery.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_CHARGING_FATAL_BATTERY)
		{
			alarm_cancel(&shbatt_tim_charging_fatal_battery.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_LOW_BATTERY_SHUTDOWN)
		{
			alarm_cancel(&shbatt_tim_low_battery_shutdown.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_OVERCURR)
		{
			alarm_cancel(&shbatt_tim_overcurr.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_BATTERY_PRESENT)
		{
			alarm_cancel(&shbatt_tim_battery_present.alm);
		}
		else if(pti.ptt == SHBATT_POLL_TIMER_TYPE_WIRELESS_CHARGE)
		{
			alarm_cancel(&shbatt_tim_wireless_charge.alm);
		}
		else
		{
			SHBATT_ERROR("%s : timer type invalid.\n",__FUNCTION__);
			ret = -EPERM;
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_voltage_alarm( struct file* fi_p, unsigned long arg )
{
	shbatt_voltage_alarm_info_t vai;

	int ret = 0;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&vai,(shbatt_voltage_alarm_info_t*)arg,sizeof(shbatt_voltage_alarm_info_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		if(vai.vat == SHBATT_VOLTAGE_ALARM_TYPE_LOW_BATTERY)
		{
			SHBATT_TRACE("[P] %s shbatt_low_battery_interrupt=false \n",__FUNCTION__);
			shbatt_low_battery_interrupt = false;
			cancel_delayed_work(&lowbatt_shutdown_struct);
			result = shbatt_seq_enable_battery_voltage_alarm_int(vai.max, vai.min, (void*)shbatt_seq_low_battery_voltage_alarm_isr);
		}
		else if(vai.vat == SHBATT_VOLTAGE_ALARM_TYPE_FATAL_BATTERY)
		{
			result = shbatt_seq_enable_battery_voltage_alarm_int(vai.max, vai.min, (void*)shbatt_seq_fatal_battery_voltage_alarm_isr);
		}
		else if(vai.vat == SHBATT_VOLTAGE_ALARM_TYPE_CHARGING_FATAL_BATTERY)
		{
			SHBATT_TRACE("[P] %s shbatt_low_battery_interrupt=false \n",__FUNCTION__);
			cancel_delayed_work(&lowbatt_shutdown_struct);
			shbatt_low_battery_interrupt = false;
			result = shbatt_seq_enable_battery_voltage_alarm_int(vai.max, vai.min, (void*)shbatt_seq_charging_fatal_battery_voltage_alarm_isr);
		}
		else
		{
			SHBATT_ERROR("%s : voltage alarm type invalid.\n",__FUNCTION__);
			ret = -EPERM;
		}
		
		if(result != SHBATT_RESULT_SUCCESS)
		{
			SHBATT_ERROR("%s : shbatt_seq_enable_battery_voltage_alarm_int fail.\n",__FUNCTION__);
			ret = -EPERM;
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_clr_voltage_alarm( struct file* fi_p, unsigned long arg )
{
	shbatt_voltage_alarm_info_t vai;

	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&vai,(shbatt_voltage_alarm_info_t*)arg,sizeof(shbatt_voltage_alarm_info_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		if((vai.vat == SHBATT_VOLTAGE_ALARM_TYPE_LOW_BATTERY) || 
			  (vai.vat == SHBATT_VOLTAGE_ALARM_TYPE_FATAL_BATTERY) || 
			  (vai.vat == SHBATT_VOLTAGE_ALARM_TYPE_CHARGING_FATAL_BATTERY))
		{
			shbatt_seq_disable_battery_voltage_alarm_int();
		}
		else
		{
			SHBATT_ERROR("%s : voltage alarm type invalid.\n",__FUNCTION__);
			ret = -EPERM;
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_safety( struct file* fi_p, unsigned long arg )
{
	shbatt_safety_t saf;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_battery_safety(&saf);

	if(copy_to_user((shbatt_safety_t*)arg,&saf,sizeof(shbatt_safety_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_terminal_temperature( struct file* fi_p, unsigned long arg )
{
	int tmp;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_terminal_temperature(&tmp);

	if(copy_to_user((int*)arg,&tmp,sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_modem_temperature( struct file* fi_p, unsigned long arg )
{
	int tmp;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_modem_temperature(&tmp);

	if(copy_to_user((int*)arg,&tmp,sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_fuelgauge_current( struct file* fi_p, unsigned long arg )
{
	int cur;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_fuelgauge_current(&cur);

	if(copy_to_user((int*)arg,&cur,sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_fuelgauge_voltage( struct file* fi_p, unsigned long arg )
{
	int vol;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_fuelgauge_voltage(&vol);

	if(copy_to_user((int*)arg,&vol,sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_fuelgauge_device_id( struct file* fi_p, unsigned long arg )
{
	unsigned int dev;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_fuelgauge_device_id(&dev);

	if(copy_to_user((unsigned int*)arg,&dev,sizeof(unsigned int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_set_fuelgauge_mode( struct file* fi_p, unsigned long arg )
{
	shbatt_fuelgauge_mode_t mode;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&mode,(shbatt_fuelgauge_mode_t*)arg,sizeof(shbatt_fuelgauge_mode_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_set_fuelgauge_mode(mode);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_fuelgauge_accumulate_current( struct file* fi_p, unsigned long arg )
{
	shbatt_accumulate_current_t acc;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_fuelgauge_accumulate_current(&acc);

	if(copy_to_user((shbatt_accumulate_current_t*)arg,&acc,sizeof(shbatt_accumulate_current_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_clr_fuelgauge_accumulate_current( struct file* fi_p, unsigned long arg )
{
	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_clr_fuelgauge_accumulate_current();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_fuelgauge_temperature( struct file* fi_p, unsigned long arg )
{
	int tmp;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_fuelgauge_temperature(&tmp);

	if(copy_to_user((int*)arg,&tmp,sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_fuelgauge_current_ad( struct file* fi_p, unsigned long arg )
{
	int raw;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_fuelgauge_current_ad(&raw);

	if(copy_to_user((int*)arg,&raw,sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_adc_channel_buffered( struct file* fi_p, unsigned long arg )
{
	shbatt_adc_t adc;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&adc,(shbatt_adc_t*)arg,sizeof(shbatt_adc_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_read_adc_channel_buffered(&adc);

		if(copy_to_user((shbatt_adc_t*)arg,&adc,sizeof(shbatt_adc_t)) != 0)
		{
			SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
			ret = -EPERM;
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_battery_log_info( struct file* fi_p, unsigned long arg )
{
	shbatt_batt_log_info_t bli;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_get_battery_log_info(&bli);

	if(copy_to_user((shbatt_batt_log_info_t*)arg,&bli,sizeof(shbatt_batt_log_info_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_post_battery_log_info( struct file* fi_p, unsigned long arg )
{
	shbatt_batt_log_info_t bli;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&bli,(shbatt_batt_log_info_t*)arg,sizeof(shbatt_batt_log_info_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_post_battery_log_info(bli);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_notify_charger_cable_status( struct file* fi_p, unsigned long arg )
{
	shbatt_cable_status_t cbs;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&cbs,(shbatt_cable_status_t*)arg,sizeof(shbatt_cable_status_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_notify_charger_cable_status(cbs);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_notify_battery_charging_status( struct file* fi_p, unsigned long arg )
{
	shbatt_chg_status_t cgs;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&cgs,(shbatt_chg_status_t*)arg,sizeof(shbatt_chg_status_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_notify_battery_charging_status(cgs);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_notify_battery_capacity( struct file* fi_p, unsigned long arg )
{
	int cap;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&cap,(int*)arg,sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_notify_battery_capacity(cap);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_notify_charging_state_machine_enable( struct file* fi_p, unsigned long arg )
{
	shbatt_boolean_t ena;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&ena,(shbatt_boolean_t*)arg,sizeof(shbatt_boolean_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_notify_charging_state_machine_enable(ena);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_notify_power_supply_changed( struct file* fi_p, unsigned long arg )
{
	shbatt_ps_category_t cat;

	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&cat,(shbatt_ps_category_t*)arg,sizeof(shbatt_ps_category_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		power_supply_changed(&shbatt_power_supplies[cat]);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_initialize( struct file* fi_p, unsigned long arg )
{
	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_api_initialize();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_exec_low_battery_check_sequence( struct file* fi_p, unsigned long arg )
{
	int evt;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&evt,(int*)arg,sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_exec_low_battery_check_sequence(evt);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_kernel_time( struct file* fi_p, unsigned long arg )
{
	int ret = 0;

	struct timeval tv;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	tv = ktime_to_timeval(alarm_get_elapsed_realtime());

	if(copy_to_user((struct timeval*)arg,&tv,sizeof(struct timeval)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

shbatt_result_t shbatt_api_read_adc_channel_no_conversion( shbatt_adc_t* adc_p )
{
	static	const shbatt_drv_adc_channel_t kernel_side_channel_ids[NUM_SHBATT_ADC_CHANNEL]=
		{										/*User side channel ids.*/
		SHBATT_DRV_ADC_CHANNEL_VCOIN,			/*SHBATT_ADC_CHANNEL_VCOIN*/
		SHBATT_DRV_ADC_CHANNEL_VBAT,			/*SHBATT_ADC_CHANNEL_VBAT*/
		SHBATT_DRV_ADC_CHANNEL_DCIN,			/*SHBATT_ADC_CHANNEL_DCIN*/
		SHBATT_DRV_ADC_CHANNEL_ICHG,			/*SHBATT_ADC_CHANNEL_ICHG*/
		SHBATT_DRV_ADC_CHANNEL_VPH_PWR,			/*SHBATT_ADC_CHANNEL_VPH_PWR*/
		SHBATT_DRV_ADC_CHANNEL_IBAT,			/*SHBATT_ADC_CHANNEL_IBAT*/
		SHBATT_DRV_ADC_CHANNEL_CAM_TEMP,		/*SHBATT_ADC_CHANNEL_CAM_TEMP*/
		SHBATT_DRV_ADC_CHANNEL_BAT_THERM,		/*SHBATT_ADC_CHANNEL_BAT_THERM*/
		SHBATT_DRV_ADC_CHANNEL_BAT_ID,			/*SHBATT_ADC_CHANNEL_BAT_ID*/
		SHBATT_DRV_ADC_CHANNEL_USBIN,			/*SHBATT_ADC_CHANNEL_USBIN*/
		SHBATT_DRV_ADC_CHANNEL_PMIC_TEMP,		/*SHBATT_ADC_CHANNEL_PMIC_TEMP*/
		SHBATT_DRV_ADC_CHANNEL_625MV,			/*SHBATT_ADC_CHANNEL_625MV*/
		SHBATT_DRV_ADC_CHANNEL_125V,			/*SHBATT_ADC_CHANNEL_125V*/
		SHBATT_DRV_ADC_CHANNEL_XO_THERM,		/*SHBATT_ADC_CHANNEL_CHG_TEMP*/ /* CHG_TEMP can't read. read XO_THERM */
		SHBATT_DRV_ADC_CHANNEL_XO_THERM,		/*SHBATT_ADC_CHANNEL_XO_THERM*/
		SHBATT_DRV_ADC_CHANNEL_PA_THERM,		/*SHBATT_ADC_CHANNEL_PA_THERM*/
		SHBATT_DRV_ADC_CHANNEL_USB_ID,			/*SHBATT_ADC_CHANNEL_USB_ID*/
		SHBATT_DRV_ADC_CHANNEL_BAT_THERM_NO_MOVING_AVERAGE,		/* SHBATT_ADC_CHANNEL_BAT_THERM_NO_MOVING_AVERAGE */
		SHBATT_DRV_ADC_CHANNEL_BACKLIGHT_TEMP,	/*SHBATT_ADC_CHANNEL_BACKLIGHT_TEMP*/
		};
	shbatt_drv_adc_channel_t kernel_side_channel_id = kernel_side_channel_ids[adc_p->channel];
	int rc, ret;
	int ibat_result, ibat_adc_result;
	struct pm8xxx_adc_chan_result adc_result = {0,};
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	static const int retry_count = 5;
	int i;
#ifdef CONFIG_SHTMP_SENSOR
	int apq_temp;
	shbatt_result_t apq_result;
#endif /*CONFIG_SHTMP_SENSOR*/

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[adc channel] %d \n",adc_p->channel);

	for(i = 0; i < retry_count; i++)
	{

		if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_CAM_TEMP)
		{
			rc = pm8xxx_adc_mpp_config_read(3, kernel_side_channel_id, &adc_result);
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BACKLIGHT_TEMP)
		{
#ifdef CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM
			rc = pm8xxx_adc_mpp_config_read(12, kernel_side_channel_id, &adc_result);
#else  /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
			SHBATT_ERROR("%s : no support channel id. SHBATT_DRV_ADC_CHANNEL_BACKLIGHT_TEMP\n",__FUNCTION__);
			rc = -EPERM;
			break;
#endif /* CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM */
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_ICHG)
		{
			l22_vreg_power_on(true);
			rc = pm8xxx_adc_mpp_config_read(9, kernel_side_channel_id, &adc_result);
			l22_vreg_power_on(false);
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_USB_ID)
		{
			rc = pm8xxx_adc_mpp_config_read(11, kernel_side_channel_id, &adc_result);
		}
		else if (kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_IBAT) {
	//		rc = pm8921_bms_get_battery_current(&ibat_result);
			rc = pm8921_bms_get_vsense_avg_read(&ibat_adc_result, &ibat_result);
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BAT_THERM_NO_MOVING_AVERAGE)
		{
			rc = pm8xxx_adc_read(SHBATT_DRV_ADC_CHANNEL_BAT_THERM, &adc_result);
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_XO_THERM)
		{
#ifdef CONFIG_SHTMP_SENSOR
			apq_result = shbatt_seq_get_apq_temperature(&apq_temp);
			if(apq_result == SHBATT_RESULT_SUCCESS)
			{
				rc = 0;
			}
			else
			{
				rc = -1;
			}
#else /*CONFIG_SHTMP_SENSOR*/
			rc = pm8xxx_adc_read(kernel_side_channel_id, &adc_result);
#endif /*CONFIG_SHTMP_SENSOR*/
		}
#ifdef CONFIG_SHTMP_SENSOR
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PA_THERM)
		{
			/* PA_THERM can't read */
			rc = 0;
		}
#endif /*CONFIG_SHTMP_SENSOR*/
		else
		{
			if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PMIC_TEMP)
			{
				ret = msm_xo_mode_vote(xo_handle, MSM_XO_MODE_ON);
				if(ret)
				{
					printk(KERN_ERR "%s msm_xo_mode_vote ON err=%d", __FUNCTION__, ret);
				}
				usleep(WAIT_TIME_AFTER_XO_VOTE_ON);
			}
			
			rc = pm8xxx_adc_read(kernel_side_channel_id, &adc_result);
			
			if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PMIC_TEMP)
			{
				ret = msm_xo_mode_vote(xo_handle, MSM_XO_MODE_OFF);
				if(ret)
				{
					printk(KERN_ERR "%s msm_xo_mode_vote OFF err=%d", __FUNCTION__, ret);
				}
			}
		}
		if(rc == 0)
		{
			break;
		}
	}
	do
	{
		if(rc != 0)
		{
			result = SHBATT_RESULT_FAIL;
			break;
		}
		
		if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_IBAT)
		{
			adc_p->physical = ibat_result;
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_XO_THERM)
		{
#ifdef CONFIG_SHTMP_SENSOR
			adc_p->adc_code		= 0;
			adc_p->measurement	= 0;
			adc_p->physical		= apq_temp;
#else /*CONFIG_SHTMP_SENSOR*/
			adc_p->adc_code		= adc_result.adc_code;
			adc_p->measurement	= adc_result.measurement;
			adc_p->physical		= adc_result.physical;
#endif /*CONFIG_SHTMP_SENSOR*/
		}
		else
		{
			adc_p->adc_code		= adc_result.adc_code;
			adc_p->measurement	= adc_result.measurement;
			adc_p->physical		= adc_result.physical;
		}
		

	/*convert to physical*/
		if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_CAM_TEMP || kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PA_THERM || kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BACKLIGHT_TEMP )
		{
			/*physical is degc.*/
			/*nothing todo.*/
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BAT_THERM)
		{
			if(shbatt_before_batt_therm == -255)
			{
				adc_p->physical = adc_result.physical;
				shbatt_before_batt_therm = adc_p->physical;
			}
			else
			{
				adc_p->physical = ((shbatt_before_batt_therm_coefficient * (int)shbatt_before_batt_therm) + (shbatt_now_batt_therm_coefficient * (int)adc_result.physical)) / (shbatt_before_batt_therm_coefficient + shbatt_now_batt_therm_coefficient);
				shbatt_before_batt_therm = adc_p->physical;
			}
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_BAT_THERM_NO_MOVING_AVERAGE)
		{
			adc_p->physical = adc_result.physical;
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PMIC_TEMP)
		{
			adc_p->physical = (int32_t)adc_result.physical / 1024;
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_XO_THERM)
		{
#ifdef CONFIG_SHTMP_SENSOR
			adc_p->physical = apq_temp;
#else /*CONFIG_SHTMP_SENSOR*/
			adc_p->physical = (int32_t)adc_result.physical / 1024;
#endif /*CONFIG_SHTMP_SENSOR*/
		}
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_ICHG)
		{
			adc_p->physical = (int32_t)adc_result.physical / 500;	/* 500 = physical / 1000 x2*/
		}
#ifdef CONFIG_SHTMP_SENSOR
		else if(kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_PA_THERM)
		{
			/* PA_THERM can't read */
			adc_p->adc_code		= 0;
			adc_p->measurement	= 0;
		}
#endif /*CONFIG_SHTMP_SENSOR*/
		else
		{
			adc_p->physical = (int32_t)adc_result.physical / 1000;	/* uv -> mv*/
		}

		if (kernel_side_channel_id == SHBATT_DRV_ADC_CHANNEL_IBAT) {
			/*adc_p->physical = (int32_t)ibat_result / 1000;*/			/* uv -> mv*/
			adc_p->physical = -(int32_t)ibat_result;				/* uv -> mv*/	/*negative value meaning discharge current in client.but negative value meaning charge current in return value.*/
		}
	}while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return result;
}

static int shbatt_drv_ioctl_cmd_read_adc_channel_no_conversion( struct file* fi_p, unsigned long arg )
{
	int result = 0;
	shbatt_adc_t adc;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	do
	{
		if(copy_from_user(&adc, (shbatt_adc_t*)arg, sizeof(adc)) != 0)
		{
			result = -EPERM;
			break;
		}

		shbatt_api_read_adc_channel_no_conversion(&adc);

		if(copy_to_user((shbatt_adc_t*)arg, &adc, sizeof(adc)) != 0)
		{
			result = -EPERM;
			break;
		}
	}while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}	

static int shbatt_drv_ioctl_cmd_get_hw_revision( struct file* fi_p, unsigned long arg )
{
	int result = 0;
	uint hw_rev;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_api_get_hw_revision( & hw_rev ))
		{
			result = -EPERM;
			break;
		}
		
		if(copy_to_user((int __user *)arg, & hw_rev, sizeof(uint)))
		{
			result = -EPERM;
			break;
		}
	}while(0);
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

static int shbatt_drv_ioctl_cmd_get_smem_info( struct file* fi_p, unsigned long arg )
{
	int result = 0;
	shbatt_smem_info_t smem_info;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_api_get_smem_info( & smem_info ))
		{
			result = -EPERM;
			break;
		}
		
		if(copy_to_user((int __user *)arg, & smem_info, sizeof(shbatt_smem_info_t)))
		{
			result = -EPERM;
			break;
		}
	}while(0);
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

static void shbatt_seq_low_battery_voltage_alarm_isr(struct notifier_block *nb, unsigned long status, void *unused)
{
	SHBATT_TRACE("[S] %s = %d\n",__FUNCTION__, (int)status);

	switch (status) {
	case 1:
		/* PM8XXX_BATT_ALARM_LOWER_COMPARATOR */
		SHBATT_TRACE("[P] %s shbatt_low_battery_interrupt=true \n",__FUNCTION__);
		shbatt_low_battery_interrupt = true;
		cancel_delayed_work(&lowbatt_shutdown_struct);
		schedule_delayed_work_on(0, &lowbatt_shutdown_struct, msecs_to_jiffies(60000));
		shbatt_seq_disable_battery_voltage_alarm_int();
		shbatt_api_exec_low_battery_check_sequence(SHBATT_LOW_BATTERY_EVENT_LOW_INTERRUPT);
		break;
	case 2:
		/* PM8XXX_BATT_ALARM_UPPER_COMPARATOR */
		break;
	default:
		break;
	};

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_fatal_battery_voltage_alarm_isr(struct notifier_block *nb, unsigned long status, void *unused)
{
	SHBATT_TRACE("[S] %s = %d\n",__FUNCTION__, (int)status);

	switch (status) {
	case 1:
		/* PM8XXX_BATT_ALARM_LOWER_COMPARATOR */
		shbatt_seq_disable_battery_voltage_alarm_int();
		shbatt_api_exec_low_battery_check_sequence(SHBATT_LOW_BATTERY_EVENT_FATAL_INTERRUPT);
		break;
	case 2:
		/* PM8XXX_BATT_ALARM_UPPER_COMPARATOR */
		break;
	default:
		break;
	};

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_charging_fatal_battery_voltage_alarm_isr(struct notifier_block *nb, unsigned long status, void *unused)
{
	SHBATT_TRACE("[S] %s = %d\n",__FUNCTION__, (int)status);

	switch (status) {
	case 1:
		/* PM8XXX_BATT_ALARM_LOWER_COMPARATOR */
		cancel_delayed_work(&lowbatt_shutdown_struct);
		schedule_delayed_work_on(0, &lowbatt_shutdown_struct, msecs_to_jiffies(60000));
		shbatt_seq_disable_battery_voltage_alarm_int();
		shbatt_api_exec_low_battery_check_sequence(SHBATT_LOW_BATTERY_EVENT_CHARGING_FATAL_INTERRUPT);
		break;
	case 2:
		/* PM8XXX_BATT_ALARM_UPPER_COMPARATOR */
		break;
	default:
		break;
	};
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static shbatt_result_t shbatt_seq_enable_battery_voltage_alarm_int( unsigned short max, unsigned short min, void* cb_p )
{
#ifdef CONFIG_BATTERY_SH
	int rc;
	
	SHBATT_TRACE("[S] %s ,max=%d,min=%d\n",__FUNCTION__,max,min);
	
	rc = pm8xxx_batt_alarm_threshold_set(PM8XXX_BATT_ALARM_LOWER_COMPARATOR, min);
	if (!rc) {
		rc = pm8xxx_batt_alarm_threshold_set(PM8XXX_BATT_ALARM_UPPER_COMPARATOR, max);
	}
	if (rc) {
		pr_err("%s: unable to set batt alarm threshold\n", __func__);
		return SHBATT_RESULT_FAIL;
	}
	
	rc |= pm8xxx_batt_alarm_unregister_notifier(&alarm_notifier);
	if (rc)
		pr_err("%s: unable to register alarm notifier\n", __func__);
	
	alarm_notifier.notifier_call = cb_p;
	rc = pm8xxx_batt_alarm_register_notifier(&alarm_notifier);
	if (rc) {
		pr_err("%s: unable to register alarm notifier\n", __func__);
		return SHBATT_RESULT_FAIL;
	}
	
	rc = pm8xxx_batt_alarm_enable(PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (!rc) {
		rc = pm8xxx_batt_alarm_enable(PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	}
	if (rc) {
		pr_err("%s: unable to set batt alarm state\n", __func__);
		return SHBATT_RESULT_FAIL;
	}
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return SHBATT_RESULT_SUCCESS;
#else
	unsigned char threshold;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	threshold = (min - 2000) / 50;

	twl_i2c_write_u8(0x0D,threshold,0x24);

	shbatt_cur_battery_voltage_alarm_cb = cb_p;

	twl_i2c_write_u8(0x0D,0xE1,0xCA);

	twl6030_interrupt_unmask(0x04,REG_INT_MSK_LINE_A);
	twl6030_interrupt_unmask(0x04,REG_INT_MSK_STS_A);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
#endif
}

static shbatt_result_t shbatt_seq_disable_battery_voltage_alarm_int( void )
{
#ifdef CONFIG_BATTERY_SH
	int rc;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (!rc)
		rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
		
	if (rc)
		pr_err("%s: unable to set batt alarm state\n", __func__);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return SHBATT_RESULT_SUCCESS;
#else
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	twl6030_interrupt_mask(0x04,REG_INT_MSK_LINE_A);
	twl6030_interrupt_mask(0x04,REG_INT_MSK_STS_A);

	twl_i2c_write_u8(0x0D,0xE0,0xCA);

	shbatt_cur_battery_voltage_alarm_cb = NULL;

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
#endif
}
static shbatt_result_t shbatt_seq_get_terminal_temperature( int* tmp_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_TERMINAL_TEMPERATURE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*tmp_p = shbatt_usse_pkt.prm.tmp;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_modem_temperature( int* tmp_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_MODEM_TEMPERATURE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*tmp_p = shbatt_usse_pkt.prm.tmp;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_get_fuelgauge_voltage( int* vol_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_FUELGAUGE_VOLTAGE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*vol_p = shbatt_usse_pkt.prm.vol;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_fuelgauge_device_id( unsigned int* dev_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_FUELGAUGE_DEVICE_ID;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*dev_p = shbatt_usse_pkt.prm.dev;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_get_fuelgauge_accumulate_current( shbatt_accumulate_current_t* acc_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_FUELGAUGE_ACCUMULATE_CURRENT;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*acc_p = shbatt_usse_pkt.prm.acc;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_clr_fuelgauge_accumulate_current( void )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_CLR_FUELGAUGE_ACCUMULATE_CURRENT;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_get_fuelgauge_current_ad( int* raw_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_FUELGAUGE_CURRENT_AD;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*raw_p = shbatt_usse_pkt.prm.raw;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_set_fuelgauge_mode( shbatt_fuelgauge_mode_t mode )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(pm8921_bms_enable(mode == SHBATT_FUELGAUGE_MODE_OPERATING)<0)
	{
		result = SHBATT_RESULT_FAIL;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_read_adc_channel_buffered( shbatt_adc_t* adc_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_READ_ADC_CHANNEL_BUFFERED;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.adc = *adc_p;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*adc_p = shbatt_usse_pkt.prm.adc;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_get_battery_log_info( shbatt_batt_log_info_t* bli_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_BATTERY_LOG_INFO;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*bli_p = shbatt_usse_pkt.prm.bli;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_notify_charger_cable_status( shbatt_cable_status_t cbs )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if((cbs == SHBATT_CABLE_STATUS_USB) || (cbs == SHBATT_CABLE_STATUS_AC))
	{
		if(shbatt_charge_check_flg == false)
		{
			shbatt_charge_check_flg = true;
#ifdef CONFIG_PM_SHBATT_WAKE_CTL_ENABLE
			SHBATT_WAKE_CTL(1);
#else  /* CONFIG_PM_SHBATT_WAKE_CTL_ENABLE */
			SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(1) not call shbatt_wake_lock_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wake_lock_num));
#endif /* CONFIG_PM_SHBATT_WAKE_CTL_ENABLE */
		}
	}
	else if(cbs == SHBATT_CABLE_STATUS_NONE)
	{
		if(shbatt_charge_check_flg == true)
		{
			shbatt_charge_check_flg = false;
#ifdef CONFIG_PM_SHBATT_WAKE_CTL_ENABLE
			SHBATT_WAKE_CTL(0);
#else  /* CONFIG_PM_SHBATT_WAKE_CTL_ENABLE */
			SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(0) not call shbatt_wake_lock_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wake_lock_num));
#endif /* CONFIG_PM_SHBATT_WAKE_CTL_ENABLE */
		}
	}

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_NOTIFY_CHARGER_CABLE_STATUS;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.cbs = cbs;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_notify_battery_charging_status( shbatt_chg_status_t cgs )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_NOTIFY_BATTERY_CHARGING_STATUS;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.cgs = cgs;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_notify_battery_capacity( int cap )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_NOTIFY_BATTERY_CAPACITY;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.cap = cap;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_notify_charging_state_machine_enable( shbatt_boolean_t ena )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_NOTIFY_CHARGING_STATE_MACHINE_ENABLE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.ena = ena;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_initialize( void )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_cur_bootinfo = shbatt_seq_get_bootinfo();

	shbatt_task_is_initialized = true;

	if(shbatt_cur_boot_time_cable_status != SHBATT_CABLE_STATUS_NONE)
	{
		shbatt_seq_notify_charger_cable_status(shbatt_cur_boot_time_cable_status);
	}

	if(shbatt_cur_boot_time_chg_status != SHBATT_CHG_STATUS_DISCHARGING)
	{
		shbatt_seq_notify_battery_charging_status(shbatt_cur_boot_time_chg_status);
	}

	if(shbatt_battery_log_cache_pre_initialize_count > 0)
	{
		shbatt_result_t result;
		int i;

		if(shbatt_battery_log_cache_pre_initialize_count > SHBATT_BATTERY_LOG_CACHE_PRE_INITIALIZE_MAX)
		{
			shbatt_battery_log_cache_pre_initialize_count = SHBATT_BATTERY_LOG_CACHE_PRE_INITIALIZE_MAX;
		}

		for(i=0;i<shbatt_battery_log_cache_pre_initialize_count;i++)
		{
			result = shbatt_seq_post_battery_log_info(shbatt_battery_log_cache_pre_initialize[i]);
			if(result != SHBATT_RESULT_SUCCESS)
			{
				SHBATT_ERROR("%s, post log fail, cnt=%d/%d, event=%d, ret=%d\n",__FUNCTION__, i, shbatt_battery_log_cache_pre_initialize_count, (int)shbatt_battery_log_cache_pre_initialize[i].event_num, (int)result);
			}
		}
		shbatt_battery_log_cache_pre_initialize_count = 0;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_exec_low_battery_check_sequence( int evt )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_EXEC_LOW_BATTERY_CHECK_SEQUENCE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.evt = evt;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_get_battery_safety( shbatt_safety_t* saf_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_BATTERY_SAFETY;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*saf_p = shbatt_usse_pkt.prm.saf;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_call_user_space_sequence_executor( void )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	INIT_COMPLETION(shbatt_usse_cmp);
	atomic_inc(&shbatt_usse_op_cnt);
	wake_up_interruptible(&shbatt_usse_wait);
	wait_for_completion_killable(&shbatt_usse_cmp);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return shbatt_usse_pkt.hdr.ret;
}
static int shbatt_seq_get_bootinfo( void )
{
#ifdef CONFIG_BATTERY_SH	/*TODO must implement.*/
	return 0;
#else	
	int boot_mode;

	sh_boot_get_bootinfo(0x1010, 4, &boot_mode);

	return boot_mode;
#endif
}

static shbatt_result_t shbatt_seq_get_fuelgauge_temperature( int* tmp_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_FUELGAUGE_TEMPERATURE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*tmp_p = shbatt_usse_pkt.prm.tmp;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}
static shbatt_result_t shbatt_seq_post_battery_log_info( shbatt_batt_log_info_t bli )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_POST_BATTERY_LOG_INFO;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.bli = bli;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static void shbatt_seq_fuelgauge_soc_poll_timer_expire_cb( struct alarm* alm_p )
{
	shbatt_soc_t soc;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	soc.type = SHBATT_TIMER_TYPE_0;
	soc.sleep = (alm_p->type == 2) ? SHBATT_TIMER_TYPE_WAKEUP : SHBATT_TIMER_TYPE_SLEEP;

	shbatt_api_exec_fuelgauge_soc_poll_sequence(soc);

	shbatt_last_timer_expire_time = ktime_to_timespec(alarm_get_elapsed_realtime());
	SHBATT_TRACE("[P] %s shbatt_last_timer_expire_time.tv_sec=%010lu shbatt_last_timer_expire_time.tv_nsec=%09lu \n",__FUNCTION__, shbatt_last_timer_expire_time.tv_sec, shbatt_last_timer_expire_time.tv_nsec);
	if(shbatt_timer_restarted == true)
	{
		shbatt_timer_restarted = false;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_fuelgauge_soc_poll_timer_expire_multi_cb( struct alarm* alm_p )
{
	shbatt_soc_t soc;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	soc.type = SHBATT_TIMER_TYPE_1;
	soc.sleep = (alm_p->type == 2) ? SHBATT_TIMER_TYPE_WAKEUP : SHBATT_TIMER_TYPE_SLEEP;

	shbatt_api_exec_fuelgauge_soc_poll_sequence(soc);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_low_battery_poll_timer_expire_cb( struct alarm* alm_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_api_exec_low_battery_check_sequence(SHBATT_LOW_BATTERY_EVENT_LOW_TIMER);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_fatal_battery_poll_timer_expire_cb( struct alarm* alm_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_api_exec_low_battery_check_sequence(SHBATT_LOW_BATTERY_EVENT_FATAL_TIMER);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_charging_fatal_battery_poll_timer_expire_cb( struct alarm* alm_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_api_exec_low_battery_check_sequence(SHBATT_LOW_BATTERY_EVENT_CHARGING_FATAL_TIMER);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_low_battery_shutdown_poll_timer_expire_cb( struct alarm* alm_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_api_exec_low_battery_check_sequence(SHBATT_LOW_BATTERY_EVENT_LOW_SHUTDOWN);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_overcurr_timer_expire_cb( struct alarm* alm_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_api_exec_overcurr_check_sequence();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_battery_present_poll_timer_expire_cb( struct alarm* alm_p )
{
	shbatt_timer_t* tim_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	tim_p = (shbatt_timer_t*)alm_p;

	shbatt_api_exec_battery_present_check_sequence(tim_p->prm);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void shbatt_seq_wireless_charge_poll_timer_expire_cb( struct alarm* alm_p )
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_api_notify_wireless_charge_timer_expire();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static shbatt_result_t shbatt_api_notify_wireless_charge_timer_expire( void )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_NOTIFY_WIRELESS_CHARGE_TIMER_EXPIRE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

static void shbatt_task_cmd_notify_wireless_charge_timer_expire( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_NOTIFY_WIRELESS_CHARGE_TIMER_EXPIRE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static shbatt_result_t shbatt_seq_exec_fuelgauge_soc_poll_sequence( shbatt_soc_t soc )
{
  shbatt_result_t result;

  SHBATT_TRACE("[S] %s \n",__FUNCTION__);

  shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_EXEC_FUELGAUGE_SOC_POLL_SEQUENCE;
  shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
  shbatt_usse_pkt.prm.soc = soc;

  result = shbatt_seq_call_user_space_sequence_executor();

  SHBATT_TRACE("[E] %s \n",__FUNCTION__);

  return result;
}

static shbatt_result_t shbatt_seq_exec_overcurr_check_sequence( void )
{
  shbatt_result_t result;

  SHBATT_TRACE("[S] %s \n",__FUNCTION__);

  shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_EXEC_OVERCURR_CHECK_SEQUENCE;
  shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
  shbatt_usse_pkt.prm.val = 0;

  result = shbatt_seq_call_user_space_sequence_executor();

  SHBATT_TRACE("[E] %s \n",__FUNCTION__);

  return result;
}

static shbatt_result_t shbatt_seq_exec_battery_present_check_sequence( int seq )
{
  shbatt_result_t result;

  SHBATT_TRACE("[S] %s \n",__FUNCTION__);

  shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_EXEC_BATTERY_PRESENT_CHECK_SEQUENCE;
  shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
  shbatt_usse_pkt.prm.seq = seq;

  result = shbatt_seq_call_user_space_sequence_executor();

  SHBATT_TRACE("[E] %s \n",__FUNCTION__);

  return result;
}

static shbatt_result_t shbatt_api_exec_fuelgauge_soc_poll_sequence( shbatt_soc_t soc )
{
	shbatt_packet_t* pkt_p;
	struct timespec ts;
	struct timeval now_time;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		SHBATT_TRACE("[P] %s timer restart \n",__FUNCTION__);
		now_time = ktime_to_timeval(alarm_get_elapsed_realtime());
		ts.tv_sec = now_time.tv_sec + 10;
		ts.tv_nsec = now_time.tv_usec * 1000;
		
		SHBATT_TRACE("[P] %s ts.tv_sec=%010lu now_time.ts.tv_nsec=%09lu \n",__FUNCTION__, ts.tv_sec, ts.tv_nsec);
		
		shbatt_tim_soc_poll.prm = 0;
		alarm_start_range(&shbatt_tim_soc_poll.alm,
							timespec_to_ktime(ts),
							timespec_to_ktime(ts));
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_EXEC_FUELGAUGE_SOC_POLL_SEQUENCE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.soc = soc;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

static shbatt_result_t shbatt_api_exec_overcurr_check_sequence( void )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_EXEC_OVERCURR_CHECK_SEQUENCE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.val = 0;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

static shbatt_result_t shbatt_api_exec_battery_present_check_sequence( int seq )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_EXEC_BATTERY_PRESENT_CHECK_SEQUENCE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.seq = seq;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}


static int shbatt_drv_register_irq( struct platform_device* dev_p )
{
#ifdef CONFIG_BATTERY_SH
	return 0;
#else
	shbatt_pm_device_info_t* di_p;

	int irq, ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	di_p = (shbatt_pm_device_info_t*)kzalloc(sizeof(shbatt_pm_device_info_t),GFP_KERNEL);

	if(di_p == NULL)
	{
		ret = -ENOMEM;
		SHBATT_ERROR("%s : memory allocation failed.\n",__FUNCTION__);
		goto register_irq_exit_0;
	}

	platform_set_drvdata(dev_p,di_p);

	irq = platform_get_irq(dev_p,0);

	ret = request_threaded_irq(irq,NULL,shbatt_pm_bat_vlow_interrupt,0,"twl_bci_vlow",di_p);

	if(ret)
	{
		SHBATT_ERROR("%s : could not request irq %d, status %d\n",__FUNCTION__,irq,ret);
		goto register_irq_exit_1;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;

register_irq_exit_1:

	kfree(di_p);

register_irq_exit_0:

	return ret;
#endif
}

static int shbatt_drv_create_ps_attrs( struct platform_device* dev_p )
{
	int idx, ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	for(idx = 0; idx < ARRAY_SIZE(shbatt_power_supplies); idx++)
	{
		shbatt_power_supplies[idx].properties = shbatt_ps_props;
		shbatt_power_supplies[idx].num_properties = ARRAY_SIZE(shbatt_ps_props);
		shbatt_power_supplies[idx].get_property = shbatt_drv_get_ps_property;
		shbatt_power_supplies[idx].set_property = shbatt_drv_set_ps_property;

		ret = power_supply_register(&dev_p->dev,&shbatt_power_supplies[idx]);

		if(ret != 0)
		{
			SHBATT_ERROR("%s : failed to power supply register.\n",__FUNCTION__);
			return -EPERM;
		}
	}

/*battery attributes.*/
	for(idx = 0; idx < ARRAY_SIZE(shbatt_battery_attributes); idx++)
	{
		struct power_supply		*psy  = &shbatt_power_supplies[SHBATT_PS_CATEGORY_BATTERY]; 
		struct device_attribute	*attr = &shbatt_battery_attributes[idx];
		ret = device_create_file(psy->dev,attr);

		if(ret != 0)
		{
			device_remove_file(psy->dev,attr);

			SHBATT_ERROR("%s : failed to %s property register.\n",__FUNCTION__,attr->attr.name);
			return -EPERM;
		}
	}

/*cable(ac,usb)attributes*/
	ret = device_create_file(shbatt_power_supplies[SHBATT_PS_CATEGORY_USB].dev,&shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE]);
	if(ret != 0)
	{
		device_remove_file(shbatt_power_supplies[SHBATT_PS_CATEGORY_USB].dev,&shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE]);

		SHBATT_ERROR("%s : failed to %s property register.\n",__FUNCTION__,shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE].attr.name);
		return -EPERM;
	}

	ret = device_create_file(shbatt_power_supplies[SHBATT_PS_CATEGORY_AC].dev,&shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE]);
	if(ret != 0)
	{
		device_remove_file(shbatt_power_supplies[SHBATT_PS_CATEGORY_AC].dev,&shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE]);

		SHBATT_ERROR("%s : failed to %s property register.\n",__FUNCTION__,shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE].attr.name);
		return -EPERM;
	}

	ret = device_create_file(shbatt_power_supplies[SHBATT_PS_CATEGORY_WIRELESS].dev,&shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE]);
	if(ret != 0)
	{
		device_remove_file(shbatt_power_supplies[SHBATT_PS_CATEGORY_WIRELESS].dev,&shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE]);

		SHBATT_ERROR("%s : failed to %s property register.\n",__FUNCTION__,shbatt_cable_status_attributes[SHBATT_PS_PROPERTY_ONLINE].attr.name);
		return -EPERM;
	}

/*fuel gauge attributes.*/
	for(idx = 0; idx < ARRAY_SIZE(shbatt_fuelgauge_attributes); idx++)
	{
		struct power_supply		*psy  = &shbatt_power_supplies[SHBATT_PS_CATEGORY_FUELGAUGE]; 
		struct device_attribute	*attr = &shbatt_fuelgauge_attributes[idx];
		ret = device_create_file(psy->dev,attr);

		if(ret != 0)
		{
			device_remove_file(psy->dev,attr);

			SHBATT_ERROR("%s : failed to %s property register.\n",__FUNCTION__,attr->attr.name);
			return -EPERM;
		}
	}
	ret = device_create_file(shbatt_power_supplies[SHBATT_PS_CATEGORY_FUELGAUGE].dev,&shbatt_battery_attributes[SHBATT_PS_PROPERTY_CAPACITY]);	/*for add capacity to fuelgauge power_supply.*/

	if(ret != 0)
	{
		device_remove_file(shbatt_power_supplies[SHBATT_PS_CATEGORY_FUELGAUGE].dev,&shbatt_battery_attributes[SHBATT_PS_PROPERTY_CAPACITY]);

		SHBATT_ERROR("%s : failed to %s property register.\n",__FUNCTION__,shbatt_battery_attributes[SHBATT_PS_PROPERTY_CAPACITY].attr.name);
		return -EPERM;
	}

/*pmic attributes.*/
	for(idx = 0; idx < ARRAY_SIZE(shbatt_pmic_attributes); idx++)
	{
		struct power_supply		*psy  = &shbatt_power_supplies[SHBATT_PS_CATEGORY_PMIC]; 
		struct device_attribute	*attr = &shbatt_pmic_attributes[idx];
		ret = device_create_file(psy->dev,attr);

		if(ret != 0)
		{
			device_remove_file(psy->dev,attr);

			SHBATT_ERROR("%s : failed to %s property register.\n",__FUNCTION__,attr->attr.name);
			return -EPERM;
		}
	}

/*adc attributes.*/
	for(idx = 0; idx < ARRAY_SIZE(shbatt_adc_attributes); idx++)
	{
		struct power_supply		*psy  = &shbatt_power_supplies[SHBATT_PS_CATEGORY_ADC]; 
		struct device_attribute	*attr = &shbatt_adc_attributes[idx];
		ret = device_create_file(psy->dev,attr);

		if(ret != 0)
		{
			device_remove_file(psy->dev,attr);

			SHBATT_ERROR("%s : failed to %s property register.\n",__FUNCTION__,attr->attr.name);
			return -EPERM;
		}
	}
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_get_ps_property( struct power_supply* psy_p, enum power_supply_property psp, union power_supply_propval* val_p )
{
	return -EINVAL;
}

static int shbatt_drv_set_ps_property( struct power_supply* psy_p, enum power_supply_property psp, const union power_supply_propval* val_p)
{
	return -EINVAL;
}

static ssize_t shbatt_drv_show_cable_status_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p )
{
	const ptrdiff_t property = attr_p - shbatt_cable_status_attributes;
	ssize_t size = -1;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	switch(property)
	{
		case SHBATT_PS_PROPERTY_ONLINE:
		{
			shbatt_cable_status_t cbs;

			if(shbatt_api_get_charger_cable_status(&cbs) == SHBATT_RESULT_SUCCESS)
			{
				int value;
				if(dev_p == shbatt_power_supplies[SHBATT_PS_CATEGORY_USB].dev)
				{
					value = (cbs == SHBATT_CABLE_STATUS_USB) ? 1 : 0;
					SHBATT_TRACE("/usb/online = %d\n",value);
				}
				else if(dev_p == shbatt_power_supplies[SHBATT_PS_CATEGORY_AC].dev)
				{
					value = (cbs == SHBATT_CABLE_STATUS_AC) ? 1 : 0;
					SHBATT_TRACE("/ac/online = %d\n",value);
				}
				else
				{
					value = 0;
				}
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",value);
			}
			if(dev_p == shbatt_power_supplies[SHBATT_PS_CATEGORY_WIRELESS].dev)
			{
				int wireless_value;
#if !defined(CONFIG_PM_WIRELESS_CHARGE)
				wireless_value = 0;
#else	/*wireless*/
				wireless_value = (shbatt_is_wireless_charging() && cbs == SHBATT_CABLE_STATUS_NONE) ? 1 : 0;
#endif
				SHBATT_TRACE("/wireless/online = %d\n",wireless_value);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",wireless_value);
			}
		}
		break;
		default:
			SHBATT_ERROR("[P] %s attr = %d\n",__FUNCTION__,property);
	}	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return size;
}
static ssize_t shbatt_drv_show_battery_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p )
{
	const ptrdiff_t property = attr_p - shbatt_battery_attributes;
	ssize_t size = -1;
	int value;
	static const char* status_text[] = 
	{
		"Unknown", "Charging", "Discharging", "Not charging", "Full" 
	};
	static const char* health_text[] = 
	{
		"Unknown", "Good", "Overheat", "Dead", "Over voltage", "Unspecified failure", "Cold"
	};
	static const char* safety_text[] = 
	{
		"Unknown", "Good", "Over charging"
	};

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	switch(property)
	{
		case SHBATT_PS_PROPERTY_STATUS:
		{
			shbatt_chg_status_t cgs = 0;
#if defined(CONFIG_PM_WIRELESS_CHARGE)	/*wireless*/
			if(shbatt_is_wireless_charging())
			{
				const char *status = status_text[SHBATT_CHG_STATUS_CHARGING];
				SHBATT_TRACE("/battery/status = %s\n", status);
				size = snprintf(buf_p, PAGE_SIZE, "%s\n", status);
				break;
			}
#endif
			if(shbatt_api_get_battery_charging_status(&cgs) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/battery/status = %s\n",status_text[cgs]);
				size = snprintf(buf_p, PAGE_SIZE, "%s\n",status_text[cgs]);
			}
		}
		break;
		case SHBATT_PS_PROPERTY_HEALTH:
		{
			shbatt_health_t hea;

			if(shbatt_api_get_battery_health(&hea) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/battery/health = %s\n",health_text[hea]);
				size = snprintf(buf_p, PAGE_SIZE, "%s\n",health_text[hea]);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_PRESENT:
		{
			shbatt_present_t pre;

			if(shbatt_api_get_battery_present(&pre) == SHBATT_RESULT_SUCCESS)
			{
				value = (pre == SHBATT_PRESENT_BATT_CONNECTED) ? 1 : 0;
				SHBATT_TRACE("/battery/present = %d\n",value);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",value);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_CAPACITY:
		{
			int cap;
			if(shbatt_api_get_battery_capacity(&cap) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/battery/capacity = %d\n",cap);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",cap);
			}
			else
			{
				cap = 1;
				SHBATT_TRACE("/battery/capacity = %d\n",cap);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",cap);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_BATT_VOL:
		{
			int vol = 0;
			if(shbatt_seq_get_battery_voltage( &vol ) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/battery/batt_vol = %d\n",vol);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",vol);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_BATT_TEMP:
		{
			int tmp;
			if(shbatt_api_get_battery_temperature(&tmp) == SHBATT_RESULT_SUCCESS)
			{
				tmp *= 10;
				SHBATT_TRACE("/battery/batt_temp = %d\n",tmp);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",tmp);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_TECHNOLOGY:
		{
			shbatt_technology_t tec;
			if(shbatt_api_get_battery_technology(&tec) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/battery/technology = %i\n",tec);
				if(tec == SHBATT_TECHNOLOGY_BATT_LIPO)
				{
					size = snprintf(buf_p, PAGE_SIZE, "%s\n","Li-ion Polymer");
				}else if(tec == SHBATT_TECHNOLOGY_BATT_LION)
				{
					size = snprintf(buf_p, PAGE_SIZE, "%s\n","Li-ion");
				}
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_CURRENT_NOW:
		{
			int cur;
			if(shbatt_get_battery_current(&cur) == SHBATT_RESULT_SUCCESS)
			{
				cur *= 1000;	/*to uv*/
				SHBATT_TRACE("/battery/current_now = %d\n", cur);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",cur);
			}
		}
		break;

		case SHBATT_PS_PROPERTY_VOLTAGE_NOW:
		{
			int vol;
			if(shbatt_seq_get_battery_voltage(&vol) == SHBATT_RESULT_SUCCESS)
			{
				vol *= 1000;	/*to uA*/
				SHBATT_TRACE("/battery/voltage_now = %d\n", vol);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",vol);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_TERMINAL_TEMP:
		{/*chg temp can't read.*/
			int pmic_temp;
			if(shbatt_seq_get_pmic_temperature(&pmic_temp) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/battery/terminal_temp = %d\n", pmic_temp);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",pmic_temp);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_CAMERA_TEMP:
		{
			int camera_temp;
			if(shbatt_seq_get_camera_temperature(&camera_temp) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/battery/camera_temp = %d\n", camera_temp);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",camera_temp);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_SAFETY:
		{
			shbatt_safety_t safety;
			if(shbatt_api_get_battery_safety(&safety) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/battery/safety = %s\n", safety_text[safety]);
				size = snprintf(buf_p, PAGE_SIZE, "%s\n",safety_text[safety]);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_VOLTAGE_MAX_DESIGN:
		{
			int vol;
			vol = SHBATT_PS_VALUE_FULL_VOLTAGE * 1000;
			SHBATT_TRACE("/battery/energy_full = %d\n",vol);
			size = snprintf(buf_p, PAGE_SIZE, "%d\n",vol);
		}
		break;
		
		case SHBATT_PS_PROPERTY_VOLTAGE_MIN_DESIGN:
		{
			int vol;
			vol = SHBATT_PS_VALUE_EMPTYE_VOLTAGE * 1000;
			SHBATT_TRACE("/battery/energy_full = %d\n",vol);
			size = snprintf(buf_p, PAGE_SIZE, "%d\n",vol);
		}
		break;
		
		case SHBATT_PS_PROPERTY_ENERGY_FULL:
		{
			int fcc;
			fcc = SHBATT_PS_VALUE_FULL_CAPACITY * 1000 * 1000;
			SHBATT_TRACE("/battery/energy_full = %d\n",fcc);
			size = snprintf(buf_p, PAGE_SIZE, "%d\n",fcc);
		}
		break;
		
		default:
			SHBATT_ERROR("[P] %s attr = %d\n",__FUNCTION__,property);
	}	

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return size;
}
static ssize_t shbatt_drv_show_fuelgauge_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p )
{
	const ptrdiff_t property = attr_p - shbatt_fuelgauge_attributes;
	ssize_t size = -1;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	switch(property)
	{
		case SHBATT_PS_PROPERTY_CURRENT:
		{
			int cur;
			if(shbatt_get_battery_current(&cur) == SHBATT_RESULT_SUCCESS)
			{
				cur *= 1000;	/*to uv*/
				SHBATT_TRACE("/shbatt_fuelgauge/current = %d\n", cur);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",cur);
			}
		}
		break;
		
		case SHBATT_PS_PROPERTY_ACCUMULATE_CURRENT:
		{
			shbatt_accumulate_current_t acc;
			if(shbatt_api_get_fuelgauge_accumulate_current(&acc) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/shbatt_fuelgauge/accumulate_current = %d\n",acc.cur);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",acc.cur);
			}
		}
		break;
		
		default:
			SHBATT_ERROR("[P] %s attr = %d\n",__FUNCTION__,property);
	}	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return size;
}
static ssize_t shbatt_drv_show_pmic_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p )
{
	const ptrdiff_t property = attr_p - shbatt_pmic_attributes;
	ssize_t size = -1;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	switch(property)
	{
		default:
			SHBATT_ERROR("[P] %s attr = %d\n",__FUNCTION__,property);
	}	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return size;
}
static ssize_t shbatt_drv_show_adc_property( struct device* dev_p, struct device_attribute* attr_p ,char* buf_p )
{
	const ptrdiff_t property = attr_p - shbatt_adc_attributes;
	ssize_t size = -1;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	switch(property)
	{
		case SHBATT_PS_PROPERTY_CPU_TEMP:
		{
#ifdef CONFIG_SHTMP_SENSOR
			int apq_temp;
			if(shbatt_seq_get_apq_temperature(&apq_temp) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/adc/cpu_temp = %d\n", apq_temp);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",apq_temp);
			}
#else /*CONFIG_SHTMP_SENSOR*/
			int xo_therm;
			if(shbatt_seq_get_xo_temperature(&xo_therm) == SHBATT_RESULT_SUCCESS)
			{
				SHBATT_TRACE("/adc/cpu_temp = %d\n", xo_therm);
				size = snprintf(buf_p, PAGE_SIZE, "%d\n",xo_therm);
			}
#endif /*CONFIG_SHTMP_SENSOR*/
		}
		break;
		
		default:
			SHBATT_ERROR("[P] %s attr = %d\n",__FUNCTION__,property);
	}	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return size;
}



static shbatt_result_t shbatt_get_battery_current(int* cur_p)
{
	shbatt_adc_t ibat;
	shbatt_result_t	result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s\n",__FUNCTION__);
	ibat.channel		= SHBATT_ADC_CHANNEL_IBAT;

	do
	{
		result = shbatt_api_read_adc_channel_no_conversion(&ibat);

		if(result != SHBATT_RESULT_SUCCESS)
		{
			break;
		}
		*cur_p = ibat.physical;
	}
	while(0);
	
	SHBATT_TRACE("[E] %s\n",__FUNCTION__);
	return result;
}

#if defined(CONFIG_PM_WIRELESS_CHARGE)	/*wireless*/
/****************************************************
		wireless charger support.
*****************************************************/
static int shbatt_drv_ioctl_cmd_get_wireless_disable_phase( struct file* fi_p, unsigned long arg )
{
	int result;
	SHBATT_TRACE("[S] %s\n",__FUNCTION__);
	do
	{
		shbatt_wireless_disable_state_t status;
		result = shbatt_get_wireless_disable_phase(&status);
		if(result != 0)
		{
			break;
		}
		
		result = copy_to_user((shbatt_wireless_disable_state_t __user *)arg, &status, sizeof(status));
	}
	while(0);
	SHBATT_TRACE("[E] %s\n",__FUNCTION__);
	return result;
}
static int shbatt_drv_ioctl_cmd_get_wireless_charging_status( struct file* fi_p, unsigned long arg )
{
	int result;
	SHBATT_TRACE("[S] %s\n",__FUNCTION__);
	do
	{
		shbatt_wireless_charging_state_t status;
		result = shbatt_get_wireless_charging_status(&status);
		if(result != 0)
		{
			break;
		}
		
		result = copy_to_user((shbatt_wireless_charging_state_t __user *)arg, &status, sizeof(status));
	}
	while(0);
	SHBATT_TRACE("[E] %s\n",__FUNCTION__);
	return result;
}


typedef void (*wireless_state_callback)(void);
static wireless_state_callback hi_callback;
static wireless_state_callback lo_callback;

/*******************************************
		for timer control.
*******************************************/

static void oneshot_timer(struct hrtimer *timer, unsigned long ms)
{
	unsigned long ns = ms * 1000 * 1000;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	hrtimer_start(timer, ktime_set(0, ns), HRTIMER_MODE_REL);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void cancel_timer(struct hrtimer *timer)
{
	int result;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = hrtimer_try_to_cancel(timer);
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

}

/********************************************
	wireless charger state machine
********************************************/
static struct hrtimer wireless_state_machine_timer;
#define PULSE_WIDTH	100
#define MARGIN		10
static void start_state_machine_timer(void)
{
	static const unsigned timed_out = PULSE_WIDTH + MARGIN;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	oneshot_timer(&wireless_state_machine_timer, timed_out);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

}

static void cancel_state_machine_timer(void)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	cancel_timer(&wireless_state_machine_timer);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

typedef struct wireless_state wireless_state_t;
typedef void (*enter_handler)(void);
typedef void (*leave_handler)(void);
typedef wireless_state_t* (*wireless_event_handler)(void);

struct wireless_state
{
	wireless_event_handler handle_rising_event;
	wireless_event_handler handle_falling_event;
	wireless_event_handler handle_timed_out_event;
	
	enter_handler enter;
	leave_handler leave;

	const shbatt_wireless_charging_state_t wireless_charging_state;

	const char* name;
	
};

/*control functions.*/
static void start_state_machine_timer(void);
static void cancel_state_machine_timer(void);
static void notify_charge_state_changed(void);

/*event handlers.*/
/* event handlers in not charging state.*/
static wireless_state_t* handle_falling_event_in_not_charging(void);

/*event handlers in waiting for rising state.*/
static wireless_state_t* handle_rising_event_in_waiting_for_rising(void);
static wireless_state_t* handle_timed_out_event_in_waiting_for_rising(void);

/*event handlers in waiting for falling state.*/
static wireless_state_t* handle_falling_event_in_waiting_for_falling(void);
static wireless_state_t* handle_timed_out_event_in_waiting_for_falling(void);

/*event handlers in charging state.*/
static wireless_state_t* handle_rising_event_in_charging(void);

/*event handlers in waiting for pulse state.*/
static wireless_state_t* handle_falling_event_in_waiting_for_pulse(void);
static wireless_state_t* handle_timed_out_event_in_waiting_for_pulse(void);

/*event handlers in waiting for full charge.*/
static wireless_state_t* handle_rising_event_in_waiting_for_full_charge(void);


static wireless_state_t unknown_state					=	{	
																.wireless_charging_state = WIRELESS_NOT_CHARGING,
																.name = "unknown_state",
															};

static wireless_state_t not_charging_state				=	{	
																
																.handle_falling_event = handle_falling_event_in_not_charging,

																.wireless_charging_state = WIRELESS_NOT_CHARGING,
																.name = "not_charging_state",
															};									

static wireless_state_t waiting_for_rising_state		=	{	
																.handle_rising_event = handle_rising_event_in_waiting_for_rising,
																.handle_timed_out_event = handle_timed_out_event_in_waiting_for_rising,
																.enter = start_state_machine_timer,
																.leave = cancel_state_machine_timer, 
																
																.wireless_charging_state = WIRELESS_NOT_CHARGING,
																.name = "waiting_for_rising_state",
															};
															
static wireless_state_t waiting_for_falling_state		=	{	
																.handle_falling_event = handle_falling_event_in_waiting_for_falling,		/*need notify.*/
																.handle_timed_out_event = handle_timed_out_event_in_waiting_for_falling,
																.enter = start_state_machine_timer,
																.leave = cancel_state_machine_timer,
																
																.wireless_charging_state = WIRELESS_NOT_CHARGING,
																.name = "waiting_for_falling_state",
															};

static wireless_state_t charging_state					=	{	
																.handle_rising_event = handle_rising_event_in_charging,
																
																.wireless_charging_state = WIRELESS_CHARGING,
																.name = "charging_state",
															};
															
static wireless_state_t waiting_for_pulse_state			=	{	
																.handle_falling_event = handle_falling_event_in_waiting_for_pulse,			/*need notify.*/
																.handle_timed_out_event = handle_timed_out_event_in_waiting_for_pulse,		/*need notify.*/
																.enter = start_state_machine_timer,
																.leave = cancel_state_machine_timer,
																
																.wireless_charging_state = WIRELESS_CHARGING,
																.name = "waiting_for_pulse_state",
															};
															
static wireless_state_t waiting_for_full_charge_state	=	{
																.handle_rising_event = handle_rising_event_in_waiting_for_full_charge,		/*need notify.*/
																
																.wireless_charging_state = WIRELESS_PRE_FULL_CHARGE,
																.name = "waiting_for_full_charge_state",
															};

static wireless_state_t* current_state = &unknown_state;

/*******************************************
		event handlers.
*******************************************/
/*not_charging_state.*/
static wireless_state_t* handle_falling_event_in_not_charging(void)
{
	wireless_state_t* next_state = &waiting_for_rising_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return next_state;
}
/*waiting_for_rising_state.*/
static wireless_state_t* handle_rising_event_in_waiting_for_rising(void)
{
	wireless_state_t* next_state = &waiting_for_falling_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return next_state;
}
static wireless_state_t* handle_timed_out_event_in_waiting_for_rising(void)
{
	wireless_state_t* next_state = &charging_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return next_state;
}
/*waiting_for_falling_state.*/
static wireless_state_t* handle_falling_event_in_waiting_for_falling(void)
{
	wireless_state_t* next_state = &charging_state;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_state;
}
static wireless_state_t* handle_timed_out_event_in_waiting_for_falling(void)
{

	wireless_state_t* next_state = &not_charging_state;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_state;
}
/*charging_state.*/
static wireless_state_t* handle_rising_event_in_charging(void)
{
	wireless_state_t* next_state = &waiting_for_pulse_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_state;
}
/*waiting_for_pulse_state.*/
static wireless_state_t* handle_falling_event_in_waiting_for_pulse(void)
{
	wireless_state_t* next_state = &waiting_for_full_charge_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return next_state;
}
static wireless_state_t* handle_timed_out_event_in_waiting_for_pulse(void)
{
	wireless_state_t* next_state = &not_charging_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return next_state;
}
/*waiting_for_full_charge_state.*/
static wireless_state_t* handle_rising_event_in_waiting_for_full_charge(void)
{
	wireless_state_t* next_state = &not_charging_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_state;
}

static void set_state(wireless_state_t* new_state)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);	
	if(current_state != new_state)
	{
		int charging_state_changed = new_state->wireless_charging_state != current_state->wireless_charging_state;

		if(current_state->leave)
			current_state->leave();
		if(new_state->enter)
			new_state->enter();
		printk(KERN_DEBUG "[STATE]%s -> %s\n", current_state->name, new_state->name);
		
		current_state = new_state;
		
		if(charging_state_changed)
		{
			notify_charge_state_changed();
		}
		
	}
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

}

static void handle_rising_event(void)
{
	wireless_state_t* next_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(!current_state->handle_rising_event)
		{
			printk(KERN_ERR "%s can't handle rising event\n", current_state->name);
			break;
		}
		next_state = current_state->handle_rising_event();
		set_state(next_state);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

}

static void handle_falling_event(void)
{
	wireless_state_t* next_state;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(!current_state->handle_falling_event)
		{
			printk(KERN_ERR "%s can't handle falling event\n", current_state->name);
			break;
		}
		next_state = current_state->handle_falling_event();
		set_state(next_state);
	}
	while(0);
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void handle_timed_out_event(void)
{
	wireless_state_t* next_state;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(!current_state->handle_timed_out_event)
		{
			printk(KERN_ERR "%s can't handle timed out event\n", current_state->name);
			break;
		}
		next_state = current_state->handle_timed_out_event();
		set_state(next_state);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

/*******************************************
		handle interrups.
********************************************/
static void rising_event_tasklet_callback(unsigned long data);
static void falling_event_tasklet_callback(unsigned long data);
int wireless_state_irq_no;

DECLARE_TASKLET( rising_event_tasklet,		rising_event_tasklet_callback,  0);
DECLARE_TASKLET( falling_event_tasklet,		falling_event_tasklet_callback,  0);

static irqreturn_t handle_wireless_state_irq(int irq_no, void* param)
{
	int is_lo;
	unsigned int irq_type;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	is_lo = GPIO_GET(SHBATT_GPIO_NUM_WIRELESS_STATUS) == 0;

	irq_type = is_lo ? IRQ_TYPE_LEVEL_HIGH : IRQ_TYPE_LEVEL_LOW;
	
	irq_set_irq_type(wireless_state_irq_no, irq_type);


	if(is_lo)
	{
		if(lo_callback)
			lo_callback();
			
		tasklet_hi_schedule(&falling_event_tasklet);
	}
	else
	{
		if(hi_callback)
			hi_callback();
			
		tasklet_hi_schedule(&rising_event_tasklet);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return IRQ_HANDLED;
}

static enum hrtimer_restart handle_timed_out(struct hrtimer *timer)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	handle_timed_out_event();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return HRTIMER_NORESTART;
}
static void rising_event_tasklet_callback(unsigned long data)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	handle_rising_event();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void falling_event_tasklet_callback(unsigned long data)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	handle_falling_event();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static int shbatt_is_wireless_charging(void)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);	
	return		current_state->wireless_charging_state == WIRELESS_CHARGING
			||	current_state->wireless_charging_state == WIRELESS_PRE_FULL_CHARGE;
}	


/*******************************************
	disable/enable wireless charger.
*******************************************/

/*for disable/enable wireless and check firmware is alive.*/
struct hrtimer wireless_charger_disable_timer;


static void bind_hi_callback(wireless_state_callback callback);
static void bind_lo_callback(wireless_state_callback callback);
inline static void unbind_hi_callback(void);
inline static void unbind_lo_callback(void);
static void handle_lo_callback(void);
static void handle_hi_callback(void);

typedef struct wireless_charger_disable_phase_tag wireless_charger_disable_phase_t;
typedef wireless_charger_disable_phase_t* (*wireless_charger_disable_handler)(void);

struct wireless_charger_disable_phase_tag
{
	wireless_charger_disable_handler hi_event_handler;
	wireless_charger_disable_handler lo_event_handler;
	wireless_charger_disable_handler timed_out_event_handler;
	
	enter_handler	enter;
	leave_handler	leave;
	const shbatt_wireless_disable_state_t	wireless_disable_phase_state;
	const char* name;
};


static void do_enable_wireless_charger(void)
{
	GPIO_SET(SHBATT_GPIO_NUM_WIRELESS_SWITCH, 0);
}

static void do_disable_wireless_charger(void)
{
	GPIO_SET(SHBATT_GPIO_NUM_WIRELESS_SWITCH, 1);
}

static void enter_to_waiting_for_firmware_phase(void);
static wireless_charger_disable_phase_t* handle_timed_out_in_wireless_charger_waiting_for_firmware_phase(void);
static void leave_from_waiting_for_firmware_phase(void);

static void enter_to_waiting_for_lo_phase(void);
static wireless_charger_disable_phase_t* handle_lo_in_waiting_for_lo_phase(void);
static wireless_charger_disable_phase_t* handle_timed_out_in_waiting_for_lo(void);
static void leave_from_waiting_for_lo_phase(void);

static void enter_to_waiting_for_hi_phase(void);
static wireless_charger_disable_phase_t* handle_hi_in_waiting_for_hi_phase(void);
static wireless_charger_disable_phase_t* handle_timed_out_in_waiting_for_hi(void);
static void leave_from_waiting_for_hi_phase(void);

static wireless_charger_disable_phase_t wireless_charger_unknown_phase = 
{
	.wireless_disable_phase_state = WIRELESS_UNKNOWN,
	.name = "wireless_charger_unknown_phase",
};
static wireless_charger_disable_phase_t wireless_charger_enabled_phase = 	/*wireless_switch low.*/
{
	.enter = do_enable_wireless_charger,
	.wireless_disable_phase_state = WIRELESS_ENABLED,
	.name = "wireless_charger_enabled_phase",
};
static wireless_charger_disable_phase_t wireless_charger_waiting_for_firmware_phase =	/*wireless_switch high.*/
{
	.timed_out_event_handler = handle_timed_out_in_wireless_charger_waiting_for_firmware_phase,
	
	.enter = enter_to_waiting_for_firmware_phase,
	.leave = leave_from_waiting_for_firmware_phase,
	.wireless_disable_phase_state = WIRELESS_BUSY,
	.name = "wireless_charger_waiting_for_firmware_phase",
};
static wireless_charger_disable_phase_t wireless_charger_waiting_for_lo_phase =	/*wireless_switch low.*/
{
	.lo_event_handler = handle_lo_in_waiting_for_lo_phase,
	.timed_out_event_handler = handle_timed_out_in_waiting_for_lo,
	
	.enter = enter_to_waiting_for_lo_phase,
	.leave = leave_from_waiting_for_lo_phase,
	.wireless_disable_phase_state = WIRELESS_BUSY,
	.name = "wireless_charger_waiting_for_lo_phase",
};
static wireless_charger_disable_phase_t wireless_charger_waiting_for_hi_phase = 	/*wireless_switch low.*/
{
	.hi_event_handler = handle_hi_in_waiting_for_hi_phase,
	.timed_out_event_handler = handle_timed_out_in_waiting_for_hi,
	
	.enter = enter_to_waiting_for_hi_phase,
	.leave = leave_from_waiting_for_hi_phase,
	.wireless_disable_phase_state = WIRELESS_BUSY,
	.name = "wireless_charger_waiting_for_hi_phase",
};
static wireless_charger_disable_phase_t wireless_charger_abnormal_phase = 	/*wireless_switch high.*/
{
	.enter = do_disable_wireless_charger,
	
	.wireless_disable_phase_state = WIRELESS_FIRMWARE_IS_DEAD,
	.name = "wireless_charger_abnormal_phase",
};
static wireless_charger_disable_phase_t wireless_charger_disable_phase = 	/*wireless_switch high.*/
{
	.enter = do_disable_wireless_charger,
	
	.wireless_disable_phase_state = WIRELESS_DISABLED,
	.name = "wireless_charger_disable_phase",
};

/*waiting for firmware phase.*/
#define FIRMWARE_POLLING_PERIOD 500
#define FIRMWARE_POLLING_MARGIN 100
static void enter_to_waiting_for_firmware_phase(void)
{
	static const unsigned timer_period = FIRMWARE_POLLING_PERIOD + FIRMWARE_POLLING_MARGIN;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do_disable_wireless_charger();
	oneshot_timer(&wireless_charger_disable_timer, timer_period);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static wireless_charger_disable_phase_t* handle_timed_out_in_wireless_charger_waiting_for_firmware_phase(void)
{
	wireless_charger_disable_phase_t* next_phase = &wireless_charger_waiting_for_lo_phase;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_phase;
}
static void leave_from_waiting_for_firmware_phase(void)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	cancel_timer(&wireless_charger_disable_timer);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}


/*waiting for lo phase.*/
#define FIRMWARE_SPECIFIC_LO_WIDTH 700
#define ABNORMAL_TIMED_OUT_MARGIN 100
static void enter_to_waiting_for_lo_phase(void)
{
	static const unsigned timer_period = FIRMWARE_POLLING_PERIOD + FIRMWARE_SPECIFIC_LO_WIDTH - (FIRMWARE_POLLING_PERIOD + FIRMWARE_POLLING_MARGIN) + ABNORMAL_TIMED_OUT_MARGIN;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	bind_lo_callback(handle_lo_callback);

	oneshot_timer(&wireless_charger_disable_timer, timer_period);
	do_enable_wireless_charger();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static wireless_charger_disable_phase_t* handle_lo_in_waiting_for_lo_phase(void)
{
	wireless_charger_disable_phase_t* next_phase = &wireless_charger_waiting_for_hi_phase;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_phase;
}
static wireless_charger_disable_phase_t* handle_timed_out_in_waiting_for_lo(void)
{
	wireless_charger_disable_phase_t* next_phase = &wireless_charger_abnormal_phase;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_phase;
}
static void leave_from_waiting_for_lo_phase(void)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	cancel_timer(&wireless_charger_disable_timer);
	unbind_lo_callback();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
/*waiting for hi phase.*/
static void enter_to_waiting_for_hi_phase(void)
{
	static const unsigned timer_period = PULSE_WIDTH + MARGIN;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	bind_hi_callback(handle_hi_callback);
	oneshot_timer(&wireless_charger_disable_timer, timer_period);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static wireless_charger_disable_phase_t* handle_hi_in_waiting_for_hi_phase(void)
{
	wireless_charger_disable_phase_t* next_phase = &wireless_charger_disable_phase;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_phase;
}
static wireless_charger_disable_phase_t* handle_timed_out_in_waiting_for_hi(void)
{
	wireless_charger_disable_phase_t* next_phase = &wireless_charger_abnormal_phase;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return next_phase;
}
static void leave_from_waiting_for_hi_phase(void)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	cancel_timer(&wireless_charger_disable_timer);
	unbind_hi_callback();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}


static wireless_charger_disable_phase_t* current_phase = &wireless_charger_unknown_phase;

static void set_new_phase(wireless_charger_disable_phase_t* new_phase)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	if(current_phase != new_phase)
	{
		if(current_phase->leave)
			current_phase->leave();
		if(new_phase->enter)
			new_phase->enter();
			
		printk(KERN_DEBUG "[PHASE]%s -> %s\n", current_phase->name, new_phase->name);
			
		current_phase = new_phase;
	}
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void bind_hi_callback(wireless_state_callback callback)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	hi_callback = callback;
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
inline static void unbind_hi_callback(void)
{
	bind_hi_callback(NULL);
}
static void bind_lo_callback(wireless_state_callback callback)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	lo_callback = callback;
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
inline static void unbind_lo_callback(void)
{
	bind_lo_callback(NULL);
}

static void handle_hi_event(void)
{
	wireless_charger_disable_phase_t* next_phase;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(!current_phase->hi_event_handler)
		{
			printk(KERN_ERR "%s can't handle hi event\n", current_phase->name);
			break;
		}
		next_phase = current_phase->hi_event_handler();
		set_new_phase(next_phase);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static void handle_lo_event(void)
{
	wireless_charger_disable_phase_t* next_phase;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(!current_phase->lo_event_handler)
		{
			printk(KERN_ERR "%s can't handle lo event\n", current_phase->name);
			break;
		}
		next_phase = current_phase->lo_event_handler();
		set_new_phase(next_phase);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
static void handle_to_event(void)
{
	wireless_charger_disable_phase_t* next_phase;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(!current_phase->timed_out_event_handler)
		{
			printk(KERN_ERR "%s can't handle to event\n", current_phase->name);
			break;
		}
		next_phase = current_phase->timed_out_event_handler();
		set_new_phase(next_phase);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

void handle_hi_callback(void)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	handle_hi_event();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static void handle_lo_callback(void)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	handle_lo_event();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}


static enum hrtimer_restart handle_phase_timed_out(struct hrtimer *timer)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	handle_to_event();
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return HRTIMER_NORESTART;
}

/*******************************************
	APIs.
*******************************************/

static shbatt_result_t shbatt_disable_wireless_charger(void)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
		if(atomic_inc_return(&wlchg_disable_charge_count) != 1)	/* not 0->1*/
		{
			break;
		}
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */

		if(!shbatt_is_wireless_charging())
		{
			set_new_phase(&wireless_charger_disable_phase);
			result = SHBATT_RESULT_FAIL;
			break;
		}
		
		if(current_phase != &wireless_charger_enabled_phase)
		{
			result = SHBATT_RESULT_FAIL;
			break;
		}
		
		set_new_phase(&wireless_charger_waiting_for_firmware_phase);
	}
	while(0);
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	return result;
}

static shbatt_result_t shbatt_enable_wireless_charger(void)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
		if(atomic_dec_return(&wlchg_disable_charge_count) != 0)	/* not 1->0*/
		{
			break;
		}
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */
		set_new_phase(&wireless_charger_enabled_phase);
	}
	while(0);
	return result;
}

static shbatt_result_t shbatt_get_wireless_disable_phase(shbatt_wireless_disable_state_t* state)
{
	*state = current_phase->wireless_disable_phase_state;
	return SHBATT_RESULT_SUCCESS;
}


static shbatt_result_t shbatt_get_wireless_charging_status(shbatt_wireless_charging_state_t* state)
{
	*state = current_state->wireless_charging_state;
	return SHBATT_RESULT_SUCCESS;
}

static void shbatt_task_cmd_notify_wireless_charger_state_changed(shbatt_packet_t* pkt_p)
{
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_NOTIFY_WIRELESS_CHARGER_STATE_CHANGED;

	shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

}
static shbatt_result_t  notify_wireless_charge_state_changed(void)
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result =  SHBATT_RESULT_REJECTED;
			break;
		}

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd = SHBATT_TASK_CMD_NOTIFY_WIRELESS_CHARGER_STATE_CHANGED;
		pkt_p->hdr.cb_p = NULL;
		pkt_p->hdr.cmp_p = NULL;
		pkt_p->hdr.ret_p = NULL;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);
	}
	while(0);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

#define SHBATT_WIRELESS_WAKELOCK_TIMEOUT	1*HZ/2

static void notify_charge_state_changed(void)
{
	shbatt_chg_status_t cgs;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	cgs = shbatt_is_wireless_charging() ? SHBATT_CHG_STATUS_CHARGING : SHBATT_CHG_STATUS_DISCHARGING;
	shbatt_api_notify_battery_charging_status(cgs);
	notify_wireless_charge_state_changed();

	if(!shbatt_is_wireless_charging())
	{
		if (!wake_lock_active(&shbatt_wireless_wake_lock))
		{
			wake_lock_timeout(&shbatt_wireless_wake_lock, SHBATT_WIRELESS_WAKELOCK_TIMEOUT);
		}
		else
		{
			wake_unlock(&shbatt_wireless_wake_lock);
			wake_lock_timeout(&shbatt_wireless_wake_lock, SHBATT_WIRELESS_WAKELOCK_TIMEOUT);
		}
	}

	printk(KERN_DEBUG"notify_charge_state_changed\n");
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}


static void initialize_wireless_charger(void)
{
	wireless_state_t* initial_state = &unknown_state;
	static const int hardware_led_control_disable = 1;
	int charging;
	int irq_detect_mode;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		GPIO_SET(SHBATT_GPIO_NUM_LED_SHUT, hardware_led_control_disable);

		/*for disable wireless charger.*/
		set_new_phase(&wireless_charger_enabled_phase);
		hrtimer_init(&wireless_charger_disable_timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
		wireless_charger_disable_timer.function = handle_phase_timed_out;
		
		charging = GPIO_GET(SHBATT_GPIO_NUM_WIRELESS_STATUS) == 0;
		irq_detect_mode = charging ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;

		wireless_state_irq_no = gpio_to_irq(SHBATT_GPIO_NUM_WIRELESS_STATUS);

		initial_state = charging ? &charging_state : &not_charging_state;
		set_state(initial_state);

		hrtimer_init(&wireless_state_machine_timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
		wireless_state_machine_timer.function = handle_timed_out;

		enable_irq_wake(wireless_state_irq_no);

		if(request_irq(wireless_state_irq_no, handle_wireless_state_irq, irq_detect_mode, "wireless state", NULL) != 0)
		{
			SHBATT_ERROR("request irq failed. at %s.\n", __FUNCTION__);
			break;
		}
	}
	while(0);
	
#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C
	do
	{
		int is_product_flg_on = 0;
		int result;
		result = wlchg_get_product_flg(&is_product_flg_on);
		if(result != 0)
		{
			SHBATT_ERROR("get product flg failed. at %s.\n", __FUNCTION__);
			break;
		}
		if(!is_product_flg_on)
		{
			uint8_t current_fw_rev = 0;
			uint8_t new_fw_rev = 0xFF;
			const struct firmware *fw;
			int is_ic_power_off = GPIO_GET(SHBATT_GPIO_NUM_WIRELESS_STATUS) != 0;
			if(is_ic_power_off)
			{
				wlchg_ic_power_on();
			}
			result = wlchg_get_revision_from_ic(&current_fw_rev);
			if(is_ic_power_off)
			{
				wlchg_ic_power_off();
			}
			
			if(result != 0)
			{
				SHBATT_ERROR(" wlchg_get_revision_from_ic() failed.result=%d\n", result);	/* maybe i2c access failed.*/
				break;
			}
			
			result = request_firmware(&fw, WLCHG_FIRMWARE_NAME, shbatt_dev_p);
			if(result != 0)
			{
				SHBATT_ERROR("wlchg request_firmware() failed.result=%d\n", result);
				break;
			}
			
			result = wlchg_get_revision_from_fw(fw, &new_fw_rev);
			if(result != 0)
			{
				SHBATT_ERROR("wlchg_get_revision_from_fw() failed.result=%d\n", result);
				release_firmware(fw);
				break;
			}

			printk(KERN_ERR"wlchg fw current_revision=%02x", current_fw_rev);
			if(new_fw_rev > current_fw_rev)
			{
				printk(KERN_INFO"wlchg fw rewrite,new_revision=%02x,current_revision=%02x", new_fw_rev, current_fw_rev);
				wlchg_fw_rewrite_workque = create_singlethread_workqueue("wlchg_fw_rewrite_workque");

				INIT_WORK(&wlchg_fw_rewrite_work.work_struct_header, wlchg_fw_rewrite_task);
				wlchg_fw_rewrite_work.fw = fw;
				queue_work(wlchg_fw_rewrite_workque, &wlchg_fw_rewrite_work.work_struct_header);
			}
			else
			{
				release_firmware(fw);
			}
			
		}
	}while(0);
#endif
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}
#else	/*wireless charger not supported.*/
static void initialize_wireless_charger(void)
{
	/*nothing todo.*/
}
static void shbatt_task_cmd_notify_wireless_charger_state_changed(shbatt_packet_t* pkt_p)
{
	/*this function is not called.but symbol is used.*/
}


#endif

#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
static shbatt_result_t hdq_initialize(void)
{
	shbatt_result_t result;
	unsigned long flags;

	SHBATT_TRACE("[S] %s\n", __FUNCTION__);

	spin_lock_irqsave(&shbatt_pkt_lock,flags);
	result = hdq_initialize_in_lock();
	spin_unlock_irqrestore(&shbatt_pkt_lock,flags);

	
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t hdq_initialize_in_lock(void)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	int ret;
	static struct pm_gpio gpio_cfg = {
		.direction        = PM_GPIO_DIR_OUT,
		.output_buffer    = PM_GPIO_OUT_BUF_CMOS,
		.output_value     = 0,
		.pull             = PM_GPIO_PULL_NO,
		.vin_sel          = PM_GPIO_VIN_L17,
		.out_strength     = PM_GPIO_STRENGTH_MED,
		.function         = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol      = 0,
		.disable_pin      = 0,
	};
	
	SHBATT_TRACE("[S] %s port=%d\n", __FUNCTION__, HDQ_GPIO_PORT_NUM);
	
	if( shbatt_hdq_initial_state == HDQ_INITIALIZED )
	{
		SHBATT_TRACE("[P] %s, already initialized, so skip.\n", __FUNCTION__);
		goto error_no_free;
	}
	
	shbatt_hdq_initial_state = HDQ_INITIALIZING;
	
	ret = gpio_request(HDQ_GPIO_PORT_NUM, "hdq");
	if(ret){
		result = SHBATT_RESULT_FAIL;
		shbatt_hdq_initial_state = HDQ_INITIAL_FAIL;
		SHBATT_TRACE("%s %d ret=%d", __FUNCTION__, __LINE__, ret);
		goto error_no_free;
	}

	ret = pm8xxx_gpio_config(HDQ_GPIO_PORT_NUM, &gpio_cfg);
	if(ret){
		result = SHBATT_RESULT_FAIL;
		shbatt_hdq_initial_state = HDQ_INITIAL_FAIL;
		SHBATT_TRACE("%s %d ret=%d", __FUNCTION__, __LINE__, ret);
		goto error;
	}
	
	result = sleep_wakeup();
	if(result != SHBATT_RESULT_SUCCESS){
		result = SHBATT_RESULT_FAIL;
		shbatt_hdq_initial_state = HDQ_INITIAL_FAIL;
		SHBATT_TRACE("%s %d ret=%d\n", __FUNCTION__, __LINE__, result);
		goto error;
	}
	
	shbatt_hdq_initial_state = HDQ_INITIALIZED;
	
error:
	gpio_free(HDQ_GPIO_PORT_NUM);
	
error_no_free:
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t hdq_read_byte(const unsigned char address, unsigned char* read_byte)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	unsigned char send_command = address;
	unsigned long flags;
	int ret;
	
	SHBATT_TRACE("[S] %s 0x%02x, 0x%02x\n", __FUNCTION__, address, (unsigned int)read_byte);
	
	spin_lock_irqsave(&shbatt_pkt_lock,flags);
	
	if( shbatt_hdq_initial_state != HDQ_INITIALIZED )
	{
		result = hdq_initialize_in_lock();
		if(result != SHBATT_RESULT_SUCCESS){
			SHBATT_ERROR("%s %d", __FUNCTION__, __LINE__);
			goto error_no_free;
		}
	}
	
	ret = gpio_request(HDQ_GPIO_PORT_NUM, "hdq");
	if(ret){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d ret=%d", __FUNCTION__, __LINE__, ret);
		goto error_no_free;
	}

	result = hdq_break ();
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto  error;
	}
	
	send_command &= ~HDQ_RW_BIT_MASK;
	*read_byte = 0x00;
	
	result = hdq_tx(send_command);
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
	result = hdq_rx(read_byte);
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
error:
	gpio_free(HDQ_GPIO_PORT_NUM);
	
error_no_free:
	spin_unlock_irqrestore(&shbatt_pkt_lock,flags);
	
	usleep(10*1000);
	
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t hdq_write_byte(const unsigned char address, const unsigned char write_byte)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	unsigned char send_command = address;
	struct timeval tv_start;
	unsigned long flags;
	int ret;
	
	SHBATT_TRACE("[S] %s 0x%02x, 0x%02x\n", __FUNCTION__, address, write_byte);

	spin_lock_irqsave(&shbatt_pkt_lock,flags);
	
	if( shbatt_hdq_initial_state != HDQ_INITIALIZED )
	{
		result = hdq_initialize_in_lock();
		if(result != SHBATT_RESULT_SUCCESS){
			SHBATT_ERROR("%s %d", __FUNCTION__, __LINE__);
			goto error_no_free;
		}
	}
	
	ret = gpio_request(HDQ_GPIO_PORT_NUM, "hdq");
	if(ret){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d ret=%d", __FUNCTION__, __LINE__, ret);
		goto error_no_free;
	}
	
	result = hdq_break();
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}

	send_command |= HDQ_RW_BIT_MASK;
	
	result = hdq_tx(send_command);
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
	do_gettimeofday(&tv_start);
	
	if(wait_ms_from_tv(&tv_start, HDQ_WAIT_MS_CYCLE_TIME) != SHBATT_RESULT_SUCCESS){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
	result = hdq_tx(write_byte);
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
error:
	gpio_free(HDQ_GPIO_PORT_NUM);
	
error_no_free:
	spin_unlock_irqrestore(&shbatt_pkt_lock,flags);
	
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);

	return result;
}

static shbatt_result_t hdq_tx(const unsigned char byte)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	hdq_bit_state_t bit;
	struct timeval tv_start;
	int i, ret;

	SHBATT_TRACE("[S] %s 0x%02x\n", __FUNCTION__, byte);

	ret = gpio_direction_output(HDQ_GPIO_PORT_NUM, 1);
	if(ret){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d ret=%d", __FUNCTION__, __LINE__, ret);
		goto error;
	}

	for(i = 0; i < 8; i++){
		if(((byte >> i) & 0x01) != 0){
			bit = HDQ_BIT_STATE_HI;
		}else{
			bit = HDQ_BIT_STATE_LO;
		}
		
		do_gettimeofday(&tv_start);
		
		result = hdq_bit_out(bit);
		if(result != SHBATT_RESULT_SUCCESS){
			break;
		}
		if(i < 7){
			result = wait_ms_from_tv(&tv_start, HDQ_WAIT_MS_WRITE_HI);
			if(result != SHBATT_RESULT_SUCCESS){
				break;
			}
		}
	}

error:
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t hdq_rx(unsigned char* byte)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	int i, ret;
	hdq_bit_state_t bit;

	SHBATT_TRACE("[S] %s 0x%02x\n", __FUNCTION__, (unsigned int)byte);

	ret = gpio_direction_input(HDQ_GPIO_PORT_NUM);
	if(ret){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d ret=%d", __FUNCTION__, __LINE__, ret);
		goto error;
	}
	
	*byte = 0x00;
	
	for(i = 0; i < 8; i++){
		result = hdq_bit_in(&bit);
		if(result != SHBATT_RESULT_SUCCESS){
			SHBATT_TRACE("%s %d count=%d result=%d", __FUNCTION__, __LINE__, i, result);
			break;
		}
		if(bit == HDQ_BIT_STATE_HI){
			*byte = *byte | (0x01 << i);
		}
	}

error:
	SHBATT_TRACE("[E] %s %d", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t hdq_bit_out(const hdq_bit_state_t bit)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	struct timeval tv_start;

	SHBATT_TRACE("[S] %s %u\n", __FUNCTION__, bit);
	
	do_gettimeofday(&tv_start);
	
	GPIO_SET(HDQ_GPIO_PORT_NUM, HDQ_BIT_STATE_LO);
	
	if(wait_ms_from_tv(&tv_start, HDQ_WAIT_MS_WRITE_LO) != SHBATT_RESULT_SUCCESS){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
	if(bit == HDQ_BIT_STATE_HI){
		GPIO_SET(HDQ_GPIO_PORT_NUM, HDQ_BIT_STATE_HI);
		if(GPIO_GET(HDQ_GPIO_PORT_NUM) == HDQ_BIT_STATE_LO){
			result = SHBATT_RESULT_FAIL;
			SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
			goto error;
		}
	}

	if(wait_ms_from_tv(&tv_start, HDQ_WAIT_MS_WRITE_LO_HI) != SHBATT_RESULT_SUCCESS){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
	GPIO_SET(HDQ_GPIO_PORT_NUM, HDQ_BIT_STATE_HI);
	if(GPIO_GET(HDQ_GPIO_PORT_NUM) == HDQ_BIT_STATE_LO){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t hdq_bit_in(hdq_bit_state_t* bit)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	struct timeval tv_start;
	unsigned long diff;

	SHBATT_TRACE("[S] %s 0x%02x\n", __FUNCTION__, (unsigned int)bit);
	
	do_gettimeofday(&tv_start);
	
	do{
		if(is_hdq_port_lo()){
			break;
		}
		
		timersub(&tv_start, &diff);
	}while(diff < HDQ_WAIT_MS_READ_TIMEOUT);

	do_gettimeofday(&tv_start);

	if(!is_hdq_port_lo()){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
	do{
		if(is_hdq_port_hi()){
			timersub(&tv_start, &diff);
			break;
		}
		timersub(&tv_start, &diff);
	}while(diff < HDQ_WAIT_MS_DEVICE_WRITE_0);
	
	if(!is_hdq_port_hi()){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
	
	if(diff > HDQ_WAIT_MS_DEVICE_WRITE_1){
		*bit = HDQ_BIT_STATE_LO;
	}else{
		*bit = HDQ_BIT_STATE_HI;
	}

error:
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t sleep_wakeup(void)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	struct timeval tv_start;
	int ret;

	SHBATT_TRACE("[S] %s\n", __FUNCTION__);

	ret = gpio_direction_output(HDQ_GPIO_PORT_NUM, 1);
	if(ret){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d ret=%d", __FUNCTION__, __LINE__, ret);
		goto error;
	}
	
	GPIO_SET(HDQ_GPIO_PORT_NUM, HDQ_BIT_STATE_LO);
	
	do_gettimeofday(&tv_start);
	
	if(wait_ms_from_tv(&tv_start, HDQ_WAIT_MS_BREAK_TIME) != SHBATT_RESULT_SUCCESS){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
		
	GPIO_SET(HDQ_GPIO_PORT_NUM, HDQ_BIT_STATE_HI);
	
	do_gettimeofday(&tv_start);
	
	if(wait_ms_from_tv(&tv_start, HDQ_WAIT_MS_WAKEUP_TIME) != SHBATT_RESULT_SUCCESS){
		result = SHBATT_RESULT_FAIL;
	}
	
error:
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t hdq_break(void)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	struct timeval tv_start;
	int ret;

	SHBATT_TRACE("[S] %s\n", __FUNCTION__);

	ret = gpio_direction_output(HDQ_GPIO_PORT_NUM, 1);
	if(ret){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d ret=%d", __FUNCTION__, __LINE__, ret);
		goto error;
	}
	
	GPIO_SET(HDQ_GPIO_PORT_NUM, HDQ_BIT_STATE_LO);
	
	do_gettimeofday(&tv_start);
	
	if(wait_ms_from_tv(&tv_start, HDQ_WAIT_MS_BREAK_TIME) != SHBATT_RESULT_SUCCESS){
		result = SHBATT_RESULT_FAIL;
		SHBATT_TRACE("%s %d", __FUNCTION__, __LINE__);
		goto error;
	}
		
	GPIO_SET(HDQ_GPIO_PORT_NUM, HDQ_BIT_STATE_HI);
	
	do_gettimeofday(&tv_start);
	
	if(wait_ms_from_tv(&tv_start, HDQ_WAIT_MS_BREAK_TIME_RECOVERY) != SHBATT_RESULT_SUCCESS){
		result = SHBATT_RESULT_FAIL;
	}
	
error:
	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static shbatt_result_t wait_ms_from_tv(struct timeval* tv_start, const int ms_wait)
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	unsigned long diff;

	SHBATT_TRACE("[S] %s %u\n", __FUNCTION__, ms_wait);
	
	do{
		timersub(tv_start, &diff);
	}while(diff < ms_wait);

	SHBATT_TRACE("[E] %s %d\n", __FUNCTION__, result);
	
	return result;
}

static int is_hdq_port_hi(void){
	int ret = 0;
	
	if(GPIO_GET(HDQ_GPIO_PORT_NUM) == HDQ_BIT_STATE_HI){
		ret = 1;
	}
	
	return ret;
}

static int is_hdq_port_lo(void){
	int ret = 1;
	
	if(GPIO_GET(HDQ_GPIO_PORT_NUM) == HDQ_BIT_STATE_HI){
		if(GPIO_GET(HDQ_GPIO_PORT_NUM) == HDQ_BIT_STATE_HI){
			ret = 0;
		}
	}
	
	return ret;
}

static void timersub(struct timeval *tv_start, unsigned long *diff)
{
	struct timeval tv_current;
	unsigned long long tsc_start;
	unsigned long long tsc_current;
	
	do_gettimeofday(&tv_current);
	
	tsc_start = (unsigned long long)(tv_start->tv_sec)*1000000+tv_start->tv_usec;
	tsc_current = (unsigned long long)(tv_current.tv_sec)*1000000+tv_current.tv_usec;
	
	*diff = (long)(tsc_current - tsc_start);
}

static int shbatt_drv_ioctl_cmd_read_hdq( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int i;
	unsigned char read_byte[3];
	shbatt_hdq_read_t hdq_read;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	l17_vreg_power_on(true);/*L17 in the regulater is ON for battery traceability*/
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&hdq_read,(shbatt_hdq_read_t __user *)arg, sizeof(shbatt_hdq_read_t)) != 0){
		SHBATT_TRACE("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
	
	for(i = 0; i < 100; i++){
		result = hdq_read_byte(hdq_read.addr_read, &read_byte[0]);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		result = hdq_read_byte(hdq_read.addr_read, &read_byte[1]);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		result = hdq_read_byte(hdq_read.addr_read, &read_byte[2]);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		if(read_byte[0] != read_byte[1] || read_byte[0] != read_byte[2]){
			result = SHBATT_RESULT_FAIL;
			continue;
		}
		hdq_read.byte_read = read_byte[0];
		result = SHBATT_RESULT_SUCCESS;
		break;
	}
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_TRACE("%s : hdq_read_byte failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((shbatt_hdq_read_t __user *)arg, &hdq_read, sizeof(shbatt_hdq_read_t)) != 0){
		SHBATT_TRACE("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
	
error:
#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	l17_vreg_power_on(false);/*L17 in the regulater is OFF for battery traceability*/
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}
static int shbatt_drv_ioctl_cmd_write_hdq( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int i;
	shbatt_hdq_write_t hdq_write;
	unsigned char read_byte;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	
#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	l17_vreg_power_on(true);/*L17 in the regulater is ON for battery traceability*/
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	if(copy_from_user(&hdq_write,(shbatt_hdq_write_t __user *)arg, sizeof(shbatt_hdq_write_t)) != 0){
		SHBATT_TRACE("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	for(i = 0; i < 100; i++){
		result = hdq_write_byte(hdq_write.addr_write, hdq_write.byte_write);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		result = hdq_read_byte(hdq_write.addr_write, &read_byte);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		if(hdq_write.byte_write != read_byte){
			result = SHBATT_RESULT_FAIL;
			continue;
		}
		result = SHBATT_RESULT_SUCCESS;
		break;
	}
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_TRACE("%s : hdq_write_byte failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	l17_vreg_power_on(false);/*L17 in the regulater is OFF for battery traceability*/
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_check_battery_auth( struct file* fi_p, unsigned long arg )
{
	int i, ret = 0;
	unsigned char read_byte[5];
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	l17_vreg_power_on(true);/*L17 in the regulater is ON for battery traceability*/
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	for(i = 0; i < 100; i++){
		result = hdq_read_byte(0x40, &read_byte[0]);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		result = hdq_read_byte(0x41, &read_byte[1]);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		result = hdq_read_byte(0x42, &read_byte[2]);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		result = hdq_read_byte(0x43, &read_byte[3]);
		if(result != SHBATT_RESULT_SUCCESS){
			continue;
		}
		read_byte[4] = '\0';
		
		result = SHBATT_RESULT_SUCCESS;
		break;
	}
	
	if(result != SHBATT_RESULT_SUCCESS){
		SHBATT_ERROR("%s : hdq_read_byte failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((unsigned char __user *)arg, read_byte, sizeof(read_byte)) != 0){
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
	
error:
#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	l17_vreg_power_on(false);/*L17 in the regulater is OFF for battery traceability*/
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

#define SHBATT_BATTERY_TRACE_PACK_YEAR_ADDR					0x59
#define SHBATT_BATTERY_TRACE_PACK_MONTH_ADDR				0x5A
#define SHBATT_BATTERY_TRACE_PACK_DAY_ADDR					0x5B
#define SHBATT_BATTERY_TRACE_PACK_MANU_NUM_UPPER_ADDR		0x5C
#define SHBATT_BATTERY_TRACE_PACK_MANU_NUM_LOWER_ADDR		0x5D
#define SHBATT_BATTERY_TRACE_MAKER_NAME_ADDR				0x5E
#define SHBATT_BATTERY_TRACE_CELL_YEAR_ADDR					0x5F
#define SHBATT_BATTERY_TRACE_CELL_MONTH_ADDR				0x60
#define SHBATT_BATTERY_TRACE_CELL_DAY_ADDR					0x61
#define SHBATT_BATTERY_TRACE_CELL_LINE_INFO_UPPER_ADDR		0x62
#define SHBATT_BATTERY_TRACE_CELL_LINE_INFO_LOWER_ADDR		0x63
#define SHBATT_BATTERY_TRACE_PACK_LINE_INFO_UPPER_ADDR		0x64
#define SHBATT_BATTERY_TRACE_PACK_LINE_INFO_LOWER_ADDR		0x65
#define BATAUTHFLAG_ON 1

static shbatt_result_t shbatt_seq_initialize_battery_trace_data( void )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	unsigned char pack_date[4] = "000";
	unsigned char cell_date[4] = "000";
	unsigned char pack_line_info[3] = "00";
	unsigned char cell_line_info[3] = "00";
	unsigned char manufactur_num[2] = "0";
	unsigned char manufactur_num_buf[5] = "0000";
	unsigned char maker_name[2] = "0";
	int count;
	unsigned short manufactur_num_short;
	int ret = 0;
	shbatt_smem_info_t smem_info;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	ret = shbatt_api_get_smem_info( & smem_info );

	if(ret == 0)
	{
		if(smem_info.batauthflg == BATAUTHFLAG_ON)
		{

			SHBATT_TRACE("[E] %s batauthflg on \n",__FUNCTION__);
			
			/* PackDate */
			sprintf(pack_date_param, "%s", "000");
			
			/* PackNum */
			sprintf(manufactur_num_param, "%s", "0000");
			
			/* MakerName */
			sprintf(maker_name_param, "%s", "0");
			
			/* CellDate */
			sprintf(cell_date_param, "%s", "000");
			
			/* CellLineInfo */
			sprintf(cell_line_info_param, "%s", "00");
			
			/* PackLineInfo */
			sprintf(pack_line_info_param, "%s", "00");
			
			return SHBATT_RESULT_SUCCESS;
		}
	}
	
#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
	l17_vreg_power_on(true);/*L17 in the regulater is ON for battery traceability*/
#endif	/* CONFIG_PM_SUPPORT_BATT_AUTH */
	
	for(count=0; count<100; count++)
	{
		/* PackDate */
		result = hdq_read_byte(SHBATT_BATTERY_TRACE_PACK_YEAR_ADDR, &pack_date[0]);
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_PACK_MONTH_ADDR, &pack_date[1]);
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_PACK_DAY_ADDR, &pack_date[2]);
		pack_date[3] = '\0';
		
		/* PackNum */
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_PACK_MANU_NUM_UPPER_ADDR, &manufactur_num[0]);
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_PACK_MANU_NUM_LOWER_ADDR, &manufactur_num[1]);
		manufactur_num_short = manufactur_num[0];
		manufactur_num_short = manufactur_num_short << 8;
		manufactur_num_short += manufactur_num[1];
		
		if(manufactur_num_short < 10)
		{
			sprintf(manufactur_num_buf, "%s%1d","000" , manufactur_num_short);
		}
		else if(manufactur_num_short < 100)
		{
			sprintf(manufactur_num_buf, "%s%2d","00" , manufactur_num_short);
		}
		else if(manufactur_num_short < 1000)
		{
			sprintf(manufactur_num_buf, "%s%3d","0" , manufactur_num_short);
		}
		else
		{
			sprintf(manufactur_num_buf, "%d", manufactur_num_short);
		}
		
		/* MakerName */
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_MAKER_NAME_ADDR, &maker_name[0]);
		maker_name[1] = '\0';

		/* CellDate */
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_CELL_YEAR_ADDR, &cell_date[0]);
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_CELL_MONTH_ADDR, &cell_date[1]);
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_CELL_DAY_ADDR, &cell_date[2]);
		cell_date[3] = '\0';

		/* CellLineInfo */
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_CELL_LINE_INFO_UPPER_ADDR, &cell_line_info[0]);
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_CELL_LINE_INFO_LOWER_ADDR, &cell_line_info[1]);
		cell_line_info[2] = '\0';

		/* PackLineInfo */
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_PACK_LINE_INFO_UPPER_ADDR, &pack_line_info[0]);
		result |= hdq_read_byte(SHBATT_BATTERY_TRACE_PACK_LINE_INFO_LOWER_ADDR, &pack_line_info[1]);
		pack_line_info[2] = '\0';

		SHBATT_TRACE("[P] %s pack_year=0x%x pack_month=0x%x pack_day=0x%x \n",__FUNCTION__, pack_date[0], pack_date[1], pack_date[2]);
		SHBATT_TRACE("[P] %s manufactur_num_upper=0x%x manufactur_num_lower=0x%x manufactur_num_short=%d \n",__FUNCTION__, manufactur_num[0], manufactur_num[1], manufactur_num_short);
		SHBATT_TRACE("[P] %s maker_name=0x%x \n",__FUNCTION__, maker_name[0]);
		SHBATT_TRACE("[P] %s cell_year=0x%x cell_month=0x%x cell_day=0x%x \n",__FUNCTION__, cell_date[0], cell_date[1], cell_date[2]);
		SHBATT_TRACE("[P] %s cell_line_info_upper=0x%x cell_line_info_lower=0x%x \n",__FUNCTION__, cell_line_info[0], cell_line_info[1]);
		SHBATT_TRACE("[P] %s pack_line_info_upper=0x%x pack_line_info_lower=0x%x \n",__FUNCTION__, pack_line_info[0], pack_line_info[1]);

		if(result == SHBATT_RESULT_SUCCESS)
		{
			break;
		}
	}
	
	if(result == SHBATT_RESULT_SUCCESS)
	{
		SHBATT_TRACE("[P] %s battery trace data read success \n",__FUNCTION__);
		
		/* PackDate */
		sprintf(pack_date_param, "%s", pack_date);
		
		/* PackNum */
		sprintf(manufactur_num_param, "%s", manufactur_num_buf);
		
		/* MakerName */
		sprintf(maker_name_param, "%s", maker_name);
		
		/* CellDate */
		sprintf(cell_date_param, "%s", cell_date);
		
		/* CellLineInfo */
		sprintf(cell_line_info_param, "%s", cell_line_info);
		
		/* PackLineInfo */
		sprintf(pack_line_info_param, "%s", pack_line_info);
	}
	else
	{
		SHBATT_TRACE("[P] %s battery trace data read fail \n",__FUNCTION__);
		
		/* PackDate */
		sprintf(pack_date_param, "%s", "000");
		
		/* PackNum */
		sprintf(manufactur_num_param, "%s", "0000");
		
		/* MakerName */
		sprintf(maker_name_param, "%s", "0");
		
		/* CellDate */
		sprintf(cell_date_param, "%s", "000");
		
		/* CellLineInfo */
		sprintf(cell_line_info_param, "%s", "00");
		
		/* PackLineInfo */
		sprintf(pack_line_info_param, "%s", "00");
	}
	
	shbatt_battery_trace_data_read_finish = 1;

	l17_vreg_power_force_off();/*L17 in the regulater is OFF for battery traceability*/
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return SHBATT_RESULT_SUCCESS;
}

static int shbatt_drv_ioctl_cmd_set_battery_auth( struct file* fi_p, unsigned long arg )
{
	int ret;
	int batt_auth = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	if(copy_from_user(&batt_auth, (int*)arg, sizeof(int)) != 0)
	{
		ret = -EPERM;
		SHBATT_TRACE("%s copy_from_user failed",__FUNCTION__);
	}
	else
	{
		SHBATT_TRACE("%s copy_from_user succeeded batt_auth=%d ",__FUNCTION__, batt_auth);
		shbatt_cur_battery_auth = batt_auth;
		ret = 0;
		l17_vreg_power_force_off();
	}
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return ret;
}
#else /* CONFIG_PM_SUPPORT_BATT_AUTH */
static shbatt_result_t hdq_initialize(void){ return SHBATT_RESULT_SUCCESS; }
static shbatt_result_t shbatt_seq_initialize_battery_trace_data( void ){ return SHBATT_RESULT_SUCCESS; }
#endif /* CONFIG_PM_SUPPORT_BATT_AUTH */

static int shbatt_drv_ioctl_cmd_get_cpu_clock_level( struct file* fi_p, unsigned long arg )
{
	int result = 0;
	shbatt_cpu_clock_level_t cpu_clock_level;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_api_get_cpu_clock_level( &cpu_clock_level ) != SHBATT_RESULT_SUCCESS)
		{
			result = -EPERM;
			break;
		}
		
		if(copy_to_user((shbatt_cpu_clock_level_t __user *)arg, &cpu_clock_level, sizeof(shbatt_cpu_clock_level_t)))
		{
			result = -EPERM;
			break;
		}
	}while(0);
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

shbatt_result_t shbatt_api_get_cpu_clock_level( shbatt_cpu_clock_level_t* cpu_clock_level_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}
		result = shbatt_seq_get_cpu_clock_level(cpu_clock_level_p);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static shbatt_result_t shbatt_seq_get_cpu_clock_level( shbatt_cpu_clock_level_t* cpu_clock_level_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

#if defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE1)
	result = shbatt_seq_get_cpu_clock_level_type1(cpu_clock_level_p);
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE2)
	result = shbatt_seq_get_cpu_clock_level_type2(cpu_clock_level_p);
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE3)
	result = shbatt_seq_get_cpu_clock_level_type3(cpu_clock_level_p);
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE4)
	result = shbatt_seq_get_cpu_clock_level_type4(cpu_clock_level_p);
#else
	result = shbatt_seq_get_cpu_clock_level_default(cpu_clock_level_p);
#endif
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}
#if defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE1)
static shbatt_result_t shbatt_seq_get_cpu_clock_level_type1( shbatt_cpu_clock_level_t* cpu_clock_level_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	int battery_capacity = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	result = shbatt_api_get_real_battery_capacity(&battery_capacity);
	if(result != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_TRACE("[E] %s shbatt_api_get_real_battery_capacity err=%d \n",__FUNCTION__, result);
		return SHBATT_RESULT_REJECTED;
	}
	SHBATT_TRACE("[P] %s battery_capacity=%d \n",__FUNCTION__, battery_capacity);
	
	if(shbatt_low_battery_interrupt == true)
	{
		SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
		return SHBATT_RESULT_SUCCESS;
	}
	if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL4;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL1;
	}
	else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL0;
	}
	
	SHBATT_TRACE("[E] %s cpu_clock_level_p=%d \n",__FUNCTION__, *cpu_clock_level_p);
	return SHBATT_RESULT_SUCCESS;
}

#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE2)
static shbatt_result_t shbatt_seq_get_cpu_clock_level_type2( shbatt_cpu_clock_level_t* cpu_clock_level_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	int battery_capacity = 0;
	int usb_host_status = 0;
	int batt_temp = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	result = shbatt_api_get_real_battery_capacity(&battery_capacity);
	if(result != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_TRACE("[E] %s shbatt_api_get_real_battery_capacity err=%d \n",__FUNCTION__, result);
		return SHBATT_RESULT_REJECTED;
	}
	SHBATT_TRACE("[P] %s battery_capacity=%d \n",__FUNCTION__, battery_capacity);
	
	usb_host_status = msm_otg_is_usb_host_running();
	SHBATT_TRACE("[P] %s usb_host_status=%d \n",__FUNCTION__, usb_host_status);
	
	if(usb_host_status == 1)
	{
		/* usb host on */
		if(shbatt_low_battery_interrupt == true)
		{
			SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
			return SHBATT_RESULT_SUCCESS;
		}
		
		if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
		}
		else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
		}
	}
	else
	{
		/* usb host off */
		result = shbatt_seq_get_battery_temperature(&batt_temp);
		if(result != SHBATT_RESULT_SUCCESS)
		{
			SHBATT_TRACE("[E] %s shbatt_seq_get_battery_temperature err=%d \n",__FUNCTION__, result);
			return SHBATT_RESULT_REJECTED;
		}
		SHBATT_TRACE("[P] %s batt_temp=%d \n",__FUNCTION__, batt_temp);
		
		if(batt_temp > 5)
		{
			/* battery temp > 5 */
			if(shbatt_low_battery_interrupt == true)
			{
				SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
				return SHBATT_RESULT_SUCCESS;
			}

			if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL4;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL1;
			}
			else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL0;
			}
		}
		else
		{
			/* battery temp =< 5 */
			if(shbatt_low_battery_interrupt == true)
			{
				SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
				return SHBATT_RESULT_SUCCESS;
			}
			
			if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
			}
			else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
			}
		}
	}
	
	SHBATT_TRACE("[E] %s cpu_clock_level_p=%d \n",__FUNCTION__, *cpu_clock_level_p);
	return SHBATT_RESULT_SUCCESS;
}
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE3)
static shbatt_result_t shbatt_seq_get_cpu_clock_level_type3( shbatt_cpu_clock_level_t* cpu_clock_level_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	int battery_capacity = 0;
	int usb_host_status = 0;
	int batt_temp = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	result = shbatt_api_get_real_battery_capacity(&battery_capacity);
	if(result != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_TRACE("[E] %s shbatt_api_get_real_battery_capacity err=%d \n",__FUNCTION__, result);
		return SHBATT_RESULT_REJECTED;
	}
	SHBATT_TRACE("[P] %s battery_capacity=%d \n",__FUNCTION__, battery_capacity);
	
	usb_host_status = msm_otg_is_usb_host_running();
	SHBATT_TRACE("[P] %s usb_host_status=%d \n",__FUNCTION__, usb_host_status);
	
	if(usb_host_status == 1)
	{
		/* usb host on */
		if(shbatt_low_battery_interrupt == true)
		{
			SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
			return SHBATT_RESULT_SUCCESS;
		}
		
		if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH_USBUSE) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH_USBUSE))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH_USBUSE) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH_USBUSE))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
		}
		else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH_USBUSE)
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
		}
	}
	else
	{
		/* usb host off */
		result = shbatt_seq_get_battery_temperature(&batt_temp);
		if(result != SHBATT_RESULT_SUCCESS)
		{
			SHBATT_TRACE("[E] %s shbatt_seq_get_battery_temperature err=%d \n",__FUNCTION__, result);
			return SHBATT_RESULT_REJECTED;
		}
		SHBATT_TRACE("[P] %s batt_temp=%d \n",__FUNCTION__, batt_temp);
		
		if(batt_temp > 5)
		{
			/* battery temp > 5 */
			if(shbatt_low_battery_interrupt == true)
			{
				SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
				return SHBATT_RESULT_SUCCESS;
			}
			if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL4;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL1;
			}
			else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL0;
			}
		}
		else
		{
			/* battery temp =< 5 */
			if(shbatt_low_battery_interrupt == true)
			{
				SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
				return SHBATT_RESULT_SUCCESS;
			}
			if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH_TLOW) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH_TLOW))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH_TLOW) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH_TLOW))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH_TLOW) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH_TLOW))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL4;
			}
			else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH_TLOW)
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
			}
		
		}
	}
	
	SHBATT_TRACE("[E] %s cpu_clock_level_p=%d \n",__FUNCTION__, *cpu_clock_level_p);
	return SHBATT_RESULT_SUCCESS;
}
#elif defined(CONFIG_PM_CPU_CLOCK_LEVEL_TYPE4)
static shbatt_result_t shbatt_seq_get_cpu_clock_level_type4( shbatt_cpu_clock_level_t* cpu_clock_level_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	int battery_capacity = 0;
	int usb_host_status = 0;
	int batt_temp = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	result = shbatt_api_get_real_battery_capacity(&battery_capacity);
	if(result != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_TRACE("[E] %s shbatt_api_get_real_battery_capacity err=%d \n",__FUNCTION__, result);
		return SHBATT_RESULT_REJECTED;
	}
	SHBATT_TRACE("[P] %s battery_capacity=%d \n",__FUNCTION__, battery_capacity);
	
	usb_host_status = msm_otg_is_usb_host_running();
	SHBATT_TRACE("[P] %s usb_host_status=%d \n",__FUNCTION__, usb_host_status);
	
	if(usb_host_status == 1)
	{
		/* usb host on */
		if(shbatt_low_battery_interrupt == true)
		{
			SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
			return SHBATT_RESULT_SUCCESS;
		}
		
		if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL4;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
		}
		else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL1;
		}
		else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
		{
			*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL0;
		}
	}
	else
	{
		/* usb host off */
		result = shbatt_seq_get_battery_temperature(&batt_temp);
		if(result != SHBATT_RESULT_SUCCESS)
		{
			SHBATT_TRACE("[E] %s shbatt_seq_get_battery_temperature err=%d \n",__FUNCTION__, result);
			return SHBATT_RESULT_REJECTED;
		}
		SHBATT_TRACE("[P] %s batt_temp=%d \n",__FUNCTION__, batt_temp);
		
		if(batt_temp > 5)
		{
			/* battery temp > 5 */
			if(shbatt_low_battery_interrupt == true)
			{
				SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
				return SHBATT_RESULT_SUCCESS;
			}

			if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL4;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL1;
			}
			else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL0;
			}
		}
		else
		{
			/* battery temp =< 5 */
			if(shbatt_low_battery_interrupt == true)
			{
				SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL7 \n",__FUNCTION__);
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL7;
				return SHBATT_RESULT_SUCCESS;
			}
			
			if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL6_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
			}
			else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
			}
			else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
			{
				*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
			}
		}
	}
	
	SHBATT_TRACE("[E] %s cpu_clock_level_p=%d \n",__FUNCTION__, *cpu_clock_level_p);
	return SHBATT_RESULT_SUCCESS;
}
#else
static shbatt_result_t shbatt_seq_get_cpu_clock_level_default( shbatt_cpu_clock_level_t* cpu_clock_level_p )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	int battery_capacity = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	result = shbatt_api_get_real_battery_capacity(&battery_capacity);
	if(result != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_TRACE("[E] %s shbatt_api_get_real_battery_capacity err=%d \n",__FUNCTION__, result);
		return SHBATT_RESULT_REJECTED;
	}
	SHBATT_TRACE("[P] %s battery_capacity=%d \n",__FUNCTION__, battery_capacity);
	
	if(shbatt_low_battery_interrupt == true)
	{
		SHBATT_TRACE("[E] %s SHBATT_CPU_CLOCK_LEVEL6 \n",__FUNCTION__);
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL6;
		return SHBATT_RESULT_SUCCESS;
	}
	
	if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL5_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL5;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL4_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL4;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL3_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL3;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL2_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL2;
	}
	else if((battery_capacity >= SHBATT_CPU_CLOCK_LEVEL1_CAPACITY_THRESH) && (battery_capacity < SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH))
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL1;
	}
	else if(battery_capacity >= SHBATT_CPU_CLOCK_LEVEL0_CAPACITY_THRESH)
	{
		*cpu_clock_level_p = SHBATT_CPU_CLOCK_LEVEL0;
	}
	
	SHBATT_TRACE("[E] %s cpu_clock_level_p=%d \n",__FUNCTION__, *cpu_clock_level_p);
	return SHBATT_RESULT_SUCCESS;
}
#endif
shbatt_result_t shbatt_api_get_real_battery_capacity( int* cap_p )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_REAL_BATTERY_CAPACITY;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.cap_p = cap_p;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static void shbatt_task_cmd_get_real_battery_capacity( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int cap = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_real_battery_capacity( &cap );

	if(pkt_p->prm.cap_p != NULL)
	{
		*(pkt_p->prm.cap_p) = cap;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s cap=%d \n",__FUNCTION__, cap);
}

static shbatt_result_t shbatt_seq_get_real_battery_capacity( int* battery_capacity_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_REAL_BATTERY_CAPACITY;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*battery_capacity_p = shbatt_usse_pkt.prm.cap;
	}

	SHBATT_TRACE("[E] %s battery_capacity_p=%d \n",__FUNCTION__, *battery_capacity_p);

	return result;
}

shbatt_result_t shbatt_api_notify_charge_switch_status( shbatt_chg_switch_status_t switch_status )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s switch_status=%d \n",__FUNCTION__, switch_status);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_NOTIFY_CHARGE_SWITCH_STATUS;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.switch_status = switch_status;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

static void shbatt_task_cmd_notify_charge_switch_status( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_notify_charge_switch_status( pkt_p->prm.switch_status );

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static shbatt_result_t shbatt_seq_notify_charge_switch_status( shbatt_chg_switch_status_t switch_status )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_NOTIFY_CHARGE_SWITCH_STATUS;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.switch_status = switch_status;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static int shbatt_drv_ioctl_cmd_get_depleted_capacity( struct file* fi_p, unsigned long arg )
{
	int result = 0;
	int depleted_capacity_per = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_api_get_depleted_capacity( &depleted_capacity_per ) != SHBATT_RESULT_SUCCESS)
		{
			result = -EPERM;
			break;
		}
		
		if(copy_to_user((int __user *)arg, &depleted_capacity_per, sizeof(int)))
		{
			result = -EPERM;
			break;
		}
	}while(0);
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

shbatt_result_t shbatt_api_get_depleted_capacity(int* depleted_capacity_per)
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_DEPLETED_CAPACITY;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.depleted_capacity_per = depleted_capacity_per;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static void shbatt_task_cmd_get_depleted_capacity( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int depleted_capacity_per = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_depleted_capacity( &depleted_capacity_per );

	if(pkt_p->prm.cap_p != NULL)
	{
		*(pkt_p->prm.depleted_capacity_per) = depleted_capacity_per;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s depleted_capacity_per=%d \n",__FUNCTION__, depleted_capacity_per);
}

static shbatt_result_t shbatt_seq_get_depleted_capacity( int* depleted_capacity_per )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_DEPLETED_CAPACITY;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*depleted_capacity_per = shbatt_usse_pkt.prm.depleted_capacity_per;
	}

	SHBATT_TRACE("[E] %s depleted_capacity_per=%d \n",__FUNCTION__, *depleted_capacity_per);

	return result;
}

static int shbatt_drv_ioctl_cmd_set_depleted_battery_flg( struct file* fi_p, unsigned long arg )
{
	int ret;
	int depleted_battery_flg = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	if(copy_from_user(&depleted_battery_flg, (int*)arg, sizeof(int)) != 0)
	{
		ret = -EPERM;
		SHBATT_TRACE("%s copy_from_user failed",__FUNCTION__);
	}
	else
	{
		SHBATT_TRACE("%s copy_from_user succeeded",__FUNCTION__);
		ret = shbatt_api_set_depleted_battery_flg( depleted_battery_flg );
	}
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return ret;
}

shbatt_result_t shbatt_api_set_depleted_battery_flg(int depleted_battery_flg)
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_SET_DEPLETED_BATTERY_FLG;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.depleted_battery_flg = depleted_battery_flg;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static void shbatt_task_cmd_set_depleted_battery_flg( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	shbatt_cb_func_t1 cb_func;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_depleted_battery_flg( pkt_p->prm.depleted_battery_flg );

	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}

	SHBATT_TRACE("[E] %s depleted_battery_flg=%d \n",__FUNCTION__, pkt_p->prm.depleted_battery_flg);
}

static shbatt_result_t shbatt_seq_set_depleted_battery_flg( int depleted_battery_flg )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_SET_DEPLETED_BATTERY_FLG;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.depleted_battery_flg = depleted_battery_flg;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s depleted_battery_flg=%d \n",__FUNCTION__, depleted_battery_flg);

	return result;
}

static int shbatt_drv_ioctl_cmd_get_depleted_battery_flg( struct file* fi_p, unsigned long arg )
{
	int result = 0;
	int depleted_battery_flg = 0;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	do
	{
		if(shbatt_api_get_depleted_battery_flg( &depleted_battery_flg ) != SHBATT_RESULT_SUCCESS)
		{
			result = -EPERM;
			break;
		}
		
		if(copy_to_user((int __user *)arg, &depleted_battery_flg, sizeof(int)))
		{
			result = -EPERM;
			break;
		}
	}while(0);
	
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}

shbatt_result_t shbatt_api_get_depleted_battery_flg(int* depleted_battery_flg)
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_DEPLETED_BATTERY_FLG;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.depleted_battery_flg_p = depleted_battery_flg;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static void shbatt_task_cmd_get_depleted_battery_flg( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int depleted_battery_flg = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_depleted_battery_flg( &depleted_battery_flg );

	if(pkt_p->prm.cap_p != NULL)
	{
		*(pkt_p->prm.depleted_battery_flg_p) = depleted_battery_flg;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s depleted_battery_flg=%d \n",__FUNCTION__, depleted_battery_flg);
}

static shbatt_result_t shbatt_seq_get_depleted_battery_flg( int* depleted_battery_flg )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_DEPLETED_BATTERY_FLG;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*depleted_battery_flg = shbatt_usse_pkt.prm.depleted_battery_flg;
	}

	SHBATT_TRACE("[E] %s depleted_battery_flg=%d \n",__FUNCTION__, *depleted_battery_flg);

	return result;
}

static int shbatt_drv_ioctl_cmd_set_battery_health( struct file* fi_p, unsigned long arg )
{
	shbatt_health_t battery_health;

	int ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&battery_health,(shbatt_health_t*)arg,sizeof(shbatt_health_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		ret = shbatt_api_set_battery_health(battery_health);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_ave_current_and_ave_voltage( struct file* fi_p, unsigned long arg )
{
	int ret;
	int cur;
	int vol;
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = shbatt_seq_get_average_current_and_average_voltage(&cur, &vol);
	
	if(ret == SHBATT_RESULT_SUCCESS)
	{
		shbatt_current_voltage_t cv;
		cv.cur = cur;
		cv.vol = vol;
		ret = copy_to_user((shbatt_current_voltage_t*)arg, &cv, sizeof(cv));
	}


	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_read_adc_channels(struct file* fi_p, unsigned long arg)
{
	shbatt_adc_read_request_t request;
	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&request,(shbatt_adc_read_request_t*)arg,sizeof(request)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}
	else
	{
		do
		{
			int i;
			if(request.request_num <= 0 || request.request_num > NUM_SHBATT_ADC_CHANNEL)
			{
				ret = -EPERM;
				break;
			}
			
			for(i = 0; i < request.request_num; i ++)
			{
				shbatt_adc_t* adc = &request.channels[i];
#ifndef CONFIG_PM_MPP_12_USE_BACKLIGHT_THERM
				if(adc->channel == SHBATT_ADC_CHANNEL_BACKLIGHT_TEMP)
				{
					/* read backlignt temp is always error. so skip*/
					continue;
				}
#endif
				ret |= shbatt_api_read_adc_channel_no_conversion(adc);
			}
			if(ret == SHBATT_RESULT_SUCCESS)
			{
				if(copy_to_user((shbatt_adc_read_request_t*)arg, &request, sizeof(request)) != 0)
				{
					SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
					ret = -EPERM;
				} 
			}
		}while(0);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;


	
}

shbatt_result_t shbatt_api_set_battery_health( shbatt_health_t health )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_SET_BATTERY_HEALTH;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.hea   = health;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static void shbatt_task_cmd_set_battery_health( shbatt_packet_t* pkt_p )
{
	shbatt_cb_func_t1 cb_func;
	shbatt_result_t result;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	result = shbatt_seq_set_battery_health(pkt_p->prm.hea);
	
	if(pkt_p->hdr.cb_p != NULL)
	{
		cb_func = (shbatt_cb_func_t1)pkt_p->hdr.cb_p;

		cb_func(result);
	}
	else
	{
		if(pkt_p->hdr.ret_p != NULL)
		{
			*(pkt_p->hdr.ret_p) = result;
		}

		if(pkt_p->hdr.cmp_p != NULL)
		{
			complete(pkt_p->hdr.cmp_p);
		}
	}
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static shbatt_result_t shbatt_seq_set_battery_health( shbatt_health_t health )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s health=%d \n",__FUNCTION__, health);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_SET_BATTERY_HEALTH;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.hea = health;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static int shbatt_drv_ioctl_cmd_set_android_shutdown_timer( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
	
	if(val == 1)
	{
		cancel_delayed_work(&lowbatt_shutdown_struct);
		schedule_delayed_work_on(0, &android_shutdown_struct, msecs_to_jiffies(60000));
	}
	
error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_seq_get_average_current_and_average_voltage(int* cur, int* vol)
{	
	int result = 0;

	static const int sampling_period = 15;	/*ms*/

	static const int sample_count = 18;

	int min_voltage = 0;
	int max_voltage = 0;
	int sum_voltage = 0;
	int min_current = 0;
	int max_current = 0;
	int sum_current = 0;
	int i;

	shbatt_adc_t v_result = {SHBATT_ADC_CHANNEL_VBAT,0,0,0};
	shbatt_adc_t i_result = {SHBATT_ADC_CHANNEL_IBAT,0,0,0};	/*The actual value is vsense.ave of BMS not IBAT.*/

	for(i = 0; i < sample_count; i++)
	{
		int vol;
		int cur;

		usleep(sampling_period * 1000/*ms to us*/);

		result |= shbatt_api_read_adc_channel_no_conversion(&v_result);
		result |= shbatt_api_read_adc_channel_no_conversion(&i_result);
		vol =  (int)v_result.physical;
		cur = (int)i_result.physical;

		sum_voltage += vol;
		sum_current += cur;
		if(i == 0)
		{
			max_voltage = vol;
			min_voltage = vol;
			max_current = cur;
			min_current = cur;
		}
		else
		{
			max_voltage = MAX(max_voltage, vol);
			min_voltage = MIN(min_voltage, vol);
			max_current = MAX(max_current, cur);
			min_current = MIN(min_current, cur);
		}
	}

	*vol = (sum_voltage - min_voltage - max_voltage) / (sample_count - 2/*max,min*/);
	*cur = (sum_current - min_current - max_current) / (sample_count - 2/*max,min*/);
	
	return result;
}
/*IIC bus access.*/
#ifdef CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C

static int wlchg_i2c_probe(struct i2c_client *clt_p, const struct i2c_device_id* id_p)
{

	wlchg_i2c_client_t* client_p;
	client_p = (wlchg_i2c_client_t*)kzalloc(sizeof(wlchg_i2c_client_t), GFP_KERNEL);
	if(!client_p)
	{
		return -ENOMEM;
	}
	i2c_set_clientdata(clt_p,client_p);
	client_p->client = clt_p;
	wlchg_i2c_client = client_p;

	return 0;
}
static int wlchg_i2c_remove(struct i2c_client* clt_p)
{
	wlchg_i2c_client_t* client_p;
    client_p = i2c_get_clientdata(clt_p);
    kfree(client_p);
	return 0;
}

static int wlchg_i2c_write(uint8_t sub_address, uint8_t* wr_data, int wr_data_size)
{
	uint8_t write_data[1/*address*/+ WLCHG_I2C_MAX_SIZE_PER_ACCESS];
	int ret;
	int try_count = 0;
	static const int retry_wait = 1 * 1000;
	static const int retry_max = 1000;	/*1ms x 1000 = 1sec*/


	struct i2c_msg msgs[] = 
	{
		{	/*write*/
			.addr = wlchg_i2c_client->client->addr,
			.flags = 0,
			.buf = write_data,
		},
	};
	
	if(wr_data_size > WLCHG_I2C_MAX_SIZE_PER_ACCESS)
	{
		return -EINVAL;
	}

	msgs[0].len = sizeof(sub_address)+wr_data_size;
	write_data[0] = sub_address;
	memcpy(&write_data[1], wr_data, wr_data_size);
	
	do
	{
		ret = i2c_transfer( wlchg_i2c_client->client->adapter, msgs, ARRAY_SIZE(msgs));
		if(ret < 0 && ret != -EAGAIN)return ret;
		if(ret == -EAGAIN)
		{
			usleep(retry_wait);
		}
	}while(ret == -EAGAIN && try_count++ < retry_max);
	
	return (ret == ARRAY_SIZE(msgs))? 0 : ret;
}

static int wlchg_i2c_read(uint8_t sub_address, uint8_t* rd_data, int rd_data_size)
{
	int ret;
	int try_count = 0;
	static const int retry_wait = 1 * 1000;
	static const int retry_max = 1000;	/*1ms x 1000 = 1sec*/

	struct i2c_msg msgs[] = 
	{
		{	/*write*/
			.addr = wlchg_i2c_client->client->addr,
			.flags = 0,
			.buf = &sub_address,
			.len = sizeof(sub_address),
		},
		{	/*read*/
			.addr = wlchg_i2c_client->client->addr,
			.flags = I2C_M_RD,
			.buf = rd_data,
		}
	};
	
	msgs[1].len = rd_data_size;
	
	do
	{
		ret = i2c_transfer( wlchg_i2c_client->client->adapter, msgs, ARRAY_SIZE(msgs));
		if(ret < 0 && ret != -EAGAIN)return ret;
		if(ret == -EAGAIN)
		{
			usleep(retry_wait);
		}
	}while(ret == -EAGAIN && try_count++ < retry_max );
	
	return (ret == ARRAY_SIZE(msgs))? 0 : ret;
}

static int wlchg_wait_for_ready(void)	
{
	static const int retry_max = 100;
	int try_count = 0;
	uint8_t status = 0;
	int ret;
	usleep(10 *1000);

	do
	{
		ret = wlchg_read_status(&status);
		if(ret != 0)
		{
			SHBATT_ERROR("ret = %d (%s, %d)\n", ret, __FUNCTION__, __LINE__);
			return ret;
		}
		try_count++;
		if(status == WLCHG_STATUS_BUSY && try_count < retry_max)
		{
			usleep(10 * 1000);
		}
	}while(status == WLCHG_STATUS_BUSY && try_count < retry_max);
	
	ret = status;
	ret = -ret;
	return ret;
}

static int wlchg_firmware_update_from_buffer(const u8* data, size_t size)
{
	int ret = 0;
	int result = 0;
	uint8_t h_rev = 0;
	uint8_t s_rev = 0;

	if(atomic_inc_return(&wlchg_rewrite_atomic) != 1)
	{
		atomic_dec(&wlchg_rewrite_atomic);
		return -EBUSY;
	}
	
	if(size % WLCHG_FLASH_DATA_SIZE != 0)
	{
		ret = -EINVAL;
		SHBATT_ERROR("file format error.ret = %d(%s, %d)\n",ret, __FUNCTION__, __LINE__);
		goto error;
	}

	ret = wlchg_ic_power_on();
	if(ret != 0)
	{
		SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
		goto error;
	}

	result = wlchg_boot_bootloader();
	if(result != 0)
	{
		SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
		goto error;
	}

	result = wlchg_wait_for_ready();
	if(result != 0)
	{
		SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
		goto end;
	}

	{
		const u8 *p;
		const u8 *pE;
		wlchg_flash_data_t wr_data;
		uint16_t start_address = 0x0C00;

		result = wlchg_set_write_address(start_address);
		if(result != 0)
		{
			SHBATT_ERROR("result = %d (%s, %d)\n", result, __FUNCTION__, __LINE__);
			goto end;
		}
		result = wlchg_wait_for_ready();
		if(result != 0)
		{
			SHBATT_ERROR("result = %d (%s, %d)\n", result, __FUNCTION__, __LINE__);
			goto end;
		}
		
		p	= data;
		pE	= p + size;
		
		while(p < pE)
		{
			memcpy(wr_data.buf, p, WLCHG_FLASH_DATA_SIZE);

#ifndef WLCHG_BOOTLOADER_V1_0
			wlchg_append_sum(&wr_data);
#endif	/* WLCHG_BOOTLOADER_V1_0 */

			result = wlchg_write_flash(&wr_data);
			if(result != 0)
			{
				SHBATT_ERROR("resultt = %d (%s, %d, 0x%x)\n",result, __FUNCTION__, __LINE__, (unsigned int)p);
				goto end;
			}

			result = wlchg_wait_for_ready();
			if(result != 0)
			{
				SHBATT_ERROR("ret = %d (%s, %d, 0x%x)\n", result, __FUNCTION__, __LINE__, (unsigned int)p);
				goto end;
			}
			p += 16;
		}
	}
end:
	ret = wlchg_finish_rewrite(&h_rev, &s_rev);
	SHBATT_ERROR("ret = %d,h_rev = 0x%02x, s_rev = 0x%02x (%s, %d)\n", ret, (int)h_rev, (int)s_rev, __FUNCTION__, __LINE__);
	ret = wlchg_wait_for_ready();
	if(ret != 0)
	{
		SHBATT_ERROR("ret = %d (%s, %d)\n", ret, __FUNCTION__, __LINE__);
	}
error:
	wlchg_ic_power_off();
	atomic_dec(&wlchg_rewrite_atomic);
	return result;
}

#ifdef WLCHG_DO_VERIFY
static int wlchg_verify(const u8 *data, size_t size)
{
	int ret;
	uint8_t status;
	uint8_t h_rev=0,s_rev=0;

	if(ret != 0)
	{
		SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
		return ret;
	}

	ret = wlchg_boot_bootloader();
	if(ret != 0)
	{
		SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
		goto error;
	}
	
	ret = wlchg_wait_for_ready();
	if(ret != 0)
	{
		SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
		goto end;
	}

	{
		wlchg_flash_data_t rd_data;
		const u8 *p;
		const u8 *pE;

		uint16_t start_address = 0x0C00;

	
		ret = wlchg_set_write_address(start_address);
		if(ret != 0)
		{
			SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
			goto end;
		}
		ret = wlchg_wait_for_ready();
		if(ret != 0)
		{
			SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
			goto end;
		}
		
		p	= data;
		pE	= p + size;
	
		while(p < pE)
		{
			ret = wlchg_read_flash(&rd_data);
			if(ret != 0)
			{
				SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
				return ret;
			}
			ret = wlchg_wait_for_ready();
			if(ret != 0)
			{
				SHBATT_ERROR("ret = %d (%s, %d)\n",ret , __FUNCTION__, __LINE__);
				continue;
			}

			ret = memcmp(rd_data.buf, p, WLCHG_FLASH_DATA_SIZE);
			if(ret != 0)
			{
				ret = -EINVAL;
				SHBATT_ERROR("ret = %d position = %08x(%s, %d)\n",ret, (int)(p - data), __FUNCTION__, __LINE__);
				goto end;
			}
			p += WLCHG_FLASH_DATA_SIZE;
		}
	}
end:
	ret = wlchg_finish_rewrite(&h_rev, &s_rev);	
	if(ret != 0)
	{
		SHBATT_ERROR("ret =%d(%s, %d)\n",ret , __FUNCTION__, __LINE__);
	}
	ret = status = wlchg_wait_for_ready();
	SHBATT_ERROR("ret =%d, h_rev=0x%02x s_rev=0x%02x(%s, %d)\n",ret , (int)h_rev, (int)s_rev,  __FUNCTION__, __LINE__);
error:
	return ret;
}
#endif	/* WLCHG_DO_VERIFY */

static int wlchg_firmware_update(void)
{
	const struct firmware* fw;
	int ret;

	ret = request_firmware(&fw, WLCHG_FIRMWARE_NAME, shbatt_dev_p);
	if(ret != 0)
	{
		SHBATT_ERROR("ret = %d(%s, %d)\n",ret, __FUNCTION__, __LINE__);
		return ret;
	}

	ret = wlchg_firmware_update_from_buffer(fw->data, fw->size);

	if(ret == 0)
	{
#ifdef WLCHG_DO_VERIFY
		ret = wlchg_verify(fw->data, fw->size);
#endif	/* WLCHG_DO_VERIFY */
	}
	release_firmware(fw);
	return ret;
}

#define SHBATT_WIRELESS_CHRARGER_FIRMWARE_BATT_TEMP_REG		0x44
static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_set_batt_temp( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int count;
	uint8_t batt_temp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	if(copy_from_user(&batt_temp,(uint8_t __user *)arg, sizeof(uint8_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
	
	for(count = 0; count < 10; count++)
	{
		SHBATT_TRACE("[P] %s batt_temp=%d \n",__FUNCTION__, batt_temp);
		ret = wlchg_i2c_write_byte(SHBATT_WIRELESS_CHRARGER_FIRMWARE_BATT_TEMP_REG, batt_temp);
		if(ret == 0)
		{
			SHBATT_TRACE("[P] %s batt_temp=%d \n",__FUNCTION__, batt_temp);
			break;
		}
	}
	
error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return ret;
}

static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_batt_temp( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int count;
	uint8_t batt_temp;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	for(count = 0; count < 10; count++)
	{
		ret = wlchg_i2c_read_byte(SHBATT_WIRELESS_CHRARGER_FIRMWARE_BATT_TEMP_REG, &batt_temp);
		if(ret == 0)
		{
			SHBATT_TRACE("[P] %s batt_temp=%d \n",__FUNCTION__, batt_temp);
			if(copy_to_user((uint8_t __user *)arg, &batt_temp, sizeof(uint8_t)) != 0)
			{
				SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
				ret = -EPERM;
				goto error;
			}
			break;
		}
	}
	
error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return ret;
}

#define SHBATT_WIRELESS_CHRARGER_FIRMWARE_CPU_TEMP_REG		0x45
static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_set_cpu_temp( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int count;
	uint8_t cpu_temp;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	if(copy_from_user(&cpu_temp,(uint8_t __user *)arg, sizeof(uint8_t)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
	
	for(count = 0; count < 10; count++)
	{
		SHBATT_TRACE("[P] %s cpu_temp=%d \n",__FUNCTION__, cpu_temp);
		ret = wlchg_i2c_write_byte(SHBATT_WIRELESS_CHRARGER_FIRMWARE_CPU_TEMP_REG, cpu_temp);
		if(ret == 0)
		{
			SHBATT_TRACE("[P] %s cpu_temp=%d \n",__FUNCTION__, cpu_temp);
			break;
		}
	}
	
error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return ret;
}

static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_cpu_temp( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int count;
	uint8_t cpu_temp;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	for(count = 0; count < 10; count++)
	{
		ret = wlchg_i2c_read_byte(SHBATT_WIRELESS_CHRARGER_FIRMWARE_CPU_TEMP_REG, &cpu_temp);
		if(ret == 0)
		{
			SHBATT_TRACE("[P] %s cpu_temp=%d \n",__FUNCTION__, cpu_temp);
			if(copy_to_user((uint8_t __user *)arg, &cpu_temp, sizeof(uint8_t)) != 0)
			{
				SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
				ret = -EPERM;
				goto error;
			}
			break;
		}
	}
	
error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return ret;
}

#define SHBATT_WIRELESS_CHRARGER_FIRMWARE_RECTIFIER_IC_TEMP_REG		0x46
static int shbatt_drv_ioctl_cmd_wireless_charger_firmware_get_rectifier_ic_temp( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int count;
	uint8_t rectifier_ic_temp;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	for(count = 0; count < 10; count++)
	{
		ret = wlchg_i2c_read_byte(SHBATT_WIRELESS_CHRARGER_FIRMWARE_RECTIFIER_IC_TEMP_REG, &rectifier_ic_temp);
		if(ret == 0)
		{
			SHBATT_TRACE("[P] %s rectifier_ic_temp=%d \n",__FUNCTION__, rectifier_ic_temp);
			if(copy_to_user((uint8_t __user *)arg, &rectifier_ic_temp, sizeof(uint8_t)) != 0)
			{
				SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
				ret = -EPERM;
				goto error;
			}
			break;
		}
	}
	
error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return ret;
}
#endif	/* CONFIG_PM_WIRELESS_CHARGE_IC_ACCESS_I2C */

static int shbatt_drv_ioctl_cmd_get_average_current( struct file* fi_p, unsigned long arg )
{
	int ret = 0;
	int average_current;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(shbatt_api_get_average_current( &average_current ) != SHBATT_RESULT_SUCCESS)
	{
		SHBATT_ERROR("%s : shbatt_api_get_average_current failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

	if(copy_to_user((int __user *)arg, &average_current, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

shbatt_result_t shbatt_api_get_average_current( int* average_current )
{
	shbatt_packet_t* pkt_p;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	do
	{
		if(shbatt_task_is_initialized == false)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		pkt_p = shbatt_task_get_packet();

		if(pkt_p == NULL)
		{
			result = SHBATT_RESULT_REJECTED;
			break;
		}

		mutex_lock(&shbatt_api_lock);

		INIT_COMPLETION(shbatt_api_cmp);

		SHBATT_WAKE_CTL(1);

		pkt_p->hdr.cmd	 = SHBATT_TASK_CMD_GET_AVERAGE_CURRENT;
		pkt_p->hdr.cb_p  = NULL;
		pkt_p->hdr.cmp_p = &shbatt_api_cmp;
		pkt_p->hdr.ret_p = &result;
		pkt_p->prm.cur_p = average_current;

		INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

		queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

		wait_for_completion_killable(&shbatt_api_cmp);

		mutex_unlock(&shbatt_api_lock);
	}
	while(0);
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static void shbatt_task_cmd_get_average_current( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;
	int average_current = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_get_average_current( &average_current );

	if(pkt_p->prm.cur_p != NULL)
	{
		*(pkt_p->prm.cur_p) = average_current;
	}

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static shbatt_result_t shbatt_seq_get_average_current( int* average_current )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_GET_AVERAGE_CURRENT;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = 0;

	result = shbatt_seq_call_user_space_sequence_executor();

	if(result == SHBATT_RESULT_SUCCESS)
	{
		*average_current = shbatt_usse_pkt.prm.cur;
SHBATT_TRACE("[P] %s average_current=%d \n",__FUNCTION__, *average_current);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return result;
}

static int shbatt_drv_ioctl_cmd_calib_ccadc( struct file* fi_p, unsigned long arg )
{
	int ret = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);
	
	pm8xxx_calib_ccadc();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_get_system_time( struct file* fi_p, unsigned long arg )
{
	int ret = 0;

	struct timeval tv;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	tv = ktime_to_timeval(ktime_get());

	if(copy_to_user((struct timeval*)arg,&tv,sizeof(struct timeval)) != 0)
	{
		SHBATT_ERROR("%s : copy_to_user failed.\n",__FUNCTION__);
		ret = -EPERM;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static int shbatt_drv_ioctl_cmd_reset_cc(struct file* fi_p, unsigned long arg)
{
	int rc;
	
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	rc = pm8921_bms_reset_cc();

	SHBATT_TRACE("[E] %s rc=%d \n",__FUNCTION__, rc);

	return 0;
}

static int shbatt_drv_ioctl_cmd_set_depleted_calc_enable( struct file* fi_p, unsigned long arg )
{
	int ret,val;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if(copy_from_user(&val,(int __user *)arg, sizeof(int)) != 0)
	{
		SHBATT_ERROR("%s : copy_from_user failed.\n",__FUNCTION__);
		ret = -EPERM;
		goto error;
	}
	else
	{
		ret = shbatt_api_set_depleted_calc_enable(val);
	}

error:
	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;
}

static shbatt_result_t shbatt_api_set_depleted_calc_enable( int val )
{
	shbatt_packet_t* pkt_p;

	SHBATT_TRACE("[S] %s val=%d \n",__FUNCTION__, val);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd = SHBATT_TASK_CMD_SET_DEPLETED_CALC_ENABLE;
	pkt_p->hdr.cb_p = NULL;
	pkt_p->hdr.cmp_p = NULL;
	pkt_p->hdr.ret_p = NULL;
	pkt_p->prm.val   = val;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

static void shbatt_task_cmd_set_depleted_calc_enable( shbatt_packet_t* pkt_p )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	result = shbatt_seq_set_depleted_calc_enable( pkt_p->prm.val );

	if(pkt_p->hdr.ret_p != NULL)
	{
		*(pkt_p->hdr.ret_p) = result;
	}

	if(pkt_p->hdr.cmp_p != NULL)
	{
		complete(pkt_p->hdr.cmp_p);
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

static shbatt_result_t shbatt_seq_set_depleted_calc_enable( int val )
{
	shbatt_result_t result;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	shbatt_usse_pkt.hdr.cmd = SHBATT_CMD_SET_DEPLETED_CALC_ENABLE;
	shbatt_usse_pkt.hdr.ret = SHBATT_RESULT_FAIL;
	shbatt_usse_pkt.prm.val = val;

	result = shbatt_seq_call_user_space_sequence_executor();

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	
	return SHBATT_RESULT_SUCCESS;
}
/*+-----------------------------------------------------------------------------+*/
/*| @ THIS FILE END :															|*/
/*+-----------------------------------------------------------------------------+*/

