/* drivers/sharp/shcts/shcts_kerl.c
 *
 * Copyright (c) 2012, Sharp. All rights reserved.
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

/* -----------------------------------------------------------------------------------
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/leds.h>

#include <linux/i2c.h>
#include <linux/mfd/pm8xxx/pm8921.h>

#include <sharp/shcts_dev.h>

/* -----------------------------------------------------------------------------------
 */
#define SHCTS_DEVELOP_MODE_ENABLE

#if defined(SHCTS_DEVELOP_MODE_ENABLE)
	#define SHCTS_LOG_DEBUG_ENABLE
	#define SHCTS_MODULE_PARAM_ENABLE
	#define SHCTS_DEBUG_CREATE_KOBJ_ENABLE
#endif /* SHCTS_DEVELOP_MODE_ENABLE */

#define SHCTS_LOG_ERROR_ENABLE
#define SHCTS_LOG_EVENT_ENABLE

/* -----------------------------------------------------------------------------------
 */
#if defined( SHCTS_LOG_ERROR_ENABLE )
	#define SHCTS_LOG_ERR_PRINT(...)	printk(KERN_ERR "[shcts] " __VA_ARGS__)
#else
	#define SHCTS_LOG_ERR_PRINT(...)
#endif /* defined( SHCTS_LOG_ERROR_ENABLE ) */

#if defined( SHCTS_LOG_DEBUG_ENABLE )
	#define SHCTS_LOG_DBG_PRINT(...)	\
		if(LogOutputEnable & (1 << 0))		printk(KERN_DEBUG "[shcts] " __VA_ARGS__)
#else
	#define	SHCTS_LOG_DBG_PRINT(...)
#endif /* defined( SHCTS_LOG_DEBUG_ENABLE ) */

#if defined( SHCTS_LOG_EVENT_ENABLE )
	#define SHCTS_LOG_EVT_PRINT(...)	\
		if(LogOutputEnable & (1 << 1))		printk(KERN_DEBUG "shcts: " __VA_ARGS__)
//		if(LogOutputEnable & (1 << 1))		printk(KERN_DEBUG "[shcts] " __VA_ARGS__)
#else
	#define SHCTS_LOG_EVT_PRINT(...)
#endif /* defined( SHCTS_LOG_EVENT_ENABLE ) */

/* -----------------------------------------------------------------------------------
 */
#define PM8921_GPIO_BASE					NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)		(pm_gpio - 1 + PM8921_GPIO_BASE)
#define PM8921_IRQ_BASE						(NR_MSM_IRQS + NR_GPIO_IRQS)
#define PM8921_GPIO_PM_TO_SYS_IRQ(pm_gpio)	(pm_gpio - 1 + NR_PM8921_IRQS)

#define SHCTS_GPIO_INT						PM8921_GPIO_PM_TO_SYS(33)

/* -----------------------------------------------------------------------------------
 */
#define SHCTS_DEV_ACCESS_TRY_COUNT_MAX	3

/* -----------------------------------------------------------------------------------
 */
enum{
	SHCTS_IRQ_WAKE_DISABLE = 0,
	SHCTS_IRQ_WAKE_ENABLE,
};

enum{
	SHCTS_IRQ_STATE_DISABLE = 0,
	SHCTS_IRQ_STATE_ENABLE,
};

enum{
	SHCTS_EVENT_STOP = 0,
	SHCTS_EVENT_START,
	SHCTS_EVENT_SLEEP,
	SHCTS_EVENT_WAKEUP,
	SHCTS_EVENT_DEEP_SLEEP,
	SHCTS_EVENT_INTERRUPT,
	SHCTS_EVENT_TIMEOUT,
};

enum{
	SHCTS_DRV_STATE_IDLE = 0,
	SHCTS_DRV_STATE_ACTIVE,
	SHCTS_DRV_STATE_SLEEP,
	SHCTS_DRV_STATE_DEEP_SLEEP,
};

enum{
	SHCTS_DEV_STATE_ACTIVE = 0,
	SHCTS_DEV_STATE_STANDBY,
	SHCTS_DEV_STATE_DEEP_SLEEP,
};

enum{
	SHCTS_KEY_MENU = 0,
	SHCTS_KEY_HOME,
	SHCTS_KEY_BACK,
	SHCTS_KEY_NUM,
};

enum{
	SHCTS_LED_1 = 0x01,
	SHCTS_LED_2 = 0x02,
	SHCTS_LED_3 = 0x04,

	SHCTS_LED_ALL = (SHCTS_LED_1 | SHCTS_LED_2 | SHCTS_LED_3),
};

/* -----------------------------------------------------------------------------------
 */
struct shcts_irq_info{
	int							irq;
	u8							state;
	u8							wake;
};

struct shcts_state_info{
	int							state;
	int							mode;
	int							starterr;
	unsigned long				starttime;
};

struct shcts_smsc{
	struct i2c_client			*i2c_client;
	struct input_dev			*input_key;
	struct shcts_irq_info		irq_mgr;
	struct shcts_state_info		state_mgr;
	u8							report_key_state;

	#if defined(SHCTS_DEBUG_CREATE_KOBJ_ENABLE)
		struct kobject				*kobj;
	#endif /* SHCTS_DEBUG_CREATE_KOBJ_ENABLE */
};

/* -----------------------------------------------------------------------------------
 */
static DEFINE_MUTEX(shcts_ctrl_lock);

/* -----------------------------------------------------------------------------------
 */
static dev_t 				shctsif_devid;
static struct class			*shctsif_class;
static struct device		*shctsif_device;
struct cdev 				shctsif_cdev;
static struct shcts_smsc	*gShcts_smsc = NULL;

#if defined( SHCTS_LOG_DEBUG_ENABLE ) || defined(SHCTS_LOG_EVENT_ENABLE)
	static int LogOutputEnable = 0x02;
	module_param(LogOutputEnable, int, S_IRUGO | S_IWUSR);
#endif /* SHCTS_LOG_DEBUG_ENABLE || SHCTS_LOG_EVENT_ENABLE */

/* -----------------------------------------------------------------------------------
 */
static int shcts_reg_write(struct shcts_smsc *cts, u8 addr, u8 data);
static int shcts_reg_read(struct shcts_smsc *cts, u8 addr, u8 *buf, u32 size);

static int shcts_set_standby(struct shcts_smsc *cts);
static int shcts_set_active(struct shcts_smsc *cts);
static int shcts_set_deep_sleep(struct shcts_smsc *cts);
static int shcts_init_param(struct shcts_smsc *cts);

static void shcts_key_event_report(struct shcts_smsc *cts, u8 state);
static void shcts_key_event_force_touchup(struct shcts_smsc *cts);
static void shcts_read_touchevent(struct shcts_smsc *cts, int state);

static int request_event(struct shcts_smsc *cts, int event, int param);
static int state_change(struct shcts_smsc *cts, int state);

static void shcts_mutex_lock_enable(void);
static void shcts_mutex_lock_disable(void);

static irqreturn_t shcts_irq(int irq, void *dev_id);
static int shcts_irq_clear(struct shcts_smsc *cts);
static void shcts_irq_disable(struct shcts_smsc *cts);
static void shcts_irq_enable(struct shcts_smsc *cts);
static int shcts_irq_resuest(struct shcts_smsc *cts);

static void shcts_device_reg_init(struct shcts_smsc *cts);
/* -----------------------------------------------------------------------------------
 */
static int shcts_statef_nop(struct shcts_smsc *cts, int param);
static int shcts_statef_cmn_error(struct shcts_smsc *cts, int param);
static int shcts_statef_cmn_stop(struct shcts_smsc *cts, int param);
static int shcts_statef_cmn_sleep(struct shcts_smsc *cts, int param);
static int shcts_statef_cmn_deep_sleep(struct shcts_smsc *cts, int param);

static int shcts_statef_idle_start(struct shcts_smsc *cts, int param);
static int shcts_statef_idle_int(struct shcts_smsc *cts, int param);

static int shcts_statef_active_enter(struct shcts_smsc *cts, int param);
static int shcts_statef_active_stop(struct shcts_smsc *cts, int param);
static int shcts_statef_active_sleep(struct shcts_smsc *cts, int param);
static int shcts_statef_active_deep_sleep(struct shcts_smsc *cts, int param);
static int shcts_statef_active_int(struct shcts_smsc *cts, int param);

static int shcts_statef_sleep_enter(struct shcts_smsc *cts, int param);
static int shcts_statef_sleep_wakeup(struct shcts_smsc *cts, int param);
static int shcts_statef_sleep_deep_sleep(struct shcts_smsc *cts, int param);
static int shcts_statef_sleep_int(struct shcts_smsc *cts, int param);

static int shcts_statef_deep_sleep_enter(struct shcts_smsc *cts, int param);
static int shcts_statef_deep_sleep_sleep(struct shcts_smsc *cts, int param);
static int shcts_statef_deep_sleep_wakeup(struct shcts_smsc *cts, int param);
static int shcts_statef_deep_sleep_int(struct shcts_smsc *cts, int param);

/* -----------------------------------------------------------------------------------
 */
typedef int (shcts_state_func)(struct shcts_smsc *cts, int param);
struct shcts_state_func{
	shcts_state_func	*enter;
	shcts_state_func	*stop;
	shcts_state_func	*start;
	shcts_state_func	*sleep;
	shcts_state_func	*wakeup;
	shcts_state_func	*deep_sleep;
	shcts_state_func	*interrupt;
	shcts_state_func	*timeout;
};

const static struct shcts_state_func state_idle = {
	.enter			= shcts_statef_nop,
	.stop			= shcts_statef_nop,
	.start			= shcts_statef_idle_start,
	.sleep			= shcts_statef_cmn_sleep,
	.wakeup			= shcts_statef_nop,
	.deep_sleep		= shcts_statef_cmn_deep_sleep,
	.interrupt		= shcts_statef_idle_int,
	.timeout		= shcts_statef_cmn_error,
};

const static struct shcts_state_func state_active = {
	.enter			= shcts_statef_active_enter,
	.stop			= shcts_statef_active_stop,
	.start			= shcts_statef_nop,
	.sleep			= shcts_statef_active_sleep,
	.wakeup			= shcts_statef_nop,
	.deep_sleep		= shcts_statef_active_deep_sleep,
	.interrupt		= shcts_statef_active_int,
	.timeout		= shcts_statef_cmn_error,
};

const static struct shcts_state_func state_sleep = {
	.enter			= shcts_statef_sleep_enter,
	.stop			= shcts_statef_cmn_stop,
	.start			= shcts_statef_nop,
	.sleep			= shcts_statef_nop,
	.wakeup			= shcts_statef_sleep_wakeup,
	.deep_sleep		= shcts_statef_sleep_deep_sleep,
	.interrupt		= shcts_statef_sleep_int,
	.timeout		= shcts_statef_cmn_error,
};

const static struct shcts_state_func state_deep_sleep = {
	.enter			= shcts_statef_deep_sleep_enter,
	.stop			= shcts_statef_cmn_stop,
	.start			= shcts_statef_nop,
	.sleep			= shcts_statef_deep_sleep_sleep,
	.wakeup			= shcts_statef_deep_sleep_wakeup,
	.deep_sleep		= shcts_statef_nop,
	.interrupt		= shcts_statef_deep_sleep_int,
	.timeout		= shcts_statef_cmn_error,
};

const static struct shcts_state_func *state_func_tbl[] = {
	&state_idle,
	&state_active,
	&state_sleep,
	&state_deep_sleep,
};

/* -----------------------------------------------------------------------------------
 */
#if defined(SHCTS_DEBUG_CREATE_KOBJ_ENABLE)
static ssize_t store_reg_write(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct shcts_smsc *cts = gShcts_smsc;
	int addr;
	int data;
	int ret = 0;

	sscanf(buf,"%X %X", &addr, &data);

	shcts_mutex_lock_enable();
	ret = shcts_reg_write(cts, addr, data);
	shcts_mutex_lock_disable();

	if(ret == 0){
		SHCTS_LOG_ERR_PRINT("%s() done [Addr:0x%02X] [Data:0x%02X]\n", __func__, addr, data);
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error [Addr:0x%02X] [Data:0x%02X]\n", __func__, addr, data);
	}

	return count;
}

static ssize_t store_reg_read(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct shcts_smsc *cts = gShcts_smsc;
	int addr;
	u8 reg_data[0x100];
	int size = 0;
	int i;
	int ret = 0;

	sscanf(buf,"%X %X", &addr, &size);

	if(size > 0x100){
		return count;
	}

	shcts_mutex_lock_enable();
	ret = shcts_reg_read(cts, addr, reg_data, size);
	shcts_mutex_lock_disable();

	if(ret == 0){
		for(i = 0; i < size; i++){
			SHCTS_LOG_ERR_PRINT("%s() [Addr:0x%02X] [Data:0x%02X]\n", __func__, (addr + i), reg_data[i]);
		}
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error [Addr:0x%02X] [Size:0x%02X]\n", __func__, addr, size);
	}

	return count;
}

static ssize_t show_drv_state(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct shcts_smsc *cts = gShcts_smsc;

	return snprintf(buf, PAGE_SIZE,
			"DrvState : %s\n",
			cts->state_mgr.state == SHCTS_DRV_STATE_IDLE ? "Idle" :
			cts->state_mgr.state == SHCTS_DRV_STATE_ACTIVE ? "Active" :
			cts->state_mgr.state == SHCTS_DRV_STATE_SLEEP ? "Sleep" :
			cts->state_mgr.state == SHCTS_DRV_STATE_DEEP_SLEEP ? "DeepSleep" : "Unknown");
}

static ssize_t store_drv_state(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct shcts_smsc *cts = gShcts_smsc;
	int state = SHCTS_DRV_STATE_IDLE;

	sscanf(buf,"%d", &state);

	shcts_mutex_lock_enable();
	state_change(cts, state);
	shcts_mutex_lock_disable();

	return count;
}

static ssize_t store_drv_event(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct shcts_smsc *cts = gShcts_smsc;
	int event = SHCTS_EVENT_STOP;

	sscanf(buf,"%d", &event);

	request_event(cts, event, 0);

	return count;
}

static struct kobj_attribute reg_write = 
	__ATTR(reg_write, S_IRUSR | S_IWUSR, NULL, store_reg_write);
static struct kobj_attribute reg_read = 
	__ATTR(reg_read, S_IRUSR | S_IWUSR, NULL, store_reg_read);

static struct kobj_attribute drv_state = 
	__ATTR(drv_state, S_IRUSR | S_IWUSR, show_drv_state, store_drv_state);
static struct kobj_attribute drv_event = 
	__ATTR(drv_event, S_IRUSR | S_IWUSR, NULL, store_drv_event);

static struct attribute *attrs_ctrl[] = {
	&reg_write.attr,
	&reg_read.attr,
	NULL
};

static struct attribute *attrs_debug[] = {
	&drv_state.attr,
	&drv_event.attr,
	NULL
};

static struct attribute_group attr_grp_ctrl = {
	.name = "ctrl",
	.attrs = attrs_ctrl,
};

static struct attribute_group attr_grp_debug = {
	.name = "debug",
	.attrs = attrs_debug,
};
#endif /* SHCTS_DEBUG_CREATE_KOBJ_ENABLE */

/* -----------------------------------------------------------------------------------
 */
static int shcts_reg_write(struct shcts_smsc *cts, u8 addr, u8 data)
{
	int i;
	int ret = 0;
	u8 buf[2];
	struct i2c_msg msg;

	msg.addr  = cts->i2c_client->addr;
	msg.flags = 0;
	msg.buf   = buf;
	msg.len   = 2;

	buf[0] = addr;
	buf[1] = data;

	for(i = 0; i < SHCTS_DEV_ACCESS_TRY_COUNT_MAX; i++){
		ret = i2c_transfer(cts->i2c_client->adapter, &msg, 1);
		if(ret >= 0){
			break;
		}
	}

	if(ret < 0){
		SHCTS_LOG_ERR_PRINT("reg write error [Addr:0x%02X][Data:0x%02X]\n", addr, data);
	}else{
		ret = 0;
	}

	return ret;
}

static int shcts_reg_read(struct shcts_smsc *cts, u8 addr, u8 *buf, u32 size)
{
	int i;
	int ret = 0;
	struct i2c_msg msg[2];

	msg[0].addr  = cts->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].buf   = &addr;
	msg[0].len   = 1;

	msg[1].addr  = cts->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf   = buf;
	msg[1].len   = size;

	for(i = 0; i < SHCTS_DEV_ACCESS_TRY_COUNT_MAX; i++){
		ret = i2c_transfer(cts->i2c_client->adapter, msg, 2);
		if(ret >= 0){
			break;
		}
	}

	if(ret < 0){
		SHCTS_LOG_ERR_PRINT("reg read error [Addr:0x%02X][Size:0x%02X]\n", addr, size);
	}else{
		ret = 0;
	}

	return ret;
}

/* -----------------------------------------------------------------------------------
 */
static int shcts_set_standby(struct shcts_smsc *cts)
{
	u8 data;
	int ret = 0;

	ret = shcts_reg_read(cts, SHCTS_REG_MAIN_CONTROL, &data, 1);
	if(ret != 0){
		return ret;
	}

	data &= (~0x30);
	data |= 0x20;

	ret = shcts_reg_write(cts, SHCTS_REG_MAIN_CONTROL, data);

	return ret;
}

static int shcts_set_active(struct shcts_smsc *cts)
{
	u8 data;
	int ret = 0;

	ret = shcts_reg_read(cts, SHCTS_REG_MAIN_CONTROL, &data, 1);
	if(ret != 0){
		return ret;
	}

	data &= (~0x30);

	ret = shcts_reg_write(cts, SHCTS_REG_MAIN_CONTROL, data);

	return ret;
}

static int shcts_set_deep_sleep(struct shcts_smsc *cts)
{
	u8 data;
	int ret = 0;

	ret = shcts_reg_read(cts, SHCTS_REG_MAIN_CONTROL, &data, 1);
	if(ret != 0){
		return ret;
	}

	data &= (~0x30);
	data |= 0x10;

	ret = shcts_reg_write(cts, SHCTS_REG_MAIN_CONTROL, data);

	return ret;
}

static int shcts_get_dev_state(struct shcts_smsc *cts)
{
	int ret = 0;
	u8 state = SHCTS_DEV_STATE_STANDBY;

	ret = shcts_reg_read(cts, SHCTS_REG_MAIN_CONTROL, &state, 1);
	if(ret == 0){
		if( (state & 0x10) != 0 ){
			ret = SHCTS_DEV_STATE_DEEP_SLEEP;
		}else if( (state & 0x20) != 0 ){
			ret = SHCTS_DEV_STATE_STANDBY;
		}else{
			ret = SHCTS_DEV_STATE_ACTIVE;
		}
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		ret = -1;
	}

	return ret;
}

static int shcts_set_led_output_enable(struct shcts_smsc *cts, u8 led)
{
	int ret = 0;
	u8 state = 0;

	ret = shcts_reg_read(cts, SHCTS_REG_LED_OUTPUT_CONTROL, &state, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() get led output state error\n", __func__);
		return ret;
	}
	state |= led;
	ret = shcts_reg_write(cts, SHCTS_REG_LED_OUTPUT_CONTROL, state);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() set led output state error\n", __func__);
	}

	return ret;
}

static int shcts_set_led_output_disable(struct shcts_smsc *cts, u8 led)
{
	int ret = 0;
	u8 state = 0;

	ret = shcts_reg_read(cts, SHCTS_REG_LED_OUTPUT_CONTROL, &state, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() get led output state error\n", __func__);
		return ret;
	}
	state &= (~led);
	ret = shcts_reg_write(cts, SHCTS_REG_LED_OUTPUT_CONTROL, state);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() set led output state error\n", __func__);
	}

	return ret;
}

static int shcts_set_calibration_activate(struct shcts_smsc *cts,u8 select)
{
	u8 data;
	int ret = 0;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	ret = shcts_reg_read(cts, SHCTS_REG_CALIBRATION_ACTIVATE, &data, 1);
	if(ret != 0){
		return ret;
	}

	if(select == 0){
		data |= 0x01;
	}else if(select == 1){
		data |= 0x02;
	}else if(select == 2){
		data |= 0x04;
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}
	ret = shcts_reg_write(cts, SHCTS_REG_CALIBRATION_ACTIVATE, data);

	return ret;
}

static int shcts_set_input_threshold(struct shcts_smsc *cts, u8 select, u8 param2)
{
	int ret = 0;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(select == 0){
		ret = shcts_reg_write(cts, SHCTS_REG_SENSOR_INPUT1_THRESHOLD, param2);
	}else if(select == 1){
		ret = shcts_reg_write(cts, SHCTS_REG_SENSOR_INPUT2_THRESHOLD, param2);
	}else if(select == 2){
		ret = shcts_reg_write(cts, SHCTS_REG_SENSOR_INPUT3_THRESHOLD, param2);
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return ret;
}

static int shcts_get_input_threshold(struct shcts_smsc *cts, u8 select)
{
	int ret = 0;
	u8 data;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(select == 0){
		ret = shcts_reg_read(cts, SHCTS_REG_SENSOR_INPUT1_THRESHOLD, &data, 1);
	}else if(select == 1){
		ret = shcts_reg_read(cts, SHCTS_REG_SENSOR_INPUT2_THRESHOLD, &data, 1);
	}else if(select == 2){
		ret = shcts_reg_read(cts, SHCTS_REG_SENSOR_INPUT3_THRESHOLD, &data, 1);
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return data;
}

static int shcts_set_param(struct shcts_smsc *cts, u8 select, u8 param2, u8 param3)
{
	int ret = 0;
	u8 data;
	int write_reg = SHCTS_REG_MAIN_CONTROL;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(select == 0){
		if(param2 == 0){
			write_reg = SHCTS_REG_MAIN_CONTROL;
		}else if(param2 == 1){
			write_reg = SHCTS_REG_SENSITIVITY_CONTROL;
		}else if((param2 == 2) || (param2 == 3) || (param2 == 4) ){
			write_reg = SHCTS_REG_AVERAGING_AND_SAMPLING_CONFIGURATION;
		}else{
			SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
			return -EFAULT;
		}
	}else if(select == 1){
		if(param2 == 0){
			write_reg = SHCTS_REG_MAIN_CONTROL;
		}else if(param2 == 1){
			write_reg = SHCTS_REG_STANDBY_SENSITIVITY;
		}else if((param2 == 2) || (param2 == 3) || (param2 == 4) ){
			write_reg = SHCTS_REG_STANDBY_CONFIGURATION;
		}else{
			SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
			return -EFAULT;
		}
	}else{
			SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
			return -EFAULT;
	}
	
	ret = shcts_reg_read(cts, write_reg, &data, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	if(param2 == 0){
		data = data & 0x3F;
		data = data | (param3 << 6);
	}else if(param2 == 1){
		if(select == 0){
			data = data & 0x8F;
			data = data | ((param3 & 0x07) << 4);
		}else if(select == 1){
			data = data & 0xFC;
			data = data | param3;
		}
	}else if(param2 == 2){
		data = data & 0x8F;
		data = data | (param3 << 4);
	}else if(param2 == 3){
		data = data & 0xF3;
		data = data | (param3 << 2);
	}else if(param2 == 4){
		data = data & 0xF8;
		data = data | param3;
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	ret = shcts_reg_write(cts, write_reg, data);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return ret;
}

static int shcts_get_param(struct shcts_smsc *cts, u8 select, u8 param2)
{
	int ret = 0;
	u8 data;
	int write_reg = SHCTS_REG_MAIN_CONTROL;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(select == 0){
		if(param2 == 0){
			write_reg = SHCTS_REG_MAIN_CONTROL;
		}else if(param2 == 1){
			write_reg = SHCTS_REG_SENSITIVITY_CONTROL;
		}else if((param2 == 2) || (param2 == 3) || (param2 == 4) ){
			write_reg = SHCTS_REG_AVERAGING_AND_SAMPLING_CONFIGURATION;
		}else{
			SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
			return -EFAULT;
		}
	}else if(select == 1){
		if(param2 == 0){
			write_reg = SHCTS_REG_MAIN_CONTROL;
		}else if(param2 == 1){
			write_reg = SHCTS_REG_STANDBY_SENSITIVITY;
		}else if((param2 == 2) || (param2 == 3) || (param2 == 4) ){
			write_reg = SHCTS_REG_STANDBY_CONFIGURATION;
		}else{
			SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
			return -EFAULT;
		}
	}else{
			SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
			return -EFAULT;
	}
	ret = shcts_reg_read(cts, write_reg, &data, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	if(param2 == 0){
		data = data & 0xC0;
		data = data >> 6;
	}else if(param2 == 1){
		if(select == 0){
			data = data >>4;
			data = data & 0x07;
		}else if(select == 1){
			data = data & 0x07;
		}
	}else if(param2 == 2){
		data = data & 0x70;
		data = data >>4;
	}else if(param2 == 3){
		data = data & 0x0C;
		data = data >>2;
	}else if(param2 == 4){
		data = data & 0x03;
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}
	return data;
}

static int shcts_led_duty_decode_read(u8 select,u8 param2)
{
	int ret = 0;
	int duty_cycle_read[17] = {0,7,9,11,14,17,20,23,26,30,35,40,46,53,63,77,100};
	
	if(select == 0){
		ret = duty_cycle_read[param2];
	}else if(select == 1){
		ret = duty_cycle_read[param2+1];
	}
	return ret;
}

static int shcts_led_duty_decode_write(u8 select,u8 param2)
{
	int ret;
	int  i;
	int duty_cycle;
	int duty_cycle_read[17] = {0,7,9,11,14,17,20,23,26,30,35,40,46,53,63,77,100};

	ret = 0;
	duty_cycle = 15;
	duty_cycle = duty_cycle + select;

	if(select == 1){
		if(param2 < duty_cycle_read[1]){
			param2 = duty_cycle_read[1];
		}
	}

	for(i = select; i <= duty_cycle; i++){
		if(param2 <= (duty_cycle_read[i]-1)){
			ret++;
		}
	}
	ret = 0x0F & ~ret;

	return ret;
}

static int shcts_set_led_duty(struct shcts_smsc *cts,u8 select, u8 param2)
{
	u8 data;
	int ret = 0;
	int decode_data = 0;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	decode_data = shcts_led_duty_decode_write(select,param2);

	ret = shcts_reg_read(cts, SHCTS_REG_DIRECT_DUTY_CYCLE, &data, 1);
	if(ret != 0){
		return ret;
	}

	if(select == 0){
		data &= 0xF0;
		data = data | decode_data;
	}else if(select == 1){
		data &= 0x0F;
		data = data | (decode_data << 4);
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}
	ret = shcts_reg_write(cts, SHCTS_REG_DIRECT_DUTY_CYCLE, data);

	return ret;
}

static int shcts_get_led_duty(struct shcts_smsc *cts, u8 select)
{
	int ret = 0;
	u8 data;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(select == 0){
		ret = shcts_reg_read(cts, SHCTS_REG_DIRECT_DUTY_CYCLE, &data, 1);
		data = data & 0x0F;
	}else if(select == 1){
		ret = shcts_reg_read(cts, SHCTS_REG_DIRECT_DUTY_CYCLE, &data, 1);
		data = data >> 4;
	}else{
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	data = shcts_led_duty_decode_read(select,data);

	return data;
}

static void shctsled_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	int ret = 1;
	struct shcts_smsc *cts = gShcts_smsc;

	shcts_mutex_lock_enable();

	if(value == LED_OFF)
	{
		SHCTS_LOG_DBG_PRINT("%s() LED OFF\n", __func__);
		ret = shcts_set_led_output_disable(cts, 1);
	}
	else if(value == LED_FULL)
	{
		SHCTS_LOG_DBG_PRINT("%s() LED ON\n", __func__);
		ret = shcts_set_led_output_enable(cts, 1);
	}

	shcts_mutex_lock_disable();	
}

static int shcts_init_param(struct shcts_smsc *cts)
{
	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	return 0;
}

void shcts_set_state_active(void)
{
	struct shcts_smsc *cts = gShcts_smsc;

	shcts_device_reg_init(cts);

	request_event(cts, SHCTS_EVENT_WAKEUP, 0);
}

void shcts_set_state_standby(void)
{
	struct shcts_smsc *cts = gShcts_smsc;

	request_event(cts, SHCTS_EVENT_SLEEP, 0);
}

void shcts_set_state_deep_sleep(void)
{
	struct shcts_smsc *cts = gShcts_smsc;

	request_event(cts, SHCTS_EVENT_DEEP_SLEEP, 0);
}

/* -----------------------------------------------------------------------------------
 */
static void shcts_key_event_report(struct shcts_smsc *cts, u8 state)
{
	int isEvent = 0;

	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	if(cts->report_key_state != state){
		if( ((cts->report_key_state & (1 << SHCTS_KEY_MENU)) ^ (state & (1 << SHCTS_KEY_MENU))) != 0 ){
			SHCTS_LOG_EVT_PRINT("KEY_MENU [%d]\n", ((state >> SHCTS_KEY_MENU) & 0x01));
			input_report_key(cts->input_key, KEY_MENU, ((state >> SHCTS_KEY_MENU) & 0x01));
			isEvent = 1;
		}
		if( ((cts->report_key_state & (1 << SHCTS_KEY_HOME)) ^ (state & (1 << SHCTS_KEY_HOME))) != 0 ){
			SHCTS_LOG_EVT_PRINT("KEY_HOMEPAGE [%d]\n", ((state >> SHCTS_KEY_HOME) & 0x01));
			input_report_key(cts->input_key, KEY_HOMEPAGE, ((state >> SHCTS_KEY_HOME) & 0x01));
			isEvent = 1;
		}
		if( ((cts->report_key_state & (1 << SHCTS_KEY_BACK)) ^ (state & (1 << SHCTS_KEY_BACK))) != 0 ){
			SHCTS_LOG_EVT_PRINT("KEY_BACK [%d]\n", ((state >> SHCTS_KEY_BACK) & 0x01));
			input_report_key(cts->input_key, KEY_BACK, ((state >> SHCTS_KEY_BACK) & 0x01));
			isEvent = 1;
		}
	}

	if(isEvent){
		input_sync(cts->input_key);
		cts->report_key_state = state;
	}
}

static void shcts_key_event_force_touchup(struct shcts_smsc *cts)
{
	int isEvent = 0;

	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	if(cts->report_key_state != 0){
		if( (cts->report_key_state & (1 << SHCTS_KEY_MENU)) != 0 ){
			SHCTS_LOG_EVT_PRINT("KEY_MENU [0]\n");
			input_report_key(cts->input_key, KEY_MENU, 0);
			isEvent = 1;
		}
		if( (cts->report_key_state & (1 << SHCTS_KEY_HOME)) != 0 ){
			SHCTS_LOG_EVT_PRINT("KEY_HOMEPAGE [0]\n");
			input_report_key(cts->input_key, KEY_HOMEPAGE, 0);
			isEvent = 1;
		}
		if( (cts->report_key_state & (1 << SHCTS_KEY_BACK)) != 0 ){
			SHCTS_LOG_EVT_PRINT("KEY_BACK [0]\n");
			input_report_key(cts->input_key, KEY_BACK, 0);
			isEvent = 1;
		}
	}

	if(isEvent){
		input_sync(cts->input_key);
	}

	cts->report_key_state = 0;
}

static void shcts_read_touchevent(struct shcts_smsc *cts, int state)
{
	u8 key_state = 0;
	int ret;

	shcts_irq_clear(cts);

	ret = shcts_reg_read(cts, SHCTS_REG_INPUT_STATUS, &key_state, 1);

	if(ret == 0){
		shcts_key_event_report(cts, key_state);
	}else{
		SHCTS_LOG_ERR_PRINT("get touch state error\n");
	}
}

/* -----------------------------------------------------------------------------------
 */
static int request_event(struct shcts_smsc *cts, int event, int param)
{
	int ret;

	SHCTS_LOG_DBG_PRINT("event %d in state %d\n", event, cts->state_mgr.state);

	shcts_mutex_lock_enable();

	switch(event){
		case SHCTS_EVENT_START:
			ret = state_func_tbl[cts->state_mgr.state]->start(cts, param);
			break;

		case SHCTS_EVENT_STOP:
			ret = state_func_tbl[cts->state_mgr.state]->stop(cts, param);
			break;

		case SHCTS_EVENT_SLEEP:
			ret = state_func_tbl[cts->state_mgr.state]->sleep(cts, param);
			break;

		case SHCTS_EVENT_WAKEUP:
			ret = state_func_tbl[cts->state_mgr.state]->wakeup(cts, param);
			break;

		case SHCTS_EVENT_DEEP_SLEEP:
			ret = state_func_tbl[cts->state_mgr.state]->deep_sleep(cts, param);
			break;

		case SHCTS_EVENT_INTERRUPT:
			ret = state_func_tbl[cts->state_mgr.state]->interrupt(cts, param);
			break;

		case SHCTS_EVENT_TIMEOUT:
			ret = state_func_tbl[cts->state_mgr.state]->timeout(cts, param);
			break;

		default:
			ret = -1;
			break;
	}

	shcts_mutex_lock_disable();

	return ret;
}

static int state_change(struct shcts_smsc *cts, int state)
{
	int ret = 0;
	int old_state = cts->state_mgr.state;

	SHCTS_LOG_DBG_PRINT("state %d -> %d\n", cts->state_mgr.state, state);

	if(cts->state_mgr.state != state){
		cts->state_mgr.state = state;
		ret = state_func_tbl[cts->state_mgr.state]->enter(cts, old_state);
	}
	return ret;
}

/* -----------------------------------------------------------------------------------
 */
static int shcts_statef_nop(struct shcts_smsc *cts, int param)
{
	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	return 0;
}

static int shcts_statef_cmn_error(struct shcts_smsc *cts, int param)
{
	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	return -1;
}

static int shcts_statef_cmn_stop(struct shcts_smsc *cts, int param)
{
	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	shcts_irq_disable(cts);
	shcts_key_event_force_touchup(cts);
	if( shcts_set_deep_sleep(cts) != 0){
		SHCTS_LOG_ERR_PRINT("%s() deep_sleep set error\n", __func__);
	}
	state_change(cts, SHCTS_DRV_STATE_IDLE);

	return 0;
}

static int shcts_statef_cmn_sleep(struct shcts_smsc *cts, int param)
{
	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	return 0;
}

static int shcts_statef_cmn_deep_sleep(struct shcts_smsc *cts, int param)
{
	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shcts_statef_idle_start(struct shcts_smsc *cts, int param)
{
	int ret = 0;

	shcts_irq_clear(cts);
	shcts_irq_enable(cts);
	ret = state_change(cts, SHCTS_DRV_STATE_ACTIVE);

	return ret;
}

static int shcts_statef_idle_int(struct shcts_smsc *cts, int param)
{
	shcts_irq_clear(cts);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shcts_statef_active_enter(struct shcts_smsc *cts, int param)
{
	shcts_set_active(cts);

	if(param == SHCTS_DRV_STATE_IDLE){
		shcts_init_param(cts);
	}

	return 0;
}

static int shcts_statef_active_stop(struct shcts_smsc *cts, int param)
{
	shcts_statef_cmn_stop(cts, 0);

	return 0;
}

static int shcts_statef_active_sleep(struct shcts_smsc *cts, int param)
{
	state_change(cts, SHCTS_DRV_STATE_SLEEP);

	return 0;
}

static int shcts_statef_active_deep_sleep(struct shcts_smsc *cts, int param)
{
	state_change(cts, SHCTS_DRV_STATE_DEEP_SLEEP);

	return 0;
}

static int shcts_statef_active_int(struct shcts_smsc *cts, int param)
{
	shcts_read_touchevent(cts, SHCTS_DRV_STATE_ACTIVE);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shcts_statef_sleep_enter(struct shcts_smsc *cts, int param)
{
	int ret = 0;

	shcts_key_event_force_touchup(cts);
	ret = shcts_set_standby(cts);

	return ret;
}

static int shcts_statef_sleep_wakeup(struct shcts_smsc *cts, int param)
{
	state_change(cts, SHCTS_DRV_STATE_ACTIVE);

	return 0;
}

static int shcts_statef_sleep_deep_sleep(struct shcts_smsc *cts, int param)
{
	state_change(cts, SHCTS_DRV_STATE_DEEP_SLEEP);

	return 0;
}

static int shcts_statef_sleep_int(struct shcts_smsc *cts, int param)
{
	shcts_irq_clear(cts);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shcts_statef_deep_sleep_enter(struct shcts_smsc *cts, int param)
{
	shcts_key_event_force_touchup(cts);
	shcts_set_deep_sleep(cts);

	return 0;
}

static int shcts_statef_deep_sleep_sleep(struct shcts_smsc *cts, int param)
{
	state_change(cts, SHCTS_DRV_STATE_SLEEP);

	return 0;
}

static int shcts_statef_deep_sleep_wakeup(struct shcts_smsc *cts, int param)
{
	state_change(cts, SHCTS_DRV_STATE_ACTIVE);

	return 0;
}

static int shcts_statef_deep_sleep_int(struct shcts_smsc *cts, int param)
{
	shcts_irq_clear(cts);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static void shcts_mutex_lock_enable(void)
{
	SHCTS_LOG_DBG_PRINT("mutex_lock\n");
	mutex_lock(&shcts_ctrl_lock);
}

static void shcts_mutex_lock_disable(void)
{
	mutex_unlock(&shcts_ctrl_lock);
	SHCTS_LOG_DBG_PRINT("mutex_unlock\n");
}

/* -----------------------------------------------------------------------------------
 */
static irqreturn_t shcts_irq(int irq, void *dev_id)
{
	struct shcts_smsc *cts = dev_id;

	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	request_event(cts, SHCTS_EVENT_INTERRUPT, 0);

	return IRQ_HANDLED;
}

static int shcts_irq_clear(struct shcts_smsc *cts)
{
	u8 data;
	int ret = 0;

	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	ret = shcts_reg_read(cts, SHCTS_REG_MAIN_CONTROL, &data, 1);
	if(ret != 0){
		SHCTS_LOG_DBG_PRINT("%s() get int state error\n", __func__);
		return -1;
	}

	data &= (~0x01);

	ret = shcts_reg_write(cts, SHCTS_REG_MAIN_CONTROL, data);
	if(ret != 0){
		SHCTS_LOG_DBG_PRINT("%s() int clear error\n", __func__);
	}

	return ret;
}

static void shcts_irq_disable(struct shcts_smsc *cts)
{
	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);
	if(cts->irq_mgr.state != SHCTS_IRQ_STATE_DISABLE){
		disable_irq_nosync(cts->irq_mgr.irq);
		cts->irq_mgr.state = SHCTS_IRQ_STATE_DISABLE;
	}
}

static void shcts_irq_enable(struct shcts_smsc *cts)
{
	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);
	if(cts->irq_mgr.state != SHCTS_IRQ_STATE_ENABLE){
		enable_irq(cts->irq_mgr.irq);
		cts->irq_mgr.state = SHCTS_IRQ_STATE_ENABLE;
	}
}

static int shcts_irq_resuest(struct shcts_smsc *cts)
{
	int rc;

	struct pm_gpio param = {
		.direction      = PM_GPIO_DIR_IN,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 0,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM_GPIO_VIN_L15,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol	= 0,
		.disable_pin	= 0,
	};

	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	rc = gpio_request(cts->irq_mgr.irq, "shcts_irq");
	if(rc != 0){
		SHCTS_LOG_ERR_PRINT("irq gpio request error\n");
	}

	rc = pm8xxx_gpio_config(cts->irq_mgr.irq, &param);
	if(rc != 0){
		SHCTS_LOG_ERR_PRINT("irq gpio config error\n");
	}

	rc = request_threaded_irq(gpio_to_irq(cts->irq_mgr.irq),
							  NULL,
							  shcts_irq,
							  IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							  SHCTS_DEVNAME,
							  cts);

	if(rc){
		SHCTS_LOG_ERR_PRINT("request_threaded_irq error:%d\n",rc);
		return -1;
	}

	cts->irq_mgr.state = SHCTS_IRQ_STATE_ENABLE;
	cts->irq_mgr.wake  = SHCTS_IRQ_WAKE_DISABLE;
	shcts_irq_disable(cts);

	return 0;
}
/* -----------------------------------------------------------------------------------
 */
static void shcts_device_reg_init(struct shcts_smsc *cts)
{
	int ret = 0;
	u8 data;

	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	ret = shcts_reg_read(cts, SHCTS_REG_SENSITIVITY_CONTROL, &data, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return;
	}
	data = data & 0x8F;
	data = data | 0x30;
	ret = shcts_reg_write(cts, SHCTS_REG_SENSITIVITY_CONTROL, data);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return;
	}
	
	
	ret = shcts_reg_read(cts, SHCTS_REG_CONFIGURATION, &data, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return;
	}
	data = data & 0xEF;
	data = data | 0x10;
	ret = shcts_reg_write(cts, SHCTS_REG_CONFIGURATION, data);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return;
	}
	
	
	ret = shcts_reg_read(cts, SHCTS_REG_CONFIGURATION2, &data, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return;
	}
	data = data & 0xFB;
	data = data | 0x04;
	ret = shcts_reg_write(cts, SHCTS_REG_CONFIGURATION2, data);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return;
	}

	ret = shcts_reg_read(cts, SHCTS_REG_DIRECT_DUTY_CYCLE, &data, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return;
	}
	data = data & 0x0F;
	data = data | 0xA0;
	ret = shcts_reg_write(cts, SHCTS_REG_DIRECT_DUTY_CYCLE, data);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return;
	}
}

/* -----------------------------------------------------------------------------------
 */
static int shcts_ioctl_enable(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;

	ret = request_event(cts, SHCTS_EVENT_START, 0);

	return ret;
}

static int shcts_ioctl_disable(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;

	ret = request_event(cts, SHCTS_EVENT_STOP, 0);

	return ret;
}

static int shcts_ioctl_get_id_product(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 id = 0;

	shcts_mutex_lock_enable();
	ret = shcts_reg_read(cts, SHCTS_REG_PRODUCT_ID, &id, 1);
	shcts_mutex_lock_disable();

	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return id;
}

static int shcts_ioctl_get_id_manufact(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 id = 0;

	shcts_mutex_lock_enable();
	ret = shcts_reg_read(cts, SHCTS_REG_MANUFACTURER_ID, &id, 1);
	shcts_mutex_lock_disable();

	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return id;
}

static int shcts_ioctl_get_id_revision(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 id = 0;

	shcts_mutex_lock_enable();
	ret = shcts_reg_read(cts, SHCTS_REG_REVISION, &id, 1);
	shcts_mutex_lock_disable();

	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return id;
}

static int shcts_ioctl_get_dev_state(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;

	shcts_mutex_lock_enable();
	ret = shcts_get_dev_state(cts);
	shcts_mutex_lock_disable();

	return ret;
}

static int shcts_ioctl_set_calibration_activate(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 select;
	struct shcts_ioctl_param param;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&select, &param.data[0], 1) != 0 ){
		return -EINVAL;
	}
	
	shcts_mutex_lock_enable();
	ret = shcts_set_calibration_activate(cts,select);
	shcts_mutex_lock_disable();

	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return ret;
}

static int shcts_ioctl_set_input_threshold(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 select;
	u8 param2;
	struct shcts_ioctl_param param;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&select, &param.data[0], 1) != 0 ){
		return -EINVAL;
	}
	if( copy_from_user(&param2, &param.data[1], 1) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_set_input_threshold(cts,select,param2);
	shcts_mutex_lock_disable();

	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return ret;
}

static int shcts_ioctl_get_input_threshold(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 select;
	struct shcts_ioctl_param param;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&select, &param.data[0], 1) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_get_input_threshold(cts,select);
	shcts_mutex_lock_disable();

	return ret;
}

static int shcts_ioctl_set_param(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 select;
	u8 param2;
	u8 param3;
	struct shcts_ioctl_param param;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&select, &param.data[0], 1) != 0 ){
		return -EINVAL;
	}
	if( copy_from_user(&param2, &param.data[1], 1) != 0 ){
		return -EINVAL;
	}
	if( copy_from_user(&param3, &param.data[2], 1) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_set_param(cts,select,param2,param3);
	shcts_mutex_lock_disable();

	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return ret;
}

static int shcts_ioctl_get_param(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 select;
	u8 param2;
	struct shcts_ioctl_param param;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&select, &param.data[0], 1) != 0 ){
		return -EINVAL;
	}
	if( copy_from_user(&param2, &param.data[1], 1) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_get_param(cts,select,param2);
	shcts_mutex_lock_disable();

	return ret;
}

static int shcts_ioctl_set_led_duty(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 select;
	u8 param2;
	struct shcts_ioctl_param param;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&select, &param.data[0], 1) != 0 ){
		return -EINVAL;
	}
	if( copy_from_user(&param2, &param.data[1], 1) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_set_led_duty(cts,select,param2);
	shcts_mutex_lock_disable();

	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() error\n", __func__);
		return -EFAULT;
	}

	return ret;
}

static int shcts_ioctl_get_led_duty(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 select;
	struct shcts_ioctl_param param;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&select, &param.data[0], 1) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_get_led_duty(cts,select);
	shcts_mutex_lock_disable();

	return ret;
}

static int shcts_ioctl_set_dev_state(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;
	u8 state = 0;
	u8 set_state = (u8)arg;

	switch(set_state){
		case SHCTS_DEV_STATE_ACTIVE:
			request_event(cts, SHCTS_EVENT_WAKEUP, 0);
			break;

		case SHCTS_DEV_STATE_STANDBY:
			request_event(cts, SHCTS_EVENT_SLEEP, 0);
			break;

		case SHCTS_DEV_STATE_DEEP_SLEEP:
			request_event(cts, SHCTS_EVENT_DEEP_SLEEP, 0);
			break;

		default:
			ret = -1;
			break;
	}

	shcts_mutex_lock_enable();
	state = shcts_get_dev_state(cts);
	shcts_mutex_lock_disable();

	if(set_state != state){
		SHCTS_LOG_ERR_PRINT("%s() error, state is [%d]\n", __func__, state);
		ret = -1;
	}

	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() set dev state error [%d]\n", __func__, set_state);
		return -EFAULT;
	}

	return 0;
}

static int shcts_ioctl_get_touch_info(struct shcts_smsc *cts, unsigned long arg)
{
	struct shcts_touch_key_info info;
	u8 key_state = 0;
	u8 delta_count[SHCTS_KEY_NUM];
	int ret;

	shcts_mutex_lock_enable();

	ret = shcts_reg_read(cts, SHCTS_REG_INPUT_STATUS, &key_state, 1);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() get touch state error\n", __func__);
		return -EFAULT;
	}

	ret = shcts_reg_read(cts, SHCTS_REG_SENSOR_INPUT1_DALTA_COUNT, delta_count, SHCTS_KEY_NUM);
	if(ret != 0){
		SHCTS_LOG_ERR_PRINT("%s() get delta count error\n", __func__);
		return -EFAULT;
	}

	shcts_mutex_lock_disable();

	info.menu_key_state = (key_state >> SHCTS_KEY_MENU) & 0x01;
	info.home_key_state = (key_state >> SHCTS_KEY_HOME) & 0x01;
	info.back_key_state = (key_state >> SHCTS_KEY_BACK) & 0x01;

	info.menu_key_delta = (signed char)delta_count[SHCTS_KEY_MENU];
	info.home_key_delta = (signed char)delta_count[SHCTS_KEY_HOME];
	info.back_key_delta = (signed char)delta_count[SHCTS_KEY_BACK];

	if(copy_to_user((u8*)arg, (u8*)&info, sizeof(info))){
		return -EFAULT;
	}

	return 0;
}

static int shcts_ioctl_set_led_state(struct shcts_smsc *cts, unsigned long arg)
{
	struct shcts_ioctl_param param;
	u8 led = 0;
	u8 onoff = 0;
	int ret = 0;

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&onoff, &param.data[0], 1) != 0 ){
		return -EINVAL;
	}
	if( copy_from_user(&led, &param.data[1], 1) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();

	if(onoff == 0){
		ret = shcts_set_led_output_disable(cts, led);
	}else{
		ret = shcts_set_led_output_enable(cts, led);
	}

	shcts_mutex_lock_disable();

	return ret;
}

static int shcts_ioctl_log_enable(struct shcts_smsc *cts, unsigned long arg)
{
	int ret = 0;

	#if defined( SHCTS_LOG_DEBUG_ENABLE ) || defined(SHCTS_LOG_EVENT_ENABLE)
		LogOutputEnable = (int)arg;
	#else
		ret = -1;
	#endif /* SHCTS_LOG_DEBUG_ENABLE || SHCTS_LOG_EVENT_ENABLE */

	return ret;
}

static int shcts_ioctl_reg_write(struct shcts_smsc *cts, unsigned long arg)
{
	u8 data[2];
	struct shcts_ioctl_param param;
	int ret = 0;

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(data, param.data, 2) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_reg_write(cts, data[0], data[1]);
	shcts_mutex_lock_disable();

	return ret;
}

static int shcts_ioctl_reg_read(struct shcts_smsc *cts, unsigned long arg)
{
	u8 addr;
	u8 data;
	struct shcts_ioctl_param param;
	int ret = 0;

	if(0 == arg || 0 != copy_from_user(&param, (void __user *)arg, sizeof(param))){
		return -EINVAL;
	}

	if( copy_from_user(&addr, param.data, 1) != 0 ){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_reg_read(cts, addr, &data, 1);
	shcts_mutex_lock_disable();

	if(ret == 0){
		if(copy_to_user(((struct shcts_ioctl_param*)arg)->data, (u8*)&data, 1)){
			return -EFAULT;
		}
	}

	return ret;
}

static int shcts_ioctl_reg_read_all(struct shcts_smsc *cts, unsigned long arg)
{
	u8 data[0x100];
	int ret = 0;

	if(0 == arg){
		return -EINVAL;
	}

	shcts_mutex_lock_enable();
	ret = shcts_reg_read(cts, 0x00, data, 0x100);
	shcts_mutex_lock_disable();

	if(ret == 0){
		if(copy_to_user(((struct shcts_ioctl_param*)arg)->data, (u8*)data, 0x100)){
			return -EFAULT;
		}
	}

	return ret;
}

/* -----------------------------------------------------------------------------------
 */
static int shctsif_open(struct inode *inode, struct file *file)
{
	SHCTS_LOG_DBG_PRINT("%s() (PID:%ld)\n", __func__, sys_getpid());

	return 0;
}

static int shctsif_release(struct inode *inode, struct file *file)
{
	SHCTS_LOG_DBG_PRINT("%s() (PID:%ld)\n", __func__, sys_getpid());

	return 0;
}

static long shctsif_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct shcts_smsc *cts = gShcts_smsc;

	SHCTS_LOG_DBG_PRINT("ioctl(PID:%ld, CMD:%d, ARG:0x%lx)\n",
											sys_getpid(), cmd, arg);

	if(cts == NULL){
		SHCTS_LOG_DBG_PRINT("%s() cts == NULL\n", __func__);
		return -EFAULT;
	}

	switch(cmd){
		case SHCTS_DEV_ENABLE:						rc = shcts_ioctl_enable(cts, arg);						break;
		case SHCTS_DEV_DISABLE:						rc = shcts_ioctl_disable(cts, arg);						break;
		case SHCTS_DEV_GET_ID_PRODUCT:				rc = shcts_ioctl_get_id_product(cts, arg);				break;
		case SHCTS_DEV_GET_ID_MANUFACT:				rc = shcts_ioctl_get_id_manufact(cts, arg);				break;
		case SHCTS_DEV_GET_ID_REVISION:				rc = shcts_ioctl_get_id_revision(cts, arg);				break;
		case SHCTS_DEV_GET_DEV_STATE:				rc = shcts_ioctl_get_dev_state(cts, arg);				break;
		case SHCTS_DEV_SET_DEV_STATE:				rc = shcts_ioctl_set_dev_state(cts, arg);				break;
		case SHCTS_DEV_GET_TOUCH_INFO:				rc = shcts_ioctl_get_touch_info(cts, arg);				break;
		case SHCTS_DEV_SET_LED_STATE:				rc = shcts_ioctl_set_led_state(cts, arg);				break;
		case SHCTS_DEV_SET_CALIBRATION_ACTIVATE:	rc = shcts_ioctl_set_calibration_activate(cts, arg);	break;
		case SHCTS_DEV_SET_INPUT_THRESHOLD:			rc = shcts_ioctl_set_input_threshold(cts, arg);			break;
		case SHCTS_DEV_GET_INPUT_THRESHOLD:			rc = shcts_ioctl_get_input_threshold(cts, arg);			break;
		case SHCTS_DEV_SET_PARAM:					rc = shcts_ioctl_set_param(cts, arg);					break;
		case SHCTS_DEV_GET_PARAM:					rc = shcts_ioctl_get_param(cts, arg);					break;
		case SHCTS_DEV_SET_LED_DUTY:				rc = shcts_ioctl_set_led_duty(cts, arg);				break;
		case SHCTS_DEV_GET_LED_DUTY:				rc = shcts_ioctl_get_led_duty(cts, arg);				break;
		case SHCTS_DEV_LOGOUTPUT_ENABLE:			rc = shcts_ioctl_log_enable(cts, arg);					break;
		case SHCTS_DEV_REG_WRTIE:					rc = shcts_ioctl_reg_write(cts, arg);					break;
		case SHCTS_DEV_REG_READ:					rc = shcts_ioctl_reg_read(cts, arg);					break;
		case SHCTS_DEV_REG_READ_ALL:				rc = shcts_ioctl_reg_read_all(cts, arg);				break;

		default:
			SHCTS_LOG_DBG_PRINT("%s() switch(cmd) default\n", __func__);
			rc = -ENOIOCTLCMD;
			break;
	}

	return rc;
}

static const struct file_operations shctsif_fileops = {
	.owner   = THIS_MODULE,
	.open    = shctsif_open,
	.release = shctsif_release,
	.unlocked_ioctl   = shctsif_ioctl,
};

static struct led_classdev shkeyled_dev =
{
	.name			= "button-backlight",
	.brightness_set	= shctsled_set,
	.brightness		= LED_OFF,
};

int __init shctsif_init(void)
{
	int rc;

	rc = alloc_chrdev_region(&shctsif_devid, 0, 1, SHCTS_IF_DEVNAME);
	if(rc < 0){
		SHCTS_LOG_ERR_PRINT("shctsif : alloc_chrdev_region error\n");
		return rc;
	}

	shctsif_class = class_create(THIS_MODULE, SHCTS_IF_DEVNAME);
	if (IS_ERR(shctsif_class)) {
		rc = PTR_ERR(shctsif_class);
		SHCTS_LOG_ERR_PRINT("shctsif : class_create error\n");
		goto error_vid_class_create;
	}

	shctsif_device = device_create(shctsif_class, NULL,
								shctsif_devid, &shctsif_cdev,
								SHCTS_IF_DEVNAME);
	if (IS_ERR(shctsif_device)) {
		rc = PTR_ERR(shctsif_device);
		SHCTS_LOG_ERR_PRINT("shctsif : device_create error\n");
		goto error_vid_class_device_create;
	}

	cdev_init(&shctsif_cdev, &shctsif_fileops);
	shctsif_cdev.owner = THIS_MODULE;
	rc = cdev_add(&shctsif_cdev, shctsif_devid, 1);
	if(rc < 0){
		SHCTS_LOG_ERR_PRINT("shctsif : cdev_add error\n");
		goto err_via_cdev_add;
	}

	SHCTS_LOG_DBG_PRINT("%s() done\n", __func__);

	return 0;

err_via_cdev_add:
	cdev_del(&shctsif_cdev);
error_vid_class_device_create:
	class_destroy(shctsif_class);
error_vid_class_create:
	unregister_chrdev_region(shctsif_devid, 1);

	return rc;
}
module_init(shctsif_init);

static void __exit shctsif_exit(void)
{
	cdev_del(&shctsif_cdev);
	class_destroy(shctsif_class);
	unregister_chrdev_region(shctsif_devid, 1);

	SHCTS_LOG_DBG_PRINT("%s() done\n", __func__);
}
module_exit(shctsif_exit);

/* -----------------------------------------------------------------------------------
 */
static int shcts_smsc_open(struct input_dev *dev)
{
	struct shcts_smsc *cts = gShcts_smsc;

	SHCTS_LOG_DBG_PRINT("%s() (PID:%ld)\n", __func__, sys_getpid());

	request_event(cts, SHCTS_EVENT_START, 0);

	return 0;
}

static void shcts_smsc_close(struct input_dev *dev)
{
	struct shcts_smsc *cts = gShcts_smsc;

	SHCTS_LOG_DBG_PRINT("%s() (PID:%ld)\n", __func__, sys_getpid());

	request_event(cts, SHCTS_EVENT_STOP, 0);
}

/* -----------------------------------------------------------------------------------
 */
static int __devinit shcts_smsc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int result = 0;
	struct shcts_smsc *cts;
	struct input_dev *input_dev_key;

	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	shcts_mutex_lock_enable();

	cts = kzalloc(sizeof(struct shcts_smsc), GFP_KERNEL);
	if(cts == NULL){
		result = -ENOMEM;
		goto shcts_smsc_probe_fail;
	}

	input_dev_key = input_allocate_device();
	if(input_dev_key == NULL){
		result = -ENOMEM;
		goto shcts_smsc_probe_fail;
	}

	gShcts_smsc = cts;
	i2c_set_clientdata(client, cts);

	cts->i2c_client = client;
	cts->irq_mgr.irq = SHCTS_GPIO_INT;
	cts->state_mgr.state = SHCTS_DRV_STATE_IDLE;
	cts->report_key_state = 0;
	cts->input_key = input_dev_key;

	shcts_device_reg_init(cts);

	if(shcts_irq_resuest(cts)){
		result = -EFAULT;
		SHCTS_LOG_ERR_PRINT("shcts:request_irq error\n");
		goto shcts_smsc_probe_fail;
	}

	shcts_mutex_lock_disable();

	input_dev_key->name 		= "shcts_key";
	input_dev_key->id.vendor	= 0x0000;
	input_dev_key->id.product	= 0x0000;
	input_dev_key->id.version	= 0x0000;
	input_dev_key->dev.parent	= &client->dev;
	input_dev_key->open			= shcts_smsc_open;
	input_dev_key->close		= shcts_smsc_close;

	__set_bit(EV_KEY, input_dev_key->evbit);
	__set_bit(KEY_MENU, input_dev_key->keybit);
	__set_bit(KEY_HOMEPAGE, input_dev_key->keybit);
	__set_bit(KEY_BACK, input_dev_key->keybit);
	__clear_bit(KEY_RESERVED, input_dev_key->keybit);

	input_set_drvdata(input_dev_key, cts);

	result = input_register_device(input_dev_key);
	if (result){
		goto shcts_smsc_probe_fail;
	}

	#if defined(SHCTS_DEBUG_CREATE_KOBJ_ENABLE)
		cts->kobj = kobject_create_and_add("shcts", kernel_kobj);
		if(cts->kobj == NULL){
			SHCTS_LOG_ERR_PRINT("kobj create failed : shcts\n");
			result = -ENOMEM;
			goto shcts_smsc_probe_fail;
		}
		result = sysfs_create_group(cts->kobj, &attr_grp_ctrl);
		if(result){
			SHCTS_LOG_ERR_PRINT("kobj create failed : attr_grp_ctrl\n");
			result = -ENOMEM;
			goto shcts_smsc_probe_fail;
		}
		result = sysfs_create_group(cts->kobj, &attr_grp_debug);
		if(result){
			SHCTS_LOG_ERR_PRINT("kobj create failed : attr_grp_debug\n");
			result = -ENOMEM;
			goto shcts_smsc_probe_fail;
		}
	#endif /* SHCTS_DEBUG_CREATE_KOBJ_ENABLE */

	result = led_classdev_register(NULL, &shkeyled_dev);
	if (result)
	{
		SHCTS_LOG_ERR_PRINT("led_classdev_register Error\n");
		return result;
	}

	SHCTS_LOG_DBG_PRINT("%s() done\n", __func__);

	return 0;

shcts_smsc_probe_fail:

	shcts_mutex_lock_disable();

	if(cts != NULL){
		input_free_device(cts->input_key);
		kfree(cts);
	}

	return result;
}

static int __devexit shcts_smsc_remove(struct i2c_client *client)
{
	struct shcts_smsc *cts;

	led_classdev_unregister(&shkeyled_dev);

	cts = i2c_get_clientdata(client);

	#if defined(SHCTS_DEBUG_CREATE_KOBJ_ENABLE)
		if(cts->kobj != NULL){
			kobject_put(cts->kobj);
		}
	#endif /* SHCTS_DEBUG_CREATE_KOBJ_ENABLE */

	if(cts != NULL){
		input_free_device(cts->input_key);
		kfree(cts);
	}

	SHCTS_LOG_DBG_PRINT("%s() done\n", __func__);

	return 0;
}

static int shcts_smsc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int retval;

	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	led_classdev_suspend(&shkeyled_dev);

	retval = enable_irq_wake(gpio_to_irq(SHCTS_GPIO_INT));
	return retval;
}

static int shcts_smsc_resume(struct i2c_client *client)
{
	int retval;

	SHCTS_LOG_DBG_PRINT("%s()\n", __func__);

	led_classdev_resume(&shkeyled_dev);

    retval = disable_irq_wake(gpio_to_irq(SHCTS_GPIO_INT));
	return retval;
}

static const struct i2c_device_id shcts_i2c_id[] = {
	{ SHCTS_DEVNAME, 0 }
};

static struct i2c_driver shcts_smsc_driver = {
	.probe		= shcts_smsc_probe,
	.remove		= __devexit_p(shcts_smsc_remove),
	.suspend	= shcts_smsc_suspend,
	.resume		= shcts_smsc_resume,
	.id_table	= shcts_i2c_id,
	.driver		= {
		.name   = SHCTS_DEVNAME,
		.owner  = THIS_MODULE,
	},
};

static int __init shcts_init(void)
{
	SHCTS_LOG_DBG_PRINT("%s() start\n", __func__);

	return i2c_add_driver(&shcts_smsc_driver);
}
module_init(shcts_init);

static void __exit shcts_exit(void)
{
	i2c_del_driver(&shcts_smsc_driver);

	SHCTS_LOG_DBG_PRINT("%s() done\n", __func__);
}
module_exit(shcts_exit);

MODULE_DESCRIPTION("SH CTS Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.0");
