/* drivers/sharp/shdisp/shdisp_system.h  (Display Driver)
 *
 * Copyright (C) 2011-2012 SHARP CORPORATION
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

#ifndef SHDISP_SYSTEM_H
#define SHDISP_SYSTEM_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

#include <sharp/shdisp_kerl.h>

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */
//#define SHDISP_SYS_SW_TIME_API
//#define SHDISP_SYS_SW_TIME_BDIC

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#define SHDISP_GPIO_CTL_LOW         0
#define SHDISP_GPIO_CTL_HIGH        1

#define SHDISP_BOOT_SW_ENABLE_DISPLAY       (1)
#define SHDISP_BOOT_SW_ENABLE_LED           (1)


#include <linux/mfd/pm8xxx/pm8921.h>
#include <../../../arch/arm/mach-msm/board-8064.h>

//#define SHDISP_GPIO_NUM_BL_RST_NEW  86
//#define SHDISP_GPIO_NUM_BL_RST_OLD  54
#define SHDISP_GPIO_NUM_BL_RST_N        (86)
#define SHDISP_GPIO_NUM_BL_I2C_SDA      (12)
#define SHDISP_GPIO_NUM_BL_I2C_SCL      (13)
#define SHDISP_GPIO_NUM_SPI_MOSI        (14)
#define SHDISP_GPIO_NUM_SPI_MISO        (15)
#define SHDISP_GPIO_NUM_SPI_SCS         (16)
#define SHDISP_GPIO_NUM_SPI_SCLK        (17)
#define SHDISP_GPIO_NUM_ALS_VCC         (22)
#define SHDISP_GPIO_NUM_LCD_SCS1        (31)
#define SHDISP_GPIO_NUM_LCD_SCS2        (32)
#define SHDISP_GPIO_NUM_LCD_CLK         (54)
#define SHDISP_GPIO_NUM_LCD_RESET       (59)
#define SHDISP_GPIO_NUM_LCD_VDD         (60)
#define SHDISP_GPIO_NUM_CLK_SEL         (107)

#ifdef SHDISP_NOT_SUPPORT_DL32_BOARD
#define SHDISP_GPIO_NUM_CLMR_RESET      (69)
#define SHDISP_GPIO_NUM_CLMR_PLLONCTL   (70)
#define SHDISP_GPIO_NUM_CLMR_TE         (71)
#define SHDISP_GPIO_NUM_CLMR_HINT       (72)
#else
#define SHDISP_GPIO_NUM_CLMR_RESET      (PM8921_GPIO_PM_TO_SYS(14))
#define SHDISP_GPIO_NUM_CLMR_PLLONCTL   (PM8921_GPIO_PM_TO_SYS(15))
#define SHDISP_GPIO_NUM_CLMR_TE         (0)
#define SHDISP_GPIO_NUM_CLMR_HINT       (58)
#endif


#if defined(CONFIG_MACH_LYNX_DL20)
#define SHDISP_HW_REV_ES0       0x00            /* ES0   */
#define SHDISP_HW_REV_ES05      0x00            /* ES0.5 */
#define SHDISP_HW_REV_ES1       0x02            /* ES1   */
#define SHDISP_HW_REV_ES15      0x02            /* ES1.5 */
#define SHDISP_HW_REV_PP1       0x03            /* PP1   */
#define SHDISP_HW_REV_PP15      0x03            /* PP1.5 */
#define SHDISP_HW_REV_PP2       0x04            /* PP2   */
#define SHDISP_HW_REV_PP25      0x05            /* PP2.5 */
#define SHDISP_HW_REV_MP        0x07            /* MP    */
#elif defined(CONFIG_MACH_LYNX_DL22)
#define SHDISP_HW_REV_ES0       0x00            /* ES0   */
#define SHDISP_HW_REV_ES05      0x00            /* ES0.5 */
#define SHDISP_HW_REV_ES1       0x02            /* ES1   */
#define SHDISP_HW_REV_ES15      0x02            /* ES1.5 */
#define SHDISP_HW_REV_PP1       0x03            /* PP1   */
#define SHDISP_HW_REV_PP15      0x03            /* PP1.5 */
#define SHDISP_HW_REV_PP2       0x04            /* PP2   */
#define SHDISP_HW_REV_PP25      0x04            /* PP2.5 */
#define SHDISP_HW_REV_MP        0x07            /* MP    */
#elif defined(CONFIG_MACH_KUR)
#define SHDISP_HW_REV_ES0       0x00            /* ES0   */
#define SHDISP_HW_REV_ES05      0x00            /* ES0.5 */
#define SHDISP_HW_REV_ES1       0x02            /* ES1   */
#define SHDISP_HW_REV_ES15      0x02            /* ES1.5 */
#define SHDISP_HW_REV_PP1       0x03            /* PP1   */
#define SHDISP_HW_REV_PP15      0x03            /* PP1.5 */
#define SHDISP_HW_REV_PP2       0x04            /* PP2   */
#define SHDISP_HW_REV_PP25      0x04            /* PP2.5 */
#define SHDISP_HW_REV_MP        0x07            /* MP    */
#else
#define SHDISP_HW_REV_ES0       0x00            /* ES0   */
#define SHDISP_HW_REV_ES05      0x00            /* ES0.5 */
#define SHDISP_HW_REV_ES1       0x02            /* ES1   */
#define SHDISP_HW_REV_ES15      0x02            /* ES1.5 */
#define SHDISP_HW_REV_PP1       0x03            /* PP1   */
#define SHDISP_HW_REV_PP15      0x03            /* PP1.5 */
#define SHDISP_HW_REV_PP2       0x04            /* PP2   */
#define SHDISP_HW_REV_PP25      0x04            /* PP2.5 */
#define SHDISP_HW_REV_MP        0x07            /* MP    */
#endif /* CONFIG_MACH_*** */

#define SHDISP_BDIC_I2C_DEVNAME     ("bdic_i2c")

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
#define SHDISP_IRQ_MAX_KIND         4


enum {
    SHDISP_HOST_CTL_CMD_LCD_CLK_START,
    SHDISP_HOST_CTL_CMD_LCD_CLK_STOP,
    SHDISP_HOST_CTL_CMD_LCD_CLK_INIT,
    SHDISP_HOST_CTL_CMD_LCD_CLK_EXIT,
    NUM_SHDISP_HOST_CTL_CMD
};

enum {
    SHDISP_IRQ_DISABLE,
    SHDISP_IRQ_ENABLE,
    NUM_SHDISP_IRQ_CMD
};


/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

#if defined(SHDISP_USE_EXT_CLOCK)
int  shdisp_SYS_Host_control(int cmd, unsigned long rate);
#endif
void shdisp_SYS_delay_us(unsigned long usec);
void shdisp_SYS_cmd_delay_us(unsigned long usec);
void shdisp_SYS_Host_gpio_init(void);
void shdisp_SYS_Host_gpio_exit(void);
int  shdisp_SYS_Host_gpio_request(int num);
int  shdisp_SYS_Host_gpio_free(int num);
int  shdisp_SYS_set_Host_gpio(int num, int value);

int  shdisp_SYS_request_irq(irqreturn_t (*irq_handler)( int , void * ) );
void shdisp_SYS_free_irq(void);
void shdisp_SYS_set_irq_init(void);
int  shdisp_SYS_set_irq( int enable );
void shdisp_sys_dbg_hw_check_start(void);
void shdisp_sys_dbg_hw_check_end(const char *func);

int  shdisp_SYS_bdic_i2c_init(void);
int  shdisp_SYS_bdic_i2c_exit(void);
int  shdisp_SYS_bdic_i2c_write(unsigned char addr, unsigned char data);
int  shdisp_SYS_bdic_i2c_mask_write(unsigned char addr, unsigned char data, unsigned char msk);
int  shdisp_SYS_bdic_i2c_multi_write(unsigned char addr, unsigned char *wval, unsigned char size);
int  shdisp_SYS_bdic_i2c_read(unsigned char addr, unsigned char *data);
int  shdisp_SYS_bdic_i2c_multi_read(unsigned char addr, unsigned char *data, int size);

#ifdef  SHDISP_USE_LEDC
int  shdisp_SYS_psals_i2c_write(unsigned char addr, unsigned char data);
int  shdisp_SYS_psals_i2c_read(unsigned char addr, unsigned char *data);
#endif  /* SHDISP_USE_LEDC */
void shdisp_SYS_semaphore_init(void);
int  shdisp_SYS_clmr_sio_init(void);
int  shdisp_SYS_clmr_sio_exit(void);
int  shdisp_SYS_clmr_sio_transfer(unsigned short reg, unsigned char *wbuf, int wlen, unsigned char *rbuf, int rlen);
int  shdisp_SYS_clmr_sio_itransfer(unsigned short reg, unsigned char *wbuf, int wlen, unsigned char *rbuf, int rlen);
int  shdisp_SYS_clmr_sio_eDram_transfer(int mode, unsigned char *buf, int len);


int shdisp_SYS_variable_length_data_read(unsigned char cmdno, unsigned char* data, int size);

#define SHDISP_CLMR_FWCMD_HOST_1WORD_WRITE            (0x00)
#define SHDISP_CLMR_FWCMD_HOST_1WORD_MASK_WRITE       (0x01)
#define SHDISP_CLMR_FWCMD_HOST_MULTI_WRITE            (0x02)
#define SHDISP_CLMR_FWCMD_HOST_EWB_LUT_WRITE          (0x08)
#define SHDISP_CLMR_FWCMD_HOST_CPF1_LUT_WRITE         (0x09)
#define SHDISP_CLMR_FWCMD_HOST_CPF2_LUT_WRITE         (0x0A)
#define SHDISP_CLMR_FWCMD_HOST_TRV_LUT_WRITE          (0x0B)
#define SHDISP_CLMR_FWCMD_HOST_TRV_MIF_WRITE          (0x0C)
#define SHDISP_CLMR_FWCMD_HOST_DSI_TX_FREQ_SET        (0x0E)
#define SHDISP_CLMR_FWCMD_HOST_SET_DEVCODE            (0x0F)
//#define SHDISP_CLMR_FWCMD_HOST_FINISH                 (0xFE0F)
//#define SHDISP_CLMR_FWCMD_HOST_DUMMY                  (0xFF)

#define SHDISP_CLMR_FWCMD_LCD_VSYNC_FPORCH_WAIT       (0x20)
#define SHDISP_CLMR_FWCMD_LCD_VSYNC_ASSERT_WAIT       (0x21)
#define SHDISP_CLMR_FWCMD_LCD_VSYNC_BPORCH_WAIT       (0x22)
#define SHDISP_CLMR_FWCMD_LCD_CUR_LINE_TIMING         (0x24)
#define SHDISP_CLMR_FWCMD_LCD_VALTRAN                 (0x2F)

#define SHDISP_CLMR_FWCMD_TIMER                       (0x30)

#define SHDISP_CLMR_FWCMD_DSI_LP_SWRITE               (0x40)
#define SHDISP_CLMR_FWCMD_DSI_HS_SWRITE               (0x41)
#define SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA           (0x42)
#define SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA           (0x43)
#define SHDISP_CLMR_FWCMD_DSI_LP_LWRITE               (0x44)
#define SHDISP_CLMR_FWCMD_DSI_HS_LWRITE               (0x45)
#define SHDISP_CLMR_FWCMD_DSI_LP_LWRITE_BTA           (0x46)
#define SHDISP_CLMR_FWCMD_DSI_HS_LWRITE_BTA           (0x47)
#define SHDISP_CLMR_FWCMD_DSI_BTA                     (0x48)
#define SHDISP_CLMR_FWCMD_DSI_DSI_TX0                 (0x0100)
#define SHDISP_CLMR_FWCMD_DSI_DSI_TX1                 (0x0200)
#define SHDISP_CLMR_FWCMD_DSI_DSI_TXC                 (0x0300)
#define SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_A             (0xA0)
#define SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_B             (0xB0)
#define SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_C             (0xC0)
#define SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_A             (0xA1)
#define SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_B             (0xB1)
#define SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_C             (0xC1)

#define SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA_A         (0xA2)
#define SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_BTA_B         (0xB2)
#define SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA_A         (0xA3)
#define SHDISP_CLMR_FWCMD_DSI_HS_SWRITE_BTA_B         (0xB3)
#define SHDISP_CLMR_FWCMD_DSI_LP_LWRITE_A             (0xA4)
#define SHDISP_CLMR_FWCMD_DSI_LP_LWRITE_B             (0xB4)
#define SHDISP_CLMR_FWCMD_DSI_LP_LWRITE_C             (0xC4)
#define SHDISP_CLMR_FWCMD_DSI_HS_LWRITE_A             (0xA5)
#define SHDISP_CLMR_FWCMD_DSI_HS_LWRITE_B             (0xB5)
#define SHDISP_CLMR_FWCMD_DSI_HS_LWRITE_C             (0xC5)
#define SHDISP_CLMR_FWCMD_DSI_LP_LWRITE_BTA_A         (0xA6)
#define SHDISP_CLMR_FWCMD_DSI_LP_LWRITE_BTA_B         (0xB6)
#define SHDISP_CLMR_FWCMD_DSI_HS_LWRITE_BTA_A         (0xA7)
#define SHDISP_CLMR_FWCMD_DSI_HS_LWRITE_BTA_B         (0xB7)
#define SHDISP_CLMR_FWCMD_DSI_BTA_A                   (0xA8)
#define SHDISP_CLMR_FWCMD_DSI_BTA_B                   (0xB8)

/* #define SHDISP_CLMR_FWCMD_DSI_XX_READ_xxxxx */

#define SHDISP_CLMR_FWCMD_DSI_HS_MULTI_SWRITE         (0x49)

#define SHDISP_CLMR_FWCMD_I2C_1BYTE_WRITE             (0x50)
#define SHDISP_CLMR_FWCMD_I2C_1BYTE_MASK_WRITE        (0x51)
#define SHDISP_CLMR_FWCMD_I2C_MULTI_WRITE             (0x52)
#define SHDISP_CLMR_FWCMD_I2C_1BYTE_READ              (0x54)

#define SHDISP_CLMR_FWCMD_LIGHTCTL_WRITE              (0x60)
#define SHDISP_CLMR_FWCMD_LIGHTCTL_READ               (0x62)
#define SHDISP_CLMR_FWCMD_LIGHTCTL_BKLMODE_SET        (0x63)
#define SHDISP_CLMR_FWCMD_LIGHTCTL_LUXMODE_SET        (0x64)
#define SHDISP_CLMR_FWCMD_LIGHTCTL_LUXPARAM_WRITE     (0x65)
#define SHDISP_CLMR_FWCMD_LIGHTCTL_LUXPARAM_READ      (0x66)
//#define SHDISP_CLMR_FWCMD_LIGHTCTL_ACCOMMODATION      (0x67)
#define SHDISP_CLMR_FWCMD_AE_MODE                     (0x67)
#define SHDISP_CLMR_FWCMD_LIGHTCTL_PARAM_REFLECT      (0x68)
#define SHDISP_CLMR_FWCMD_AE_TIME_SET                 (0x69)

#define SHDISP_CLMR_FWCMD_ALS_1BYTE_WRITE             (0x70)
#define SHDISP_CLMR_FWCMD_ALS_1BYTE_MASK_WRITE        (0x71)
#define SHDISP_CLMR_FWCMD_ALS_1BYTE_READ              (0x72)

#define SHDISP_CLMR_FWCMD_SMITE_SET_MODE              (0x80)
#define SHDISP_CLMR_FWCMD_SMITE_WRITE_CONFIG          (0x81)
#define SHDISP_CLMR_FWCMD_SMITE_READ_STATE            (0x82)
#define SHDISP_CLMR_FWCMD_SMITE_COMMIT                (0x84)

#define SHDISP_CLMR_FWCMD_VSPREGON                    (0x92)
#define SHDISP_CLMR_FWCMD_TRV_SBL_CPF_ONOFF           (0x93)
#define SHDISP_CLMR_FWCMD_LUT_ON                      (0x94)

#define SHDISP_CLMR_FWCMD_APINO_NOTHING              (0x00)
#define SHDISP_CLMR_FWCMD_APINO_LCD                  (0x10)
#define SHDISP_CLMR_FWCMD_APINO_MIPICMD              (0x20)
#define SHDISP_CLMR_FWCMD_APINO_BKL                  (0x30)
#define SHDISP_CLMR_FWCMD_APINO_PHOTESENSOR          (0x40)
#define SHDISP_CLMR_FWCMD_APINO_PROXSENSOR           (0x50)
#define SHDISP_CLMR_FWCMD_APINO_LED                  (0x60)
#define SHDISP_CLMR_FWCMD_APINO_OTHER                (0x70)
#define SHDISP_CLMR_FWCMD_APINO_MIN                  (0x00)
#define SHDISP_CLMR_FWCMD_APINO_MAX                  (0x70)

#define SHDISP_CLMR_BDIC_SLAVE_ADDR ( 0xA8 )

#define WAIT_1FRAME_US (16666)

void shdisp_SYS_bdic_i2c_set_api(unsigned char apino);
int  shdisp_SYS_bdic_i2c_doKick_if_exist(void);
void shdisp_SYS_bdic_i2c_cmd_reset(void);
int  shdisp_FWCMD_buf_is_empty(void);
void shdisp_FWCMD_buf_set_nokick(int on);
int  shdisp_FWCMD_buf_get_nokick(void);
void shdisp_FWCMD_buf_init(unsigned char apino);
int  shdisp_FWCMD_buf_finish( void );
int  shdisp_FWCMD_doKick(unsigned char notice, unsigned int len, unsigned char * rtnbuf);
int shdisp_FWCMD_eDramPtrReset(void);
char shdisp_FWCMD_set_apino(unsigned char apino);
int shdisp_FWCMD_safe_finishanddoKick( void );
int  shdisp_FWCMD_buf_add( unsigned char code, unsigned short len, unsigned char * data );
int  shdisp_FWCMD_buf_add_multi( unsigned char code, unsigned char slvadr, unsigned char addr, unsigned short len, unsigned char * data );

void shdisp_FWCMD_doKickAndeDramReadFlag_OnOff(int onoff);
int shdisp_SYS_getInfoReg2(void);
void shdisp_SYS_FWCMD_set_timeoutexception(int);
int  shdisp_SYS_FWCMD_istimeoutexception(void);

#endif /* SHDISP_SYSTEM_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
