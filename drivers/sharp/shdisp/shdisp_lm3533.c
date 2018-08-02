/* drivers/sharp/shdisp/shdisp_lm3533.c  (Display Driver)
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

/* ------------------------------------------------------------------------- */
/* SHARP DISPLAY DRIVER FOR KERNEL MODE                                      */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */


#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/idr.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <sharp/shdisp_kerl.h>
#include "shdisp_lm3533.h"
#include "shdisp_system.h"
#include "shdisp_type.h"
#include "shdisp_dbg.h"


#include "./data/shdisp_lm3533_data_gp6d.h"

#include "shdisp_clmr.h"
#include "shdisp_pm.h"

#include <sharp/sh_boot_manager.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define SHDISP_BDIC_BKL_MODE_OFF                (0)
#define SHDISP_BDIC_BKL_MODE_FIX                (1)
#define SHDISP_BDIC_BKL_MODE_AUTO               (2)

#define SHDISP_BDIC_BKL_DTV_OFF                 (0)
#define SHDISP_BDIC_BKL_DTV_ON                  (1)

#define SHDISP_BDIC_BKL_EMG_OFF                 (0)
#define SHDISP_BDIC_BKL_EMG_ON                  (1)

#define SHDISP_BDIC_BKL_ECO_OFF                 (0)
#define SHDISP_BDIC_BKL_ECO_ON                  (1)

#define SHDISP_BDIC_BKL_CHG_OFF                 (0)
#define SHDISP_BDIC_BKL_CHG_ON                  (1)

#define SHDISP_BDIC_TRI_LED_MODE_NORMAL         (0)
#define SHDISP_BDIC_TRI_LED_MODE_BLINK          (1)
#define SHDISP_BDIC_TRI_LED_MODE_FIREFLY        (2)
#define SHDISP_BDIC_TRI_LED_MODE_HISPEED        (3)
#define SHDISP_BDIC_TRI_LED_MODE_STANDARD       (4)
#define SHDISP_BDIC_TRI_LED_MODE_BREATH         (5)
#define SHDISP_BDIC_TRI_LED_MODE_LONG_BREATH    (6)
#define SHDISP_BDIC_TRI_LED_MODE_WAVE           (7)
#define SHDISP_BDIC_TRI_LED_MODE_FLASH          (8)
#define SHDISP_BDIC_TRI_LED_MODE_AURORA         (9)
#define SHDISP_BDIC_TRI_LED_MODE_RAINBOW       (10)

#define SHDISP_BDIC_I2C_W_START             0x01
#define SHDISP_BDIC_I2C_R_START             0x02
#define SHDISP_BDIC_I2C_R_TIMRE_ST          0x10
#define SHDISP_BDIC_I2C_ADO_UPDATE_MASK     0x60
#define SHDISP_BDIC_I2C_SEND_WAIT           1000
#define SHDISP_BDIC_I2C_SEND_RETRY          200

#define SHDISP_BDIC_I2C_SEND_RETRY_WAIT     100

#define SHDISP_FW_STACK_EXCUTE

#define SHDISP_BDIC_REGSET(x)         (shdisp_bdic_seq_regset(x, ARRAY_SIZE(x)))

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
    SHDISP_BDIC_STR,
    SHDISP_BDIC_SET,
    SHDISP_BDIC_CLR
};

typedef struct {
    unsigned char   addr;
    unsigned char   flg;
    unsigned char   data;
    unsigned long   wait;
} shdisp_bdicRegSetting_t;

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_backlight_off(void);
static void shdisp_bdic_seq_backlight_fix_on(int param);
static void shdisp_bdic_seq_backlight_auto_on(int param);
static void shdisp_bdic_LD_LCD_PWR_on(void);
static void shdisp_bdic_LD_LCD_PWR_off(void);
static void shdisp_bdic_LD_LCD_M_PWR_on(void);
static void shdisp_bdic_LD_LCD_M_PWR_off(void);
static void shdisp_bdic_LD_LCD_VDD_ON(void);
static void shdisp_bdic_LD_LCD_VDD_OFF(void);
static int  shdisp_bdic_seq_led_off(void);
static int  shdisp_bdic_seq_led_normal_on(unsigned char color);
static void shdisp_bdic_seq_led_blink_on(unsigned char color, int ontime, int interval, int count);
static void shdisp_bdic_seq_led_firefly_on(unsigned char color, int ontime, int interval, int count);
static void shdisp_bdic_seq_led_high_speed_on(unsigned char color, int interval, int count);
static void shdisp_bdic_seq_led_standard_on(unsigned char color, int interval, int count);
static void shdisp_bdic_seq_led_breath_on(unsigned char color, int interval, int count);
static void shdisp_bdic_seq_led_long_breath_on(unsigned char color, int interval, int count);
static void shdisp_bdic_seq_led_wave_on(unsigned char color, int interval, int count);
static void shdisp_bdic_seq_led_flash_on(unsigned char color, int interval, int count);
static void shdisp_bdic_seq_led_aurora_on(int interval, int count);
static void shdisp_bdic_seq_led_rainbow_on(int interval, int count);
static int shdisp_bdic_LD_PHOTO_SENSOR_get_val(unsigned short *value);
static int shdisp_bdic_LD_PHOTO_SENSOR_get_lux(unsigned short *value, unsigned long *lux);
static int shdisp_bdic_LD_i2c_write(struct shdisp_bdic_i2c_msg *msg);
static int shdisp_bdic_LD_i2c_read_mode0(struct shdisp_bdic_i2c_msg *msg);
static int shdisp_bdic_LD_i2c_read_mode1(struct shdisp_bdic_i2c_msg *msg);
static int shdisp_bdic_LD_i2c_read_mode2(struct shdisp_bdic_i2c_msg *msg);
static int shdisp_bdic_LD_i2c_read_mode3(struct shdisp_bdic_i2c_msg *msg);
static void shdisp_bdic_seq_psals_active(unsigned long dev_type);
static void shdisp_bdic_seq_psals_standby(unsigned long dev_type);
static void shdisp_bdic_seq_ps_background(unsigned long state);
static void shdisp_bdic_LD_LCD_BKL_dtv_on(void);
static void shdisp_bdic_LD_LCD_BKL_dtv_off(void);
static void shdisp_bdic_LD_LCD_BKL_emg_on(void);
static void shdisp_bdic_LD_LCD_BKL_emg_off(void);
static void shdisp_bdic_LD_LCD_BKL_get_mode(int *mode);
static void shdisp_bdic_LD_LCD_BKL_eco_on(void);
static void shdisp_bdic_LD_LCD_BKL_eco_off(void);
static void shdisp_bdic_LD_LCD_BKL_chg_on(void);
static void shdisp_bdic_LD_LCD_BKL_chg_off(void);

static void shdisp_bdic_seq_bdic_active_for_led(int);
static void shdisp_bdic_seq_bdic_standby_for_led(int);
static void shdisp_bdic_seq_clmr_off_for_led(int);
static int  shdisp_bdic_seq_regset(const shdisp_bdicRegSetting_t* regtable, int size);
static void shdisp_bdic_PD_set_hw_reset(void);
static void shdisp_bdic_PD_rel_hw_reset(void);
static void shdisp_bdic_PD_BKL_control(unsigned char request, int param);
static void shdisp_bdic_PD_GPIO_control(unsigned char port, unsigned char status);
static void shdisp_bdic_PD_TRI_LED_control(unsigned char request, int param);
static void shdisp_bdic_PD_TRI_LED_set_anime(void);
static void shdisp_bdic_PD_TRI_LED_set_chdig(void);
static void shdisp_bdic_PWM_set_value(int pwm_param);
static void shdisp_bdic_bkl_adj_clmr_write_cmd(int mode, unsigned char* value, int len );


static void shdisp_bdic_PD_REG_OPT_set_value( int table );
static void shdisp_bdic_PD_REG_OPT_temp_set( unsigned char value );
static void shdisp_bdic_PD_REG_ADO_get_opt(unsigned short *value);

static int shdisp_bdic_IO_write_reg(unsigned char reg, unsigned char val);
static int shdisp_bdic_IO_read_reg(unsigned char reg, unsigned char *val);
static int shdisp_bdic_IO_read_check_reg(unsigned char reg, unsigned char *val);
static int shdisp_bdic_IO_multi_read_reg(unsigned char reg, unsigned char *val, int size);
static int shdisp_bdic_IO_set_bit_reg(unsigned char reg, unsigned char val);
static int shdisp_bdic_IO_clr_bit_reg(unsigned char reg, unsigned char val);
static int shdisp_photo_sensor_change_range( int bkl_opt );
static int shdisp_photo_sensor_IO_write_reg(unsigned char reg, unsigned char val);
static int shdisp_photo_sensor_IO_read_reg(unsigned char reg, unsigned char *val);
#ifndef SHDISP_BDIC_USE_ALS_I2CCMD
static int shdisp_bdic_als_clmr_write_cmd(unsigned char reg, unsigned char val);
static int shdisp_bdic_als_clmr_read_cmd(unsigned char reg, unsigned char *val);
#endif  /* SHDISP_BDIC_USE_ALS_I2CCMD */
#if 0
static int shdisp_bdic_als_clmr_msk_write_cmd(unsigned char reg, unsigned char val, unsigned char msk);
#endif

static int shdisp_bdic_PD_ps_req_mask( unsigned char *remask );
static int shdisp_bdic_PD_ps_req_restart( unsigned char remask );
#if 0
static int shdisp_bdic_PD_psals_burst_write(unsigned char *wval, unsigned char cnt);
static int shdisp_bdic_PD_psals_write_threshold(void);
#endif
/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static struct shdisp_bdic_state_str s_state_str;

static int shdisp_bdic_bkl_mode;
static int shdisp_bdic_bkl_param;
static int shdisp_bdic_bkl_before_mode;

static int shdisp_bdic_dtv;

static int shdisp_bdic_emg;

static int shdisp_bdic_eco;

static int shdisp_bdic_chg;

static unsigned char shdisp_bdic_tri_led_color;
static int shdisp_bdic_tri_led_mode;
static int shdisp_bdic_tri_led_ontime;
static int shdisp_bdic_tri_led_interval;
static int shdisp_bdic_tri_led_count;

static int shdisp_bdic_i2c_errflag;

static struct shdisp_main_bkl_ctl shdisp_bkl_priority_table[NUM_SHDISP_MAIN_BKL_DEV_TYPE] = {
    { SHDISP_MAIN_BKL_MODE_OFF      , SHDISP_MAIN_BKL_PARAM_0   },
    { SHDISP_MAIN_BKL_MODE_AUTO     , SHDISP_MAIN_BKL_PARAM_0   }
};
static unsigned int shdisp_bdic_irq_fac = 0;
static unsigned int shdisp_bdic_irq_fac_exe = 0;

static int  shdisp_bdic_irq_prioriy[SHDISP_IRQ_MAX_KIND];

static unsigned char shdisp_backup_irq_reg[6];

static int shdisp_bdic_irq_det_flag = 0;

static const shdisp_bdicRegSetting_t shdisp_bdic_init[] = {
     {BDIC_REG_POLARITY,            SHDISP_BDIC_STR,    0x0D,                   0}
    ,{BDIC_REG_OUTPUT_CONFIG1,      SHDISP_BDIC_STR,    0x90,                   0}
    ,{BDIC_REG_PWM_CONFIG_A,        SHDISP_BDIC_STR,    0x01,                   0}
    ,{BDIC_REG_PWM_CONFIG_C,        SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PWM_CONFIG_D,        SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PWM_CONFIG_E,        SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_AB,    SHDISP_BDIC_STR,    0x02,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_C,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_D,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_E,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_FULL_SCALE_CURR_A,   SHDISP_BDIC_STR,    BDIC_REG_M1LED_VAL,     0}
    ,{BDIC_REG_FULL_SCALE_CURR_C,   SHDISP_BDIC_STR,    0x1A,                   0}
    ,{BDIC_REG_FULL_SCALE_CURR_D,   SHDISP_BDIC_STR,    0x1A,                   0}
    ,{BDIC_REG_FULL_SCALE_CURR_E,   SHDISP_BDIC_STR,    0x1A,                   0}
    ,{BDIC_REG_BRIGHT_A,            SHDISP_BDIC_STR,    0xFF,                   0}
};

static const shdisp_bdicRegSetting_t shdisp_bdic_bklon[] = {
     {BDIC_REG_BANK_ENABLE,         SHDISP_BDIC_SET,    0x01,                   0}
};

static const shdisp_bdicRegSetting_t shdisp_bdic_bkloff[] = {
     {BDIC_REG_BANK_ENABLE,         SHDISP_BDIC_CLR,    0x01,               15000}
};

static const shdisp_bdicRegSetting_t shdisp_bdic_ledon[] = {
     {BDIC_REG_BANK_ENABLE,         SHDISP_BDIC_SET,    0x1C,                   0}
};

static const shdisp_bdicRegSetting_t shdisp_bdic_ledoff[] = {
     {BDIC_REG_BANK_ENABLE,         SHDISP_BDIC_CLR,    0x1C,                   0}
};

static const shdisp_bdicRegSetting_t shdisp_bdic_led_anime_off[] = {
     {BDIC_REG_BRIGHT_CONFIG_C,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_D,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_E,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_PTNGEN1_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_ALS_SCALING_CTL,     SHDISP_BDIC_STR,    0x00,                   0}
};

static shdisp_bdicRegSetting_t shdisp_bdic_led_blink_on[] = {
     {BDIC_REG_BRIGHT_CONFIG_C,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_D,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_E,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_PTNGEN1_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_ALS_SCALING_CTL,     SHDISP_BDIC_STR,    0x00,                   0}
};

static shdisp_bdicRegSetting_t shdisp_bdic_led_anime_firefly[] = {
     {BDIC_REG_BRIGHT_CONFIG_C,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_D,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_BRIGHT_CONFIG_E,     SHDISP_BDIC_STR,    0x04,                   0}
    ,{BDIC_REG_PTNGEN1_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN1_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN2_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_DELAY,       SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_LOWTIME,     SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_HIGHTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_LOWBRIGHT,   SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_RISETIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_PTNGEN3_FALLTIME,    SHDISP_BDIC_STR,    0x00,                   0}
    ,{BDIC_REG_ALS_SCALING_CTL,     SHDISP_BDIC_STR,    0x00,                   0}
};

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */
//#define SHDISP_SW_BDIC_I2C_RWLOG
//#define SHDISP_SW_BDIC_POW_CTLLOG
#ifdef SHDISP_LOG_ENABLE
#define SHDISP_SW_BDIC_ADJUST_DATALOG
#endif
//#define SHDISP_SW_BDIC_IRQ_LOG

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_boot_init                                                 */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_boot_init( void )
{

    SHDISP_TRACE("in\n")
    shdisp_bdic_bkl_mode        = SHDISP_BDIC_BKL_MODE_OFF;
    shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
    shdisp_bdic_bkl_param       = 0;

    shdisp_bdic_dtv             = SHDISP_BDIC_BKL_DTV_OFF;

    shdisp_bdic_emg             = SHDISP_BDIC_BKL_EMG_OFF;

    shdisp_bdic_eco             = SHDISP_BDIC_BKL_ECO_OFF;

    shdisp_bdic_chg             = SHDISP_BDIC_BKL_CHG_OFF;

    shdisp_bdic_tri_led_color    = 0;
    shdisp_bdic_tri_led_mode     = SHDISP_BDIC_TRI_LED_MODE_NORMAL;
    shdisp_bdic_tri_led_ontime   = 0;
    shdisp_bdic_tri_led_interval = 0;
    shdisp_bdic_tri_led_count    = 0;

#ifdef SHDISP_NOT_SUPPORT_NO_OS
    shdisp_bdic_PD_set_hw_reset();
#endif  /* SHDISP_NOT_SUPPORT_NO_OS */

    SHDISP_TRACE("out\n")
    return SHDISP_BDIC_IS_EXIST;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_bdic_exist                                                */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_bdic_exist(int* bdic_is_exist)
{
    *bdic_is_exist = s_state_str.bdic_is_exist;
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_initialize                                                */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_initialize(struct shdisp_bdic_state_str* state_str)
{
    s_state_str.bdic_is_exist                   = state_str->bdic_is_exist;
    s_state_str.bdic_main_bkl_opt_mode_output   = SHDISP_BDIC_MAIN_BKL_OPT_LOW;
    s_state_str.bdic_main_bkl_opt_mode_ado      = SHDISP_BDIC_MAIN_BKL_OPT_LOW;
    s_state_str.shdisp_lux_change_level1        = SHDISP_LUX_CHANGE_LEVEL1;
    s_state_str.shdisp_lux_change_level2        = SHDISP_LUX_CHANGE_LEVEL2;

    s_state_str.clmr_is_exist                   = state_str->clmr_is_exist;

    s_state_str.bdic_clrvari_index              = state_str->bdic_clrvari_index;
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_release_hw_reset                                      */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_release_hw_reset(void)
{
    shdisp_bdic_PD_GPIO_control(SHDISP_GPIO_NUM_LCD_RESET, SHDISP_GPIO_CTL_HIGH);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_set_hw_reset                                          */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_set_hw_reset(void)
{
    shdisp_bdic_PD_GPIO_control(SHDISP_GPIO_NUM_LCD_RESET, SHDISP_GPIO_CTL_LOW);
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_power_on                                              */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_power_on(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_LD_LCD_PWR_on();
    SHDISP_DEBUG("out\n")

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_power_off                                             */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_power_off(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_LD_LCD_PWR_off();
    SHDISP_DEBUG("out\n")

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_m_power_on                                            */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_m_power_on(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_LD_LCD_M_PWR_on();
    SHDISP_DEBUG("out\n")

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_m_power_off                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_m_power_off(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_LD_LCD_M_PWR_off();
    SHDISP_DEBUG("out\n")

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_vo2_on                                                */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_vo2_on(void)
{
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_vo2_off                                               */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_vo2_off(void)
{
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_vdd_on                                                */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_vdd_on(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_LD_LCD_VDD_ON();
    SHDISP_DEBUG("out\n")

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_vdd_off                                               */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_vdd_off(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_LD_LCD_VDD_OFF();
    SHDISP_DEBUG("out\n")

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_off                                               */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_off(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_seq_backlight_off();
    SHDISP_DEBUG("out\n")
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_fix_on                                            */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_fix_on(int param)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_seq_backlight_fix_on(param);
    SHDISP_DEBUG("out\n")
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_auto_on                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_auto_on(int param)
{
    SHDISP_DEBUG("in\n")
    shdisp_bdic_seq_backlight_auto_on(param);
    SHDISP_DEBUG("out\n")
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_get_param                                         */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_get_param(unsigned long int* param)
{
    int mode = 0;

    switch (shdisp_bdic_bkl_mode) {
    case SHDISP_BDIC_BKL_MODE_FIX:
        shdisp_bdic_LD_LCD_BKL_get_mode(&mode);

        if (shdisp_bdic_dtv == SHDISP_BDIC_BKL_DTV_ON) {
            *param = shdisp_main_dtv_bkl_tbl[shdisp_bdic_bkl_param][mode];
        }
        else {
            *param = shdisp_main_bkl_tbl[shdisp_bdic_bkl_param][mode];
        }
        break;

    case SHDISP_BDIC_BKL_MODE_AUTO:
        *param = 0x100;
        break;

    default:
        *param = 0;
        break;
    }
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_set_request                                       */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_LCD_BKL_set_request(int type, struct shdisp_main_bkl_ctl *tmp)
{
    shdisp_bkl_priority_table[type].mode  = tmp->mode;
    shdisp_bkl_priority_table[type].param = tmp->param;

    shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
    shdisp_bdic_bkl_mode   = tmp->mode;
    shdisp_bdic_bkl_param  = tmp->param;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_set_request                                       */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_TRI_LED_set_request(struct shdisp_tri_led *tmp)
{
    int color = 0x00;

    color = (tmp->blue << 2) | (tmp->green << 1) | tmp->red;

    shdisp_bdic_tri_led_mode     = tmp->led_mode;
    shdisp_bdic_tri_led_color    = color;
    shdisp_bdic_tri_led_ontime   = tmp->ontime;
    shdisp_bdic_tri_led_interval = tmp->interval;
    shdisp_bdic_tri_led_count    = tmp->count;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_get_request                                       */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_LCD_BKL_get_request(int type, struct shdisp_main_bkl_ctl *tmp, struct shdisp_main_bkl_ctl *req)
{
    shdisp_bkl_priority_table[type].mode  = tmp->mode;
    shdisp_bkl_priority_table[type].param = tmp->param;


    SHDISP_DEBUG("shdisp_bdic_API_LCD_BKL_get_request  tmp->mode %d \n",tmp->mode)
    SHDISP_DEBUG("shdisp_bdic_API_LCD_BKL_get_request  tmp->param %d \n",tmp->param)

    if (shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].mode == SHDISP_MAIN_BKL_MODE_OFF) {
        req->mode  = shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].mode;
        req->param = shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].param;
    }
    else if (((shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].mode == SHDISP_MAIN_BKL_MODE_FIX) &&
                 (shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].param == SHDISP_MAIN_BKL_PARAM_1))) {
        req->mode  = shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].mode;
        req->param = shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].param;
    }
    else if (shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP_AUTO].param == SHDISP_MAIN_BKL_PARAM_1) {
        req->mode  = shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP_AUTO].mode;
        req->param = shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP_AUTO].param;
    }
    else {
        req->mode  = shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].mode;
        req->param = shdisp_bkl_priority_table[SHDISP_MAIN_BKL_DEV_TYPE_APP].param;
    }
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_dtv_on                                            */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_dtv_on(void)
{
    shdisp_bdic_LD_LCD_BKL_dtv_on();

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_dtv_off                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_dtv_off(void)
{
    shdisp_bdic_LD_LCD_BKL_dtv_off();

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_emg_on                                            */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_emg_on(void)
{
    shdisp_bdic_LD_LCD_BKL_emg_on();

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_emg_off                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_emg_off(void)
{
    shdisp_bdic_LD_LCD_BKL_emg_off();

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_eco_on                                            */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_eco_on(void)
{
    shdisp_bdic_LD_LCD_BKL_eco_on();

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_emg_off                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_eco_off(void)
{
    shdisp_bdic_LD_LCD_BKL_eco_off();

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_chg_on                                            */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_chg_on(void)
{
    shdisp_bdic_LD_LCD_BKL_chg_on();

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_LCD_BKL_chg_off                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_LCD_BKL_chg_off(void)
{
    shdisp_bdic_LD_LCD_BKL_chg_off();

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_off                                               */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_TRI_LED_off(void)
{
    int ret;
    ret = shdisp_bdic_seq_led_off();
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_normal_on                                         */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_TRI_LED_normal_on(unsigned char color)
{
    int ret;
    ret = shdisp_bdic_seq_led_normal_on(color);
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_blink_on                                          */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_blink_on(unsigned char color, int ontime, int interval, int count)
{
    if ((ontime < NUM_SHDISP_TRI_LED_ONTIME) &&
        (interval > SHDISP_TRI_LED_INTERVAL_NONE) &&
        (interval < NUM_SHDISP_TRI_LED_INTERVAL)) {
        shdisp_bdic_seq_led_blink_on(color, ontime, interval, count);
    }
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_firefly_on                                        */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_firefly_on(unsigned char color, int ontime, int interval, int count)
{
    if (((ontime == SHDISP_TRI_LED_ONTIME_TYPE3) || (ontime == SHDISP_TRI_LED_ONTIME_TYPE7)) &&
        (interval > SHDISP_TRI_LED_INTERVAL_NONE) &&
        (interval < NUM_SHDISP_TRI_LED_INTERVAL)) {
        shdisp_bdic_seq_led_firefly_on(color, ontime, interval, count);
    }
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_high_speed_on                                     */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_high_speed_on(unsigned char color, int interval, int count)
{
    shdisp_bdic_seq_led_high_speed_on(color, interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_standard_on                                       */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_standard_on(unsigned char color, int interval, int count)
{
    shdisp_bdic_seq_led_standard_on(color, interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_breath_on                                         */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_breath_on(unsigned char color, int interval, int count)
{
    shdisp_bdic_seq_led_breath_on(color, interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_long_breath_on                                    */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_long_breath_on(unsigned char color, int interval, int count)
{
    shdisp_bdic_seq_led_long_breath_on(color, interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_wave_on                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_wave_on(unsigned char color, int interval, int count)
{
    shdisp_bdic_seq_led_wave_on(color, interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_flash_on                                          */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_flash_on(unsigned char color, int interval, int count)
{
    shdisp_bdic_seq_led_flash_on(color, interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_aurora_on                                         */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_aurora_on(int interval, int count)
{
    shdisp_bdic_seq_led_aurora_on(interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_rainbow_on                                        */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_TRI_LED_rainbow_on(int interval, int count)
{
    shdisp_bdic_seq_led_rainbow_on(interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_TRI_LED_get_clrvari_index                                 */
/* ------------------------------------------------------------------------- */

int  shdisp_bdic_API_TRI_LED_get_clrvari_index( int clrvari )
{
    int i=0;

    for( i=0; i<SHDISP_COL_VARI_KIND ; i++) {
        if( (int)shdisp_clrvari_index[i] == clrvari )
            break;
    }
    if( i >= SHDISP_COL_VARI_KIND )
        i = 0;

    return i;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_PHOTO_SENSOR_get_val                                      */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_PHOTO_SENSOR_get_val(unsigned short *value)
{
    int ret;

    ret = shdisp_bdic_LD_PHOTO_SENSOR_get_val(value);

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_PHOTO_SENSOR_get_lux                                      */
/* ------------------------------------------------------------------------- */

int  shdisp_bdic_API_PHOTO_SENSOR_get_lux(unsigned short *value, unsigned long *lux)
{
    int ret;

    ret = shdisp_bdic_LD_PHOTO_SENSOR_get_lux(value, lux);

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_PHOTO_SENSOR_lux_change_ind                               */
/* ------------------------------------------------------------------------- */

int  shdisp_bdic_API_PHOTO_SENSOR_lux_change_ind(int *mode)
{
    if(s_state_str.bdic_main_bkl_opt_mode_ado == SHDISP_BDIC_MAIN_BKL_OPT_LOW ){
        *mode = SHDISP_LUX_MODE_LOW;
    }
    else {
        *mode = SHDISP_LUX_MODE_HIGH;
    }

    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_PHOTO_SENSOR_lux_judge                                    */
/* ------------------------------------------------------------------------- */

int  shdisp_bdic_API_PHOTO_SENSOR_lux_judge(void)
{
    int           ret;
    unsigned char val = 0;
    unsigned short ADO_dat;

    val &= 0x1F;
    shdisp_bdic_PD_REG_ADO_get_opt(&ADO_dat);

    if( s_state_str.bdic_main_bkl_opt_mode_output == SHDISP_BDIC_MAIN_BKL_OPT_LOW ){
        if(val >= s_state_str.shdisp_lux_change_level1 ){
#ifdef SHDISP_SW_BDIC_IRQ_LOG
            printk("[SHDISP DBG] lux_judge: ADO_INDEX(=%d) >= level1(=%d) (ADO=0x%04X) -> JUDGE_OUT\n", (int)val, (int)s_state_str.shdisp_lux_change_level1, ADO_dat);
#endif
            shdisp_photo_sensor_change_range( SHDISP_BDIC_MAIN_BKL_OPT_HIGH );


            shdisp_bdic_PD_REG_OPT_temp_set(0x73);



            shdisp_bdic_PD_REG_OPT_set_value(SHDISP_BDIC_MAIN_BKL_OPT_HIGH);


            ret = SHDISP_BDIC_LUX_JUDGE_OUT;
        }
        else{
#ifdef SHDISP_SW_BDIC_IRQ_LOG
            printk("[SHDISP DBG] lux_judge: ADO_INDEX(=%d) < level1(=%d) (ADO=0x%04X) -> JUDGE_IN_CONTI\n", (int)val, (int)s_state_str.shdisp_lux_change_level1, ADO_dat);
#endif

            ret = SHDISP_BDIC_LUX_JUDGE_IN_CONTI;
        }
    }
    else{
        if(val < s_state_str.shdisp_lux_change_level2 ){
#ifdef SHDISP_SW_BDIC_IRQ_LOG
            printk("[SHDISP DBG] lux_judge: ADO_INDEX(=%d) < level2(=%d) (ADO=0x%04X) -> JUDGE_IN\n", (int)val, (int)s_state_str.shdisp_lux_change_level2, ADO_dat);
#endif
            shdisp_photo_sensor_change_range( SHDISP_BDIC_MAIN_BKL_OPT_LOW );


            shdisp_bdic_PD_REG_OPT_temp_set(0x66);



            shdisp_bdic_PD_REG_OPT_set_value(SHDISP_BDIC_MAIN_BKL_OPT_LOW);


            ret = SHDISP_BDIC_LUX_JUDGE_IN;
        }
        else{
#ifdef SHDISP_SW_BDIC_IRQ_LOG
            printk("[SHDISP DBG] lux_judge: ADO_INDEX(=%d) >= level2(=%d) (ADO=0x%04X) -> JUDGE_OUT_CONTI\n", (int)val, (int)s_state_str.shdisp_lux_change_level2, ADO_dat);
#endif

            ret = SHDISP_BDIC_LUX_JUDGE_OUT_CONTI;
        }
    }
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_change_level_lut                                          */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_change_level_lut(void)
{

    int level_lut = SHDISP_MAIN_DISP_CABC_LUT0;

    unsigned char val = 0;

    val &= 0x1F;

    if( s_state_str.bdic_main_bkl_opt_mode_output == SHDISP_BDIC_MAIN_BKL_OPT_LOW ){
        level_lut = SHDISP_MAIN_DISP_CABC_LUT0;
    }
    else if(val >= CABC_LUX_LEVEL_LUT4_5)
    {
        level_lut = SHDISP_MAIN_DISP_CABC_LUT5;
    }
    else if(val >= CABC_LUX_LEVEL_LUT3_4)
    {
        level_lut = SHDISP_MAIN_DISP_CABC_LUT4;
    }
    else if(val >= CABC_LUX_LEVEL_LUT2_3)
    {
        level_lut = SHDISP_MAIN_DISP_CABC_LUT3;
    }
    else if(val >= CABC_LUX_LEVEL_LUT1_2)
    {
        level_lut = SHDISP_MAIN_DISP_CABC_LUT2;
    }
    else if(val >= CABC_LUX_LEVEL_LUT0_1)
    {
        level_lut = SHDISP_MAIN_DISP_CABC_LUT1;
    }
    else
    {
        level_lut = SHDISP_MAIN_DISP_CABC_LUT0;
    }

    return level_lut;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_i2c_transfer                                              */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_i2c_transfer(struct shdisp_bdic_i2c_msg *msg)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char value = 0x00;
    int i2c_rtimer_restart_flg = 0;

    if (msg == NULL) {
        SHDISP_ERR("<NULL_POINTER> msg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if (msg->mode >= NUM_SHDISP_BDIC_I2C_M) {
        SHDISP_ERR("<INVALID_VALUE> msg->mode(%d).\n", msg->mode);
        return SHDISP_RESULT_FAILURE;
    }



    if (value & SHDISP_BDIC_I2C_R_TIMRE_ST) {
#ifdef SHDISP_SW_BDIC_I2C_RWLOG
        printk("[SHDISP] bdic i2c transfer : stop i2c rtimer.\n");
#endif
        i2c_rtimer_restart_flg = 1;
        shdisp_SYS_delay_us(1000);
    }

    switch (msg->mode) {
    case SHDISP_BDIC_I2C_M_W:
        ret = shdisp_bdic_LD_i2c_write(msg);
        break;
    case SHDISP_BDIC_I2C_M_R:
        ret = shdisp_bdic_LD_i2c_read_mode0(msg);
        break;
    case SHDISP_BDIC_I2C_M_R_MODE1:
        ret = shdisp_bdic_LD_i2c_read_mode1(msg);
        break;
    case SHDISP_BDIC_I2C_M_R_MODE2:
        ret = shdisp_bdic_LD_i2c_read_mode2(msg);
        break;
    case SHDISP_BDIC_I2C_M_R_MODE3:
        ret = shdisp_bdic_LD_i2c_read_mode3(msg);
        break;
    default:
        SHDISP_ERR("<INVALID_VALUE> msg->mode(%d).\n", msg->mode);
        ret = SHDISP_RESULT_FAILURE;
        break;
    }

    if (i2c_rtimer_restart_flg == 1) {
#ifdef SHDISP_SW_BDIC_I2C_RWLOG
        printk("[SHDISP] bdic i2c transfer : restart i2c rtimer.\n");
#endif
    }
    else {
    }

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_DIAG_write_reg                                            */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_DIAG_write_reg(unsigned char reg, unsigned char val)
{
    int ret = 0;

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    ret = shdisp_bdic_IO_write_reg(reg, val);

    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    } else {
        shdisp_SYS_bdic_i2c_cmd_reset();
    }

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_DIAG_read_reg                                             */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_DIAG_read_reg(unsigned char reg, unsigned char *val)
{
    int ret = 0;

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    ret = shdisp_bdic_IO_read_reg(reg, val);

    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    } else {
        shdisp_SYS_bdic_i2c_cmd_reset();
    }

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_DIAG_multi_read_reg                                       */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_DIAG_multi_read_reg(unsigned char reg, unsigned char *val, int size)
{
    int ret = 0;

    ret = shdisp_bdic_IO_multi_read_reg(reg, val, size);

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_RECOVERY_check_restoration                                */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_RECOVERY_check_restoration(void)
{
    unsigned char dummy = 0;


    if (dummy & 0x04) {
        return SHDISP_RESULT_SUCCESS;
    }
    else {
        return SHDISP_RESULT_FAILURE;
    }
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_RECOVERY_resume_bdic_status                               */
/* ------------------------------------------------------------------------- */

int  shdisp_bdic_API_RECOVERY_resume_bdic_status(void)
{
    int ret;

    ret = shdisp_pm_bdic_resume();
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }


    if (shdisp_bdic_tri_led_color != 0) {
        switch (shdisp_bdic_tri_led_mode) {
        case SHDISP_BDIC_TRI_LED_MODE_NORMAL:
            shdisp_bdic_API_TRI_LED_normal_on(shdisp_bdic_tri_led_color);
            break;
        case SHDISP_BDIC_TRI_LED_MODE_BLINK:
            shdisp_bdic_API_TRI_LED_blink_on(shdisp_bdic_tri_led_color, shdisp_bdic_tri_led_ontime, shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count);
            break;
        case SHDISP_BDIC_TRI_LED_MODE_FIREFLY:
            shdisp_bdic_API_TRI_LED_firefly_on(shdisp_bdic_tri_led_color, shdisp_bdic_tri_led_ontime, shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count);
            break;
        case SHDISP_BDIC_TRI_LED_MODE_HISPEED:
            shdisp_bdic_API_TRI_LED_high_speed_on( shdisp_bdic_tri_led_color, shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count );
            break;
        case SHDISP_BDIC_TRI_LED_MODE_STANDARD:
            shdisp_bdic_API_TRI_LED_standard_on( shdisp_bdic_tri_led_color, shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count );
            break;
        case SHDISP_BDIC_TRI_LED_MODE_BREATH:
            shdisp_bdic_API_TRI_LED_breath_on( shdisp_bdic_tri_led_color, shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count );
            break;
        case SHDISP_BDIC_TRI_LED_MODE_LONG_BREATH:
            shdisp_bdic_API_TRI_LED_long_breath_on( shdisp_bdic_tri_led_color, shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count );
            break;
        case SHDISP_BDIC_TRI_LED_MODE_WAVE:
            shdisp_bdic_API_TRI_LED_wave_on( shdisp_bdic_tri_led_color, shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count );
            break;
        case SHDISP_BDIC_TRI_LED_MODE_FLASH:
            shdisp_bdic_API_TRI_LED_flash_on( shdisp_bdic_tri_led_color, shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count );
            break;
        case SHDISP_BDIC_TRI_LED_MODE_AURORA:
            shdisp_bdic_API_TRI_LED_aurora_on(shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count);
            break;
        case SHDISP_BDIC_TRI_LED_MODE_RAINBOW:
            shdisp_bdic_API_TRI_LED_rainbow_on(shdisp_bdic_tri_led_interval, shdisp_bdic_tri_led_count);
            break;
        default:
            break;
        }
    }

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        switch (shdisp_bdic_bkl_mode) {
        case SHDISP_BDIC_BKL_MODE_FIX:
            shdisp_bdic_API_LCD_BKL_fix_on(shdisp_bdic_bkl_param);
            break;
        case SHDISP_BDIC_BKL_MODE_AUTO:
            shdisp_bdic_API_LCD_BKL_auto_on(shdisp_bdic_bkl_param);
            break;
        default:
            break;
        }
    }

    return SHDISP_RESULT_SUCCESS;
}


#if defined (CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_DBG_INFO_output                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_DBG_INFO_output(void)
{
    int idx;
    unsigned char   *p;
    unsigned char   *pbuf;

    SHDISP_DEBUG(" BDIC INFO ->>");
    SHDISP_DEBUG(" s_state_str.bdic_is_exist                 = %d.\n", s_state_str.bdic_is_exist);
    SHDISP_DEBUG(" s_state_str.bdic_main_bkl_opt_mode_output = %d. (0:low/1:high)\n", s_state_str.bdic_main_bkl_opt_mode_output);
    SHDISP_DEBUG(" s_state_str.bdic_main_bkl_opt_mode_ado    = %d. (0:low/1:high)\n", s_state_str.bdic_main_bkl_opt_mode_ado);
    SHDISP_DEBUG(" s_state_str.shdisp_lux_change_level1      = %d.\n", s_state_str.shdisp_lux_change_level1);
    SHDISP_DEBUG(" s_state_str.shdisp_lux_change_level2      = %d.\n", s_state_str.shdisp_lux_change_level2);
    SHDISP_DEBUG(" s_state_str.bdic_clrvari_index            = %d.\n", s_state_str.bdic_clrvari_index);
    SHDISP_DEBUG(" shdisp_bdic_bkl_mode                      = %d.\n", shdisp_bdic_bkl_mode);
    SHDISP_DEBUG(" shdisp_bdic_bkl_param                     = %d.\n", shdisp_bdic_bkl_param);
    SHDISP_DEBUG(" shdisp_bdic_dtv                           = %d.\n", shdisp_bdic_dtv);
    SHDISP_DEBUG(" shdisp_bdic_emg                           = %d.\n", shdisp_bdic_emg);
    SHDISP_DEBUG(" shdisp_bdic_eco                           = %d.\n", shdisp_bdic_eco);
    SHDISP_DEBUG(" shdisp_bdic_chg                           = %d.\n", shdisp_bdic_chg);
    SHDISP_DEBUG(" shdisp_bdic_tri_led_color                 = %d.\n", (int)shdisp_bdic_tri_led_color);
    SHDISP_DEBUG(" shdisp_bdic_tri_led_mode                  = %d.\n", shdisp_bdic_tri_led_mode);
    SHDISP_DEBUG(" shdisp_bdic_tri_led_ontime                = %d.\n", shdisp_bdic_tri_led_ontime);
    SHDISP_DEBUG(" shdisp_bdic_tri_led_interval              = %d.\n", shdisp_bdic_tri_led_interval);
    SHDISP_DEBUG(" shdisp_bdic_tri_led_count                 = %d.\n", shdisp_bdic_tri_led_count);

    for ( idx = 0; idx < NUM_SHDISP_MAIN_BKL_DEV_TYPE; idx++ ) {
        SHDISP_DEBUG("shdisp_bkl_priority_table[%d]          = (mode:%d, param:%d).\n", idx, shdisp_bkl_priority_table[idx].mode, shdisp_bkl_priority_table[idx].param);
    }

    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_DIAG, SHDISP_DEV_STATE_ON);
    pbuf = kzalloc((((BDIC_REG_MAXIMUM_ADDRESS+7)/8)*8), GFP_KERNEL);
    if (!pbuf) {
        SHDISP_ERR("kzalloc failed. size=%d\n", (((BDIC_REG_MAXIMUM_ADDRESS+7)/8)*8));
        return;
    }
    p = pbuf;
    for (idx = BDIC_REG_MINIMUM_ADDRESS; idx <= BDIC_REG_MAXIMUM_ADDRESS; idx++ ) {
        *p = 0x00;
        shdisp_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_DIAG, SHDISP_DEV_STATE_OFF);

    p = pbuf;
    for (idx = BDIC_REG_MINIMUM_ADDRESS; idx <= BDIC_REG_MAXIMUM_ADDRESS; idx += 8) {
        SHDISP_DEBUG("BDIC_REG_DUMP 0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x\n", idx, *p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5), *(p+6), *(p+7));
        p += 8;
    }
    SHDISP_DEBUG("BDIC INFO <<-\n");

    kfree(pbuf);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_psals_API_DBG_INFO_output                                          */
/* ------------------------------------------------------------------------- */

void shdisp_psals_API_DBG_INFO_output(void)
{
    int idx;
    unsigned char   *p;
    unsigned char   *pbuf;

    SHDISP_DEBUG("PSALS SENSOR INFO ->>");
    (void)shdisp_pm_psals_power_manager(SHDISP_DEV_TYPE_DIAG, SHDISP_DEV_STATE_ON);
    pbuf = kzalloc((((SENSOR_REG_D2_MSB+7)/8)*8), GFP_KERNEL);
    if (!pbuf) {
        SHDISP_ERR("kzalloc failed. size=%d\n", (((SENSOR_REG_D2_MSB+7)/8)*8));
        return;
    }

    shdisp_SYS_delay_us(1000*1000);

    p = pbuf;
    for (idx = SENSOR_REG_COMMAND1; idx <= SENSOR_REG_D2_MSB; idx++ ) {
        *p = 0x00;
        shdisp_photo_sensor_IO_read_reg(idx, p);
        p++;
    }
    p = pbuf;
    SHDISP_DEBUG("SENSOR_REG_DUMP 0x00: %02x %02x %02x %02x %02x %02x %02x %02x\n", *p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5), *(p+6), *(p+7));
    p += 8;
    SHDISP_DEBUG("SENSOR_REG_DUMP 0x08: %02x %02x %02x %02x %02x %02x %02x %02x\n", *p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5), *(p+6), *(p+7));
    p += 8;
    SHDISP_DEBUG("SENSOR_REG_DUMP 0x10: %02x %02x                              \n", *p, *(p+1));
    kfree(pbuf);

    (void)shdisp_pm_psals_power_manager(SHDISP_DEV_TYPE_DIAG, SHDISP_DEV_STATE_OFF);

    SHDISP_DEBUG("PSALS SENSOR INFO <<-\n");
    return;
}

#endif /* CONFIG_ANDROID_ENGINEERING */


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_set_reg                                               */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_set_reg( int irq_type, int onoff )
{
    unsigned char   val1, val2,val3,val4,val5;

    switch ( irq_type ) {
    case SHDISP_IRQ_TYPE_ALS:
        break;
    case SHDISP_IRQ_TYPE_PS:
        break;
    case SHDISP_IRQ_TYPE_DET:
        break;
    default:
        return;
    }

    if( onoff == SHDISP_IRQ_MASK ){
        val1 = 0;
        val2 = 0;
        val3 = 0;
        val4 = 0;
        val5 = 0;
        if((val1 == 0) && (val2 == 0) && (val3 == 0) && (val4 == 0) && (val5 == 0)){
#ifdef SHDISP_SW_BDIC_IRQ_LOG
            printk("[SHDISP]I2C_ERR Interrupt Mask\n");
#endif /* SHDISP_SW_BDIC_IRQ_LOG */
        }
    }
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_check_type                                            */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_IRQ_check_type( int irq_type )
{
    if(( irq_type < SHDISP_IRQ_TYPE_ALS ) || ( irq_type >= NUM_SHDISP_IRQ_TYPE ))
        return SHDISP_RESULT_FAILURE;

    if( irq_type == SHDISP_IRQ_TYPE_DET ){
        if( !( SHDISP_INT_ENABLE_GFAC & SHDISP_BDIC_INT_GFAC_DET )){
           return SHDISP_RESULT_FAILURE;
        }
    }

    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_save_fac                                              */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_save_fac(void)
{
    unsigned char value1=0, value2=0, value3=0;

#ifdef SHDISP_SW_BDIC_IRQ_LOG
    printk("[SHDISP] shdisp_bdic_AIP_save_irq_fac:GFAC3=%02x GFAC2=%02x GFAC1=%02x\n", value3, value2, value1);
#endif /* SHDISP_SW_BDIC_IRQ_LOG */
    shdisp_bdic_irq_fac = (unsigned int)value1 | ((unsigned int)value2 << 8 ) | ((unsigned int)value3 << 16);

    if(shdisp_bdic_irq_det_flag == 1) {
        shdisp_bdic_irq_det_flag = 2;
    }
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_check_DET                                              */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_IRQ_check_DET(void)
{
    if( shdisp_bdic_irq_fac & SHDISP_BDIC_INT_GFAC_DET ){
        return SHDISP_BDIC_IRQ_TYPE_DET;
    }
    else{
        return SHDISP_BDIC_IRQ_TYPE_NONE;
    }
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_check_fac                                              */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_IRQ_check_fac(void)
{
    int i;
    if( shdisp_bdic_irq_fac == 0 ){
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_bdic_irq_fac_exe = (shdisp_bdic_irq_fac & SHDISP_INT_ENABLE_GFAC);
    if( shdisp_bdic_irq_fac_exe == 0 ){
        return SHDISP_RESULT_FAILURE;
    }

    for(i=0; i<SHDISP_IRQ_MAX_KIND ; i++){
        shdisp_bdic_irq_prioriy[i] = SHDISP_BDIC_IRQ_TYPE_NONE;
    }

    i=0;
    if(( shdisp_bdic_irq_fac_exe & (SHDISP_BDIC_INT_GFAC_PS | SHDISP_BDIC_INT_GFAC_I2C_ERR | SHDISP_BDIC_INT_GFAC_PS2 )) == SHDISP_BDIC_INT_GFAC_PS ){
        shdisp_bdic_irq_prioriy[i] = SHDISP_BDIC_IRQ_TYPE_PS;
        i++;
    }

    if(( shdisp_bdic_irq_fac_exe & SHDISP_BDIC_INT_GFAC_DET ) != 0 ){
        shdisp_bdic_irq_prioriy[i] = SHDISP_BDIC_IRQ_TYPE_DET;
        i++;
    }
    else if(( shdisp_bdic_irq_fac_exe & SHDISP_BDIC_INT_GFAC_I2C_ERR ) != 0
         || ( shdisp_bdic_irq_fac_exe & SHDISP_BDIC_INT_GFAC_PS2 ) != 0){
        shdisp_bdic_irq_prioriy[i] = SHDISP_BDIC_IRQ_TYPE_I2C_ERR;
        i++;
    }
    else if((shdisp_bdic_irq_fac_exe & (SHDISP_BDIC_INT_GFAC_ALS | SHDISP_BDIC_INT_GFAC_OPTSEL)) != 0 ){
        shdisp_bdic_irq_prioriy[i] = SHDISP_BDIC_IRQ_TYPE_ALS;
        i++;
    }

    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_get_fac                                                   */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_IRQ_get_fac( int iQueFac )
{
    if( iQueFac >= SHDISP_IRQ_MAX_KIND )
        return SHDISP_BDIC_IRQ_TYPE_NONE;

    return shdisp_bdic_irq_prioriy[iQueFac];
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_Clear                                                 */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_Clear(void)
{

}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_i2c_error_Clear                                       */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_i2c_error_Clear(void)
{
    unsigned char out2=0,out3=0;

    if( SHDISP_INT_ENABLE_GFAC & SHDISP_BDIC_INT_GFAC_I2C_ERR ){
        out3 = 0x08;
    }

    if( SHDISP_INT_ENABLE_GFAC & SHDISP_BDIC_INT_GFAC_PS2 ){
        out2 = 0x02;
    }
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_i2c_error_Clear                                       */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_det_fac_Clear(void)
{
    unsigned char out3=0;

    if( SHDISP_INT_ENABLE_GFAC & SHDISP_BDIC_INT_GFAC_DET ){
        out3 = 0x04;
    }
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_backup_irq_reg                                        */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_backup_irq_reg(void)
{
    memset(&shdisp_backup_irq_reg, 0, sizeof(shdisp_backup_irq_reg));
#ifdef SHDISP_SW_BDIC_IRQ_LOG
    printk("[SHDISP DBG] backup_irq_reg: R1=0x%02X,R2=0x%02X,R3=0x%02X,F1=0x%02X,F2=0x%02X,F3=0x%02X\n",
        shdisp_backup_irq_reg[0], shdisp_backup_irq_reg[1], shdisp_backup_irq_reg[2],
        shdisp_backup_irq_reg[3], shdisp_backup_irq_reg[4], shdisp_backup_irq_reg[5]);
#endif
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_return_irq_reg                                        */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_return_irq_reg(void)
{
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_irq_reg_i2c_mask                                      */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_irq_reg_i2c_mask(void)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_det_irq_ctrl                                          */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_IRQ_det_irq_ctrl(int ctrl)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_det_irq_mask                                          */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_det_irq_mask(void)
{
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_set_det_flag                                          */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_IRQ_set_det_flag(void)
{
    shdisp_bdic_irq_det_flag = 1;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_clr_det_flag                                          */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_IRQ_clr_det_flag(void)
{
    if(shdisp_bdic_irq_det_flag != 1) {
        shdisp_bdic_irq_det_flag = 0;
    }
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_get_det_flag                                          */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_API_IRQ_get_det_flag(void)
{
    return shdisp_bdic_irq_det_flag;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_dbg_Clear_All                                         */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_dbg_Clear_All(void)
{
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_dbg_set_fac                                           */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_dbg_set_fac(unsigned int nGFAC)
{
    shdisp_bdic_irq_fac = nGFAC;

}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_IRQ_dbg_photo_param                                       */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_API_IRQ_dbg_photo_param( int level1, int level2)
{
    s_state_str.shdisp_lux_change_level1 = (unsigned char)( level1 & 0x00FF );
    s_state_str.shdisp_lux_change_level2 = (unsigned char)( level2 & 0x00FF );
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_PHOTO_lux_timer                                           */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_PHOTO_lux_timer( int onoff )
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char   remask;

    ret = shdisp_pm_is_als_active();
    if (ret == SHDISP_RESULT_FAILURE) {
        return SHDISP_RESULT_FAILURE;
    }

    if( onoff == SHDISP_BDIC_PHOTO_LUX_TIMER_OFF ){
        shdisp_bdic_PD_ps_req_mask(&remask);
    }
    else {
    }

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_SENSOR_start                                              */
/* ------------------------------------------------------------------------- */

int shdisp_bdic_API_SENSOR_start( int new_opt_mode )
{
    int ret = SHDISP_RESULT_SUCCESS;

    s_state_str.bdic_main_bkl_opt_mode_ado = new_opt_mode;
    shdisp_bdic_PD_ps_req_restart(1);
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_api_prox_sensor_pow_ctl                                       */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_api_prox_sensor_pow_ctl(int power_mode)
{
    int clmr_power, ps_active;

    clmr_power = shdisp_pm_is_clmr_on();
    ps_active = shdisp_pm_is_ps_active();

    switch (power_mode) {
    case SHDISP_PROX_SENSOR_POWER_OFF:
        shdisp_bdic_seq_psals_standby(SHDISP_DEV_TYPE_PS);
        break;
    case SHDISP_PROX_SENSOR_POWER_ON:
        shdisp_bdic_seq_psals_active(SHDISP_DEV_TYPE_PS);
        break;
    case SHDISP_PROX_SENSOR_BGMODE_ON:
        if (clmr_power == SHDISP_DEV_STATE_OFF && ps_active == SHDISP_RESULT_FAILURE) {
            return SHDISP_RESULT_FAILURE;
        }
        if (clmr_power == SHDISP_DEV_STATE_ON || ps_active == SHDISP_RESULT_FAILURE) {
            shdisp_bdic_seq_ps_background(SHDISP_DEV_STATE_OFF);
        }
        break;
    case SHDISP_PROX_SENSOR_BGMODE_OFF:
        if (clmr_power == SHDISP_DEV_STATE_OFF && ps_active == SHDISP_RESULT_FAILURE) {
            return SHDISP_RESULT_FAILURE;
        }
        if (clmr_power == SHDISP_DEV_STATE_OFF || ps_active == SHDISP_RESULT_FAILURE) {
            shdisp_bdic_seq_ps_background(SHDISP_DEV_STATE_ON);
        }
        break;
    default:
        break;
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_api_als_sensor_pow_ctl                                        */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_api_als_sensor_pow_ctl(int dev_type, int power_mode)
{
    unsigned long type = 0;
    int param_chk = 0;

    switch (dev_type) {
    case SHDISP_PHOTO_SENSOR_TYPE_APP:
        type = SHDISP_DEV_TYPE_ALS_APP;
        break;
    case SHDISP_PHOTO_SENSOR_TYPE_LUX:
        type = SHDISP_DEV_TYPE_ALS_LUX;
        break;
    default:
        param_chk = 1;
        SHDISP_ERR("<INVALID_VALUE> ctl->type(%d).\n", dev_type);
        break;
    }

    switch (power_mode) {
    case SHDISP_PHOTO_SENSOR_DISABLE:
        shdisp_bdic_seq_psals_standby(type);
        break;
    case SHDISP_PHOTO_SENSOR_ENABLE:
        shdisp_bdic_seq_psals_active(type);
        break;
    default:
        param_chk = 1;
        SHDISP_ERR("<INVALID_VALUE> ctl->power(%d).\n", power_mode);
        break;
    }

    if (param_chk == 1) {
        return SHDISP_RESULT_FAILURE;
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_api_set_default_sensor_param                                  */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_api_set_default_sensor_param(struct shdisp_photo_sensor_adj *tmp_adj)
{
    unsigned long tmp_param1;
    unsigned long tmp_param2;

    tmp_param1 = ((unsigned long)BDIC_REG_ALS_ADJ0_H_DEFAULT_A << 8);
    tmp_param2 = (unsigned long)BDIC_REG_ALS_ADJ0_L_DEFAULT_A;
    tmp_adj->als_adjust[0].als_adj0     = tmp_param1 | tmp_param2;
    tmp_param1 = ((unsigned long)BDIC_REG_ALS_ADJ1_H_DEFAULT_A << 8);
    tmp_param2 = (unsigned long)BDIC_REG_ALS_ADJ1_L_DEFAULT_A;
    tmp_adj->als_adjust[0].als_adj1     = tmp_param1 | tmp_param2;
    tmp_adj->als_adjust[0].als_shift    = BDIC_REG_ALS_SHIFT_DEFAULT_A;
    tmp_adj->als_adjust[0].clear_offset = BDIC_REG_CLEAR_OFFSET_DEFAULT_A;
    tmp_adj->als_adjust[0].ir_offset    = BDIC_REG_IR_OFFSET_DEFAULT_A;

    tmp_param1 = ((unsigned long)BDIC_REG_ALS_ADJ0_H_DEFAULT_B << 8);
    tmp_param2 = (unsigned long)BDIC_REG_ALS_ADJ0_L_DEFAULT_B;
    tmp_adj->als_adjust[1].als_adj0     = tmp_param1 | tmp_param2;
    tmp_param1 = ((unsigned long)BDIC_REG_ALS_ADJ1_H_DEFAULT_B << 8);
    tmp_param2 = (unsigned long)BDIC_REG_ALS_ADJ1_L_DEFAULT_B;
    tmp_adj->als_adjust[1].als_adj1     = tmp_param1 | tmp_param2;
    tmp_adj->als_adjust[1].als_shift    = BDIC_REG_ALS_SHIFT_DEFAULT_B;
    tmp_adj->als_adjust[1].clear_offset = BDIC_REG_CLEAR_OFFSET_DEFAULT_B;
    tmp_adj->als_adjust[1].ir_offset    = BDIC_REG_IR_OFFSET_DEFAULT_B;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_api_set_prox_sensor_param                                     */
/* ------------------------------------------------------------------------- */

void shdisp_bdic_api_set_prox_sensor_param( struct shdisp_prox_params *prox_params)
{
    memcpy( &s_state_str.prox_params, prox_params, sizeof( struct shdisp_prox_params));
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_api_get_LC_MLED01                                             */
/* ------------------------------------------------------------------------- */
unsigned short shdisp_bdic_api_get_LC_MLED01(void)
{
    return ((BDIC_REG_M1LED_VAL<<8) | BDIC_REG_M2LED_VAL);
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_get_lux_data                                              */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_API_get_lux_data(void)
{
    unsigned char para[1] = { 0x05 };
    unsigned char rtnbuf[4] = {0};

    SHDISP_DEBUG("in\n");

    shdisp_FWCMD_buf_init(0);
    shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_LUXPARAM_READ, 1, para);
    shdisp_FWCMD_buf_finish();
    shdisp_FWCMD_doKick(1, 4, rtnbuf);

    SHDISP_DEBUG("rtnbuf[0]=0x%02x\n", rtnbuf[0]);
    SHDISP_DEBUG("rtnbuf[1]=0x%02x\n", rtnbuf[1]);
    SHDISP_DEBUG("rtnbuf[2]=0x%02x\n", rtnbuf[2]);
    SHDISP_DEBUG("rtnbuf[3]=0x%02x\n", rtnbuf[3]);

    SHDISP_DEBUG("out\n");
    return (rtnbuf[3] << 24) | (rtnbuf[2] << 16) | (rtnbuf[1] << 8) | rtnbuf[0];
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_set_bkl_mode                                              */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_set_bkl_mode(unsigned char bkl_mode, unsigned char data, unsigned char msk)
{
    unsigned char value[1];

    SHDISP_DEBUG("in\n");

    value[0] = bkl_mode;
    value[0] = value[0] & ~msk;
    value[0] |= data;

    SHDISP_DEBUG("[SHDISP]%s bkl_mode=0x%02x\n", __func__, value[0]);

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_BKLMODE_SET, 1, value);
    } else {
        shdisp_FWCMD_buf_init(0);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_BKLMODE_SET, 1, value);
        shdisp_FWCMD_buf_finish();
        shdisp_FWCMD_doKick(1, 0, 0);
    }

    SHDISP_DEBUG("out\n");
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_set_lux_mode                                              */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_set_lux_mode(unsigned char lux_mode, unsigned char data, unsigned char msk)
{
    unsigned char value[1];

    SHDISP_DEBUG("in\n");

    value[0] = lux_mode;
    value[0] = value[0] & ~msk;
    value[0] |= data;

    SHDISP_DEBUG("[SHDISP]%s lux_mode=0x%02x\n", __func__, value[0]);
    shdisp_bdic_bkl_adj_clmr_write_cmd(SHDISP_BDIC_ALS_SET, value, 1);
    SHDISP_DEBUG("out\n");
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_RECOVERY_lux_data_backup                                  */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_RECOVERY_lux_data_backup(void)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_API_RECOVERY_lux_data_restore                                 */
/* ------------------------------------------------------------------------- */
void shdisp_bdic_API_RECOVERY_lux_data_restore(void)
{
}

/* ------------------------------------------------------------------------- */
/* Logical Driver                                                            */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_backlight_off                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_backlight_off(void)
{
    int ret;

    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_BKL, SHDISP_DEV_STATE_ON);
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_BKL, SHDISP_DEV_STATE_ON);

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_SET_MODE_OFF, 0);
    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_STOP, 0);

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    (void)shdisp_pm_psals_power_manager(SHDISP_DEV_TYPE_ALS_BKL, SHDISP_DEV_STATE_OFF);
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_BKL, SHDISP_DEV_STATE_OFF);
    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_BKL|SHDISP_DEV_TYPE_ALS_BKL, SHDISP_DEV_STATE_OFF);
    return;

}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_backlight_fix_on                                          */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_backlight_fix_on(int param)
{
    int ret;

    SHDISP_DEBUG("param:%d\n", param);
    (void)shdisp_pm_clmr_power_manager((SHDISP_DEV_TYPE_BKL|SHDISP_DEV_TYPE_ALS_BKL), SHDISP_DEV_STATE_ON);
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_BKL, SHDISP_DEV_STATE_ON);

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_SET_MODE_FIX, param);
    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    (void)shdisp_pm_psals_power_manager(SHDISP_DEV_TYPE_ALS_BKL, SHDISP_DEV_STATE_ON);

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_POST_START_FIX, 0);

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    SHDISP_DEBUG("Completed.\n");
    return;

}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_backlight_auto_on                                         */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_backlight_auto_on(int param)
{
#ifndef SHDISP_NOT_SUPPORT_BKL_AUTO_MODE
    int ret;

    SHDISP_DEBUG("param:%d\n", param);
    (void)shdisp_pm_clmr_power_manager((SHDISP_DEV_TYPE_BKL|SHDISP_DEV_TYPE_ALS_BKL), SHDISP_DEV_STATE_ON);
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_BKL, SHDISP_DEV_STATE_ON);

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_SET_MODE_AUTO, param);
    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_PRE_START, 0);

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    (void)shdisp_pm_psals_power_manager(SHDISP_DEV_TYPE_ALS_BKL, SHDISP_DEV_STATE_ON);

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_POST_START, 0);

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }
    SHDISP_DEBUG("Completed.\n");
#endif  /* SHDISP_NOT_SUPPORT_BKL_AUTO_MODE */
    return;

}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_PWR_on                                                 */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_PWR_on(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_clmr_api_lcd_vsp_power_on();
    SHDISP_DEBUG("out\n")
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_PWR_off                                                */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_PWR_off(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_clmr_api_lcd_vsp_power_off();
    SHDISP_DEBUG("out\n")
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_M_PWR_on                                               */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_M_PWR_on(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_clmr_api_lcd_vsn_power_on();
    SHDISP_DEBUG("out\n")
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_M_PWR_off                                              */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_M_PWR_off(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_clmr_api_lcd_vsn_power_off();
    SHDISP_DEBUG("out\n")
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_VDD_ON                                                 */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_VDD_ON(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_VDD, SHDISP_GPIO_CTL_HIGH);
    shdisp_SYS_delay_us(10000);
    SHDISP_DEBUG("out\n")
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_VDD_OFF                                                */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_VDD_OFF(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_VDD, SHDISP_GPIO_CTL_LOW);
    shdisp_SYS_delay_us(1000);
    SHDISP_DEBUG("out\n")
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_off                                                   */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_seq_led_off(void)
{
    int ret;

    shdisp_bdic_seq_bdic_active_for_led(SHDISP_DEV_TYPE_LED_NMR|SHDISP_DEV_TYPE_LED_ANI);

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_LED);

    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_STOP, 0);

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();

    shdisp_bdic_seq_bdic_standby_for_led(SHDISP_DEV_TYPE_LED_NMR|SHDISP_DEV_TYPE_LED_ANI);

    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_normal_on                                             */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_seq_led_normal_on(unsigned char color)
{
    int ret;

    SHDISP_DEBUG("color:%d\n", color);
    shdisp_bdic_seq_bdic_active_for_led(SHDISP_DEV_TYPE_LED_NMR);

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_LED);

    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_MODE_NORMAL, color);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_START, 0);

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();

    shdisp_bdic_seq_clmr_off_for_led(SHDISP_DEV_TYPE_LED_NMR);
    SHDISP_DEBUG("End.(%d)\n",ret);

    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_blink_on                                              */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_blink_on(unsigned char color, int ontime, int interval, int count)
{
    shdisp_bdic_seq_bdic_active_for_led(SHDISP_DEV_TYPE_LED_ANI);
    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_LED);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_MODE_BLINK,   color);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_ONTIME,       ontime);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_START, 0);
    shdisp_SYS_bdic_i2c_doKick_if_exist();
    shdisp_bdic_seq_clmr_off_for_led(SHDISP_DEV_TYPE_LED_ANI);
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_firefly_on                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_firefly_on(unsigned char color, int ontime, int interval, int count)
{
    shdisp_bdic_seq_bdic_active_for_led(SHDISP_DEV_TYPE_LED_ANI);
    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_LED);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_MODE_FIREFLY, color);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_ONTIME,       ontime);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    shdisp_bdic_PD_TRI_LED_control(SHDISP_BDIC_REQ_START, 0);
    shdisp_SYS_bdic_i2c_doKick_if_exist();
    shdisp_bdic_seq_clmr_off_for_led(SHDISP_DEV_TYPE_LED_ANI);
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_high_speed_on                                         */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_high_speed_on(unsigned char color, int interval, int count)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_standard_on                                           */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_standard_on(unsigned char color, int interval, int count)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_breath_on                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_breath_on(unsigned char color, int interval, int count)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_long_breath_on                                        */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_long_breath_on(unsigned char color, int interval, int count)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_wave_on                                               */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_wave_on(unsigned char color, int interval, int count)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_flash_on                                              */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_flash_on(unsigned char color, int interval, int count)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_aurora_on                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_aurora_on(int interval, int count)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_led_rainbow_on                                         */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_led_rainbow_on(int interval, int count)
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_PHOTO_SENSOR_get_val                                       */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_LD_PHOTO_SENSOR_get_val(unsigned short *value)
{
    int ret;

    ret = shdisp_pm_is_als_active();
    if (ret == SHDISP_RESULT_FAILURE) {
        SHDISP_ERR("<OTHER> photo sensor user none.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_bdic_PD_REG_ADO_get_opt(value);

    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_PHOTO_SENSOR_get_lux                                       */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_LD_PHOTO_SENSOR_get_lux(unsigned short *value, unsigned long *lux)
{
    int mode = s_state_str.bdic_main_bkl_opt_mode_ado;
    unsigned long ret_lux;
    int ext_flag, ok_lux;
    int i,j;
    int ret;

    ret = shdisp_pm_is_als_active();
    if (ret == SHDISP_RESULT_FAILURE) {
        SHDISP_ERR("<OTHER> photo sensor user none.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_bdic_PD_REG_ADO_get_opt(value);

    ret_lux = shdisp_bdic_bkl_lux_tbl[1][31].lux;
    for (j=0; j<2 ; j++){
        ok_lux = 0;
        for (i=0; i<32 ; i++) {
            if (*value <= shdisp_bdic_bkl_lux_tbl[mode][i].ado_range) {
                ext_flag = shdisp_bdic_bkl_lux_tbl[mode][i].ext_flag;
                if( ext_flag == mode ){
                    ret_lux = shdisp_bdic_bkl_lux_tbl[mode][i].lux;
                    ok_lux = 1;
                }
                else{
                    mode = ext_flag;
                }
                break;
            }
        }
        if ((ok_lux) || (i >= 32)) {
            break;
        }
    }

    *lux = ret_lux;

#ifdef SHDISP_SW_BDIC_IRQ_LOG
    if( s_state_str.bdic_main_bkl_opt_mode_ado == mode )
        printk("[SHDISP] shdisp_bdic_LD_PHOTO_SENSOR_get_lux : mode=%d / ado=0x%04X, lux=%lu\n", mode, *value, ret_lux);
    else
        printk("[SHDISP] shdisp_bdic_LD_PHOTO_SENSOR_get_lux : ext_mode=%d / ado=0x%04X, lux=%lu\n", mode, *value, ret_lux);
#endif /* SHDISP_SW_BDIC_IRQ_LOG */

    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_i2c_write                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_LD_i2c_write(struct shdisp_bdic_i2c_msg *msg)
{
    unsigned char value = 0x00;
    int i;

#ifdef SHDISP_SW_BDIC_I2C_RWLOG
    int t;
    if (msg->wlen <= 0) {
        printk("[SHDISP] bdic i2c Write(addr=0x%02X, size=%d)\n", msg->addr, msg->wlen);
    }
    else {
        printk("[SHDISP] bdic i2c Write(addr=0x%02X, size=%d Wbuf=", msg->addr, msg->wlen);
        for (t=0; t<(msg->wlen-1); t++) {
            printk("0x%02X,", (msg->wbuf[t] & 0xFF));
        }
        printk("0x%02X)\n", (msg->wbuf[t] & 0xFF));
    }
#endif

    if (msg->wbuf == NULL) {
        SHDISP_ERR("<NULL_POINTER> msg->wbuf.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if (msg->wlen > SHDISP_BDIC_I2C_WBUF_MAX) {
        SHDISP_ERR("<INVALID_VALUE> msg->wlen(%d).\n", msg->wlen);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_SYS_delay_us(1000);




    shdisp_bdic_i2c_errflag = 0;

    shdisp_SYS_delay_us(SHDISP_BDIC_I2C_SEND_WAIT);

    for (i=0; i<=SHDISP_BDIC_I2C_SEND_RETRY; i++) {
        if (shdisp_bdic_i2c_errflag != 0) {
            SHDISP_ERR("<OTHER> i2c write isr.\n");
            return SHDISP_RESULT_FAILURE_I2C_TMO;
        }
        else {
            if ((value & SHDISP_BDIC_I2C_W_START) == 0x00) {
                return SHDISP_RESULT_SUCCESS;
            }
            else {
                shdisp_SYS_delay_us(SHDISP_BDIC_I2C_SEND_RETRY_WAIT);
            }
        }
    }

    SHDISP_ERR("<OTHER> i2c write timeout.\n");
    return SHDISP_RESULT_FAILURE_I2C_TMO;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_i2c_read_mode0                                             */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_LD_i2c_read_mode0(struct shdisp_bdic_i2c_msg *msg)
{
    unsigned char value = 0x00;
    int i, t;

    if (msg->wbuf == NULL) {
        SHDISP_ERR("<NULL_POINTER> msg->wbuf.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if (msg->wlen != 0x01) {
        SHDISP_ERR("<INVALID_VALUE> msg->wlen(%d).\n", msg->wlen);
        return SHDISP_RESULT_FAILURE;
    }

    if (msg->rbuf == NULL) {
        SHDISP_ERR("<NULL_POINTER> msg->rbuf.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if (msg->rlen > SHDISP_BDIC_I2C_RBUF_MAX) {
        SHDISP_ERR("<INVALID_VALUE> msg->rlen(%d).\n", msg->rlen);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_SYS_delay_us(1000);





    shdisp_bdic_i2c_errflag = 0;

    shdisp_SYS_delay_us(SHDISP_BDIC_I2C_SEND_WAIT);

    for (i=0; i<=SHDISP_BDIC_I2C_SEND_RETRY; i++) {
        if (shdisp_bdic_i2c_errflag != 0) {
            SHDISP_ERR("<OTHER> i2c write isr.\n");
            return SHDISP_RESULT_FAILURE_I2C_TMO;
        }
        else {
            if ((value & SHDISP_BDIC_I2C_R_START) == 0x00) {
                for (t=0; t<msg->rlen; t++) {
                    msg->rbuf[t] = value;
                }
                return SHDISP_RESULT_SUCCESS;
            }
            else {
                shdisp_SYS_delay_us(SHDISP_BDIC_I2C_SEND_RETRY_WAIT);
            }
        }
    }

    SHDISP_ERR("<OTHER> i2c write timeout.\n");
    return SHDISP_RESULT_FAILURE_I2C_TMO;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_i2c_read_mode1                                             */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_LD_i2c_read_mode1(struct shdisp_bdic_i2c_msg *msg)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_i2c_read_mode2                                             */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_LD_i2c_read_mode2(struct shdisp_bdic_i2c_msg *msg)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_i2c_read_mode3                                             */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_LD_i2c_read_mode3(struct shdisp_bdic_i2c_msg *msg)
{
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_psals_active                                              */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_psals_active(unsigned long dev_type)
{
    (void)shdisp_pm_clmr_power_manager(dev_type, SHDISP_DEV_STATE_ON);
    (void)shdisp_pm_psals_power_manager(dev_type, SHDISP_DEV_STATE_ON);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_psals_standby                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_psals_standby(unsigned long dev_type)
{
    (void)shdisp_pm_psals_power_manager(dev_type, SHDISP_DEV_STATE_OFF);
    (void)shdisp_pm_clmr_power_manager(dev_type, SHDISP_DEV_STATE_OFF);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_ps_background                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_ps_background(unsigned long state)
{
    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_PS, state);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_dtv_on                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_dtv_on(void)
{
    int ret;

    if (shdisp_bdic_dtv == SHDISP_BDIC_BKL_DTV_ON) {
        return;
    }

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_DTV_ON, 0);

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_dtv_off                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_dtv_off(void)
{
    int ret;

    if (shdisp_bdic_dtv == SHDISP_BDIC_BKL_DTV_OFF) {
        return;
    }

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_DTV_OFF, 0);

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_emg_on                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_emg_on(void)
{
    int ret;

    if (shdisp_bdic_emg == SHDISP_BDIC_BKL_EMG_ON) {
        return;
    }

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_EMG_ON, 0);

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_emg_off                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_emg_off(void)
{
    int ret;

    if (shdisp_bdic_emg == SHDISP_BDIC_BKL_EMG_OFF) {
        return;
    }

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_EMG_OFF, 0);

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_get_mode                                           */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_get_mode(int *mode)
{

    if (shdisp_bdic_emg == SHDISP_BDIC_BKL_EMG_ON) {
        *mode = SHDISP_BKL_TBL_MODE_EMERGENCY;
    }
    else if ((shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_FIX) && (shdisp_bdic_chg == SHDISP_BDIC_BKL_CHG_ON)) {
        *mode = SHDISP_BKL_TBL_MODE_CHARGE;
    }
    else if (shdisp_bdic_eco == SHDISP_BDIC_BKL_ECO_ON) {
        *mode = SHDISP_BKL_TBL_MODE_ECO;
    }
    else {
        *mode = SHDISP_BKL_TBL_MODE_NORMAL;
    }

    if (*mode >= NUM_SHDISP_BKL_TBL_MODE) {
        *mode = SHDISP_BKL_TBL_MODE_NORMAL;
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_eco_on                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_eco_on(void)
{
    int ret;

    if (shdisp_bdic_eco == SHDISP_BDIC_BKL_ECO_ON) {
        return;
    }

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_ECO_ON, 0);

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_eco_off                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_eco_off(void)
{
    int ret;

    if (shdisp_bdic_eco == SHDISP_BDIC_BKL_ECO_OFF) {
        return;
    }

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_ECO_OFF, 0);

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_chg_on                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_chg_on(void)
{
    int ret;

    if (shdisp_bdic_chg == SHDISP_BDIC_BKL_CHG_ON) {
        return;
    }

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_CHG_ON, 0);

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_LD_LCD_BKL_chg_off                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_LD_LCD_BKL_chg_off(void)
{
    int ret;

    if (shdisp_bdic_chg == SHDISP_BDIC_BKL_CHG_OFF) {
        return;
    }

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_BKL_CHG_OFF, 0);

    if (shdisp_bdic_bkl_mode != SHDISP_BDIC_BKL_MODE_OFF) {
        shdisp_bdic_PD_BKL_control(SHDISP_BDIC_REQ_START, 0);
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_doKick_if_exist.\n");
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* Phygical Driver                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_bdic_active_for_led                                       */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_bdic_active_for_led(int dev_type)
{
    SHDISP_DEBUG("dev_type:%d\n", dev_type);
    (void)shdisp_pm_clmr_power_manager(dev_type, SHDISP_DEV_STATE_ON);
    (void)shdisp_pm_bdic_power_manager(dev_type, SHDISP_DEV_STATE_ON);
    SHDISP_DEBUG("Completed\n");
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_bdic_standby_for_led                                      */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_bdic_standby_for_led(int dev_type)
{
    (void)shdisp_pm_bdic_power_manager(dev_type, SHDISP_DEV_STATE_OFF);
    (void)shdisp_pm_clmr_power_manager(dev_type, SHDISP_DEV_STATE_OFF);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_seq_clmr_off_for_led                                          */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_seq_clmr_off_for_led(int dev_type)
{
    (void)shdisp_pm_clmr_power_manager(dev_type, SHDISP_DEV_STATE_OFF);
    return;
}

/*---------------------------------------------------------------------------*/
/*      shdisp_bdic_seq_regset                                               */
/*---------------------------------------------------------------------------*/
static int shdisp_bdic_seq_regset(const shdisp_bdicRegSetting_t* regtable, int size)
{
    int i;
    int ret = SHDISP_RESULT_SUCCESS;
    shdisp_bdicRegSetting_t* tbl;

    tbl = (shdisp_bdicRegSetting_t*)regtable;
    for (i = 0; i < size; i++) {
        switch(tbl->flg) {
        case SHDISP_BDIC_STR:
            ret = shdisp_bdic_IO_write_reg(tbl->addr, tbl->data);
            break;
        case SHDISP_BDIC_SET:
            ret = shdisp_bdic_IO_set_bit_reg(tbl->addr, tbl->data);
            break;
        case SHDISP_BDIC_CLR:
            ret = shdisp_bdic_IO_clr_bit_reg(tbl->addr, tbl->data);
            break;
        default:
            break;
        }
        if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("bdic R/W Error addr=%02X, data=%02X\n", tbl->addr, tbl->data);
            return ret;
        }
        if (tbl->wait > 0) {
            shdisp_SYS_cmd_delay_us(tbl->wait);
        }
        tbl++;
    }
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_set_hw_reset                                               */
/* ------------------------------------------------------------------------- */
static void shdisp_bdic_PD_set_hw_reset(void)
{
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_BL_RST_N, SHDISP_GPIO_CTL_LOW);
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_rel_hw_reset                                               */
/* ------------------------------------------------------------------------- */
static void shdisp_bdic_PD_rel_hw_reset(void)
{
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_BL_RST_N, SHDISP_GPIO_CTL_HIGH);
    shdisp_SYS_delay_us(10000);
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_set_active                                                 */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_PD_set_active(int power_status)
{
    int ret;

    SHDISP_TRACE("[S] \n");

    shdisp_bdic_PD_rel_hw_reset();

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);

    SHDISP_BDIC_REGSET(shdisp_bdic_init);

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();

    SHDISP_TRACE("[E] ret=%d\n", ret);

    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_set_standby                                                */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_PD_set_standby(void)
{

    SHDISP_TRACE("[S]\n");

    shdisp_bdic_PD_set_hw_reset();

    SHDISP_TRACE("[E]\n");

    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_BKL_control                                                */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_PD_BKL_control(unsigned char request, int param)
{

    unsigned char value[1];

    switch (request) {
    case SHDISP_BDIC_REQ_ACTIVE:
        break;

    case SHDISP_BDIC_REQ_STANDBY:
        break;

    case SHDISP_BDIC_REQ_START:
        SHDISP_BDIC_REGSET(shdisp_bdic_bklon);

        if(shdisp_bdic_bkl_before_mode == SHDISP_BDIC_BKL_MODE_OFF){

            shdisp_bdic_PWM_set_value(SHDISP_BDIC_BL_PWM_FIX_PARAM);

            value[0] = 0x01;
            shdisp_bdic_bkl_adj_clmr_write_cmd(SHDISP_BDIC_BL_MODE_SET, value, 1);

            value[0] = 0x01;
            shdisp_bdic_bkl_adj_clmr_write_cmd(SHDISP_BDIC_ALS_PARAM_SET, value, 1);

        }
        else if(shdisp_bdic_bkl_before_mode == SHDISP_BDIC_BKL_MODE_FIX){

            shdisp_bdic_PWM_set_value(SHDISP_BDIC_BL_PWM_FIX_PARAM);

            value[0] = 0x01;
            shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_ALS_PARAM_SET, value , 1 );
        }
        else if(shdisp_bdic_bkl_before_mode == SHDISP_BDIC_BKL_MODE_AUTO){
            if ( shdisp_bdic_bkl_mode == SHDISP_BDIC_BKL_MODE_FIX ) {

                shdisp_bdic_PWM_set_value(SHDISP_BDIC_BL_PWM_FIX_PARAM);

                value[0] = 0x01;
                shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_BL_MODE_SET, value , 1 );

                value[0] = 0x01;
                shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_ALS_PARAM_SET, value , 1 );

            }
            else if ( shdisp_bdic_bkl_mode == SHDISP_BDIC_BKL_MODE_AUTO ) {

                shdisp_bdic_PWM_set_value(SHDISP_BDIC_BL_PWM_AUTO_PARAM);

            }
        }
        break;

    case SHDISP_BDIC_REQ_PRE_START:
        SHDISP_BDIC_REGSET(shdisp_bdic_bklon);

        if(shdisp_bdic_bkl_before_mode == SHDISP_BDIC_BKL_MODE_OFF){
            shdisp_bdic_PWM_set_value(SHDISP_BDIC_BL_PWM_FIX_PARAM);

            shdisp_bdic_PWM_set_value(SHDISP_BDIC_BL_PWM_AUTO_PARAM);

            value[0] = 0x03;
            shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_BL_MODE_SET, value , 1 );

            value[0] = 0x01;
            shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_ALS_PARAM_SET, value , 1 );
        }
        break;

    case SHDISP_BDIC_REQ_POST_START_FIX:
        break;

    case SHDISP_BDIC_REQ_POST_START:
        if(shdisp_bdic_bkl_before_mode == SHDISP_BDIC_BKL_MODE_OFF){
        }
        else if(shdisp_bdic_bkl_before_mode == SHDISP_BDIC_BKL_MODE_FIX){

            shdisp_bdic_PWM_set_value(SHDISP_BDIC_BL_PWM_AUTO_PARAM);

            value[0] = 0x03;
            shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_BL_MODE_SET, value , 1 );

        }
        else if(shdisp_bdic_bkl_before_mode == SHDISP_BDIC_BKL_MODE_AUTO){
            shdisp_bdic_PWM_set_value(SHDISP_BDIC_BL_PWM_AUTO_PARAM);
        }

        break;

    case SHDISP_BDIC_REQ_STOP:
        value[0] = 0x00;
        shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_BL_MODE_SET, value, 1 );

        shdisp_bdic_bkl_mode  = SHDISP_BDIC_BKL_MODE_OFF;
        shdisp_bdic_bkl_param = 0;
        SHDISP_BDIC_REGSET(shdisp_bdic_bkloff);
        break;

    case SHDISP_BDIC_REQ_BKL_SET_MODE_OFF:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_bkl_mode  = SHDISP_BDIC_BKL_MODE_OFF;
        shdisp_bdic_bkl_param = param;
        break;

    case SHDISP_BDIC_REQ_BKL_SET_MODE_FIX:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_bkl_mode  = SHDISP_BDIC_BKL_MODE_FIX;
        shdisp_bdic_bkl_param = param;
        break;

    case SHDISP_BDIC_REQ_BKL_SET_MODE_AUTO:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_bkl_mode  = SHDISP_BDIC_BKL_MODE_AUTO;
        shdisp_bdic_bkl_param = param;
        break;

    case SHDISP_BDIC_REQ_BKL_DTV_OFF:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_dtv = SHDISP_BDIC_BKL_DTV_OFF;
        break;

    case SHDISP_BDIC_REQ_BKL_DTV_ON:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_dtv = SHDISP_BDIC_BKL_DTV_ON;
        break;

    case SHDISP_BDIC_REQ_BKL_EMG_OFF:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_emg = SHDISP_BDIC_BKL_EMG_OFF;
        break;

    case SHDISP_BDIC_REQ_BKL_EMG_ON:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_emg = SHDISP_BDIC_BKL_EMG_ON;
        break;

    case SHDISP_BDIC_REQ_BKL_ECO_OFF:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_eco = SHDISP_BDIC_BKL_ECO_OFF;
        break;

    case SHDISP_BDIC_REQ_BKL_ECO_ON:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_eco = SHDISP_BDIC_BKL_ECO_ON;
        break;

    case SHDISP_BDIC_REQ_BKL_CHG_OFF:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_chg = SHDISP_BDIC_BKL_CHG_OFF;
        break;

    case SHDISP_BDIC_REQ_BKL_CHG_ON:
        shdisp_bdic_bkl_before_mode = shdisp_bdic_bkl_mode;
        shdisp_bdic_chg = SHDISP_BDIC_BKL_CHG_ON;
        break;

    default:
        break;
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_GPIO_control                                               */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_PD_GPIO_control(unsigned char port, unsigned char status)
{
    shdisp_SYS_set_Host_gpio((int)port, (int)status);
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_TRI_LED_control                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_PD_TRI_LED_control(unsigned char request, int param)
{

    switch (request) {
    case SHDISP_BDIC_REQ_ACTIVE:
        break;

    case SHDISP_BDIC_REQ_STANDBY:
        break;

    case SHDISP_BDIC_REQ_START:
        switch (shdisp_bdic_tri_led_mode) {
        case SHDISP_BDIC_TRI_LED_MODE_NORMAL:
        case SHDISP_BDIC_TRI_LED_MODE_BLINK:
        case SHDISP_BDIC_TRI_LED_MODE_FIREFLY:
            SHDISP_BDIC_REGSET(shdisp_bdic_ledoff);
            SHDISP_BDIC_REGSET(shdisp_bdic_led_anime_off);
            shdisp_SYS_cmd_delay_us(5500);
            break;
        default:
            break;
        }

        shdisp_bdic_PD_TRI_LED_set_anime();
        shdisp_bdic_PD_TRI_LED_set_chdig();

        switch (shdisp_bdic_tri_led_mode) {
        case SHDISP_BDIC_TRI_LED_MODE_NORMAL:
        case SHDISP_BDIC_TRI_LED_MODE_BLINK:
        case SHDISP_BDIC_TRI_LED_MODE_FIREFLY:
            SHDISP_BDIC_REGSET(shdisp_bdic_ledon);
            break;
        default:
            break;
        }
        break;
    case SHDISP_BDIC_REQ_STOP:
        SHDISP_BDIC_REGSET(shdisp_bdic_ledoff);
        shdisp_bdic_tri_led_mode     = SHDISP_BDIC_TRI_LED_MODE_NORMAL;
        shdisp_bdic_tri_led_color    = 0;
        shdisp_bdic_tri_led_ontime   = 0;
        shdisp_bdic_tri_led_interval = 0;
        shdisp_bdic_tri_led_count    = 0;
        break;

    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_NORMAL:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_NORMAL;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;

    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_BLINK:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_BLINK;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;

    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_FIREFLY:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_FIREFLY;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;

    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_HISPEED:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_HISPEED;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;
    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_STANDARD:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_STANDARD;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;
    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_BREATH:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_BREATH;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;
    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_LONG_BREATH:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_LONG_BREATH;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;
    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_WAVE:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_WAVE;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;
    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_FLASH:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_FLASH;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;
    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_AURORA:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_AURORA;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;
    case SHDISP_BDIC_REQ_TRI_LED_SET_MODE_RAINBOW:
        shdisp_bdic_tri_led_mode  = SHDISP_BDIC_TRI_LED_MODE_RAINBOW;
        shdisp_bdic_tri_led_color = (unsigned char)param;
        break;

    case SHDISP_BDIC_REQ_TRI_LED_SET_ONTIME:
        shdisp_bdic_tri_led_ontime = param;
        break;

    case SHDISP_BDIC_REQ_TRI_LED_SET_INTERVAL:
        shdisp_bdic_tri_led_interval = param;
        break;

    case SHDISP_BDIC_REQ_TRI_LED_SET_COUNT:
        shdisp_bdic_tri_led_count = param;
        break;

    default:
        break;
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_TRI_LED_set_anime                                          */
/* ------------------------------------------------------------------------- */
static void shdisp_bdic_PD_TRI_LED_set_anime(void)
{
    switch (shdisp_bdic_tri_led_mode) {
    case SHDISP_BDIC_TRI_LED_MODE_NORMAL:
        SHDISP_BDIC_REGSET(shdisp_bdic_led_anime_off);
        break;

    case SHDISP_BDIC_TRI_LED_MODE_BLINK:
        if (shdisp_bdic_tri_led_color & 0x01) {
            shdisp_bdic_led_blink_on[3].data    = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][0];
            shdisp_bdic_led_blink_on[4].data    = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][1];
            shdisp_bdic_led_blink_on[5].data    = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][2];
            shdisp_bdic_led_blink_on[6].data    = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][3];
            shdisp_bdic_led_blink_on[7].data    = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][4];
            shdisp_bdic_led_blink_on[8].data    = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][5];
            shdisp_bdic_led_blink_on[21].data  |= 0x01;
        }
        else {
            shdisp_bdic_led_blink_on[3].data    = 0x00;
            shdisp_bdic_led_blink_on[4].data    = 0x00;
            shdisp_bdic_led_blink_on[5].data    = 0x00;
            shdisp_bdic_led_blink_on[6].data    = 0x00;
            shdisp_bdic_led_blink_on[7].data    = 0x00;
            shdisp_bdic_led_blink_on[8].data    = 0x00;
            shdisp_bdic_led_blink_on[21].data  &= ~0x01;
        }

        if (shdisp_bdic_tri_led_color & 0x02) {
            shdisp_bdic_led_blink_on[9].data    = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][0];
            shdisp_bdic_led_blink_on[10].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][1];
            shdisp_bdic_led_blink_on[11].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][2];
            shdisp_bdic_led_blink_on[12].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][3];
            shdisp_bdic_led_blink_on[13].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][4];
            shdisp_bdic_led_blink_on[14].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][5];
            shdisp_bdic_led_blink_on[21].data  |= 0x04;
        }
        else {
            shdisp_bdic_led_blink_on[9].data    = 0x00;
            shdisp_bdic_led_blink_on[10].data   = 0x00;
            shdisp_bdic_led_blink_on[11].data   = 0x00;
            shdisp_bdic_led_blink_on[12].data   = 0x00;
            shdisp_bdic_led_blink_on[13].data   = 0x00;
            shdisp_bdic_led_blink_on[14].data   = 0x00;
            shdisp_bdic_led_blink_on[21].data  &= ~0x04;
        }

        if (shdisp_bdic_tri_led_color & 0x04) {
            shdisp_bdic_led_blink_on[15].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][0];
            shdisp_bdic_led_blink_on[16].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][1];
            shdisp_bdic_led_blink_on[17].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][2];
            shdisp_bdic_led_blink_on[18].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][3];
            shdisp_bdic_led_blink_on[19].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][4];
            shdisp_bdic_led_blink_on[20].data   = shdisp_triple_led_blink_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][5];
            shdisp_bdic_led_blink_on[21].data  |= 0x10;
        }
        else {
            shdisp_bdic_led_blink_on[15].data   = 0x00;
            shdisp_bdic_led_blink_on[16].data   = 0x00;
            shdisp_bdic_led_blink_on[17].data   = 0x00;
            shdisp_bdic_led_blink_on[18].data   = 0x00;
            shdisp_bdic_led_blink_on[19].data   = 0x00;
            shdisp_bdic_led_blink_on[20].data   = 0x00;
            shdisp_bdic_led_blink_on[21].data  &= ~0x10;
        }

        SHDISP_BDIC_REGSET(shdisp_bdic_led_blink_on);
        break;

    case SHDISP_BDIC_TRI_LED_MODE_FIREFLY:
        if (shdisp_bdic_tri_led_color & 0x01) {
            shdisp_bdic_led_anime_firefly[3].data    = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][0];
            shdisp_bdic_led_anime_firefly[4].data    = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][1];
            shdisp_bdic_led_anime_firefly[5].data    = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][2];
            shdisp_bdic_led_anime_firefly[6].data    = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][3];
            shdisp_bdic_led_anime_firefly[7].data    = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][4];
            shdisp_bdic_led_anime_firefly[8].data    = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][5];
            shdisp_bdic_led_anime_firefly[21].data  |= 0x01;
        }
        else {
            shdisp_bdic_led_anime_firefly[3].data    = 0x00;
            shdisp_bdic_led_anime_firefly[4].data    = 0x00;
            shdisp_bdic_led_anime_firefly[5].data    = 0x00;
            shdisp_bdic_led_anime_firefly[6].data    = 0x00;
            shdisp_bdic_led_anime_firefly[7].data    = 0x00;
            shdisp_bdic_led_anime_firefly[8].data    = 0x00;
            shdisp_bdic_led_anime_firefly[21].data  &= ~0x01;
        }

        if (shdisp_bdic_tri_led_color & 0x02) {
            shdisp_bdic_led_anime_firefly[9].data    = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][0];
            shdisp_bdic_led_anime_firefly[10].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][1];
            shdisp_bdic_led_anime_firefly[11].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][2];
            shdisp_bdic_led_anime_firefly[12].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][3];
            shdisp_bdic_led_anime_firefly[13].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][4];
            shdisp_bdic_led_anime_firefly[14].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][5];
            shdisp_bdic_led_anime_firefly[21].data  |= 0x04;
        }
        else {
            shdisp_bdic_led_anime_firefly[9].data    = 0x00;
            shdisp_bdic_led_anime_firefly[10].data   = 0x00;
            shdisp_bdic_led_anime_firefly[11].data   = 0x00;
            shdisp_bdic_led_anime_firefly[12].data   = 0x00;
            shdisp_bdic_led_anime_firefly[13].data   = 0x00;
            shdisp_bdic_led_anime_firefly[14].data   = 0x00;
            shdisp_bdic_led_anime_firefly[21].data  &= ~0x04;
        }

        if (shdisp_bdic_tri_led_color & 0x04) {
            shdisp_bdic_led_anime_firefly[15].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][0];
            shdisp_bdic_led_anime_firefly[16].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][1];
            shdisp_bdic_led_anime_firefly[17].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][2];
            shdisp_bdic_led_anime_firefly[18].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][3];
            shdisp_bdic_led_anime_firefly[19].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][4];
            shdisp_bdic_led_anime_firefly[20].data   = shdisp_triple_led_firefly_tbl[shdisp_bdic_tri_led_ontime][shdisp_bdic_tri_led_interval][5];
            shdisp_bdic_led_anime_firefly[21].data  |= 0x10;
        }
        else {
            shdisp_bdic_led_anime_firefly[15].data   = 0x00;
            shdisp_bdic_led_anime_firefly[16].data   = 0x00;
            shdisp_bdic_led_anime_firefly[17].data   = 0x00;
            shdisp_bdic_led_anime_firefly[18].data   = 0x00;
            shdisp_bdic_led_anime_firefly[19].data   = 0x00;
            shdisp_bdic_led_anime_firefly[20].data   = 0x00;
            shdisp_bdic_led_anime_firefly[21].data  &= ~0x10;
        }

        SHDISP_BDIC_REGSET(shdisp_bdic_led_anime_firefly);
        break;

    case SHDISP_BDIC_TRI_LED_MODE_HISPEED:
    case SHDISP_BDIC_TRI_LED_MODE_STANDARD:
    case SHDISP_BDIC_TRI_LED_MODE_BREATH:
    case SHDISP_BDIC_TRI_LED_MODE_LONG_BREATH:
    case SHDISP_BDIC_TRI_LED_MODE_WAVE:
    case SHDISP_BDIC_TRI_LED_MODE_FLASH:
    case SHDISP_BDIC_TRI_LED_MODE_AURORA:
    case SHDISP_BDIC_TRI_LED_MODE_RAINBOW:
    default:
        break;
    }

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_TRI_LED_set_chdig                                          */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_PD_TRI_LED_set_chdig(void)
{
    int clrvari = 0;
    unsigned char wBuf[3];

    switch (shdisp_bdic_tri_led_mode) {
    case SHDISP_BDIC_TRI_LED_MODE_NORMAL:
        wBuf[0] = shdisp_triple_led_tbl[clrvari][shdisp_bdic_tri_led_color][0];
        wBuf[1] = shdisp_triple_led_tbl[clrvari][shdisp_bdic_tri_led_color][1];
        wBuf[2] = shdisp_triple_led_tbl[clrvari][shdisp_bdic_tri_led_color][2];
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_C, wBuf[0]);
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_D, wBuf[1]);
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_E, wBuf[2]);
        break;

    case SHDISP_BDIC_TRI_LED_MODE_BLINK:
    case SHDISP_BDIC_TRI_LED_MODE_FIREFLY:
        wBuf[0] = shdisp_triple_led_anime_tbl[clrvari][1][shdisp_bdic_tri_led_color][0];
        wBuf[1] = shdisp_triple_led_anime_tbl[clrvari][1][shdisp_bdic_tri_led_color][1];
        wBuf[2] = shdisp_triple_led_anime_tbl[clrvari][1][shdisp_bdic_tri_led_color][2];
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_C, wBuf[0]);
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_D, wBuf[1]);
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_E, wBuf[2]);
        break;

    case SHDISP_BDIC_TRI_LED_MODE_HISPEED:
    case SHDISP_BDIC_TRI_LED_MODE_STANDARD:
    case SHDISP_BDIC_TRI_LED_MODE_BREATH:
    case SHDISP_BDIC_TRI_LED_MODE_LONG_BREATH:
    case SHDISP_BDIC_TRI_LED_MODE_WAVE:
    case SHDISP_BDIC_TRI_LED_MODE_FLASH:
    case SHDISP_BDIC_TRI_LED_MODE_AURORA:
    case SHDISP_BDIC_TRI_LED_MODE_RAINBOW:
        wBuf[0] = 0x00;
        wBuf[1] = 0x00;
        wBuf[2] = 0x00;
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_C, wBuf[0]);
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_D, wBuf[1]);
        shdisp_bdic_IO_write_reg(BDIC_REG_BRIGHT_E, wBuf[2]);
        break;
    default:
        break;
    }
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PWM_set_value                                                 */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_PWM_set_value(int pwm_param)
{
    unsigned char value[3] = { 0, 0, 0 };
    unsigned short temp_val;
    int mode = 0;
    unsigned char *pwm_value;

    switch (shdisp_bdic_bkl_mode) {
    case SHDISP_BDIC_BKL_MODE_OFF:
        return;

    case SHDISP_BDIC_BKL_MODE_FIX:
        shdisp_bdic_LD_LCD_BKL_get_mode(&mode);

        if (shdisp_bdic_dtv == SHDISP_BDIC_BKL_DTV_ON) {
            temp_val = shdisp_main_dtv_bkl_tbl[shdisp_bdic_bkl_param][mode];
        }
        else {
            temp_val = shdisp_main_bkl_tbl[shdisp_bdic_bkl_param][mode];
        }
        value[0] = 0x5D;
        value[1] = ( temp_val & 0x00ff );
        value[2] = ( temp_val >> 8 );

        shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_BL_PARAM_WRITE, value, sizeof(value) );

        break;
    case SHDISP_BDIC_BKL_MODE_AUTO:

        shdisp_bdic_LD_LCD_BKL_get_mode(&mode);

        if( pwm_param == SHDISP_BDIC_BL_PWM_FIX_PARAM){
            if (shdisp_bdic_dtv == SHDISP_BDIC_BKL_DTV_ON) {
                temp_val = shdisp_main_dtv_bkl_tbl[SHDISP_MAIN_BKL_PARAM_15][mode];
            }
            else {
                temp_val = shdisp_main_bkl_tbl[SHDISP_MAIN_BKL_PARAM_15][mode];
            }
            value[0] = 0x5D;
            value[1] = ( temp_val & 0x00ff );
            value[2] = ( temp_val >> 8 );

            shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_BL_PARAM_WRITE, value, sizeof(value) );

        }
        else{

            if( s_state_str.bdic_main_bkl_opt_mode_output == SHDISP_BDIC_MAIN_BKL_OPT_LOW ){

                pwm_value = (char *)shdisp_main_bkl_pwm_low_tbl;
                shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_BL_PARAM_WRITE,
                                                    pwm_value+1,
                                                    129 );

            }
            else{
                pwm_value = (char *)shdisp_main_bkl_pwm_high_tbl;
                shdisp_bdic_bkl_adj_clmr_write_cmd( SHDISP_BDIC_BL_PARAM_WRITE,
                                                    pwm_value+1,
                                                    129 );
            }
        }
        break;

    default:
        return;

    }
    return;

}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_psals_power_on                                             */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_PD_psals_power_on(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_ALS_VCC, SHDISP_GPIO_CTL_HIGH);
    shdisp_SYS_delay_us(5000);
    SHDISP_DEBUG("out\n")
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_psals_power_off                                            */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_PD_psals_power_off(void)
{
    SHDISP_DEBUG("in\n")
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_ALS_VCC, SHDISP_GPIO_CTL_LOW);
    SHDISP_DEBUG("out\n")
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_psals_ps_init                                              */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_PD_psals_ps_init(int *state)
{
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_psals_ps_deinit                                            */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_PD_psals_ps_deinit(int *state)
{
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_psals_als_init                                             */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_PD_psals_als_init(int *state)
{
    int ret;
    int als_mode;
    unsigned char lux_mode;
    int lux_data;


    SHDISP_DEBUG("[S] state = %d\n", *state);

    lux_data = shdisp_bdic_API_get_lux_data();
    lux_mode = (( lux_data  & 0x0000ff00 ) >> 8);

    shdisp_pm_get_als_sensor_param(&als_mode);
    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_PHOTESENSOR);

    switch (*state) {
    case SHDISP_SENSOR_STATE_PROX_OFF_ALC_OFF:
    case SHDISP_SENSOR_STATE_PROX_ON_ALC_OFF:
        shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND1, 0x00);
        if (als_mode == SHDISP_BDIC_MAIN_BKL_OPT_LOW) {
            shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND2, 0x1C);
        }
        else {
            shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND2, 0x1E);
        }
        shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND3, 0x10);
        shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND4, 0x00);
        shdisp_SYS_cmd_delay_us(1000);
        shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND1, 0xD0);
        shdisp_SYS_cmd_delay_us(1000);
        shdisp_bdic_API_set_lux_mode(lux_mode, 0x01 , 0x01);
        *state = SHDISP_SENSOR_STATE_PROX_OFF_ALC_ON;
        break;
    case SHDISP_SENSOR_STATE_PROX_OFF_ALC_ON:
    case SHDISP_SENSOR_STATE_PROX_ON_ALC_ON:
        break;
    default:
        break;
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();

    SHDISP_DEBUG("[E] ret=%d\n", ret);
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_psals_als_deinit                                           */
/* ------------------------------------------------------------------------- */
int shdisp_bdic_PD_psals_als_deinit(int *state)
{
    int ret;
    unsigned char lux_mode, bkl_mode;
    int lux_data;

    SHDISP_DEBUG("[S] state = %d\n", *state);

    lux_data = shdisp_bdic_API_get_lux_data();
    bkl_mode = lux_data  & 0x000000ff;
    lux_mode = (( lux_data  & 0x0000ff00 ) >> 8);

    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_PHOTESENSOR);

    switch (*state) {
    case SHDISP_SENSOR_STATE_PROX_OFF_ALC_OFF:
    case SHDISP_SENSOR_STATE_PROX_ON_ALC_OFF:
        break;
    case SHDISP_SENSOR_STATE_PROX_OFF_ALC_ON:
    case SHDISP_SENSOR_STATE_PROX_ON_ALC_ON:
        shdisp_bdic_API_set_lux_mode(lux_mode, 0x00 , 0x01);
        shdisp_bdic_API_set_bkl_mode(bkl_mode, 0x04 , 0x04);
        shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND1, 0x80);
        shdisp_SYS_cmd_delay_us(1000);
        shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND1, 0x00);
        *state = SHDISP_SENSOR_STATE_PROX_OFF_ALC_OFF;
        break;
    default:
        break;
    }

    ret = shdisp_SYS_bdic_i2c_doKick_if_exist();

    SHDISP_DEBUG("[E] ret=%d\n", ret);

    return ret;
}
#if 0
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_psals_burst_write                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_bdic_PD_psals_burst_write(unsigned char *wval, unsigned char cnt)
{
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_psals_write_threshold                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_bdic_PD_psals_write_threshold(void)
{
    int ret;
    unsigned char wBuf[SHDISP_BDIC_I2C_WBUF_MAX];

    wBuf[0] = (unsigned char)(s_state_str.prox_params.threshold_low & 0x00FF);
    wBuf[1] = (unsigned char)((s_state_str.prox_params.threshold_low >> 8) & 0x00FF);
    wBuf[2] = (unsigned char)(s_state_str.prox_params.threshold_high & 0x00FF);
    wBuf[3] = (unsigned char)((s_state_str.prox_params.threshold_high >> 8) & 0x00FF);

    ret = shdisp_photo_sensor_IO_write_reg(0x08 , wBuf[0]);
    ret = shdisp_photo_sensor_IO_write_reg(0x09 , wBuf[1]);
    ret = shdisp_photo_sensor_IO_write_reg(0x0A , wBuf[2]);
    ret = shdisp_photo_sensor_IO_write_reg(0x0B , wBuf[3]);

    return ret;
}
#endif

#ifndef SHDISP_BDIC_USE_ALS_I2CCMD
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_als_clmr_write_cmd                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_bdic_als_clmr_write_cmd(unsigned char reg, unsigned char val)
{
    int result = SHDISP_RESULT_SUCCESS;
    char ary[2];

    ary[0] = reg;
    ary[1] = val;

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_ALS_1BYTE_WRITE, 2, ary);
    }
    else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_ALS_1BYTE_WRITE, 2, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 0, 0);
    }
    return result;
}
#endif  /* SHDISP_BDIC_USE_ALS_I2CCMD */

#if 0
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_als_clmr_msk_write_cmd                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_bdic_als_clmr_msk_write_cmd(unsigned char reg, unsigned char val, unsigned char msk)
{
    int result = SHDISP_RESULT_SUCCESS;
    char ary[3];

    ary[0] = reg;
    ary[1] = msk;
    ary[2] = val;

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_ALS_1BYTE_MASK_WRITE, 3, ary);
    }
    else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_ALS_1BYTE_MASK_WRITE, 3, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 0, 0);
    }
    return result;
}
#endif

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_bkl_adj_clmr_write_cmd                                        */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_bkl_adj_clmr_write_cmd(int mode, unsigned char* value, int len )
{
#ifndef SHDISP_FW_STACK_EXCUTE
    if(shdisp_FWCMD_buf_get_nokick()){
        shdisp_SYS_bdic_i2c_doKick_if_exist();
        shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);
    }
#endif
    switch (mode) {
    case SHDISP_BDIC_BL_PARAM_WRITE:
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_WRITE, len, value);
        break;
    case SHDISP_BDIC_BL_MODE_SET:
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_BKLMODE_SET, len, value);
        break;
    case SHDISP_BDIC_ALS_SET:
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET, len, value);
        break;
    case SHDISP_BDIC_ALS_PARAM_SET:
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_LIGHTCTL_PARAM_REFLECT, len, value);
        break;
    default:
        break;
    }
#ifndef SHDISP_FW_STACK_EXCUTE
    if(shdisp_FWCMD_buf_get_nokick()){
        shdisp_SYS_bdic_i2c_doKick_if_exist();
        shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_BKL);
    }
#endif
    return;

}

#ifndef SHDISP_BDIC_USE_ALS_I2CCMD
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_als_clmr_read_cmd                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_bdic_als_clmr_read_cmd(unsigned char reg, unsigned char *val)
{
    int result = SHDISP_RESULT_SUCCESS;
    char ary[1];

    ary[0] = reg;

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_ALS_1BYTE_READ, 1, ary);
    }
    else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_ALS_1BYTE_READ, 1, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 1, val);
    }

    return result;
}
#endif  /* SHDISP_BDIC_USE_ALS_I2CCMD */

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_REG_OPT_set_value                                          */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_PD_REG_OPT_set_value( int table )
{
    int mode = 0;

    shdisp_bdic_LD_LCD_BKL_get_mode(&mode);

    if( table == SHDISP_BDIC_MAIN_BKL_OPT_LOW ){
#ifdef SHDISP_SW_BDIC_IRQ_LOG
        printk("[SHDISP DBG]opt low table write(mode=%d)\n", mode);
#endif
        s_state_str.bdic_main_bkl_opt_mode_output = SHDISP_BDIC_MAIN_BKL_OPT_LOW;
        shdisp_pm_set_als_sensor_param(SHDISP_BDIC_MAIN_BKL_OPT_LOW);
    }
    else if( table == SHDISP_BDIC_MAIN_BKL_OPT_HIGH ){
#ifdef SHDISP_SW_BDIC_IRQ_LOG
        printk("[SHDISP DBG]opt high table write(mode=%d)\n", mode);
#endif
        s_state_str.bdic_main_bkl_opt_mode_output = SHDISP_BDIC_MAIN_BKL_OPT_HIGH;
        shdisp_pm_set_als_sensor_param(SHDISP_BDIC_MAIN_BKL_OPT_HIGH);
    }

    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_REG_OPT_temp_set                                           */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_PD_REG_OPT_temp_set( unsigned char value )
{
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_REG_ADO_get_opt                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_bdic_PD_REG_ADO_get_opt(unsigned short *value)
{
    static unsigned short before_val = 0x0000;
    int InfoReg2;

    SHDISP_DEBUG("in, before ADO=0x%04x\n", before_val);
    InfoReg2 = shdisp_SYS_getInfoReg2();
    SHDISP_DEBUG("InfoReg2=0x%08x\n",InfoReg2);

    before_val = (InfoReg2 & 0x0000ffff);
    *value = before_val;
    SHDISP_DEBUG("out, ADO_get_opt value = 0x%04X\n", *value);
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_ps_req_mask                                                   */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_PD_ps_req_mask(unsigned char *remask)
{
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_PD_ps_req_restart                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_bdic_PD_ps_req_restart(unsigned char remask)
{
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Input/Output                                                              */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_IO_write_reg                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_IO_write_reg(unsigned char reg, unsigned char val)
{
    int ret;
    int chkadrs = (int)reg;

    if (chkadrs < BDIC_REG_MINIMUM_ADDRESS || chkadrs > BDIC_REG_MAXIMUM_ADDRESS) {
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_SYS_bdic_i2c_write(reg, val);

    switch(ret){
    case SHDISP_RESULT_SUCCESS:
        break;
    case SHDISP_RESULT_FAILURE_I2C_TMO:
        SHDISP_ERR("<TIME_OUT> shdisp_SYS_bdic_i2c_write.\n");
        break;
    default:
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_write.\n");
        ret = SHDISP_RESULT_FAILURE;
    }

    return ret;
}
#if 0
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_IO_multi_write_reg                                            */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_IO_multi_write_reg(unsigned char reg, unsigned char *wval, unsigned char size)
{
    int ret;

    ret = shdisp_SYS_bdic_i2c_multi_write(reg, wval, size);

    switch(ret){
    case SHDISP_RESULT_SUCCESS:
        break;
    case SHDISP_RESULT_FAILURE_I2C_TMO:
        SHDISP_ERR("<TIME_OUT> shdisp_SYS_bdic_i2c_write.\n");
        break;
    default:
        SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_write.\n");
        ret = SHDISP_RESULT_FAILURE;
    }

    return ret;
}
#endif
/* ------------------------------------------------------------------------- */
/* shdisp_bdic_IO_read_reg                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_IO_read_reg(unsigned char reg, unsigned char *val)
{
    int ret;
    int chkadrs = (int)reg;

    if (chkadrs < BDIC_REG_MINIMUM_ADDRESS || chkadrs > BDIC_REG_MAXIMUM_ADDRESS) {
        return SHDISP_RESULT_FAILURE;
    }

    if (val == NULL) {
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_bdic_IO_read_check_reg(reg, val);

    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_IO_read_check_reg                                             */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_IO_read_check_reg(unsigned char reg, unsigned char *val)
{
    int ret;
    int retry = 0;
    unsigned char try_1st, try_2nd;
    int chkadrs = (int)reg;

    if (chkadrs < BDIC_REG_MINIMUM_ADDRESS || chkadrs > BDIC_REG_MAXIMUM_ADDRESS) {
        return SHDISP_RESULT_FAILURE;
    }

    try_1st = 0;
    try_2nd = 0;

    for (retry = 0; retry < 3; retry++) {
        ret = shdisp_SYS_bdic_i2c_read(reg, &try_1st);

        if (ret == SHDISP_RESULT_FAILURE_I2C_TMO) {
            SHDISP_ERR("<TIME_OUT> shdisp_SYS_bdic_i2c_read.\n");
            return SHDISP_RESULT_FAILURE_I2C_TMO;
        }
        else if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_read.\n");
            return SHDISP_RESULT_FAILURE;
        }

        ret = shdisp_SYS_bdic_i2c_read(reg, &try_2nd);

        if (ret == SHDISP_RESULT_FAILURE_I2C_TMO) {
            SHDISP_ERR("<TIME_OUT> shdisp_SYS_bdic_i2c_read.\n");
            return SHDISP_RESULT_FAILURE_I2C_TMO;
        }
        else if (ret != SHDISP_RESULT_SUCCESS) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_read.\n");
            return SHDISP_RESULT_FAILURE;
        }

        if (try_1st == try_2nd) {
            *val = try_1st;
            return SHDISP_RESULT_SUCCESS;
        }
        else if (retry == 2) {
            SHDISP_ERR("<OTHER> i2c read retry over! addr:0x%02X val:(1st:0x%02X, 2nd:0x%02X).\n", reg, try_1st, try_2nd);
            *val = try_1st;
            return SHDISP_RESULT_SUCCESS;
        }
        else {
            SHDISP_ERR("<OTHER> i2c read retry (%d)! addr:0x%02X val:(1st:0x%02X, 2nd:0x%02X).\n", retry, reg, try_1st, try_2nd);
        }
    }

    SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_read.\n");
    return SHDISP_RESULT_FAILURE;
}

/* ------------------------------------------------------------------------- */
/* shdisp_bdic_IO_multi_read_reg                                             */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_IO_multi_read_reg(unsigned char reg, unsigned char *val, int size)
{
    int ret;
    int chkadrs_s = (int)reg;
    int chkadrs_e = (int)(reg + (unsigned char)(size - 1));

    if (chkadrs_s < BDIC_REG_MINIMUM_ADDRESS || chkadrs_e > BDIC_REG_MAXIMUM_ADDRESS) {
        return SHDISP_RESULT_FAILURE;
    }

    if (val == NULL) {
        SHDISP_ERR("<NULL_POINTER> val.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if ((size < 1) || (size > 8)) {
        SHDISP_ERR("<INVALID_VALUE> size(%d).\n", size);
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_SYS_bdic_i2c_multi_read(reg, val, size);

    if (ret == SHDISP_RESULT_SUCCESS) {
        return SHDISP_RESULT_SUCCESS;
    }
    else if (ret == SHDISP_RESULT_FAILURE_I2C_TMO) {
        SHDISP_ERR("<TIME_OUT> shdisp_SYS_bdic_i2c_multi_read.\n");
        return SHDISP_RESULT_FAILURE_I2C_TMO;
    }

    SHDISP_ERR("<RESULT_FAILURE> shdisp_SYS_bdic_i2c_multi_read.\n");
    return SHDISP_RESULT_FAILURE;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_IO_set_bit_reg                                                */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_IO_set_bit_reg(unsigned char reg, unsigned char val)
{
    int ret;
    int chkadrs = (int)reg;

    if (chkadrs < BDIC_REG_MINIMUM_ADDRESS || chkadrs > BDIC_REG_MAXIMUM_ADDRESS) {
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_SYS_bdic_i2c_mask_write(reg, val, val);
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_IO_clr_bit_reg                                                */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_IO_clr_bit_reg(unsigned char reg, unsigned char val)
{
    int ret;
    int chkadrs = (int)reg;

    if (chkadrs < BDIC_REG_MINIMUM_ADDRESS || chkadrs > BDIC_REG_MAXIMUM_ADDRESS) {
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_SYS_bdic_i2c_mask_write(reg, 0, val);
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_photo_sensor_change_range                                          */
/* ------------------------------------------------------------------------- */

static int shdisp_photo_sensor_change_range( int bkl_opt )
{
    int           ret;
    unsigned char val=0;

    ret = shdisp_photo_sensor_IO_read_reg(SENSOR_REG_COMMAND2, &val);
    val &= 0xF8;

    if( bkl_opt == SHDISP_BDIC_MAIN_BKL_OPT_LOW )
        val |= 0x04;
    else
        val |= 0x06;
    ret = shdisp_photo_sensor_IO_write_reg(SENSOR_REG_COMMAND2, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_photo_sensor_IO_write_reg                                          */
/* ------------------------------------------------------------------------- */

static int shdisp_photo_sensor_IO_write_reg(unsigned char reg, unsigned char val)
{
    int ret = SHDISP_RESULT_SUCCESS;

#ifdef SHDISP_BDIC_USE_ALS_I2CCMD
    ret = shdisp_SYS_psals_i2c_write(reg, val);
#else   /* SHDISP_BDIC_USE_ALS_I2CCMD */
    ret = shdisp_bdic_als_clmr_write_cmd(reg, val);
#endif  /* SHDISP_BDIC_USE_ALS_I2CCMD */
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_photo_sensor_IO_read_reg                                           */
/* ------------------------------------------------------------------------- */

static int shdisp_photo_sensor_IO_read_reg(unsigned char reg, unsigned char *val)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char rbuf;

    rbuf = 0x00;

#ifdef SHDISP_BDIC_USE_ALS_I2CCMD
    shdisp_SYS_bdic_i2c_set_api(SHDISP_CLMR_FWCMD_APINO_PHOTESENSOR);

    ret = shdisp_SYS_psals_i2c_read(reg, &rbuf);
    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_SYS_bdic_i2c_doKick_if_exist();
        if (ret == SHDISP_RESULT_SUCCESS) {
            *val = rbuf;
        }
    } else {
        shdisp_SYS_bdic_i2c_cmd_reset();
    }
#else   /* SHDISP_BDIC_USE_ALS_I2CCMD */
    ret = shdisp_bdic_als_clmr_read_cmd(reg, &rbuf);
    if (ret == SHDISP_RESULT_SUCCESS) {
        *val = rbuf;
    }
#endif  /* SHDISP_BDIC_USE_ALS_I2CCMD */
    return ret;
}


MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
