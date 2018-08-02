/* drivers/sharp/shdisp_no_lcdc/shdisp_bd2802gu.c  (Display Driver)
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
#include <sharp/shdisp_kerl.h>
#include "shdisp_bd2802gu.h"
#include "shdisp_system.h"
#if defined(CONFIG_MACH_LYNX_DL35)
#include "./data/shdisp_bd2802gu_data_dl35.h"
#else /* CONFIG_MACH_DEFAULT */
#include "./data/shdisp_bd2802gu_data_default.h"
#endif /* CONFIG_MACH_ */
#include "shdisp_dbg.h"

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */
//#define SHDISP_LCDC_SW_REG_BKUP
//#define SHDISP_LCDC_SW_I2C_WLOG

#define SHDISP_BD2802GU_FILE "shdisp_bd2802gu.c"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define SHDISP_LEDC_REG_MAX             0x16

#define SHDISP_LEDC_RET_ERR_NULL        -1
#define SHDISP_LEDC_RET_ERR_NOIC        -2

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_ledc_LD_api_check(void);
static int shdisp_ledc_LD_power_on(void);
static int shdisp_ledc_LD_power_off(void);
static void shdisp_ledc_LD_set_color(struct shdisp_ledc_req *ledc_req);
static void shdisp_ledc_PD_init(void);
static int shdisp_ledc_PD_power_on(void);
static int shdisp_ledc_PD_power_off(void);
static void shdisp_ledc_PD_LED_off(void);
static void shdisp_ledc_PD_set_rgb(struct shdisp_ledc_rgb *ledc_rgb);
static void shdisp_ledc_PD_LED_on_color(unsigned char color1, unsigned char color2, int mode, int count);
static int shdisp_ledc_IO_write_reg(unsigned char reg, unsigned char val);
static int shdisp_ledc_IO_read_reg(unsigned char reg, unsigned char *val);

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static struct shdisp_ledc_status *plcdc_info = NULL;
#ifdef SHDISP_LCDC_SW_REG_BKUP
static unsigned char shdisp_ledc_reg[SHDISP_LEDC_REG_MAX];
#endif
static int ledc_clrvari_index = 0x00;

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_set_status_addr                                           */
/* ------------------------------------------------------------------------- */

void shdisp_ledc_API_set_status_addr(struct shdisp_ledc_status* state_str)
{
    plcdc_info = state_str;
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_init                                                      */
/* ------------------------------------------------------------------------- */

int shdisp_ledc_API_init(void)
{
    int ret;
    
    shdisp_ledc_PD_init();
    
    ret = shdisp_SYS_ledc_i2c_init();
    if(ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }
    
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_exit                                                      */
/* ------------------------------------------------------------------------- */

int shdisp_ledc_API_exit(void)
{
    int ret;
    
    ret = shdisp_SYS_ledc_i2c_exit();
    if(ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }
    
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_power_on                                                  */
/* ------------------------------------------------------------------------- */

int shdisp_ledc_API_power_on(void)
{
    int ret;
    
    ret = shdisp_ledc_LD_api_check();
    if(ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<OTHER> status error(%d).\n", ret);
    }
    
    if(plcdc_info->power_status != SHDISP_LEDC_PWR_STATUS_OFF) {
        SHDISP_ERR("<OTHER> power status error(%d).\n", plcdc_info->power_status);
        return SHDISP_RESULT_FAILURE;
    }
    
    return shdisp_ledc_LD_power_on();
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_power_off                                                 */
/* ------------------------------------------------------------------------- */

int shdisp_ledc_API_power_off(void)
{
    int ret;
    
    ret = shdisp_ledc_LD_api_check();
    if(ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<OTHER> status error(%d).\n", ret);
    }
    
    if(plcdc_info->power_status != SHDISP_LEDC_PWR_STATUS_ON) {
        SHDISP_ERR("<OTHER> power status error(%d).\n", plcdc_info->power_status);
        return SHDISP_RESULT_FAILURE;
    }
    
    return shdisp_ledc_LD_power_off();
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_set_rgb                                                   */
/* ------------------------------------------------------------------------- */

void shdisp_ledc_API_set_rgb(struct shdisp_ledc_rgb *ledc_rgb)
{
    int ret;
    
    ret = shdisp_ledc_LD_api_check();
    if(ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<OTHER> status error(%d).\n", ret);
    }
    
    shdisp_ledc_PD_set_rgb(ledc_rgb);
    
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_set_color                                                 */
/* ------------------------------------------------------------------------- */

void shdisp_ledc_API_set_color(struct shdisp_ledc_req *ledc_req)
{
    int ret;
    
    ret = shdisp_ledc_LD_api_check();
    if(ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<OTHER> status error(%d).\n", ret);
    }
    
    if((plcdc_info->ledc_req.red[0] == ledc_req->red[0])
    && (plcdc_info->ledc_req.red[1] == ledc_req->red[1])
    && (plcdc_info->ledc_req.green[0] == ledc_req->green[0])
    && (plcdc_info->ledc_req.green[1] == ledc_req->green[1])
    && (plcdc_info->ledc_req.blue[0] == ledc_req->blue[0])
    && (plcdc_info->ledc_req.blue[1] == ledc_req->blue[1])
    && (plcdc_info->ledc_req.led_mode == ledc_req->led_mode)
    && (plcdc_info->ledc_req.on_count == ledc_req->on_count)) {
        return;
    }
    
    shdisp_ledc_LD_set_color(ledc_req);
    
    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_set_clrvari_index                                         */
/* ------------------------------------------------------------------------- */

int  shdisp_ledc_API_set_clrvari_index( int clrvari )
{
    int i=0;
    
    for( i=0; i<SHDISP_LEDC_COL_VARI_KIND ; i++) {
        if( (int)shdisp_ledc_clrvari_index[i] == clrvari )
            break;
    }
    if( i >= SHDISP_LEDC_COL_VARI_KIND )
        i = 0;
        
    ledc_clrvari_index = i;
    
    return ledc_clrvari_index;
}

/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_get_color_index_and_reedit                                */
/* ------------------------------------------------------------------------- */

unsigned char shdisp_ledc_API_get_color_index_and_reedit(struct shdisp_ledc_req *ledc_req, int ledidx)
{
    unsigned char color=0xFF;
    int idx;
    SHDISP_TRACE("shdisp_ledc_API_get_color_index_and_reedit:Start \n");
    
    SHDISP_TRACE("shdisp_ledc_API_get_color_index_and_reedit:IDX[0x%02x]", ledidx);
    SHDISP_TRACE("shdisp_ledc_API_get_color_index_and_reedit:R[0x%02x], G[0x%02x], B[0x%02x]\n", 
        (unsigned int)ledc_req->red[ledidx],
        (unsigned int)ledc_req->green[ledidx],
        (unsigned int)ledc_req->blue[ledidx]);
    
    for(idx=0; idx<SHDISP_LEDC_COLOR_KIND; idx ++) {
        if(( shdisp_ledc_color_index_tbl[idx][SHDISP_LEDC_COLOR_TBL_INDEX_R] == ledc_req->red[ledidx] ) &&
           ( shdisp_ledc_color_index_tbl[idx][SHDISP_LEDC_COLOR_TBL_INDEX_G] == ledc_req->green[ledidx] ) &&
           ( shdisp_ledc_color_index_tbl[idx][SHDISP_LEDC_COLOR_TBL_INDEX_B] == ledc_req->blue[ledidx] )){
           color = (unsigned char)shdisp_ledc_color_index_tbl[idx][SHDISP_LEDC_COLOR_TBL_INDEX_COLOR];
           SHDISP_TRACE("shdisp_ledc_API_get_color_index_and_reedit:Color setting1[0x%02x]", color);
           break;
        }
    }
    
    if( color == 0xFF ){
        if( ledc_req->red[ledidx] > 1) {
            ledc_req->red[ledidx] = 1;
        }
        if( ledc_req->green[ledidx] > 1) {
            ledc_req->green[ledidx] = 1;
        }
        if( ledc_req->blue[ledidx] > 1) {
            ledc_req->blue[ledidx] = 1;
        }
        for(idx=0; idx<SHDISP_LEDC_COLOR_KIND; idx ++) {
            if(( shdisp_ledc_color_index_tbl[idx][SHDISP_LEDC_COLOR_TBL_INDEX_R] == ledc_req->red[ledidx] ) &&
               ( shdisp_ledc_color_index_tbl[idx][SHDISP_LEDC_COLOR_TBL_INDEX_G] == ledc_req->green[ledidx] ) &&
               ( shdisp_ledc_color_index_tbl[idx][SHDISP_LEDC_COLOR_TBL_INDEX_B] == ledc_req->blue[ledidx] )){
               color = (unsigned char)shdisp_ledc_color_index_tbl[idx][SHDISP_LEDC_COLOR_TBL_INDEX_COLOR];
               SHDISP_TRACE("shdisp_ledc_API_get_color_index_and_reedit:Color Setting2[0x%02x]", color);
               break;
            }
        }
        if( color == 0xFF ) {
            color = SHDISP_LEDC_COLOR_NONE;
            SHDISP_TRACE("shdisp_ledc_API_get_color_index_and_reedit:Color None[0x%02x]", color);
        }
    }
    SHDISP_TRACE("shdisp_ledc_API_get_color_index_and_reedit:End \n");
    return color;
}

/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_DIAG_write_reg                                            */
/* ------------------------------------------------------------------------- */

void shdisp_ledc_API_DIAG_write_reg(unsigned char reg, unsigned char val)
{
    int ret;
    
    ret = shdisp_ledc_LD_api_check();
    if(ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<OTHER> status error(%d).\n", ret);
    }
    
    if(plcdc_info->power_status != SHDISP_LEDC_PWR_STATUS_ON) {
        SHDISP_ERR("<OTHER> power status error(%d).\n", plcdc_info->power_status);
        return;
    }
    
    shdisp_ledc_IO_write_reg(reg, val);
    
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_API_DIAG_read_reg                                             */
/* ------------------------------------------------------------------------- */

void shdisp_ledc_API_DIAG_read_reg(unsigned char reg, unsigned char *val)
{
    int ret;
    
    ret = shdisp_ledc_LD_api_check();
    if(ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<OTHER> status error(%d).\n", ret);
    }
    
    if(plcdc_info->power_status != SHDISP_LEDC_PWR_STATUS_ON) {
        SHDISP_ERR("<OTHER> power status error(%d).\n", plcdc_info->power_status);
        return;
    }
    
    shdisp_ledc_IO_read_reg(reg, val);
    
    return;
}


/* ------------------------------------------------------------------------- */
/* Logical Driver                                                            */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_ledc_LD_api_check                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_ledc_LD_api_check(void)
{
    if(plcdc_info == NULL) {
        return SHDISP_LEDC_RET_ERR_NULL;
    }
    
#if 0
    if(plcdc_info->ledc_is_exist != SHDISP_LEDC_IS_EXIST) {
        return SHDISP_LEDC_RET_ERR_NOIC;
    }
#endif
    
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_LD_power_on                                                   */
/* ------------------------------------------------------------------------- */

static int shdisp_ledc_LD_power_on(void)
{
    return shdisp_ledc_PD_power_on();
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_LD_power_off                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_ledc_LD_power_off(void)
{
    return shdisp_ledc_PD_power_off();
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_LD_set_color                                                  */
/* ------------------------------------------------------------------------- */

static void shdisp_ledc_LD_set_color(struct shdisp_ledc_req *ledc_req)
{
    unsigned char color1, color2;

    color1 = shdisp_ledc_API_get_color_index_and_reedit(ledc_req, SHDISP_LEDC_LED_INDEX_RGB1);
    color2 = shdisp_ledc_API_get_color_index_and_reedit(ledc_req, SHDISP_LEDC_LED_INDEX_RGB2);

    if((color1 == 0x00) && (color2 == 0x00)) {
        shdisp_ledc_PD_LED_off();
        
        shdisp_ledc_LD_power_off();
    } else {
        shdisp_ledc_LD_power_on();
        
        shdisp_ledc_PD_LED_on_color(color1, color2, ledc_req->led_mode, ledc_req->on_count);
    }
    
    plcdc_info->ledc_req.red[0]   = ledc_req->red[0];
    plcdc_info->ledc_req.red[1]   = ledc_req->red[1];
    plcdc_info->ledc_req.green[0] = ledc_req->green[0];
    plcdc_info->ledc_req.green[1] = ledc_req->green[1];
    plcdc_info->ledc_req.blue[0]  = ledc_req->blue[0];
    plcdc_info->ledc_req.blue[1]  = ledc_req->blue[1];
    plcdc_info->ledc_req.led_mode = ledc_req->led_mode;
    plcdc_info->ledc_req.on_count = ledc_req->on_count;
    
    return;
}


/* ------------------------------------------------------------------------- */
/* Phygical Driver                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_ledc_PD_init                                                       */
/* ------------------------------------------------------------------------- */

static void shdisp_ledc_PD_init(void)
{
#ifdef SHDISP_LCDC_SW_REG_BKUP
    memset(shdisp_ledc_reg, 0, sizeof(shdisp_ledc_reg));
    shdisp_ledc_reg[LEDC_REG_R1_PTRN] = 0x07;
    shdisp_ledc_reg[LEDC_REG_G1_PTRN] = 0x07;
    shdisp_ledc_reg[LEDC_REG_B1_PTRN] = 0x07;
    shdisp_ledc_reg[LEDC_REG_R2_PTRN] = 0x07;
    shdisp_ledc_reg[LEDC_REG_G2_PTRN] = 0x07;
    shdisp_ledc_reg[LEDC_REG_B2_PTRN] = 0x07;
#endif
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_PD_power_on                                                   */
/* ------------------------------------------------------------------------- */

static int shdisp_ledc_PD_power_on(void)
{
    if (plcdc_info->power_status == SHDISP_LEDC_PWR_STATUS_ON)
        return SHDISP_RESULT_SUCCESS;
    
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LEDC_RST_N, SHDISP_GPIO_CTL_LOW);
    shdisp_SYS_delay_us(100);
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LEDC_RST_N, SHDISP_GPIO_CTL_HIGH);
    shdisp_SYS_delay_us(100);
    
    plcdc_info->power_status = SHDISP_LEDC_PWR_STATUS_ON;
    
    shdisp_ledc_PD_init();
    
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_PD_power_off                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_ledc_PD_power_off(void)
{
    if (plcdc_info->power_status == SHDISP_LEDC_PWR_STATUS_OFF)
        return SHDISP_RESULT_SUCCESS;
    
    shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LEDC_RST_N, SHDISP_GPIO_CTL_LOW);
    shdisp_SYS_delay_us(100);
    
    plcdc_info->power_status = SHDISP_LEDC_PWR_STATUS_OFF;
    
    shdisp_ledc_PD_init();
    
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_PD_LED_off                                                    */
/* ------------------------------------------------------------------------- */

static void shdisp_ledc_PD_LED_off(void)
{
    if (plcdc_info->power_status == SHDISP_LEDC_PWR_STATUS_OFF)
        return;
    
    shdisp_ledc_IO_write_reg(LEDC_REG_RGB_CTRL, 0x00);
    
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_PD_set_rgb                                                    */
/* ------------------------------------------------------------------------- */

static void shdisp_ledc_PD_set_rgb(struct shdisp_ledc_rgb *ledc_rgb)
{
    shdisp_ledc_IO_write_reg(LEDC_REG_RGB_CTRL,  0x00);
    shdisp_ledc_IO_write_reg(LEDC_REG_RGB1_TIME, (unsigned char)((ledc_rgb->mode >>  8) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_RGB2_TIME, (unsigned char)((ledc_rgb->mode      ) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_R1_VAL1,   (unsigned char)((ledc_rgb->red[0]   >> 16) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_R1_VAL2,   (unsigned char)((ledc_rgb->red[0]   >>  8) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_R1_PTRN,   (unsigned char)((ledc_rgb->red[0]        ) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_G1_VAL1,   (unsigned char)((ledc_rgb->green[0] >> 16) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_G1_VAL2,   (unsigned char)((ledc_rgb->green[0] >>  8) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_G1_PTRN,   (unsigned char)((ledc_rgb->green[0]      ) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_B1_VAL1,   (unsigned char)((ledc_rgb->blue[0]  >> 16) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_B1_VAL2,   (unsigned char)((ledc_rgb->blue[0]  >>  8) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_B1_PTRN,   (unsigned char)((ledc_rgb->blue[0]       ) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_R2_VAL1,   (unsigned char)((ledc_rgb->red[1]   >> 16) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_R2_VAL2,   (unsigned char)((ledc_rgb->red[1]   >>  8) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_R2_PTRN,   (unsigned char)((ledc_rgb->red[1]        ) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_G2_VAL1,   (unsigned char)((ledc_rgb->green[1] >> 16) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_G2_VAL2,   (unsigned char)((ledc_rgb->green[1] >>  8) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_G2_PTRN,   (unsigned char)((ledc_rgb->green[1]      ) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_B2_VAL1,   (unsigned char)((ledc_rgb->blue[1]  >> 16) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_B2_VAL2,   (unsigned char)((ledc_rgb->blue[1]  >>  8) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_B2_PTRN,   (unsigned char)((ledc_rgb->blue[1]       ) & 0xFF));
    shdisp_ledc_IO_write_reg(LEDC_REG_RGB_CTRL,  (unsigned char)((ledc_rgb->mode >> 16) & 0xFF));
    
    plcdc_info->ledc_req.red[0]   = 0x01;
    plcdc_info->ledc_req.red[1]   = 0x01;
    plcdc_info->ledc_req.green[0] = 0x01;
    plcdc_info->ledc_req.green[1] = 0x01;
    plcdc_info->ledc_req.blue[0]  = 0x01;
    plcdc_info->ledc_req.blue[1]  = 0x01;
    plcdc_info->ledc_req.led_mode = NUM_SHDISP_LEDC_RGB_MODE;
    plcdc_info->ledc_req.on_count = NUM_SHDISP_LEDC_ONCOUNT;
    
    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_PD_LED_on_color                                               */
/* ------------------------------------------------------------------------- */

static void shdisp_ledc_PD_LED_on_color(unsigned char color1, unsigned char color2, int mode, int count)
{
    unsigned char rgb_ctrl = 0x00;
    unsigned char rgb1val1 = 0x00;
    unsigned char rgb1val2 = 0x00;
    unsigned char rgb2val1 = 0x00;
    unsigned char rgb2val2 = 0x00;
    unsigned char val1on = 0x00;
    unsigned char val2on = 0x00;
    int i, ptn_no,anime_no;
    int clvari = ledc_clrvari_index;
    SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:Start\n");
    
    switch (mode) {
    case SHDISP_LEDC_RGB_MODE_NORMAL:
    case SHDISP_LEDC_RGB_MODE_PATTERN1:
    case SHDISP_LEDC_RGB_MODE_PATTERN2:
    case SHDISP_LEDC_RGB_MODE_PATTERN3:
    case SHDISP_LEDC_RGB_MODE_PATTERN4:
    case SHDISP_LEDC_RGB_MODE_PATTERN5:
    case SHDISP_LEDC_RGB_MODE_PATTERN6:
    case SHDISP_LEDC_RGB_MODE_PATTERN7:
    case SHDISP_LEDC_RGB_MODE_PATTERN8:
    case SHDISP_LEDC_RGB_MODE_PATTERN9:
    case SHDISP_LEDC_RGB_MODE_PATTERN10:
    case SHDISP_LEDC_RGB_MODE_PATTERN11:
    case SHDISP_LEDC_RGB_MODE_PATTERN12:
        shdisp_ledc_IO_write_reg(LEDC_REG_RGB_CTRL, 0x00);

        ptn_no = mode;
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:SetPattern[0x%08x] \n", ptn_no);
        shdisp_ledc_IO_write_reg(LEDC_REG_RGB1_TIME, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB1_TIME]);
        shdisp_ledc_IO_write_reg(LEDC_REG_RGB2_TIME, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB2_TIME]);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:RGB1_TIME:[%02x], RGB2_TIME:[%02x] \n",
        shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB1_TIME],
        shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB2_TIME]);
        for (i = 0; i < NUM_SHDISP_LEDC_RGB_PARTS; i++) {
            rgb1val1 = 0x00;
            rgb1val2 = 0x00;
            rgb2val1 = 0x00;
            rgb2val2 = 0x00;
            val1on = 0x00;
            val2on = 0x00;
            if(shdisp_ledc_rgb1_tbl[clvari][color1][i] != 0) {
                val1on = shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB1_VAL1];
                val2on = shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB1_VAL2];
                if(val1on > val2on) {
                    rgb1val1 = shdisp_ledc_rgb1_tbl[clvari][color1][i];
                    rgb1val2 = val2on;
                } else {
                    rgb1val1 = val1on;
                    rgb1val2 = shdisp_ledc_rgb1_tbl[clvari][color1][i];
                }
            }
            if(shdisp_ledc_rgb2_tbl[clvari][color2][i] != 0) {
                val1on = shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB2_VAL1];
                val2on = shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB2_VAL2];
                if(val1on > val2on) {
                    rgb2val1 = shdisp_ledc_rgb2_tbl[clvari][color2][i];
                    rgb2val2 = val2on;
                } else {
                    rgb2val1 = val1on;
                    rgb2val2 = shdisp_ledc_rgb2_tbl[clvari][color2][i];
                }
            }
            switch(i){
                case SHDISP_LEDC_RGB_PARTS_RED:
                    shdisp_ledc_IO_write_reg(LEDC_REG_R1_VAL1, rgb1val1);
                    shdisp_ledc_IO_write_reg(LEDC_REG_R1_VAL2, rgb1val2);
                    shdisp_ledc_IO_write_reg(LEDC_REG_R1_PTRN, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB1_PTRN]);
                    shdisp_ledc_IO_write_reg(LEDC_REG_R2_VAL1, rgb2val1);
                    shdisp_ledc_IO_write_reg(LEDC_REG_R2_VAL2, rgb2val2);
                    shdisp_ledc_IO_write_reg(LEDC_REG_R2_PTRN, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB2_PTRN]);
                    break;
                case SHDISP_LEDC_RGB_PARTS_GREEN:
                    shdisp_ledc_IO_write_reg(LEDC_REG_G1_VAL1, rgb1val1);
                    shdisp_ledc_IO_write_reg(LEDC_REG_G1_VAL2, rgb1val2);
                    shdisp_ledc_IO_write_reg(LEDC_REG_G1_PTRN, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB1_PTRN]);
                    shdisp_ledc_IO_write_reg(LEDC_REG_G2_VAL1, rgb2val1);
                    shdisp_ledc_IO_write_reg(LEDC_REG_G2_VAL2, rgb2val2);
                    shdisp_ledc_IO_write_reg(LEDC_REG_G2_PTRN, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB2_PTRN]);
                    break;
                case SHDISP_LEDC_RGB_PARTS_BLUE:
                    shdisp_ledc_IO_write_reg(LEDC_REG_B1_VAL1, rgb1val1);
                    shdisp_ledc_IO_write_reg(LEDC_REG_B1_VAL2, rgb1val2);
                    shdisp_ledc_IO_write_reg(LEDC_REG_B1_PTRN, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB1_PTRN]);
                    shdisp_ledc_IO_write_reg(LEDC_REG_B2_VAL1, rgb2val1);
                    shdisp_ledc_IO_write_reg(LEDC_REG_B2_VAL2, rgb2val2);
                    shdisp_ledc_IO_write_reg(LEDC_REG_B2_PTRN, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB2_PTRN]);
                    break;
                default:
                    SHDISP_ERR("[SHDISP] ledc on color RGB1[i=0x%02x]",i);
                    break;
            }
            SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:setColor[%02x] \n", i);
            SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:PART[%d] R1_VAL1[%02x], R1_VAL2:[%02x], PTRN:[%02x] \n",
                i, rgb1val1, rgb1val2, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB1_PTRN]);
            SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:PART[%d] R2_VAL2[%02x], R2_VAL2:[%02x], PTRN:[%02x] \n",
                i, rgb2val1, rgb2val2, shdisp_ledc_ptn_tbl[clvari][ptn_no][SHDISP_LEDC_SETTING_PTN_RGB2_PTRN]);
        }
        if(count == SHDISP_LEDC_ONCOUNT_REPEAT) {
            rgb_ctrl  = (color1 == SHDISP_LEDC_COLOR_NONE) ? LEDC_REG_RGB_CTRL_VAL_RGB12_OFF : LEDC_REG_RGB_CTRL_VAL_RGB1_REPEAT;
            rgb_ctrl |= (color2 == SHDISP_LEDC_COLOR_NONE) ? LEDC_REG_RGB_CTRL_VAL_RGB12_OFF : LEDC_REG_RGB_CTRL_VAL_RGB2_REPEAT;
        } else {
            rgb_ctrl  = (color1 == SHDISP_LEDC_COLOR_NONE) ? LEDC_REG_RGB_CTRL_VAL_RGB12_OFF : LEDC_REG_RGB_CTRL_VAL_RGB1_1SHOT;
            rgb_ctrl |= (color2 == SHDISP_LEDC_COLOR_NONE) ? LEDC_REG_RGB_CTRL_VAL_RGB12_OFF : LEDC_REG_RGB_CTRL_VAL_RGB2_1SHOT;
        }
        shdisp_ledc_IO_write_reg(LEDC_REG_RGB_CTRL, rgb_ctrl);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:REPEAT[%02x] \n",rgb_ctrl);
        break;
    case SHDISP_LEDC_RGB_MODE_ANIMATION1:
    case SHDISP_LEDC_RGB_MODE_ANIMATION2:
    case SHDISP_LEDC_RGB_MODE_ANIMATION3:
    case SHDISP_LEDC_RGB_MODE_ANIMATION4:
    case SHDISP_LEDC_RGB_MODE_ANIMATION5:
    case SHDISP_LEDC_RGB_MODE_ANIMATION6:
    case SHDISP_LEDC_RGB_MODE_ANIMATION7:
    case SHDISP_LEDC_RGB_MODE_ANIMATION8:
    case SHDISP_LEDC_RGB_MODE_ANIMATION9:
    case SHDISP_LEDC_RGB_MODE_ANIMATION10:
        anime_no = mode - SHDISP_LEDC_RGB_MODE_ANIMATION1;
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:SetAnimation[0x%08x]\n", anime_no);
        shdisp_ledc_IO_write_reg(LEDC_REG_RGB_CTRL, 0x00);
        shdisp_ledc_IO_write_reg(LEDC_REG_RGB1_TIME, shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_RGB_TIME]);
        shdisp_ledc_IO_write_reg(LEDC_REG_R1_VAL1,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_VAL1]);
        shdisp_ledc_IO_write_reg(LEDC_REG_R1_VAL2,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_VAL2]);
        shdisp_ledc_IO_write_reg(LEDC_REG_R1_PTRN,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_PTRN]);
        shdisp_ledc_IO_write_reg(LEDC_REG_G1_VAL1,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_VAL1]);
        shdisp_ledc_IO_write_reg(LEDC_REG_G1_VAL2,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_VAL2]);
        shdisp_ledc_IO_write_reg(LEDC_REG_G1_PTRN,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_PTRN]);
        shdisp_ledc_IO_write_reg(LEDC_REG_B1_VAL1,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_VAL1]);
        shdisp_ledc_IO_write_reg(LEDC_REG_B1_VAL2,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_VAL2]);
        shdisp_ledc_IO_write_reg(LEDC_REG_B1_PTRN,   shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_PTRN]);
        shdisp_ledc_IO_write_reg(LEDC_REG_RGB2_TIME, shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_RGB_TIME]);
        shdisp_ledc_IO_write_reg(LEDC_REG_R2_VAL1,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_VAL1]);
        shdisp_ledc_IO_write_reg(LEDC_REG_R2_VAL2,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_VAL2]);
        shdisp_ledc_IO_write_reg(LEDC_REG_R2_PTRN,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_PTRN]);
        shdisp_ledc_IO_write_reg(LEDC_REG_G2_VAL1,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_VAL1]);
        shdisp_ledc_IO_write_reg(LEDC_REG_G2_VAL2,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_VAL2]);
        shdisp_ledc_IO_write_reg(LEDC_REG_G2_PTRN,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_PTRN]);
        shdisp_ledc_IO_write_reg(LEDC_REG_B2_VAL1,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_VAL1]);
        shdisp_ledc_IO_write_reg(LEDC_REG_B2_VAL2,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_VAL2]);
        shdisp_ledc_IO_write_reg(LEDC_REG_B2_PTRN,   shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_PTRN]);

        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:[RGB1] TIME[%02x]\n",
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_RGB_TIME]);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:[RGB1_R] R_VAL1[%02x], R_VAL2[%02x], PTRN[%02x] \n",
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_VAL1],
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_VAL2],
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_PTRN]);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:[RGB1_G] G_VAL1[%02x], G_VAL2[%02x], PTRN[%02x] \n",
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_VAL1],
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_VAL2],
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_PTRN]);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:[RGB1_B] B_VAL1[%02x], B_VAL2[%02x], PTRN[%02x] \n",
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_VAL1],
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_VAL2],
            shdisp_ledc_anime1_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_PTRN]);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:[RGB2] TIME[%02x]\n",
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_RGB_TIME]);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:[RGB2_R] R_VAL1[%02x], R_VAL2[%02x], PTRN[%02x] \n",
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_VAL1],
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_VAL2],
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_R_PTRN]);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:[RGB2_G] G_VAL1[%02x], G_VAL2[%02x], PTRN[%02x] \n",
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_VAL1],
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_VAL2],
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_G_PTRN]);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:[RGB2_B] B_VAL1[%02x], B_VAL2[%02x], PTRN[%02x] \n",
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_VAL1],
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_VAL2],
            shdisp_ledc_anime2_tbl[clvari][anime_no][SHDISP_LEDC_SETTING_ANIME_B_PTRN]);
        if(count == SHDISP_LEDC_ONCOUNT_REPEAT) {
            rgb_ctrl  = (color1 == SHDISP_LEDC_COLOR_NONE) ? LEDC_REG_RGB_CTRL_VAL_RGB12_OFF : LEDC_REG_RGB_CTRL_VAL_RGB1_REPEAT;
            rgb_ctrl |= (color2 == SHDISP_LEDC_COLOR_NONE) ? LEDC_REG_RGB_CTRL_VAL_RGB12_OFF : LEDC_REG_RGB_CTRL_VAL_RGB2_REPEAT;
        } else {
            rgb_ctrl  = (color1 == SHDISP_LEDC_COLOR_NONE) ? LEDC_REG_RGB_CTRL_VAL_RGB12_OFF : LEDC_REG_RGB_CTRL_VAL_RGB1_1SHOT;
            rgb_ctrl |= (color2 == SHDISP_LEDC_COLOR_NONE) ? LEDC_REG_RGB_CTRL_VAL_RGB12_OFF : LEDC_REG_RGB_CTRL_VAL_RGB2_1SHOT;
        }
        shdisp_ledc_IO_write_reg(LEDC_REG_RGB_CTRL, rgb_ctrl);
        SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:REPEAT[%02x] \n",rgb_ctrl);
        break;
    default:
        break;
    }
    
    SHDISP_TRACE("shdisp_ledc_PD_LED_on_color:End\n");
    return;
}


/* ------------------------------------------------------------------------- */
/* Input/Output                                                              */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_ledc_IO_write_reg                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_ledc_IO_write_reg(unsigned char reg, unsigned char val)
{
    int ret;
    
#ifdef SHDISP_LCDC_SW_I2C_WLOG
    printk("[SHDISP] ledc i2c Write(reg=0x%02x, val=0x%02X)", reg, val);
#endif
    
    ret = shdisp_SYS_ledc_i2c_write(reg, val);
    
    if (ret == SHDISP_RESULT_SUCCESS) {
#ifdef SHDISP_LCDC_SW_REG_BKUP
        if(reg < SHDISP_LEDC_REG_MAX) {
            shdisp_ledc_reg[reg] = val;
        }
#endif
        return SHDISP_RESULT_SUCCESS;
    } else if (ret == SHDISP_RESULT_FAILURE_I2C_TMO) {
        SHDISP_ERR("<TIME_OUT> shdisp_SYS_ledc_i2c_write.\n");
        return SHDISP_RESULT_FAILURE_I2C_TMO;
    }
    
    return SHDISP_RESULT_FAILURE;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ledc_IO_read_reg                                                  */
/* ------------------------------------------------------------------------- */

static int shdisp_ledc_IO_read_reg(unsigned char reg, unsigned char *val)
{
#ifdef SHDISP_LCDC_SW_REG_BKUP
    if(reg < SHDISP_LEDC_REG_MAX) {
        *val = shdisp_ledc_reg[reg];
    }
#endif
    return SHDISP_RESULT_SUCCESS;
}


MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
