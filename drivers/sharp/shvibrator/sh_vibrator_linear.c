/* drivers/sharp/shvibrator/sh_vibrator.c
 *
 * Copyright (C) 2013 SHARP CORPORATION
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
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include "../../staging/android/timed_output.h"
#include <linux/sched.h>

#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#include <mach/pmic.h>
#include <mach/msm_rpcrouter.h>

#include <linux/slab.h>
#include <linux/i2c.h>

#include <sharp/sh_smem.h>

#if defined( CONFIG_SENSORS_AMI603 ) || defined( CONFIG_SH_YAS530 )
#define SHVIB_PAUSE_PEDOMETER
#endif
#if defined( SHVIB_PAUSE_PEDOMETER )
#if defined( CONFIG_SENSORS_AMI603 )
#include <sharp/shmds_driver.h>
#elif defined( CONFIG_SH_YAS530 )
#include <sharp/shpem_kerl.h>
#endif
#endif

#ifdef CONFIG_SHTERM
#include <sharp/shterm_k.h>
#endif /* CONFIG_SHTERM */

#if defined( CONFIG_SENSORS_SHGEIGER )
#include <sharp/shgeiger.h>
#endif

#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)

#define SHVIB_RST_PDN				12
#define SHVIB_PDN					13

#define SHVIB_I2C_DRIVER_NAME		"sh_vib_i2c"

#define HW_ES0      0x00
#define HW_ES05     0x01
#define HW_ES1      0x02
#define HW_PP1      0x03
#define HW_PP2      0x04
#define HW_PM       0x07

#define MODEL_TYPE_D1       0x01
#define MODEL_TYPE_S1       0x02
#define MODEL_TYPE_A1       0x03
#define MODEL_TYPE_D2       0x04
#define MODEL_TYPE_D3       0x05

/* adb debug_log */
int shvibrator_func_log  = 0;
int shvibrator_debug_log = 0;
int shvibrator_error_log = 1;

#ifdef CONFIG_ANDROID_ENGINEERING
    module_param(shvibrator_func_log,  int, 0600);
    module_param(shvibrator_debug_log, int, 0600);
    module_param(shvibrator_error_log, int, 0600);
#endif  /* CONFIG_ANDROID_ENGINEERING */

#define SHVIBRATOR_FUNC_LOG(fmt, args...) \
	if( shvibrator_func_log == 1 ){ \
		printk( KERN_DEBUG "[shvibrator][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#define SHVIBRATOR_DBG_LOG(fmt, args...) \
	if( shvibrator_debug_log == 1 ){ \
		printk( KERN_DEBUG "[shvibrator][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#define SHVIBRATOR_ERR_LOG(fmt, args...) \
	if( shvibrator_error_log == 1 ){ \
		printk( KERN_DEBUG "[shvibrator]" fmt "\n",## args ); \
	} \
	else if( shvibrator_error_log == 2 ){ \
		printk( KERN_DEBUG "[shvibrator][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}


static struct work_struct work_vibrator_on;
static struct work_struct work_vibrator_off;
static struct work_struct work_hrtimer;
static struct hrtimer vibe_timer;

static struct regulator *reg;
static const char *id = "8921_l10";
static int min_uV = 3000000, max_uV = 3000000;

#if defined( SHVIB_PAUSE_PEDOMETER )
static int pause_pedometer = 0; /* Whether pedometer would be paused. */
static int paused_pedometer = 0; /* Whether pedometer was already paused. */
#endif

static int sh_vibrator_reqtime = 0;
static struct mutex sh_vibrator_pmic_mutex;
static struct wake_lock shvibrator_linear_wake_lock;

static int shvibrator_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id);
static int shvibrator_i2c_remove(struct i2c_client *client);
static int shvibrator_i2c_suspend(struct i2c_client *client, pm_message_t mesg);

int __init shvibrator_i2c_init(void);
void shvibrator_i2c_exit(void);
int shvibrator_i2c_read(struct i2c_client *client,
									int reg, u8 *data, int size);
int shvibrator_i2c_write(struct i2c_client *client,
									int reg, u8 data);
static int vibrator_init = 0;
struct shvib_i2c_data {
	struct i2c_client *client_p;
};
struct shvib_i2c_data *vib_data = NULL;

#if CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2)
static uint16_t sh_get_hw_revision(void)
{
    sharp_smem_common_type *p_sharp_smem_common_type;

	SHVIBRATOR_FUNC_LOG("START");

    p_sharp_smem_common_type = sh_smem_get_common_address();

    if( p_sharp_smem_common_type != 0 ) {
        SHVIBRATOR_FUNC_LOG("END");
        return p_sharp_smem_common_type->sh_hw_revision;
    }
    else {

        SHVIBRATOR_FUNC_LOG("END");
        return 0xFF;
    }

	SHVIBRATOR_FUNC_LOG("END");
}
#endif /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */

static void set_pmic_vibrator(int on)
{
	SHVIBRATOR_FUNC_LOG("START");
	
	mutex_lock(&sh_vibrator_pmic_mutex);

	if (on){
#if CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2)
		if( (sh_get_hw_revision() & 0x07) == HW_ES1 ){
			gpio_direction_output(PM8921_GPIO_PM_TO_SYS(SHVIB_RST_PDN), 1);
			SHVIBRATOR_DBG_LOG("PM8921_GPIO %d High",SHVIB_RST_PDN);
		}else if (!regulator_is_enabled(reg)) {
			regulator_enable(reg);
			SHVIBRATOR_DBG_LOG("PM8921_L10 ON");
		}
#else /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */
		if (!regulator_is_enabled(reg)) {
			regulator_enable(reg);
			SHVIBRATOR_DBG_LOG("PM8921_L10 ON");
		}
#endif /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */

		usleep(300);

		if(vib_data){

#if CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2)
			if(shvibrator_i2c_write(vib_data->client_p, 0x01, 0x0b) < 0) goto set_pmic_vibrator_i2c_err;
			if(shvibrator_i2c_write(vib_data->client_p, 0x02, 0x0f) < 0) goto set_pmic_vibrator_i2c_err;
			if(shvibrator_i2c_write(vib_data->client_p, 0x05, 0x00) < 0) goto set_pmic_vibrator_i2c_err;
#else /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */
			if(shvibrator_i2c_write(vib_data->client_p, 0x01, 0x07) < 0) goto set_pmic_vibrator_i2c_err;
			if(shvibrator_i2c_write(vib_data->client_p, 0x02, 0x0f) < 0) goto set_pmic_vibrator_i2c_err;
#endif /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */

			SHVIBRATOR_DBG_LOG("i2c write OK");
		}else{
			SHVIBRATOR_ERR_LOG("i2c regist error");
			goto set_pmic_vibrator_i2c_err;
		}
		gpio_direction_output(PM8921_GPIO_PM_TO_SYS(SHVIB_PDN), 1);
		SHVIBRATOR_DBG_LOG("PM8921_GPIO %d High",SHVIB_PDN);
	}else{
		gpio_direction_output(PM8921_GPIO_PM_TO_SYS(SHVIB_PDN), 0);
		SHVIBRATOR_DBG_LOG("PM8921_GPIO %d Low",SHVIB_PDN);

#if CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2)
		if( (sh_get_hw_revision() & 0x07) == HW_ES1 ){
			usleep(30 * 1000); /* for braking behavior */
			gpio_direction_output(PM8921_GPIO_PM_TO_SYS(SHVIB_RST_PDN), 0);
			SHVIBRATOR_DBG_LOG("PM8921_GPIO %d Low",SHVIB_RST_PDN);
		}else if (regulator_is_enabled(reg)) {
			usleep(30 * 1000); /* for braking behavior */
			regulator_disable(reg);
			SHVIBRATOR_DBG_LOG("PM8921_L10 OFF");
		} else {
			SHVIBRATOR_DBG_LOG("OFF during OFF : do nothing");
		}
#else /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */
		if (regulator_is_enabled(reg)) {
			usleep(30 * 1000); /* for braking behavior */
			regulator_disable(reg);
			SHVIBRATOR_DBG_LOG("PM8921_L10 OFF");
		} else {
			SHVIBRATOR_DBG_LOG("OFF during OFF : do nothing");
		}
#endif /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */
		wake_unlock(&shvibrator_linear_wake_lock);
		SHVIBRATOR_DBG_LOG("shvibrator_linear_wake_lock : unlock ");
	}
	mutex_unlock(&sh_vibrator_pmic_mutex);

	SHVIBRATOR_FUNC_LOG("END");

	return;
	
set_pmic_vibrator_i2c_err:

	SHVIBRATOR_ERR_LOG("i2c write error");

	mutex_unlock(&sh_vibrator_pmic_mutex);

	SHVIBRATOR_FUNC_LOG("END");

	return;
}

static void pmic_vibrator_on(struct work_struct *work)
{
	SHVIBRATOR_FUNC_LOG("START");

#if defined( SHVIB_PAUSE_PEDOMETER )
	if (!paused_pedometer && pause_pedometer) {
#if defined( CONFIG_SENSORS_AMI603 )
		printk("[shvibrator] Pause a pedometer.\n");
		AMI602Pedometer_Pause();
#elif defined( CONFIG_SH_YAS530 )
		pr_debug("[shvibrator] Pause a acclerometer.\n");
		SHMDS_Acclerometer_Control( SHMDS_ACC_PAUSE );
#endif
		paused_pedometer = 1;
	} else {
#if defined( CONFIG_SENSORS_AMI603 )
		pr_debug("[shvibrator] Don't pause a pedometer.\n");
#elif defined( CONFIG_SH_YAS530 )
		pr_debug("[shvibrator] Don't pause a acclerometer.\n");
#endif
	}
#endif

/* [BatteryTemperatureLog] [start] */
#ifdef CONFIG_SHTERM
    pr_debug("%s() shterm_k_set_info( SHTERM_INFO_VIB, 1 ) \n", __func__);
    shterm_k_set_info( SHTERM_INFO_VIB, 1 );
#endif /* CONFIG_SHTERM */
/* [BatteryTemperatureLog] [end] */
/* [stop geiger when using speaker/vibration] -> */
#if defined( CONFIG_SENSORS_SHGEIGER )
	ShGeigerPause_Vib();
#endif
/* [stop geiger when using speaker/vibration] <- */

	set_pmic_vibrator(1);
	
	hrtimer_start(&vibe_timer,
		      ktime_set(sh_vibrator_reqtime / 1000, (sh_vibrator_reqtime % 1000) * 1000000),
		      HRTIMER_MODE_REL);
	pr_debug("[shvibrator] timer start. %d \n", sh_vibrator_reqtime);

	SHVIBRATOR_FUNC_LOG("END");
}

static void pmic_vibrator_off(struct work_struct *work)
{
	SHVIBRATOR_FUNC_LOG("START");

	set_pmic_vibrator(0);

/* [stop geiger when using speaker/vibration] -> */
#if defined( CONFIG_SENSORS_SHGEIGER )
	ShGeigerReStart_Vib();
#endif
/* [stop geiger when using speaker/vibration] <- */
/* [BatteryTemperatureLog] [start] */
#ifdef CONFIG_SHTERM
    pr_debug("%s() shterm_k_set_info( SHTERM_INFO_VIB, 0 ) \n", __func__);
    shterm_k_set_info( SHTERM_INFO_VIB, 0 );
#endif /* CONFIG_SHTERM */
/* [BatteryTemperatureLog] [end] */

#if defined( SHVIB_PAUSE_PEDOMETER )
	if (paused_pedometer) {
#if defined( CONFIG_SENSORS_AMI603 )
		pr_debug("[shvibrator] Restart a pedometer.\n");
		AMI602Pedometer_ReStart();
#elif defined( CONFIG_SH_YAS530 )
		pr_debug("[shvibrator] Restart a acclerometer.\n");
		SHMDS_Acclerometer_Control( SHMDS_ACC_RESTART );
#endif
		paused_pedometer = 0;
	} else {
#if defined( CONFIG_SENSORS_AMI603 )
		pr_debug("[shvibrator] Don't restart a pedometer.\n");
#elif defined( CONFIG_SH_YAS530 )
		pr_debug("[shvibrator] Don't restart a acclerometer.\n");
#endif
	}
#endif

	SHVIBRATOR_FUNC_LOG("END");
}

static void timed_vibrator_on(struct timed_output_dev *sdev)
{
	SHVIBRATOR_FUNC_LOG("START");

	if(schedule_work(&work_vibrator_on) != 1){
		pr_debug("[shvibrator] update vibrator on workqueue\n");
		cancel_work_sync(&work_vibrator_on);
		schedule_work(&work_vibrator_on);
	}

	SHVIBRATOR_FUNC_LOG("END");
}

static void timed_vibrator_off(struct timed_output_dev *sdev)
{
	SHVIBRATOR_FUNC_LOG("START");

	if(schedule_work(&work_vibrator_off) != 1){
		pr_debug("[shvibrator] update vibrator off workqueue\n");
		cancel_work_sync(&work_vibrator_off);
		schedule_work(&work_vibrator_off);
	}

	SHVIBRATOR_FUNC_LOG("END");
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	SHVIBRATOR_FUNC_LOG("START");

	pr_debug("[shvibrator] value=%d.\n", value);
	SHVIBRATOR_DBG_LOG("value = %d", value);
	
	hrtimer_cancel(&vibe_timer);
	
	sh_vibrator_reqtime = value;
	cancel_work_sync(&work_hrtimer);
	cancel_work_sync(&work_vibrator_on);
	cancel_work_sync(&work_vibrator_off);
	
	if (value == 0)
		timed_vibrator_off(dev);
	else {
		wake_lock(&shvibrator_linear_wake_lock);
		SHVIBRATOR_DBG_LOG("shvibrator_linear_wake_lock : lock ");
		sh_vibrator_reqtime = (value > 15000 ? 15000 : value);

#if defined( SHVIB_PAUSE_PEDOMETER )
#if defined( CONFIG_SENSORS_AMI603 )
		if (value >= 700) {
			pause_pedometer = 1;
		} else {
			pause_pedometer = 0;
		}
#elif defined( CONFIG_SH_YAS530 )
		pause_pedometer = 1;
#endif
#endif
		
		timed_vibrator_on(dev);
		
	}

	SHVIBRATOR_FUNC_LOG("END");
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	SHVIBRATOR_FUNC_LOG("START");

	if (hrtimer_active(&vibe_timer)) {
		ktime_t r = hrtimer_get_remaining(&vibe_timer);
		struct timeval t = ktime_to_timeval(r);
		SHVIBRATOR_FUNC_LOG("END");
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	SHVIBRATOR_FUNC_LOG("END");

	return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	SHVIBRATOR_FUNC_LOG("START");

	pr_debug("[shvibrator] timer stop.\n");
	/* add work queue for hrtimer -> */
	schedule_work(&work_hrtimer);
	/* add work queue for hrtimer <- */

	SHVIBRATOR_FUNC_LOG("END");

	return HRTIMER_NORESTART;
}

/* add work queue for hrtimer -> */
static void hrtimer_work_func(struct work_struct *work)
{
	SHVIBRATOR_FUNC_LOG("START");

	pmic_vibrator_off(NULL);

	SHVIBRATOR_FUNC_LOG("END");
}
/* add work queue for hrtimer <- */

int shvibrator_i2c_read(struct i2c_client *client,
									int reg, u8 *data, int size)
{
	int rc;
	u8 buf[2];
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags= 0,
			.len  = 1,
			.buf  = buf,
		},
		{
			.addr = client->addr,
			.flags= I2C_M_RD,
			.len  = size,
			.buf  = data,
		}
	};

	SHVIBRATOR_FUNC_LOG("START");

	buf[0] = reg;
	rc = i2c_transfer(client->adapter, msg, 2);
	SHVIBRATOR_DBG_LOG("i2c read: reg = 0x%02X  data = 0x%02X",reg,*data);
	if(rc != 2){
		dev_err(&client->dev,
		       "shvibrator_i2c_read FAILED: read of register %d\n", reg);
		rc = -1;
		goto i2c_rd_exit;
	}
	
i2c_rd_exit:

	SHVIBRATOR_FUNC_LOG("END");

	return rc;
}

int shvibrator_i2c_write(struct i2c_client *client,
									int reg, u8 data)
{
	int rc;
	u8 buf[2];
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags= 0,
		.len  = 2,
		.buf  = buf,
	};

	SHVIBRATOR_FUNC_LOG("START");
	SHVIBRATOR_DBG_LOG("i2c write: reg = 0x%02X  data = 0x%02X",reg,data);
	
	buf[0] = reg;
	buf[1] = data;
	rc = i2c_transfer(client->adapter, &msg, 1);
	if(rc != 1){
		dev_err(&client->dev,
		       "shvibrator_i2c_write FAILED: writing to reg %d\n", reg);
		rc = -1;
	}
	
	SHVIBRATOR_FUNC_LOG("END");

	return rc;
}

static const struct i2c_device_id shvibrator_i2c_id[] = {
	{ SHVIB_I2C_DRIVER_NAME, 0 },
	{ }
};

static struct i2c_driver shvibrator_i2c_driver = {
	.driver		= {
		.name = SHVIB_I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe		= shvibrator_i2c_probe,
	.remove		= __devexit_p(shvibrator_i2c_remove),
	.suspend	= shvibrator_i2c_suspend,
	.id_table	= shvibrator_i2c_id
};

static int shvibrator_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int rc;
	struct shvib_i2c_data *sd;
	
		if(vib_data){
		rc = -EPERM;
		goto probe_exit;
	}
	
	SHVIBRATOR_FUNC_LOG("START");

	sd = (struct shvib_i2c_data*)kzalloc(sizeof *sd, GFP_KERNEL);
	if(!sd){
		rc = -ENOMEM;
		goto probe_exit;
	}
	vib_data = sd;
	i2c_set_clientdata(client, sd);
	sd->client_p = client;
	
	SHVIBRATOR_FUNC_LOG("END");

	return 0;
	
probe_exit:

	SHVIBRATOR_FUNC_LOG("END");

	return rc;
}

static int shvibrator_i2c_remove(struct i2c_client *client)
{
	SHVIBRATOR_FUNC_LOG("START");

	return 0;

	SHVIBRATOR_FUNC_LOG("END");
}

static int shvibrator_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	SHVIBRATOR_FUNC_LOG("START");

	hrtimer_cancel(&vibe_timer);
	cancel_work_sync(&work_hrtimer);
	cancel_work_sync(&work_vibrator_on);
	cancel_work_sync(&work_vibrator_off);
	set_pmic_vibrator(0);

	SHVIBRATOR_FUNC_LOG("END");
	return 0;
}

int __init shvibrator_i2c_init(void)
{
	int ret;

	SHVIBRATOR_FUNC_LOG("START");

	if (vibrator_init){
		SHVIBRATOR_FUNC_LOG("END");
		return 0;
	}
	ret = i2c_add_driver(&shvibrator_i2c_driver);
	vibrator_init = 1;

	SHVIBRATOR_FUNC_LOG("END");

	return ret;
}

void shvibrator_i2c_exit(void)
{
	SHVIBRATOR_FUNC_LOG("START");

	if (vibrator_init)
		i2c_del_driver(&shvibrator_i2c_driver);

	SHVIBRATOR_FUNC_LOG("END");
}

static struct timed_output_dev pmic_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

static int __init msm_init_pmic_vibrator(void)
{
	int rc;
	int ret;

	struct pm_gpio vib_config = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 0,
		.pull			= PM_GPIO_PULL_NO,
		.vin_sel		= PM_GPIO_VIN_S4,
		.out_strength	= PM_GPIO_STRENGTH_LOW,
		.function		= PM_GPIO_FUNC_NORMAL,
		.inv_int_pol	= 0,
		.disable_pin	= 0,
	};

	SHVIBRATOR_FUNC_LOG("START");

	INIT_WORK(&work_vibrator_on, pmic_vibrator_on);
	INIT_WORK(&work_vibrator_off, pmic_vibrator_off);
	/* add work queue for hrtimer -> */
	INIT_WORK(&work_hrtimer, hrtimer_work_func);
	/* add work queue for hrtimer <- */

#if CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2)
	if( (sh_get_hw_revision() & 0x07) == HW_ES1 ){
		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(SHVIB_RST_PDN),&vib_config);

		if(rc < 0){
			pr_err("linearVIB reset :gpio_config error\n");
		}
	}
#endif /*  CCONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */

	rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(SHVIB_PDN),&vib_config);

	if(rc < 0){
		pr_err("linearVIB:gpio_config error\n");
	}

	rc = shvibrator_i2c_init();
	if(rc < 0){
		printk(KERN_WARNING "linearVIB:shvibrator_i2c_init error\n");
	}

	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	mutex_init(&sh_vibrator_pmic_mutex);

	ret = timed_output_dev_register(&pmic_vibrator);

	reg= regulator_get(pmic_vibrator.dev, id);
	if (IS_ERR(reg)) {
		SHVIBRATOR_ERR_LOG("regulator get error");
		SHVIBRATOR_FUNC_LOG("END");
		return -1;
	}

	regulator_set_voltage(reg, min_uV, max_uV);
	wake_lock_init(&shvibrator_linear_wake_lock, WAKE_LOCK_SUSPEND, "shvibrator_linear_wake_lock");

	SHVIBRATOR_FUNC_LOG("END");

	return ret;

}
module_init( msm_init_pmic_vibrator );

static void __exit msm_exit_pmic_vibrator(void)
{
	SHVIBRATOR_FUNC_LOG("START");

    shvibrator_i2c_exit();

	regulator_put(reg);

	timed_output_dev_unregister(&pmic_vibrator);
	wake_unlock(&shvibrator_linear_wake_lock);
	wake_lock_destroy(&shvibrator_linear_wake_lock);

	SHVIBRATOR_FUNC_LOG("END");
}
module_exit( msm_exit_pmic_vibrator );

MODULE_AUTHOR("SHARP CORPORATION");
MODULE_DESCRIPTION("linear vibrator driver");
MODULE_LICENSE("GPL v2");
