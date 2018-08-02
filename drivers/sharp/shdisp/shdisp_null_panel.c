/* drivers/sharp/shdisp/shdisp_null_panel.c  (Display Driver)
 *
 * Copyright (C) 2012-2013 SHARP CORPORATION
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
#include "shdisp_panel.h"
#include "shdisp_null_panel.h"
#include "shdisp_system.h"
#include "shdisp_bl69y6.h"
#include "../../video/msm/msm_fb_panel.h"
#include "../../video/msm/mipi_sharp_clmr.h"
#include "shdisp_type.h"
#include "shdisp_pm.h"
#include "shdisp_clmr.h"
#include "shdisp_dbg.h"


/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

static int shdisp_null_panel_API_init_io(void);
static int shdisp_null_panel_API_empty_func(void);
static void shdisp_null_panel_API_set_param(struct shdisp_panel_param_str *param_str);
static int shdisp_null_panel_API_power_on(void);
static int shdisp_null_panel_API_power_off(void);
static int shdisp_null_panel_API_check_upper_unit(void);
static int shdisp_null_panel_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out);
static int shdisp_null_panel_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size);
static int shdisp_null_panel_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size);
static int shdisp_null_panel_API_diag_set_flicker_param(unsigned short alpha);
static int shdisp_null_panel_API_diag_get_flicker_param(unsigned short *alpha);
static int shdisp_null_panel_API_check_recovery(void);
static int shdisp_null_panel_API_recovery_type(int *type);
static int shdisp_null_panel_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma);
static int shdisp_null_panel_API_disp_update(struct shdisp_main_update *update);
static int shdisp_null_panel_API_disp_clear_screen(struct shdisp_main_clear *clear);
static int shdisp_null_panel_API_diag_get_flicker_low_param(unsigned short *alpha);


/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static struct shdisp_panel_param_str null_panel_param_str;
static struct dsi_buf shdisp_mipi_null_panel_tx_buf;
static struct dsi_buf shdisp_mipi_null_panel_rx_buf;

/* ------------------------------------------------------------------------- */
/*      packet header                                                        */
/* ------------------------------------------------------------------------- */

static struct shdisp_panel_operations shdisp_null_panel_fops = {
    shdisp_null_panel_API_init_io,
    shdisp_null_panel_API_empty_func,
    shdisp_null_panel_API_empty_func,
    shdisp_null_panel_API_set_param,
    shdisp_null_panel_API_power_on,
    shdisp_null_panel_API_power_off,
    shdisp_null_panel_API_empty_func,
    shdisp_null_panel_API_empty_func,
    shdisp_null_panel_API_empty_func,
    shdisp_null_panel_API_empty_func,
    shdisp_null_panel_API_empty_func,
    shdisp_null_panel_API_check_upper_unit,
    shdisp_null_panel_API_check_flicker_param,
    shdisp_null_panel_API_diag_write_reg,
    shdisp_null_panel_API_diag_read_reg,
    shdisp_null_panel_API_diag_set_flicker_param,
    shdisp_null_panel_API_diag_get_flicker_param,
    shdisp_null_panel_API_check_recovery,
    shdisp_null_panel_API_recovery_type,
    shdisp_null_panel_API_set_abl_lut,
    shdisp_null_panel_API_disp_update,
    shdisp_null_panel_API_disp_clear_screen,
    shdisp_null_panel_API_diag_get_flicker_low_param,
};

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* MACRAOS                                                                   */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_create                                              */
/* ------------------------------------------------------------------------- */
struct shdisp_panel_operations *shdisp_null_panel_API_create(void)
{
    SHDISP_DEBUG("\n");
    return &shdisp_null_panel_fops;
}


static int shdisp_null_panel_mipi_dsi_buf_alloc(struct dsi_buf *dp, int size)
{

    dp->start = kmalloc(size, GFP_KERNEL);
    if (dp->start == NULL) {
        SHDISP_ERR("%u\n", __LINE__);
        return -ENOMEM;
    }

    dp->end = dp->start + size;
    dp->size = size;

    dp->data = dp->start;
    dp->len = 0;
    return size;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_init_io                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_init_io(void)
{
    SHDISP_DEBUG("in\n");
    shdisp_null_panel_mipi_dsi_buf_alloc(&shdisp_mipi_null_panel_tx_buf, DSI_BUF_SIZE);
    shdisp_null_panel_mipi_dsi_buf_alloc(&shdisp_mipi_null_panel_rx_buf, DSI_BUF_SIZE);
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_empty_func                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_empty_func(void)
{
    SHDISP_DEBUG("\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_set_param                                           */
/* ------------------------------------------------------------------------- */
static void shdisp_null_panel_API_set_param(struct shdisp_panel_param_str *param_str)
{
    SHDISP_DEBUG("in\n");
    null_panel_param_str.vcom_alpha = param_str->vcom_alpha;
    null_panel_param_str.vcom_alpha_low = param_str->vcom_alpha_low;
    SHDISP_DEBUG("out\n");

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_power_on                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_power_on(void)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_power_off                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_power_off(void)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_check_upper_unit                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_check_upper_unit(void)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_check_flicker_param                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_diag_write_reg                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_diag_read_reg                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_diag_set_flicker_param                              */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_diag_set_flicker_param(unsigned short alpha)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_diag_get_flicker_param                              */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_diag_get_flicker_param(unsigned short *alpha)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_diag_get_flicker_low_param                          */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_diag_get_flicker_low_param(unsigned short *alpha)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_check_recovery                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_check_recovery(void)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_recovery_type                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_recovery_type(int *type)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_set_abl_lut                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_disp_update                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_disp_update(struct shdisp_main_update *update)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_null_panel_API_disp_clear_screen                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_null_panel_API_disp_clear_screen(struct shdisp_main_clear *clear)
{
    return SHDISP_RESULT_SUCCESS;
}

static int shdisp_null_panel_mipi_cmd_start_display(void)
{
    return SHDISP_RESULT_SUCCESS;
}

static int shdisp_null_panel_mipi_cmd_lcd_off(void)
{
    return SHDISP_RESULT_SUCCESS;
}

/* Test Image Generator ON */
int shdisp_panel_null_panel_API_TestImageGen(int onoff)
{
    return SHDISP_RESULT_SUCCESS;
}

static int shdisp_null_panel_mipi_cmd_lcd_on(void)
{
    return SHDISP_RESULT_SUCCESS;
}

int shdisp_null_panel_API_mipi_lcd_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    ret = shdisp_null_panel_mipi_cmd_lcd_on();
    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}

int shdisp_null_panel_API_mipi_lcd_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    ret = shdisp_null_panel_mipi_cmd_lcd_off();
    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}

int shdisp_null_panel_API_mipi_start_display(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    ret = shdisp_null_panel_mipi_cmd_start_display();
    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}

int shdisp_null_panel_API_cabc_init(void)
{
    return 0;
}

int shdisp_null_panel_API_cabc_indoor_on(void)
{
    return 0;
}

int shdisp_null_panel_API_cabc_outdoor_on(int lut_level)
{
    return 0;
}

int shdisp_null_panel_API_cabc_off(int wait_on, int pwm_disable)
{
    return 0;
}

int shdisp_null_panel_API_cabc_outdoor_move(int lut_level)
{
    return 0;
}


MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
