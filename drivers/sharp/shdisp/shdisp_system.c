/* drivers/sharp/shdisp/shdisp_system.c  (Display Driver)
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
#include <linux/spi/spi.h>
#include <mach/gpio.h>
#include <mach/msm_iomap.h>
#include <asm/io.h>
#include <linux/clk.h>
#include <sharp/sh_smem.h>
#include <sharp/shdisp_kerl.h>
#include "shdisp_system.h"
#include "shdisp_clmr.h"
#include "shdisp_bdic.h"
#include "shdisp_dbg.h"
#include "shdisp_pm.h"
#include "../../../arch/arm/mach-msm/include/mach/msm_xo.h"


/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#if defined(CONFIG_SHDISP_PANEL_SWITCH)
  #define CONFIG_SHDISP_EXCLK_PMIC_A1
  #define CONFIG_SHDISP_EXCLK_MSM
#else  /* CONFIG_SHDISP_PANEL_SWITCH */
  #if defined(CONFIG_MACH_LYNX_DL20) || defined(CONFIG_MACH_KUR)
    #define CONFIG_SHDISP_EXCLK_PMIC_A1
  #elif defined(CONFIG_MACH_LYNX_DL22)
    #define CONFIG_SHDISP_EXCLK_MSM
  #elif defined(CONFIG_MACH_DECKARD_AS96)
    #define CONFIG_SHDISP_EXCLK_PMIC_A1
  #else
    #define CONFIG_SHDISP_EXCLK_MSM
  #endif
#endif /* CONFIG_SHDISP_PANEL_SWITCH */

#define SHDISP_INT_FLAGS (IRQF_TRIGGER_LOW | IRQF_DISABLED)


#define SHDISP_CLMR_SPI_WRITE_SPEED             (38400000)
#define SHDISP_CLMR_SPI_READ_SPEED              (15060000)

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

static int shdisp_host_gpio_request(int num);
static int shdisp_host_gpio_free(int num);
#if defined(SHDISP_USE_EXT_CLOCK)
static void shdisp_host_ctl_lcd_clk_start(unsigned long rate);
static void shdisp_host_ctl_lcd_clk_stop(unsigned long rate);
static int shdisp_host_ctl_lcd_clk_init(void);
static int shdisp_host_ctl_lcd_clk_exit(void);
#endif /* SHDISP_USE_EXT_CLOCK */

static int  shdisp_bdic_i2c_probe(struct i2c_client *client, const struct i2c_device_id * devid);
static int  shdisp_bdic_i2c_remove(struct i2c_client *client);
static int __devinit shdisp_clmr_sio_probe(struct spi_device *spi);
static int __devinit shdisp_clmr_sio_remove(struct spi_device *spi);
static int shdisp_SYS_getInfoReg0_check(int mode);

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

typedef struct bdic_data_tag
{
    struct i2c_client *this_client;
} bdic_i2c_data_t;

static const struct i2c_device_id shdisp_bdic_id[] = {
    { SHDISP_BDIC_I2C_DEVNAME, 0 },
    { }
};

static struct i2c_driver bdic_driver =
{
    .driver = {
        .owner   = THIS_MODULE,
        .name    = SHDISP_BDIC_I2C_DEVNAME,
    },
    .class    = I2C_CLASS_HWMON,
    .probe    = shdisp_bdic_i2c_probe,
    .id_table = shdisp_bdic_id,
    .remove   = shdisp_bdic_i2c_remove,
};

static struct spi_driver clmr_mipi_spi_driver = {
    .driver = {
        .name = "mipi_spi",
        .owner = THIS_MODULE,
    },
    .probe = shdisp_clmr_sio_probe,
    .remove = __devexit_p(shdisp_clmr_sio_remove),
};

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
#define SHDISP_SYS_I2C_FWCMD_BUF_LEN (16 * 16 * 24) /* 6144byte */
static struct shdisp_fwcmd_buffer {
    unsigned int pos;
    unsigned int lastINTnopos;
    unsigned char buf[SHDISP_SYS_I2C_FWCMD_BUF_LEN];
} shdisp_fwcmd_buf;
static unsigned char spi_transfer_buf[SHDISP_SYS_I2C_FWCMD_BUF_LEN];
static unsigned int fmcmd_kickcnt = 1;
static unsigned int fmcmd_api     = SHDISP_CLMR_FWCMD_APINO_NOTHING;
static unsigned int fmcmd_nokick  = 0;

static unsigned short sendaddr[2];
static unsigned short senddata[17];
static unsigned char  readdata[32];
static unsigned short sendcmd;
static struct spi_device *spid = NULL;

static bdic_i2c_data_t *bdic_i2c_p = NULL;
#if defined(CONFIG_SHDISP_PANEL_SWITCH)
#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
static struct msm_xo_voter *xo_handle;
#endif
#else  /* CONFIG_SHDISP_PANEL_SWITCH */
#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1) || defined(CONFIG_SHDISP_EXCLK_PMIC_D1)
static struct msm_xo_voter *xo_handle;
#endif
#endif /* CONFIG_SHDISP_PANEL_SWITCH */
static const unsigned int shdisp_int_irq_port = PM8921_GPIO_IRQ(PM8921_IRQ_BASE,24);
static int shdisp_int_irq_port_staus = 0;
static struct semaphore shdisp_sem_clmr_sio;
static int shdisp_FWCMD_doKickAndeDramReadFlag = 0;
static spinlock_t shdisp_set_irq_spinlock;

#if defined(CONFIG_SHDISP_EXCLK_MSM)
#elif defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
static int shdisp_lcd_clk_count = 0;
#endif
/* ------------------------------------------------------------------------- */
/* DEBUG MACROS                                                              */
/* ------------------------------------------------------------------------- */
#ifdef SHDISP_SYS_SW_TIME_API
static void shdisp_dbg_api_wait_start(void);
static void shdisp_dbg_api_wait_end(unsigned long usec);
#define SHDISP_SYS_DBG_API_WAIT_START           shdisp_dbg_api_wait_start();
#define SHDISP_SYS_DBG_API_WAIT_END(usec)       shdisp_dbg_api_wait_end(usec);
static struct shdisp_sys_dbg_api_info shdisp_sys_dbg_api;
#ifdef SHDISP_SYS_SW_TIME_BDIC
static void shdisp_dbg_bdic_init(void);
static void shdisp_dbg_bdic_logout(void);
static void shdisp_dbg_bdic_singl_write_start(void);
static void shdisp_dbg_bdic_singl_write_retry(void);
static void shdisp_dbg_bdic_singl_write_end(int ret);
static void shdisp_dbg_bdic_singl_read_start(void);
static void shdisp_dbg_bdic_singl_read_retry(void);
static void shdisp_dbg_bdic_singl_read_end(int ret);
static void shdisp_dbg_bdic_multi_read_start(void);
static void shdisp_dbg_bdic_multi_read_retry(void);
static void shdisp_dbg_bdic_multi_read_end(int ret);
static void shdisp_dbg_bdic_multi_write_start(void);
static void shdisp_dbg_bdic_multi_write_retry(void);
static void shdisp_dbg_bdic_multi_write_end(int ret);
#define SHDISP_SYS_DBG_DBIC_INIT                shdisp_dbg_bdic_init();
#define SHDISP_SYS_DBG_DBIC_LOGOUT              shdisp_dbg_bdic_logout();
#define SHDISP_SYS_DBG_DBIC_SINGL_W_START       shdisp_dbg_bdic_singl_write_start();
#define SHDISP_SYS_DBG_DBIC_SINGL_W_RETRY       shdisp_dbg_bdic_singl_write_retry();
#define SHDISP_SYS_DBG_DBIC_SINGL_W_END(ret)    shdisp_dbg_bdic_singl_write_end(ret);
#define SHDISP_SYS_DBG_DBIC_SINGL_R_START       shdisp_dbg_bdic_singl_read_start();
#define SHDISP_SYS_DBG_DBIC_SINGL_R_RETRY       shdisp_dbg_bdic_singl_read_retry();
#define SHDISP_SYS_DBG_DBIC_SINGL_R_END(ret)    shdisp_dbg_bdic_singl_read_end(ret);
#define SHDISP_SYS_DBG_DBIC_MULTI_R_START       shdisp_dbg_bdic_multi_read_start();
#define SHDISP_SYS_DBG_DBIC_MULTI_R_RETRY       shdisp_dbg_bdic_multi_read_retry();
#define SHDISP_SYS_DBG_DBIC_MULTI_R_END(ret)    shdisp_dbg_bdic_multi_read_end(ret);
#define SHDISP_SYS_DBG_DBIC_MULTI_W_START       shdisp_dbg_bdic_multi_write_start();
#define SHDISP_SYS_DBG_DBIC_MULTI_W_RETRY       shdisp_dbg_bdic_multi_write_retry();
#define SHDISP_SYS_DBG_DBIC_MULTI_W_END(ret)    shdisp_dbg_bdic_multi_write_end(ret);
struct shdisp_sys_dbg_i2c_rw_info {
    unsigned long   w_singl_ok_count;
    unsigned long   w_singl_ng_count;
    unsigned long   w_singl_retry;
    struct timespec w_singl_t_start;
    struct timespec w_singl_t_sum;
    unsigned long   r_singl_ok_count;
    unsigned long   r_singl_ng_count;
    unsigned long   r_singl_retry;
    struct timespec r_singl_t_start;
    struct timespec r_singl_t_sum;
    unsigned long   r_multi_ok_count;
    unsigned long   r_multi_ng_count;
    unsigned long   r_multi_retry;
    struct timespec r_multi_t_start;
    struct timespec r_multi_t_sum;
    unsigned long   w_multi_ok_count;
    unsigned long   w_multi_ng_count;
    unsigned long   w_multi_retry;
    struct timespec w_multi_t_start;
    struct timespec w_multi_t_sum;
};
static struct shdisp_sys_dbg_i2c_rw_info shdisp_sys_dbg_bdic;
#else  /* SHDISP_SYS_SW_TIME_BDIC */
#define SHDISP_SYS_DBG_DBIC_INIT
#define SHDISP_SYS_DBG_DBIC_LOGOUT
#define SHDISP_SYS_DBG_DBIC_SINGL_W_START
#define SHDISP_SYS_DBG_DBIC_SINGL_W_RETRY
#define SHDISP_SYS_DBG_DBIC_SINGL_W_END(ret)
#define SHDISP_SYS_DBG_DBIC_SINGL_R_START
#define SHDISP_SYS_DBG_DBIC_SINGL_R_RETRY
#define SHDISP_SYS_DBG_DBIC_SINGL_R_END(ret)
#define SHDISP_SYS_DBG_DBIC_MULTI_R_START
#define SHDISP_SYS_DBG_DBIC_MULTI_R_RETRY
#define SHDISP_SYS_DBG_DBIC_MULTI_R_END(ret)
#define SHDISP_SYS_DBG_DBIC_MULTI_W_START
#define SHDISP_SYS_DBG_DBIC_MULTI_W_RETRY
#define SHDISP_SYS_DBG_DBIC_MULTI_W_END(ret)
#endif /* SHDISP_SYS_SW_TIME_BDIC */
#else  /* SHDISP_SYS_SW_TIME_API */
#define SHDISP_SYS_DBG_API_WAIT_START
#define SHDISP_SYS_DBG_API_WAIT_END(usec)
#define SHDISP_SYS_DBG_DBIC_INIT
#define SHDISP_SYS_DBG_DBIC_SINGL_W_START
#define SHDISP_SYS_DBG_DBIC_SINGL_W_RETRY
#define SHDISP_SYS_DBG_DBIC_SINGL_W_END(ret)
#define SHDISP_SYS_DBG_DBIC_SINGL_R_START
#define SHDISP_SYS_DBG_DBIC_SINGL_R_RETRY
#define SHDISP_SYS_DBG_DBIC_SINGL_R_END(ret)
#define SHDISP_SYS_DBG_DBIC_MULTI_R_START
#define SHDISP_SYS_DBG_DBIC_MULTI_R_RETRY
#define SHDISP_SYS_DBG_DBIC_MULTI_R_END(ret)
#define SHDISP_SYS_DBG_DBIC_MULTI_W_START
#define SHDISP_SYS_DBG_DBIC_MULTI_W_RETRY
#define SHDISP_SYS_DBG_DBIC_MULTI_W_END(ret)
#endif /* SHDISP_SYS_SW_TIME_API */

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
#if defined(SHDISP_USE_EXT_CLOCK)
/* ------------------------------------------------------------------------- */
/* shdisp_SYS_Host_control                                                   */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_Host_control(int cmd, unsigned long rate)
{
    int ret = SHDISP_RESULT_SUCCESS;
    SHDISP_DEBUG("in cmd=%d, rate=%08lx\n", cmd, rate);
    switch (cmd) {
    case SHDISP_HOST_CTL_CMD_LCD_CLK_START:
        shdisp_host_ctl_lcd_clk_start(rate);
        break;
    case SHDISP_HOST_CTL_CMD_LCD_CLK_STOP:
        shdisp_host_ctl_lcd_clk_stop(rate);
        break;
    case SHDISP_HOST_CTL_CMD_LCD_CLK_INIT:
        ret = shdisp_host_ctl_lcd_clk_init();
        break;
    case SHDISP_HOST_CTL_CMD_LCD_CLK_EXIT:
        ret = shdisp_host_ctl_lcd_clk_exit();
        break;
    default:
        SHDISP_ERR("<INVALID_VALUE> cmd(%d), rate(%d).\n", cmd, (int)rate);
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_DEBUG("out\n");

    return ret;
}
#endif


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_delay_us                                                       */
/* ------------------------------------------------------------------------- */

void shdisp_SYS_delay_us(unsigned long usec)
{
    struct timespec tu;

    if (usec >= 1000*1000) {
        tu.tv_sec  = usec / 1000000;
        tu.tv_nsec = (usec % 1000000) * 1000;
    }
    else
    {
        tu.tv_sec  = 0;
        tu.tv_nsec = usec * 1000;
    }

    SHDISP_SYS_DBG_API_WAIT_START;

    hrtimer_nanosleep(&tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);

    SHDISP_SYS_DBG_API_WAIT_END(usec);

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_cmd_delay_us                                                   */
/* ------------------------------------------------------------------------- */
void shdisp_SYS_cmd_delay_us(unsigned long usec)
{
#if 1
    unsigned short wait = (unsigned short)(usec / 100);
    unsigned char ary[2];

    ary[0] = (unsigned char)(wait & 0x00FF);
    ary[1] = (unsigned char)(wait >> 8 & 0x00FF);

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_TIMER, 2, ary);
    } else {
        shdisp_FWCMD_buf_init(0);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_TIMER, 2, ary);
        shdisp_FWCMD_buf_finish();
        shdisp_FWCMD_doKick(1,0,0);
    }
#else
    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_finish();
        shdisp_FWCMD_doKick(1,0,0);
    }
    shdisp_SYS_delay_us(usec);
#endif
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_Host_gpio_init                                                 */
/* ------------------------------------------------------------------------- */

void shdisp_SYS_Host_gpio_init(void)
{
#ifdef SHDISP_NOT_SUPPORT_NO_OS
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_BL_RST_N);

    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_BL_RST_N, SHDISP_GPIO_CTL_LOW);
    shdisp_SYS_delay_us(15000);
  #ifndef SHDISP_USE_LEDC
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_BL_RST_N, SHDISP_GPIO_CTL_HIGH);
    shdisp_SYS_delay_us(1000);
  #endif  /* SHDISP_USE_LEDC */
  #ifdef SHDISP_USE_LEDC
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_LCD_VDD);
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_VDD, SHDISP_GPIO_CTL_LOW);
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_LCD_RESET);
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_RESET, SHDISP_GPIO_CTL_LOW);
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_ALS_VCC);
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_ALS_VCC, SHDISP_GPIO_CTL_LOW);
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_LCD_SCS1);
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_SCS1, SHDISP_GPIO_CTL_HIGH);
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_LCD_SCS2);
    if (shdisp_api_get_hw_revision() <= SHDISP_HW_REV_ES1) {
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_SCS2, SHDISP_GPIO_CTL_HIGH);
    }
    else {
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_SCS2, SHDISP_GPIO_CTL_LOW);
    }
  #endif  /* SHDISP_USE_LEDC */
#else   /* SHDISP_NOT_SUPPORT_NO_OS */
  #ifdef SHDISP_USE_LEDC
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_BL_RST_N);
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_LCD_VDD);
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_ALS_VCC);
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_LCD_SCS1);
    shdisp_host_gpio_request(SHDISP_GPIO_NUM_LCD_SCS2);
  #endif  /* SHDISP_USE_LEDC */
#endif  /* SHDISP_NOT_SUPPORT_NO_OS */
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_Host_gpio_exit                                                 */
/* ------------------------------------------------------------------------- */

void shdisp_SYS_Host_gpio_exit(void)
{
    shdisp_host_gpio_free(SHDISP_GPIO_NUM_BL_RST_N);
#ifdef SHDISP_USE_LEDC
    shdisp_host_gpio_free(SHDISP_GPIO_NUM_ALS_VCC);
    shdisp_host_gpio_free(SHDISP_GPIO_NUM_LCD_RESET);
    shdisp_host_gpio_free(SHDISP_GPIO_NUM_LCD_VDD);
    if (shdisp_api_get_hw_revision() <= SHDISP_HW_REV_ES1) {
        shdisp_host_gpio_free(SHDISP_GPIO_NUM_LCD_SCS1);
        shdisp_host_gpio_free(SHDISP_GPIO_NUM_LCD_SCS2);
    }
#endif  /* SHDISP_USE_LEDC */
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_Host_gpio_request                                              */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_Host_gpio_request(int num)
{
    return shdisp_host_gpio_request(num);
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_Host_gpio_free                                                 */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_Host_gpio_free(int num)
{
    return shdisp_host_gpio_free(num);
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_set_Host_gpio                                                  */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_set_Host_gpio(int num, int value)
{
    if (value != SHDISP_GPIO_CTL_LOW && value != SHDISP_GPIO_CTL_HIGH) {
        SHDISP_ERR("<INVALID_VALUE> value(%d).\n", value);
        return SHDISP_RESULT_FAILURE;
    }

    switch(num) {
    case SHDISP_GPIO_NUM_SPI_SCS:
    case SHDISP_GPIO_NUM_CLMR_RESET:
    case SHDISP_GPIO_NUM_CLMR_PLLONCTL:
    case SHDISP_GPIO_NUM_BL_RST_N:
#ifdef SHDISP_USE_LEDC
    case SHDISP_GPIO_NUM_ALS_VCC:
    case SHDISP_GPIO_NUM_LCD_SCS1:
    case SHDISP_GPIO_NUM_LCD_SCS2:
    case SHDISP_GPIO_NUM_LCD_RESET:
    case SHDISP_GPIO_NUM_LCD_VDD:
#endif  /* SHDISP_USE_LEDC */
        break;
    default:
        SHDISP_ERR("<INVALID_VALUE> num(%d).\n", num);
        return SHDISP_RESULT_FAILURE;
    }
    gpio_set_value(num, value);
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_request_irq                                                    */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_request_irq(irqreturn_t (*irq_handler)( int , void * ) )
{
    int ret;

    if( irq_handler == NULL )
        return SHDISP_RESULT_FAILURE;


    ret = request_irq(shdisp_int_irq_port, *irq_handler,
                      SHDISP_INT_FLAGS, "shdisp", NULL);

    if( ret == 0 ){
        disable_irq(shdisp_int_irq_port);
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_free_irq                                                     */
/* ------------------------------------------------------------------------- */

void  shdisp_SYS_free_irq(void)
{
    free_irq(shdisp_int_irq_port, NULL);
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_set_irq_init                                                   */
/* ------------------------------------------------------------------------- */

void  shdisp_SYS_set_irq_init(void)
{
    spin_lock_init( &shdisp_set_irq_spinlock );
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_set_irq                                                        */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_set_irq( int enable )
{
    unsigned long flags = 0;
    int ret = SHDISP_RESULT_SUCCESS;

    spin_lock_irqsave( &shdisp_set_irq_spinlock, flags);

    if( enable == shdisp_int_irq_port_staus ){
        spin_unlock_irqrestore( &shdisp_set_irq_spinlock, flags);
        return SHDISP_RESULT_SUCCESS;
    }
    
    if( enable == SHDISP_IRQ_ENABLE ){
        enable_irq_wake(shdisp_int_irq_port);
        enable_irq(shdisp_int_irq_port);
        shdisp_int_irq_port_staus = enable;
    }
    else if( enable == SHDISP_IRQ_DISABLE ){
        disable_irq_nosync(shdisp_int_irq_port);
        disable_irq_wake(shdisp_int_irq_port);
        shdisp_int_irq_port_staus = enable;
    }
    else{
        SHDISP_ERR("<INVALID_VALUE> shdisp_SYS_set_irq:enable=%d.\n", enable);
        ret = SHDISP_RESULT_FAILURE;
    }
    spin_unlock_irqrestore( &shdisp_set_irq_spinlock, flags);
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_bdic_i2c_init                                                  */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_bdic_i2c_init(void)
{
    int ret;

    ret = i2c_add_driver(&bdic_driver);
    if ( ret < 0 ) {
        SHDISP_ERR("<RESULT_FAILURE> i2c_add_driver.\n");
        return SHDISP_RESULT_FAILURE;
    }
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_bdic_i2c_exit                                                  */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_bdic_i2c_exit(void)
{
    i2c_del_driver(&bdic_driver);

    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_bdic_i2c_write                                                 */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_bdic_i2c_write(unsigned char addr, unsigned char data)
{
#if (SHDISP_BOOT_SW_ENABLE_DISPLAY|SHDISP_BOOT_SW_ENABLE_LED)
    int result = SHDISP_RESULT_SUCCESS;
    unsigned char ary[3];

    ary[0] = SHDISP_BDIC_I2C_SLAVE_ADDR;
    ary[1] = addr;
    ary[2] = data;

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_WRITE, 3, ary);
    } else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_WRITE, 3, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 0, NULL);
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
    }
#endif  /* (SHDISP_BOOT_SW_ENABLE_DISPLAY|SHDISP_BOOT_SW_ENABLE_LED) */
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_bdic_i2c_mask_write                                            */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_bdic_i2c_mask_write(unsigned char addr, unsigned char data, unsigned char msk)
{
#if (SHDISP_BOOT_SW_ENABLE_DISPLAY|SHDISP_BOOT_SW_ENABLE_LED)
    int result = SHDISP_RESULT_SUCCESS;
    unsigned char ary[4];

    ary[0] = SHDISP_BDIC_I2C_SLAVE_ADDR;
    ary[1] = addr;
    ary[2] = msk;
    ary[3] = data;

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_MASK_WRITE, 4, ary);
    } else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_MASK_WRITE, 4, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 0, NULL);
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
    }
#endif  /* (SHDISP_BOOT_SW_ENABLE_DISPLAY|SHDISP_BOOT_SW_ENABLE_LED) */
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_bdic_i2c_multi_write                                           */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_bdic_i2c_multi_write(unsigned char addr, unsigned char *wval, unsigned char size)
{
#if (SHDISP_BOOT_SW_ENABLE_DISPLAY|SHDISP_BOOT_SW_ENABLE_LED)
    int result = SHDISP_RESULT_SUCCESS;

    if ((size < 1) || (size > 20)) {
        SHDISP_ERR("<INVALID_VALUE> size(%d).\n", size);
        return SHDISP_RESULT_FAILURE;
    }

    if (wval == NULL) {
        SHDISP_ERR("<NULL_POINTER> val.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add_multi(SHDISP_CLMR_FWCMD_I2C_MULTI_WRITE, SHDISP_BDIC_I2C_SLAVE_ADDR, addr, size, wval);
    } else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add_multi(SHDISP_CLMR_FWCMD_I2C_MULTI_WRITE, SHDISP_BDIC_I2C_SLAVE_ADDR, addr, size, wval);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 0, NULL);
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
    }
#endif  /* (SHDISP_BOOT_SW_ENABLE_DISPLAY|SHDISP_BOOT_SW_ENABLE_LED) */
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_bdic_i2c_read                                                  */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_bdic_i2c_read(unsigned char addr, unsigned char *data)
{
#if (SHDISP_BOOT_SW_ENABLE_DISPLAY|SHDISP_BOOT_SW_ENABLE_LED)
    int result = SHDISP_RESULT_SUCCESS;
    char ary[2];
    unsigned char rtnbuf[4] = {0};

    ary[0] = SHDISP_BDIC_I2C_SLAVE_ADDR;
    ary[1] = addr;

    if (data == NULL) {
        SHDISP_ERR("<NULL_POINTER> data.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if (shdisp_FWCMD_buf_get_nokick()) {
        result = shdisp_SYS_bdic_i2c_doKick_if_exist();
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
        shdisp_FWCMD_buf_set_nokick(1);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_READ, 2, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 4, rtnbuf);
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
    }
    else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_READ, 2, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 4, rtnbuf);
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
    }

    *data = rtnbuf[0];
    SHDISP_DEBUG("[SHDISP]%s addr=0x%02x, data=0x%02x\n", __func__, addr, *data);
#endif  /* (SHDISP_BOOT_SW_ENABLE_DISPLAY|SHDISP_BOOT_SW_ENABLE_LED) */
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_bdic_i2c_multi_read                                            */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_bdic_i2c_multi_read(unsigned char addr, unsigned char *data, int size)
{
    int result = SHDISP_RESULT_SUCCESS;
    int i = 0;
    char ary[2] = { SHDISP_BDIC_I2C_SLAVE_ADDR, addr };
    unsigned char rtnbuf[4] = {0};

    if ((size < 1) || (size > 8)) {
        SHDISP_ERR("<INVALID_VALUE> size(%d).\n", size);
        return SHDISP_RESULT_FAILURE;
    }

    if (data == NULL) {
        SHDISP_ERR("<NULL_POINTER> data.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if (shdisp_FWCMD_buf_get_nokick()) {
        result = shdisp_SYS_bdic_i2c_doKick_if_exist();
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
        shdisp_FWCMD_buf_set_nokick(1);

        for (i = 0; i < size; i++) {
            shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_READ, 2, ary);
            shdisp_FWCMD_buf_finish();
            result = shdisp_FWCMD_doKick(1, 4, rtnbuf);
            if (result != SHDISP_RESULT_SUCCESS) {
                return result;
            }
            *data = rtnbuf[0];
            SHDISP_DEBUG("addr=0x%02x, data=0x%02x\n", ary[1], *data);
            ary[1]++;
            data++;
        }
    }
    else {
        for (i = 0; i < size; i++) {
            shdisp_FWCMD_buf_init(0x70);
            shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_READ, 2, ary);
            shdisp_FWCMD_buf_finish();
            result = shdisp_FWCMD_doKick(1, 4, rtnbuf);
            if (result != SHDISP_RESULT_SUCCESS) {
                return result;
            }
            *data = rtnbuf[0];
            SHDISP_DEBUG("addr=0x%02x, data=0x%02x\n", ary[1], *data);
            ary[1]++;
            data++;
        }
    }
    return SHDISP_RESULT_SUCCESS;
}

#ifdef  SHDISP_USE_LEDC
/* ------------------------------------------------------------------------- */
/* shdisp_SYS_psals_i2c_write                                                */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_psals_i2c_write(unsigned char addr, unsigned char data)
{
    int result = SHDISP_RESULT_SUCCESS;
    unsigned char ary[3];

    ary[0] = SHDISP_BDIC_SENSOR_SLAVE_ADDR;
    ary[1] = addr;
    ary[2] = data;

    if (shdisp_FWCMD_buf_get_nokick()) {
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_WRITE, 3, ary);
    } else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_WRITE, 3, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 0, NULL);
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_psals_i2c_read                                                 */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_psals_i2c_read(unsigned char addr, unsigned char *data)
{
    int result = SHDISP_RESULT_SUCCESS;
    unsigned char ary[2];
    unsigned char rtnbuf[4] = {0};

    if (data == NULL) {
        SHDISP_ERR("<NULL_POINTER> data.\n");
        return SHDISP_RESULT_FAILURE;
    }

    ary[0] = SHDISP_BDIC_SENSOR_SLAVE_ADDR;
    ary[1] = addr;

    if (shdisp_FWCMD_buf_get_nokick()) {
        result = shdisp_SYS_bdic_i2c_doKick_if_exist();
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
        shdisp_FWCMD_buf_set_nokick(1);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_READ, 2, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 4, rtnbuf);
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
    }
    else {
        shdisp_FWCMD_buf_init(0x70);
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_I2C_1BYTE_READ, 2, ary);
        shdisp_FWCMD_buf_finish();
        result = shdisp_FWCMD_doKick(1, 4, rtnbuf);
        if (result != SHDISP_RESULT_SUCCESS) {
            return result;
        }
    }

    *data = rtnbuf[0];
    SHDISP_TRACE("addr=0x%02x, data=0x%02x\n", addr, *data);
    return SHDISP_RESULT_SUCCESS;
}
#endif  /* SHDISP_USE_LEDC */

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_clmr_sio_init                                              */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_clmr_sio_init(void)
{
    int ret;

    ret = spi_register_driver(&clmr_mipi_spi_driver);
    if ( ret < 0 ) {
        SHDISP_ERR("<RESULT_FAILURE> spi_register_driver.\n");
        return SHDISP_RESULT_FAILURE;
    }

    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_clmr_sio_exit                                              */
/* ------------------------------------------------------------------------- */

int  shdisp_SYS_clmr_sio_exit(void)
{
    spi_unregister_driver(&clmr_mipi_spi_driver);

    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_clmr_sio_transfer                                          */
/* ------------------------------------------------------------------------- */
int shdisp_SYS_clmr_sio_transfer(unsigned short reg, unsigned char *wbuf, int wlen, unsigned char *rbuf, int rlen)
{
    int result = SHDISP_RESULT_SUCCESS;
    struct spi_message  m;
    struct spi_transfer *x, xfer[7];
    unsigned short wdata;
    const unsigned char *data;
    int i, ret = 0;

    if (!spid) {
        SHDISP_ERR(": spid=NULL\n");
        return SHDISP_RESULT_FAILURE;
    }

    if( shdisp_SYS_FWCMD_istimeoutexception() ){
        SHDISP_DEBUG(": FW timeout...\n");
        return SHDISP_RESULT_SUCCESS;
    }

#ifdef SHDISP_CLMR_REG_WRITEDUMP
    if( wlen ){
        unsigned char datar[4] = {0,};
        shdisp_SYS_clmr_sio_transfer(reg, NULL, 0, datar, 4);
        SHDISP_DEBUG( "[writed dump] reg=0x%04x: before = 0x%02x%02x%02x%02x:"
                                                 " write  = 0x%02x%02x%02x%02x\n",
                                   reg, datar[0], datar[1], datar[2], datar[3],
                                        wbuf[0], wbuf[1], wbuf[2], wbuf[3] );
    }
#endif /* SHDISP_CLMR_REG_WRITEDUMP */
    down(&shdisp_sem_clmr_sio);

    ret = gpio_request(SHDISP_GPIO_NUM_SPI_SCS, "CLMR_SCS");
    if(ret != 0) {
        up(&shdisp_sem_clmr_sio);
        SHDISP_ERR("gpio_request() failed. ret = %d\n", ret);
        return SHDISP_RESULT_FAILURE;
    }

    spi_message_init(&m);

    memset(xfer, 0, sizeof(xfer));
    x = &xfer[0];

    if (wlen > 0) {
        sendcmd = 0x00C0;   /* write command */
    }
    else if (rlen > 0) {
        sendcmd = 0x00C2;   /* read  command */
    }
    else {
        up(&shdisp_sem_clmr_sio);
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_FAILURE;
    }

    if (sendcmd & 0x0001){
        sendcmd = ( 0x8000 | 0x0000 | (sendcmd >> 1) );
    }else{
        sendcmd = ( 0x0000 | 0x0000 | (sendcmd >> 1) );
    }
    x->tx_buf           = &sendcmd;
    x->bits_per_word    = 9;
    x->len              = 2;
    x->speed_hz         = SHDISP_CLMR_SPI_WRITE_SPEED;
    spi_message_add_tail(x, &m);

    x++;
    sendaddr[0] = (unsigned short)(0x0000 | ((reg >> 8) & 0x00ff));
    if(sendaddr[0] & 0x0001){
        sendaddr[0] = ( 0x8000 | 0x0080 | (sendaddr[0] >> 1) );
    }else{
        sendaddr[0] = ( 0x0000 | 0x0080 | (sendaddr[0] >> 1) );
    }

    sendaddr[1] = (unsigned short)(0x0000 | (reg & 0x00ff));
    if(sendaddr[1] & 0x0001){
        sendaddr[1] = ( 0x8000 | 0x0080 | (sendaddr[1] >> 1) );
    }else{
        sendaddr[1] = ( 0x0000 | 0x0080 | (sendaddr[1] >> 1) );
    }
    x->tx_buf           = sendaddr;
    x->bits_per_word    = 9;
    x->len              = 4;
    x->speed_hz         = SHDISP_CLMR_SPI_WRITE_SPEED;
    spi_message_add_tail(x, &m);

    if (rlen > 0) {
        ret = spi_sync(spid, &m);
        if (ret < 0) {
            SHDISP_ERR(":[%d] spi_sync() ret=%d \n", __LINE__, ret);
            result = SHDISP_RESULT_FAILURE;
        }

        spi_message_init(&m);

        memset(xfer, 0, sizeof(xfer));
        x = &xfer[0];

        /* SPI_CS LOW */
        gpio_tlmm_config(GPIO_CFG(SHDISP_GPIO_NUM_SPI_SCS, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_SPI_SCS, SHDISP_GPIO_CTL_LOW);

        sendcmd = 0x00C4;   /* read data command */
        if(sendcmd & 0x0001){
            sendcmd = ( 0x8000 | 0x0000 | (sendcmd >> 1) );
        }else{
            sendcmd = ( 0x0000 | 0x0000 | (sendcmd >> 1) );
        }

        x->tx_buf           = &sendcmd;
        x->bits_per_word    = 9;
        x->len              = 2;
        x->speed_hz         = SHDISP_CLMR_SPI_WRITE_SPEED;
        spi_message_add_tail(x, &m);
        ret = spi_sync(spid, &m);
        if (ret < 0) {
            SHDISP_ERR(":[%d] spi_sync() ret=%d \n", __LINE__, ret);
            result = SHDISP_RESULT_FAILURE;
        }

        spi_message_init(&m);

        x++;
        x->rx_buf           = readdata;
        x->len              = rlen;
        x->bits_per_word    = 8;
        x->speed_hz         = SHDISP_CLMR_SPI_READ_SPEED;
        spi_message_add_tail(x, &m);

        ret = spi_sync(spid, &m);
        if (ret < 0) {
            SHDISP_ERR(":[%d] spi_sync() ret=%d \n", __LINE__, ret);
            result = SHDISP_RESULT_FAILURE;
        }

        /* SPI_CS HIGH */
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_SPI_SCS, SHDISP_GPIO_CTL_HIGH);
        gpio_tlmm_config(GPIO_CFG(SHDISP_GPIO_NUM_SPI_SCS, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

        for (i = 0;i < rlen; i++) {
            *rbuf = readdata[i];
            rbuf++;
        }

    } else if (wlen > 0) {
        x++;

        data = wbuf;
        for (i = 0; i < wlen; i++) {
            wdata = ( 0x0100 | (unsigned short)(*data) );

            if (wdata & 0x0001) {
                wdata = ( 0x8000 | 0x0080 | (wdata >> 1) );
            }
            else
            {
                wdata = ( 0x0000 | 0x0080 | (wdata >> 1) );
            }

            senddata[i] = wdata;
            data++;
        }

        x->tx_buf           = senddata;
        x->len              = wlen * 2;
        x->bits_per_word    = 9;
        x->speed_hz         = SHDISP_CLMR_SPI_WRITE_SPEED;
        spi_message_add_tail(x, &m);

        ret = spi_sync(spid, &m);
        if (ret < 0) {
            SHDISP_ERR(":[%d] spi_sync() ret=%d \n", __LINE__, ret);
            result = SHDISP_RESULT_FAILURE;
        }
    } else {
        ;
    }

    gpio_free(SHDISP_GPIO_NUM_SPI_SCS);

    up(&shdisp_sem_clmr_sio);

#if 0
    if( wlen > 0 ){
        unsigned char data[4] = {0,};
        shdisp_SYS_clmr_sio_transfer(reg, NULL, 0, data, 4);
        SHDISP_DEBUG( "[writed dump] reg=0x%04x: val = 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
                                       reg, data[0], data[1], data[2], data[3] );
    }
#endif /* SHDISP_CLMR_REG_WRITEDUMP */

    return result;
}


/* ------------------------------------------------------------------------- */
/* shdisp_SYS_clmr_sio_itransfer                                         */
/* ------------------------------------------------------------------------- */
int shdisp_SYS_clmr_sio_itransfer(unsigned short reg, unsigned char *wbuf, int wlen, unsigned char *rbuf, int rlen)
{
    int result = SHDISP_RESULT_SUCCESS;
    int i;

    if (!spid) {
        SHDISP_ERR(": spid=NULL\n");
        return SHDISP_RESULT_FAILURE;
    }


    if (rlen > 0) {

        for (i = 0; i < rlen / 4; i++) {

            result = shdisp_SYS_clmr_sio_transfer(reg, NULL, 0, rbuf, 4);
            if (result != SHDISP_RESULT_SUCCESS) {
                return result;
            }
            reg += 4;
            rbuf += 4;

        }

    } else if (wlen > 0) {

        for (i = 0; i < wlen / 4; i++) {

            result = shdisp_SYS_clmr_sio_transfer(reg, wbuf, 4, NULL, 0);
            if (result != SHDISP_RESULT_SUCCESS) {
                return result;
            }
            reg += 4;
            wbuf += 4;

        }

    } else {
        ;
    }

    return result;
}

static struct shdisp_SYS_sio_DBI9bitFillConv {
    unsigned char   cmd;
    unsigned int    srcshift_fr;
    unsigned char   fr_mask;
    unsigned int    dstmove_fr;
    unsigned int    srcshift_bk;
    unsigned char   bk_mask;
    unsigned int    dstmove_bk;
} shdisp_SYS_sio_DBI9bitFillConvTbl[] = {
    { 0x80, 1, 0xFF, 1, 7, 0x80, 1 },
    { 0x40, 2, 0x7F, 1, 6, 0xC0, 1 },
    { 0x20, 3, 0x3F, 1, 5, 0xE0, 1 },
    { 0x10, 4, 0x1F, 1, 4, 0xF0, 1 },
    { 0x08, 5, 0x0F, 1, 3, 0xF8, 1 },
    { 0x04, 6, 0x07, 1, 2, 0xFC, 1 },
    { 0x02, 7, 0x03, 1, 1, 0xFE, 1 },
    { 0x01, 8, 0x01, 2, 0, 0xFF, 2 },
};

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_sio_DBI9bitFillConv                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_SYS_sio_DBI9bitFillConv(unsigned char * dstpos, unsigned char * src, int len, int pos)
{
    struct shdisp_SYS_sio_DBI9bitFillConv * conv;
    int i = 0;
    unsigned char * dstfr = dstpos;
    unsigned char * dstbk = dstpos+1;

    for( i = 0; i < len; i++ ){
        conv = &shdisp_SYS_sio_DBI9bitFillConvTbl[pos];
        *dstfr |=  ((conv->cmd) | (*src)>>conv->srcshift_fr) & conv->fr_mask;
        dstfr  += conv->dstmove_fr;
        pos++;
        *dstfr = 0;
        *dstbk = ((*src)<<conv->srcshift_bk) & conv->bk_mask;
        dstbk  += conv->dstmove_bk;
        pos %= 8;
        src++;
    }
    return dstfr - dstpos;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_clmr_sio_eDramWrite_DBI                                        */
/* ------------------------------------------------------------------------- */
int shdisp_SYS_clmr_sio_eDramWrite_DBI(int mode, unsigned char *buf, int len)
{
    int ret;
    int result = SHDISP_RESULT_SUCCESS;
    struct spi_message  m;
    struct spi_transfer xfer[2];

    int blockSize = 16*9/8;
    int cmdSize = 4;
    int convSize = 0;
    int convedSize = 0;
    int i = 0;
    int useBufSize;
    int transcnt;
    int blockConvCntof1Buf = (sizeof(spi_transfer_buf)-cmdSize)/blockSize;
    blockConvCntof1Buf /= 2;
    blockConvCntof1Buf *= 2;
    useBufSize = blockConvCntof1Buf * blockSize;
    transcnt = ( (((len*9)/8)-1) / useBufSize) +1;

    memset(xfer, 0, sizeof(xfer));

    xfer[0].tx_buf = spi_transfer_buf;
    xfer[0].speed_hz = SHDISP_CLMR_SPI_WRITE_SPEED;
    xfer[0].bits_per_word = 32;
    xfer[0].len = useBufSize + cmdSize;

    convSize = blockConvCntof1Buf * 16;

    if( mode == 0 ){
        *((unsigned short*)spi_transfer_buf) = ((0x00C7)>>1) | 0x8000;
    }
    else /* if ( mode == 2 ) */{
        *((unsigned short*)spi_transfer_buf) = ((0x00C8)>>1) | 0x0000;
    }

    for( i = 0; i < transcnt-1; i++ ){
        shdisp_SYS_sio_DBI9bitFillConv(spi_transfer_buf+1, buf, convSize, 1);
        buf += convSize;
        convedSize += convSize;

        spi_message_init(&m);
        spi_message_add_tail(&xfer[0], &m);
        ret = spi_sync(spid, &m);

        if (ret < 0) {
            SHDISP_ERR(":[%d] spi_sync() ret=%d \n", __LINE__, ret);
            result = SHDISP_RESULT_FAILURE;
        }
        *((unsigned short*)spi_transfer_buf) = ((0x00C8)>>1) | 0x0000;
    }

    shdisp_SYS_sio_DBI9bitFillConv(spi_transfer_buf+1, buf, len-convedSize, 1);
    xfer[0].bits_per_word = 32;
    xfer[0].len = ((1+len-convedSize) *9) /8;
    xfer[0].len = ((xfer[0].len / 4)+1)*4;
    spi_message_init(&m);
    spi_message_add_tail(&xfer[0], &m);
    ret = spi_sync(spid, &m);

    if (ret < 0) {
        SHDISP_ERR(":[%d] spi_sync() ret=%d \n", __LINE__, ret);
        result = SHDISP_RESULT_FAILURE;
    }
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_clmr_sio_eDram_transfer                                    */
/* ------------------------------------------------------------------------- */
int shdisp_SYS_clmr_sio_eDram_transfer(int mode, unsigned char *buf, int len)
{
    int result = SHDISP_RESULT_SUCCESS;
    struct spi_message  m;
    struct spi_transfer *x, xfer[7];
    unsigned char *rbuf;
    int i, ret = 0;

    if (!spid) {
        SHDISP_ERR(": spid=NULL\n");
        return SHDISP_RESULT_FAILURE;
    }

    if ((mode < 0) || (mode > 3)) {
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_FAILURE;
    }

    if( shdisp_SYS_FWCMD_istimeoutexception() ){
        SHDISP_DEBUG(": FW timeout...\n");
        return SHDISP_RESULT_SUCCESS;
    }

    shdisp_api_set_pll_on_ctl_reg(SHDISP_CLMR_PLL_ON);

    down(&shdisp_sem_clmr_sio);

    ret = gpio_request(SHDISP_GPIO_NUM_SPI_SCS, "CLMR_SCS");
    if(ret != 0) {
        up(&shdisp_sem_clmr_sio);
        SHDISP_ERR("gpio_request() failed. ret = %d\n", ret);
        shdisp_api_set_pll_on_ctl_reg(SHDISP_CLMR_PLL_OFF);
        return SHDISP_RESULT_FAILURE;
    }

    /* read */
    if (mode & 1) {

        spi_message_init(&m);

        memset(xfer, 0, sizeof(xfer));
        x = &xfer[0];

        if (mode & 2) {
            sendcmd = 0x00CA;   /* continue read  eDram command */
            sendcmd = ( 0x0000 | 0x0000 | (sendcmd >> 1) );
        } else {
            sendcmd = 0x00C9;   /* read  eDram command */
            sendcmd = ( 0x8000 | 0x0000 | (sendcmd >> 1) );
            len += 16;  /* dummy data */
        }
        x->tx_buf           = &sendcmd;
        x->bits_per_word    = 9;
        x->len              = 2;
        x->speed_hz         = SHDISP_CLMR_SPI_WRITE_SPEED;

        spi_message_add_tail(x, &m);

        /* SPI_CS LOW */
        gpio_tlmm_config(GPIO_CFG(SHDISP_GPIO_NUM_SPI_SCS, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_SPI_SCS, SHDISP_GPIO_CTL_LOW);

        ret = spi_sync(spid, &m);
        if (ret < 0) {
            SHDISP_ERR(":[%d] spi_sync() ret=%d \n", __LINE__, ret);
            result = SHDISP_RESULT_FAILURE;
        }

        x = &xfer[1];
        rbuf = buf;
        for (i = 0; i < len / 16; i++) {
            spi_message_init(&m);
            x->rx_buf           = rbuf;
            x->len              = 16;
            x->bits_per_word    = 8;
            x->speed_hz         = SHDISP_CLMR_SPI_READ_SPEED;
            spi_message_add_tail(x, &m);

            ret = spi_sync(spid, &m);
            if (ret < 0) {
                SHDISP_ERR(":[%d] spi_sync() ret=%d \n", __LINE__, ret);
                result = SHDISP_RESULT_FAILURE;
            }

#if 0
            SHDISP_DEBUG(": [%04x/%04x]rbuf=", (mode == 1) ? (i == 0) ? 0xFFFF : (i - 1) * 16 : i * 16, (mode == 1) ? len - 16 : len);
            for (j = 0; j < 16; j++) {
                printk("%02x ", rbuf[j]);
            }
            printk("\n");
#endif
            if (i == 0 && mode == 1) {
                /* dummy data */
            } else {
                rbuf += 16;
            }
        }

        /* SPI_CS HIGH */
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_SPI_SCS, SHDISP_GPIO_CTL_HIGH);
        gpio_tlmm_config(GPIO_CFG(SHDISP_GPIO_NUM_SPI_SCS, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

    /* write */
    } else {
        shdisp_SYS_clmr_sio_eDramWrite_DBI( mode, buf, len );

    }

    gpio_free(SHDISP_GPIO_NUM_SPI_SCS);

    up(&shdisp_sem_clmr_sio);

    shdisp_api_set_pll_on_ctl_reg(SHDISP_CLMR_PLL_OFF);

    return result;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_semaphore_init                                                 */
/* ------------------------------------------------------------------------- */
void shdisp_SYS_semaphore_init(void)
{
    sema_init(&shdisp_sem_clmr_sio, 1);
}


/* ------------------------------------------------------------------------- */
/* SUB ROUTINE                                                               */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_host_gpio_request                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_host_gpio_request(int num)
{
    int ret = SHDISP_RESULT_SUCCESS;

    switch (num) {
    case SHDISP_GPIO_NUM_BL_RST_N:
        gpio_request(SHDISP_GPIO_NUM_BL_RST_N, "BL_RST_N");
        break;
    case SHDISP_GPIO_NUM_CLK_SEL:
        gpio_request(SHDISP_GPIO_NUM_CLK_SEL, "LCD_CLK_SEL");
        break;
#ifdef SHDISP_USE_LEDC
    case SHDISP_GPIO_NUM_ALS_VCC:
        gpio_request(SHDISP_GPIO_NUM_ALS_VCC, "ALS_VCC");
        break;
    case SHDISP_GPIO_NUM_LCD_RESET:
        gpio_request(SHDISP_GPIO_NUM_LCD_RESET, "LCD_RESET");
        break;
    case SHDISP_GPIO_NUM_LCD_VDD:
        gpio_request(SHDISP_GPIO_NUM_LCD_VDD, "LCD_VDD");
        break;
    case SHDISP_GPIO_NUM_LCD_SCS1:
        gpio_request(SHDISP_GPIO_NUM_LCD_SCS1, "LCD_SCS1");
        break;
    case SHDISP_GPIO_NUM_LCD_SCS2:
        gpio_request(SHDISP_GPIO_NUM_LCD_SCS2, "LCD_SCS2");
        break;
#endif  /* SHDISP_USE_LEDC */
    default:
        SHDISP_ERR("<INVALID_VALUE> num(%d).\n", num);
        ret = SHDISP_RESULT_FAILURE;
        break;
    }

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_host_gpio_free                                                     */
/* ------------------------------------------------------------------------- */

static int shdisp_host_gpio_free(int num)
{
    int ret = SHDISP_RESULT_SUCCESS;

    switch (num) {
    case SHDISP_GPIO_NUM_BL_RST_N:
    case SHDISP_GPIO_NUM_CLK_SEL:
#ifdef SHDISP_USE_LEDC
    case SHDISP_GPIO_NUM_ALS_VCC:
    case SHDISP_GPIO_NUM_LCD_RESET:
    case SHDISP_GPIO_NUM_LCD_VDD:
    case SHDISP_GPIO_NUM_LCD_SCS1:
    case SHDISP_GPIO_NUM_LCD_SCS2:
#endif  /* SHDISP_USE_LEDC */
        gpio_free(num);
        break;
    default:
        SHDISP_ERR("<INVALID_VALUE> num(%d).\n", num);
        ret = SHDISP_RESULT_FAILURE;
        break;
    }

    return ret;
}
#if defined(SHDISP_USE_EXT_CLOCK)
/* ------------------------------------------------------------------------- */
/* shdisp_host_ctl_lcd_clk_start                                             */
/* ------------------------------------------------------------------------- */

static void shdisp_host_ctl_lcd_clk_start(unsigned long rate)
{
#if defined(CONFIG_SHDISP_PANEL_SWITCH)

#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
    int ret = 0;
#endif /* CONFIG_SHDISP_EXCLK_PMIC_A1 */

    SHDISP_DEBUG("if(shdisp_api_get_panel_info() == 0)\n");
    if(shdisp_api_get_panel_info() == 0) {
#if defined(CONFIG_SHDISP_EXCLK_MSM)
        if(shdisp_lcd_clk_count != 0) {
    SHDISP_DEBUG("out1\n");
            return;
        }
        gp0_clk = clk_get(NULL, "gp0_clk");
        clk_set_rate(gp0_clk, rate);
        clk_prepare(gp0_clk);
        clk_enable(gp0_clk);
        shdisp_lcd_clk_count = 1;
#endif /* CONFIG_SHDISP_EXCLK_MSM */
    } else {
#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
        if(shdisp_lcd_clk_count != 0) {
            SHDISP_DEBUG("out1\n");
            return;
        }
    SHDISP_DEBUG("ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_ON)\n");
        ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_ON);
        if (ret) {
            SHDISP_ERR("%s failed to vote for TCXOA1. ret=%d\n", __func__, ret);
        }
        else {
            shdisp_lcd_clk_count = 1;
        }
#endif /* CONFIG_SHDISP_EXCLK_PMIC_A1 */
    }

#else  /* CONFIG_SHDISP_PANEL_SWITCH */

#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
    int ret = 0;
    if(shdisp_lcd_clk_count != 0) {
        SHDISP_DEBUG("out1\n");
        return;
    }
    SHDISP_DEBUG("ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_ON)\n");
    ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_ON);
    if (ret) {
        SHDISP_ERR("%s failed to vote for TCXOA1. ret=%d\n", __func__, ret);
    }
    else {
        shdisp_lcd_clk_count = 1;
    }
#elif defined(CONFIG_SHDISP_EXCLK_PMIC_D1)
    int ret = 0;
    SHDISP_DEBUG("ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_ON)\n");
    ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_ON);
    if (ret) {
        SHDISP_ERR("%s failed to vote for TCXOD1. ret=%d\n", __func__, ret);
    }
#elif defined(CONFIG_SHDISP_EXCLK_MSM)

    if(shdisp_lcd_clk_count != 0) {
        SHDISP_DEBUG("out2\n");
        return;
    }

    SHDISP_DEBUG("gp0_clk = clk_get(NULL, gp0_clk)\n");
    gp0_clk = clk_get(NULL, "gp0_clk");
    clk_set_rate(gp0_clk, rate);
    clk_prepare(gp0_clk);
    clk_enable(gp0_clk);

    shdisp_lcd_clk_count = 1;

#endif  /* CONFIG_SHDISP_EXCLK_*** */

#endif /* CONFIG_SHDISP_PANEL_SWITCH */

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_host_ctl_lcd_clk_stop                                              */
/* ------------------------------------------------------------------------- */

static void shdisp_host_ctl_lcd_clk_stop(unsigned long rate)
{
#if defined(CONFIG_SHDISP_PANEL_SWITCH)

#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
    int ret = 0;
#endif /* CONFIG_SHDISP_EXCLK_PMIC_A1 */

    if(shdisp_api_get_panel_info() == 0) {
#if defined(CONFIG_SHDISP_EXCLK_MSM)
        if(shdisp_lcd_clk_count == 0) {
            return;
        }

        clk_disable(gp0_clk);
        clk_unprepare(gp0_clk);

        shdisp_lcd_clk_count = 0;
#endif /* CONFIG_SHDISP_EXCLK_MSM */
    } else {
#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
        if(shdisp_lcd_clk_count == 0) {
            return;
        }
        ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_OFF);
        if (ret) {
            SHDISP_ERR("%s failed to vote for TCXOA1. ret=%d\n", __func__, ret);
        }
        else {
            shdisp_lcd_clk_count = 0;
        }
#endif /* CONFIG_SHDISP_EXCLK_PMIC_A1 */
    }

#else  /* CONFIG_SHDISP_PANEL_SWITCH */

#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
    int ret = 0;
    if(shdisp_lcd_clk_count == 0) {
        return;
    }
    ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_OFF);
    if (ret) {
        SHDISP_ERR("%s failed to vote for TCXOA1. ret=%d\n", __func__, ret);
    }
    else {
        shdisp_lcd_clk_count = 0;
    }
#elif defined(CONFIG_SHDISP_EXCLK_PMIC_D1)
    int ret = 0;
    ret = msm_xo_mode_vote(xo_handle , MSM_XO_MODE_OFF);
    if (ret) {
        SHDISP_ERR("%s failed to vote for TCXOD1. ret=%d\n", __func__, ret);
    }
#elif defined(CONFIG_SHDISP_EXCLK_MSM)

    if(shdisp_lcd_clk_count == 0) {
        return;
    }

    clk_disable(gp0_clk);
    clk_unprepare(gp0_clk);

    shdisp_lcd_clk_count = 0;

#endif  /* CONFIG_SHDISP_EXCLK_*** */

#endif /* CONFIG_SHDISP_PANEL_SWITCH */

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_host_ctl_lcd_clk_init                                              */
/* ------------------------------------------------------------------------- */

static int shdisp_host_ctl_lcd_clk_init()
{
    int ret = SHDISP_RESULT_SUCCESS;

#if defined(CONFIG_SHDISP_PANEL_SWITCH)
    SHDISP_DEBUG("in\n");

    if(shdisp_api_get_panel_info() != 0) {
        xo_handle = msm_xo_get(MSM_XO_TCXO_A1, "anonymous");
        if (IS_ERR(xo_handle)) {
            ret = PTR_ERR(xo_handle);
            SHDISP_ERR("%s not able to get the handle to vote for TCXO A1 buffer. ret=%d\n", __func__, ret);
            ret = SHDISP_RESULT_FAILURE;
        }
    }

#else  /* CONFIG_SHDISP_PANEL_SWITCH */

#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
    xo_handle = msm_xo_get(MSM_XO_TCXO_A1, "anonymous");
    if (IS_ERR(xo_handle)) {
        ret = PTR_ERR(xo_handle);
        SHDISP_ERR("%s not able to get the handle to vote for TCXO A1 buffer. ret=%d\n", __func__, ret);
        ret = SHDISP_RESULT_FAILURE;
    }
#elif defined(CONFIG_SHDISP_EXCLK_PMIC_D1)
    xo_handle = msm_xo_get(MSM_XO_TCXO_D1, "anonymous");
    if (IS_ERR(xo_handle)) {
        ret = PTR_ERR(xo_handle);
        SHDISP_ERR("%s not able to get the handle to vote for TCXO D1 buffer. ret=%d\n", __func__, ret);
        ret = SHDISP_RESULT_FAILURE;
    }
#endif  /* CONFIG_SHDISP_EXCLK_*** */

    SHDISP_DEBUG("out\n");

#endif /* CONFIG_SHDISP_PANEL_SWITCH */

    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_host_ctl_lcd_clk_exit                                              */
/* ------------------------------------------------------------------------- */

static int shdisp_host_ctl_lcd_clk_exit()
{
    int ret = SHDISP_RESULT_SUCCESS;

#if defined(CONFIG_SHDISP_PANEL_SWITCH)

#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1)
    if(shdisp_api_get_panel_info() != 0) {
        msm_xo_put(xo_handle);
    }
#endif /* CONFIG_SHDISP_EXCLK_PMIC_A1 */

#else  /* CONFIG_SHDISP_PANEL_SWITCH */

#if defined(CONFIG_SHDISP_EXCLK_PMIC_A1) || defined(CONFIG_SHDISP_EXCLK_PMIC_D1)
    msm_xo_put(xo_handle);
#endif  /* CONFIG_SHDISP_EXCLK_*** */

#endif /* CONFIG_SHDISP_PANEL_SWITCH */

    return ret;
}
#endif


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_i2c_probe                                                     */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_i2c_probe(struct i2c_client *client, const struct i2c_device_id * devid)
{
    bdic_i2c_data_t* i2c_p;

    if(bdic_i2c_p != NULL){
        return -EPERM;
    }

    i2c_p = (bdic_i2c_data_t*)kzalloc(sizeof(bdic_i2c_data_t),GFP_KERNEL);
    if(i2c_p == NULL){
        return -ENOMEM;
    }

    bdic_i2c_p = i2c_p;

    i2c_set_clientdata(client,i2c_p);
    i2c_p->this_client = client;

    return 0;
}


/* ------------------------------------------------------------------------- */
/* shdisp_bdic_i2c_remove                                                    */
/* ------------------------------------------------------------------------- */

static int shdisp_bdic_i2c_remove(struct i2c_client *client)
{
    bdic_i2c_data_t* i2c_p;

    i2c_p = i2c_get_clientdata(client);

    kfree(i2c_p);

    return 0;
}


/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sio_probe                                                 */
/* ------------------------------------------------------------------------- */

static int __devinit shdisp_clmr_sio_probe(struct spi_device *spi)
{
    spid=spi;

    if (spid) {
        SHDISP_DEBUG(": spid=%p\n", spid);
    } else {
        SHDISP_DEBUG(": spid=NULL\n");
    }
    return 0;
}


/* ------------------------------------------------------------------------- */
/* shdisp_clmr_sio_remove                                                */
/* ------------------------------------------------------------------------- */

static int __devinit shdisp_clmr_sio_remove(struct spi_device *spi)
{
    return 0;
}





#define SHDISP_CLMR_ARMINFOREG2     0x1948
#define SHDISP_CLMR_ARMINFOREG3     0x194C
#define SHDISP_CLMR_INFOREG0        0x140
#define SHDISP_CLMR_INFOREG3        0x14C
#define SHDISP_FW2HOST_SLEEP            1000
//#define SHDISP_FW2HOST_WAIT_COUNT       100
#define SHDISP_FW2HOST_WAIT_COUNT       500
/* ------------------------------------------------------------------------- */
/* shdisp_SYS_variable_length_data_read                                      */
/* ------------------------------------------------------------------------- */
int shdisp_SYS_variable_length_data_read(unsigned char cmdno, unsigned char* data, int size)
{
    int check;
    unsigned char info[4] = {0};
    unsigned char arminfo[8];
    int ret = 0;
    int i;
    int arminfobeginpos;
    int rest_size = size;

    do {
        check = shdisp_SYS_getInfoReg0_check(0);
        if ((check & 0xC0000000) == 0x40000000) {
            memset(arminfo, 0x00, sizeof(arminfo));
            shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_ARMINFOREG2, NULL, 0, &arminfo[0], 4);
            shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_ARMINFOREG3, NULL, 0, &arminfo[4], 4);

            SHDISP_DEBUG("ARMINFOREG2[0-3] = 0x%02x%02x%02x%02x\n", arminfo[0],arminfo[1],arminfo[2],arminfo[3]);
            SHDISP_DEBUG("ARMINFOREG3[4-7] = 0x%02x%02x%02x%02x\n", arminfo[4],arminfo[5],arminfo[6],arminfo[7]);

            if (cmdno == (check & 0x0000007F)) {

                if (rest_size > 4) {
                    i = 4;
                    arminfobeginpos = 0;
                }
                else {
                    i = rest_size;
                    arminfobeginpos = 4-i;
                }
                rest_size -= i;
                for (; i > 0; i--) {
                    *(data++) = arminfo[arminfobeginpos + i - 1];
                }

                if (rest_size > 4) {
                    i = 4;
                    arminfobeginpos = 4;
                }
                else {
                    i = rest_size;
                    arminfobeginpos = 4 + (4-i);
                }
                rest_size -= i;
                for (; i > 0; i--) {
                    *(data++) = arminfo[arminfobeginpos + i - 1];
                }
            }

            shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG3, NULL, 0, info, sizeof(info));
            info[0] = (info[0] & 0x3F) | 0x40;
            shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG3, info, sizeof(info), NULL, 0);

            check = shdisp_SYS_getInfoReg0_check(1);
            if ((check & 0xC0000000) == 0x80000000) {
                memset(arminfo, 0x00, sizeof(arminfo));
                shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_ARMINFOREG2, NULL, 0, &arminfo[0], 4);
                shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_ARMINFOREG3, NULL, 0, &arminfo[4], 4);

                SHDISP_DEBUG("ARMINFOREG2[0-3] = 0x%02x%02x%02x%02x\n", arminfo[0],arminfo[1],arminfo[2],arminfo[3]);
                SHDISP_DEBUG("ARMINFOREG3[4-7] = 0x%02x%02x%02x%02x\n", arminfo[4],arminfo[5],arminfo[6],arminfo[7]);

                if (cmdno == (check & 0x0000007F)) {

                    if ( rest_size > 4 ) {
                        i = 4;
                        arminfobeginpos = 0;
                    }
                    else {
                        i = rest_size;
                        arminfobeginpos = 4-i;
                    }
                    rest_size -= i;
                    for (; i > 0; i--) {
                        *(data++) = arminfo[arminfobeginpos + i - 1];
                    }

                    if( rest_size > 4 ){
                        i = 4;
                        arminfobeginpos = 4;
                    }
                    else {
                        i = rest_size;
                        arminfobeginpos = 4 + (4-i);
                    }
                    rest_size -= i;
                    for (; i > 0; i--) {
                        *(data++) = arminfo[arminfobeginpos + i - 1];
                    }
                }

                shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG3, NULL, 0, info, sizeof(info));
                info[0] = (info[0] & 0x3F) | 0x80;
                shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG3, info, sizeof(info), NULL, 0);
            }
            else if ((check & 0xC0000000) == 0xC0000000) {
                goto host_end;
            }
            else {
                ret = -1;
                goto timeout_end;
            }
        }
        else if((check & 0xC0000000) == 0xC0000000) {
            goto host_end;
        }
        else {
            ret = -1;
            goto timeout_end;
        }
    } while(1);

host_end:
    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG3, NULL, 0, info, sizeof(info));
    info[0] = (info[0] & 0x3F) | 0xC0;
    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG3, info, sizeof(info), NULL, 0);

    check = shdisp_SYS_getInfoReg0_check(2);
    if ((check & 0xC0000000) != 0x00000000) {
        ret = -1;
        goto timeout_end;
    }

    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG3, NULL, 0, info, sizeof(info));
    info[0] = (info[0] & 0x3F) | 0x00;
    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG3, info, sizeof(info), NULL, 0);

    ret = check;

timeout_end:
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_getInfoReg0_check                                              */
/* ------------------------------------------------------------------------- */
static int shdisp_SYS_getInfoReg0_check(int mode)
{
    int i;
    unsigned char info[4] = {0};

    for (i = 0; i < SHDISP_FW2HOST_WAIT_COUNT; i++) {
        info[0] = 0x00;
        shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG0, NULL, 0, info, sizeof(info));
        if((mode == 0) && (((info[0] & 0xC0) == 0xC0) || ((info[0] & 0xC0) == 0x40))) {
            break;
        }
        else if((mode == 1) && (((info[0] & 0xC0) == 0xC0) || ((info[0] & 0xC0) == 0x80))) {
            break;
        }
        else if((mode == 2) && ((info[0] & 0xC0) == 0x00)) {
            break;
        }
        else {
            shdisp_SYS_delay_us(SHDISP_FW2HOST_SLEEP);
        }
    }

    if (i >= SHDISP_FW2HOST_WAIT_COUNT) {
        SHDISP_ERR(":[%d] timeout!!! mode=%d info[0]=0x%x\n", __LINE__, mode, info[0]);
    }
    else {
        SHDISP_DEBUG("mode = %d count = %d data = 0x%02x%02x%02x%02x\n", mode, i, info[0], info[1], info[2], info[3]);
    }

    return (info[0] << 24) | (info[1] << 16) | (info[2] << 8) | info[3];
}

/* ------------------------------------------------------------------------- */
/* shdisp_SYS_getInfoReg2                                              */
/* ------------------------------------------------------------------------- */
int shdisp_SYS_getInfoReg2(void)
{
    unsigned char info[4] = {0};
    int ado, ado_tbl, ado_cnd;

    shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_INFOREG2, NULL, 0, info, sizeof(info));

    ado = ((info[2] << 8) | info[3]);

    ado_tbl = (info[1] & 0x1F);

    ado_cnd = (info[1] & 0xE0);

    SHDISP_DEBUG("data = 0x%02x%02x%02x%02x\n", info[0], info[1], info[2], info[3]);
    SHDISP_DEBUG("ADO = %x\n", ado);
    SHDISP_DEBUG("ADO_TBL = %x\n", ado_tbl);
    SHDISP_DEBUG("ADO_CND = %x\n", ado_cnd);

    return (info[0] << 24) | (info[1] << 16) | (info[2] << 8) | info[3];
}

void shdisp_SYS_bdic_i2c_set_api(unsigned char apino)
{
    shdisp_FWCMD_buf_set_nokick(1);

    shdisp_FWCMD_buf_init(apino);

    return;
}

int shdisp_SYS_bdic_i2c_doKick_if_exist(void)
{
    int result = SHDISP_RESULT_SUCCESS;

    if (!shdisp_FWCMD_buf_is_empty()) {
        result = shdisp_FWCMD_buf_finish();
        if (result == SHDISP_RESULT_SUCCESS) {
            result = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    else {
        SHDISP_DEBUG("buf_cmd_is_empty\n");
    }

    shdisp_FWCMD_buf_set_nokick(0);

    return result;
}

void shdisp_SYS_bdic_i2c_cmd_reset()
{
    if (!shdisp_FWCMD_buf_is_empty()) {
        SHDISP_DEBUG("buf_cmd_is_exist!!!!!!!!\n");
    }

    shdisp_FWCMD_buf_set_nokick(0);

    shdisp_FWCMD_buf_init(0);

    return;
}

int shdisp_FWCMD_buf_is_empty(void)
{
    struct shdisp_fwcmd_buffer * const buf = &shdisp_fwcmd_buf;

    return (buf->pos) ? 0 : 1;
}

void shdisp_FWCMD_buf_set_nokick(int on)
{
    fmcmd_nokick = on;
}
int shdisp_FWCMD_buf_get_nokick(void)
{
    return fmcmd_nokick;
}

/* ------------------------------------------------------------------------- */
/* shdisp_FWCMD_set_apino                                                    */
/* ------------------------------------------------------------------------- */
char shdisp_FWCMD_set_apino(unsigned char apino) {
    const char oldAPINo = fmcmd_api;

    if( (apino >= SHDISP_CLMR_FWCMD_APINO_MIN )
    &&  (apino <= SHDISP_CLMR_FWCMD_APINO_MAX)
    ){
        shdisp_FWCMD_buf_init(apino);
        fmcmd_api = apino;
        shdisp_FWCMD_buf_set_nokick(apino != SHDISP_CLMR_FWCMD_APINO_NOTHING);
    }
    else {
        SHDISP_DEBUG("%s apino =0x%02x error\n", __func__, apino);
    }

    return oldAPINo;
}

/* ------------------------------------------------------------------------- */
/* shdisp_FWCMD_safe_finishanddoKick                                         */
/* ------------------------------------------------------------------------- */
int shdisp_FWCMD_safe_finishanddoKick( void ) {
    int result;
    if( (fmcmd_nokick)
    &&  (!shdisp_FWCMD_buf_is_empty())
    ){
        result = shdisp_FWCMD_buf_finish();
        if (result == SHDISP_RESULT_SUCCESS) {
            result = shdisp_FWCMD_doKick(1, 0, NULL);
        }
    }
    else {
        result = SHDISP_RESULT_SUCCESS;
        SHDISP_DEBUG("%s buffer is empty end. \n", __func__);
    }
    return result;
}

/* ------------------------------------------------------------------------- */
/* shdisp_FWCMD_buf_init                                                     */
/* ------------------------------------------------------------------------- */
void shdisp_FWCMD_buf_init(unsigned char apino)
{
    struct shdisp_fwcmd_buffer * const buf = &shdisp_fwcmd_buf;

    if (buf->pos) {
        SHDISP_DEBUG( "shdisp_FWCMD_buf_init. error! buffer is not empty.\n" );
#if 1
        {
            unsigned int pos = 0;
            const unsigned int posEnd = buf->pos;
            do {
                SHDISP_DEBUG( "pos = %d: 16bit val(0x) = "
                        "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
                        pos, buf->buf[pos], buf->buf[pos+1], buf->buf[pos+2], buf->buf[pos+3],
                             buf->buf[pos+4], buf->buf[pos+5], buf->buf[pos+6], buf->buf[pos+7],
                             buf->buf[pos+8], buf->buf[pos+9], buf->buf[pos+10], buf->buf[pos+11],
                             buf->buf[pos+12], buf->buf[pos+13], buf->buf[pos+14], buf->buf[pos+15]);
            } while( (pos+=16) <= posEnd );
        }
#endif
    }

    buf->pos = 0;
    buf->lastINTnopos = 0;
    fmcmd_api = apino;

    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_FWCMD_buf_add                                                      */
/* ------------------------------------------------------------------------- */
int shdisp_FWCMD_buf_add( unsigned char code, unsigned short len, unsigned char * data )
{
    struct shdisp_fwcmd_buffer * const buf = &shdisp_fwcmd_buf;
    if( (buf->pos + len + 3) > SHDISP_SYS_I2C_FWCMD_BUF_LEN ){
        SHDISP_ERR( "shdisp_FWCMD_buf_add. error! buffer filled.\n" );
        return SHDISP_RESULT_FAILURE;
    }

    buf->buf[buf->pos] = code;
    buf->buf[buf->pos+1] = 0x7F & (fmcmd_api + fmcmd_kickcnt);
    (*(unsigned short*)&(buf->buf[buf->pos+2])) = len;

    if (len != 0 && data != NULL) {
        memcpy(&buf->buf[buf->pos+4], data, len);
    }

    buf->lastINTnopos = buf->pos + 1;
    buf->pos += (2 + 2 + len);

    fmcmd_kickcnt++;
    if (16 <= fmcmd_kickcnt) {
        fmcmd_kickcnt = 1;
    }

    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_FWCMD_buf_add_multi                                                */
/* ------------------------------------------------------------------------- */
int shdisp_FWCMD_buf_add_multi( unsigned char code, unsigned char slvadr, unsigned char addr, unsigned short len, unsigned char * data )
{
    struct shdisp_fwcmd_buffer * const buf = &shdisp_fwcmd_buf;
    if( (buf->pos + 1 + len + 3) > SHDISP_SYS_I2C_FWCMD_BUF_LEN ){
        SHDISP_ERR( "shdisp_FWCMD_buf_add_multi. error! buffer filled.\n" );
        return SHDISP_RESULT_FAILURE;
    }

    buf->buf[buf->pos] = code;
    buf->buf[buf->pos+1] = 0x7F & (fmcmd_api + fmcmd_kickcnt);
    (*(unsigned short*)&(buf->buf[buf->pos+2])) = len+2;

    buf->buf[buf->pos + 4] = slvadr;
    buf->buf[buf->pos + 5] = addr;
    memcpy(&buf->buf[buf->pos+6], data, len);

    buf->lastINTnopos = buf->pos + 1;
    buf->pos += (6 + len);

    fmcmd_kickcnt++;
    if (16 <= fmcmd_kickcnt) {
        fmcmd_kickcnt = 1;
    }

    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_FWCMD_buf_finish                                                   */
/* ------------------------------------------------------------------------- */
int shdisp_FWCMD_buf_finish( void )
{
    struct shdisp_fwcmd_buffer * const buf = &shdisp_fwcmd_buf;
    unsigned char dmycnt = ((buf->pos+(2-1)) ^ 0x0F) & 0x0F;

    if( buf->pos == 0 ){
        SHDISP_ERR( "shdisp_FWCMD_buf_finish. error fwcmd empty!\n" );
        return SHDISP_RESULT_FAILURE;
    }

    buf->buf[buf->pos]   = 0xFE;
    buf->buf[buf->pos+1] = 0x0F;

    buf->pos+=2;
    if( dmycnt != 0 ){
        memset(&buf->buf[buf->pos], 0xFF, dmycnt);
        buf->pos += dmycnt;
    }

    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_FWCMD_dump                                                         */
/* ------------------------------------------------------------------------- */
void shdisp_FWCMD_dump( unsigned char * str, unsigned char * begin, unsigned short len )
{
#ifdef SHDISP_LOG_ENABLE
    unsigned char * buf = begin;
    unsigned char * bufEnd = begin + len;
    unsigned int pos = 0;;

    if( !buf ) {
        SHDISP_DEBUG( "str=%s, but begin == NULL end\n", str );
        return;
    }

    do {
        SHDISP_DEBUG( "%s pos = %d: 16bit val(0x) = "
                      "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
                     str, pos,
                     *buf    , *(buf+1), *(buf+ 2), *(buf+ 3), *(buf+ 4), *(buf+ 5), *(buf+ 6), *(buf+ 7),
                     *(buf+8), *(buf+9), *(buf+10), *(buf+11), *(buf+12), *(buf+13), *(buf+14), *(buf+15) );
        pos += 16;
    } while( (buf+=16) < bufEnd );
#endif /* SHDISP_LOG_ENABLE */
}


/* ------------------------------------------------------------------------- */
/* shdisp_FWCMD_doKick                                                       */
/* ------------------------------------------------------------------------- */
int shdisp_FWCMD_doKick(unsigned char notice, unsigned int len, unsigned char * rtnbuf )
{
    int result = SHDISP_RESULT_SUCCESS;
    struct shdisp_fwcmd_buffer * const buf = &shdisp_fwcmd_buf;
    const unsigned int posEnd = buf->pos;
    const unsigned char data[4] = { 0x00, 0x00, 0x00, 0x01 };
    unsigned char cmdno;
#ifdef SHDISP_LOG_ENABLE
    char * eDramReadbuf = 0;
#endif

    if( posEnd == 0 ){
        SHDISP_ERR( "shdisp_FWCMD_doKick. error fwcmd empty!\n" );
        return SHDISP_RESULT_FAILURE;
    }

    if( shdisp_pm_is_clmr_on() != SHDISP_DEV_STATE_ON ){
        SHDISP_ERR( "clmr stoped!\n" );
        shdisp_FWCMD_dump( "powroff_doKick err", buf->buf, posEnd );
        if( (rtnbuf != NULL) && (len != 0) ){
            memset( rtnbuf, 0, len );
        }
        result = SHDISP_RESULT_SUCCESS;
        goto error_end;
    }

    if( shdisp_SYS_FWCMD_istimeoutexception() ){
        SHDISP_DEBUG(": FW timeout...\n");
        result = SHDISP_RESULT_SUCCESS;
        goto error_end;
    }
#ifdef SHDISP_LOG_ENABLE
    if( shdisp_FWCMD_doKickAndeDramReadFlag ){
        eDramReadbuf = kzalloc(buf->pos, GFP_KERNEL);
        if( eDramReadbuf ){
            notice = 1;
            shdisp_FWCMD_eDramPtrReset();
        }
        else {
            SHDISP_DEBUG( "shdisp_FWCMD_doKick. eDramReadbuf alloc failed!\n" );
        }
    }
#endif

    cmdno = buf->buf[buf->lastINTnopos];
    buf->buf[buf->lastINTnopos] |= (notice) ? 0x80 : 0x00;


    shdisp_FWCMD_dump( "doKick", buf->buf, posEnd );

    result = shdisp_SYS_clmr_sio_eDram_transfer(2, buf->buf, buf->pos);
    if (result != SHDISP_RESULT_SUCCESS) {
        goto error_end;
    }

    if (notice) {
        shdisp_clmr_api_prepare_handshake(cmdno, rtnbuf, len);
    }

    result = shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_ARMSETINT1, (unsigned char*)data, 4, NULL, 0);
    if (result != SHDISP_RESULT_SUCCESS) {
        goto error_end;
    }

    if (notice) {
        result = shdisp_clmr_api_wait4fw_cmd_comp(cmdno);
        if (result != SHDISP_RESULT_SUCCESS){
            SHDISP_ERR(": FW timeout...\n");
            shdisp_SYS_FWCMD_set_timeoutexception(1);
            result= SHDISP_RESULT_SUCCESS;
            goto error_end;
        }

        if( shdisp_clmr_api_get_handhake_error() ){
            result = SHDISP_RESULT_FAILURE;
        }
    }
#if 0
    else {
        unsigned char info[4] = {0};

        shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_INFOREG0,
                                        NULL, 0, info, sizeof(info));
        SHDISP_DEBUG("data = 0x%02x%02x%02x%02x\n",
                    info[0], info[1], info[2], info[3]);
    }
#endif

#ifdef SHDISP_LOG_ENABLE
    if( ( shdisp_FWCMD_doKickAndeDramReadFlag )
    &&  ( eDramReadbuf )
    ){
        int eDramcompare;
        shdisp_SYS_clmr_sio_eDram_transfer(1, eDramReadbuf, buf->pos);
        eDramcompare = memcmp(buf->buf, eDramReadbuf, buf->pos );
        SHDISP_DEBUG( "dokickAndeDramRead compare=%d\n", eDramcompare );
        shdisp_FWCMD_dump( "dokickAndeDramRead", eDramReadbuf, posEnd );
        kfree(eDramReadbuf);
        shdisp_FWCMD_eDramPtrReset();
    }
#endif

error_end:
    buf->pos = 0;
    buf->lastINTnopos = 0;

    return result;
}


/*---------------------------------------------------------------------------*/
/*      shdisp_FWCMD_eDramPtrReset                                           */
/*---------------------------------------------------------------------------*/
int shdisp_FWCMD_eDramPtrReset(void)
{
    unsigned char ptrrst[4] =  { 0x00, 0x00, 0x00, 0x04 };
    int rtn;
    
    shdisp_clmr_api_eDramPtr_rst_start();
    rtn = shdisp_SYS_clmr_sio_transfer(SHDISP_CLMR_REG_ARMSETINT1, &ptrrst[0], sizeof(ptrrst), NULL, 0);
    if( rtn == SHDISP_RESULT_FAILURE ) {
        SHDISP_ERR("ptrrst:[%d] failed. \n", __LINE__ );
        return SHDISP_RESULT_FAILURE;
    }

    rtn = shdisp_clmr_api_wait4eDramPtr_rst_comp();
    if( rtn == SHDISP_RESULT_FAILURE ) {
        SHDISP_ERR("timeout:[%d] \n", __LINE__);
        return SHDISP_RESULT_FAILURE;
    }
    return SHDISP_RESULT_SUCCESS;
}


void shdisp_FWCMD_doKickAndeDramReadFlag_OnOff(int onoff)
{
    shdisp_FWCMD_doKickAndeDramReadFlag = onoff;
}






#ifdef SHDISP_SYS_SW_TIME_API
/* ------------------------------------------------------------------------- */
/* shdisp_sys_dbg_hw_check_start                                             */
/* ------------------------------------------------------------------------- */

void shdisp_sys_dbg_hw_check_start(void)
{
    memset(&shdisp_sys_dbg_api, 0, sizeof(shdisp_sys_dbg_api));

    SHDISP_SYS_DBG_DBIC_INIT;

    shdisp_sys_dbg_api.flag = 1;

    getnstimeofday(&shdisp_sys_dbg_api.t_api_start);
}

/* ------------------------------------------------------------------------- */
/* shdisp_sys_dbg_hw_check_end                                               */
/* ------------------------------------------------------------------------- */

void shdisp_sys_dbg_hw_check_end(const char *func)
{
    struct timespec stop, df;
    u64 msec_api, msec_req, msec_sum;
    u64 usec_api, usec_req, usec_sum;

    getnstimeofday(&stop);

    df = timespec_sub(stop, shdisp_sys_dbg_api.t_api_start);

    msec_api = timespec_to_ns(&df);
    do_div(msec_api, NSEC_PER_USEC);
    usec_api = do_div(msec_api, USEC_PER_MSEC);

    msec_req = timespec_to_ns(&shdisp_sys_dbg_api.t_wait_req);
    do_div(msec_req, NSEC_PER_USEC);
    usec_req = do_div(msec_req, USEC_PER_MSEC);

    msec_sum = timespec_to_ns(&shdisp_sys_dbg_api.t_wait_sum);
    do_div(msec_sum, NSEC_PER_USEC);
    usec_sum = do_div(msec_sum, USEC_PER_MSEC);

    SHDISP_DEBUG("[API]() total=%lu.%03lums, wait=%lu.%03lums( %lu.%03lums )\n",
    (unsigned long)msec_api, (unsigned long)usec_api,
    (unsigned long)msec_sum, (unsigned long)usec_sum,
    (unsigned long)msec_req, (unsigned long)usec_req );

    SHDISP_SYS_DBG_DBIC_LOGOUT;

    shdisp_sys_dbg_api.flag = 0;
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_api_wait_start                                                 */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_api_wait_start(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&shdisp_sys_dbg_api.t_wait_start);
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_api_wait_end                                                   */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_api_wait_end(unsigned long usec)
{
    struct timespec stop, df;

    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&stop);

    df = timespec_sub(stop, shdisp_sys_dbg_api.t_wait_start);

    shdisp_sys_dbg_api.t_wait_sum = timespec_add(shdisp_sys_dbg_api.t_wait_sum, df);

    timespec_add_ns(&shdisp_sys_dbg_api.t_wait_req, (usec * 1000));
}

#ifdef SHDISP_SYS_SW_TIME_BDIC
/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_init                                                      */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_init(void)
{
    memset(&shdisp_sys_dbg_bdic, 0, sizeof(shdisp_sys_dbg_bdic));
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_logout                                                    */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_logout(void)
{
    u64 nsec_wk;
    unsigned long usec_wk1, usec_avl;

    nsec_wk = timespec_to_ns(&shdisp_sys_dbg_bdic.w_singl_t_sum);
    if(nsec_wk != 0) {
        do_div(nsec_wk, NSEC_PER_USEC);
        usec_wk1 = nsec_wk;
        usec_avl = usec_wk1 / shdisp_sys_dbg_bdic.w_singl_ok_count;
        SHDISP_DEBUG("[---] -- bdic w_s %lu,%lu,%lu, total=%lu.%03lums, avl=%lu.%03lums\n",
        shdisp_sys_dbg_bdic.w_singl_ok_count, shdisp_sys_dbg_bdic.w_singl_ng_count, shdisp_sys_dbg_bdic.w_singl_retry,
        usec_wk1/USEC_PER_MSEC, usec_wk1%USEC_PER_MSEC, usec_avl/USEC_PER_MSEC, usec_avl%USEC_PER_MSEC);
    }

    nsec_wk = timespec_to_ns(&shdisp_sys_dbg_bdic.r_singl_t_sum);
    if(nsec_wk != 0) {
        do_div(nsec_wk, NSEC_PER_USEC);
        usec_wk1 = nsec_wk;
        usec_avl = usec_wk1 / shdisp_sys_dbg_bdic.r_singl_ok_count;
        SHDISP_DEBUG("[---] -- bdic r_s %lu,%lu,%lu, total=%lu.%03lums, avl=%lu.%03lums\n",
        shdisp_sys_dbg_bdic.r_singl_ok_count, shdisp_sys_dbg_bdic.r_singl_ng_count, shdisp_sys_dbg_bdic.r_singl_retry,
        usec_wk1/USEC_PER_MSEC, usec_wk1%USEC_PER_MSEC, usec_avl/USEC_PER_MSEC, usec_avl%USEC_PER_MSEC);
    }

    nsec_wk = timespec_to_ns(&shdisp_sys_dbg_bdic.r_multi_t_sum);
    if(nsec_wk != 0) {
        do_div(nsec_wk, NSEC_PER_USEC);
        usec_wk1 = nsec_wk;
        usec_avl = usec_wk1 / shdisp_sys_dbg_bdic.r_multi_ok_count;
        SHDISP_DEBUG("[---] -- bdic r_m %lu,%lu,%lu, total=%lu.%03lums, avl=%lu.%03lums\n",
        shdisp_sys_dbg_bdic.r_multi_ok_count, shdisp_sys_dbg_bdic.r_multi_ng_count, shdisp_sys_dbg_bdic.r_multi_retry,
        usec_wk1/USEC_PER_MSEC, usec_wk1%USEC_PER_MSEC, usec_avl/USEC_PER_MSEC, usec_avl%USEC_PER_MSEC);
    }
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_singl_write_start                                         */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_singl_write_start(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&shdisp_sys_dbg_bdic.w_singl_t_start);
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_singl_write_retry                                         */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_singl_write_retry(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    shdisp_sys_dbg_bdic.w_singl_retry++;
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_singl_write_end                                           */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_singl_write_end(int ret)
{
    struct timespec stop, df;

    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&stop);

    df = timespec_sub(stop, shdisp_sys_dbg_bdic.w_singl_t_start);

    shdisp_sys_dbg_bdic.w_singl_t_sum = timespec_add(shdisp_sys_dbg_bdic.w_singl_t_sum, df);

    if ( ret == 0 ) {
        shdisp_sys_dbg_bdic.w_singl_ok_count++;
    }
    else {
        shdisp_sys_dbg_bdic.w_singl_ng_count++;
    }
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_singl_read_start                                          */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_singl_read_start(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&shdisp_sys_dbg_bdic.r_singl_t_start);
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_singl_read_retry                                          */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_singl_read_retry(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    shdisp_sys_dbg_bdic.r_singl_retry++;
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_singl_read_end                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_singl_read_end(int ret)
{
    struct timespec stop, df;

    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&stop);

    df = timespec_sub(stop, shdisp_sys_dbg_bdic.r_singl_t_start);

    shdisp_sys_dbg_bdic.r_singl_t_sum = timespec_add(shdisp_sys_dbg_bdic.r_singl_t_sum, df);

    if ( ret == 0 ) {
        shdisp_sys_dbg_bdic.r_singl_ok_count++;
    }
    else {
        shdisp_sys_dbg_bdic.r_singl_ng_count++;
    }
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_multi_read_start                                          */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_multi_read_start(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&shdisp_sys_dbg_bdic.r_multi_t_start);
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_multi_read_retry                                          */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_multi_read_retry(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    shdisp_sys_dbg_bdic.r_multi_retry++;
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_multi_read_end                                            */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_multi_read_end(int ret)
{
    struct timespec stop, df;

    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&stop);

    df = timespec_sub(stop, shdisp_sys_dbg_bdic.r_multi_t_start);

    shdisp_sys_dbg_bdic.r_multi_t_sum = timespec_add(shdisp_sys_dbg_bdic.r_multi_t_sum, df);

    if ( ret == 0 ) {
        shdisp_sys_dbg_bdic.r_multi_ok_count++;
    }
    else {
        shdisp_sys_dbg_bdic.r_multi_ng_count++;
    }
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_multi_write_start                                         */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_multi_write_start(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&shdisp_sys_dbg_bdic.w_multi_t_start);
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_multi_write_retry                                         */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_multi_write_retry(void)
{
    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    shdisp_sys_dbg_bdic.w_multi_retry++;
}

/* ------------------------------------------------------------------------- */
/* shdisp_dbg_bdic_multi_write_end                                           */
/* ------------------------------------------------------------------------- */

static void shdisp_dbg_bdic_multi_write_end(int ret)
{
    struct timespec stop, df;

    if(shdisp_sys_dbg_api.flag == 0) {
        return;
    }

    getnstimeofday(&stop);

    df = timespec_sub(stop, shdisp_sys_dbg_bdic.w_multi_t_start);

    shdisp_sys_dbg_bdic.w_multi_t_sum = timespec_add(shdisp_sys_dbg_bdic.w_multi_t_sum, df);

    if ( ret == 0 ) {
        shdisp_sys_dbg_bdic.w_multi_ok_count++;
    }
    else {
        shdisp_sys_dbg_bdic.w_multi_ng_count++;
    }
}

#endif /* SHDISP_SYS_SW_TIME_BDIC */
#endif /* SHDISP_SYS_SW_TIME_API */


static int istimeout = 0;
void shdisp_SYS_FWCMD_set_timeoutexception(int value)
{
    istimeout = value;
    SHDISP_DEBUG("value=%d \n", value);
}

int shdisp_SYS_FWCMD_istimeoutexception(void)
{
    if( istimeout ){
        SHDISP_DEBUG("istimeout=%d \n", istimeout );
    }
    return istimeout;
}

MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
