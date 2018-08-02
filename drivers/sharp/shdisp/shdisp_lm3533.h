/* drivers/sharp/shdisp/shdisp_lm3533.h  (Display Driver)
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

#ifndef SHDISP_LM3533_H
#define SHDISP_LM3533_H

#define SHDISP_USE_LEDC

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <sharp/shdisp_kerl.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#define BDIC_REG_MINIMUM_ADDRESS            (0x00000010)
#define BDIC_REG_OUTPUT_CONFIG1             (0x10)
#define BDIC_REG_OUTPUT_CONFIG2             (0x11)
#define BDIC_REG_START_SHUT_RAMP_RATES      (0x12)
#define BDIC_REG_RUN_TIME_RAMP_RATES        (0x13)
#define BDIC_REG_PWM_CONFIG_A               (0x14)
#define BDIC_REG_PWM_CONFIG_B               (0x15)
#define BDIC_REG_PWM_CONFIG_C               (0x16)
#define BDIC_REG_PWM_CONFIG_D               (0x17)
#define BDIC_REG_PWM_CONFIG_E               (0x18)
#define BDIC_REG_PWM_CONFIG_F               (0x19)
#define BDIC_REG_BRIGHT_CONFIG_AB           (0x1A)
#define BDIC_REG_BRIGHT_CONFIG_C            (0x1B)
#define BDIC_REG_BRIGHT_CONFIG_D            (0x1C)
#define BDIC_REG_BRIGHT_CONFIG_E            (0x1D)
#define BDIC_REG_BRIGHT_CONFIG_F            (0x1E)
#define BDIC_REG_FULL_SCALE_CURR_A          (0x1F)
#define BDIC_REG_FULL_SCALE_CURR_B          (0x20)
#define BDIC_REG_FULL_SCALE_CURR_C          (0x21)
#define BDIC_REG_FULL_SCALE_CURR_D          (0x22)
#define BDIC_REG_FULL_SCALE_CURR_E          (0x23)
#define BDIC_REG_FULL_SCALE_CURR_F          (0x24)
#define BDIC_REG_PUMP_CONTROL               (0x26)
#define BDIC_REG_BANK_ENABLE                (0x27)
#define BDIC_REG_ALS_SCALING_CTL            (0x28)
#define BDIC_REG_ALS_PTN_SCALER1            (0x29)
#define BDIC_REG_ALS_PTN_SCALER2            (0x2A)
#define BDIC_REG_ALS_PTN_SCALER3            (0x2B)
#define BDIC_REG_POLARITY                   (0x2C)
#define BDIC_REG_R_ALS_SELECT               (0x30)
#define BDIC_REG_ALS_CONFIG                 (0x31)
#define BDIC_REG_ALS_ALGORITHM              (0x32)
#define BDIC_REG_ALS_DELAY_CTL              (0x33)
#define BDIC_REG_READ_ALS_ZONE              (0x34)
#define BDIC_REG_READ_ALS_DELAY             (0x35)
#define BDIC_REG_READ_ALS_UPONLY            (0x36)
#define BDIC_REG_READ_ADC                   (0x37)
#define BDIC_REG_READ_AVG_ADC               (0x38)
#define BDIC_REG_BRIGHT_A                   (0x40)
#define BDIC_REG_BRIGHT_B                   (0x41)
#define BDIC_REG_BRIGHT_C                   (0x42)
#define BDIC_REG_BRIGHT_D                   (0x43)
#define BDIC_REG_BRIGHT_E                   (0x44)
#define BDIC_REG_BRIGHT_F                   (0x45)
#define BDIC_REG_ALS_BOUNDARY_0H            (0x50)
#define BDIC_REG_ALS_BOUNDARY_0L            (0x51)
#define BDIC_REG_ALS_BOUNDARY_1H            (0x52)
#define BDIC_REG_ALS_BOUNDARY_1L            (0x53)
#define BDIC_REG_ALS_BOUNDARY_2H            (0x54)
#define BDIC_REG_ALS_BOUNDARY_2L            (0x55)
#define BDIC_REG_ALS_BOUNDARY_3H            (0x56)
#define BDIC_REG_ALS_BOUNDARY_3L            (0x57)
#define BDIC_REG_ALS_M1_ZONE_0              (0x60)
#define BDIC_REG_ALS_M1_ZONE_1              (0x61)
#define BDIC_REG_ALS_M1_ZONE_2              (0x62)
#define BDIC_REG_ALS_M1_ZONE_3              (0x63)
#define BDIC_REG_ALS_M1_ZONE_4              (0x64)
#define BDIC_REG_ALS_M2_ZONE_0              (0x65)
#define BDIC_REG_ALS_M2_ZONE_1              (0x66)
#define BDIC_REG_ALS_M2_ZONE_2              (0x67)
#define BDIC_REG_ALS_M2_ZONE_3              (0x68)
#define BDIC_REG_ALS_M2_ZONE_4              (0x69)
#define BDIC_REG_ALS_M3_ZONE_0              (0x6A)
#define BDIC_REG_ALS_M3_ZONE_1              (0x6B)
#define BDIC_REG_ALS_M3_ZONE_2              (0x6C)
#define BDIC_REG_ALS_M3_ZONE_3              (0x6D)
#define BDIC_REG_ALS_M3_ZONE_4              (0x6E)
#define BDIC_REG_PTNGEN1_DELAY              (0x70)
#define BDIC_REG_PTNGEN1_LOWTIME            (0x71)
#define BDIC_REG_PTNGEN1_HIGHTIME           (0x72)
#define BDIC_REG_PTNGEN1_LOWBRIGHT          (0x73)
#define BDIC_REG_PTNGEN1_RISETIME           (0x74)
#define BDIC_REG_PTNGEN1_FALLTIME           (0x75)
#define BDIC_REG_PTNGEN2_DELAY              (0x80)
#define BDIC_REG_PTNGEN2_LOWTIME            (0x81)
#define BDIC_REG_PTNGEN2_HIGHTIME           (0x82)
#define BDIC_REG_PTNGEN2_LOWBRIGHT          (0x83)
#define BDIC_REG_PTNGEN2_RISETIME           (0x84)
#define BDIC_REG_PTNGEN2_FALLTIME           (0x85)
#define BDIC_REG_PTNGEN3_DELAY              (0x90)
#define BDIC_REG_PTNGEN3_LOWTIME            (0x91)
#define BDIC_REG_PTNGEN3_HIGHTIME           (0x92)
#define BDIC_REG_PTNGEN3_LOWBRIGHT          (0x93)
#define BDIC_REG_PTNGEN3_RISETIME           (0x94)
#define BDIC_REG_PTNGEN3_FALLTIME           (0x95)
#define BDIC_REG_PTNGEN4_DELAY              (0xA0)
#define BDIC_REG_PTNGEN4_LOWTIME            (0xA1)
#define BDIC_REG_PTNGEN4_HIGHTIME           (0xA2)
#define BDIC_REG_PTNGEN4_LOWBRIGHT          (0xA3)
#define BDIC_REG_PTNGEN4_RISETIME           (0xA4)
#define BDIC_REG_PTNGEN4_FALLTIME           (0xA5)
#define BDIC_REG_LED_OPEN_FAULT_READ        (0xB0)
#define BDIC_REG_LED_SHORT_FAULT_READ       (0xB1)
#define BDIC_REG_LED_FAULT_ENABLE           (0xB2)
#define BDIC_REG_MAXIMUM_ADDRESS            (0x000000B2)

#define SENSOR_REG_COMMAND1                 (0x00)
#define SENSOR_REG_COMMAND2                 (0x01)
#define SENSOR_REG_COMMAND3                 (0x02)
#define SENSOR_REG_COMMAND4                 (0x03)
#define SENSOR_REG_INT_LT_LSB               (0x04)
#define SENSOR_REG_INT_LT_MSB               (0x05)
#define SENSOR_REG_INT_HT_LSB               (0x06)
#define SENSOR_REG_INT_HT_MSB               (0x07)
#define SENSOR_REG_PS_LT_LSB                (0x08)
#define SENSOR_REG_PS_LT_MSB                (0x09)
#define SENSOR_REG_PS_HT_LSB                (0x0A)
#define SENSOR_REG_PS_HT_MSB                (0x0B)
#define SENSOR_REG_D0_LSB                   (0x0C)
#define SENSOR_REG_D0_MSB                   (0x0D)
#define SENSOR_REG_D1_LSB                   (0x0E)
#define SENSOR_REG_D1_MSB                   (0x0F)
#define SENSOR_REG_D2_LSB                   (0x10)
#define SENSOR_REG_D2_MSB                   (0x11)


#define SHDISP_BDIC_I2C_SLAVE_ADDR          (0x36<<1)
#define SHDISP_BDIC_I2C_WBUF_MAX            (6)
#define SHDISP_BDIC_I2C_RBUF_MAX            (6)

#define SHDISP_BDIC_INT_GFAC_GFAC0          (0x00000001)
#define SHDISP_BDIC_INT_GFAC_GFAC1          (0x00000002)
#define SHDISP_BDIC_INT_GFAC_GFAC2          (0x00000004)
#define SHDISP_BDIC_INT_GFAC_PS             (0x00000008)
#define SHDISP_BDIC_INT_GFAC_GFAC4          (0x00000010)
#define SHDISP_BDIC_INT_GFAC_ALS            (0x00000100)
#define SHDISP_BDIC_INT_GFAC_PS2            (0x00000200)
#define SHDISP_BDIC_INT_GFAC_OPTON          (0x00000400)
#define SHDISP_BDIC_INT_GFAC_CPON           (0x00000800)
#define SHDISP_BDIC_INT_GFAC_ANIME          (0x00001000)
#define SHDISP_BDIC_INT_GFAC_TEST1          (0x00002000)
#define SHDISP_BDIC_INT_GFAC_DCDC2_ERR      (0x00004000)
#define SHDISP_BDIC_INT_GFAC_TSD            (0x00008000)
#define SHDISP_BDIC_INT_GFAC_TEST2          (0x00010000)
#define SHDISP_BDIC_INT_GFAC_TEST3          (0x00020000)
#define SHDISP_BDIC_INT_GFAC_DET            (0x00040000)
#define SHDISP_BDIC_INT_GFAC_I2C_ERR        (0x00080000)
#define SHDISP_BDIC_INT_GFAC_TEST4          (0x00100000)
#define SHDISP_BDIC_INT_GFAC_OPTSEL         (0x00200000)
#define SHDISP_BDIC_INT_GFAC_TEST5          (0x00400000)
#define SHDISP_BDIC_INT_GFAC_TEST6          (0x00800000)

#define SHDISP_BKL_TBL_MODE_NORMAL          (0)
#define SHDISP_BKL_TBL_MODE_ECO             (1)
#define SHDISP_BKL_TBL_MODE_EMERGENCY       (2)
#define SHDISP_BKL_TBL_MODE_CHARGE          (3)

#define SHDISP_BDIC_SENSOR_TYPE_PHOTO       (1)
#define SHDISP_BDIC_SENSOR_TYPE_PROX        (2)
#define SHDISP_BDIC_SENSOR_SLAVE_ADDR       (0x39<<1)

#define SHDISP_OPT_CHANGE_WAIT_TIME         (150)

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

enum {
    SHDISP_BDIC_REQ_NONE = 0,
    SHDISP_BDIC_REQ_ACTIVE,
    SHDISP_BDIC_REQ_STANDBY,
    SHDISP_BDIC_REQ_STOP,
    SHDISP_BDIC_REQ_START,
    SHDISP_BDIC_REQ_BKL_SET_MODE_OFF,
    SHDISP_BDIC_REQ_BKL_SET_MODE_FIX,
    SHDISP_BDIC_REQ_BKL_SET_MODE_AUTO,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_NORMAL,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_BLINK,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_FIREFLY,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_HISPEED,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_STANDARD,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_BREATH,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_LONG_BREATH,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_WAVE,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_FLASH,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_AURORA,
    SHDISP_BDIC_REQ_TRI_LED_SET_MODE_RAINBOW,
    SHDISP_BDIC_REQ_TRI_LED_SET_ONTIME,
    SHDISP_BDIC_REQ_TRI_LED_SET_INTERVAL,
    SHDISP_BDIC_REQ_TRI_LED_SET_COUNT,
    SHDISP_BDIC_REQ_PHOTO_SENSOR_CONFIG,
    SHDISP_BDIC_REQ_BKL_DTV_OFF,
    SHDISP_BDIC_REQ_BKL_DTV_ON,
    SHDISP_BDIC_REQ_BKL_EMG_OFF,
    SHDISP_BDIC_REQ_BKL_EMG_ON,
    SHDISP_BDIC_REQ_BKL_ECO_OFF,
    SHDISP_BDIC_REQ_BKL_ECO_ON,
    SHDISP_BDIC_REQ_BKL_CHG_OFF,
    SHDISP_BDIC_REQ_BKL_CHG_ON,
    SHDISP_BDIC_REQ_PRE_START,
    SHDISP_BDIC_REQ_POST_START,
    SHDISP_BDIC_REQ_POST_START_FIX
};

enum {
    SHDISP_BDIC_PWR_STATUS_OFF,
    SHDISP_BDIC_PWR_STATUS_STANDBY,
    SHDISP_BDIC_PWR_STATUS_ACTIVE,
    NUM_SHDISP_BDIC_PWR_STATUS
};

enum {
    SHDISP_BDIC_DEV_TYPE_LCD_BKL,
    SHDISP_BDIC_DEV_TYPE_LCD_PWR,
    SHDISP_BDIC_DEV_TYPE_TRI_LED,
    SHDISP_BDIC_DEV_TYPE_TRI_LED_ANIME,
    SHDISP_BDIC_DEV_TYPE_PHOTO_SENSOR_APP,
    SHDISP_BDIC_DEV_TYPE_PHOTO_SENSOR_LUX,
    SHDISP_BDIC_DEV_TYPE_PHOTO_SENSOR_BKL,
    SHDISP_BDIC_DEV_TYPE_PROX_SENSOR,
    NUM_SHDISP_BDIC_DEV_TYPE
};

enum {
    SHDISP_BDIC_DEV_PWR_OFF,
    SHDISP_BDIC_DEV_PWR_ON,
    NUM_SHDISP_BDIC_DEV_PWR
};

enum {
    SHDISP_MAIN_BKL_DEV_TYPE_APP,
    SHDISP_MAIN_BKL_DEV_TYPE_APP_AUTO,
    NUM_SHDISP_MAIN_BKL_DEV_TYPE
};

enum {
    SHDISP_SENSOR_STATE_PROX_OFF_ALC_OFF,
    SHDISP_SENSOR_STATE_PROX_ON_ALC_OFF,
    SHDISP_SENSOR_STATE_PROX_OFF_ALC_ON,
    SHDISP_SENSOR_STATE_PROX_ON_ALC_ON,
    NUM_SHDISP_SENSOR_STATE
};

enum {
    SHDISP_BDIC_IRQ_TYPE_NONE,
    SHDISP_BDIC_IRQ_TYPE_ALS,
    SHDISP_BDIC_IRQ_TYPE_PS,
    SHDISP_BDIC_IRQ_TYPE_DET,
    SHDISP_BDIC_IRQ_TYPE_I2C_ERR,
    NUM_SHDISP_BDIC_IRQ_TYPE
};

enum {
    SHDISP_IRQ_MASK,
    SHDISP_IRQ_NO_MASK,
    NUM_SHDISP_IRQ_SWITCH
};

enum {
    SHDISP_MAIN_BKL_ADJ_RETRY0,
    SHDISP_MAIN_BKL_ADJ_RETRY1,
    SHDISP_MAIN_BKL_ADJ_RETRY2,
    SHDISP_MAIN_BKL_ADJ_RETRY3,
    NUM_SHDISP_MAIN_BKL_ADJ
};

enum {
    SHDISP_BDIC_MAIN_BKL_OPT_LOW,
    SHDISP_BDIC_MAIN_BKL_OPT_HIGH,
    NUM_SHDISP_BDIC_MAIN_BKL_OPT_MODE
};

enum {
    SHDISP_BDIC_PHOTO_LUX_TIMER_ON,
    SHDISP_BDIC_PHOTO_LUX_TIMER_OFF,
    NUM_SHDISP_BDIC_PHOTO_LUX_TIMER_SWITCH
};

enum {
    SHDISP_BDIC_LUX_JUDGE_IN,
    SHDISP_BDIC_LUX_JUDGE_IN_CONTI,
    SHDISP_BDIC_LUX_JUDGE_OUT,
    SHDISP_BDIC_LUX_JUDGE_OUT_CONTI,
    SHDISP_BDIC_LUX_JUDGE_ERROR,
    NUM_SHDISP_BDIC_LUX_JUDGE
};

enum {
    SHDISP_BDIC_BL_PARAM_WRITE = 0,
    SHDISP_BDIC_BL_PARAM_READ,
    SHDISP_BDIC_BL_MODE_SET,
    SHDISP_BDIC_ALS_SET,
    SHDISP_BDIC_ALS_PARAM_WRITE,
    SHDISP_BDIC_ALS_PARAM_READ,
    SHDISP_BDIC_ALS_PARAM_SET,
    SHDISP_BDIC_CABC_CTL,
    SHDISP_BDIC_CABC_CTL_TIME_SET
};

enum {
    SHDISP_BDIC_BL_PWM_FIX_PARAM = 0,
    SHDISP_BDIC_BL_PWM_AUTO_PARAM
};

struct shdisp_bdic_state_str{
    int bdic_is_exist;
    int bdic_main_bkl_opt_mode_output;
    int bdic_main_bkl_opt_mode_ado;
    unsigned char shdisp_lux_change_level1;
    unsigned char shdisp_lux_change_level2;
    struct shdisp_als_adjust als_adjust[2];
    int bdic_clrvari_index;
    int clmr_is_exist;
    struct shdisp_prox_params   prox_params;
};

struct shdisp_bdic_bkl_lux_str{
    unsigned int   ext_flag;
    unsigned short ado_range;
    unsigned long  lux;
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
int  shdisp_bdic_API_boot_init( void );
void shdisp_bdic_API_initialize(struct shdisp_bdic_state_str* state_str);
void shdisp_bdic_API_bdic_exist(int* bdic_is_exist);
void shdisp_bdic_API_LCD_release_hw_reset(void);
void shdisp_bdic_API_LCD_set_hw_reset(void);
void shdisp_bdic_API_LCD_power_on(void);
void shdisp_bdic_API_LCD_power_off(void);
void shdisp_bdic_API_LCD_m_power_on(void);
void shdisp_bdic_API_LCD_m_power_off(void);
void shdisp_bdic_API_LCD_vo2_on(void);
void shdisp_bdic_API_LCD_vo2_off(void);
void shdisp_bdic_API_LCD_vdd_on(void);
void shdisp_bdic_API_LCD_vdd_off(void);
void shdisp_bdic_API_LCD_BKL_off(void);
void shdisp_bdic_API_LCD_BKL_fix_on(int param);
void shdisp_bdic_API_LCD_BKL_auto_on(int param);
void shdisp_bdic_API_LCD_BKL_get_param(unsigned long int* param);
void shdisp_bdic_API_LCD_BKL_set_request(int type, struct shdisp_main_bkl_ctl *tmp);
void shdisp_bdic_API_TRI_LED_set_request(struct shdisp_tri_led *tmp);
void shdisp_bdic_API_LCD_BKL_get_request(int type, struct shdisp_main_bkl_ctl *tmp, struct shdisp_main_bkl_ctl *req);
void shdisp_bdic_API_LCD_BKL_dtv_on(void);
void shdisp_bdic_API_LCD_BKL_dtv_off(void);
void shdisp_bdic_API_LCD_BKL_emg_on(void);
void shdisp_bdic_API_LCD_BKL_emg_off(void);
void shdisp_bdic_API_LCD_BKL_eco_on(void);
void shdisp_bdic_API_LCD_BKL_eco_off(void);
void shdisp_bdic_API_LCD_BKL_chg_on(void);
void shdisp_bdic_API_LCD_BKL_chg_off(void);

int  shdisp_bdic_API_TRI_LED_off(void);
int  shdisp_bdic_API_TRI_LED_normal_on(unsigned char color);
void shdisp_bdic_API_TRI_LED_blink_on(unsigned char color, int ontime, int interval, int count);
void shdisp_bdic_API_TRI_LED_firefly_on(unsigned char color, int ontime, int interval, int count);
void shdisp_bdic_API_TRI_LED_high_speed_on(unsigned char color, int interval, int count);
void shdisp_bdic_API_TRI_LED_standard_on(unsigned char color, int interval, int count);
void shdisp_bdic_API_TRI_LED_breath_on(unsigned char color, int interval, int count);
void shdisp_bdic_API_TRI_LED_long_breath_on(unsigned char color, int interval, int count);
void shdisp_bdic_API_TRI_LED_wave_on(unsigned char color, int interval, int count);
void shdisp_bdic_API_TRI_LED_flash_on(unsigned char color, int interval, int count);
void shdisp_bdic_API_TRI_LED_aurora_on(int interval, int count);
void shdisp_bdic_API_TRI_LED_rainbow_on(int interval, int count);
void shdisp_bdic_API_TRI_LED_gradation_on(unsigned char color, int ontime, int interval);
int  shdisp_bdic_API_TRI_LED_get_clrvari_index( int clrvari );
int  shdisp_bdic_API_PHOTO_SENSOR_get_val(unsigned short *value);
int  shdisp_bdic_API_PHOTO_SENSOR_get_lux(unsigned short *value, unsigned long *lux);
int  shdisp_bdic_API_PHOTO_SENSOR_lux_change_ind(int *mode);
int  shdisp_bdic_API_PHOTO_SENSOR_lux_judge(void);
int  shdisp_bdic_API_change_level_lut(void);
int shdisp_bdic_API_i2c_transfer(struct shdisp_bdic_i2c_msg *msg);
int  shdisp_bdic_API_DIAG_write_reg(unsigned char reg, unsigned char val);
int  shdisp_bdic_API_DIAG_read_reg(unsigned char reg, unsigned char *val);
int  shdisp_bdic_API_DIAG_multi_read_reg(unsigned char reg, unsigned char *val, int size);
int  shdisp_bdic_API_RECOVERY_check_restoration(void);
int  shdisp_bdic_API_RECOVERY_check_bdic_practical(void);
#if defined (CONFIG_ANDROID_ENGINEERING)
void shdisp_bdic_API_DBG_INFO_output(void);
void shdisp_psals_API_DBG_INFO_output(void);
#endif /* CONFIG_ANDROID_ENGINEERING */


void shdisp_bdic_API_IRQ_set_reg( int irq_type, int onoff );
int  shdisp_bdic_API_IRQ_check_type( int irq_type );
void shdisp_bdic_API_IRQ_save_fac(void);
int  shdisp_bdic_API_IRQ_check_fac(void);
int  shdisp_bdic_API_IRQ_get_fac( int iQueFac );
void shdisp_bdic_API_IRQ_Clear(void);
void shdisp_bdic_API_IRQ_i2c_error_Clear(void);
void shdisp_bdic_API_IRQ_det_fac_Clear(void);
int  shdisp_bdic_API_IRQ_check_DET(void);
void shdisp_bdic_API_IRQ_backup_irq_reg(void);
void shdisp_bdic_API_IRQ_return_irq_reg(void);
void shdisp_bdic_API_IRQ_irq_reg_i2c_mask(void);
void shdisp_bdic_API_IRQ_det_irq_mask(void);
void shdisp_bdic_API_IRQ_set_det_flag(void);
void shdisp_bdic_API_IRQ_clr_det_flag(void);
int  shdisp_bdic_API_IRQ_get_det_flag(void);
void shdisp_bdic_API_IRQ_dbg_Clear_All(void);
void shdisp_bdic_API_IRQ_dbg_set_fac(unsigned int nGFAC);

void shdisp_bdic_API_IRQ_dbg_photo_param( int level1, int level2);

int  shdisp_bdic_API_PHOTO_lux_timer( int onoff );
int  shdisp_bdic_API_SENSOR_start(int new_opt_mode);
int shdisp_bdic_api_prox_sensor_pow_ctl(int power_mode);
int shdisp_bdic_api_als_sensor_pow_ctl(int dev_type, int power_mode);
int  shdisp_bdic_PD_set_active(int power_status);
int  shdisp_bdic_PD_set_standby(void);
int  shdisp_bdic_PD_psals_power_on(void);
int  shdisp_bdic_PD_psals_power_off(void);
int  shdisp_bdic_PD_psals_ps_init(int *state);
int  shdisp_bdic_PD_psals_ps_deinit(int *state);
int  shdisp_bdic_PD_psals_als_init(int *state);
int  shdisp_bdic_PD_psals_als_deinit(int *state);
void shdisp_bdic_API_IRQ_det_irq_ctrl(int ctrl);
void shdisp_bdic_api_set_default_sensor_param(struct shdisp_photo_sensor_adj *tmp_adj);
void shdisp_bdic_api_set_prox_sensor_param( struct shdisp_prox_params *prox_params);
unsigned short shdisp_bdic_api_get_LC_MLED01(void);
void shdisp_bdic_API_RECOVERY_lux_data_backup(void);
void shdisp_bdic_API_RECOVERY_lux_data_restore(void);
#endif /* SHDISP_LM3533_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
