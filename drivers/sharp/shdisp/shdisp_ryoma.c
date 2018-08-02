/* drivers/sharp/shdisp/shdisp_ryoma.c  (Display Driver)
 *
 * Copyright (C) 2011-2013 SHARP CORPORATION
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
#include "shdisp_ryoma.h"
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
#define SHDISP_RYOMA_RESET      32
#define SHDISP_RYOMA_DISABLE    0
#define SHDISP_RYOMA_ENABLE     1

#define SHDISP_LCDDR_GAMMA_OFFSET       12
#define SHDISP_LCDDR_GAMMA_STATUS_OK    0x96
#define SHDISP_LCDDR_GAMMA_STATUS_OK_2  0x97

#define SHDISP_RYOMA_1V_WAIT                 18000
#define SHDISP_RYOMA_VCOM_MIN                0x0010
#define SHDISP_RYOMA_VCOM_MAX                0x00EF
#define SHDISP_RYOMA_ALPHA_DEFAULT           0x002A
#define SHDISP_RYOMA_CLOCK                   19200000

#define SHDISP_FW_STACK_EXCUTE

#define SHDISP_RYOMA_GAMMA_SETTING_A                        0xC7
#define SHDISP_RYOMA_GAMMA_SETTING_B                        0xC8
#define SHDISP_RYOMA_GAMMA_SETTING_C                        0xC9
#define SHDISP_RYOMA_GAMMA_SETTING_SIZE                     24
#define SHDISP_RYOMA_GAMMA_LEVEL_MIN                        1
#define SHDISP_RYOMA_GAMMA_LEVEL_MAX                        12
#define SHDISP_RYOMA_NEGATIVE_OFFSET                  12

#define SW_NORMAL_DISPLAY

#define MIPI_SHARP_CLMR_1HZ_BLACK_ON    1
#define MIPI_SHARP_CLMR_AUTO_PAT_OFF    0

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

static int shdisp_ryoma_API_init_io(void);
static int shdisp_ryoma_API_empty_func(void);
static void shdisp_ryoma_API_set_param(struct shdisp_panel_param_str *param_str);
static int shdisp_ryoma_API_power_on(void);
static int shdisp_ryoma_API_power_off(void);
static int shdisp_ryoma_API_check_upper_unit(void);
static int shdisp_ryoma_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out);
static int shdisp_ryoma_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size);
static int shdisp_ryoma_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size);
static int shdisp_ryoma_API_diag_set_flicker_param(unsigned short alpha);
static int shdisp_ryoma_API_diag_get_flicker_param(unsigned short *alpha);
static int shdisp_ryoma_API_check_recovery(void);
static int shdisp_ryoma_API_recovery_type(int *type);
static int shdisp_ryoma_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma);
static int shdisp_ryoma_API_disp_update(struct shdisp_main_update *update);
static int shdisp_ryoma_API_disp_clear_screen(struct shdisp_main_clear *clear);

static int shdisp_ryoma_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
static int shdisp_ryoma_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
static int shdisp_ryoma_API_diag_set_gamma(struct shdisp_diag_gamma *gamma);

static int shdisp_ryoma_API_diag_get_flicker_low_param(unsigned short *alpha);

static int shdisp_ryoma_API_power_on_recovery(void);
static int shdisp_ryoma_API_power_off_recovery(void);

static char shdisp_ryoma_mipi_manufacture_id(void);

int shdisp_ryoma_init_phy_gamma(struct shdisp_lcddr_phy_gamma_reg *phy_gamma);

static int shdisp_ryoma_mipi_tx_vcom_write(void);
static int shdisp_ryoma_mipi_tx_vcom_off_write(void);
#if defined(SW_NORMAL_DISPLAY)
static int shdisp_ryoma_mipi_cmd_lcd_display_normal_on(void);
#endif

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static struct shdisp_panel_param_str ryoma_param_str;
#ifndef SHDISP_NOT_SUPPORT_FLICKER
static unsigned char ryoma_wdata[9];
static unsigned char ryoma_rdata[9];
#endif
static struct dsi_buf shdisp_mipi_ryoma_tx_buf;
static struct dsi_buf shdisp_mipi_ryoma_rx_buf;

static char mipi_sh_ryoma_set_val_gamma_a[25];
static char mipi_sh_ryoma_set_val_gamma_b[25];
static char mipi_sh_ryoma_set_val_gamma_c[25];
static char mipi_sh_ryoma_set_val_cmd_timing4[14];

/* ------------------------------------------------------------------------- */
/*      packet header                                                        */
/* ------------------------------------------------------------------------- */
/*      LCD ON                                                              */
/*      Initial Setting                                                     */
static char mipi_sh_ryoma_cmd_protect[2] = {0xB0, 0x00};
static char mipi_sh_ryoma_cmd_manufacture_id[1] = {0xBF};
static char mipi_sh_ryoma_cmd_init1[7] = {0xB3, 
    0x2D,0xC0,0x00,0x00,0x00,0x00};
static char mipi_sh_ryoma_cmd_init2[3] = {0xB4, 0x0C,0x12};
static char mipi_sh_ryoma_cmd_init3[3] = {0xB6, 0x39,0xB3};
static char mipi_sh_ryoma_cmd_init4[2] = {0xB7, 0x00};
static char mipi_sh_ryoma_cmd_panel_pin[12] = {0xCB, 
    0x65,0x26,0xC0,0x19,0x0A,0x00,0x00,0x00,0x00,0xC0,0x00};
static char mipi_sh_ryoma_cmd_panel_if[2] = {0xCC, 0x01};
static char mipi_sh_ryoma_cmd_timing1[39] = {0xC1, 
    0x0C,0x62,0x40,0x52,0x02,0x00,0x00,0x00,0x00,0x00,0x02,0xC7,
    0x06,0x02,0x08,0x09,0x0A,0x0B,0x00,0x00,0x00,0x00,0x62,0x30,
    0x40,0xA5,0x0F,0x04,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00};
static char mipi_sh_ryoma_cmd_timing2[8] = {0xC2, 
    0x30,0xF5,0x00,0x08,0x0E,0x00,0x00};
static char mipi_sh_ryoma_cmd_timing3[4] = {0xC3, 0x00,0x00,0x00};
static char mipi_sh_ryoma_cmd_timing4[14] = {0xC4, 
    0x70,0x00,0x00,0x00,0x00,0x1F,0x00,0x00,0x00,0x00,0x00,0x1F,
    0x00};
static char mipi_sh_ryoma_cmd_timing5[7] = {0xC6, 
    0x7A,0x0D,0x6F,0x7A,0x0D,0x6F};
static char mipi_sh_ryoma_cmd_gamma_a[25] = {0xC7, 
    0x00,0x06,0x0C,0x13,0x20,0x34,0x24,0x34,0x41,0x4D,0x5E,0x6B,
    0x00,0x06,0x0C,0x13,0x20,0x34,0x24,0x32,0x3E,0x4A,0x5B,0x67};
static char mipi_sh_ryoma_cmd_gamma_b[25] = {0xC8, 
    0x00,0x06,0x0C,0x13,0x20,0x34,0x24,0x34,0x41,0x4D,0x5E,0x6B,
    0x00,0x06,0x0C,0x13,0x20,0x34,0x24,0x32,0x3E,0x4A,0x5B,0x67};
static char mipi_sh_ryoma_cmd_gamma_c[25] = {0xC9, 
    0x00,0x06,0x0C,0x13,0x20,0x34,0x24,0x34,0x41,0x4D,0x5E,0x6B,
    0x00,0x06,0x0C,0x13,0x20,0x34,0x24,0x32,0x3E,0x4A,0x5B,0x67};
static char mipi_sh_ryoma_cmd_power1[17] = {0xD0, 
    0x00,0x10,0x19,0x18,0x99,0x98,0x18,0x00,0x88,0x01,0xBB,0x0C,
    0x8F,0x0E,0x21,0x20};
static char mipi_sh_ryoma_cmd_power15[2] = {0xD2, 0x9C};
static char mipi_sh_ryoma_cmd_power2[25] = {0xD3, 
    0x1B,0xB3,0xBB,0xBB,0x33,0x33,0x33,0x33,0x55,0x01,0x00,0xA0,
    0xA8,0xA0,0x07,0xC7,0xB7,0x33,0xA2,0x73,0xC7,0x00,0x00,0x00};
static char mipi_sh_ryoma_cmd_vcom[9] = {0xD5, 
    0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char mipi_sh_ryoma_cmd_sleepout[2] = {0xD6, 0x01};
static char mipi_sh_ryoma_cmd_power3[48] = {0xD7, 
    0x44,0x01,0xFF,0xFF,0x3F,0xFC,0x51,0x9D,0x71,0xF0,0x0F,0x00,
    0xE0,0xFF,0x01,0xF0,0x03,0x00,0x1E,0x00,0x08,0x94,0x40,0xF0,
    0x73,0x7F,0x78,0x08,0xC0,0x07,0x0A,0x55,0x15,0x28,0x54,0x55,
    0xA0,0xF0,0xFF,0x00,0x40,0x55,0x05,0x00,0x20,0x20,0x01};
static char mipi_sh_ryoma_cmd_power4[17] = {0xD9, 
    0x01,0xF8,0x57,0x07,0x00,0x10,0x00,0xC0,0x00,0x05,0x33,0x33,
    0x00,0xF0,0x33,0x33};
static char mipi_sh_ryoma_cmd_set_sync1[3] = {0xEC, 0x01,0x07};
static char mipi_sh_ryoma_cmd_set_sync2[6] = {0xED, 0x02,0x00,0x10,0x00,0x00};
static char mipi_sh_ryoma_cmd_set_sync3[2] = {0xEE, 0x00};
static char mipi_sh_ryoma_cmd_set_sync4[14] = {0xEF, 
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00};
static char mipi_sh_ryoma_cmd_column_addr[5] = {0x2A, 0x00,0x00,0x02,0xCF};
static char mipi_sh_ryoma_cmd_page_addr[5] = {0x2B, 0x00,0x00,0x04,0xFF};
static char mipi_sh_ryoma_cmd_tear_on[2] = {0x35, 0x00};
static char mipi_sh_ryoma_cmd_display_on[2] = {0x29, 0x00};
static char mipi_sh_ryoma_cmd_exit_sleep[2] = {0x11, 0x00};

#if 0
static char mipi_sh_ryoma_cmd_normal_on1[2] = {0xB3, 
    0x0C};
static char mipi_sh_ryoma_cmd_normal_on2[7] = {0xC6, 
    0xB2,0x0D,0x6F,0xB2,0x0D,0x6F};
static char mipi_sh_ryoma_cmd_normal_on3[7] = {0xC6, 
    0x7A,0x0D,0x6F,0x7A,0x0D,0x6F};
static char mipi_sh_ryoma_cmd_normal_on4[9] = {0xD3, 
    0x1B,0xB3,0xBB,0xBB,0x33,0x33,0x33,0x33};
static char mipi_sh_ryoma_cmd_normal_on5[17] = {0xD9, 
    0x00,0x68,0x4F,0x07,0x00,0x10,0x00,0xC0,0x00,0x05,0x33,0x33,
    0x00,0xF0,0x33,0x33};
#endif
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_protect[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_protect), mipi_sh_ryoma_cmd_protect}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_manufacture_id[] = {
    {DTYPE_GEN_READ1, 1, 0, 1, 0, 1, mipi_sh_ryoma_cmd_manufacture_id}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_init[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_init1), mipi_sh_ryoma_cmd_init1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_init2), mipi_sh_ryoma_cmd_init2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_init3), mipi_sh_ryoma_cmd_init3},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_init4), mipi_sh_ryoma_cmd_init4},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_panel_pin), mipi_sh_ryoma_cmd_panel_pin},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_panel_if), mipi_sh_ryoma_cmd_panel_if}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_timing1[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_timing1), mipi_sh_ryoma_cmd_timing1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_timing2), mipi_sh_ryoma_cmd_timing2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_timing3), mipi_sh_ryoma_cmd_timing3},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_set_val_cmd_timing4), mipi_sh_ryoma_set_val_cmd_timing4},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_timing5), mipi_sh_ryoma_cmd_timing5}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_gamma[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_set_val_gamma_a), mipi_sh_ryoma_set_val_gamma_a},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_set_val_gamma_b), mipi_sh_ryoma_set_val_gamma_b},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_set_val_gamma_c), mipi_sh_ryoma_set_val_gamma_c}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_power1[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_power1), mipi_sh_ryoma_cmd_power1},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_power15), mipi_sh_ryoma_cmd_power15},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_power2), mipi_sh_ryoma_cmd_power2}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_power2[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_sleepout), mipi_sh_ryoma_cmd_sleepout},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_power3), mipi_sh_ryoma_cmd_power3},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_power4), mipi_sh_ryoma_cmd_power4}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_sync[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_set_sync1), mipi_sh_ryoma_cmd_set_sync1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_set_sync2), mipi_sh_ryoma_cmd_set_sync2},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_set_sync3), mipi_sh_ryoma_cmd_set_sync3},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_set_sync4), mipi_sh_ryoma_cmd_set_sync4}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_display_on[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_column_addr), mipi_sh_ryoma_cmd_column_addr},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_page_addr), mipi_sh_ryoma_cmd_page_addr},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_tear_on), mipi_sh_ryoma_cmd_tear_on},
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_display_on), mipi_sh_ryoma_cmd_display_on}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_display_on_start[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, (90*1000), sizeof(mipi_sh_ryoma_cmd_exit_sleep), mipi_sh_ryoma_cmd_exit_sleep}
};
#if 0
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_display_normal_on[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_normal_on1), mipi_sh_ryoma_cmd_normal_on1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_normal_on2), mipi_sh_ryoma_cmd_normal_on2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, (18*1000), sizeof(mipi_sh_ryoma_cmd_normal_on3), mipi_sh_ryoma_cmd_normal_on3},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_normal_on4), mipi_sh_ryoma_cmd_normal_on4},
    {DTYPE_GEN_LWRITE, 1, 0, 0, (90*1000), sizeof(mipi_sh_ryoma_cmd_normal_on5), mipi_sh_ryoma_cmd_normal_on5}
};
#endif
#if 0
static char mipi_sh_ryoma_cmd_black_screen1[2] = {0xD9, 
    0x00};
static char mipi_sh_ryoma_cmd_black_screen2[9] = {0xD3, 
    0x1B,0xB3,0x99,0x81,0x01,0x33,0x33,0x11};
static char mipi_sh_ryoma_cmd_black_screen3[17] = {0xD9, 
    0x09,0x68,0x4F,0x0F,0x00,0x10,0x00,0xC0,0x00,0x05,0x01,0x01,
    0xFF,0xF3,0x11,0x11};
static char mipi_sh_ryoma_cmd_black_screen4[7] = {0xC6, 
    0xB2,0x0D,0x6F,0xB2,0x0D,0x6F};
static char mipi_sh_ryoma_cmd_black_screen5[7] = {0xC6, 
    0xB2,0x13,0xA2,0xB2,0x13,0xA2};
static char mipi_sh_ryoma_cmd_black_screen6[2] = {0xB3, 
    0x00};
#endif
#if 0
static char mipi_sh_ryoma_cmd_only_panel_pw_supply1[2] = {0xD9, 
    0x00};
static char mipi_sh_ryoma_cmd_only_panel_pw_supply2[2] = {0xD9, 
    0x09};
static char mipi_sh_ryoma_cmd_only_panel_pw_supply3[6] = {0xCB, 
    0x00,0x00,0x00,0x00,0x00};
static char mipi_sh_ryoma_cmd_only_panel_pw_supply4[6] = {0xCB, 
    0x05,0x00,0x00,0x00,0x0A};
static char mipi_sh_ryoma_cmd_only_panel_pw_supply5[6] = {0xCB, 
    0x65,0x26,0xC0,0x19,0x0A};
#endif
static char mipi_sh_ryoma_cmd_low_power[2] = {0xD9, 0x00};
static char mipi_sh_ryoma_cmd_disp_off[2] = {0x28, 0x00};
static char mipi_sh_ryoma_cmd_sleep_on[2] = {0x10, 0x00};
static char mipi_sh_ryoma_cmd_standby[2] = {0xB1, 0x01};
#if 0
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_display_black_screen[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, (18*1000), sizeof(mipi_sh_ryoma_cmd_black_screen1), mipi_sh_ryoma_cmd_black_screen1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_black_screen2), mipi_sh_ryoma_cmd_black_screen2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, (18*1000), sizeof(mipi_sh_ryoma_cmd_black_screen3), mipi_sh_ryoma_cmd_black_screen3},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_black_screen4), mipi_sh_ryoma_cmd_black_screen4},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_black_screen5), mipi_sh_ryoma_cmd_black_screen5},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_black_screen6), mipi_sh_ryoma_cmd_black_screen6},
    {DTYPE_DCS_WRITE,  1, 0, 0, (90*1000), sizeof(mipi_sh_ryoma_cmd_exit_sleep), mipi_sh_ryoma_cmd_exit_sleep}
};
#endif
#if 0
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_only_panel_pw_supply[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, (18*1000), sizeof(mipi_sh_ryoma_cmd_only_panel_pw_supply1), mipi_sh_ryoma_cmd_only_panel_pw_supply1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, (36*1000), sizeof(mipi_sh_ryoma_cmd_only_panel_pw_supply2), mipi_sh_ryoma_cmd_only_panel_pw_supply2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, (18*1000), sizeof(mipi_sh_ryoma_cmd_only_panel_pw_supply3), mipi_sh_ryoma_cmd_only_panel_pw_supply3},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_only_panel_pw_supply4), mipi_sh_ryoma_cmd_only_panel_pw_supply4},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sh_ryoma_cmd_only_panel_pw_supply5), mipi_sh_ryoma_cmd_only_panel_pw_supply5}
};
#endif
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_display_off[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, (18*1000), sizeof(mipi_sh_ryoma_cmd_low_power), mipi_sh_ryoma_cmd_low_power},
    {DTYPE_DCS_WRITE, 1, 0, 0, (18*1000), sizeof(mipi_sh_ryoma_cmd_disp_off), mipi_sh_ryoma_cmd_disp_off}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_display_off2[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, (108*1000), sizeof(mipi_sh_ryoma_cmd_sleep_on), mipi_sh_ryoma_cmd_sleep_on},
    {DTYPE_GEN_WRITE2, 1, 0, 0, (18*1000), sizeof(mipi_sh_ryoma_cmd_standby), mipi_sh_ryoma_cmd_standby}
};

#if 0
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_init_set_pwm[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_init_set_pwm), mipi_sharp_cmd_cabc_ryoma_init_set_pwm}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_init_set_para[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_init_set_param), mipi_sharp_cmd_cabc_ryoma_init_set_param}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_init_set_flicker[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_init_set_flicker), mipi_sharp_cmd_cabc_ryoma_init_set_flicker}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_init_set_connector[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_init_set_connector), mipi_sharp_cmd_cabc_ryoma_init_set_connector}
};

static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_indoor_set_image_exrate_lut0[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_indoor_set_image_exrate_lut0), mipi_sharp_cmd_cabc_ryoma_indoor_set_image_exrate_lut0}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_indoor_set_flicker[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_indoor_set_flicker), mipi_sharp_cmd_cabc_ryoma_indoor_set_flicker}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_indoor_set_on[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_indoor_set_on), mipi_sharp_cmd_cabc_ryoma_indoor_set_on}
};

static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_outdoor_set_image_exrate_lut1[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut1), mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut1}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_outdoor_set_image_exrate_lut2[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut2), mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut2}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_outdoor_set_image_exrate_lut3[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut3), mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut3}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_outdoor_set_image_exrate_lut4[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut4), mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut4}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_outdoor_set_image_exrate_lut5[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut5), mipi_sharp_cmd_cabc_ryoma_outdoor_set_image_exrate_lut5}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_outdoor_set_on[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_outdoor_set_on), mipi_sharp_cmd_cabc_ryoma_outdoor_set_on}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_outdoor_set_flicker[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_outdoor_set_flicker), mipi_sharp_cmd_cabc_ryoma_outdoor_set_flicker}
};

static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_off_set_image_exrate_lut0[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_off_set_image_exrate_lut0), mipi_sharp_cmd_cabc_ryoma_off_set_image_exrate_lut0}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_off_set_off[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_off_set_off), mipi_sharp_cmd_cabc_ryoma_off_set_off}
};

static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_transition_set_image_exrate_lut1[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut1), mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut1}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_transition_set_image_exrate_lut2[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut2), mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut2}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_transition_set_image_exrate_lut3[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut3), mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut3}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_transition_set_image_exrate_lut4[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut4), mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut4}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_transition_set_image_exrate_lut5[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut5), mipi_sharp_cmd_cabc_ryoma_transition_set_image_exrate_lut5}
};


static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_bank_lock[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_bank_lock), mipi_sharp_cmd_cabc_bank_lock}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_bank_01[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_bank_01), mipi_sharp_cmd_cabc_bank_01}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_trv_lock0[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_trv_lock0), mipi_sharp_cmd_cabc_trv_lock0}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_trv_lock1[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_trv_lock1), mipi_sharp_cmd_cabc_trv_lock1}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_bank_00[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_bank_00), mipi_sharp_cmd_cabc_bank_00}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_fntn_s[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_fntn_s), mipi_sharp_cmd_cabc_fntn_s}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_trv_mode_set[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_trv_mode_set), mipi_sharp_cmd_cabc_trv_mode_set}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_val_go[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_val_go), mipi_sharp_cmd_cabc_val_go}
};

static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_trv_on[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_trv_on), mipi_sharp_cmd_cabc_trv_on}
};

static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_trv_off[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_trv_off), mipi_sharp_cmd_cabc_trv_off}
};

static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_trv_lut_set_on[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_trv_lut_set_on), mipi_sharp_cmd_cabc_trv_lut_set_on}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_bank_07[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_bank_07), mipi_sharp_cmd_cabc_bank_07}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_bank_08[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_bank_08), mipi_sharp_cmd_cabc_bank_08}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_bank_09[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_bank_09), mipi_sharp_cmd_cabc_bank_09}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_bank_0A[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_bank_0A), mipi_sharp_cmd_cabc_bank_0A}
};
static struct dsi_cmd_desc mipi_sh_ryoma_cmds_cabc_trv_lut_set_off[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(mipi_sharp_cmd_cabc_trv_lut_set_off), mipi_sharp_cmd_cabc_trv_lut_set_off}
};
#endif

static char mipi_sh_ryoma_clk_state[16] = {
    0x00,0x68,0x4F,0x07,0x00,0x00,0x00,0x80,0x00,0x76,0x33,0x33,
    0x00,0xF0,0x33,0x33};
static char mipi_sh_ryoma_panel_sync[2] = {0x01,0x00};


static struct shdisp_panel_operations shdisp_ryoma_fops = {
    shdisp_ryoma_API_init_io,
    shdisp_ryoma_API_empty_func,
    shdisp_ryoma_API_empty_func,
    shdisp_ryoma_API_set_param,
    shdisp_ryoma_API_power_on,
    shdisp_ryoma_API_power_off,
    shdisp_ryoma_API_empty_func,
    shdisp_ryoma_API_empty_func,
    shdisp_ryoma_API_empty_func,
    shdisp_ryoma_API_empty_func,
    shdisp_ryoma_API_empty_func,
    shdisp_ryoma_API_check_upper_unit,
    shdisp_ryoma_API_check_flicker_param,
    shdisp_ryoma_API_diag_write_reg,
    shdisp_ryoma_API_diag_read_reg,
    shdisp_ryoma_API_diag_set_flicker_param,
    shdisp_ryoma_API_diag_get_flicker_param,
    shdisp_ryoma_API_check_recovery,
    shdisp_ryoma_API_recovery_type,
    shdisp_ryoma_API_set_abl_lut,
    shdisp_ryoma_API_disp_update,
    shdisp_ryoma_API_disp_clear_screen,
    shdisp_ryoma_API_diag_get_flicker_low_param,

    shdisp_ryoma_API_diag_set_gamma_info,
    shdisp_ryoma_API_diag_get_gamma_info,
    shdisp_ryoma_API_diag_set_gamma,

    shdisp_ryoma_API_power_on_recovery,
    shdisp_ryoma_API_power_off_recovery,
};

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */
#define SHDISP_RYOMA_SW_CHK_UPPER_UNIT

/* ------------------------------------------------------------------------- */
/* MACRAOS                                                                   */
/* ------------------------------------------------------------------------- */
#define MIPI_DSI_COMMAND_TX_CLMR(x)         (shdisp_panel_API_mipi_dsi_cmds_tx(&shdisp_mipi_ryoma_tx_buf, x, ARRAY_SIZE(x)))
#define MIPI_DSI_COMMAND_TX_CLMR_DUMMY(x)   (shdisp_panel_API_mipi_dsi_cmds_tx(&shdisp_mipi_ryoma_tx_buf, x, ARRAY_SIZE(x)))


/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_create                                                    */
/* ------------------------------------------------------------------------- */
struct shdisp_panel_operations *shdisp_ryoma_API_create(void)
{
    SHDISP_DEBUG("\n");
    return &shdisp_ryoma_fops;
}


static int shdisp_ryoma_mipi_dsi_buf_alloc(struct dsi_buf *dp, int size)
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
/* shdisp_ryoma_API_init_io                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_init_io(void)
{
    int ret = 0;
    struct shdisp_lcddr_phy_gamma_reg* phy_gamma;
    SHDISP_DEBUG("in\n");
    shdisp_ryoma_mipi_dsi_buf_alloc(&shdisp_mipi_ryoma_tx_buf, DSI_BUF_SIZE);
    shdisp_ryoma_mipi_dsi_buf_alloc(&shdisp_mipi_ryoma_rx_buf, DSI_BUF_SIZE);

    if (shdisp_api_get_boot_disp_status() == SHDISP_MAIN_DISP_ON) {
        shdisp_SYS_Host_control(SHDISP_HOST_CTL_CMD_LCD_CLK_START, SHDISP_RYOMA_CLOCK);
    }

    phy_gamma = shdisp_api_get_lcddr_phy_gamma();
    ret = shdisp_ryoma_init_phy_gamma(phy_gamma);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_ryoma_init_phy_gamma.\n");
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_empty_func                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_empty_func(void)
{
    SHDISP_DEBUG("\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_set_param                                                 */
/* ------------------------------------------------------------------------- */
static void shdisp_ryoma_API_set_param(struct shdisp_panel_param_str *param_str)
{
    SHDISP_DEBUG("in\n");
    ryoma_param_str.vcom_alpha = param_str->vcom_alpha;
    ryoma_param_str.vcom_alpha_low = param_str->vcom_alpha_low;
    SHDISP_DEBUG("out\n");

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_power_on                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_power_on(void)
{
    SHDISP_DEBUG("in\n");
    shdisp_clmr_api_disp_init();

    /* eR63311_rev100_120928.pdf */
    /* IOVCC ON */
    /* VS4 enters automatically at system start-up */

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    shdisp_bdic_API_LCD_vo2_off();
    shdisp_bdic_API_LCD_power_on();
    shdisp_bdic_API_LCD_m_power_on();
    shdisp_bdic_API_LCD_vo2_on();

#ifdef SHDISP_NOT_SUPPORT_DL32_BOARD
    gpio_request(SHDISP_RYOMA_RESET, "RYOMA_GPIO_RST");
    gpio_set_value(SHDISP_RYOMA_RESET, SHDISP_RYOMA_ENABLE);
#else
    shdisp_bdic_API_LCD_release_hw_reset();
    shdisp_SYS_cmd_delay_us(1000);
    shdisp_SYS_Host_control(SHDISP_HOST_CTL_CMD_LCD_CLK_START, SHDISP_RYOMA_CLOCK);
    shdisp_SYS_cmd_delay_us(3000);
#endif

    shdisp_clmr_api_auto_pat_ctrl(MIPI_SHARP_CLMR_AUTO_PAT_OFF);

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif
    shdisp_clmr_api_custom_blk_init();

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_power_off                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_power_off(void)
{
    SHDISP_DEBUG("in\n");

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    shdisp_clmr_api_display_stop();

    shdisp_bdic_API_LCD_m_power_off();
    shdisp_bdic_API_LCD_power_off();
#ifdef SHDISP_NOT_SUPPORT_DL32_BOARD
    shdisp_SYS_cmd_delay_us(10000);

    gpio_set_value(SHDISP_RYOMA_RESET, SHDISP_RYOMA_DISABLE);
    gpio_free(SHDISP_RYOMA_RESET);
#else
    shdisp_SYS_cmd_delay_us(3000);
    shdisp_bdic_API_LCD_set_hw_reset();
#endif
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    shdisp_SYS_Host_control(SHDISP_HOST_CTL_CMD_LCD_CLK_STOP, SHDISP_RYOMA_CLOCK);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_check_upper_unit                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_check_upper_unit(void)
{
#ifndef SHDISP_RYOMA_SW_CHK_UPPER_UNIT
#define SHDISP_GPIO_NUM_UPPER_UNIT  28          /* TEST_MODE */
    int val;
    SHDISP_DEBUG("in\n");
    gpio_request(SHDISP_GPIO_NUM_UPPER_UNIT, "upper_unit");
    val = gpio_get_value(SHDISP_GPIO_NUM_UPPER_UNIT);
    gpio_free(SHDISP_GPIO_NUM_UPPER_UNIT);
    SHDISP_DEBUG("check_upper_unit val=%d\n", val);
    if(val) {
        SHDISP_ERR("<OTHER> Upper unit does not exist.\n");
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_DEBUG("out\n");
#endif /* SHDISP_RYOMA_SW_CHK_UPPER_UNIT */
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_check_flicker_param                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    unsigned short tmp_alpha = alpha_in;

    SHDISP_DEBUG("in\n");
    if (alpha_out == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha_out.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if ((tmp_alpha & 0xF000) != 0x9000) {
        *alpha_out = SHDISP_NOT_ADJUST_VCOM_VAL;
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_SUCCESS;
    }

    tmp_alpha = tmp_alpha & 0x01FF;
    if ((tmp_alpha < SHDISP_RYOMA_VCOM_MIN) || (tmp_alpha > SHDISP_RYOMA_VCOM_MAX)) {
        *alpha_out = SHDISP_NOT_ADJUST_VCOM_VAL;
        SHDISP_DEBUG("out2\n");
        return SHDISP_RESULT_SUCCESS;
    }

    *alpha_out = tmp_alpha;

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_diag_write_reg                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_ryoma_tx_buf, addr, write_data, size);
    if(ret) {
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_diag_read_reg                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf, &shdisp_mipi_ryoma_rx_buf, addr, read_data, size);
    if(ret) {
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_diag_set_flicker_param                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_diag_set_flicker_param(unsigned short alpha)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int i;
    int ret = 0;

    SHDISP_DEBUG("in\n");
    if ((alpha < SHDISP_RYOMA_VCOM_MIN) || (alpha > SHDISP_RYOMA_VCOM_MAX)) {
        SHDISP_ERR("<INVALID_VALUE> alpha(0x%04X).\n", alpha);
        return SHDISP_RESULT_FAILURE;
    }

    for (i=1; i<=8; i++) {
        ryoma_rdata[i] = 0;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf, &shdisp_mipi_ryoma_rx_buf, 0xD5, ryoma_rdata, 8);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    ryoma_wdata[0] = ryoma_rdata[0];
    ryoma_wdata[1] = ryoma_rdata[1];
    ryoma_wdata[2] = ryoma_rdata[2];
    ryoma_wdata[3] = (unsigned char) (alpha & 0xFF);
    ryoma_wdata[4] = (unsigned char) (alpha & 0xFF);
    ryoma_wdata[5] = (unsigned char) (alpha & 0xFF);
    ryoma_wdata[6] = (unsigned char) (alpha & 0xFF);
    ryoma_wdata[7] = (unsigned char) (alpha & 0xFF);

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_ryoma_tx_buf, 0xD5, ryoma_wdata, 8);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    ryoma_param_str.vcom_alpha = alpha;
    ryoma_param_str.vcom_alpha_low = alpha;
    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_diag_get_flicker_param                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_diag_get_flicker_param(unsigned short *alpha)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int i;
    int ret = 0;

    SHDISP_DEBUG("in\n");
    if (alpha == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha.\n");
        return SHDISP_RESULT_FAILURE;
    }

    for (i=1; i<=8; i++) {
        ryoma_rdata[i] = 0;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf, &shdisp_mipi_ryoma_rx_buf, 0xD5, ryoma_rdata, 8);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    *alpha = ((unsigned short)ryoma_rdata[3]) & 0xFF;
    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_diag_get_flicker_low_param                               */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_diag_get_flicker_low_param(unsigned short *alpha)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int i;
    int ret = 0;
    
    if (alpha == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha.\n");
        return SHDISP_RESULT_FAILURE;
    }
    
    for (i=1; i<=8; i++) {
        ryoma_rdata[i] = 0;
    }
    
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf, &shdisp_mipi_ryoma_rx_buf, 0xD5, ryoma_rdata, 8);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    
    *alpha = ((unsigned short)ryoma_rdata[6]) & 0xFF;
    return SHDISP_RESULT_SUCCESS;
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_check_recovery                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_check_recovery(void)
{
#ifndef SHDISP_NOT_SUPPORT_DET
    int ret;

    SHDISP_DEBUG("in\n");
    ret = shdisp_bdic_API_RECOVERY_check_restoration();

    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_bdic_API_RECOVERY_check_restoration.\n");
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_recovery_type                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_recovery_type(int *type)
{
    SHDISP_DEBUG("in\n");
    *type = SHDISP_SUBSCRIBE_TYPE_INT;

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_set_abl_lut                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_disp_update                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_disp_update(struct shdisp_main_update *update)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_disp_clear_screen                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_disp_clear_screen(struct shdisp_main_clear *clear)
{
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_diag_set_gamma_info                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    int ret = 0;
    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_ryoma_tx_buf,
                                               SHDISP_RYOMA_GAMMA_SETTING_A,
                                               gamma_info->gammaR,
                                               SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_ryoma_tx_buf,
                                               SHDISP_RYOMA_GAMMA_SETTING_B,
                                               gamma_info->gammaG,
                                               SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
    }
    
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_ryoma_tx_buf,
                                               SHDISP_RYOMA_GAMMA_SETTING_C,
                                               gamma_info->gammaB,
                                               SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
    }
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_diag_get_gamma_info                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    int ret = 0;
    unsigned char ryoma_rdata[SHDISP_RYOMA_GAMMA_SETTING_SIZE];

    SHDISP_DEBUG("in\n");
    if (gamma_info == NULL){
        SHDISP_ERR("<NULL_POINTER> gamma_info.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    memset(ryoma_rdata, 0, SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf,
                                              &shdisp_mipi_ryoma_rx_buf,
                                              SHDISP_RYOMA_GAMMA_SETTING_A,
                                              ryoma_rdata,
                                              SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
    }
    memcpy(gamma_info->gammaR, ryoma_rdata, SHDISP_RYOMA_GAMMA_SETTING_SIZE);

    memset(ryoma_rdata, 0, SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf,
                                              &shdisp_mipi_ryoma_rx_buf,
                                              SHDISP_RYOMA_GAMMA_SETTING_B,
                                              ryoma_rdata,
                                              SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
    }
    memcpy(gamma_info->gammaG, ryoma_rdata, SHDISP_RYOMA_GAMMA_SETTING_SIZE);

    memset(ryoma_rdata, 0, SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf,
                                              &shdisp_mipi_ryoma_rx_buf,
                                              SHDISP_RYOMA_GAMMA_SETTING_C,
                                              ryoma_rdata,
                                              SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
    }
    memcpy(gamma_info->gammaB, ryoma_rdata, SHDISP_RYOMA_GAMMA_SETTING_SIZE);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_diag_set_gamma                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_diag_set_gamma(struct shdisp_diag_gamma *gamma)
{
    int ret = 0;
    unsigned char ryoma_rwdata[SHDISP_RYOMA_GAMMA_SETTING_SIZE];

    SHDISP_DEBUG("in\n");
    if ((gamma->level < SHDISP_RYOMA_GAMMA_LEVEL_MIN) || (gamma->level > SHDISP_RYOMA_GAMMA_LEVEL_MAX)) {
        SHDISP_ERR("<INVALID_VALUE> gamma->level(%d).\n", gamma->level);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    memset(ryoma_rwdata, 0, SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf,
                                              &shdisp_mipi_ryoma_rx_buf,
                                              SHDISP_RYOMA_GAMMA_SETTING_A,
                                              ryoma_rwdata,
                                              SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
    }

    ryoma_rwdata[gamma->level - 1]                                = gamma->gammaR_p;
    ryoma_rwdata[gamma->level + SHDISP_RYOMA_NEGATIVE_OFFSET - 1] = gamma->gammaR_n;
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_ryoma_tx_buf,
                                               SHDISP_RYOMA_GAMMA_SETTING_A,
                                               ryoma_rwdata,
                                               SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
    }

    memset(ryoma_rwdata, 0, SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf,
                                              &shdisp_mipi_ryoma_rx_buf,
                                              SHDISP_RYOMA_GAMMA_SETTING_B,
                                              ryoma_rwdata,
                                              SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
    }

    ryoma_rwdata[gamma->level - 1]                                = gamma->gammaG_p;
    ryoma_rwdata[gamma->level + SHDISP_RYOMA_NEGATIVE_OFFSET - 1] = gamma->gammaG_n;
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_ryoma_tx_buf,
                                               SHDISP_RYOMA_GAMMA_SETTING_B,
                                               ryoma_rwdata,
                                               SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
    }

    memset(ryoma_rwdata, 0, SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf,
                                              &shdisp_mipi_ryoma_rx_buf,
                                              SHDISP_RYOMA_GAMMA_SETTING_C,
                                              ryoma_rwdata,
                                              SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
    }

    ryoma_rwdata[gamma->level - 1]                                = gamma->gammaB_p;
    ryoma_rwdata[gamma->level + SHDISP_RYOMA_NEGATIVE_OFFSET - 1] = gamma->gammaB_n;
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_ryoma_tx_buf,
                                               SHDISP_RYOMA_GAMMA_SETTING_C,
                                               ryoma_rwdata,
                                               SHDISP_RYOMA_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


static char shdisp_ryoma_mipi_manufacture_id(void)
{
    struct dsi_buf *rp, *tp;
    struct dsi_cmd_desc *cmd;
    char *rdata;

    tp = &shdisp_mipi_ryoma_tx_buf;
    rp = &shdisp_mipi_ryoma_rx_buf;

    cmd = mipi_sh_ryoma_cmds_manufacture_id;
    shdisp_panel_API_mipi_dsi_cmds_rx(tp, rp, cmd, 5+8);

    rdata = rp->data;

    SHDISP_DEBUG("addr:BFh data: %02X %02X %02X %02X %02X\n", *(rdata+8), *(rdata+9), *(rdata+10), *(rdata+11), *(rdata+12));
    SHDISP_DEBUG("Header: %02X %02X %02X %02X %02X %02X %02X %02X\n", *(rdata+0), *(rdata+1), *(rdata+2), *(rdata+3), *(rdata+4), *(rdata+5), *(rdata+6), *(rdata+7));

#if 0
    {
        /* Device Code Read */
        static char mipi_sh_ryoma_cmd_b0  = 0xb0;    /* Read */
        static struct dsi_cmd_desc mipi_sh_ryoma_cmds_b0[] = {
            {DTYPE_GEN_READ1, 1, 0, 1, 0,
                1,
                &mipi_sh_ryoma_cmd_b0}
        };
        cmd = mipi_sh_ryoma_cmds_b0;
        shdisp_panel_API_mipi_dsi_cmds_rx(tp, rp, cmd, 1+8);
        rdata = rp->data;
        SHDISP_DEBUG("addr:B0h data: %02X\n", *(rdata+8));
        SHDISP_DEBUG("Header: %02X %02X %02X %02X %02X %02X %02X %02X\n", *(rdata+0), *(rdata+1), *(rdata+2), *(rdata+3), *(rdata+4), *(rdata+5), *(rdata+6), *(rdata+7));

    }
#endif
    return *(rdata+8);
}

static int shdisp_ryoma_mipi_cmd_start_display(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    /* Preparation display ON */
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_display_on_start);


#ifdef SHDISP_FW_STACK_EXCUTE
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
#endif

    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}

static int shdisp_ryoma_mipi_cmd_lcd_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_display_off);
#ifdef SHDISP_FW_STACK_EXCUTE
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
#endif

    mipi_sh_ryoma_clk_state[0] = mipi_sh_ryoma_cmd_low_power[1];

    shdisp_ryoma_mipi_tx_vcom_off_write();

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_display_off2);
#ifdef SHDISP_FW_STACK_EXCUTE
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
#endif
    SHDISP_DEBUG("out\n");
    return ret;
}

static int shdisp_ryoma_mipi_tx_vcom_write()
{
    struct dsi_cmd_desc cmd[1];
    char cmd_buf[13];
    unsigned short alpha;
    unsigned short alpha_low;
    char data;
    int ret;
    
    SHDISP_DEBUG("in\n");
    alpha = shdisp_api_get_alpha();
    alpha_low = shdisp_api_get_alpha_low();
    
    if( (alpha == SHDISP_NOT_ADJUST_VCOM_VAL) || (alpha_low == SHDISP_NOT_ADJUST_VCOM_VAL) ) {
        memcpy(&cmd_buf[0], mipi_sh_ryoma_cmd_vcom, 4);
        cmd[0].dlen = 4;
    }
    else {
        memcpy(&cmd_buf[0], mipi_sh_ryoma_cmd_vcom, 9);
        cmd[0].dlen = 9;
        cmd_buf[4]  = (unsigned char) (alpha & 0xFF);
        cmd_buf[5]  = (unsigned char) (alpha & 0xFF);
        data = (unsigned char) (alpha & 0xFF);
        if ((alpha % 2) == 1){
            data = (data - 1) / 2;
        }
        else {
            data = (data / 2);
        }
        if(data < SHDISP_RYOMA_VCOM_MIN){
            data = SHDISP_RYOMA_VCOM_MIN;
        }
        cmd_buf[6]  = data;
        cmd_buf[7]  = (unsigned char) (alpha_low & 0xFF);
        cmd_buf[8]  = (unsigned char) (alpha_low & 0xFF);
    }
    
    cmd[0].dtype = DTYPE_GEN_LWRITE;
    cmd[0].last = 0x01;
    cmd[0].vc = 0x00;
    cmd[0].ack = 0x00;
    cmd[0].wait = 0x00;
    cmd[0].payload = cmd_buf;
    
    ret = MIPI_DSI_COMMAND_TX_CLMR(cmd);
    
    SHDISP_DEBUG("out ret=%d\n", ret);
    return 0;
}

static int shdisp_ryoma_mipi_tx_vcom_off_write(void)
{
    struct dsi_cmd_desc cmd[1];
    unsigned short alpha;
    char cmd_buf[13];
    int paramCnt;
    char data;
    int ret;
    
    SHDISP_DEBUG("in\n");
#if 0
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_ryoma_tx_buf, &shdisp_mipi_ryoma_rx_buf, 0xD5, ryoma_rdata, 8);
    if(ret) {
        pr_err("%s: mipi_sharp_diag_read_reg fail.\n", __func__);
        return ret;
    }
    memcpy(&cmd_buf[0], mipi_sh_ryoma_cmd_vcom_off, 4);
    for(paramCnt = 4; paramCnt < 9; paramCnt++){
        data = (ryoma_rdata[paramCnt - 1] + 1) / 2;
        if(data < SHDISP_RYOMA_VCOM_MIN){
            data = SHDISP_RYOMA_VCOM_MIN;
        }
        cmd_buf[paramCnt] = data;
    }
#else
    alpha = shdisp_api_get_alpha();
    memcpy(&cmd_buf[0], mipi_sh_ryoma_cmd_vcom, 9);
    if(alpha != SHDISP_NOT_ADJUST_VCOM_VAL) {
        if ((alpha % 2) == 1){
            data = ((alpha & 0xFF) + 1) / 2;
        }
        else {
            data = (alpha & 0xFF) / 2;
        }
        if(data < SHDISP_RYOMA_VCOM_MIN){
            data = SHDISP_RYOMA_VCOM_MIN;
        }
    }
    else {
        data = SHDISP_RYOMA_VCOM_MIN;
    }
    for(paramCnt = 4; paramCnt < 9; paramCnt++){
        cmd_buf[paramCnt]  = data;
    }
#endif
    
    cmd[0].dtype = DTYPE_GEN_LWRITE;
    cmd[0].last = 0x01;
    cmd[0].vc = 0x00;
    cmd[0].ack = 0x00;
    cmd[0].wait = (18*1000);
    cmd[0].dlen = 9;
    cmd[0].payload = cmd_buf;
    
    ret = MIPI_DSI_COMMAND_TX_CLMR(cmd);
    
    SHDISP_DEBUG("out ret=%d\n", ret);
    return 0;
}

static int shdisp_ryoma_mipi_cmd_lcd_on(void)
{
    char device_id = 0x00;
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_protect);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out1 ret=%d\n", ret);
        return ret;
    }

    device_id = shdisp_ryoma_mipi_manufacture_id();

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_init);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out2 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_timing1);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out3 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_gamma);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out4 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_power1);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out5 ret=%d\n", ret);
        return ret;
    }

    shdisp_ryoma_mipi_tx_vcom_write();

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_power2);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out6 ret=%d\n", ret);
        return ret;
    }

    memcpy(&mipi_sh_ryoma_clk_state[0], &mipi_sh_ryoma_cmd_power4[1], 16);

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_sync);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out7 ret=%d\n", ret);
        return ret;
    }

    memcpy(&mipi_sh_ryoma_panel_sync[0], &mipi_sh_ryoma_cmd_set_sync1[1], 2);

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_display_on);
#ifdef SHDISP_FW_STACK_EXCUTE
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
#endif


    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}

#if defined(SW_NORMAL_DISPLAY)
static int shdisp_ryoma_mipi_cmd_lcd_display_normal_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    shdisp_clmr_api_auto_pat_ctrl(MIPI_SHARP_CLMR_AUTO_PAT_OFF);


    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}
static int shdisp_ryoma_mipi_cmd_lcd_off_black_screen_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    shdisp_clmr_api_auto_pat_ctrl(MIPI_SHARP_CLMR_1HZ_BLACK_ON);
    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}
#endif

int shdisp_ryoma_API_mipi_lcd_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");

    ret = shdisp_ryoma_mipi_cmd_lcd_on();

    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}

int shdisp_ryoma_API_mipi_lcd_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    ret = shdisp_ryoma_mipi_cmd_lcd_off();
    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}
int shdisp_ryoma_API_mipi_lcd_on_after_black_screen(void)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
    shdisp_ryoma_mipi_cmd_lcd_display_normal_on();
    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}

int shdisp_ryoma_API_mipi_lcd_off_black_screen_on(void)
{
    SHDISP_DEBUG("in\n");
    shdisp_ryoma_mipi_cmd_lcd_off_black_screen_on();
    SHDISP_DEBUG("out\n");
    return 0;
}

int shdisp_ryoma_API_mipi_start_display(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_clmr_api_data_transfer_starts();


#ifdef SHDISP_FW_STACK_EXCUTE
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
#endif

    shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_60_0);

    ret = shdisp_ryoma_mipi_cmd_start_display();

    shdisp_clmr_api_set_device();

    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}

int shdisp_ryoma_API_cabc_init(void)
{
    return 0;
}

int shdisp_ryoma_API_cabc_indoor_on(void)
{
    return 0;
}

int shdisp_ryoma_API_cabc_outdoor_on(int lut_level)
{
    return 0;
}

int shdisp_ryoma_API_cabc_off(int wait_on, int pwm_disable)
{
    return 0;
}

int shdisp_ryoma_API_cabc_outdoor_move(int lut_level)
{
    return 0;
}

/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_power_on_recovery                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_power_on_recovery(void)
{
    SHDISP_DEBUG("in\n");

    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_bdic_API_LCD_vo2_off();
    shdisp_bdic_API_LCD_power_on();
    shdisp_bdic_API_LCD_m_power_on();
    shdisp_bdic_API_LCD_vo2_on();
    shdisp_SYS_cmd_delay_us(5*1000);

    shdisp_bdic_API_LCD_release_hw_reset();

    shdisp_SYS_cmd_delay_us(10*1000);

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    SHDISP_DEBUG("out\n");

    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_ryoma_API_power_off_recovery                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_ryoma_API_power_off_recovery(void)
{
    SHDISP_DEBUG("in\n");

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_bdic_API_LCD_m_power_off();
    shdisp_bdic_API_LCD_power_off();
    shdisp_bdic_API_LCD_set_hw_reset();

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    SHDISP_DEBUG("out\n");

    return SHDISP_RESULT_SUCCESS;
}

#if defined (CONFIG_ANDROID_ENGINEERING)
void shdisp_ryoma_dbg_mipi_cmd_gamma(void)
{
    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_ryoma_cmds_gamma);
    SHDISP_DEBUG("out\n");
}

void shdisp_ryoma_dbg_mipi_cmd_testregister(void)
{
    static char TestRegister[2] = {0xFA, 0xAA};
    static struct dsi_cmd_desc TestRegister_cmd[] = {
        {DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(TestRegister), TestRegister}
    };

    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    MIPI_DSI_COMMAND_TX_CLMR(TestRegister_cmd);
    SHDISP_DEBUG("out\n");
}
#endif /* CONFIG_ANDROID_ENGINEERING */

int shdisp_ryoma_init_phy_gamma(struct shdisp_lcddr_phy_gamma_reg *phy_gamma)
{
    int ret = 0;

    int cnt;
    int checksum;

    SHDISP_DEBUG("in\n");
    mipi_sh_ryoma_set_val_gamma_a[0] = mipi_sh_ryoma_cmd_gamma_a[0];
    mipi_sh_ryoma_set_val_gamma_b[0] = mipi_sh_ryoma_cmd_gamma_b[0];
    mipi_sh_ryoma_set_val_gamma_c[0] = mipi_sh_ryoma_cmd_gamma_c[0];
    if(phy_gamma == NULL) {
        pr_err("%s: phy_gamma is NULL.\n", __func__);
        ret = -1;
    }
    else if((phy_gamma->status != SHDISP_LCDDR_GAMMA_STATUS_OK) && 
            (phy_gamma->status != SHDISP_LCDDR_GAMMA_STATUS_OK_2) ) {
        pr_err("%s: gammg status invalid. status=%02x\n", __func__, phy_gamma->status);
        ret = -1;
    }
    else {
        checksum = phy_gamma->status;
        for(cnt = 0; cnt < SHDISP_LCDDR_PHY_GAMMA_BUF_MAX; cnt++) {
            checksum = checksum + phy_gamma->buf[cnt];
        }
        if((checksum & 0xFFFF) != phy_gamma->chksum) {
            pr_err("%s: gammg chksum NG. chksum=%04x calc_chksum=%04x\n", __func__, phy_gamma->chksum, (checksum & 0xFFFF));
            ret = -1;
        }
        else {
            for(cnt = 0; cnt < SHDISP_LCDDR_GAMMA_OFFSET; cnt++) {
                mipi_sh_ryoma_set_val_gamma_a[cnt + 1]                             = phy_gamma->buf[cnt];
                mipi_sh_ryoma_set_val_gamma_a[cnt + SHDISP_LCDDR_GAMMA_OFFSET + 1] = phy_gamma->buf[cnt + SHDISP_LCDDR_GAMMA_OFFSET];
                mipi_sh_ryoma_set_val_gamma_b[cnt + 1]                             = phy_gamma->buf[cnt + SHDISP_LCDDR_GAMMA_OFFSET * 2];
                mipi_sh_ryoma_set_val_gamma_b[cnt + SHDISP_LCDDR_GAMMA_OFFSET + 1] = phy_gamma->buf[cnt + SHDISP_LCDDR_GAMMA_OFFSET * 3];
                mipi_sh_ryoma_set_val_gamma_c[cnt + 1]                             = phy_gamma->buf[cnt + SHDISP_LCDDR_GAMMA_OFFSET * 4];
                mipi_sh_ryoma_set_val_gamma_c[cnt + SHDISP_LCDDR_GAMMA_OFFSET + 1] = phy_gamma->buf[cnt + SHDISP_LCDDR_GAMMA_OFFSET * 5];
            }
        }
    }
    if(ret < 0) {
        for(cnt = 1; cnt < SHDISP_LCDDR_GAMMA_OFFSET + 1; cnt++) {
            mipi_sh_ryoma_set_val_gamma_a[cnt]                             = mipi_sh_ryoma_cmd_gamma_a[cnt];
            mipi_sh_ryoma_set_val_gamma_a[cnt + SHDISP_LCDDR_GAMMA_OFFSET] = mipi_sh_ryoma_cmd_gamma_a[cnt + SHDISP_LCDDR_GAMMA_OFFSET];
            mipi_sh_ryoma_set_val_gamma_b[cnt]                             = mipi_sh_ryoma_cmd_gamma_b[cnt];
            mipi_sh_ryoma_set_val_gamma_b[cnt + SHDISP_LCDDR_GAMMA_OFFSET] = mipi_sh_ryoma_cmd_gamma_b[cnt + SHDISP_LCDDR_GAMMA_OFFSET];
            mipi_sh_ryoma_set_val_gamma_c[cnt]                             = mipi_sh_ryoma_cmd_gamma_c[cnt];
            mipi_sh_ryoma_set_val_gamma_c[cnt + SHDISP_LCDDR_GAMMA_OFFSET] = mipi_sh_ryoma_cmd_gamma_c[cnt + SHDISP_LCDDR_GAMMA_OFFSET];
        }
    }
    memcpy(&mipi_sh_ryoma_set_val_cmd_timing4[0], &mipi_sh_ryoma_cmd_timing4[0], sizeof(mipi_sh_ryoma_cmd_timing4));
    if((ret == 0) && (phy_gamma->status == SHDISP_LCDDR_GAMMA_STATUS_OK_2)) {
        mipi_sh_ryoma_set_val_cmd_timing4[5] = 0x43;
        mipi_sh_ryoma_set_val_cmd_timing4[11] = 0x03;
    }
    SHDISP_DEBUG("out ret=%04x\n", ret);
    return ret;
}

MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
