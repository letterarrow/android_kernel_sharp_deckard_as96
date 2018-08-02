/* driver/sharp/wtap/wtap_kerl.c  (W-Tap Sensor Driver)
 *
 * Copyright (C) 2012 SHARP CORPORATION All rights reserved.
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
/* SHARP W-TAP SENSOR DRIVER FOR KERNEL MODE                                 */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
#include <linux/proc_fs.h>
#include <linux/completion.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/freezer.h>
#include <linux/mutex.h>
//#include <mach/gpio.h>
#include <linux/gpio.h>
#include <mach/vreg.h>
#include <sharp/sh_smem.h>
#include <sharp/wtap_kerl.h>
#include <sharp/sh_boot_manager.h>
#include <linux/module.h>

#include "wtap_lis3dsh.h"

#ifdef CONFIG_SHTERM
#define WTAP_SHTERM_ENABLE
#include "sharp/shterm_k.h"
#endif

/* ------------------------------------------------------------------------- */
/* MACRAOS                                                                   */
/* ------------------------------------------------------------------------- */

/* Hard */
#define PM8921_GPIO_BASE                NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)

#define WTAP_GPIO_INT                   PM8921_GPIO_PM_TO_SYS(9)
#define WTAP_GPIO_INT_FLAGS             (IRQF_TRIGGER_LOW | IRQF_DISABLED)

#define I2C_AUTO_INCREMENT  0x80        /* Autoincrement i2c address */

/* local define */
#define WTAP_IRQ_DISABLE        0
#define WTAP_IRQ_ENABLE         1

#define WTAP_DEV_NO_CHECK       0
#define WTAP_DEV_NO_EXIST       1
#define WTAP_DEV_EXIST          2

#define WTAP_INT_STATE_STANDBY  0
#define WTAP_INT_STATE_ACTIVE   1
#define WTAP_INT_STATE_CANCEL   2
#define WTAP_INT_STATE_DETECT   3

#define WTAP_IRQ_DISABLE    0
#define WTAP_IRQ_ENABLE     1

#define WTAP_ACC_WAIT           50

/* boot_mode flag */
static int wtap_product_mode = 0;

#if defined (CONFIG_ANDROID_ENGINEERING)
    module_param(wtap_product_mode, int, 0600);
#endif

static short wtap_param_version = 0;
#if defined (CONFIG_ANDROID_ENGINEERING)
    module_param(wtap_param_version, short, 0400);
#endif

static long wtap_wakelock_timeout = HZ / 10;
#if defined (CONFIG_ANDROID_ENGINEERING)
    module_param(wtap_wakelock_timeout, long, 0600);
#endif

/* counter */
static unsigned int wtap_read_request_cnt = 0;
static unsigned int wtap_read_cancel_cnt  = 0;
static unsigned int wtap_read_detect_cnt  = 0;
static unsigned int wtap_read_error_cnt   = 0;

#if defined (CONFIG_ANDROID_ENGINEERING)
    module_param(wtap_read_request_cnt, int, 0600);
    module_param(wtap_read_cancel_cnt,  int, 0600);
    module_param(wtap_read_detect_cnt,  int, 0600);
    module_param(wtap_read_error_cnt,   int, 0600);
#endif

/* Debug Log */
#define WTAP_FILE "wtap_kerl.c"

static int wtap_ctx_log = 0;
static int wtap_err_log = 1;
static int wtap_dbg_log = 0;
static int wtap_reg_log = 0;

#if defined (CONFIG_ANDROID_ENGINEERING)
    module_param(wtap_ctx_log, int, 0600);
    module_param(wtap_err_log, int, 0600);
    module_param(wtap_dbg_log, int, 0600);
    module_param(wtap_reg_log, int, 0600);
#endif

#define WTAP_CTX_LOG(fmt, args...) \
        if(wtap_ctx_log == 1) { \
            printk("[WTAP_CONTEXT][%s][%s] " fmt "\n", WTAP_FILE, __func__, ## args); \
        }

#define WTAP_ERR_LOG(fmt, args...) \
        if(wtap_err_log == 1) { \
            printk("[WTAP_ERROR][%s][%s] " fmt "\n", WTAP_FILE, __func__, ## args); \
        }

#define WTAP_DBG_LOG(fmt, args...) \
        if(wtap_dbg_log == 1) { \
            printk("[WTAP_DEBUG][%s][%s] " fmt "\n", WTAP_FILE, __func__, ## args); \
        }

#define WTAP_REG_LOG(fmt, args...) \
        if(wtap_reg_log == 1) { \
            printk("[WTAP_REG][%s][%s] " fmt "\n", WTAP_FILE, __func__, ## args); \
        }

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

static int  wtap_FOPS_open(struct inode *inode, struct file *filp);
static int  wtap_FOPS_release(struct inode *inode, struct file *filp);
static long wtap_FOPS_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int  wtap_FOPS_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos);
static int  wtap_IOCTL_setting_write(void __user *argp);
static int  wtap_IOCTL_setting_read(void __user *argp);
static int  wtap_IOCTL_setting_clear(void);
static int  wtap_IOCTL_read(void __user *argp);
static int  wtap_IOCTL_read_stop(void);
static int  wtap_IOCTL_reg_write(void __user *argp);
static int  wtap_IOCTL_reg_read(void __user *argp);
static int  wtap_IOCTL_diag_sensor_read(void __user *argp);
static int  wtap_IOCTL_data_set(void __user *argp);
static int wtap_IOCTL_wakelock_release(void);
static void wtap_SYS_delay_us(unsigned long usec);
static void wtap_SYS_host_gpio_init(void);
static void wtap_SYS_host_gpio_exit(void);
static int  wtap_SYS_reg_i2c_init(struct i2c_client *client, const struct i2c_device_id * devid);
static int  wtap_SYS_reg_i2c_exit(struct i2c_client *client);
static int  wtap_SYS_reg_i2c_write(unsigned char addr, unsigned char data);
static int  wtap_SYS_reg_i2c_read(unsigned char addr, unsigned char *data);
static int  wtap_SYS_reg_i2c_seq_read(unsigned char addr, unsigned char *data, int len);
static void wtap_SYS_wake_lock_init(void);
static void wtap_SYS_wake_lock(void);
static void wtap_SYS_wake_unlock(void);
static void wtap_SYS_wake_lock_timeout(long timeout);
static int  wtap_INT_request_irq(void);
static void wtap_INT_free_irq(void);
static int  wtap_INT_set_irq(int enable);
static irqreturn_t wtap_INT_isr(int irq_num, void *data);
static int  wtap_check_exist(void);
static void wtap_init_state(void);
static void wtap_get_boot_mode(void);
static void wtap_work_func(struct work_struct *work);

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static int wtap_driver_open_counter = 0;
static unsigned char wtap_dev_exist = WTAP_DEV_NO_CHECK;
static unsigned char wtap_int_state = WTAP_INT_STATE_STANDBY;
static wait_queue_head_t wtap_read_wait;
static int wtap_driver_set_data_counter = 0;
static atomic_t wtap_irq_flg = ATOMIC_INIT(WTAP_IRQ_DISABLE);
static struct work_struct wtap_work;
static struct mutex wtap_lock;
static struct wake_lock wtap_wake_lock;
static atomic_t wtap_wake_lock_counter = ATOMIC_INIT(0);
static struct wake_lock wtap_wake_lock_timeout;

static struct file_operations wtap_fops = {
    .owner          = THIS_MODULE,
    .open           = wtap_FOPS_open,
    .release        = wtap_FOPS_release,
    .unlocked_ioctl = wtap_FOPS_ioctl,
    .read           = wtap_FOPS_read,
};

static struct miscdevice wtap_device = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "wtap",
    .fops   = &wtap_fops,
};

typedef struct wtap_data_tag
{
    struct i2c_client *this_client;
} wtap_i2c_data_t;

static const struct i2c_device_id wtap_device_id[] = {
    { "wtap_i2c", 0 },
    { }
};

static struct i2c_driver wtap_driver = {
    .driver = {
        .owner  = THIS_MODULE,
        .name   = "wtap_i2c",
    },
    .class      = I2C_CLASS_HWMON,
    .probe      = wtap_SYS_reg_i2c_init,
    .id_table   = wtap_device_id,
    .remove     = wtap_SYS_reg_i2c_exit,
};

static wtap_i2c_data_t *wtap_i2c_p = NULL;

struct wtap_setting_data wtap_reg_data;

/* ------------------------------------------------------------------------- */
/* REGISTER SETTING                                                          */
/* ------------------------------------------------------------------------- */

static int  wtap_REG_initialize(void)
{
    int ret = WTAP_RESULT_SUCCESS;
    int i;

    for( i = 0; i < wtap_reg_data.init_num; i++ ) {
        if( wtap_reg_data.wtap_reg_initialize_tbl[i][0] == 0xFF ) {
            WTAP_REG_LOG("msleep: %d ms", wtap_reg_data.wtap_reg_initialize_tbl[i][1]);
            msleep(wtap_reg_data.wtap_reg_initialize_tbl[i][1]);
        }
        else {
            WTAP_REG_LOG("adr: %02x, data: %02x", wtap_reg_data.wtap_reg_initialize_tbl[i][0], wtap_reg_data.wtap_reg_initialize_tbl[i][1]);
            ret += wtap_SYS_reg_i2c_write( wtap_reg_data.wtap_reg_initialize_tbl[i][0], wtap_reg_data.wtap_reg_initialize_tbl[i][1] );
        }
    }
    
    if (ret != WTAP_RESULT_SUCCESS) {
        WTAP_ERR_LOG("<OTHER> i2c control error.");
        return WTAP_RESULT_FAILURE;
    }
    
    return WTAP_RESULT_SUCCESS;
}


static int  wtap_REG_setting_state(void)
{
    int ret = WTAP_RESULT_SUCCESS;
    int i;
    
    for( i = 0; i < wtap_reg_data.setting_num; i++ ) {
        WTAP_REG_LOG("adr: %02x, data: %02x", wtap_reg_data.wtap_reg_setting_state_tbl[i][0], wtap_reg_data.wtap_reg_setting_state_tbl[i][1]);
        ret += wtap_SYS_reg_i2c_write( wtap_reg_data.wtap_reg_setting_state_tbl[i][0], wtap_reg_data.wtap_reg_setting_state_tbl[i][1] );
    }

    if (ret != WTAP_RESULT_SUCCESS) {
        WTAP_ERR_LOG("<OTHER> i2c control error.");
        return WTAP_RESULT_FAILURE;
    }
    
    return WTAP_RESULT_SUCCESS;
}


static int  wtap_REG_enable(void)
{
    int ret = WTAP_RESULT_SUCCESS;
    int i;
    
    for( i = 0; i < wtap_reg_data.enable_num; i++ ) {
        WTAP_REG_LOG("adr: %02x, data: %02x", wtap_reg_data.wtap_reg_enable_tbl[i][0], wtap_reg_data.wtap_reg_enable_tbl[i][1]);
        ret += wtap_SYS_reg_i2c_write( wtap_reg_data.wtap_reg_enable_tbl[i][0], wtap_reg_data.wtap_reg_enable_tbl[i][1] );
    }
    
    if (ret != WTAP_RESULT_SUCCESS) {
        WTAP_ERR_LOG("<OTHER> i2c control error.");
        return WTAP_RESULT_FAILURE;
    }
    
    return WTAP_RESULT_SUCCESS;
}


static int  wtap_REG_disable(void)
{
    int ret = WTAP_RESULT_SUCCESS;
    int i;
    
    for( i = 0; i < wtap_reg_data.disable_num; i++ ) {
        WTAP_REG_LOG("adr: %02x, data: %02x", wtap_reg_data.wtap_reg_disable_tbl[i][0], wtap_reg_data.wtap_reg_disable_tbl[i][1]);
        ret += wtap_SYS_reg_i2c_write( wtap_reg_data.wtap_reg_disable_tbl[i][0], wtap_reg_data.wtap_reg_disable_tbl[i][1] );
    }
    
    if (ret != WTAP_RESULT_SUCCESS) {
        WTAP_ERR_LOG("<OTHER> i2c control error.");
        return WTAP_RESULT_FAILURE;
    }
    
    return WTAP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* FOPS                                                                      */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* wtap_FOPS_open                                                            */
/* ------------------------------------------------------------------------- */

static int wtap_FOPS_open(struct inode *inode, struct file *filp)
{
    if (wtap_check_exist() != WTAP_RESULT_SUCCESS) {
        return WTAP_RESULT_SUCCESS;
    }
    
    WTAP_CTX_LOG("start");
    
    if (wtap_driver_open_counter == 0) {
        printk("[WTAP] new open w-tap device driver.\n");
    }
    
    wtap_driver_open_counter++;
    
    WTAP_CTX_LOG("end");
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_FOPS_release                                                         */
/* ------------------------------------------------------------------------- */

static int wtap_FOPS_release(struct inode *inode, struct file *filp)
{
    if (wtap_check_exist() != WTAP_RESULT_SUCCESS) {
        return WTAP_RESULT_SUCCESS;
    }
    
    WTAP_CTX_LOG("start");
    
    if (wtap_driver_open_counter > 0) {
        wtap_driver_open_counter--;
        if (wtap_driver_open_counter == 0) {
            printk("[WTAP] all close w-tap device driver.\n");
            wtap_SYS_wake_unlock();
        }
    }
    
    WTAP_CTX_LOG("end");
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_FOPS_ioctl                                                           */
/* ------------------------------------------------------------------------- */

static long wtap_FOPS_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret;
    void __user *argp = (void __user*)arg;
    
    if (wtap_check_exist() != WTAP_RESULT_SUCCESS) {
        return WTAP_RESULT_SUCCESS;
    }
    
    switch (cmd) {
    case WTAP_IOCTL_SETTING_WRITE:
        ret = wtap_IOCTL_setting_write(argp);
        break;
    case WTAP_IOCTL_SETTING_READ:
        ret = wtap_IOCTL_setting_read(argp);
        break;
    case WTAP_IOCTL_SETTING_CLEAR:
        ret = wtap_IOCTL_setting_clear();
        break;
    case WTAP_IOCTL_READ:
        ret = wtap_IOCTL_read(argp);
        break;
    case WTAP_IOCTL_READ_STOP:
        ret = wtap_IOCTL_read_stop();
        break;
    case WTAP_IOCTL_REG_WRITE:
        ret = wtap_IOCTL_reg_write(argp);
        break;
    case WTAP_IOCTL_REG_READ:
        ret = wtap_IOCTL_reg_read(argp);
        break;
    case WTAP_IOCTL_DIAG_READ:
        ret = wtap_IOCTL_diag_sensor_read(argp);
        break;
    case WTAP_IOCTL_DATA_SET:
        ret = wtap_IOCTL_data_set(argp);
        break;
    case WTAP_IOCTL_WAKELOCK:
        ret = wtap_IOCTL_wakelock_release();
        break;
    default:
        WTAP_ERR_LOG("<INVALID_VALUE> cmd(0x%08x).\n", cmd);
        ret = -EFAULT;
        break;
    }
    
    return ret;
}


/* ------------------------------------------------------------------------- */
/* wtap_FOPS_read                                                            */
/* ------------------------------------------------------------------------- */

static int wtap_FOPS_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    if (wtap_check_exist() != WTAP_RESULT_SUCCESS) {
        return WTAP_RESULT_SUCCESS;
    }
    
    WTAP_CTX_LOG("start");
    WTAP_CTX_LOG("end");
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* IOCTL                                                                     */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_setting_write                                                  */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_setting_write(void __user *argp)
{
    int ret;
    struct wtap_setting_data setting_data;
    
    WTAP_CTX_LOG("start");
    
    ret = copy_from_user(&setting_data, argp, sizeof(struct wtap_setting_data));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_from_user(struct wtap_setting_data). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }

    if (setting_data.init_num > WTAP_REG_TBL_INIT) {
        WTAP_ERR_LOG("<INVALID_VALUE> init_num = %d.", setting_data.init_num);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    if (setting_data.setting_num > WTAP_REG_TBL_SETTING_STATE) {
        WTAP_ERR_LOG("<INVALID_VALUE> setting_num = %d.", setting_data.setting_num);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    if (setting_data.enable_num > WTAP_REG_TBL_ENABLE) {
        WTAP_ERR_LOG("<INVALID_VALUE> enable_num = %d.", setting_data.enable_num);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    if (setting_data.disable_num > WTAP_REG_TBL_DISABLE) {
        WTAP_ERR_LOG("<INVALID_VALUE> disable_num = %d.", setting_data.disable_num);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    if (setting_data.wtap_setting_state.axis >= NUM_WTAP_AXIS) {
        WTAP_ERR_LOG("<INVALID_VALUE> axis = %d.", setting_data.wtap_setting_state.axis);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE_USER;
    }
    if (setting_data.wtap_setting_state.strength >= NUM_WTAP_STRENGTH) {
        WTAP_ERR_LOG("<INVALID_VALUE> strength = %d.", setting_data.wtap_setting_state.strength);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE_USER;
    }
    if (setting_data.wtap_setting_state.interval >= NUM_WTAP_INTERVAL) {
        WTAP_ERR_LOG("<INVALID_VALUE> interval = %d.", setting_data.wtap_setting_state.interval);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE_USER;
    }
    
    if (wtap_int_state != WTAP_INT_STATE_STANDBY) {
        WTAP_ERR_LOG("<OTHER> wtap_int_state = %d.", (int)wtap_int_state);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE_INT;
    }
    
    if (setting_data.wtap_setting_state.axis == WTAP_AXIS_NONE) {
        setting_data.wtap_setting_state.axis = wtap_reg_data.wtap_setting_state.axis;
    }

    if (setting_data.wtap_setting_state.strength == WTAP_STRENGTH_00) {
        setting_data.wtap_setting_state.strength = wtap_reg_data.wtap_setting_state.strength;
    }

    if (setting_data.wtap_setting_state.interval == WTAP_INTERVAL_00) {
        setting_data.wtap_setting_state.interval = wtap_reg_data.wtap_setting_state.interval;
    }
    
    WTAP_DBG_LOG("axis:%d, strength:%d, interval:%d", 
        setting_data.wtap_setting_state.axis, setting_data.wtap_setting_state.strength, setting_data.wtap_setting_state.interval);

    if ((setting_data.wtap_setting_state.axis == wtap_reg_data.wtap_setting_state.axis) &&
        (setting_data.wtap_setting_state.strength == wtap_reg_data.wtap_setting_state.strength) &&
        (setting_data.wtap_setting_state.interval == wtap_reg_data.wtap_setting_state.interval))
    {
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_SUCCESS;
    }

    if (setting_data.wtap_setting_state.strength == wtap_reg_data.wtap_setting_state.strength) {
        wtap_reg_data.wtap_setting_state.axis       = setting_data.wtap_setting_state.axis;
        wtap_reg_data.wtap_setting_state.interval   = setting_data.wtap_setting_state.interval;
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_SUCCESS;
    }
    
    memcpy(&wtap_reg_data, &setting_data, sizeof(struct wtap_setting_data));
    wtap_param_version = wtap_reg_data.version;

    wtap_REG_initialize();
    
    WTAP_CTX_LOG("end");
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_setting_read                                                   */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_setting_read(void __user *argp)
{
    int ret;
    struct w_tap_setting_param r_setting_param;
    
    WTAP_CTX_LOG("start");
    
    ret = copy_from_user(&r_setting_param, argp, sizeof(struct w_tap_setting_param));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_from_user(struct w_tap_setting_param). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    r_setting_param.axis     = wtap_reg_data.wtap_setting_state.axis;
    r_setting_param.strength = wtap_reg_data.wtap_setting_state.strength;
    r_setting_param.interval = wtap_reg_data.wtap_setting_state.interval;

    WTAP_DBG_LOG("axis:%d, strength:%d, interval:%d", r_setting_param.axis, r_setting_param.strength, r_setting_param.interval);
    
    ret = copy_to_user(argp, &r_setting_param, sizeof(struct w_tap_setting_param));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_to_user(struct w_tap_setting_param). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    WTAP_CTX_LOG("end");
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_setting_clear                                                  */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_setting_clear(void)
{
    WTAP_CTX_LOG("start");
    
    if (wtap_int_state != WTAP_INT_STATE_STANDBY) {
        WTAP_ERR_LOG("<OTHER> wtap_int_state = %d.", (int)wtap_int_state);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE_INT;
    }
    
    wtap_init_state();
    
    wtap_REG_setting_state();
    
    WTAP_CTX_LOG("end");
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_read                                                           */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_read(void __user *argp)
{
    int ret;
    unsigned char r_reg_tap;
    unsigned char r_reg_val[6];
    struct w_tap_read_data r_read_data;
    
    WTAP_CTX_LOG("start");
    
    wtap_read_request_cnt++;
    
    ret = copy_from_user(&r_read_data, argp, sizeof(struct w_tap_read_data));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_from_user(struct w_tap_read_data). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    mutex_lock(&wtap_lock);

    if (wtap_int_state != WTAP_INT_STATE_STANDBY) {
        WTAP_ERR_LOG("<OTHER> wtap_int_state = %d.", (int)wtap_int_state);
        mutex_unlock(&wtap_lock);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE_INT;
    }
    
    wtap_SYS_wake_lock();

    wtap_int_state = WTAP_INT_STATE_ACTIVE;
    
    wtap_REG_setting_state();
    wtap_REG_enable();
    
    wtap_INT_set_irq(WTAP_IRQ_ENABLE);

    #ifdef WTAP_SHTERM_ENABLE
    shterm_k_set_info(SHTERM_INFO_WTAP, 1);
    #endif

    wtap_SYS_wake_unlock();

    mutex_unlock(&wtap_lock);

    ret = wait_event_freezable(wtap_read_wait, (wtap_int_state != WTAP_INT_STATE_ACTIVE) );
    
    if (ret != 0) {
        WTAP_DBG_LOG("SYSTEM_BUSY wakelock\n");
        wtap_SYS_wake_lock();
    }
    
    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_OUTS2   , &r_reg_tap    );
    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_OUT_X_L , &r_reg_val[0] );
    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_OUT_X_H , &r_reg_val[1] );
    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_OUT_Y_L , &r_reg_val[2] );
    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_OUT_Y_H , &r_reg_val[3] );
    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_OUT_Z_L , &r_reg_val[4] );
    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_OUT_Z_H , &r_reg_val[5] );
    
    WTAP_DBG_LOG("[addr:7Fh] OUTS2   = 0x%02x.", r_reg_tap);

    WTAP_DBG_LOG("wait_event ret = %d.", ret);
    
    mutex_lock(&wtap_lock);

    wtap_REG_disable();
    

    if (ret != 0) {
        WTAP_DBG_LOG("SYSTEM_BUSY\n");
        wtap_INT_set_irq(WTAP_IRQ_DISABLE);

        r_read_data.Result = WTAP_SYSTEM_BUSY;
        wtap_read_error_cnt++;
    }
    else {
        if (wtap_int_state == WTAP_INT_STATE_CANCEL) {
            WTAP_DBG_LOG("STATE CANCEL\n");
            r_read_data.Result = WTAP_USER_CANCEL;
            wtap_read_cancel_cnt++;
        }
        else if(wtap_int_state == WTAP_INT_STATE_DETECT) {
            WTAP_DBG_LOG("STATE DETECT\n");
            r_read_data.Result = WTAP_INTRRUPT_WTAP;
            wtap_read_detect_cnt++;
            
            wtap_SYS_wake_lock_timeout(wtap_wakelock_timeout);
        }
        else {
            WTAP_DBG_LOG("SYSTEM_ERR\n");
            r_read_data.Result = WTAP_SYSTEM_ERROR;
            wtap_read_error_cnt++;
        }
    }
    wtap_int_state = WTAP_INT_STATE_STANDBY;

    #ifdef WTAP_SHTERM_ENABLE
    shterm_k_set_info(SHTERM_INFO_WTAP, 0);
    #endif
    
    ret = copy_to_user(argp, &r_read_data, sizeof(struct w_tap_read_data));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_to_user(struct w_tap_read_data). [ret = %d]", ret);

        mutex_unlock(&wtap_lock);

        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }

    
    mutex_unlock(&wtap_lock);
    
    
    WTAP_CTX_LOG("end");
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_read_stop                                                      */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_read_stop(void)
{
    WTAP_CTX_LOG("start");
    
    mutex_lock(&wtap_lock);

    if (wtap_int_state != WTAP_INT_STATE_ACTIVE) {
        WTAP_DBG_LOG("status not Active");
        mutex_unlock(&wtap_lock);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_SUCCESS;
    }
    
    wtap_SYS_wake_lock();

    wtap_INT_set_irq(WTAP_IRQ_DISABLE);
    
    wtap_int_state = WTAP_INT_STATE_CANCEL;
    
    wake_up_interruptible(&wtap_read_wait);
    
    mutex_unlock(&wtap_lock);

    WTAP_CTX_LOG("end");
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_reg_write                                                      */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_reg_write(void __user *argp)
{
    int ret;
    struct wtap_sensor_reg w_reg_data;
    
    WTAP_CTX_LOG("start");
    
    ret = copy_from_user(&w_reg_data, argp, sizeof(struct wtap_sensor_reg));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_from_user(struct wtap_sensor_reg). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    ret = wtap_SYS_reg_i2c_write(w_reg_data.addr, w_reg_data.val);
    if (ret != WTAP_RESULT_SUCCESS) {
        WTAP_ERR_LOG("<RESULT_FAILURE> wtap_SYS_reg_i2c_write(addr:0x%02x, val:0x%02x). [ret = %d]", w_reg_data.addr, w_reg_data.val, ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    WTAP_CTX_LOG("end");
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_reg_read                                                       */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_reg_read(void __user *argp)
{
    int ret;
    struct wtap_sensor_reg r_reg_data;
    
    WTAP_CTX_LOG("start");
    
    ret = copy_from_user(&r_reg_data, argp, sizeof(struct wtap_sensor_reg));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_from_user(struct wtap_sensor_reg). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    r_reg_data.val = 0x00;
    
    ret = wtap_SYS_reg_i2c_read(r_reg_data.addr, &(r_reg_data.val));
    if (ret != WTAP_RESULT_SUCCESS) {
        WTAP_ERR_LOG("<RESULT_FAILURE> wtap_SYS_reg_i2c_read(addr:0x%02x). [ret = %d]", r_reg_data.addr, ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    ret = copy_to_user(argp, &r_reg_data, sizeof(struct wtap_sensor_reg));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_to_user(struct wtap_sensor_reg). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    WTAP_CTX_LOG("end\n");
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_diag_sensor_read                                               */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_diag_sensor_read(void __user *argp)
{
    int ret;
    unsigned char r_reg_stat;
    unsigned char r_reg_status;
    struct w_tap_read_data r_read_data;
    unsigned char r_reg_val[6];
    
    WTAP_CTX_LOG("start");
    
    ret = copy_from_user(&r_read_data, argp, sizeof(struct w_tap_read_data));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_from_user(struct w_tap_read_data). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    wtap_int_state = WTAP_INT_STATE_ACTIVE;

    wtap_REG_enable();

    msleep(WTAP_ACC_WAIT);

    wtap_INT_set_irq(WTAP_IRQ_ENABLE);

    ret = wait_event_freezable(wtap_read_wait, (wtap_int_state != WTAP_INT_STATE_ACTIVE) );

    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_STATUS  , &r_reg_status );
    wtap_SYS_reg_i2c_read( WTAP_LIS3DSH_REG_STAT    , &r_reg_stat   );

    r_reg_val[0] = (WTAP_LIS3DSH_REG_OUT_X_L | I2C_AUTO_INCREMENT);
    wtap_SYS_reg_i2c_seq_read(WTAP_LIS3DSH_REG_OUT_X_L, &r_reg_val[0], 6);

    WTAP_DBG_LOG("[addr:27h] STATUS= 0x%02x.", r_reg_status);
    WTAP_DBG_LOG("[addr:18h] STAT  = 0x%02x.", r_reg_stat);
    WTAP_DBG_LOG("[addr:28h] OUT_X_L = 0x%02x.", r_reg_val[0]);
    WTAP_DBG_LOG("[addr:29h] OUT_X_H = 0x%02x.", r_reg_val[1]);
    WTAP_DBG_LOG("[addr:2Ah] OUT_Y_L = 0x%02x.", r_reg_val[2]);
    WTAP_DBG_LOG("[addr:2Bh] OUT_Y_H = 0x%02x.", r_reg_val[3]);
    WTAP_DBG_LOG("[addr:2Ch] OUT_Z_L = 0x%02x.", r_reg_val[4]);
    WTAP_DBG_LOG("[addr:2Dh] OUT_Z_H = 0x%02x.", r_reg_val[5]);

    r_read_data.out_x = ((signed short) (r_reg_val[1] << 8) | r_reg_val[0]);
    r_read_data.out_y = ((signed short) (r_reg_val[3] << 8) | r_reg_val[2]);
    r_read_data.out_z = ((signed short) (r_reg_val[5] << 8) | r_reg_val[4]);
    

    wtap_REG_disable();

    if (ret != 0) {
        r_read_data.Result = WTAP_SYSTEM_BUSY;
    }
    else {
        r_read_data.Result = WTAP_INTRRUPT_SENSOR_READ;
    }

    wtap_int_state = WTAP_INT_STATE_STANDBY;

    wtap_SYS_wake_unlock();

    ret = copy_to_user(argp, &r_read_data, sizeof(struct w_tap_read_data));
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> copy_to_user(struct w_tap_read_data). [ret = %d]", ret);
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_FAILURE;
    }
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_data_set                                                       */
/* ------------------------------------------------------------------------- */

static int  wtap_IOCTL_data_set(void __user *argp)
{
    int ret;
    int i;
    struct wtap_setting_data r_set_data;
    
    WTAP_CTX_LOG("start");
    
    if( wtap_driver_set_data_counter != 0 ) {
        WTAP_CTX_LOG("end");
        return WTAP_RESULT_SUCCESS;
    }

    if( wtap_product_mode == 1 ) {
    
        WTAP_DBG_LOG("diag mode");

        wtap_reg_data.init_num      = WTAP_REG_TBL_INIT_DIAG;
        wtap_reg_data.setting_num   = WTAP_REG_TBL_SETTING_STATE_DIAG;
        wtap_reg_data.enable_num    = WTAP_REG_TBL_ENABLE_DIAG;
        wtap_reg_data.disable_num   = WTAP_REG_TBL_DISABLE_DIAG;
        
        for( i = 0; i < wtap_reg_data.init_num; i++ ){
            wtap_reg_data.wtap_reg_initialize_tbl[i][0] = wtap_reg_initialize_diag_tbl[i][0];
            wtap_reg_data.wtap_reg_initialize_tbl[i][1] = wtap_reg_initialize_diag_tbl[i][1];
        }

        for( i = 0; i < wtap_reg_data.enable_num; i++ ){
            wtap_reg_data.wtap_reg_enable_tbl[i][0] = wtap_reg_active_diag_tbl[i][0];
            wtap_reg_data.wtap_reg_enable_tbl[i][1] = wtap_reg_active_diag_tbl[i][1];
        }

        for( i = 0; i < wtap_reg_data.disable_num; i++ ){
            wtap_reg_data.wtap_reg_disable_tbl[i][0] = wtap_reg_standby_diag_tbl[i][0];
            wtap_reg_data.wtap_reg_disable_tbl[i][1] = wtap_reg_standby_diag_tbl[i][1];
        }
        wtap_REG_initialize();

    } else {
        WTAP_DBG_LOG("normal mode");
        ret = copy_from_user(&r_set_data, argp, sizeof(struct wtap_setting_data));
        
        if (ret != 0) {
            WTAP_ERR_LOG("<RESULT_FAILURE> copy_from_user(struct wtap_setting_data). [ret = %d]", ret);
            WTAP_CTX_LOG("end");
            return WTAP_RESULT_FAILURE;
        }
        if (r_set_data.init_num > WTAP_REG_TBL_INIT) {
            WTAP_ERR_LOG("<INVALID_VALUE> init_num = %d.", r_set_data.init_num);
            return WTAP_RESULT_FAILURE;
        }
        if (r_set_data.setting_num > WTAP_REG_TBL_SETTING_STATE) {
            WTAP_ERR_LOG("<INVALID_VALUE> setting_num = %d.", r_set_data.setting_num);
            return WTAP_RESULT_FAILURE;
        }
        if (r_set_data.enable_num > WTAP_REG_TBL_ENABLE) {
            WTAP_ERR_LOG("<INVALID_VALUE> enable_num = %d.", r_set_data.enable_num);
            return WTAP_RESULT_FAILURE;
        }
        if (r_set_data.disable_num > WTAP_REG_TBL_DISABLE) {
            WTAP_ERR_LOG("<INVALID_VALUE> disable_num = %d.", r_set_data.disable_num);
            return WTAP_RESULT_FAILURE;
        }
        if ((r_set_data.wtap_setting_state.axis >= NUM_WTAP_AXIS) ||
            (r_set_data.wtap_setting_state.axis == 0)) {
            WTAP_ERR_LOG("<INVALID_VALUE> axis = %d.", r_set_data.wtap_setting_state.axis);
            return WTAP_RESULT_FAILURE;
        }
        if ((r_set_data.wtap_setting_state.strength >= NUM_WTAP_STRENGTH) ||
            (r_set_data.wtap_setting_state.strength == 0)) {
            WTAP_ERR_LOG("<INVALID_VALUE> strength = %d.", r_set_data.wtap_setting_state.strength);
            return WTAP_RESULT_FAILURE;
        }
        if ((r_set_data.wtap_setting_state.interval >= NUM_WTAP_INTERVAL) ||
            (r_set_data.wtap_setting_state.interval == 0)) {
            WTAP_ERR_LOG("<INVALID_VALUE> interval = %d.", r_set_data.wtap_setting_state.interval);
            return WTAP_RESULT_FAILURE;
        }

        memcpy(&wtap_reg_data, &r_set_data, sizeof(struct wtap_setting_data));
        wtap_param_version = wtap_reg_data.version;
        
        wtap_REG_initialize();
        wtap_REG_setting_state();
    }

    WTAP_DBG_LOG("wtap_param_version:%d", wtap_param_version);

    wtap_driver_set_data_counter = 1;

    WTAP_CTX_LOG("end\n");
    return WTAP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* wtap_IOCTL_wakelock_release                                               */
/* ------------------------------------------------------------------------- */

static int wtap_IOCTL_wakelock_release(void)
{
    WTAP_CTX_LOG("start\n");
    wtap_SYS_wake_unlock();
    WTAP_CTX_LOG("end\n");
    return WTAP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* SYSTEM                                                                    */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* wtap_SYS_delay_us                                                         */
/* ------------------------------------------------------------------------- */

static void wtap_SYS_delay_us(unsigned long usec)
{
    struct timespec tu;
    
    if (usec >= 1000*1000) {
        tu.tv_sec  = usec / 1000000;
        tu.tv_nsec = (usec % 1000000) * 1000;
    }
    else {
        tu.tv_sec  = 0;
        tu.tv_nsec = usec * 1000;
    }
    
    hrtimer_nanosleep(&tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);
    
    return;
}


/* ------------------------------------------------------------------------- */
/* wtap_SYS_host_gpio_init                                                   */
/* ------------------------------------------------------------------------- */

static void wtap_SYS_host_gpio_init(void)
{
    int ret;
    
    struct pm_gpio wtap_gpio_param = {
        .direction      = PM_GPIO_DIR_IN,
        .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
        .output_value   = 0,
        .pull           = PM_GPIO_PULL_UP_1P5_30,
        .vin_sel        = PM_GPIO_VIN_S4,
        .out_strength   = PM_GPIO_STRENGTH_MED,
        .function       = PM_GPIO_FUNC_NORMAL,
        .inv_int_pol    = 0,
        .disable_pin    = 0,
    };
    
    ret = gpio_request(WTAP_GPIO_INT, "ACCEL_INT");
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> gpio_request(WTAP_GPIO_INT). [ret = %d]", ret);
    }
    
    ret = pm8xxx_gpio_config(WTAP_GPIO_INT, &wtap_gpio_param);
    if (ret != 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> pm8xxx_gpio_config(WTAP_GPIO_INT). [ret = %d]", ret);
    }
    
    return;
}


/* ------------------------------------------------------------------------- */
/* wtap_SYS_host_gpio_exit                                                   */
/* ------------------------------------------------------------------------- */

static void wtap_SYS_host_gpio_exit(void)
{
    gpio_free(WTAP_GPIO_INT);
    
    return;
}


/* ------------------------------------------------------------------------- */
/* wtap_SYS_reg_i2c_init                                                     */
/* ------------------------------------------------------------------------- */

static int  wtap_SYS_reg_i2c_init(struct i2c_client *client, const struct i2c_device_id * devid)
{
    wtap_i2c_data_t* i2c_p;
    
    if(wtap_i2c_p != NULL){
        return -EPERM;
    }
    
    i2c_p = (wtap_i2c_data_t*)kzalloc(sizeof(wtap_i2c_data_t),GFP_KERNEL);
    if (i2c_p == NULL) {
        return -ENOMEM;
    }
    
    wtap_i2c_p = i2c_p;
    
    i2c_set_clientdata(client,i2c_p);
    i2c_p->this_client = client;
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_SYS_reg_i2c_exit                                                     */
/* ------------------------------------------------------------------------- */

static int  wtap_SYS_reg_i2c_exit(struct i2c_client *client)
{
    wtap_i2c_data_t* i2c_p;
    
    i2c_p = i2c_get_clientdata(client);
    
    kfree(i2c_p);
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_SYS_reg_i2c_write                                                    */
/* ------------------------------------------------------------------------- */

static int  wtap_SYS_reg_i2c_write(unsigned char addr, unsigned char data)
{
    struct i2c_msg msg;
    unsigned char write_buf[2];
    int i2c_ret;
    int result = 1;
    int retry;
    
    if ((addr < WTAP_REG_MIN) || (addr > WTAP_REG_MAX)) {
        WTAP_ERR_LOG("<OTHER> invalid addr.\n");
        return WTAP_RESULT_FAILURE;
    }

    msg.addr     = wtap_i2c_p->this_client->addr;
    msg.flags    = 0;
    msg.len      = 2;
    msg.buf      = write_buf;
    write_buf[0] = addr;
    write_buf[1] = data;
    
    for (retry = 0; retry <= 10; retry++) {
        i2c_ret = i2c_transfer(wtap_i2c_p->this_client->adapter, &msg, 1);
        if (i2c_ret > 0) {
            result = 0;
            break;
        }
    }
    
    if (result == 1) {
        WTAP_ERR_LOG("<OTHER> i2c_transfer time out(i2c_ret = %d).\n", i2c_ret);
        return WTAP_RESULT_FAILURE;
    }
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_SYS_reg_i2c_read                                                     */
/* ------------------------------------------------------------------------- */

static int  wtap_SYS_reg_i2c_read(unsigned char addr, unsigned char *data)
{
    struct i2c_msg msg;
    unsigned char write_buf[2];
    unsigned char read_buf[2];
    int i2c_ret;
    int result = 1;
    int retry;
    
    if (data == NULL) {
        WTAP_ERR_LOG("<NULL_POINTER> data.\n");
        return WTAP_RESULT_FAILURE;
    }
    
    if ((addr < WTAP_REG_MIN) || (addr > WTAP_REG_MAX)) {
        WTAP_ERR_LOG("<OTHER> invalid addr.\n");
        return WTAP_RESULT_FAILURE;
    }
    
    for (retry = 0; retry <= 10; retry++) {
        msg.addr     = wtap_i2c_p->this_client->addr;
        msg.flags    = 0;
        msg.len      = 1;
        msg.buf      = write_buf;
        write_buf[0] = addr;
        
        i2c_ret = i2c_transfer(wtap_i2c_p->this_client->adapter, &msg, 1);
        
        if (i2c_ret > 0) {
            msg.addr  = wtap_i2c_p->this_client->addr;
            msg.flags = I2C_M_RD;
            msg.len   = 1;
            msg.buf   = read_buf;
            
            i2c_ret = i2c_transfer(wtap_i2c_p->this_client->adapter, &msg, 1);
            if (i2c_ret > 0) {
                *data = read_buf[0];
                result = 0;
                break;
            }
        }
    }
    
    if (result == 1) {
        WTAP_ERR_LOG("<OTHER> i2c_transfer time out(i2c_ret = %d).\n", i2c_ret);
        return WTAP_RESULT_FAILURE;
    }
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_SYS_reg_i2c_seq_read                                                 */
/* ------------------------------------------------------------------------- */

static int  wtap_SYS_reg_i2c_seq_read(unsigned char addr, unsigned char *data, int len)
{
    struct i2c_msg msg[2];
    int i2c_ret;
    int result = 1;
    int retry;
    
    WTAP_DBG_LOG("start\n");

    if (data == NULL) {
        WTAP_ERR_LOG("<NULL_POINTER> data.\n");
        return WTAP_RESULT_FAILURE;
    }
    if ((addr < WTAP_REG_MIN) || (addr > WTAP_REG_MAX)) {
        WTAP_ERR_LOG("<OTHER> invalid addr.\n");
        return WTAP_RESULT_FAILURE;
    }

    msg[0].addr     = wtap_i2c_p->this_client->addr;
    msg[0].flags    = (wtap_i2c_p->this_client->flags & I2C_M_TEN);
    msg[0].len      = 1;
    msg[0].buf      = data;
    
    msg[1].addr     = wtap_i2c_p->this_client->addr;
    msg[1].flags    = (wtap_i2c_p->this_client->flags & I2C_M_TEN) | I2C_M_RD;
    msg[1].len      = len;
    msg[1].buf      = data;
    
    for (retry = 0; retry <= 10; retry++) {
        i2c_ret = i2c_transfer(wtap_i2c_p->this_client->adapter, msg, 2);
        
        if (i2c_ret > 0) {
            result = 0;    
            break;
       }     
    }
    
    if (result == 1) {
        WTAP_ERR_LOG("<OTHER> i2c_transfer time out(i2c_ret = %d).\n", i2c_ret);
        return WTAP_RESULT_FAILURE;
    }

    return WTAP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* wtap_SYS_wake_lock_init                                                   */
/* ------------------------------------------------------------------------- */

static void wtap_SYS_wake_lock_init(void)
{
    WTAP_CTX_LOG("start\n");
    wake_lock_init(&wtap_wake_lock, WAKE_LOCK_SUSPEND, "wtap_wake_lock");
    wake_lock_init(&wtap_wake_lock_timeout, WAKE_LOCK_SUSPEND, "wtap_wake_lock_timeout");
    WTAP_CTX_LOG("end\n");
    
    return;
}

/* ------------------------------------------------------------------------- */
/* wtap_SYS_wake_lock                                                        */
/* ------------------------------------------------------------------------- */

static void wtap_SYS_wake_lock(void)
{
    int flg = 0;
    
    WTAP_CTX_LOG("start\n");
    flg = atomic_cmpxchg(&wtap_wake_lock_counter, 0, 1);
    if( flg == 0 ){
        wake_lock(&wtap_wake_lock);
    }
    WTAP_CTX_LOG("end\n");
    
    return;
}

/* ------------------------------------------------------------------------- */
/* wtap_SYS_wake_unlock                                                      */
/* ------------------------------------------------------------------------- */

static void wtap_SYS_wake_unlock(void)
{
    int flg = 0;

    WTAP_CTX_LOG("start\n");
    flg = atomic_cmpxchg(&wtap_wake_lock_counter, 1, 0);
    if( flg == 1 ){
        wake_unlock(&wtap_wake_lock);
    }
    WTAP_CTX_LOG("end\n");
    
    return;
}

/* ------------------------------------------------------------------------- */
/* wtap_SYS_wake_lock_timeout                                                */
/* ------------------------------------------------------------------------- */

static void wtap_SYS_wake_lock_timeout(long timeout)
{
    WTAP_CTX_LOG("start\n");
    wake_lock_timeout(&wtap_wake_lock_timeout, timeout);
    WTAP_CTX_LOG("end\n");
    
    return;
}

/* ------------------------------------------------------------------------- */
/* INTERRUPT                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* wtap_INT_request_irq                                                      */
/* ------------------------------------------------------------------------- */

static int  wtap_INT_request_irq(void)
{
    int ret = WTAP_RESULT_SUCCESS;
    
    ret = request_irq(gpio_to_irq(WTAP_GPIO_INT), wtap_INT_isr, WTAP_GPIO_INT_FLAGS, "wtap", NULL);
    if (ret) {
        WTAP_ERR_LOG("<RESULT_FAILURE> request_irq(PM8921(9)). [ret = %d]", ret);
        ret = WTAP_RESULT_FAILURE;
    }else{
        disable_irq_nosync(gpio_to_irq(WTAP_GPIO_INT));
    }
    
    return ret;
}


/* ------------------------------------------------------------------------- */
/* wtap_INT_free_irq                                                         */
/* ------------------------------------------------------------------------- */

static void wtap_INT_free_irq(void)
{
    free_irq(gpio_to_irq(WTAP_GPIO_INT), NULL);
    
    return;
}

/* ------------------------------------------------------------------------- */
/* wtap_INT_set_irq                                                          */
/* ------------------------------------------------------------------------- */

static int  wtap_INT_set_irq(int enable)
{
    int ret = 0;
    int flg = 0;
    
    if (enable == WTAP_IRQ_ENABLE) {
        flg = atomic_cmpxchg(&wtap_irq_flg, WTAP_IRQ_DISABLE, WTAP_IRQ_ENABLE);
        if(flg == WTAP_IRQ_DISABLE){
            ret = enable_irq_wake(gpio_to_irq(WTAP_GPIO_INT));
            enable_irq(gpio_to_irq(WTAP_GPIO_INT));
        }
    }
    else if(enable == WTAP_IRQ_DISABLE) {
        flg = atomic_cmpxchg(&wtap_irq_flg, WTAP_IRQ_ENABLE, WTAP_IRQ_DISABLE);
        if(flg == WTAP_IRQ_ENABLE){
            disable_irq_nosync(gpio_to_irq(WTAP_GPIO_INT));
            ret = disable_irq_wake(gpio_to_irq(WTAP_GPIO_INT));
        }
    }
    WTAP_DBG_LOG("[irq_wake] ret: %d\n", ret);
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_INT_isr                                                              */
/* ------------------------------------------------------------------------- */

static irqreturn_t wtap_INT_isr(int irq_num, void *data)
{
    irqreturn_t rc = IRQ_HANDLED;
    
    WTAP_CTX_LOG("start");
    
    wtap_SYS_wake_lock();

    wtap_INT_set_irq(WTAP_IRQ_DISABLE);
    
    schedule_work(&wtap_work);
    
    
    
    WTAP_CTX_LOG("end");
    
    return rc;
}


/* ------------------------------------------------------------------------- */
/* OTHER                                                                     */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* wtap_check_exist                                                          */
/* ------------------------------------------------------------------------- */

static int  wtap_check_exist(void)
{
    int ret;
    unsigned char read_data = 0;
    
    if (wtap_dev_exist == WTAP_DEV_NO_CHECK) {
        ret = wtap_SYS_reg_i2c_read(WTAP_LIS3DSH_REG_WHO_AM_I, &read_data);
        if (ret != WTAP_RESULT_SUCCESS) {
            wtap_dev_exist = WTAP_DEV_NO_EXIST;
        } else {
            wtap_dev_exist = WTAP_DEV_EXIST;
        }
    }
    
    if (wtap_dev_exist == WTAP_DEV_NO_EXIST) {
        WTAP_ERR_LOG("<WARNING> W-Tap device is not connected.");
        return WTAP_RESULT_FAILURE;
    }
    
    return WTAP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* wtap_init_state                                                           */
/* ------------------------------------------------------------------------- */

static void wtap_init_state(void)
{
    wtap_reg_data.wtap_setting_state.axis     = WTAP_DEF_SETTING_AXIS;
    wtap_reg_data.wtap_setting_state.strength = WTAP_DEF_SETTING_STRENGTH;
    wtap_reg_data.wtap_setting_state.interval = WTAP_DEF_SETTING_INTERVAL;
    
    return;
}

/* ------------------------------------------------------------------------- */
/* wtap_get_boot_mode                                                        */
/* ------------------------------------------------------------------------- */

static void wtap_get_boot_mode(void)
{
    unsigned short boot_mode;

    boot_mode = sh_boot_get_bootmode();
    
    switch(boot_mode) {
    case SH_BOOT_F_T:
    case SH_BOOT_H_C:
        wtap_product_mode = 1;
        break;
        
    default:
        wtap_product_mode = 0;
        break;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/* wtap_work_func                                                            */
/* ------------------------------------------------------------------------- */

static void wtap_work_func(struct work_struct *work)
{
    WTAP_CTX_LOG("start");

    mutex_lock(&wtap_lock);

    if (wtap_int_state != WTAP_INT_STATE_ACTIVE) {
        WTAP_DBG_LOG("status not Active");
        mutex_unlock(&wtap_lock);
        WTAP_CTX_LOG("end");
        return;
    }
    
    
    wtap_int_state = WTAP_INT_STATE_DETECT;
    
    wake_up_interruptible(&wtap_read_wait);
    
    mutex_unlock(&wtap_lock);

    WTAP_CTX_LOG("end");

}

/* ------------------------------------------------------------------------- */
/* wtap_init                                                                 */
/* ------------------------------------------------------------------------- */

static int __init wtap_init(void)
{
    int ret = WTAP_RESULT_SUCCESS;
    
    wtap_SYS_host_gpio_init();
    
    ret = misc_register(&wtap_device);
    if (ret) {
        WTAP_ERR_LOG("<RESULT_FAILURE> misc_register(&wtap_device). [ret = %d]", ret);
        goto misc_register_error;
    }
    
    ret = i2c_add_driver(&wtap_driver);
    if (ret < 0) {
        WTAP_ERR_LOG("<RESULT_FAILURE> i2c_add_driver(&wtap_driver). [ret = %d]", ret);
        goto i2c_add_driver_error;
    }

    
    memset(&wtap_reg_data, 0x00, sizeof(struct wtap_setting_data));

    wtap_init_state();

    
    /* initialize waitqueue */
    init_waitqueue_head(&wtap_read_wait);
    
    mutex_init(&wtap_lock);

    ret = wtap_INT_request_irq();
    if (ret != WTAP_RESULT_SUCCESS) {
        WTAP_ERR_LOG("<RESULT_FAILURE> wtap_INT_request_irq(). [ret = %d]", ret);
        goto request_irq_error;
    }

    INIT_WORK(&wtap_work, wtap_work_func);
    
    wtap_SYS_wake_lock_init();

    wtap_SYS_delay_us(5000);
    
    ret = wtap_check_exist();
    if (ret == WTAP_RESULT_SUCCESS) {
        wtap_SYS_reg_i2c_write( WTAP_LIS3DSH_REG_CTRL1, 0x07 );
    }

    wtap_get_boot_mode();
    
    return 0;
    
request_irq_error:
    wtap_int_state = WTAP_INT_STATE_CANCEL;
    wake_up_interruptible(&wtap_read_wait);
    
    i2c_del_driver(&wtap_driver);
    
i2c_add_driver_error:
    misc_deregister(&wtap_device);
    
misc_register_error:
    wtap_SYS_host_gpio_exit();
    
    return -1;
}
module_init(wtap_init);


/* ------------------------------------------------------------------------- */
/* wtap_exit                                                                 */
/* ------------------------------------------------------------------------- */

static void wtap_exit(void)
{
    wtap_INT_set_irq(WTAP_IRQ_DISABLE);
    wtap_INT_free_irq();
    
    wtap_int_state = WTAP_INT_STATE_CANCEL;
    wake_up_interruptible(&wtap_read_wait);
    
    i2c_del_driver(&wtap_driver);
    
    misc_deregister(&wtap_device);
    
    wtap_SYS_host_gpio_exit();
    
    wtap_SYS_wake_unlock();
    wake_lock_destroy(&wtap_wake_lock);
    wake_lock_destroy(&wtap_wake_lock_timeout);
    
    return;
}
module_exit(wtap_exit);


MODULE_DESCRIPTION("proximity sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.0");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
