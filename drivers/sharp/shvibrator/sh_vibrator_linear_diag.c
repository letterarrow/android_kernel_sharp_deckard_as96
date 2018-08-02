/* drivers/sharp/shvibrator/sh_vibrator_diag.c
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
/* CONFIG_SH_AUDIO_DRIVER newly created */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <mach/pmic.h>

#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/delay.h>

#include <linux/slab.h>
#include <linux/i2c.h>

#include <sharp/sh_smem.h>

#define SHVIB_DRIVER_NAME		"shvibrator"

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
extern int shvibrator_func_log;
extern int shvibrator_debug_log;
extern int shvibrator_error_log;

#ifdef CONFIG_ANDROID_ENGINEERING
    module_param(shvibrator_func_log,  int, 0600);
    module_param(shvibrator_debug_log, int, 0600);
    module_param(shvibrator_error_log, int, 0600);
#endif  /* CONFIG_ANDROID_ENGINEERING */

#define SHVIBRATOR_FUNC_LOG(fmt, args...) \
	if( shvibrator_func_log == 1 ){ \
		printk( KERN_DEBUG "[shvibrator][diag][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#define SHVIBRATOR_DBG_LOG(fmt, args...) \
	if( shvibrator_debug_log == 1 ){ \
		printk( KERN_DEBUG "[shvibrator][diag][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#define SHVIBRATOR_ERR_LOG(fmt, args...) \
	if( shvibrator_error_log == 1 ){ \
		printk( KERN_DEBUG "[shvibrator][diag]" fmt "\n",## args ); \
	} \
	else if( shvibrator_error_log == 2 ){ \
		printk( KERN_DEBUG "[shvibrator][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}


static dev_t 				shvib_devid;
static struct class*		shvib_class;
static struct device*		shvib_device;
struct cdev 				shvib_cdev;

static struct regulator *reg;
static const char *id = "8921_l10";
static int min_uV = 3000000, max_uV = 3000000;

extern int __init shvibrator_i2c_init(void);
extern void shvibrator_i2c_exit(void);
extern int shvibrator_i2c_read(struct i2c_client *client,
									int reg, u8 *data, int size);
extern int shvibrator_i2c_write(struct i2c_client *client,
									int reg, u8 data);
struct shvib_i2c_data {
	struct i2c_client *client_p;
};
extern struct shvib_i2c_data *vib_data;

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
		
		usleep(30 * 1000);

#if CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2)
		if( (sh_get_hw_revision() & 0x07) == HW_ES1 ){
			gpio_direction_output(PM8921_GPIO_PM_TO_SYS(SHVIB_RST_PDN), 0);
			SHVIBRATOR_DBG_LOG("PM8921_GPIO %d Low",SHVIB_RST_PDN);
		}else if (regulator_is_enabled(reg)) {
			regulator_disable(reg);
			SHVIBRATOR_DBG_LOG("PM8921_L10 OFF");
		}
#else /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */
		if (regulator_is_enabled(reg)) {
			regulator_disable(reg);
			SHVIBRATOR_DBG_LOG("PM8921_L10 OFF");
		}
#endif /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */
	}
	SHVIBRATOR_FUNC_LOG("END");

	return;

set_pmic_vibrator_i2c_err:

	SHVIBRATOR_ERR_LOG("i2c write error");
	SHVIBRATOR_FUNC_LOG("END");

	return;

}

static int vib_connect_check(void)
{

int rc = -1;
int i2c_err = 0;

	SHVIBRATOR_FUNC_LOG("START");

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
		u8 getval;

		if( shvibrator_i2c_write(vib_data->client_p, 0x03, 0x05) < 0)
			i2c_err++;
		if( shvibrator_i2c_read(vib_data->client_p, 0x03, &getval ,1) < 0)
			i2c_err++;

		SHVIBRATOR_DBG_LOG("reg 0x03 value = 0x%02X i2c_err = %d",getval,i2c_err);

    	if((getval == 0x05) && (i2c_err == 0))
    	{
			rc = 0;
		}

	}

#if CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2)
		if( (sh_get_hw_revision() & 0x07) == HW_ES1 ){
			gpio_direction_output(PM8921_GPIO_PM_TO_SYS(SHVIB_RST_PDN), 0);
			SHVIBRATOR_DBG_LOG("PM8921_GPIO %d Low",SHVIB_RST_PDN);
		}else if (regulator_is_enabled(reg)) {
			regulator_disable(reg);
			SHVIBRATOR_DBG_LOG("PM8921_L10 OFF");
		}
#else /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */
		if (regulator_is_enabled(reg)) {
			regulator_disable(reg);
			SHVIBRATOR_DBG_LOG("PM8921_L10 OFF");
		}
#endif /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */
	
	SHVIBRATOR_FUNC_LOG("END");

	return rc;
}


static int shvib_open(struct inode *inode, struct file *file)
{
	SHVIBRATOR_FUNC_LOG("START");
	SHVIBRATOR_FUNC_LOG("END");

	return 0;
}

static ssize_t shvib_write(struct file *filp, 
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char cmd;

	SHVIBRATOR_FUNC_LOG("START");

	pr_debug("[shvib]shvib_write(%s) start\n", ubuf);
	
	if(get_user(cmd, ubuf)){
		SHVIBRATOR_ERR_LOG("get_user error");
		SHVIBRATOR_FUNC_LOG("END");
		return -EFAULT;
	}

	pr_debug("[shvib]shvib_write() cmd = %d\n", cmd);
	
	if(cmd == '1'){
		/* vibrator on */
		set_pmic_vibrator(1);
	}
	else if(cmd == '2'){
		/* i2c connect check */
		if(vib_connect_check() < 0){
			SHVIBRATOR_ERR_LOG("i2c check error");
			SHVIBRATOR_FUNC_LOG("END");
			return -1;
		}
	}else{
		/* vibrator off */
		set_pmic_vibrator(0);
	}
	
	SHVIBRATOR_FUNC_LOG("END");

	return cnt;
}

static int shvib_release(struct inode *inode, struct file *file)
{
	SHVIBRATOR_FUNC_LOG("START");
	SHVIBRATOR_FUNC_LOG("END");

	return 0;
}

static const struct file_operations shvib_fileops = {
	.owner   = THIS_MODULE,
	.open    = shvib_open,
	.write   = shvib_write,
	.release = shvib_release,
};

static int __init init_pmic_shvibrator(void)
{
	int rc;

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

#if CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2)
	if( (sh_get_hw_revision() & 0x07) == HW_ES1 ){
		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(SHVIB_RST_PDN),&vib_config);

		if(rc < 0){
			pr_err("linearVIB reset :gpio_config error\n");
		}
	}
#endif /* CONFIG_SHVIBRATOR_MODEL_TYPE == (MODEL_TYPE_D2) */

	rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(SHVIB_PDN),&vib_config);

	if(rc < 0){
		pr_err("linearVIB:gpio_config error\n");
		goto err_gpio_request;
	}

	rc = alloc_chrdev_region(&shvib_devid, 0, 1, SHVIB_DRIVER_NAME);
	if(rc < 0){
		pr_err("shvib:alloc_chrdev_region error\n");
		SHVIBRATOR_FUNC_LOG("END");
		return rc;
	}
	rc = shvibrator_i2c_init();
	if(rc < 0){
		printk(KERN_WARNING "linearVIB:shvibrator_i2c_init error\n");
	}

	shvib_class = class_create(THIS_MODULE, SHVIB_DRIVER_NAME);
	if (IS_ERR(shvib_class)) {
		rc = PTR_ERR(shvib_class);
		pr_err("shvib:class_create error\n");
		goto error_vid_class_create;
	}

	shvib_device = device_create(shvib_class, NULL, 
								 shvib_devid, &shvib_cdev, 
								 SHVIB_DRIVER_NAME);
	if (IS_ERR(shvib_device)) {
		rc = PTR_ERR(shvib_device);
		pr_err("shvib:device_create error\n");
		goto error_vid_class_device_create;
	}
	
	cdev_init(&shvib_cdev, &shvib_fileops);
	shvib_cdev.owner = THIS_MODULE;
	rc = cdev_add(&shvib_cdev, shvib_devid, 1);
	if(rc < 0){
		pr_err("shvib:cdev_add error\n");
		goto err_via_cdev_add;
	}

	reg= regulator_get(shvib_device, id);
	if (IS_ERR(reg)) {
		SHVIBRATOR_ERR_LOG("regulator get error");
		SHVIBRATOR_FUNC_LOG("END");
		return -1;
	}

    regulator_set_voltage(reg, min_uV, max_uV);

	pr_debug("[shvib]shvib_init() done\n");

	SHVIBRATOR_FUNC_LOG("END");

	return 0;
	
err_gpio_request:
	gpio_free(SHVIB_PDN);
err_via_cdev_add:
	cdev_del(&shvib_cdev);
error_vid_class_device_create:
	class_destroy(shvib_class);
error_vid_class_create:
	unregister_chrdev_region(shvib_devid, 1);

	SHVIBRATOR_FUNC_LOG("END");

	return rc;
}
module_init( init_pmic_shvibrator );

static void __exit exit_pmic_shvibrator(void)
{
	SHVIBRATOR_FUNC_LOG("START");

    shvibrator_i2c_exit();

	regulator_put(reg);

	cdev_del(&shvib_cdev);
	class_destroy(shvib_class);
	unregister_chrdev_region(shvib_devid, 1);

	SHVIBRATOR_FUNC_LOG("END");
}
module_exit( exit_pmic_shvibrator );

MODULE_AUTHOR("SHARP CORPORATION");
MODULE_DESCRIPTION("linear vibrator driver for diag");
MODULE_LICENSE("GPL v2");
