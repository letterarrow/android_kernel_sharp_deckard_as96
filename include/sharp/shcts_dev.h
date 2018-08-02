/* include/sharp/shcts_dev.h
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
#ifndef __SHCTS_DEV_H__
#define __SHCTS_DEV_H__

/* -----------------------------------------------------------------------------------
 */
#define SHCTS_DEVNAME		"shcts_kerl"
#define SHCTS_IF_DEVNAME	"shctsif"
#define SHCTS_IF_DEVPATH 	"/dev/shctsif"

/* -----------------------------------------------------------------------------------
 */
#define SHCTS_REG_MAIN_CONTROL							0x00
#define SHCTS_REG_GENERAL_STATUS						0x02
#define SHCTS_REG_INPUT_STATUS							0x03
#define SHCTS_REG_LED_STATUS							0x04
#define SHCTS_REG_NOIZE_FLAG_STATUS					0x0A
#define SHCTS_REG_SENSOR_INPUT1_DALTA_COUNT			0x10
#define SHCTS_REG_SENSOR_INPUT2_DALTA_COUNT			0x11
#define SHCTS_REG_SENSOR_INPUT3_DALTA_COUNT			0x12
#define SHCTS_REG_SENSITIVITY_CONTROL					0x1F
#define SHCTS_REG_CONFIGURATION						0x20
#define SHCTS_REG_CONFIGURATION2						0x44
#define SHCTS_REG_SENSOR_INPUT_ENABLE					0x21
#define SHCTS_REG_SENSOR_INPUT_CONFIG					0x22
#define SHCTS_REG_SENSOR_INPUT_CONFIG2					0x23
#define SHCTS_REG_AVERAGING_AND_SAMPLING_CONFIGURATION	0x24
#define SHCTS_REG_CALIBRATION_ACTIVATE					0x26
#define SHCTS_REG_INTERRUPT_ENABLE						0x27
#define SHCTS_REG_REPEAT_RATE_ENABLE					0x28
#define SHCTS_REG_MULTIPLE_TOUCH_CONFIGURATION			0x2A
#define SHCTS_REG_MULTIPLE_TOUCH_PATTERN_CONFIGURATION	0x2B
#define SHCTS_REG_MULTIPLE_TOUCH_PATTERN				0x2D
#define SHCTS_REG_RECALIBRATION_CONFIGURATION			0x2F
#define SHCTS_REG_SENSOR_INPUT1_THRESHOLD				0x30
#define SHCTS_REG_SENSOR_INPUT2_THRESHOLD				0x31
#define SHCTS_REG_SENSOR_INPUT3_THRESHOLD				0x32
#define SHCTS_REG_SENSOR_INPUT_NOIZE_THRESHOLD			0x38
#define SHCTS_REG_STANDBY_CHANNEL						0x40
#define SHCTS_REG_STANDBY_CONFIGURATION				0x41
#define SHCTS_REG_STANDBY_SENSITIVITY					0x42
#define SHCTS_REG_STANDBY_THRESHOLD					0x43
#define SHCTS_REG_SENSOR_INPUT1_BASE_COUNT				0x50
#define SHCTS_REG_SENSOR_INPUT2_BASE_COUNT				0x51
#define SHCTS_REG_SENSOR_INPUT3_BASE_COUNT				0x52
#define SHCTS_REG_LED_OUTPUT_TYPE						0x71
#define SHCTS_REG_SENSOR_INPUT_LED_LINKING				0x72
#define SHCTS_REG_LED_POLARITY							0x73
#define SHCTS_REG_LED_OUTPUT_CONTROL					0x74
#define SHCTS_REG_LINKED_LED_TRANSITION_CONTROL		0x77
#define SHCTS_REG_LED_MIRROR_CONTROL					0x79
#define SHCTS_REG_LED_BEHAVIOR1						0x81
#define SHCTS_REG_LED_PULSE1_PERIOD					0x84
#define SHCTS_REG_LED_PULSE2_PERIOD					0x85
#define SHCTS_REG_LED_BREATHE_PERIOD					0x86
#define SHCTS_REG_LED_CONFIGURATION					0x88
#define SHCTS_REG_LED_PULSE1_DUTY_CYCLE				0x90
#define SHCTS_REG_LED_PULSE2_DUTY_CYCLE				0x91
#define SHCTS_REG_LED_BREATHE_DUTY_CYCLE				0x92
#define SHCTS_REG_DIRECT_DUTY_CYCLE					0x93
#define SHCTS_REG_LED_DIRECT_RAMP_RATES				0x94
#define SHCTS_REG_LED_OFF_DELAY						0x95
#define SHCTS_REG_SENSOR_INPUT1_CALIBRATION			0xB1
#define SHCTS_REG_SENSOR_INPUT2_CALIBRATION			0xB2
#define SHCTS_REG_SENSOR_INPUT3_CALIBRATION			0xB3
#define SHCTS_REG_SENSOR_INPUT_CALIBRATION_LSB1		0xB9
#define SHCTS_REG_PRODUCT_ID							0xFD
#define SHCTS_REG_MANUFACTURER_ID						0xFE
#define SHCTS_REG_REVISION								0xFF

/* -----------------------------------------------------------------------------------
 */
#define SHCTS_IOC_MAGIC					0xF0

#define SHCTS_DEV_ENABLE				_IO  ( SHCTS_IOC_MAGIC,   1)
#define SHCTS_DEV_DISABLE				_IO  ( SHCTS_IOC_MAGIC,   2)
#define SHCTS_DEV_GET_ID_PRODUCT		_IOR ( SHCTS_IOC_MAGIC,   3, unsigned char)
#define SHCTS_DEV_GET_ID_MANUFACT		_IOR ( SHCTS_IOC_MAGIC,   4, unsigned char)
#define SHCTS_DEV_GET_ID_REVISION		_IOR ( SHCTS_IOC_MAGIC,   5, unsigned char)
#define SHCTS_DEV_GET_DEV_STATE			_IOR ( SHCTS_IOC_MAGIC,   6, unsigned char)
#define SHCTS_DEV_SET_DEV_STATE			_IOW ( SHCTS_IOC_MAGIC,   7, unsigned char)
#define SHCTS_DEV_GET_TOUCH_INFO		_IOR ( SHCTS_IOC_MAGIC,   8, struct shcts_touch_key_info)
#define SHCTS_DEV_SET_LED_STATE			_IOW ( SHCTS_IOC_MAGIC,   9, unsigned char)
#define SHCTS_DEV_SET_CALIBRATION_ACTIVATE	_IOW ( SHCTS_IOC_MAGIC,   10, struct shcts_ioctl_param)
#define SHCTS_DEV_SET_INPUT_THRESHOLD	_IOW ( SHCTS_IOC_MAGIC,   11, struct shcts_ioctl_param)
#define SHCTS_DEV_GET_INPUT_THRESHOLD	_IOW ( SHCTS_IOC_MAGIC,   12, struct shcts_ioctl_param)
#define SHCTS_DEV_SET_PARAM				_IOW ( SHCTS_IOC_MAGIC,   13, struct shcts_ioctl_param)
#define SHCTS_DEV_GET_PARAM				_IOW ( SHCTS_IOC_MAGIC,   14, struct shcts_ioctl_param)
#define SHCTS_DEV_SET_LED_DUTY			_IOW ( SHCTS_IOC_MAGIC,   15, struct shcts_ioctl_param)
#define SHCTS_DEV_GET_LED_DUTY			_IOW ( SHCTS_IOC_MAGIC,   16, struct shcts_ioctl_param)

#define SHCTS_DEV_LOGOUTPUT_ENABLE		_IOW ( SHCTS_IOC_MAGIC, 100, int)
#define SHCTS_DEV_REG_WRTIE				_IOW ( SHCTS_IOC_MAGIC, 101, struct shcts_ioctl_param)
#define SHCTS_DEV_REG_READ				_IOR ( SHCTS_IOC_MAGIC, 102, struct shcts_ioctl_param)
#define SHCTS_DEV_REG_READ_ALL			_IOR ( SHCTS_IOC_MAGIC, 103, struct shcts_ioctl_param)

/* -----------------------------------------------------------------------------------
 */
struct shcts_ioctl_param {
	int				size;
	unsigned char*	data;
};

struct shcts_touch_key_info {
	unsigned char		menu_key_state;
	unsigned char		home_key_state;
	unsigned char		back_key_state;

	signed char			menu_key_delta;
	signed char			home_key_delta;
	signed char			back_key_delta;
};

/* -----------------------------------------------------------------------------------
 */
extern void shcts_set_state_active(void);
extern void shcts_set_state_standby(void);
extern void shcts_set_state_deep_sleep(void);

/* -----------------------------------------------------------------------------------
 */

#endif /* __SHCTS_DEV_H__ */
