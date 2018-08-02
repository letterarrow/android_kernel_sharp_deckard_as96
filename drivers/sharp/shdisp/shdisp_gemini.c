/* drivers/sharp/shdisp/shdisp_gemini.c  (Display Driver)
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
#include "shdisp_panel.h"
#include "shdisp_gemini.h"
#include "shdisp_system.h"
#include "shdisp_bdic.h"
#include "../../video/msm/msm_fb_panel.h"
#include "../../video/msm/mipi_sharp_clmr.h"
#include "shdisp_type.h"
#include "shdisp_pm.h"
#include "shdisp_clmr.h"
#include "shdisp_dbg.h"



/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define SHDISP_gemini_PROVISIONAL_REG_RW
#define SHDISP_FW_STACK_EXCUTE

#define SHDISP_gemini_RESET      32
#define SHDISP_gemini_DISABLE    0
#define SHDISP_gemini_ENABLE     1

//#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
   static unsigned char current_page = 0;
//#endif

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

static int shdisp_gemini_API_init_io(void);
static int shdisp_gemini_API_empty_func(void);
static void shdisp_gemini_API_set_param(struct shdisp_panel_param_str *param_str);
static int shdisp_gemini_API_power_on(void);
static int shdisp_gemini_API_power_off(void);
static int shdisp_gemini_API_check_upper_unit(void);
static int shdisp_gemini_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out);
static int shdisp_gemini_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size);
static int shdisp_gemini_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size);
static int shdisp_gemini_API_diag_set_flicker_param(unsigned short alpha);
static int shdisp_gemini_API_diag_get_flicker_param(unsigned short *alpha);
static int shdisp_gemini_API_check_recovery(void);
static int shdisp_gemini_API_recovery_type(int *type);
static int shdisp_gemini_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma);
static int shdisp_gemini_API_disp_update(struct shdisp_main_update *update);
static int shdisp_gemini_API_disp_clear_screen(struct shdisp_main_clear *clear);
static int shdisp_gemini_API_diag_get_flicker_low_param(unsigned short *alpha);
static int shdisp_gemini_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
static int shdisp_gemini_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
static int shdisp_gemini_API_diag_set_gamma(struct shdisp_diag_gamma *gamma);
static int shdisp_gemini_API_power_on_recovery(void);
static int shdisp_gemini_API_power_off_recovery(void);
static int shdisp_gemini_API_set_drive_freq(int type);
static int shdisp_gemini_API_diag_set_flicker_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param);
static int shdisp_gemini_API_diag_get_flicker_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param);
static int shdisp_gemini_API_diag_get_flicker_low_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param);
static int shdisp_gemini_API_get_drive_freq(void);

static unsigned char shdisp_gemini_get_device_code(void);
unsigned char shdisp_gemini_mipi_manufacture_id(void);

void shdisp_gemini_init_flicker_param(struct shdisp_diag_flicker_param *flicker_param);
void shdisp_gemini_init_flicker_param_low(struct shdisp_diag_flicker_param *flicker_param);
void shdisp_gemini_init_phy_gamma(struct shdisp_lcddr_phy_gamma_reg *phy_gamma);


/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static struct shdisp_panel_param_str gemini_param_str;
static struct dsi_buf shdisp_mipi_gemini_tx_buf;
static struct dsi_buf shdisp_mipi_gemini_rx_buf;

static int           hw_revision = 0;

static int drive_freq_lasy_type = SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A;

/* ------------------------------------------------------------------------- */
/*      packet header                                                        */
/* ------------------------------------------------------------------------- */
/*      LCD ON                                                              */
/*      Initial Setting                                                     */

static char mipi_sh_gemini_cmd_Switch_to_Page[10][2] = {
    {0xB0, 0x00},
    {0xB0, 0x01},
    {0xB0, 0x02},
    {0xB0, 0x03},
    {0xB0, 0x04},
    {0xB0, 0x05},
    {0xB0, 0x06},
    {0xB0, 0x07},
    {0xB0, 0x08},
    {0xB0, 0x09}
};

static char mipi_sh_gemini_cmd_deviceCode[1] = {0xB1};

static char mipi_sh_gemini_cmd_DisplaySettings[12][2] = {
    {0xB2, 0x72},
    {0xB3, 0x08},
    {0xB4, 0x0C},
    {0xB5, 0x00},
    {0xB6, 0x50},
    {0xB7, 0x00},
    {0xB8, 0x03},
    {0xB9, 0xC0},
    {0xBA, 0x08},
    {0xBB, 0x00},
    {0xBC, 0x02},
    {0xBD, 0x00}
};

#define SHDISP_GEMINI_NO_VSPS_VSNS      0
#define SHDISP_GEMINI_NO_VGHS           1
#define SHDISP_GEMINI_NO_VGLS           2
#define SHDISP_GEMINI_NO_VPH_VPL        5
#define SHDISP_GEMINI_NO_VNH_VNL        6

static char mipi_sh_gemini_cmd_master_VoltageSettings[7][2] = {
    {0xBE, 0xAA},
    {0xBF, 0x14},
    {0xC0, 0x26},
    {0xC1, 0x33},
    {0xC4, 0x05},
    {0xC5, 0x75},
    {0xC6, 0x75}
};
static char mipi_sh_gemini_cmd_slave__VoltageSettings[7][2] = {
    {0xBE, 0xAA},
    {0xBF, 0x14},
    {0xC0, 0x26},
    {0xC1, 0x33},
    {0xC4, 0x05},
    {0xC5, 0x75},
    {0xC6, 0x75}
};

#define SHDISP_GEMINI_ANALOG_GAMMA_LEVEL_MIN                1
#define SHDISP_GEMINI_ANALOG_GAMMA_LEVEL_MAX                12
#define SHDISP_GEMINI_ANALOG_GAMMA_NEGATIVE_OFFSET          (SHDISP_PANEL_ANALOG_GAMMA_TBL_SIZE / 2)

static char mipi_sh_gemini_cmd_master_AnalogGammaSetting[24][2] = {
    {0xC7, 0x00},
    {0xC8, 0x0A},
    {0xC9, 0x13},
    {0xCA, 0x20},
    {0xCB, 0x30},
    {0xCC, 0x13},
    {0xCD, 0x1A},
    {0xCE, 0x43},
    {0xCF, 0x4B},
    {0xD0, 0x56},
    {0xD1, 0x68},
    {0xD2, 0x01},
    {0xD3, 0x00},
    {0xD4, 0x0A},
    {0xD5, 0x13},
    {0xD6, 0x20},
    {0xD7, 0x30},
    {0xD8, 0x13},
    {0xD9, 0x1A},
    {0xDA, 0x43},
    {0xDB, 0x4B},
    {0xDC, 0x56},
    {0xDD, 0x68},
    {0xDE, 0x01}
};
static char mipi_sh_gemini_cmd_slave__AnalogGammaSetting[24][2] = {
    {0xC7, 0x00},
    {0xC8, 0x0A},
    {0xC9, 0x13},
    {0xCA, 0x20},
    {0xCB, 0x30},
    {0xCC, 0x13},
    {0xCD, 0x1A},
    {0xCE, 0x43},
    {0xCF, 0x4B},
    {0xD0, 0x56},
    {0xD1, 0x68},
    {0xD2, 0x01},
    {0xD3, 0x00},
    {0xD4, 0x0A},
    {0xD5, 0x13},
    {0xD6, 0x20},
    {0xD7, 0x30},
    {0xD8, 0x13},
    {0xD9, 0x1A},
    {0xDA, 0x43},
    {0xDB, 0x4B},
    {0xDC, 0x56},
    {0xDD, 0x68},
    {0xDE, 0x01}
};

static char mipi_sh_gemini_cmd_BiasSettings[7][2] = {
    {0xDF, 0x33},
    {0xE0, 0x00},
    {0xE1, 0x01},
    {0xE2, 0x00},
    {0xE3, 0x05},
    {0xE4, 0x11},
    {0xE5, 0x33}
};

#define SHDISP_GEMINI_VCOM_MIN          0x0000
#define SHDISP_GEMINI_VCOM_MAX          0x01FF
#define VCOMDC_1                        0x01
#define VCOMDC_2                        0x72
#define SHDISP_GEMINI_VCOMDC            ((VCOMDC_1 << 8) | VCOMDC_2)
#define SHDISP_GEMINI_NO_VCOMDC_1       1
#define SHDISP_GEMINI_NO_VCOMDC_2       2
#define SHDISP_GEMINI_NO_LPVCOMDC1_1    3
#define SHDISP_GEMINI_NO_LPVCOMDC1_2    4
#define SHDISP_GEMINI_NO_LPVCOMDC2_1    5
#define SHDISP_GEMINI_NO_LPVCOMDC2_2    6
#define SHDISP_GEMINI_NO_LPVCOMDC3_1    7
#define SHDISP_GEMINI_NO_LPVCOMDC3_2    8
#define SHDISP_GEMINI_NO_PFVCOMDC_1     9
#define SHDISP_GEMINI_NO_PFVCOMDC_2     10

static char mipi_sh_gemini_cmd_master_VCOMSettings[12][2] = {
    {0xE6, 0x30},
    {0xE7, VCOMDC_1},
    {0xE8, VCOMDC_2},
    {0xE9, 0x01},
    {0xEA, 0x72},
    {0xEB, 0x01},
    {0xEC, 0x72},
    {0xED, 0x01},
    {0xEE, 0x72},
    {0xEF, 0x01},
    {0xF0, 0xCC},
    {0xF1, 0x00}
};
static char mipi_sh_gemini_cmd_slave__VCOMSettings[12][2] = {
    {0xE6, 0x30},
    {0xE7, VCOMDC_1},
    {0xE8, VCOMDC_2},
    {0xE9, 0x01},
    {0xEA, 0x72},
    {0xEB, 0x01},
    {0xEC, 0x72},
    {0xED, 0x01},
    {0xEE, 0x72},
    {0xEF, 0x01},
    {0xF0, 0xCC},
    {0xF1, 0x00}
};

#define SHDISP_GEMINI_GDM_P1_B6       0x6D
#define SHDISP_GEMINI_GDM_P1_B7       0x6D
#define SHDISP_GEMINI_GDM_P1_B6_B     0x68
#define SHDISP_GEMINI_GDM_P1_B7_B     0x68

static char mipi_sh_gemini_cmd_GDMSettings[22][2] = {
    {0xB1, 0x7E},
    {0xB2, 0x78},
    {0xB3, 0x00},
    {0xB4, 0x44},
    {0xB5, 0x00},
    {0xB6, SHDISP_GEMINI_GDM_P1_B6},
    {0xB7, SHDISP_GEMINI_GDM_P1_B7},
    {0xB8, 0x09},
    {0xB9, 0x0A},
    {0xBA, 0x00},
    {0xBB, 0x00},
    {0xBC, 0x0F},
    {0xBD, 0x06},
    {0xBE, 0xF0},
    {0xBF, 0x60},
    {0xC2, 0x02},
    {0xC3, 0x06},
    {0xC4, 0x0F},
    {0xC5, 0x00},
    {0xC6, 0x41},
    {0xC7, 0x01},
    {0xC8, 0x19}
};

static char mipi_sh_gemini_cmd_StopSettings[8][2] = {
    {0xB1, 0x61},
    {0xB2, 0x00},
    {0xB3, 0x00},
    {0xB4, 0x00},
    {0xB5, 0x3C},
    {0xB6, 0x00},
    {0xB7, 0x03},
    {0xB8, 0x08}
};

static char mipi_sh_gemini_cmd_PowerON_OFF[31][2] = {
    {0xBA, 0x04},
    {0xBB, 0x55},
    {0xBC, 0x11},
    {0xBD, 0x00},
    {0xBE, 0x01},
    {0xBF, 0x00},
    {0xC0, 0x03},
    {0xC1, 0x04},
    {0xC2, 0x00},
    {0xC3, 0x03},
    {0xC4, 0x00},
    {0xC5, 0x03},
    {0xC6, 0x00},
    {0xC7, 0x73},
    {0xC8, 0x77},
    {0xC9, 0x07},
    {0xCA, 0x03},
    {0xCB, 0x00},
    {0xCC, 0x00},
    {0xCD, 0x01},
    {0xCE, 0x01},
    {0xCF, 0xA9},
    {0xD0, 0x02},
    {0xD1, 0x01},
    {0xD2, 0x00},
    {0xD3, 0x30},
    {0xD4, 0xFF},
    {0xD5, 0x00},
    {0xD6, 0x00},
    {0xD7, 0x1F},
    {0xE0, 0x3C}
};

static char mipi_sh_gemini_cmd_PSOS_signalSettings[2][2] = {
    {0xB5, 0x00},
    {0xB6, 0x02}
};

static char mipi_sh_gemini_cmd_CABC_CE_ON_OFF[2][2] = {
    {0xB1, 0x00},
    {0xC0, 0x00}
};

static char mipi_sh_gemini_cmd_master_DigitalGammaSettings[54][2] = {
    {0xB1, 0x0F},
    {0xB2, 0xFE},
    {0xB3, 0x94},
    {0xB4, 0x00},
    {0xB5, 0x00},
    {0xB6, 0x20},
    {0xB7, 0x40},
    {0xB8, 0x80},
    {0xB9, 0xC0},
    {0xBA, 0x00},
    {0xBB, 0x80},
    {0xBC, 0x00},
    {0xBD, 0x80},
    {0xBE, 0x00},
    {0xBF, 0x80},
    {0xC0, 0xC0},
    {0xC1, 0xE0},
    {0xC2, 0xFC},
    {0xC3, 0x0F},
    {0xC4, 0xFE},
    {0xC5, 0x94},
    {0xC6, 0x00},
    {0xC7, 0x00},
    {0xC8, 0x20},
    {0xC9, 0x40},
    {0xCA, 0x80},
    {0xCB, 0xC0},
    {0xCC, 0x00},
    {0xCD, 0x80},
    {0xCE, 0x00},
    {0xCF, 0x80},
    {0xD0, 0x00},
    {0xD1, 0x80},
    {0xD2, 0xC0},
    {0xD3, 0xE0},
    {0xD4, 0xFC},
    {0xD5, 0x0F},
    {0xD6, 0xFE},
    {0xD7, 0x94},
    {0xD8, 0x00},
    {0xD9, 0x00},
    {0xDA, 0x20},
    {0xDB, 0x40},
    {0xDC, 0x80},
    {0xDD, 0xC0},
    {0xDE, 0x00},
    {0xDF, 0x80},
    {0xE0, 0x00},
    {0xE1, 0x80},
    {0xE2, 0x00},
    {0xE3, 0x80},
    {0xE4, 0xC0},
    {0xE5, 0xE0},
    {0xE6, 0xFC}
};
static char mipi_sh_gemini_cmd_slave__DigitalGammaSettings[54][2] = {
    {0xB1, 0x0F},
    {0xB2, 0xFE},
    {0xB3, 0x94},
    {0xB4, 0x00},
    {0xB5, 0x00},
    {0xB6, 0x20},
    {0xB7, 0x40},
    {0xB8, 0x80},
    {0xB9, 0xC0},
    {0xBA, 0x00},
    {0xBB, 0x80},
    {0xBC, 0x00},
    {0xBD, 0x80},
    {0xBE, 0x00},
    {0xBF, 0x80},
    {0xC0, 0xC0},
    {0xC1, 0xE0},
    {0xC2, 0xFC},
    {0xC3, 0x0F},
    {0xC4, 0xFE},
    {0xC5, 0x94},
    {0xC6, 0x00},
    {0xC7, 0x00},
    {0xC8, 0x20},
    {0xC9, 0x40},
    {0xCA, 0x80},
    {0xCB, 0xC0},
    {0xCC, 0x00},
    {0xCD, 0x80},
    {0xCE, 0x00},
    {0xCF, 0x80},
    {0xD0, 0x00},
    {0xD1, 0x80},
    {0xD2, 0xC0},
    {0xD3, 0xE0},
    {0xD4, 0xFC},
    {0xD5, 0x0F},
    {0xD6, 0xFE},
    {0xD7, 0x94},
    {0xD8, 0x00},
    {0xD9, 0x00},
    {0xDA, 0x20},
    {0xDB, 0x40},
    {0xDC, 0x80},
    {0xDD, 0xC0},
    {0xDE, 0x00},
    {0xDF, 0x80},
    {0xE0, 0x00},
    {0xE1, 0x80},
    {0xE2, 0x00},
    {0xE3, 0x80},
    {0xE4, 0xC0},
    {0xE5, 0xE0},
    {0xE6, 0xFC}
};

static char mipi_sh_gemini_cmd_MIPI_CLK_Settings[10][2] = {
    {0xB1, 0x22},
    {0xB2, 0x00},
    {0xB3, 0x00},
    {0xB4, 0x13},
    {0xB5, 0x00},
    {0xB6, 0xAA},
    {0xB7, 0xAA},
    {0xB8, 0x0A},
    {0xB9, 0x30},
    {0xBA, 0x00}
};

static char mipi_sh_gemini_cmd_DisplaySettings2[1][2] = {
    {0xC9, 0x08}
};

static char mipi_sh_gemini_cmd_DisplaySettings3[3][2] = {
    {0xB5, 0x00},
    {0xB8, 0xA5},
    {0xB7, 0xD0}
};

static char mipi_sh_gemini_cmd_set_display_on[1] = {0x29};
static char mipi_sh_gemini_cmd_exit_sleep_mode[1] = {0x11};

static char mipi_sh_gemini_cmd_SetDisplayOff[1] = {0x28};
static char mipi_sh_gemini_cmd_EnterSleepMode[1] = {0x10};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_enter_sleep_mode[] = {
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 1, mipi_sh_gemini_cmd_EnterSleepMode}
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_0[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[0]}
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_deviceCode[] = {
    {DTYPE_DCS_READ   | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 1, 0, 1, mipi_sh_gemini_cmd_deviceCode}
};

#ifndef SHDISP_GEMINI_ES1_CM_SAMPLE
static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_1[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[12]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[12]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[13]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[13]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[14]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[14]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[15]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[15]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[16]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[16]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[17]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[17]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[18]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[18]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[19]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[19]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[20]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[20]},
    {DTYPE_DCS_WRITE1 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[21]},
    {DTYPE_DCS_WRITE1 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[21]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[22]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[22]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[23]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[23]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[11]}
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_VCOMSettings[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[10]},
};
#endif  /* SHDISP_GEMINI_ES1_CM_SAMPLE */

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_1_B[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VoltageSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VoltageSettings[4]},
#ifndef SHDISP_GEMINI_ES1_CM_SAMPLE
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[12]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[12]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[13]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[13]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[14]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[14]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[15]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[15]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[16]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[16]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[17]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[17]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[18]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[18]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[19]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[19]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[20]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[20]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[21]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[21]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[22]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[22]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_AnalogGammaSetting[23]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__AnalogGammaSetting[23]},
#endif  /* SHDISP_GEMINI_ES1_CM_SAMPLE */
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_BiasSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_master_VCOMSettings[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_slave__VCOMSettings[11]}
};


static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_2[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[12]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[13]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[14]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[15]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[16]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[17]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[18]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[19]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[20]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_GDMSettings[21]}
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_3[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_StopSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_StopSettings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_StopSettings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_StopSettings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_StopSettings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_StopSettings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_StopSettings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_StopSettings[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[10]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[11]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[12]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[13]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[14]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[15]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[16]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[17]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[18]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[19]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[20]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[21]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[22]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[23]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[24]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[25]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[26]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[27]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[28]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[29]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PowerON_OFF[30]}
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_4[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PSOS_signalSettings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_PSOS_signalSettings[1]}
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_5[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_CABC_CE_ON_OFF[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_CABC_CE_ON_OFF[1]}
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_7[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[3]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[4]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[5]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[6]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[7]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_MIPI_CLK_Settings[9]}
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_8[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[8]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings2[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[0]},
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_9[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings3[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings3[1]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings3[2]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[0]},
};


static struct dsi_cmd_desc mipi_sh_gemini_cmds_proc_9_cut2[] = {
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[9]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_DisplaySettings3[0]},
    {DTYPE_GEN_WRITE2 | SHDISP_CLMR_FWCMD_DSI_DSI_TXC, 1, 0, 0, 0, 2, mipi_sh_gemini_cmd_Switch_to_Page[0]},
};

static struct dsi_cmd_desc mipi_sh_gemini_cmds_display_on1[] = {
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 1, mipi_sh_gemini_cmd_exit_sleep_mode},
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 1, mipi_sh_gemini_cmd_exit_sleep_mode},
};
static struct dsi_cmd_desc mipi_sh_gemini_cmds_display_on2[] = {
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, 0, 1, mipi_sh_gemini_cmd_set_display_on},
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0, 1, mipi_sh_gemini_cmd_set_display_on},
};


static struct dsi_cmd_desc mipi_sh_gemini_cmds_display_off[] = {
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0,                  1, mipi_sh_gemini_cmd_SetDisplayOff},
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, (1*WAIT_1FRAME_US), 1, mipi_sh_gemini_cmd_SetDisplayOff},
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TX1, 1, 0, 0, 0,                  1, mipi_sh_gemini_cmd_EnterSleepMode},
    {DTYPE_DCS_WRITE | SHDISP_CLMR_FWCMD_DSI_DSI_TX0, 1, 0, 0, (4*WAIT_1FRAME_US), 1, mipi_sh_gemini_cmd_EnterSleepMode},
};

const static unsigned char freq_drive_a[] = {
    (CALI_PLL1CTL_VAL & 0x000000FF) >>  0,
    (CALI_PLL1CTL_VAL & 0x0000FF00) >>  8,
    (CALI_PLL1CTL_VAL & 0x00FF0000) >> 16,
    (CALI_PLL1CTL_VAL & 0xFF000000) >> 24,
    (CALI_PLL1CTL2_VAL & 0x000000FF) >>  0,
    (CALI_PLL1CTL2_VAL & 0x0000FF00) >>  8,
    (CALI_PLL1CTL2_VAL & 0x00FF0000) >> 16,
    (CALI_PLL1CTL2_VAL & 0xFF000000) >> 24,
    (CALI_PLL1CTL3_VAL & 0x000000FF) >>  0,
    (CALI_PLL1CTL3_VAL & 0x0000FF00) >>  8,
    (CALI_PLL1CTL3_VAL & 0x00FF0000) >> 16,
    (CALI_PLL1CTL3_VAL & 0xFF000000) >> 24,
    (CALI_GPDIV_VAL & 0x00FF) >> 0,
    (CALI_GPDIV_VAL & 0xFF00) >> 8,
    (CALI_PTGHF_VAL & 0x00FF) >> 0,
    (CALI_PTGHF_VAL & 0xFF00) >> 8,
    (CALI_PTGHP_VAL & 0x00FF) >> 0,
    (CALI_PTGHP_VAL & 0xFF00) >> 8,
    (CALI_PTGHB_VAL & 0x00FF) >> 0,
    (CALI_PTGHB_VAL & 0xFF00) >> 8,
    (CALI_PTGVF_VAL & 0x00FF) >> 0,
    (CALI_PTGVF_VAL & 0xFF00) >> 8,
    (CALI_PTGVP_VAL & 0x00FF) >> 0,
    (CALI_PTGVP_VAL & 0xFF00) >> 8,
    (CALI_PTGVB_VAL & 0x00FF) >> 0,
    (CALI_PTGVB_VAL & 0xFF00) >> 8,
    SHDISP_GEMINI_GDM_P1_B6,
    SHDISP_GEMINI_GDM_P1_B7,
};

const static unsigned char freq_drive_b[] = {
    (CALI_PLL1CTL_VAL_B & 0x000000FF) >>  0,
    (CALI_PLL1CTL_VAL_B & 0x0000FF00) >>  8,
    (CALI_PLL1CTL_VAL_B & 0x00FF0000) >> 16,
    (CALI_PLL1CTL_VAL_B & 0xFF000000) >> 24,
    (CALI_PLL1CTL2_VAL_B & 0x000000FF) >>  0,
    (CALI_PLL1CTL2_VAL_B & 0x0000FF00) >>  8,
    (CALI_PLL1CTL2_VAL_B & 0x00FF0000) >> 16,
    (CALI_PLL1CTL2_VAL_B & 0xFF000000) >> 24,
    (CALI_PLL1CTL3_VAL_B & 0x000000FF) >>  0,
    (CALI_PLL1CTL3_VAL_B & 0x0000FF00) >>  8,
    (CALI_PLL1CTL3_VAL_B & 0x00FF0000) >> 16,
    (CALI_PLL1CTL3_VAL_B & 0xFF000000) >> 24,
    (CALI_GPDIV_VAL_B & 0x00FF) >> 0,
    (CALI_GPDIV_VAL_B & 0xFF00) >> 8,
    (CALI_PTGHF_VAL_B & 0x00FF) >> 0,
    (CALI_PTGHF_VAL_B & 0xFF00) >> 8,
    (CALI_PTGHP_VAL_B & 0x00FF) >> 0,
    (CALI_PTGHP_VAL_B & 0xFF00) >> 8,
    (CALI_PTGHB_VAL_B & 0x00FF) >> 0,
    (CALI_PTGHB_VAL_B & 0xFF00) >> 8,
    (CALI_PTGVF_VAL_B & 0x00FF) >> 0,
    (CALI_PTGVF_VAL_B & 0xFF00) >> 8,
    (CALI_PTGVP_VAL_B & 0x00FF) >> 0,
    (CALI_PTGVP_VAL_B & 0xFF00) >> 8,
    (CALI_PTGVB_VAL_B & 0x00FF) >> 0,
    (CALI_PTGVB_VAL_B & 0xFF00) >> 8,
    SHDISP_GEMINI_GDM_P1_B6_B,
    SHDISP_GEMINI_GDM_P1_B7_B,
};

static struct shdisp_panel_operations shdisp_gemini_fops = {
    shdisp_gemini_API_init_io,
    shdisp_gemini_API_empty_func,
    shdisp_gemini_API_empty_func,
    shdisp_gemini_API_set_param,
    shdisp_gemini_API_power_on,
    shdisp_gemini_API_power_off,
    shdisp_gemini_API_empty_func,
    shdisp_gemini_API_empty_func,
    shdisp_gemini_API_empty_func,
    shdisp_gemini_API_empty_func,
    shdisp_gemini_API_empty_func,
    shdisp_gemini_API_check_upper_unit,
    shdisp_gemini_API_check_flicker_param,
    shdisp_gemini_API_diag_write_reg,
    shdisp_gemini_API_diag_read_reg,
    shdisp_gemini_API_diag_set_flicker_param,
    shdisp_gemini_API_diag_get_flicker_param,
    shdisp_gemini_API_check_recovery,
    shdisp_gemini_API_recovery_type,
    shdisp_gemini_API_set_abl_lut,
    shdisp_gemini_API_disp_update,
    shdisp_gemini_API_disp_clear_screen,
    shdisp_gemini_API_diag_get_flicker_low_param,
    shdisp_gemini_API_diag_set_gamma_info,
    shdisp_gemini_API_diag_get_gamma_info,
    shdisp_gemini_API_diag_set_gamma,
    shdisp_gemini_API_power_on_recovery,
    shdisp_gemini_API_power_off_recovery,
    shdisp_gemini_API_set_drive_freq,
    shdisp_gemini_API_diag_set_flicker_param_multi_cog,
    shdisp_gemini_API_diag_get_flicker_param_multi_cog,
    shdisp_gemini_API_diag_get_flicker_low_param_multi_cog,
    shdisp_gemini_API_get_drive_freq,
};

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */
#define SHDISP_gemini_SW_CHK_UPPER_UNIT

/* ------------------------------------------------------------------------- */
/* MACRAOS                                                                   */
/* ------------------------------------------------------------------------- */
#define MIPI_DSI_COMMAND_TX_CLMR(x)         (shdisp_panel_API_mipi_dsi_cmds_tx(&shdisp_mipi_gemini_tx_buf, x, ARRAY_SIZE(x)))
#define MIPI_DSI_COMMAND_TX_CLMR_DUMMY(x)   (shdisp_panel_API_mipi_dsi_cmds_tx_dummy(x))

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_create                                                    */
/* ------------------------------------------------------------------------- */
struct shdisp_panel_operations *shdisp_gemini_API_create(void)
{
    SHDISP_DEBUG("\n");
    return &shdisp_gemini_fops;
}


static int shdisp_gemini_mipi_dsi_buf_alloc(struct dsi_buf *dp, int size)
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
/* shdisp_gemini_API_init_io                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_init_io(void)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    struct shdisp_diag_flicker_param flicker_param;
    struct shdisp_diag_flicker_param flicker_param_low;
#endif
    struct shdisp_lcddr_phy_gamma_reg* phy_gamma;

    SHDISP_DEBUG("in\n");
    shdisp_gemini_mipi_dsi_buf_alloc(&shdisp_mipi_gemini_tx_buf, DSI_BUF_SIZE);
    shdisp_gemini_mipi_dsi_buf_alloc(&shdisp_mipi_gemini_rx_buf, DSI_BUF_SIZE);

#ifndef SHDISP_NOT_SUPPORT_FLICKER
    flicker_param.master_alpha = shdisp_api_get_alpha();
    flicker_param.slave_alpha = shdisp_api_get_slave_alpha();
    shdisp_gemini_init_flicker_param(&flicker_param);
    flicker_param_low.master_alpha = shdisp_api_get_alpha_low();
    flicker_param_low.slave_alpha = shdisp_api_get_slave_alpha_low();
    shdisp_gemini_init_flicker_param_low(&flicker_param_low);
#endif
    phy_gamma = shdisp_api_get_lcddr_phy_gamma();
    shdisp_gemini_init_phy_gamma(phy_gamma);
    hw_revision = shdisp_api_get_hw_revision();

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_empty_func                                              */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_empty_func(void)
{
    SHDISP_DEBUG("\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_set_param                                               */
/* ------------------------------------------------------------------------- */
static void shdisp_gemini_API_set_param(struct shdisp_panel_param_str *param_str)
{
    SHDISP_DEBUG("in\n");
    gemini_param_str.vcom_alpha = param_str->vcom_alpha;
    gemini_param_str.vcom_alpha_low = param_str->vcom_alpha_low;
    SHDISP_DEBUG("out\n");

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_power_on                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_power_on(void)
{
    SHDISP_DEBUG("in\n");
    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);

#ifndef SHDISP_USE_LEDC
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);
#endif  /* SHDISP_USE_LEDC */
    shdisp_clmr_api_disp_init();

    /* eR63311_rev100_120928.pdf */
    /* IOVCC ON */
    /* VS4 enters automatically at system start-up */


#if 1
    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_SYS_delay_us(10*1000);
#endif

#ifdef SHDISP_USE_LEDC
    shdisp_bdic_API_LCD_vdd_on();
#else   /* SHDISP_USE_LEDC */
    shdisp_bdic_API_LCD_vo2_off();
#endif  /* SHDISP_USE_LEDC */
    shdisp_bdic_API_LCD_power_on();
    shdisp_bdic_API_LCD_m_power_on();
#ifndef SHDISP_USE_LEDC
    shdisp_bdic_API_LCD_vo2_on();
#endif  /* SHDISP_USE_LEDC */

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif  /* SHDISP_FW_STACK_EXCUTE */
    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_ON);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif  /* SHDISP_FW_STACK_EXCUTE */

    shdisp_SYS_cmd_delay_us(3000);

    shdisp_bdic_API_LCD_release_hw_reset();
    shdisp_SYS_delay_us(1*1000);

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_enter_sleep_mode);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif  /* SHDISP_FW_STACK_EXCUTE */

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_power_off                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_power_off(void)
{
    SHDISP_DEBUG("in\n");

#ifndef SHDISP_USE_LEDC
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
#endif  /* SHDISP_USE_LEDC */

    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_OFF);

    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_bdic_API_LCD_m_power_off();
    shdisp_bdic_API_LCD_power_off();
#ifdef SHDISP_USE_LEDC
    shdisp_bdic_API_LCD_vdd_off();
#endif  /* SHDISP_USE_LEDC */

#ifndef SHDISP_USE_LEDC
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif
#endif  /* SHDISP_USE_LEDC */

#ifndef SHDISP_USE_LEDC
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);
#endif  /* SHDISP_USE_LEDC */
    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_check_upper_unit                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_check_upper_unit(void)
{
#ifndef SHDISP_gemini_SW_CHK_UPPER_UNIT
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
#endif /* SHDISP_gemini_SW_CHK_UPPER_UNIT */
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_check_flicker_param                                     */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    unsigned short tmp_alpha = alpha_in;

    SHDISP_DEBUG("in alpha_in=%d\n", alpha_in);
    if (alpha_out == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha_out.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if ((tmp_alpha & 0xF000) != 0x9000) {
        *alpha_out = SHDISP_GEMINI_VCOMDC;
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_SUCCESS;
    }

    tmp_alpha = tmp_alpha & 0x01FF;
    if ((tmp_alpha < SHDISP_GEMINI_VCOM_MIN) || (tmp_alpha > SHDISP_GEMINI_VCOM_MAX)) {
        *alpha_out = SHDISP_GEMINI_VCOMDC;
        SHDISP_DEBUG("out2\n");
        return SHDISP_RESULT_SUCCESS;
    }

    *alpha_out = tmp_alpha;

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}

//#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
int shdisp_gemini_API_start_video(void)
{
    SHDISP_DEBUG("in\n");
    {
        char pageChg[4]    = { DTYPE_GEN_WRITE2, 0xB0, 00, 00 };
        char displayOn[4] = { DTYPE_DCS_WRITE, 0x29, 00, 00 };
        shdisp_FWCMD_buf_init(0);
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_C, sizeof(pageChg), pageChg );
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_C, sizeof(displayOn), displayOn );
        shdisp_FWCMD_buf_finish();
        shdisp_FWCMD_doKick(1, 0, 0);
    }
    {
        char clmrPstVideoOn[4] = { 00, 00, 00, 0x03 };
        shdisp_SYS_clmr_sio_transfer( SHDISP_CLMR_REG_PSTCTL, clmrPstVideoOn, sizeof(clmrPstVideoOn), 0, 0 );
    }

    shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_1);

#ifndef SHDISP_USE_LEDC
    shdisp_bdic_API_IRQ_det_irq_ctrl(1);
#endif  /* SHDISP_USE_LEDC */
    shdisp_clmr_api_fw_detlcdandbdic_ctrl(1);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

int shdisp_gemini_API_stop_video(void)
{
    SHDISP_DEBUG("in\n");

    shdisp_clmr_api_fw_detlcdandbdic_ctrl(0);
#ifndef SHDISP_USE_LEDC
    shdisp_bdic_API_IRQ_det_irq_ctrl(0);
#endif  /* SHDISP_USE_LEDC */

    shdisp_panel_API_request_RateCtrl(0,0,0);

    {
        char pageChg[4]    = { DTYPE_GEN_WRITE2, 0xB0, 00, 00 };
        char displayOff[4] = { DTYPE_DCS_WRITE, 0x28, 00, 00 };

        shdisp_FWCMD_buf_init(0);
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_C, sizeof(pageChg), pageChg );
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_C, sizeof(displayOff), displayOff );
        shdisp_FWCMD_buf_finish();
        shdisp_FWCMD_doKick(1, 0, 0);
        shdisp_SYS_cmd_delay_us(WAIT_1FRAME_US*1);
    }
    {
        char clmrPstVideoOff[4] = { 00, 00, 00, 00 };
        shdisp_SYS_clmr_sio_transfer( SHDISP_CLMR_REG_PSTCTL, clmrPstVideoOff, sizeof(clmrPstVideoOff), 0, 0 );
        shdisp_SYS_cmd_delay_us(WAIT_1FRAME_US*1);
    }
    {
        char pageRestore[4]    = { DTYPE_GEN_WRITE2, 0xB0, 00, 00 };
        pageRestore[2] = (char)current_page;
        shdisp_FWCMD_buf_init(0);
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE_C, sizeof(pageRestore), pageRestore );
        shdisp_FWCMD_buf_finish();
        shdisp_FWCMD_doKick(1, 0, 0);
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_write_reg                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size)
{
    int ret = 0;
#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
    int dsi_mode = 0;
#endif

    SHDISP_DEBUG("in\n");
#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
    if(addr == 0xB0)
    {
        current_page = *write_data;
    }
    SHDISP_DEBUG("current_page = %d\n", current_page);
    shdisp_gemini_API_stop_video();
#endif

/* DSI read test edit >>> */
#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
    switch (cog){
        case SHDISP_DIAG_COG_ID_NONE:
            dsi_mode = SHDISP_DSI_LOW_POWER_MODE;
            break;
        case SHDISP_DIAG_COG_ID_MASTER:
            dsi_mode = SHDISP_DSI_LOW_POWER_MODE_MS;
            break;
        case SHDISP_DIAG_COG_ID_SLAVE:
            dsi_mode = SHDISP_DSI_LOW_POWER_MODE_SL;
            break;
        case SHDISP_DIAG_COG_ID_BOTH:
            dsi_mode = SHDISP_DSI_LOW_POWER_MODE_BOTH;
            break;
        default:
            SHDISP_ERR("transfer_mode error. mode=%d\n", dsi_mode);
            break;
    }
    shdisp_panel_API_mipi_set_transfer_mode(dsi_mode);
#else
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE_BOTH);
#endif
/* DSI read test edit <<< */
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_gemini_tx_buf, addr, write_data, size);
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
    shdisp_gemini_API_start_video();
#endif
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_read_reg                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size)
{
    int ret = 0;
#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
    int dsi_mode = 0;
#endif

    SHDISP_DEBUG("in\n");
#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
    shdisp_gemini_API_stop_video();
#endif

/* DSI read test edit >>> */
#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
    switch (cog){
        case SHDISP_DIAG_COG_ID_NONE:
            dsi_mode = SHDISP_DSI_LOW_POWER_MODE;
            break;
        case SHDISP_DIAG_COG_ID_MASTER:
            dsi_mode = SHDISP_DSI_LOW_POWER_MODE_MS;
            break;
        case SHDISP_DIAG_COG_ID_SLAVE:
            dsi_mode = SHDISP_DSI_LOW_POWER_MODE_SL;
            break;
        default:
            SHDISP_ERR("transfer_mode error. mode=%d\n", dsi_mode);
            break;
    }
    shdisp_panel_API_mipi_set_transfer_mode(dsi_mode);
#else
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE_MS);
#endif
/* DSI read test edit <<< */

    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_gemini_tx_buf, &shdisp_mipi_gemini_rx_buf, addr, read_data, size);
    SHDISP_DEBUG("dummy read end\n");
    shdisp_SYS_delay_us(10*1000);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_gemini_tx_buf, &shdisp_mipi_gemini_rx_buf, addr, read_data, size);

#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
    shdisp_gemini_API_start_video();
#endif
    if(ret) {
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_set_flicker_param                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_set_flicker_param(unsigned short alpha)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_get_flicker_param                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_get_flicker_param(unsigned short *alpha)
{
    int ret = SHDISP_RESULT_SUCCESS;
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_get_flicker_low_param                               */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_get_flicker_low_param(unsigned short *alpha)
{
    int ret = SHDISP_RESULT_SUCCESS;
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_set_flicker_param_multi_cog                        */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_set_flicker_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned short m_pfvcomdc, s_pfvcomdc;
    unsigned char wdata;

    SHDISP_DEBUG("in\n");

    if (((flicker_param->master_alpha < SHDISP_GEMINI_VCOM_MIN) || (flicker_param->master_alpha > SHDISP_GEMINI_VCOM_MAX))
     || ((flicker_param->slave_alpha < SHDISP_GEMINI_VCOM_MIN) || (flicker_param->slave_alpha > SHDISP_GEMINI_VCOM_MAX))) {
        SHDISP_ERR("<INVALID_VALUE> master_alpha(0x%04X) slave_alpha(0x%04X).\n", flicker_param->master_alpha, flicker_param->slave_alpha);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_gemini_API_stop_video();
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[0][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[0][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 0 write_reg.\n");
        goto shdisp_end;
    }

    wdata = (flicker_param->master_alpha >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VCOMDC_1 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->slave_alpha  >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VCOMDC_1 Slave write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->master_alpha & 0xFF);
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VCOMDC_2 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->slave_alpha & 0xFF);
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VCOMDC_2 Slave write_reg.\n");
        goto shdisp_end;
    }

    wdata = (flicker_param->master_alpha >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC1_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC1_1 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->slave_alpha  >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC1_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC1_1 Slave write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->master_alpha & 0xFF);
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC1_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC1_2 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->slave_alpha & 0xFF);
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC1_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC1_2 Slave write_reg.\n");
        goto shdisp_end;
    }

    wdata = (flicker_param->master_alpha >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC2_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC2_1 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->slave_alpha  >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC2_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC2_1 Slave write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->master_alpha & 0xFF);
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC2_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC2_2 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->slave_alpha & 0xFF);
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC2_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC2_2 Slave write_reg.\n");
        goto shdisp_end;
    }

    wdata = (flicker_param->master_alpha >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC3_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC3_1 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->slave_alpha  >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC3_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC3_1 Slave write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->master_alpha & 0xFF);
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC3_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC3_2 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = (flicker_param->slave_alpha & 0xFF);
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC3_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> LPVCOMDC3_2 Slave write_reg.\n");
        goto shdisp_end;
    }

    m_pfvcomdc = 0x0113 + ((flicker_param->master_alpha + 1) / 2);
    wdata = (m_pfvcomdc >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_PFVCOMDC_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> PFVCOMDC_1 Master write_reg.\n");
        goto shdisp_end;
    }
    s_pfvcomdc = 0x0113 + ((flicker_param->slave_alpha + 1) / 2);
    wdata = (s_pfvcomdc >> 8) & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_PFVCOMDC_1][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> PFVCOMDC_1 Slave write_reg.\n");
        goto shdisp_end;
    }
    wdata = m_pfvcomdc & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_PFVCOMDC_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> PFVCOMDC_2 Master write_reg.\n");
        goto shdisp_end;
    }
    wdata = s_pfvcomdc & 0xFF;
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_PFVCOMDC_2][0],
                                                         &wdata,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> PFVCOMDC_2 Slave write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_FWCMD_buf_finish();
    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_FWCMD_doKick(1, 0, NULL);
    }
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> FWCMD Error.\n");
        goto shdisp_end;
    }

shdisp_end:

    shdisp_FWCMD_buf_set_nokick(0);
    shdisp_FWCMD_buf_init(0);

    shdisp_gemini_API_start_video();

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_get_flicker_param_multi_cog                        */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_get_flicker_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char rdata;
    unsigned char device_code;

    SHDISP_DEBUG("in\n");

    if (flicker_param == NULL){
        SHDISP_ERR("<NULL_POINTER> flicker_param.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_gemini_API_stop_video();
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);


    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[0][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[0][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 0 write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VCOMDC_1 Master read_reg.\n");
        goto shdisp_end;
    }
    flicker_param->master_alpha = rdata << 8;
#if 1
    device_code = shdisp_gemini_get_device_code();
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VCOMDC_1 Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    flicker_param->slave_alpha = rdata << 8;
    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_2][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VCOMDC_2 Master read_reg.\n");
        goto shdisp_end;
    }
    flicker_param->master_alpha |= rdata;
#if 1
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_2][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VCOMDC_2 Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    flicker_param->slave_alpha |= rdata;

    SHDISP_DEBUG("master_alpha =0x%04X slave_alpha =0x%04X.\n", flicker_param->master_alpha, flicker_param->slave_alpha);

shdisp_end:

    shdisp_gemini_API_start_video();

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_get_flicker_low_param_multi_cog                    */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_get_flicker_low_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char rdata;
    unsigned char device_code;

    SHDISP_DEBUG("in\n");

    if (flicker_param == NULL){
        SHDISP_ERR("<NULL_POINTER> flicker_param.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_gemini_API_stop_video();
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[0][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[0][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 0 write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VCOMDC_1 Master read_reg.\n");
        goto shdisp_end;
    }
    flicker_param->master_alpha = rdata << 8;
#if 1
    device_code = shdisp_gemini_get_device_code();
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VCOMDC_1 Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    flicker_param->slave_alpha = rdata << 8;
    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_2][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VCOMDC_2 Master read_reg.\n");
        goto shdisp_end;
    }
    flicker_param->master_alpha |= rdata;
#if 1
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_2][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VCOMDC_2 Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    flicker_param->slave_alpha |= rdata;

    SHDISP_DEBUG("master_alpha =0x%04X slave_alpha =0x%04X.\n", flicker_param->master_alpha, flicker_param->slave_alpha);

shdisp_end:

    shdisp_gemini_API_start_video();

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_check_recovery                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_check_recovery(void)
{
#ifndef SHDISP_NOT_SUPPORT_DET
    int ret;

    SHDISP_DEBUG("in\n");
#ifndef SHDISP_USE_LEDC
    ret = shdisp_bdic_API_RECOVERY_check_restoration();
#endif  /* SHDISP_USE_LEDC */

    if (ret != SHDISP_RESULT_SUCCESS) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_bdic_API_RECOVERY_check_restoration.\n");
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_recovery_type                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_recovery_type(int *type)
{
    SHDISP_DEBUG("in\n");
    *type = SHDISP_SUBSCRIBE_TYPE_INT;

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_set_abl_lut                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_disp_update                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_disp_update(struct shdisp_main_update *update)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_disp_clear_screen                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_disp_clear_screen(struct shdisp_main_clear *clear)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_set_gamma_info                                     */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    int ret = 0;
    int i;

    SHDISP_DEBUG("in\n");

    if (gamma_info == NULL){
        SHDISP_ERR("<NULL_POINTER> gamma_info.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_gemini_API_stop_video();
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[0][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[0][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 0 write_reg.\n");
        goto shdisp_end;
    }

    for (i = 0; i < SHDISP_PANEL_ANALOG_GAMMA_TBL_SIZE; i++) {
        ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                             mipi_sh_gemini_cmd_master_AnalogGammaSetting[i][0],
                                                             &gamma_info->master_info.analog_gamma[i],
                                                             1,
                                                             SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma Setting (Analog) Master %d write_reg.\n", i);
            goto shdisp_end;
        }
        ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                             mipi_sh_gemini_cmd_slave__AnalogGammaSetting[i][0],
                                                             &gamma_info->slave_info.analog_gamma[i],
                                                             1,
                                                             SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma Setting (Analog) Slave %d write_reg.\n", i);
            goto shdisp_end;
        }
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[5][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[5][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 5 write_reg.\n");
        goto shdisp_end;
    }

    for (i = 0; i < SHDISP_PANEL_GAMMA_TBL_SIZE; i++) {
        ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                             mipi_sh_gemini_cmd_master_DigitalGammaSettings[i][0],
                                                             &gamma_info->master_info.gammaR[i],
                                                             1,
                                                             SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma R Setting (Digital) Master %d write_reg.\n", i);
            goto shdisp_end;
        }
        ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                             mipi_sh_gemini_cmd_slave__DigitalGammaSettings[i][0],
                                                             &gamma_info->slave_info.gammaR[i],
                                                             1,
                                                             SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma R Setting (Digital) Slave %d write_reg.\n", i);
            goto shdisp_end;
        }
    }

    for (i = 0; i < SHDISP_PANEL_GAMMA_TBL_SIZE; i++) {
        ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                             mipi_sh_gemini_cmd_master_DigitalGammaSettings[SHDISP_PANEL_GAMMA_TBL_SIZE + i][0],
                                                             &gamma_info->master_info.gammaG[i],
                                                             1,
                                                             SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma G Setting (Digital) Master %d write_reg.\n", i);
            goto shdisp_end;
        }
        ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                             mipi_sh_gemini_cmd_slave__DigitalGammaSettings[SHDISP_PANEL_GAMMA_TBL_SIZE + i][0],
                                                             &gamma_info->slave_info.gammaG[i],
                                                             1,
                                                             SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma G Setting (Digital) Slave %d write_reg.\n", i);
            goto shdisp_end;
        }
    }

    for (i = 0; i < SHDISP_PANEL_GAMMA_TBL_SIZE; i++) {
        ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                             mipi_sh_gemini_cmd_master_DigitalGammaSettings[(SHDISP_PANEL_GAMMA_TBL_SIZE * 2) + i][0],
                                                             &gamma_info->master_info.gammaB[i],
                                                             1,
                                                             SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma B Setting (Digital) Master %d write_reg.\n", i);
            goto shdisp_end;
        }
        ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                             mipi_sh_gemini_cmd_slave__DigitalGammaSettings[(SHDISP_PANEL_GAMMA_TBL_SIZE * 2) + i][0],
                                                             &gamma_info->slave_info.gammaB[i],
                                                             1,
                                                             SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma B Setting (Digital) Slave %d write_reg.\n", i);
            goto shdisp_end;
        }
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[0][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[0][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 0 write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VSPS_VSNS][0],
                                                         &gamma_info->master_info.vsps_vsns,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VSPS_VSNS Master write_reg.\n");
        goto shdisp_end;
    }
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VSPS_VSNS][0],
                                                         &gamma_info->slave_info.vsps_vsns,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VSPS_VSNS Slave write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VGHS][0],
                                                         &gamma_info->master_info.vghs,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VGHS Master write_reg.\n");
        goto shdisp_end;
    }
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VGHS][0],
                                                         &gamma_info->slave_info.vghs,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VGHS Slave write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VGLS][0],
                                                         &gamma_info->master_info.vgls,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VGLS Master write_reg.\n");
        goto shdisp_end;
    }
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VGLS][0],
                                                         &gamma_info->slave_info.vgls,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VGLS Slave write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VPH_VPL][0],
                                                         &gamma_info->master_info.vph_vpl,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VPH_VPL Master write_reg.\n");
        goto shdisp_end;
    }
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VPH_VPL][0],
                                                         &gamma_info->slave_info.vph_vpl,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VPH_VPL Slave write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VNH_VNL][0],
                                                         &gamma_info->master_info.vnh_vnl,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VNH_VNL Master write_reg.\n");
        goto shdisp_end;
    }
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VNH_VNL][0],
                                                         &gamma_info->slave_info.vnh_vnl,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VNH_VNL Slave write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_FWCMD_buf_finish();
    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_FWCMD_doKick(1, 0, NULL);
    }
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> FWCMD Error.\n");
        goto shdisp_end;
    }

shdisp_end:

    shdisp_FWCMD_buf_set_nokick(0);
    shdisp_FWCMD_buf_init(0);

    shdisp_gemini_API_start_video();

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_get_gamma_info                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    int ret = 0;
    int i;
    unsigned char rdata = 0;
    unsigned char device_code;

    SHDISP_DEBUG("in\n");

    if (gamma_info == NULL){
        SHDISP_ERR("<NULL_POINTER> gamma_info.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_gemini_API_stop_video();
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[0][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[0][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 0 write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_AnalogGammaSetting[0][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);

    for (i = 0; i < SHDISP_PANEL_ANALOG_GAMMA_TBL_SIZE; i++) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_master_AnalogGammaSetting[i][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma Setting (Analog) Master %d read_reg.\n", i);
            goto shdisp_end;
        }
        gamma_info->master_info.analog_gamma[i] = rdata;
#if 1
        device_code = shdisp_api_get_device_code();
        if (device_code >= 0x01) {
                if (i == 0) {
                    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                                        &shdisp_mipi_gemini_rx_buf,
                                                                        mipi_sh_gemini_cmd_slave__AnalogGammaSetting[0][0],
                                                                        &rdata,
                                                                        1,
                                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
                }
                ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                                    &shdisp_mipi_gemini_rx_buf,
                                                                    mipi_sh_gemini_cmd_slave__AnalogGammaSetting[i][0],
                                                                    &rdata,
                                                                    1,
                                                                    SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
                if(ret) {
                    SHDISP_ERR("<RESULT_FAILURE> Gamma Setting (Analog) Slave %d read_reg.\n", i);
                    goto shdisp_end;
                }
        }
#endif
        gamma_info->slave_info.analog_gamma[i] = rdata;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[5][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[5][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 5 write_reg.\n");
        goto shdisp_end;
    }

    for (i = 0; i < SHDISP_PANEL_GAMMA_TBL_SIZE; i++) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_master_DigitalGammaSettings[i][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma R Setting (Digital) Master %d read_reg.\n", i);
            goto shdisp_end;
        }
        gamma_info->master_info.gammaR[i] = rdata;
#if 1
        if (device_code >= 0x01) {
            ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                                &shdisp_mipi_gemini_rx_buf,
                                                                mipi_sh_gemini_cmd_slave__DigitalGammaSettings[i][0],
                                                                &rdata,
                                                                1,
                                                                SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
            if(ret) {
                SHDISP_ERR("<RESULT_FAILURE> Gamma R Setting (Digital) Slave %d read_reg.\n", i);
                goto shdisp_end;
            }
        }
#endif
        gamma_info->slave_info.gammaR[i] = rdata;
    }

    for (i = 0; i < SHDISP_PANEL_GAMMA_TBL_SIZE; i++) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_master_DigitalGammaSettings[SHDISP_PANEL_GAMMA_TBL_SIZE + i][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma G Setting (Digital) Master %d read_reg.\n", i);
            goto shdisp_end;
        }
        gamma_info->master_info.gammaG[i] = rdata;
#if 1
        if (device_code >= 0x01) {
            ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                                &shdisp_mipi_gemini_rx_buf,
                                                                mipi_sh_gemini_cmd_slave__DigitalGammaSettings[SHDISP_PANEL_GAMMA_TBL_SIZE + i][0],
                                                                &rdata,
                                                                1,
                                                                SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
            if(ret) {
                SHDISP_ERR("<RESULT_FAILURE> Gamma G Setting (Digital) Slave %d read_reg.\n", i);
                goto shdisp_end;
            }
        }
#endif
        gamma_info->slave_info.gammaG[i] = rdata;
    }

    for (i = 0; i < SHDISP_PANEL_GAMMA_TBL_SIZE; i++) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_master_DigitalGammaSettings[(SHDISP_PANEL_GAMMA_TBL_SIZE * 2) + i][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> Gamma B Setting (Digital) Master %d read_reg.\n", i);
            goto shdisp_end;
        }
        gamma_info->master_info.gammaB[i] = rdata;
#if 1
        if (device_code >= 0x01) {
            ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                                &shdisp_mipi_gemini_rx_buf,
                                                                mipi_sh_gemini_cmd_slave__DigitalGammaSettings[(SHDISP_PANEL_GAMMA_TBL_SIZE * 2) + i][0],
                                                                &rdata,
                                                                1,
                                                                SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
            if(ret) {
                SHDISP_ERR("<RESULT_FAILURE> Gamma B Setting (Digital) Slave %d read_reg.\n", i);
                goto shdisp_end;
            }
        }
#endif
        gamma_info->slave_info.gammaB[i] = rdata;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[0][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[0][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 0 write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VSPS_VSNS][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VSPS_VSNS Master read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->master_info.vsps_vsns = rdata;
#if 1
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VSPS_VSNS][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VSPS_VSNS Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    gamma_info->slave_info.vsps_vsns = rdata;

    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VGHS][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VGHS Master read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->master_info.vghs = rdata;
#if 1
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VGHS][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VGHS Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    gamma_info->slave_info.vghs = rdata;

    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VGLS][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VGLS Master read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->master_info.vgls = rdata;
#if 1
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VGLS][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VGLS Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    gamma_info->slave_info.vgls = rdata;

    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VPH_VPL][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VPH_VPL Master read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->master_info.vph_vpl = rdata;
#if 1
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VPH_VPL][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VPH_VPL Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    gamma_info->slave_info.vph_vpl = rdata;

    ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                        &shdisp_mipi_gemini_rx_buf,
                                                        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VNH_VNL][0],
                                                        &rdata,
                                                        1,
                                                        SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> VNH_VNL Master read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->master_info.vnh_vnl = rdata;
#if 1
    if (device_code >= 0x01) {
        ret = shdisp_panel_API_mipi_diag_read_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                            &shdisp_mipi_gemini_rx_buf,
                                                            mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VNH_VNL][0],
                                                            &rdata,
                                                            1,
                                                            SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> VNH_VNL Slave read_reg.\n");
            goto shdisp_end;
        }
    }
#endif
    gamma_info->slave_info.vnh_vnl = rdata;

shdisp_end:

    shdisp_gemini_API_start_video();

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_diag_set_gamma                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_diag_set_gamma(struct shdisp_diag_gamma *gamma)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");

    if ((gamma->level < SHDISP_GEMINI_ANALOG_GAMMA_LEVEL_MIN) || (gamma->level > SHDISP_GEMINI_ANALOG_GAMMA_LEVEL_MAX)) {
        SHDISP_ERR("<INVALID_VALUE> gamma->level(%d).\n", gamma->level);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_gemini_API_stop_video();
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_Switch_to_Page[0][0],
                                                         &mipi_sh_gemini_cmd_Switch_to_Page[0][1],
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TXC);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Switch to Page 0 write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_AnalogGammaSetting[gamma->level - 1][0],
                                                         &gamma->master_gamma_p,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Gamma Setting (Analog) Master %d write_reg.\n", gamma->level - 1);
        goto shdisp_end;
    }
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__AnalogGammaSetting[gamma->level - 1][0],
                                                         &gamma->slave_gamma_p,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Gamma Setting (Analog) Slave %d write_reg.\n", gamma->level - 1);
        goto shdisp_end;
    }
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_master_AnalogGammaSetting[SHDISP_GEMINI_ANALOG_GAMMA_NEGATIVE_OFFSET + (gamma->level - 1)][0],
                                                         &gamma->master_gamma_n,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Gamma Setting (Analog) Master %d write_reg.\n", SHDISP_GEMINI_ANALOG_GAMMA_NEGATIVE_OFFSET + (gamma->level - 1));
        goto shdisp_end;
    }
    ret = shdisp_panel_API_mipi_diag_write_reg_multi_cog(&shdisp_mipi_gemini_tx_buf,
                                                         mipi_sh_gemini_cmd_slave__AnalogGammaSetting[SHDISP_GEMINI_ANALOG_GAMMA_NEGATIVE_OFFSET + (gamma->level - 1)][0],
                                                         &gamma->slave_gamma_n,
                                                         1,
                                                         SHDISP_CLMR_FWCMD_DSI_DSI_TX1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> Gamma Setting (Analog) Slave %d write_reg.\n", SHDISP_GEMINI_ANALOG_GAMMA_NEGATIVE_OFFSET + (gamma->level - 1));
        goto shdisp_end;
    }

    ret = shdisp_FWCMD_buf_finish();
    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_FWCMD_doKick(1, 0, NULL);
    }
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> FWCMD Error.\n");
        goto shdisp_end;
    }

shdisp_end:

    shdisp_FWCMD_buf_set_nokick(0);
    shdisp_FWCMD_buf_init(0);

    shdisp_gemini_API_start_video();

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_sqe_set_drive_freq                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_sqe_set_drive_freq(int type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("called type=%d.\n", type);

    switch(type) {
    case SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A:
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_DSI_TX_FREQ_SET, sizeof(freq_drive_a), (unsigned char*)freq_drive_a);
        drive_freq_lasy_type = type;
        break;
    case SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_B:
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_DSI_TX_FREQ_SET, sizeof(freq_drive_b), (unsigned char*)freq_drive_b);
        drive_freq_lasy_type = type;
        break;
    default:
        SHDISP_ERR("type error.\n");
    }

    SHDISP_DEBUG("done.\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_set_drive_freq                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_set_drive_freq(int type)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("called type=%d.\n", type);

    switch(type) {
    case SHDISP_MAIN_DISP_DRIVE_FREQ_DEFAULT:
        type = SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A;
        break;
    default:
        break;
    }

    if (drive_freq_lasy_type == type) {
        SHDISP_DEBUG("<INVALID_VALUE> Same Parameter.\n");
        return ret;
    }

    if (shdisp_pm_is_clmr_on() != SHDISP_DEV_STATE_ON) {
        drive_freq_lasy_type = type;
        SHDISP_DEBUG("CLMR IS NOT ACTIVE.\n");
        return ret;
    }

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    
    shdisp_gemini_sqe_set_drive_freq(type);
    
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_DEBUG("done.\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_get_drive_freq                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_get_drive_freq(void)
{
    return drive_freq_lasy_type;

}


static unsigned char shdisp_gemini_get_device_code(void)
{
    unsigned char device_code = 0;

    device_code = shdisp_api_get_device_code();
    if (device_code == 0xFF) {
        device_code = shdisp_gemini_mipi_manufacture_id();
        shdisp_api_set_device_code(device_code);
    }

    SHDISP_DEBUG("device_code=0x%x\n", device_code);
    return device_code;
}

unsigned char shdisp_gemini_mipi_manufacture_id(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    struct dsi_buf *rp, *tp;
    struct dsi_cmd_desc *cmd;
    char *rdata;

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_0);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out0 ret=%d\n", ret);
        return ret;
    }

    tp = &shdisp_mipi_gemini_tx_buf;
    rp = &shdisp_mipi_gemini_rx_buf;

    cmd = mipi_sh_gemini_cmds_deviceCode;
    
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE_MS);
    
    shdisp_panel_API_mipi_dsi_cmds_rx(tp, rp, cmd, 1);

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);

    rdata = rp->data;

    SHDISP_DEBUG("Header: %02X %02X %02X %02X %02X %02X %02X %02X\n", *(rdata+0), *(rdata+1), *(rdata+2), *(rdata+3), *(rdata+4), *(rdata+5), *(rdata+6), *(rdata+7));

    return *(rdata+5);
}

static int shdisp_gemini_mipi_cmd_start_display(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char device_code = 0;

    SHDISP_DEBUG("in\n");

    device_code = shdisp_gemini_get_device_code();
    if (hw_revision == SHDISP_HW_REV_PP1 && device_code == 0x00) {
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_SCS2, SHDISP_GPIO_CTL_HIGH);
    }
    else if (hw_revision >= SHDISP_HW_REV_PP1) {
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_SCS2, SHDISP_GPIO_CTL_LOW);
    }

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_display_on1);

    shdisp_clmr_api_data_transfer_starts();

    shdisp_SYS_cmd_delay_us(5*WAIT_1FRAME_US);

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_VCOMSettings);

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_display_on2);


#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_1);

    if (drive_freq_lasy_type != SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A) {
#ifdef SHDISP_FW_STACK_EXCUTE
        shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
        shdisp_gemini_sqe_set_drive_freq(drive_freq_lasy_type);
#ifdef SHDISP_FW_STACK_EXCUTE
        shdisp_FWCMD_safe_finishanddoKick();
        shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif
    }

    SHDISP_DEBUG("out\n");
    return ret;
}

static int shdisp_gemini_mipi_cmd_lcd_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char device_code = 0;

    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_display_off);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif  /* SHDISP_FW_STACK_EXCUTE */

    device_code = shdisp_gemini_get_device_code();
    if (hw_revision == SHDISP_HW_REV_PP1 && device_code == 0x00) {
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_SCS2, SHDISP_GPIO_CTL_LOW);
    }
    else if (hw_revision >= SHDISP_HW_REV_PP1) {
        shdisp_SYS_set_Host_gpio(SHDISP_GPIO_NUM_LCD_SCS2, SHDISP_GPIO_CTL_LOW);
    }

    shdisp_clmr_api_display_stop();

    SHDISP_DEBUG("out\n");
    return ret;
}

static int shdisp_gemini_mipi_cmd_lcd_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char device_code = 0;

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);

    device_code = shdisp_gemini_get_device_code();

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

#ifdef SHDISP_GEMINI_ES1_CM_SAMPLE
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_1_B);
#else   /* SHDISP_GEMINI_ES1_CM_SAMPLE */
    if (hw_revision == SHDISP_HW_REV_PP1 && device_code == 0x00) {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_1_B);
    }
    else {
        if (device_code != 0x00) {
            mipi_sh_gemini_cmd_DisplaySettings[1][1] = 0x09;
        }
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_1);
    }
#endif  /* SHDISP_GEMINI_ES1_CM_SAMPLE */

    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out1 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_2);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out2 ret=%d\n", ret);
        return ret;
    }
    if (device_code != 0x00) {
        mipi_sh_gemini_cmd_PowerON_OFF[8][1] = 0x01;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_3);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out3 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_4);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out4 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_5);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out5 ret=%d\n", ret);
        return ret;
    }

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_7);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out7 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_8);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out8 ret=%d\n", ret);
        return ret;
    }

#ifdef SHDISP_GEMINI_ES1_CM_SAMPLE
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_9);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out9 ret=%d\n", ret);
        return ret;
    }
#else   /* SHDISP_GEMINI_ES1_CM_SAMPLE */
    if (hw_revision == SHDISP_HW_REV_PP1 && device_code == 0x00) {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_9);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out9 ret=%d\n", ret);
            return ret;
        }
    }
    else {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_gemini_cmds_proc_9_cut2);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out9_cut2 ret=%d\n", ret);
            return ret;
        }
    }
#endif  /* SHDISP_GEMINI_ES1_CM_SAMPLE */
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    shdisp_clmr_api_custom_blk_init();

    SHDISP_DEBUG("out\n");
    return ret;
}

int shdisp_gemini_API_mipi_lcd_on(void)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
    ret = shdisp_gemini_mipi_cmd_lcd_on();
    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}

int shdisp_gemini_API_mipi_lcd_off(void)
{
    SHDISP_DEBUG("in\n");
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    shdisp_gemini_mipi_cmd_lcd_off();

#ifdef SHDISP_FW_STACK_EXCUTE
#endif
    SHDISP_DEBUG("out\n");
    return 0;
}

int shdisp_gemini_API_mipi_start_display(void)
{
    SHDISP_DEBUG("in\n");
    shdisp_gemini_mipi_cmd_start_display();
    SHDISP_DEBUG("out\n");
    return 0;
}

int shdisp_gemini_API_cabc_init(void)
{
    return 0;
}

int shdisp_gemini_API_cabc_indoor_on(void)
{
    return 0;
}

int shdisp_gemini_API_cabc_outdoor_on(int lut_level)
{
    return 0;
}

int shdisp_gemini_API_cabc_off(int wait_on, int pwm_disable)
{
    return 0;
}

int shdisp_gemini_API_cabc_outdoor_move(int lut_level)
{
    return 0;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_power_on_recovery                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_power_on_recovery(void)
{
#ifdef SHDISP_gemini_PROVISIONAL_REG_RW
   current_page = 0;
#endif
    SHDISP_DEBUG("in\n");

    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);
#ifndef SHDISP_USE_LEDC
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);
#endif  /* SHDISP_USE_LEDC */

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

#ifdef SHDISP_USE_LEDC
    shdisp_bdic_API_LCD_vdd_on();
#else   /* SHDISP_USE_LEDC */
    shdisp_bdic_API_LCD_vo2_off();
#endif  /* SHDISP_USE_LEDC */
    shdisp_bdic_API_LCD_power_on();
    shdisp_bdic_API_LCD_m_power_on();
#ifndef SHDISP_USE_LEDC
    shdisp_bdic_API_LCD_vo2_on();
#endif  /* SHDISP_USE_LEDC */

    shdisp_SYS_cmd_delay_us(10*1000);

    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_ON);
    shdisp_SYS_cmd_delay_us(3000);

    shdisp_bdic_API_LCD_release_hw_reset();
    shdisp_SYS_cmd_delay_us(20*1000);

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_API_power_off_recovery                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_gemini_API_power_off_recovery(void)
{
    SHDISP_DEBUG("in\n");

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_OFF);

    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_bdic_API_LCD_m_power_off();
    shdisp_SYS_cmd_delay_us(10000);
    shdisp_bdic_API_LCD_power_off();
    shdisp_SYS_cmd_delay_us(10000);
#ifdef SHDISP_USE_LEDC
    shdisp_bdic_API_LCD_vdd_off();
#endif  /* SHDISP_USE_LEDC */

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

#ifndef SHDISP_USE_LEDC
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);
#endif  /* SHDISP_USE_LEDC */
    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_init_flicker_param                                          */
/* ------------------------------------------------------------------------- */
void shdisp_gemini_init_flicker_param(struct shdisp_diag_flicker_param *flicker_param)
{
    SHDISP_DEBUG("in flicker_param master_alpha = 0x%04x slave_alpha = 0x%04x\n", flicker_param->master_alpha, flicker_param->slave_alpha);
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][1]   = (flicker_param->master_alpha >> 8) & 0xFF;
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_2][1]   =  flicker_param->master_alpha       & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_1][1]   = (flicker_param->slave_alpha  >> 8) & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_VCOMDC_2][1]   =  flicker_param->slave_alpha        & 0xFF;
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_PFVCOMDC_1][1] = ((0x0113 + ((flicker_param->master_alpha + 1) / 2)) >> 8) & 0xFF;
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_PFVCOMDC_2][1] = ( 0x0113 + ((flicker_param->master_alpha + 1) / 2)      ) & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_PFVCOMDC_1][1] = ((0x0113 + ((flicker_param->slave_alpha  + 1) / 2)) >> 8) & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_PFVCOMDC_2][1] = ( 0x0113 + ((flicker_param->slave_alpha  + 1) / 2)      ) & 0xFF;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_init_flicker_param_low                                      */
/* ------------------------------------------------------------------------- */
void shdisp_gemini_init_flicker_param_low(struct shdisp_diag_flicker_param *flicker_param)
{
    SHDISP_DEBUG("in flicker_param master_alpha = 0x%04x slave_alpha = 0x%04x\n", flicker_param->master_alpha, flicker_param->slave_alpha);
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC1_1][1] = (flicker_param->master_alpha >> 8) & 0xFF;
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC1_2][1] =  flicker_param->master_alpha       & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC1_1][1] = (flicker_param->slave_alpha  >> 8) & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC1_2][1] =  flicker_param->slave_alpha        & 0xFF;
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC2_1][1] = (flicker_param->master_alpha >> 8) & 0xFF;
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC2_2][1] =  flicker_param->master_alpha       & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC2_1][1] = (flicker_param->slave_alpha  >> 8) & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC2_2][1] =  flicker_param->slave_alpha        & 0xFF;
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC3_1][1] = (flicker_param->master_alpha >> 8) & 0xFF;
    mipi_sh_gemini_cmd_master_VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC3_2][1] =  flicker_param->master_alpha       & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC3_1][1] = (flicker_param->slave_alpha  >> 8) & 0xFF;
    mipi_sh_gemini_cmd_slave__VCOMSettings[SHDISP_GEMINI_NO_LPVCOMDC3_2][1] =  flicker_param->slave_alpha        & 0xFF;
}


/* ------------------------------------------------------------------------- */
/* shdisp_gemini_init_phy_gamma                                              */
/* ------------------------------------------------------------------------- */
void shdisp_gemini_init_phy_gamma(struct shdisp_lcddr_phy_gamma_reg *phy_gamma)
{
    int idx;
    unsigned int checksum;
    int set_default = 0;

    SHDISP_DEBUG("in.\n");


    if (phy_gamma == NULL) {
        SHDISP_ERR("phy_gamma is NULL.\n");
        return;
    }

    if (phy_gamma->status != SHDISP_LCDDR_GAMMA_STATUS_OK) {
        SHDISP_ERR("gamma status invalid. status=%02x\n", phy_gamma->status);
    } else {
        checksum = phy_gamma->status;
        for (idx = 0; idx < SHDISP_LCDDR_PHY_ANALOG_GAMMA_BUF_MAX; idx++) {
            checksum += phy_gamma->master.analog_gamma[idx];
            checksum += phy_gamma->slave.analog_gamma[idx];
        }
        for (idx = 0; idx < SHDISP_LCDDR_PHY_GAMMA_BUF_MAX; idx++) {
            checksum += phy_gamma->master.gamma[idx];
            checksum += phy_gamma->slave.gamma[idx];
        }
        for (idx = 0; idx < SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE; idx++) {
            checksum += phy_gamma->master.applied_voltage[idx];
            checksum += phy_gamma->slave.applied_voltage[idx];
        }

        if ((checksum & 0x00FFFFFF) != phy_gamma->chksum) {
            SHDISP_ERR("gammg chksum NG. phy_chksum = %06x calc_chksum = %06x\n", phy_gamma->chksum, (checksum & 0x00FFFFFF));
        }
    }

    if (set_default) {
        for (idx = 0; idx < SHDISP_LCDDR_PHY_ANALOG_GAMMA_BUF_MAX; idx++) {
            phy_gamma->master.analog_gamma[idx] = mipi_sh_gemini_cmd_master_AnalogGammaSetting[idx][1];
            phy_gamma->slave.analog_gamma[idx]  = mipi_sh_gemini_cmd_slave__AnalogGammaSetting[idx][1];
        }
        for (idx = 0; idx < SHDISP_LCDDR_PHY_GAMMA_BUF_MAX; idx++) {
            phy_gamma->master.gamma[idx] = mipi_sh_gemini_cmd_master_DigitalGammaSettings[idx][1];
            phy_gamma->slave.gamma[idx]  = mipi_sh_gemini_cmd_slave__DigitalGammaSettings[idx][1];
        }
        phy_gamma->master.applied_voltage[0] = mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VSPS_VSNS][1];
        phy_gamma->master.applied_voltage[1] = mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VGHS     ][1];
        phy_gamma->master.applied_voltage[2] = mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VGLS     ][1];
        phy_gamma->master.applied_voltage[3] = mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VPH_VPL  ][1];
        phy_gamma->master.applied_voltage[4] = mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VNH_VNL  ][1];
        phy_gamma->slave.applied_voltage[0]  = mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VSPS_VSNS][1];
        phy_gamma->slave.applied_voltage[1]  = mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VGHS     ][1];
        phy_gamma->slave.applied_voltage[2]  = mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VGLS     ][1];
        phy_gamma->slave.applied_voltage[3]  = mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VPH_VPL  ][1];
        phy_gamma->slave.applied_voltage[4]  = mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VNH_VNL  ][1];

        phy_gamma->status = SHDISP_LCDDR_GAMMA_STATUS_OK;
        phy_gamma->chksum = phy_gamma->status;
        for (idx = 0; idx < SHDISP_LCDDR_PHY_ANALOG_GAMMA_BUF_MAX; idx++) {
            phy_gamma->chksum += phy_gamma->master.analog_gamma[idx];
            phy_gamma->chksum += phy_gamma->slave.analog_gamma[idx];
        }
        for (idx = 0; idx < SHDISP_LCDDR_PHY_GAMMA_BUF_MAX; idx++) {
            phy_gamma->chksum += phy_gamma->master.gamma[idx];
            phy_gamma->chksum += phy_gamma->slave.gamma[idx];
        }
        for (idx = 0; idx < SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE; idx++) {
            phy_gamma->chksum += phy_gamma->master.applied_voltage[idx];
            phy_gamma->chksum += phy_gamma->slave.applied_voltage[idx];
        }
    } else {
        for (idx = 0; idx < SHDISP_LCDDR_PHY_ANALOG_GAMMA_BUF_MAX; idx++) {
            mipi_sh_gemini_cmd_master_AnalogGammaSetting[idx][1] = phy_gamma->master.analog_gamma[idx];
            mipi_sh_gemini_cmd_slave__AnalogGammaSetting[idx][1] = phy_gamma->slave.analog_gamma[idx];
        }
        for (idx = 0; idx < SHDISP_LCDDR_PHY_GAMMA_BUF_MAX; idx++) {
            mipi_sh_gemini_cmd_master_DigitalGammaSettings[idx][1] = phy_gamma->master.gamma[idx];
            mipi_sh_gemini_cmd_slave__DigitalGammaSettings[idx][1] = phy_gamma->slave.gamma[idx];
        }
        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VSPS_VSNS][1] = phy_gamma->master.applied_voltage[0];
        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VGHS     ][1] = phy_gamma->master.applied_voltage[1];
        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VGLS     ][1] = phy_gamma->master.applied_voltage[2];
        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VPH_VPL  ][1] = phy_gamma->master.applied_voltage[3];
        mipi_sh_gemini_cmd_master_VoltageSettings[SHDISP_GEMINI_NO_VNH_VNL  ][1] = phy_gamma->master.applied_voltage[4];
        mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VSPS_VSNS][1] = phy_gamma->slave.applied_voltage[0];
        mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VGHS     ][1] = phy_gamma->slave.applied_voltage[1];
        mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VGLS     ][1] = phy_gamma->slave.applied_voltage[2];
        mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VPH_VPL  ][1] = phy_gamma->slave.applied_voltage[3];
        mipi_sh_gemini_cmd_slave__VoltageSettings[SHDISP_GEMINI_NO_VNH_VNL  ][1] = phy_gamma->slave.applied_voltage[4];
    }

    SHDISP_DEBUG("out.\n");
    return;
}


MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
