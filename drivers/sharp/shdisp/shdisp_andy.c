/* drivers/sharp/shdisp/shdisp_andy.c  (Display Driver)
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
#include <linux/regulator/consumer.h>
#include <sharp/shdisp_kerl.h>
#include "shdisp_panel.h"
#include "shdisp_andy.h"
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
//#define SHDISP_ANDY_PROVISIONAL_REG_RW
#define SHDISP_FW_STACK_EXCUTE

#define SHDISP_ANDY_RESET      32
#define SHDISP_ANDY_DISABLE    0
#define SHDISP_ANDY_ENABLE     1

#define SHDISP_ANDY_1V_WAIT                 18000
#define SHDISP_ANDY_VCOM_MIN                0x0000
#define SHDISP_ANDY_VCOM_MAX                0x0190
//#define SHDISP_ANDY_CLOCK                   27000000
//#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
   static unsigned char current_page = 0;
//#endif
#define ANDY_VCOM_REG_NUM                   (6)

#define SHDISP_ANDY_GAMMA_SETTING_SIZE      60
#define SHDISP_ANDY_GAMMA_LEVEL_MIN         1
#define SHDISP_ANDY_GAMMA_LEVEL_MAX         30
#define SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET   30
#define SHDISP_ANDY_VGH         2
#define SHDISP_ANDY_VGL         3
#define SHDISP_ANDY_GVDDP       6
#define SHDISP_ANDY_GVDDN       7
#define SHDISP_ANDY_GVDDP2      8
#define SHDISP_ANDY_VGHO        10
#define SHDISP_ANDY_VGLO        11
#define SHDISP_ANDY_AVDDR       15
#define SHDISP_ANDY_AVEER       16

#define MIPI_SHARP_CLMR_1HZ_BLACK_ON    1
#define MIPI_SHARP_CLMR_AUTO_PAT_OFF    0

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

static int shdisp_andy_API_init_io(void);
static int shdisp_andy_API_exit_io(void);
static int shdisp_andy_API_empty_func(void);
static void shdisp_andy_API_set_param(struct shdisp_panel_param_str *param_str);
static int shdisp_andy_API_power_on(void);
static int shdisp_andy_API_power_off(void);
static int shdisp_andy_API_check_upper_unit(void);
static int shdisp_andy_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out);
static int shdisp_andy_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size);
static int shdisp_andy_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size);
static int shdisp_andy_API_diag_set_flicker_param(unsigned short alpha);
static int shdisp_andy_API_diag_get_flicker_param(unsigned short *alpha);
static int shdisp_andy_API_check_recovery(void);
static int shdisp_andy_API_recovery_type(int *type);
static int shdisp_andy_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma);
static int shdisp_andy_API_disp_update(struct shdisp_main_update *update);
static int shdisp_andy_API_disp_clear_screen(struct shdisp_main_clear *clear);
static int shdisp_andy_API_diag_get_flicker_low_param(unsigned short *alpha);
static int shdisp_andy_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
static int shdisp_andy_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
static int shdisp_andy_API_diag_set_gamma(struct shdisp_diag_gamma *gamma);
static int shdisp_andy_API_power_on_recovery(void);
static int shdisp_andy_API_power_off_recovery(void);
static int shdisp_andy_sqe_set_drive_freq(int type);
static int shdisp_andy_API_set_drive_freq(int type);
static int shdisp_andy_mipi_cmd_lcd_on_after_black_screen(void);

unsigned char shdisp_andy_mipi_manufacture_id(void);

int shdisp_andy_init_phy_gamma(struct shdisp_lcddr_phy_gamma_reg *phy_gamma);

static int shdisp_andy_regulator_init(void);
static int shdisp_andy_regulator_exit(void);
static int shdisp_andy_regulator_on(void);
static int shdisp_andy_regulator_off(void);


/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static struct shdisp_panel_param_str andy_param_str;
#ifndef SHDISP_NOT_SUPPORT_FLICKER
static unsigned char andy_wdata[8];
static unsigned char andy_rdata[8];
static char Andy_VCOM_Reg[6] = { 0x13, 0x14, 0x15, 0x5C, 0x5D, 0x5E };
#endif
static struct dsi_buf shdisp_mipi_andy_tx_buf;
static struct dsi_buf shdisp_mipi_andy_rx_buf;

static char mipi_sh_andy_set_val_GAMMAREDposi[60][2];
static char mipi_sh_andy_set_val_GAMMAREDnega[60][2];
static char mipi_sh_andy_set_val_GAMMAGREENposi[60][2];
static char mipi_sh_andy_set_val_GAMMAGREENnega[60][2];
static char mipi_sh_andy_set_val_GAMMABLUEposi[60][2];
static char mipi_sh_andy_set_val_GAMMABLUEnega[60][2];
static char mipi_sh_andy_set_val_RegulatorPumpSetting[25][2];

static struct shdisp_diag_gamma_info diag_tmp_gamma_info;
static int diag_tmp_gamma_info_set = 0;
static struct regulator *regu_lcd_vddi;

static int drive_freq_lasy_type = SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A;

/* ------------------------------------------------------------------------- */
/*      packet header                                                        */
/* ------------------------------------------------------------------------- */
/*      LCD ON                                                              */
/*      Initial Setting                                                     */

static char mipi_sh_andy_cmd_LRFlip[2] = {0x36, 0x40};
static char mipi_sh_andy_cmd_MIPIVideoBurstwrite[2] = {0xB0, 0x18};
static char mipi_sh_andy_cmd_DeviceCodeRead[2] = {0x3A, 0x00};

static char mipi_sh_andy_cmd_SwitchCommand[8][2] = {
    {0xFF, 0x00},
    {0xFF, 0x01},
    {0xFF, 0x02},
    {0xFF, 0x03},
    {0xFF, 0x04},
    {0xFF, 0x05},
    {0xFF, 0xFF},
    {0xFF, 0xEE}
};

static char mipi_sh_andy_cmd_PorchLineSetting[5][2] = {
    {0xD3, 0x0B},
    {0xD4, 0x0A},
    {0xD5, 0x0A},
    {0xD6, 0x0A},
    {0xD9, 0x61}
};

static char mipi_sh_andy_cmd_VCOMOFFSetting[2] = {0xE9, 0x04};
static char mipi_sh_andy_cmd_MTPLoadStop[2] = {0xFB, 0x01};
static char mipi_sh_andy_cmd_SetNormalMode[2] = {0xB0, 0x00};

#define VGH          0x55
#define VGL          0xD5
#define GVDDP        0x9D
#define GVDDN        0x9D
#define GVDDP2_TS1_0 0xF0
#define GVDDP2_TS1_1 0x70
#define VGHO         0x50
#define VGLO         0x6E
#define VCOM1_L      0x42
#define VCOM2_L      0x42
#define VCOM12_H     0x60
#define AVDDR        0x15
#define AVEER        0x15
#define LPVCOM1      0x00
#define LPVCOM2      0x00
#define VCOMOFF      0x21

#define NO_VCOM1_L  12
#define NO_VCOM2_L  13
#define NO_VCOM12_H 14
#define NO_LPVCOM1  18
#define NO_LPVCOM2  19
#define NO_VCOMOFF  20

#define RTN         0x7D
#define GIP         0x67

#define RTN_B       0x82
#define GIP_B       0x6C

static char mipi_sh_andy_cmd_RegulatorPumpSetting[25][2] = {
    {0x04, 0x0C},
    {0x05, 0x3A},
    {0x06, VGH},
    {0x07, VGL},
    {0x0A, 0x0C},
    {0x0C, 0xA6},
    {0x0D, GVDDP},
    {0x0E, GVDDN},
    {0x0F, GVDDP2_TS1_0},
    {0x10, 0x63},
    {0x11, VGHO},
    {0x12, VGLO},
    {0x13, VCOM1_L},
    {0x14, VCOM2_L},
    {0x15, VCOM12_H},
    {0x16, AVDDR},
    {0x17, AVEER},
    {0x5B, 0xCA},
    {0x5C, LPVCOM1},
    {0x5D, LPVCOM2},
    {0x5E, VCOMOFF},
    {0x60, 0xD5},
    {0x61, 0xF7},
    {0x6C, 0xAB},
    {0x6D, 0x44}
};

static char mipi_sh_andy_cmd_BiasSetting_AfterBlackScreen[3][2] = {
    {0x5B, 0xCA},
    {0x6C, 0xAB},
    {0x6D, 0x44}
};
static char mipi_sh_andy_cmd_BiasSetting_BlackScreenOn[3][2] = {
    {0x5B, 0xC0},
    {0x6C, 0x08},
    {0x6D, 0x00}
};


static char mipi_sh_andy_cmd_GAMMAREDposi[60][2] = {
    {0x75, 0x00},
    {0x76, 0x44},
    {0x77, 0x00},
    {0x78, 0x53},
    {0x79, 0x00},
    {0x7A, 0x63},
    {0x7B, 0x00},
    {0x7C, 0x74},
    {0x7D, 0x00},
    {0x7E, 0x83},
    {0x7F, 0x00},
    {0x80, 0x8F},
    {0x81, 0x00},
    {0x82, 0x9A},
    {0x83, 0x00},
    {0x84, 0xA4},
    {0x85, 0x00},
    {0x86, 0xAE},
    {0x87, 0x00},
    {0x88, 0xD1},
    {0x89, 0x00},
    {0x8A, 0xED},
    {0x8B, 0x01},
    {0x8C, 0x1C},
    {0x8D, 0x01},
    {0x8E, 0x41},
    {0x8F, 0x01},
    {0x90, 0x7B},
    {0x91, 0x01},
    {0x92, 0xAB},
    {0x93, 0x01},
    {0x94, 0xAC},
    {0x95, 0x01},
    {0x96, 0xD9},
    {0x97, 0x02},
    {0x98, 0x05},
    {0x99, 0x02},
    {0x9A, 0x1F},
    {0x9B, 0x02},
    {0x9C, 0x42},
    {0x9D, 0x02},
    {0x9E, 0x5B},
    {0x9F, 0x02},
    {0xA0, 0x88},
    {0xA2, 0x02},
    {0xA3, 0x97},
    {0xA4, 0x02},
    {0xA5, 0xA9},
    {0xA6, 0x02},
    {0xA7, 0xC0},
    {0xA9, 0x02},
    {0xAA, 0xE4},
    {0xAB, 0x03},
    {0xAC, 0x10},
    {0xAD, 0x03},
    {0xAE, 0x66},
    {0xAF, 0x03},
    {0xB0, 0x97},
    {0xB1, 0x03},
    {0xB2, 0x99}
};

static char mipi_sh_andy_cmd_GAMMAREDnega[60][2] = {
    {0xB3, 0x00},
    {0xB4, 0x44},
    {0xB5, 0x00},
    {0xB6, 0x53},
    {0xB7, 0x00},
    {0xB8, 0x63},
    {0xB9, 0x00},
    {0xBA, 0x74},
    {0xBB, 0x00},
    {0xBC, 0x83},
    {0xBD, 0x00},
    {0xBE, 0x8F},
    {0xBF, 0x00},
    {0xC0, 0x9A},
    {0xC1, 0x00},
    {0xC2, 0xA4},
    {0xC3, 0x00},
    {0xC4, 0xAE},
    {0xC5, 0x00},
    {0xC6, 0xD1},
    {0xC7, 0x00},
    {0xC8, 0xED},
    {0xC9, 0x01},
    {0xCA, 0x1C},
    {0xCB, 0x01},
    {0xCC, 0x41},
    {0xCD, 0x01},
    {0xCE, 0x7B},
    {0xCF, 0x01},
    {0xD0, 0xAB},
    {0xD1, 0x01},
    {0xD2, 0xAC},
    {0xD3, 0x01},
    {0xD4, 0xD4},
    {0xD5, 0x02},
    {0xD6, 0x00},
    {0xD7, 0x02},
    {0xD8, 0x1A},
    {0xD9, 0x02},
    {0xDA, 0x39},
    {0xDB, 0x02},
    {0xDC, 0x52},
    {0xDD, 0x02},
    {0xDE, 0x75},
    {0xDF, 0x02},
    {0xE0, 0x85},
    {0xE1, 0x02},
    {0xE2, 0x96},
    {0xE3, 0x02},
    {0xE4, 0xAB},
    {0xE5, 0x02},
    {0xE6, 0xCD},
    {0xE7, 0x02},
    {0xE8, 0xF9},
    {0xE9, 0x03},
    {0xEA, 0x4F},
    {0xEB, 0x03},
    {0xEC, 0x80},
    {0xED, 0x03},
    {0xEE, 0x82}
};



static char mipi_sh_andy_cmd_GAMMAGREENposi[60][2] = {
    {0xEF, 0x00},
    {0xF0, 0x44},
    {0xF1, 0x00},
    {0xF2, 0x53},
    {0xF3, 0x00},
    {0xF4, 0x63},
    {0xF5, 0x00},
    {0xF6, 0x74},
    {0xF7, 0x00},
    {0xF8, 0x83},
    {0xF9, 0x00},
    {0xFA, 0x8F},
    {0x00, 0x00},
    {0x01, 0x9A},
    {0x02, 0x00},
    {0x03, 0xA4},
    {0x04, 0x00},
    {0x05, 0xAE},
    {0x06, 0x00},
    {0x07, 0xD1},
    {0x08, 0x00},
    {0x09, 0xED},
    {0x0A, 0x01},
    {0x0B, 0x1C},
    {0x0C, 0x01},
    {0x0D, 0x41},
    {0x0E, 0x01},
    {0x0F, 0x7B},
    {0x10, 0x01},
    {0x11, 0xAB},
    {0x12, 0x01},
    {0x13, 0xAC},
    {0x14, 0x01},
    {0x15, 0xD9},
    {0x16, 0x02},
    {0x17, 0x05},
    {0x18, 0x02},
    {0x19, 0x1F},
    {0x1A, 0x02},
    {0x1B, 0x42},
    {0x1C, 0x02},
    {0x1D, 0x5B},
    {0x1E, 0x02},
    {0x1F, 0x88},
    {0x20, 0x02},
    {0x21, 0x97},
    {0x22, 0x02},
    {0x23, 0xA9},
    {0x24, 0x02},
    {0x25, 0xC0},
    {0x26, 0x02},
    {0x27, 0xE4},
    {0x28, 0x03},
    {0x29, 0x10},
    {0x2A, 0x03},
    {0x2B, 0x66},
    {0x2D, 0x03},
    {0x2F, 0x97},
    {0x30, 0x03},
    {0x31, 0x99}
};

static char mipi_sh_andy_cmd_GAMMAGREENnega[60][2] = {
    {0x32, 0x00},
    {0x33, 0x44},
    {0x34, 0x00},
    {0x35, 0x53},
    {0x36, 0x00},
    {0x37, 0x63},
    {0x38, 0x00},
    {0x39, 0x74},
    {0x3A, 0x00},
    {0x3B, 0x83},
    {0x3D, 0x00},
    {0x3F, 0x8F},
    {0x40, 0x00},
    {0x41, 0x9A},
    {0x42, 0x00},
    {0x43, 0xA4},
    {0x44, 0x00},
    {0x45, 0xAE},
    {0x46, 0x00},
    {0x47, 0xD1},
    {0x48, 0x00},
    {0x49, 0xED},
    {0x4A, 0x01},
    {0x4B, 0x1C},
    {0x4C, 0x01},
    {0x4D, 0x41},
    {0x4E, 0x01},
    {0x4F, 0x7B},
    {0x50, 0x01},
    {0x51, 0xAB},
    {0x52, 0x01},
    {0x53, 0xAC},
    {0x54, 0x01},
    {0x55, 0xD4},
    {0x56, 0x02},
    {0x58, 0x00},
    {0x59, 0x02},
    {0x5A, 0x1A},
    {0x5B, 0x02},
    {0x5C, 0x39},
    {0x5D, 0x02},
    {0x5E, 0x52},
    {0x5F, 0x02},
    {0x60, 0x75},
    {0x61, 0x02},
    {0x62, 0x85},
    {0x63, 0x02},
    {0x64, 0x96},
    {0x65, 0x02},
    {0x66, 0xAB},
    {0x67, 0x02},
    {0x68, 0xCD},
    {0x69, 0x02},
    {0x6A, 0xF9},
    {0x6B, 0x03},
    {0x6C, 0x4F},
    {0x6D, 0x03},
    {0x6E, 0x80},
    {0x6F, 0x03},
    {0x70, 0x82}
};

static char mipi_sh_andy_cmd_GAMMABLUEposi[60][2] = {
    {0x71, 0x00},
    {0x72, 0x44},
    {0x73, 0x00},
    {0x74, 0x53},
    {0x75, 0x00},
    {0x76, 0x63},
    {0x77, 0x00},
    {0x78, 0x74},
    {0x79, 0x00},
    {0x7A, 0x83},
    {0x7B, 0x00},
    {0x7C, 0x8F},
    {0x7D, 0x00},
    {0x7E, 0x9A},
    {0x7F, 0x00},
    {0x80, 0xA4},
    {0x81, 0x00},
    {0x82, 0xAE},
    {0x83, 0x00},
    {0x84, 0xD1},
    {0x85, 0x00},
    {0x86, 0xED},
    {0x87, 0x01},
    {0x88, 0x1C},
    {0x89, 0x01},
    {0x8A, 0x41},
    {0x8B, 0x01},
    {0x8C, 0x7B},
    {0x8D, 0x01},
    {0x8E, 0xAB},
    {0x8F, 0x01},
    {0x90, 0xAC},
    {0x91, 0x01},
    {0x92, 0xD9},
    {0x93, 0x02},
    {0x94, 0x05},
    {0x95, 0x02},
    {0x96, 0x1F},
    {0x97, 0x02},
    {0x98, 0x42},
    {0x99, 0x02},
    {0x9A, 0x5B},
    {0x9B, 0x02},
    {0x9C, 0x88},
    {0x9D, 0x02},
    {0x9E, 0x97},
    {0x9F, 0x02},
    {0xA0, 0xA9},
    {0xA2, 0x02},
    {0xA3, 0xC0},
    {0xA4, 0x02},
    {0xA5, 0xE4},
    {0xA6, 0x03},
    {0xA7, 0x10},
    {0xA9, 0x03},
    {0xAA, 0x66},
    {0xAB, 0x03},
    {0xAC, 0x97},
    {0xAD, 0x03},
    {0xAE, 0x99}
};

static char mipi_sh_andy_cmd_GAMMABLUEnega[60][2] = {
    {0xAF, 0x00},
    {0xB0, 0x44},
    {0xB1, 0x00},
    {0xB2, 0x53},
    {0xB3, 0x00},
    {0xB4, 0x63},
    {0xB5, 0x00},
    {0xB6, 0x74},
    {0xB7, 0x00},
    {0xB8, 0x83},
    {0xB9, 0x00},
    {0xBA, 0x8F},
    {0xBB, 0x00},
    {0xBC, 0x9A},
    {0xBD, 0x00},
    {0xBE, 0xA4},
    {0xBF, 0x00},
    {0xC0, 0xAE},
    {0xC1, 0x00},
    {0xC2, 0xD1},
    {0xC3, 0x00},
    {0xC4, 0xED},
    {0xC5, 0x01},
    {0xC6, 0x1C},
    {0xC7, 0x01},
    {0xC8, 0x41},
    {0xC9, 0x01},
    {0xCA, 0x7B},
    {0xCB, 0x01},
    {0xCC, 0xAB},
    {0xCD, 0x01},
    {0xCE, 0xAC},
    {0xCF, 0x01},
    {0xD0, 0xD4},
    {0xD1, 0x02},
    {0xD2, 0x00},
    {0xD3, 0x02},
    {0xD4, 0x1A},
    {0xD5, 0x02},
    {0xD6, 0x39},
    {0xD7, 0x02},
    {0xD8, 0x52},
    {0xD9, 0x02},
    {0xDA, 0x75},
    {0xDB, 0x02},
    {0xDC, 0x85},
    {0xDD, 0x02},
    {0xDE, 0x96},
    {0xDF, 0x02},
    {0xE0, 0xAB},
    {0xE1, 0x02},
    {0xE2, 0xCD},
    {0xE3, 0x02},
    {0xE4, 0xF9},
    {0xE5, 0x03},
    {0xE6, 0x4F},
    {0xE7, 0x03},
    {0xE8, 0x80},
    {0xE9, 0x03},
    {0xEA, 0x82}
};

static char mipi_sh_andy_cmd_CGOUTMappingSetting[80][2] ={
    {0x00, 0x0A},
    {0x01, 0x06},
    {0x02, 0x12},
    {0x03, 0x0E},
    {0x04, 0x38},
    {0x05, 0x18},
    {0x06, 0x1C},
    {0x07, 0x19},
    {0x08, 0x1D},
    {0x09, 0x1A},
    {0x0A, 0x16},
    {0x0B, 0x1B},
    {0x0C, 0x17},
    {0x0D, 0x38},
    {0x0E, 0x0C},
    {0x0F, 0x08},
    {0x10, 0x14},
    {0x11, 0x10},
    {0x12, 0x04},
    {0x13, 0x05},
    {0x14, 0x0C},
    {0x15, 0x08},
    {0x16, 0x14},
    {0x17, 0x10},
    {0x18, 0x38},
    {0x19, 0x18},
    {0x1A, 0x1C},
    {0x1B, 0x19},
    {0x1C, 0x1D},
    {0x1D, 0x1A},
    {0x1E, 0x16},
    {0x1F, 0x1B},
    {0x20, 0x17},
    {0x21, 0x38},
    {0x22, 0x0A},
    {0x23, 0x06},
    {0x24, 0x12},
    {0x25, 0x0E},
    {0x26, 0x04},
    {0x27, 0x05},
    {0x28, 0x10},
    {0x29, 0x14},
    {0x2A, 0x08},
    {0x2B, 0x0C},
    {0x2D, 0x38},
    {0x2F, 0x17},
    {0x30, 0x1B},
    {0x31, 0x16},
    {0x32, 0x1A},
    {0x33, 0x1D},
    {0x34, 0x19},
    {0x35, 0x1C},
    {0x36, 0x18},
    {0x37, 0x38},
    {0x38, 0x0E},
    {0x39, 0x12},
    {0x3A, 0x06},
    {0x3B, 0x0A},
    {0x3D, 0x04},
    {0x3F, 0x05},
    {0x40, 0x0E},
    {0x41, 0x12},
    {0x42, 0x06},
    {0x43, 0x0A},
    {0x44, 0x38},
    {0x45, 0x17},
    {0x46, 0x1B},
    {0x47, 0x16},
    {0x48, 0x1A},
    {0x49, 0x1D},
    {0x4A, 0x19},
    {0x4B, 0x1C},
    {0x4C, 0x18},
    {0x4D, 0x38},
    {0x4E, 0x10},
    {0x4F, 0x14},
    {0x50, 0x08},
    {0x51, 0x0C},
    {0x52, 0x04},
    {0x53, 0x05}
};


static char mipi_sh_andy_cmd_GIPTimingSetting[26][2] = {
    {0x54, 0x06},
    {0x55, 0x05},
    {0x56, 0x04},
    {0x58, 0x03},
    {0x59, 0x1B},
    {0x5A, 0x1B},
    {0x5B, 0x01},
    {0x5C, 0x32},
    {0x5E, 0x24},
    {0x5F, 0x25},
    {0x60, 0x24},
    {0x61, 0x25},
    {0x62, 0x28},
    {0x63, 0x01},
    {0x64, 0x32},
    {0x65, 0x00},
    {0x66, 0x44},
    {0x67, 0x11},
    {0x68, 0x01},
    {0x69, 0x20},
    {0x6A, 0x04},
    {0x6B, 0x22},
    {0x6C, 0x08},
    {0x6D, 0x08},
    {0x78, 0x00},
    {0x79, 0x00}
};

static char mipi_sh_andy_cmd_GateEQSetting[4][2] = {
    {0x7E, 0xFF},
    {0x7F, 0xFF},
    {0x80, 0xFF},
    {0x81, 0x00}
};

static char mipi_sh_andy_cmd_DisplayLineSetting[6][2] = {
    {0x8D, 0x00},
    {0x8E, 0x00},
    {0x8F, 0xC0},
    {0x90, RTN },
    {0x91, 0x0D},
    {0x92, 0x0A}
};

static char mipi_sh_andy_cmd_SuspendTimingSetting[24][2] = {
    {0x96, 0x11},
    {0x97, 0x14},
    {0x98, 0x00},
    {0x99, 0x00},
    {0x9A, 0x00},
    {0x9B, GIP },
    {0x9C, 0x15},
    {0x9D, 0x30},
    {0x9F, 0x0F},
    {0xA2, 0xB0},
    {0xA7, 0x0A},
    {0xA9, 0x00},
    {0xAA, 0x70},
    {0xAB, 0xDA},
    {0xAC, 0xFF},
    {0xAE, 0xF4},
    {0xAF, 0x40},
    {0xB0, 0x7F},
    {0xB1, 0x16},
    {0xB2, 0x53},
    {0xB3, 0x00},
    {0xB4, 0x2A},
    {0xB5, 0x3A},
    {0xB6, 0xF0}
};

static char mipi_sh_andy_cmd_PowerOn_OffTimingSetting[24][2] = {
    {0xBC, 0x85},
    {0xBD, 0xF4},
    {0xBE, 0x33},
    {0xBF, 0x13},
    {0xC0, 0x77},
    {0xC1, 0x77},
    {0xC2, 0x77},
    {0xC3, 0x77},
    {0xC4, 0x77},
    {0xC5, 0x77},
    {0xC6, 0x77},
    {0xC7, 0x77},
    {0xC8, 0xAA},
    {0xC9, 0x2A},
    {0xCA, 0x00},
    {0xCB, 0xAA},
    {0xCC, 0x12},
    {0xCD, 0x00},
    {0xCE, 0x18},
    {0xCF, 0x88},
    {0xD0, 0xA0},
    {0xD1, 0x00},
    {0xD2, 0x00},
    {0xD3, 0x00}
};

static char mipi_sh_andy_cmd_SynchronizeSignalSetting[3][2] = {
    {0xD6, 0x00},
    {0xD7, 0x00},
    {0xD8, 0x00}
};

static char mipi_sh_andy_cmd_SynchronizeSignalSetting2[3][2] = {
    {0xD6, 0x07},
    {0xD7, 0x00},
    {0xD8, 0x00}
};

static char mipi_sh_andy_cmd_BAPFunctionSetting[4][2] = {
    {0xED, 0x00},
    {0xEE, 0x00},
    {0xEF, 0x70},
    {0xFA, 0x03}
};

static char mipi_sh_andy_cmd_EngineerSetting[6][2] = {
    {0x40, 0x62},
    {0x41, 0x51},
    {0x42, 0x84},
    {0x1E, 0x66},
    {0x3A, 0x60},
    {0x39, 0x26}
};

static char mipi_sh_andy_cmd_exit_sleep_mode[1] = {0x11};
static char mipi_sh_andy_cmd_set_display_on[1] = {0x29};

static char mipi_sh_andy_cmd_SetDisplayOff[1] = {0x28};
static char mipi_sh_andy_cmd_VCOM1_OFF_Setting[2] = {0x13, VCOM1_L / 2};
static char mipi_sh_andy_cmd_VCOM2_OFF_Setting[2] = {0x14, VCOM2_L / 2};
static char mipi_sh_andy_cmd_EnterSleepMode[1] = {0x10};
static char mipi_sh_andy_cmd_DeepStanby[2] = {0x4F, 0x01};


/* Test Image Generator ON */
static char mipi_sh_andy_cmd_TestImageGeneratorOn[7]   = {0xDE,
     0x01, 0x00, 0x02, 0x00, 0x00, 0xFF};

/* Test Image Generator OFF */
static char mipi_sh_andy_cmd_TestImageGeneratorOff[7]  = {0xDE,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


/*      Initial Setting 1                                                   */
static struct dsi_cmd_desc mipi_sh_andy_cmds_initial1[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_LRFlip},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_MIPIVideoBurstwrite},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PorchLineSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PorchLineSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PorchLineSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PorchLineSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PorchLineSetting[4]},


    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_MTPLoadStop},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_MTPLoadStop},
};

/*      Initial Setting 1.0                                                   */
static struct dsi_cmd_desc mipi_sh_andy_cmds_initial1_regulator_ts1_0[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[24]}
};

/*      Initial Setting 1.1                                                   */
static struct dsi_cmd_desc mipi_sh_andy_cmds_initial1_regulator_ts1_1[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[24]}
};

/*      Initial Setting 1.1 flicker_unadjusted                                */
static struct dsi_cmd_desc mipi_sh_andy_cmds_initial1_regulator_ts1_1_flicker_unadjusted[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[24]}
};

/*      Initial Setting 2.0                                                */
static struct dsi_cmd_desc mipi_sh_andy_cmds_initial1_regulator_ts2_0[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[24]}
};

/*      Initial Setting 2.0 flicker_unadjusted                                */
static struct dsi_cmd_desc mipi_sh_andy_cmds_initial1_regulator_ts2_0_flicker_unadjusted[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_RegulatorPumpSetting[24]}
};

/*      Device Code Read                                                    */
#if 0
static struct dsi_cmd_desc mipi_sh_andy_cmds_deviceCode[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[1]},
    {DTYPE_GEN_READ1,  1, 0, 0, 0, 2, mipi_sh_andy_cmd_DeviceCodeRead},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[0]},
};
#endif

/*      Gamma Setting                                                       */
static struct dsi_cmd_desc mipi_sh_andy_cmds_gamma[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[24]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[25]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[26]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[27]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[28]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[29]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[30]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[31]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[32]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[33]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[34]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[35]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[36]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[37]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[38]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[39]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[40]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[41]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[42]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[43]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[44]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[45]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[46]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[47]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[48]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[49]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[50]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[51]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[52]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[53]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[54]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[55]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[56]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[57]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[58]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDposi[59]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[24]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[25]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[26]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[27]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[28]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[29]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[30]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[31]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[32]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[33]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[34]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[35]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[36]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[37]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[38]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[39]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[40]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[41]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[42]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[43]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[44]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[45]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[46]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[47]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[48]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[49]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[50]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[51]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[52]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[53]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[54]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[55]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[56]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[57]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[58]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAREDnega[59]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[11]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[2]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[24]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[25]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[26]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[27]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[28]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[29]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[30]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[31]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[32]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[33]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[34]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[35]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[36]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[37]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[38]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[39]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[40]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[41]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[42]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[43]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[44]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[45]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[46]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[47]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[48]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[49]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[50]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[51]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[52]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[53]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[54]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[55]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[56]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[57]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[58]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENposi[59]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[24]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[25]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[26]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[27]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[28]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[29]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[30]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[31]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[32]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[33]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[34]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[35]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[36]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[37]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[38]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[39]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[40]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[41]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[42]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[43]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[44]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[45]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[46]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[47]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[48]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[49]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[50]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[51]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[52]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[53]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[54]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[55]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[56]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[57]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[58]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMAGREENnega[59]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[24]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[25]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[26]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[27]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[28]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[29]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[30]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[31]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[32]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[33]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[34]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[35]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[36]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[37]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[38]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[39]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[40]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[41]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[42]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[43]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[44]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[45]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[46]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[47]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[48]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[49]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[50]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[51]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[52]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[53]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[54]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[55]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[56]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[57]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[58]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEposi[59]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[24]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[25]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[26]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[27]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[28]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[29]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[30]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[31]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[32]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[33]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[34]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[35]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[36]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[37]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[38]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[39]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[40]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[41]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[42]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[43]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[44]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[45]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[46]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[47]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[48]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[49]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[50]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[51]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[52]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[53]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[54]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[55]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[56]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[57]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[58]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_set_val_GAMMABLUEnega[59]}
};

/*      Terminal control Setting                                            */
static struct dsi_cmd_desc mipi_sh_andy_cmds_terminal[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_MTPLoadStop},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_MTPLoadStop},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[24]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[25]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[26]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[27]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[28]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[29]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[30]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[31]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[32]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[33]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[34]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[35]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[36]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[37]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[40]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[41]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[42]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[43]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[44]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[45]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[46]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[47]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[48]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[49]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[50]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[51]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[52]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[53]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[54]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[55]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[56]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[57]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[60]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[61]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[62]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[63]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[64]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[65]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[66]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[67]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[68]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[69]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[70]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[71]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[72]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[73]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[74]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[75]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[76]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_CGOUTMappingSetting[77]}
};

/*      Timing Setting                                                      */
static struct dsi_cmd_desc mipi_sh_andy_cmds_timing[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[22]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[23]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GIPTimingSetting[24]}
};

/*      Initial Setting 2                                                   */
static struct dsi_cmd_desc mipi_sh_andy_cmds_initial2[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GateEQSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GateEQSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_GateEQSetting[2]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_DisplayLineSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_DisplayLineSetting[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_DisplayLineSetting[5]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[17]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[18]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SuspendTimingSetting[22]}
};

/*      Power Setting                                                       */
static struct dsi_cmd_desc mipi_sh_andy_cmds_power[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[8]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[9]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[10]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[11]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[12]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[13]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[14]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[15]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[16]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[19]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[20]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[21]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_PowerOn_OffTimingSetting[23]}
};

/*      Synchronization signal Setting                                      */
static struct dsi_cmd_desc mipi_sh_andy_cmds_sync_ts1_0[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SynchronizeSignalSetting[0]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_BAPFunctionSetting[2]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[6]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_VCOMOFFSetting},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_MTPLoadStop},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_MTPLoadStop},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[0]}
};

static struct dsi_cmd_desc mipi_sh_andy_cmds_sync_ts2_0[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SynchronizeSignalSetting[0]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_BAPFunctionSetting[2]},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[7]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[3]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[4]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_EngineerSetting[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_MTPLoadStop},

    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[0]}
};

/*      Preparation display ON                                              */
static struct dsi_cmd_desc mipi_sh_andy_cmds_display_on[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, 1, mipi_sh_andy_cmd_exit_sleep_mode}
};

static struct dsi_cmd_desc mipi_sh_andy_cmds_display_on_start[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, 1, mipi_sh_andy_cmd_set_display_on},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SynchronizeSignalSetting2[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SynchronizeSignalSetting2[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SynchronizeSignalSetting2[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[0]}
};

static struct dsi_cmd_desc mipi_sh_andy_cmds_display_on_after_balck_screen[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_BiasSetting_AfterBlackScreen[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_BiasSetting_AfterBlackScreen[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_BiasSetting_AfterBlackScreen[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[0]}
};

static struct dsi_cmd_desc mipi_sh_andy_cmds_display_on_balck_screen[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_BiasSetting_BlackScreenOn[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_BiasSetting_BlackScreenOn[1]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_BiasSetting_BlackScreenOn[2]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[0]}
};

static struct dsi_cmd_desc mipi_sh_andy_cmds_display_off[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[5]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SynchronizeSignalSetting[0]},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_SwitchCommand[0]},
    {DTYPE_DCS_WRITE,  1, 0, 0, (WAIT_1FRAME_US*1), 1, mipi_sh_andy_cmd_SetDisplayOff},
    {DTYPE_DCS_WRITE,  1, 0, 0, 0, 2, mipi_sh_andy_cmd_SetNormalMode},
    {DTYPE_DCS_WRITE,  1, 0, 0, (WAIT_1FRAME_US*4), 1, mipi_sh_andy_cmd_EnterSleepMode},
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, mipi_sh_andy_cmd_DeepStanby},
};

/* Test Image Generator ON */
static struct dsi_cmd_desc mipi_sh_andy_cmds_TIG_on[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_andy_cmd_TestImageGeneratorOn),
        mipi_sh_andy_cmd_TestImageGeneratorOn}
};

/* Test Image Generator OFF */
static struct dsi_cmd_desc mipi_sh_andy_cmds_TIG_off[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_andy_cmd_TestImageGeneratorOff),
        mipi_sh_andy_cmd_TestImageGeneratorOff}
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
    RTN,
    GIP,
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
    RTN_B,
    GIP_B,
};

static struct shdisp_panel_operations shdisp_andy_fops = {
    shdisp_andy_API_init_io,
    shdisp_andy_API_exit_io,
    shdisp_andy_API_empty_func,
    shdisp_andy_API_set_param,
    shdisp_andy_API_power_on,
    shdisp_andy_API_power_off,
    shdisp_andy_API_empty_func,
    shdisp_andy_API_empty_func,
    shdisp_andy_API_empty_func,
    shdisp_andy_API_empty_func,
    shdisp_andy_API_empty_func,
    shdisp_andy_API_check_upper_unit,
    shdisp_andy_API_check_flicker_param,
    shdisp_andy_API_diag_write_reg,
    shdisp_andy_API_diag_read_reg,
    shdisp_andy_API_diag_set_flicker_param,
    shdisp_andy_API_diag_get_flicker_param,
    shdisp_andy_API_check_recovery,
    shdisp_andy_API_recovery_type,
    shdisp_andy_API_set_abl_lut,
    shdisp_andy_API_disp_update,
    shdisp_andy_API_disp_clear_screen,
    shdisp_andy_API_diag_get_flicker_low_param,
    shdisp_andy_API_diag_set_gamma_info,
    shdisp_andy_API_diag_get_gamma_info,
    shdisp_andy_API_diag_set_gamma,
    shdisp_andy_API_power_on_recovery,
    shdisp_andy_API_power_off_recovery,
    shdisp_andy_API_set_drive_freq,
};

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */
#define SHDISP_ANDY_SW_CHK_UPPER_UNIT

/* ------------------------------------------------------------------------- */
/* MACRAOS                                                                   */
/* ------------------------------------------------------------------------- */
#define MIPI_DSI_COMMAND_TX_CLMR(x)         (shdisp_panel_API_mipi_dsi_cmds_tx(&shdisp_mipi_andy_tx_buf, x, ARRAY_SIZE(x)))
#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
#define MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(x)  (shdisp_panel_API_mipi_dsi_cmds_mltshortpkt_tx(&shdisp_mipi_andy_tx_buf, x, ARRAY_SIZE(x)))
#endif
#define MIPI_DSI_COMMAND_TX_CLMR_DUMMY(x)   (shdisp_panel_API_mipi_dsi_cmds_tx_dummy(x))
#define IS_FLICKER_ADJUSTED(param) (((param & 0xF000) == 0x9000) ? 1 : 0)

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_create                                                    */
/* ------------------------------------------------------------------------- */
struct shdisp_panel_operations *shdisp_andy_API_create(void)
{
    SHDISP_DEBUG("\n");
    return &shdisp_andy_fops;
}


static int shdisp_andy_mipi_dsi_buf_alloc(struct dsi_buf *dp, int size)
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
/* shdisp_andy_API_init_io                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_init_io(void)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    unsigned short tmp_alpha;
    unsigned short tmp_alpha_low;
    unsigned short tmp;
    unsigned short vcomdcoff;
#endif
    int ret = 0;
    struct shdisp_lcddr_phy_gamma_reg* phy_gamma;

    SHDISP_DEBUG("in\n");

    (void)shdisp_andy_regulator_init();

#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
    shdisp_andy_mipi_dsi_buf_alloc(&shdisp_mipi_andy_tx_buf, 2048);
#else
    shdisp_andy_mipi_dsi_buf_alloc(&shdisp_mipi_andy_tx_buf, DSI_BUF_SIZE);
#endif
    shdisp_andy_mipi_dsi_buf_alloc(&shdisp_mipi_andy_rx_buf, DSI_BUF_SIZE);

#ifndef SHDISP_NOT_SUPPORT_FLICKER
    tmp_alpha = shdisp_api_get_alpha();
    tmp_alpha_low = shdisp_api_get_alpha_low();
    SHDISP_DEBUG("alpha=0x%04x alpha_low=0x%04x\n", tmp_alpha, tmp_alpha_low);
    mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM1_L][1] = (char) (tmp_alpha & 0xFF);
    mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM2_L][1] = mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM1_L][1];
    mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM12_H][1] = (char) (mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM12_H][1] & 0xFC);
    if ((tmp_alpha >> 8) & 0x01){
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM12_H][1] |= 0x03;
    }
    SHDISP_DEBUG("VCOM1_L=0x%02x VCOM2_L=0x%02x VCOM12_H=0x%02x\n",
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM1_L][1],
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM2_L][1],
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOM12_H][1]);

    if ((tmp_alpha % 2) == 0){
        vcomdcoff = tmp_alpha / 2;
    } else {
        vcomdcoff = (tmp_alpha + 1) / 2;
    }

    if (tmp_alpha_low - tmp_alpha >= 0){
        tmp = tmp_alpha_low - tmp_alpha;
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_LPVCOM1][1] = (tmp & 0x0F);
    } else {
        tmp = tmp_alpha - tmp_alpha_low - 1;
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_LPVCOM1][1] = ((tmp & 0x0F) | 0x10);
    }
    mipi_sh_andy_cmd_RegulatorPumpSetting[NO_LPVCOM2][1] = mipi_sh_andy_cmd_RegulatorPumpSetting[NO_LPVCOM1][1];
    if (vcomdcoff & 0x100){
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_LPVCOM2][1] |= 0x80;
    }
    mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOMOFF][1] = (unsigned char) (vcomdcoff & 0xFF);
    SHDISP_DEBUG("LPVCOM1=0x%02x LPVCOM2=0x%02x VCOMOFF=0x%02x\n",
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_LPVCOM1][1],
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_LPVCOM2][1],
        mipi_sh_andy_cmd_RegulatorPumpSetting[NO_VCOMOFF][1]);

#endif
    phy_gamma = shdisp_api_get_lcddr_phy_gamma();
    ret = shdisp_andy_init_phy_gamma(phy_gamma);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_andy_init_phy_gamma.\n");
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_exit_io                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_exit_io(void)
{
    SHDISP_DEBUG("in\n");
    (void)shdisp_andy_regulator_exit();
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_empty_func                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_empty_func(void)
{
    SHDISP_DEBUG("\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_set_param                                                 */
/* ------------------------------------------------------------------------- */
static void shdisp_andy_API_set_param(struct shdisp_panel_param_str *param_str)
{
    SHDISP_DEBUG("in\n");
    andy_param_str.vcom_alpha = param_str->vcom_alpha;
    andy_param_str.vcom_alpha_low = param_str->vcom_alpha_low;
    SHDISP_DEBUG("out\n");

    return;
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_power_on                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_power_on(void)
{

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
   current_page = 0;
#endif
    SHDISP_DEBUG("in\n");
    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);

    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);

    /* eR63311_rev100_120928.pdf */
    /* IOVCC ON */
    (void)shdisp_andy_regulator_on();

    /* VS4 enters automatically at system start-up */
    shdisp_clmr_api_disp_init();


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
    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_SYS_cmd_delay_us(1*1000);
    shdisp_bdic_API_LCD_release_hw_reset();
    shdisp_SYS_cmd_delay_us(10*1000);
    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_SYS_cmd_delay_us(1*1000);
    shdisp_bdic_API_LCD_release_hw_reset();
    shdisp_SYS_cmd_delay_us(10*1000);
#if 0
    shdisp_SYS_cmd_delay_us(1000);
    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_ON);
#endif
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif


    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_power_off                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_power_off(void)
{
    SHDISP_DEBUG("in\n");

//#ifdef USE_LINUX
//#endif   /* USE_LINUX */

    shdisp_clmr_api_display_stop();

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_bdic_API_LCD_m_power_off();
    shdisp_bdic_API_LCD_power_off();
    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_SYS_cmd_delay_us(5*1000);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    /* IOVCC OFF */
    (void)shdisp_andy_regulator_off();

    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_check_upper_unit                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_check_upper_unit(void)
{
#ifndef SHDISP_ANDY_SW_CHK_UPPER_UNIT
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
#endif /* SHDISP_ANDY_SW_CHK_UPPER_UNIT */
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_check_flicker_param                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    unsigned short tmp_alpha = alpha_in;

    SHDISP_DEBUG("in alpha_in=%d\n", alpha_in);
    if (alpha_out == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha_out.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if ((tmp_alpha & 0xF000) != 0x9000) {
        *alpha_out = VCOM1_L;
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_SUCCESS;
    }

    tmp_alpha = tmp_alpha & 0x01FF;
    if ((tmp_alpha < SHDISP_ANDY_VCOM_MIN) || (tmp_alpha > SHDISP_ANDY_VCOM_MAX)) {
        *alpha_out = VCOM1_L;
        SHDISP_DEBUG("out2\n");
        return SHDISP_RESULT_SUCCESS;
    }

    *alpha_out = tmp_alpha;

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}

//#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
static int shdisp_andy_API_start_video(void)
{
    SHDISP_DEBUG("in\n");
    {
        char pageChg[4]    = { DTYPE_DCS_WRITE1, 0xff, 00, 00 };
        char displayOn[4] = { DTYPE_DCS_WRITE, 0x29, 00, 00 };
        shdisp_FWCMD_buf_init(0);
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE, sizeof(pageChg), pageChg );
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE, sizeof(displayOn), displayOn );
        shdisp_FWCMD_buf_finish();
        shdisp_FWCMD_doKick(1, 0, 0);
    }
    {
        char clmrPstVideoOn[4] = { 00, 00, 00, 0x03 };
        shdisp_SYS_clmr_sio_transfer( SHDISP_CLMR_REG_PSTCTL, clmrPstVideoOn, sizeof(clmrPstVideoOn), 0, 0 );
    }

    shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_1);

    shdisp_bdic_API_IRQ_det_irq_ctrl(1);
    shdisp_clmr_api_fw_detlcdandbdic_ctrl(1);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}
static int shdisp_andy_API_stop_video(void)
{
    SHDISP_DEBUG("in\n");

    shdisp_clmr_api_fw_detlcdandbdic_ctrl(0);
    shdisp_bdic_API_IRQ_det_irq_ctrl(0);

    shdisp_panel_API_request_RateCtrl(0,0,0);

    {
        char pageChg[4]    = { DTYPE_DCS_WRITE1, 0xff, 00, 00 };
        char displayOff[4] = { DTYPE_DCS_WRITE, 0x28, 00, 00 };

        shdisp_FWCMD_buf_init(0);
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE, sizeof(pageChg), pageChg );
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE, sizeof(displayOff), displayOff );
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
        char pageRestore[4]    = { DTYPE_DCS_WRITE1, 0xff, 00, 00 };
        pageRestore[2] = (char)current_page;
        shdisp_FWCMD_buf_init(0);
        shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_LP_SWRITE, sizeof(pageRestore), pageRestore );
        shdisp_FWCMD_buf_finish();
        shdisp_FWCMD_doKick(1, 0, 0);
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}
//#endif

/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_diag_write_reg                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    if(addr == 0xff)
    {
        current_page = *write_data;
    }
    SHDISP_DEBUG("current_page = %d\n", current_page);
    shdisp_andy_API_stop_video();
#endif
/* DSI read test edit >>> */
#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
#else
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
#endif
/* DSI read test edit <<< */
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf, addr, write_data, size);
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    shdisp_SYS_delay_us(50*1000);

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    shdisp_andy_API_start_video();
#endif
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_diag_read_reg                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    shdisp_andy_API_stop_video();
#endif

/* DSI read test edit >>> */
#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
#else
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
#endif
/* DSI read test edit <<< */

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf, &shdisp_mipi_andy_rx_buf, addr, read_data, size);
    shdisp_SYS_delay_us(50*1000);
#endif
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf, &shdisp_mipi_andy_rx_buf, addr, read_data, size);

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    shdisp_andy_API_start_video();
#endif
    if(ret) {
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_diag_set_flicker_param                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_diag_set_flicker_param(unsigned short alpha)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int i;
    int ret = 0;
    char pageChange[4] = { DTYPE_DCS_WRITE1, 0xff, 01, 00 };
    unsigned char andy_rdata_tmp[8];
#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    unsigned char back_page = 0;
#endif
    unsigned short vcomdcoff;

    SHDISP_DEBUG("in\n");
    if ((alpha < SHDISP_ANDY_VCOM_MIN) || (alpha > SHDISP_ANDY_VCOM_MAX)) {
        SHDISP_ERR("<INVALID_VALUE> alpha(0x%04X).\n", alpha);
        return SHDISP_RESULT_FAILURE;
    }

    for (i=1; i<=7; i++) {
        andy_rdata[i] = 0;
        andy_rdata_tmp[i] = 0;
        andy_wdata[i] = 0;
    }

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    back_page = current_page;
    shdisp_andy_API_stop_video();
#endif
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_buf_init(0);
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
    shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_HS_SWRITE, sizeof(pageChange), pageChange );
//#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
//#endif

    andy_rdata_tmp[0] = (unsigned char) (alpha & 0xFF);
    andy_rdata_tmp[1] = (unsigned char) (alpha & 0xFF);
    andy_rdata_tmp[2] = 0x60;
    if ((alpha >> 8) & 0x01){
        andy_rdata_tmp[2] = (unsigned char) (andy_rdata_tmp[2] | 0x03);
    }
    if ((alpha % 2) == 0){
        vcomdcoff = alpha / 2;
    } else {
        vcomdcoff = (alpha + 1) / 2;
    }
    andy_rdata_tmp[3] = 0x00;
    andy_rdata_tmp[4] = 0x00;
    if ((vcomdcoff >> 8) & 0x01){
        andy_rdata_tmp[4] = (unsigned char) (andy_rdata_tmp[4] | 0x80);
    }
    andy_rdata_tmp[5] = (unsigned char) (vcomdcoff & 0xFF);

#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
    for (i=0; i<ANDY_VCOM_REG_NUM; i++) {
        andy_wdata[i] = andy_rdata_tmp[i];
    }
    ret = shdisp_panel_API_mipi_diag_mltshortpkt_write_reg(&shdisp_mipi_andy_tx_buf, Andy_VCOM_Reg, andy_wdata, ANDY_VCOM_REG_NUM);
    if (ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg!!\n" );
    }
#else
    for (i=0; i<ANDY_VCOM_REG_NUM; i++) {
        andy_wdata[0] = andy_rdata_tmp[i];
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf, Andy_VCOM_Reg[i], andy_wdata, 1);
        if (ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg addr=0x%02x data=0x%02x\n", Andy_VCOM_Reg[i], andy_wdata[0]);
        }
    }
#endif

    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    current_page = back_page;
    shdisp_andy_API_start_video();
#endif
    SHDISP_DEBUG("VCOM1_L=0x%02x VCOM2_L=0x%02x VCOM12_H=0x%02x\n",
        andy_rdata_tmp[0],andy_rdata_tmp[1],andy_rdata_tmp[2]);
    SHDISP_DEBUG("LPVCOM1=0x%02x LPVCOM2=0x%02x VCOMOFF=0x%02x\n",
        andy_rdata_tmp[3],andy_rdata_tmp[4],andy_rdata_tmp[5]);
    SHDISP_DEBUG("alpha=0x%04x\n", alpha);
    andy_param_str.vcom_alpha = alpha;
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_diag_get_flicker_param                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_diag_get_flicker_param(unsigned short *alpha)
{
    int ret = SHDISP_RESULT_SUCCESS;
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int i;
    char pageChange[4] = { DTYPE_DCS_WRITE1, 0xff, 01, 00 };
    unsigned char andy_rdata_tmp[8];
#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    unsigned char back_page = 0;
#endif

    SHDISP_DEBUG("in\n");
    if (alpha == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha.\n");
        return SHDISP_RESULT_FAILURE;
    }

    for (i=1; i<=7; i++) {
        andy_rdata[i] = 0;
        andy_rdata_tmp[i] = 0;
    }

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    back_page = current_page;
    shdisp_andy_API_stop_video();
#endif
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_buf_init(0);
    shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_HS_SWRITE, sizeof(pageChange), pageChange );
    shdisp_FWCMD_buf_finish();
    shdisp_FWCMD_doKick(1, 0, 0);

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf, &shdisp_mipi_andy_rx_buf, Andy_VCOM_Reg[0], andy_rdata, 1);
    shdisp_SYS_delay_us(50*1000);
#endif
    for (i=0; i<3; i++) {
        if (i != 1){
            ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf, &shdisp_mipi_andy_rx_buf, Andy_VCOM_Reg[i], andy_rdata, 1);
            if (ret) {
                SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg addr=0x%02x data=0x%02x\n", Andy_VCOM_Reg[i], andy_rdata[0]);
            }
            andy_rdata_tmp[i] = andy_rdata[0];
        }
    }

    *alpha = ((andy_rdata_tmp[2] & 0x01) << 8) | andy_rdata_tmp[0];

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    current_page = back_page;
    shdisp_andy_API_start_video();
#endif

    SHDISP_DEBUG("out alpha=0x%04X\n", *alpha);
#endif
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_diag_get_flicker_low_param                               */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_diag_get_flicker_low_param(unsigned short *alpha)
{
    int ret = SHDISP_RESULT_SUCCESS;
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int i;
    char pageChange[4] = { DTYPE_DCS_WRITE1, 0xff, 01, 00 };
    unsigned char andy_rdata_tmp[8];
#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    unsigned char back_page = 0;
#endif
    unsigned short tmp_alpha;

    SHDISP_DEBUG("in\n");
    if (alpha == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha.\n");
        return SHDISP_RESULT_FAILURE;
    }

    for (i=1; i<=7; i++) {
        andy_rdata[i] = 0;
        andy_rdata_tmp[i] = 0;
    }

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    back_page = current_page;
    shdisp_andy_API_stop_video();
#endif
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_buf_init(0);
    shdisp_FWCMD_buf_add( SHDISP_CLMR_FWCMD_DSI_HS_SWRITE, sizeof(pageChange), pageChange );
    shdisp_FWCMD_buf_finish();
    shdisp_FWCMD_doKick(1, 0, 0);

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf, &shdisp_mipi_andy_rx_buf, Andy_VCOM_Reg[0], andy_rdata, 1);
    shdisp_SYS_delay_us(50*1000);
#endif
    for (i=0; i<4; i++) {
        if (i != 1){
            ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf, &shdisp_mipi_andy_rx_buf, Andy_VCOM_Reg[i], andy_rdata, 1);
            if (ret) {
                SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg addr=0x%02x data=0x%02x\n", Andy_VCOM_Reg[i], andy_rdata[0]);
            }
            andy_rdata_tmp[i] = andy_rdata[0];
        }
    }

    tmp_alpha = ((andy_rdata_tmp[2] & 0x01) << 8) | andy_rdata_tmp[0];
    if (andy_rdata_tmp[3] & 0x10){
        *alpha = tmp_alpha - (andy_rdata_tmp[3] & 0x0f) - 1;
    } else {
        *alpha = tmp_alpha + (andy_rdata_tmp[3] & 0x0f);
    }

#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
    current_page = back_page;
    shdisp_andy_API_start_video();
#endif

    SHDISP_DEBUG("out low_alpha=0x%04X\n", *alpha);
#endif
    return ret;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_check_recovery                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_check_recovery(void)
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
/* shdisp_andy_API_recovery_type                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_recovery_type(int *type)
{
    SHDISP_DEBUG("in\n");
    *type = SHDISP_SUBSCRIBE_TYPE_INT;

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_set_abl_lut                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_disp_update                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_disp_update(struct shdisp_main_update *update)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_disp_clear_screen                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_disp_clear_screen(struct shdisp_main_clear *clear)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_diag_set_gamma_info                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info, int set_applied_voltage)
{
#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
    int i, j = 0;
    int ret = 0;
    unsigned char andy_gamma_wdata[372];
    unsigned char andy_gamma_addr[372] = {
        mipi_sh_andy_cmd_SwitchCommand[1][0],
        mipi_sh_andy_cmd_GAMMAREDposi[0][0],
        mipi_sh_andy_cmd_GAMMAREDposi[1][0],
        mipi_sh_andy_cmd_GAMMAREDposi[2][0],
        mipi_sh_andy_cmd_GAMMAREDposi[3][0],
        mipi_sh_andy_cmd_GAMMAREDposi[4][0],
        mipi_sh_andy_cmd_GAMMAREDposi[5][0],
        mipi_sh_andy_cmd_GAMMAREDposi[6][0],
        mipi_sh_andy_cmd_GAMMAREDposi[7][0],
        mipi_sh_andy_cmd_GAMMAREDposi[8][0],
        mipi_sh_andy_cmd_GAMMAREDposi[9][0],
        mipi_sh_andy_cmd_GAMMAREDposi[10][0],
        mipi_sh_andy_cmd_GAMMAREDposi[11][0],
        mipi_sh_andy_cmd_GAMMAREDposi[12][0],
        mipi_sh_andy_cmd_GAMMAREDposi[13][0],
        mipi_sh_andy_cmd_GAMMAREDposi[14][0],
        mipi_sh_andy_cmd_GAMMAREDposi[15][0],
        mipi_sh_andy_cmd_GAMMAREDposi[16][0],
        mipi_sh_andy_cmd_GAMMAREDposi[17][0],
        mipi_sh_andy_cmd_GAMMAREDposi[18][0],
        mipi_sh_andy_cmd_GAMMAREDposi[19][0],
        mipi_sh_andy_cmd_GAMMAREDposi[20][0],
        mipi_sh_andy_cmd_GAMMAREDposi[21][0],
        mipi_sh_andy_cmd_GAMMAREDposi[22][0],
        mipi_sh_andy_cmd_GAMMAREDposi[23][0],
        mipi_sh_andy_cmd_GAMMAREDposi[24][0],
        mipi_sh_andy_cmd_GAMMAREDposi[25][0],
        mipi_sh_andy_cmd_GAMMAREDposi[26][0],
        mipi_sh_andy_cmd_GAMMAREDposi[27][0],
        mipi_sh_andy_cmd_GAMMAREDposi[28][0],
        mipi_sh_andy_cmd_GAMMAREDposi[29][0],
        mipi_sh_andy_cmd_GAMMAREDposi[30][0],
        mipi_sh_andy_cmd_GAMMAREDposi[31][0],
        mipi_sh_andy_cmd_GAMMAREDposi[32][0],
        mipi_sh_andy_cmd_GAMMAREDposi[33][0],
        mipi_sh_andy_cmd_GAMMAREDposi[34][0],
        mipi_sh_andy_cmd_GAMMAREDposi[35][0],
        mipi_sh_andy_cmd_GAMMAREDposi[36][0],
        mipi_sh_andy_cmd_GAMMAREDposi[37][0],
        mipi_sh_andy_cmd_GAMMAREDposi[38][0],
        mipi_sh_andy_cmd_GAMMAREDposi[39][0],
        mipi_sh_andy_cmd_GAMMAREDposi[40][0],
        mipi_sh_andy_cmd_GAMMAREDposi[41][0],
        mipi_sh_andy_cmd_GAMMAREDposi[42][0],
        mipi_sh_andy_cmd_GAMMAREDposi[43][0],
        mipi_sh_andy_cmd_GAMMAREDposi[44][0],
        mipi_sh_andy_cmd_GAMMAREDposi[45][0],
        mipi_sh_andy_cmd_GAMMAREDposi[46][0],
        mipi_sh_andy_cmd_GAMMAREDposi[47][0],
        mipi_sh_andy_cmd_GAMMAREDposi[48][0],
        mipi_sh_andy_cmd_GAMMAREDposi[49][0],
        mipi_sh_andy_cmd_GAMMAREDposi[50][0],
        mipi_sh_andy_cmd_GAMMAREDposi[51][0],
        mipi_sh_andy_cmd_GAMMAREDposi[52][0],
        mipi_sh_andy_cmd_GAMMAREDposi[53][0],
        mipi_sh_andy_cmd_GAMMAREDposi[54][0],
        mipi_sh_andy_cmd_GAMMAREDposi[55][0],
        mipi_sh_andy_cmd_GAMMAREDposi[56][0],
        mipi_sh_andy_cmd_GAMMAREDposi[57][0],
        mipi_sh_andy_cmd_GAMMAREDposi[58][0],
        mipi_sh_andy_cmd_GAMMAREDposi[59][0],
        mipi_sh_andy_cmd_GAMMAREDnega[0][0],
        mipi_sh_andy_cmd_GAMMAREDnega[1][0],
        mipi_sh_andy_cmd_GAMMAREDnega[2][0],
        mipi_sh_andy_cmd_GAMMAREDnega[3][0],
        mipi_sh_andy_cmd_GAMMAREDnega[4][0],
        mipi_sh_andy_cmd_GAMMAREDnega[5][0],
        mipi_sh_andy_cmd_GAMMAREDnega[6][0],
        mipi_sh_andy_cmd_GAMMAREDnega[7][0],
        mipi_sh_andy_cmd_GAMMAREDnega[8][0],
        mipi_sh_andy_cmd_GAMMAREDnega[9][0],
        mipi_sh_andy_cmd_GAMMAREDnega[10][0],
        mipi_sh_andy_cmd_GAMMAREDnega[11][0],
        mipi_sh_andy_cmd_GAMMAREDnega[12][0],
        mipi_sh_andy_cmd_GAMMAREDnega[13][0],
        mipi_sh_andy_cmd_GAMMAREDnega[14][0],
        mipi_sh_andy_cmd_GAMMAREDnega[15][0],
        mipi_sh_andy_cmd_GAMMAREDnega[16][0],
        mipi_sh_andy_cmd_GAMMAREDnega[17][0],
        mipi_sh_andy_cmd_GAMMAREDnega[18][0],
        mipi_sh_andy_cmd_GAMMAREDnega[19][0],
        mipi_sh_andy_cmd_GAMMAREDnega[20][0],
        mipi_sh_andy_cmd_GAMMAREDnega[21][0],
        mipi_sh_andy_cmd_GAMMAREDnega[22][0],
        mipi_sh_andy_cmd_GAMMAREDnega[23][0],
        mipi_sh_andy_cmd_GAMMAREDnega[24][0],
        mipi_sh_andy_cmd_GAMMAREDnega[25][0],
        mipi_sh_andy_cmd_GAMMAREDnega[26][0],
        mipi_sh_andy_cmd_GAMMAREDnega[27][0],
        mipi_sh_andy_cmd_GAMMAREDnega[28][0],
        mipi_sh_andy_cmd_GAMMAREDnega[29][0],
        mipi_sh_andy_cmd_GAMMAREDnega[30][0],
        mipi_sh_andy_cmd_GAMMAREDnega[31][0],
        mipi_sh_andy_cmd_GAMMAREDnega[32][0],
        mipi_sh_andy_cmd_GAMMAREDnega[33][0],
        mipi_sh_andy_cmd_GAMMAREDnega[34][0],
        mipi_sh_andy_cmd_GAMMAREDnega[35][0],
        mipi_sh_andy_cmd_GAMMAREDnega[36][0],
        mipi_sh_andy_cmd_GAMMAREDnega[37][0],
        mipi_sh_andy_cmd_GAMMAREDnega[38][0],
        mipi_sh_andy_cmd_GAMMAREDnega[39][0],
        mipi_sh_andy_cmd_GAMMAREDnega[40][0],
        mipi_sh_andy_cmd_GAMMAREDnega[41][0],
        mipi_sh_andy_cmd_GAMMAREDnega[42][0],
        mipi_sh_andy_cmd_GAMMAREDnega[43][0],
        mipi_sh_andy_cmd_GAMMAREDnega[44][0],
        mipi_sh_andy_cmd_GAMMAREDnega[45][0],
        mipi_sh_andy_cmd_GAMMAREDnega[46][0],
        mipi_sh_andy_cmd_GAMMAREDnega[47][0],
        mipi_sh_andy_cmd_GAMMAREDnega[48][0],
        mipi_sh_andy_cmd_GAMMAREDnega[49][0],
        mipi_sh_andy_cmd_GAMMAREDnega[50][0],
        mipi_sh_andy_cmd_GAMMAREDnega[51][0],
        mipi_sh_andy_cmd_GAMMAREDnega[52][0],
        mipi_sh_andy_cmd_GAMMAREDnega[53][0],
        mipi_sh_andy_cmd_GAMMAREDnega[54][0],
        mipi_sh_andy_cmd_GAMMAREDnega[55][0],
        mipi_sh_andy_cmd_GAMMAREDnega[56][0],
        mipi_sh_andy_cmd_GAMMAREDnega[57][0],
        mipi_sh_andy_cmd_GAMMAREDnega[58][0],
        mipi_sh_andy_cmd_GAMMAREDnega[59][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[0][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[1][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[2][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[3][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[4][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[5][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[6][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[7][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[8][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[9][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[10][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[11][0],
        mipi_sh_andy_cmd_SwitchCommand[2][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[12][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[13][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[14][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[15][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[16][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[17][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[18][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[19][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[20][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[21][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[22][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[23][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[24][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[25][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[26][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[27][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[28][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[29][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[30][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[31][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[32][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[33][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[34][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[35][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[36][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[37][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[38][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[39][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[40][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[41][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[42][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[43][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[44][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[45][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[46][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[47][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[48][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[49][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[50][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[51][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[52][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[53][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[54][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[55][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[56][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[57][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[58][0],
        mipi_sh_andy_cmd_GAMMAGREENposi[59][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[0][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[1][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[2][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[3][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[4][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[5][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[6][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[7][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[8][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[9][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[10][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[11][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[12][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[13][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[14][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[15][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[16][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[17][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[18][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[19][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[20][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[21][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[22][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[23][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[24][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[25][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[26][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[27][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[28][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[29][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[30][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[31][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[32][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[33][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[34][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[35][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[36][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[37][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[38][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[39][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[40][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[41][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[42][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[43][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[44][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[45][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[46][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[47][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[48][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[49][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[50][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[51][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[52][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[53][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[54][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[55][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[56][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[57][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[58][0],
        mipi_sh_andy_cmd_GAMMAGREENnega[59][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[0][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[1][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[2][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[3][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[4][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[5][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[6][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[7][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[8][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[9][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[10][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[11][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[12][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[13][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[14][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[15][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[16][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[17][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[18][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[19][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[20][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[21][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[22][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[23][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[24][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[25][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[26][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[27][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[28][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[29][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[30][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[31][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[32][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[33][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[34][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[35][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[36][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[37][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[38][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[39][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[40][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[41][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[42][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[43][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[44][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[45][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[46][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[47][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[48][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[49][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[50][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[51][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[52][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[53][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[54][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[55][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[56][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[57][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[58][0],
        mipi_sh_andy_cmd_GAMMABLUEposi[59][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[0][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[1][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[2][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[3][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[4][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[5][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[6][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[7][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[8][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[9][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[10][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[11][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[12][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[13][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[14][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[15][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[16][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[17][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[18][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[19][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[20][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[21][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[22][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[23][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[24][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[25][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[26][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[27][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[28][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[29][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[30][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[31][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[32][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[33][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[34][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[35][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[36][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[37][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[38][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[39][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[40][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[41][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[42][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[43][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[44][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[45][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[46][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[47][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[48][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[49][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[50][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[51][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[52][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[53][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[54][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[55][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[56][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[57][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[58][0],
        mipi_sh_andy_cmd_GAMMABLUEnega[59][0],
        mipi_sh_andy_cmd_SwitchCommand[1][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGH][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGL][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDP][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDN][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDP2][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGHO][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGLO][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_AVDDR][0],
        mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_AVEER][0],
    };

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);

    for (i = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        if (i == 0) {
            andy_gamma_wdata[j++] = mipi_sh_andy_cmd_SwitchCommand[1][1];
        }
        andy_gamma_wdata[j++] = ((gamma_info->gammaR[i] >> 8) & 0x0003);
        andy_gamma_wdata[j++] =  (gamma_info->gammaR[i]       & 0x00FF);
    }

    for (i = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        if (i == 6) {
            andy_gamma_wdata[j++] = mipi_sh_andy_cmd_SwitchCommand[2][1];
        }
        andy_gamma_wdata[j++] = ((gamma_info->gammaG[i] >> 8) & 0x0003);
        andy_gamma_wdata[j++] =  (gamma_info->gammaG[i]       & 0x00FF);
    }

    for (i = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        andy_gamma_wdata[j++] = ((gamma_info->gammaB[i] >> 8) & 0x0003);
        andy_gamma_wdata[j++] =  (gamma_info->gammaB[i]       & 0x00FF);
    }

    if (!set_applied_voltage) {
        ret = shdisp_panel_API_mipi_diag_mltshortpkt_write_reg(&shdisp_mipi_andy_tx_buf, andy_gamma_addr, andy_gamma_wdata, j);
        if (ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_mltshortpkt_write_reg!!\n" );
            goto shdisp_end;
        }
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> doKick shdisp_panel_API_mipi_diag_mltshortpkt_write_reg.\n");
            goto shdisp_end;
        }
        goto shdisp_end;
    }

    andy_gamma_wdata[j++] = mipi_sh_andy_cmd_SwitchCommand[1][1];
    andy_gamma_wdata[j++] = gamma_info->vgh;
    andy_gamma_wdata[j++] = gamma_info->vgl;
    andy_gamma_wdata[j++] = gamma_info->gvddp;
    andy_gamma_wdata[j++] = gamma_info->gvddn;
    andy_gamma_wdata[j++] = gamma_info->gvddp2;
    andy_gamma_wdata[j++] = gamma_info->vgho;
    andy_gamma_wdata[j++] = gamma_info->vglo;
    andy_gamma_wdata[j++] = gamma_info->avddr;
    andy_gamma_wdata[j++] = gamma_info->aveer;

    ret = shdisp_panel_API_mipi_diag_mltshortpkt_write_reg(&shdisp_mipi_andy_tx_buf, andy_gamma_addr, andy_gamma_wdata, j);
    if (ret) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_mltshortpkt_write_reg!!\n" );
        goto shdisp_end;
    }
    ret = shdisp_FWCMD_buf_finish();
    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_FWCMD_doKick(1, 0, NULL);
    }
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> doKick shdisp_panel_API_mipi_diag_mltshortpkt_write_reg.\n");
        goto shdisp_end;
    }
    memcpy(&diag_tmp_gamma_info, gamma_info, sizeof(diag_tmp_gamma_info));
    diag_tmp_gamma_info_set = 1;

shdisp_end:

    shdisp_FWCMD_buf_set_nokick(0);
    shdisp_FWCMD_buf_init(0);

    shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_SwitchCommand[0][0],
                                               &mipi_sh_andy_cmd_SwitchCommand[0][1],
                                               1);

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
#else
    int i, j;
    int ret = 0;
    unsigned char andy_wdata[1];

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_buf_set_nokick(1);
    shdisp_FWCMD_buf_init(SHDISP_CLMR_FWCMD_APINO_OTHER);

    for (i = 0, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE / 2; i++) {
        if (i == 0) {
            ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                       mipi_sh_andy_cmd_SwitchCommand[1][0],
                                                       &mipi_sh_andy_cmd_SwitchCommand[1][1],
                                                       1);
            if(ret) {
                SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
                goto shdisp_end;
            }
        }
        andy_wdata[0] = ((gamma_info->gammaR[i] >> 8) & 0x0003);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMAREDposi[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
        andy_wdata[0] = (gamma_info->gammaR[i] & 0x00FF);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMAREDposi[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
    }

    for (i = SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        andy_wdata[0] = ((gamma_info->gammaR[i] >> 8) & 0x0003);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMAREDnega[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
        andy_wdata[0] = (gamma_info->gammaR[i] & 0x00FF);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMAREDnega[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
    }

    for (i = 0, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE / 2; i++) {
        if (i == 6) {
            ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                       mipi_sh_andy_cmd_SwitchCommand[2][0],
                                                       &mipi_sh_andy_cmd_SwitchCommand[2][1],
                                                       1);
            if(ret) {
                SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
                goto shdisp_end;
            }
        }
        andy_wdata[0] = ((gamma_info->gammaG[i] >> 8) & 0x0003);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMAGREENposi[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
        andy_wdata[0] = (gamma_info->gammaG[i] & 0x00FF);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMAGREENposi[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
    }

    for (i = SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        andy_wdata[0] = ((gamma_info->gammaG[i] >> 8) & 0x0003);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMAGREENnega[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
        andy_wdata[0] = (gamma_info->gammaG[i] & 0x00FF);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMAGREENnega[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
    }

    for (i = 0, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE / 2; i++) {
        andy_wdata[0] = ((gamma_info->gammaB[i] >> 8) & 0x0003);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMABLUEposi[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
        andy_wdata[0] = (gamma_info->gammaB[i] & 0x00FF);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMABLUEposi[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
    }

    for (i = SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        andy_wdata[0] = ((gamma_info->gammaB[i] >> 8) & 0x0003);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMABLUEnega[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
        andy_wdata[0] = (gamma_info->gammaB[i] & 0x00FF);
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                   mipi_sh_andy_cmd_GAMMABLUEnega[j++][0],
                                                   andy_wdata,
                                                   1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
    }

    if (!set_applied_voltage) {
        ret = shdisp_FWCMD_buf_finish();
        if (ret == SHDISP_RESULT_SUCCESS) {
            ret = shdisp_FWCMD_doKick(1, 0, NULL);
        }
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
            goto shdisp_end;
        }
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_SwitchCommand[1][0],
                                               &mipi_sh_andy_cmd_SwitchCommand[1][1],
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGH][0],
                                               &gamma_info->vgh,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGL][0],
                                               &gamma_info->vgl,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDP][0],
                                               &gamma_info->gvddp,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDN][0],
                                               &gamma_info->gvddn,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDP2][0],
                                               &gamma_info->gvddp2,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGHO][0],
                                               &gamma_info->vgho,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGLO][0],
                                               &gamma_info->vglo,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_AVDDR][0],
                                               &gamma_info->avddr,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_AVEER][0],
                                               &gamma_info->aveer,
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    ret = shdisp_FWCMD_buf_finish();
    if (ret == SHDISP_RESULT_SUCCESS) {
        ret = shdisp_FWCMD_doKick(1, 0, NULL);
    }
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    memcpy(&diag_tmp_gamma_info, gamma_info, sizeof(diag_tmp_gamma_info));
    diag_tmp_gamma_info_set = 1;

shdisp_end:

    shdisp_FWCMD_buf_set_nokick(0);
    shdisp_FWCMD_buf_init(0);

    shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_SwitchCommand[0][0],
                                               &mipi_sh_andy_cmd_SwitchCommand[0][1],
                                               1);

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
#endif
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_diag_get_gamma_info                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info, int set_applied_voltage)
{
    int i, j;
    int ret = 0;
    unsigned char andy_rdata[1];
    unsigned short andy_temp_data[SHDISP_ANDY_GAMMA_SETTING_SIZE];

    SHDISP_DEBUG("in\n");
    if (gamma_info == NULL){
        SHDISP_ERR("<NULL_POINTER> gamma_info.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);

    memset(andy_temp_data, 0, sizeof(andy_temp_data));
    for (i = 0, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE / 2; i++) {
        if (i == 0) {
            ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                       mipi_sh_andy_cmd_SwitchCommand[1][0],
                                                       &mipi_sh_andy_cmd_SwitchCommand[1][1],
                                                       1);
            if(ret) {
                SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
                goto shdisp_end;
            }
            memset(andy_rdata, 0, sizeof(andy_rdata));
            ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                      &shdisp_mipi_andy_rx_buf,
                                                      mipi_sh_andy_cmd_GAMMAREDposi[0][0],
                                                      andy_rdata,
                                                      1);
        }
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMAREDposi[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] = ((andy_rdata[0] << 8) & 0x0300);
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMAREDposi[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] |= (andy_rdata[0] & 0x00FF);
    }

    for (i = SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMAREDnega[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] = ((andy_rdata[0] << 8) & 0x0300);
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMAREDnega[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] |= (andy_rdata[0] & 0x00FF);
    }
    memcpy(gamma_info->gammaR, andy_temp_data, sizeof(andy_temp_data));

    memset(andy_temp_data, 0, sizeof(andy_temp_data));
    for (i = 0, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE / 2; i++) {
        if (i == 6) {
            ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                                       mipi_sh_andy_cmd_SwitchCommand[2][0],
                                                       &mipi_sh_andy_cmd_SwitchCommand[2][1],
                                                       1);
            if(ret) {
                SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
                goto shdisp_end;
            }
        }
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMAGREENposi[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] = ((andy_rdata[0] << 8) & 0x0300);
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMAGREENposi[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] |= (andy_rdata[0] & 0x00FF);
    }

    for (i = SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMAGREENnega[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] = ((andy_rdata[0] << 8) & 0x0300);
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMAGREENnega[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] |= (andy_rdata[0] & 0x00FF);
    }
    memcpy(gamma_info->gammaG, andy_temp_data, sizeof(andy_temp_data));

    memset(andy_temp_data, 0, sizeof(andy_temp_data));
    for (i = 0, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE / 2; i++) {
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMABLUEposi[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] = ((andy_rdata[0] << 8) & 0x0300);
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMABLUEposi[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] |= (andy_rdata[0] & 0x00FF);
    }

    for (i = SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET, j = 0; i < SHDISP_ANDY_GAMMA_SETTING_SIZE; i++) {
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMABLUEnega[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] = ((andy_rdata[0] << 8) & 0x0300);
        memset(andy_rdata, 0, sizeof(andy_rdata));
        ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                                  &shdisp_mipi_andy_rx_buf,
                                                  mipi_sh_andy_cmd_GAMMABLUEnega[j++][0],
                                                  andy_rdata,
                                                  1);
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_panel_API_mipi_diag_read_reg.\n");
            goto shdisp_end;
        }
        andy_temp_data[i] |= (andy_rdata[0] & 0x00FF);
    }
    memcpy(gamma_info->gammaB, andy_temp_data, sizeof(andy_temp_data));

    if (!set_applied_voltage) {
        goto shdisp_end;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_SwitchCommand[1][0],
                                               &mipi_sh_andy_cmd_SwitchCommand[1][1],
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        goto shdisp_end;
    }

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGH][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->vgh = andy_rdata[0];

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGL][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->vgl = andy_rdata[0];

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDP][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->gvddp = andy_rdata[0];

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDN][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->gvddn = andy_rdata[0];

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_GVDDP2][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->gvddp2 = andy_rdata[0];

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGHO][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->vgho = andy_rdata[0];

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_VGLO][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->vglo = andy_rdata[0];

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_AVDDR][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->avddr = andy_rdata[0];

    memset(andy_rdata, 0, sizeof(andy_rdata));
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                               mipi_sh_andy_cmd_RegulatorPumpSetting[SHDISP_ANDY_AVEER][0],
                                              andy_rdata,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        goto shdisp_end;
    }
    gamma_info->aveer = andy_rdata[0];

shdisp_end:

    shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_SwitchCommand[0][0],
                                               &mipi_sh_andy_cmd_SwitchCommand[0][1],
                                               1);

    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_diag_set_gamma_info                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");

    ret = shdisp_andy_diag_set_gamma_info(gamma_info, 1);
    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_diag_get_gamma_info                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");

    shdisp_andy_API_stop_video();
    ret = shdisp_andy_diag_get_gamma_info(gamma_info, 1);
    shdisp_andy_API_start_video();
    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_diag_set_gamma                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_diag_set_gamma(struct shdisp_diag_gamma *gamma)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
    if ((gamma->level < SHDISP_ANDY_GAMMA_LEVEL_MIN) || (gamma->level > SHDISP_ANDY_GAMMA_LEVEL_MAX)) {
        SHDISP_ERR("<INVALID_VALUE> gamma->level(%d).\n", gamma->level);
        return SHDISP_RESULT_FAILURE;
    }


    if (!diag_tmp_gamma_info_set) {

        shdisp_andy_API_stop_video();
        ret = shdisp_andy_diag_get_gamma_info(&diag_tmp_gamma_info, 0);
        shdisp_andy_API_start_video();
        if(ret) {
            SHDISP_ERR("<RESULT_FAILURE> shdisp_andy_diag_get_gamma_info.\n");
            goto shdisp_end;
        }
        diag_tmp_gamma_info_set = 1;
    }

    diag_tmp_gamma_info.gammaR[gamma->level - 1]                                       = gamma->gammaR_p;
    diag_tmp_gamma_info.gammaR[SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET + (gamma->level - 1)] = gamma->gammaR_n;
    diag_tmp_gamma_info.gammaG[gamma->level - 1]                                       = gamma->gammaG_p;
    diag_tmp_gamma_info.gammaG[SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET + (gamma->level - 1)] = gamma->gammaG_n;
    diag_tmp_gamma_info.gammaB[gamma->level - 1]                                       = gamma->gammaB_p;
    diag_tmp_gamma_info.gammaB[SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET + (gamma->level - 1)] = gamma->gammaB_n;

    ret = shdisp_andy_diag_set_gamma_info(&diag_tmp_gamma_info, 0);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_andy_diag_set_gamma_info.\n");
        goto shdisp_end;
    }

shdisp_end:


    if(ret) {
        return ret;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_sqe_set_drive_freq                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_sqe_set_drive_freq(int type)
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
/* shdisp_andy_API_set_drive_freq                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_set_drive_freq(int type)
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

    shdisp_andy_sqe_set_drive_freq(type);

    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_DEBUG("done.\n");
    return ret;
}

unsigned char shdisp_andy_mipi_manufacture_id(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char andy_rdata_tmp[1];

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_SwitchCommand[1][0],
                                               &mipi_sh_andy_cmd_SwitchCommand[1][1],
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
    }
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_andy_tx_buf,
                                              &shdisp_mipi_andy_rx_buf,
                                              mipi_sh_andy_cmd_DeviceCodeRead[0],
                                              andy_rdata_tmp,
                                              1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
    }
        ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_andy_tx_buf,
                                               mipi_sh_andy_cmd_SwitchCommand[0][0],
                                               &mipi_sh_andy_cmd_SwitchCommand[0][1],
                                               1);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
    }

    return andy_rdata_tmp[0];
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_regulator_init                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_regulator_init(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    if (shdisp_api_get_hw_handset() == 1 && shdisp_api_get_hw_revision() >= SHDISP_HW_REV_PP2) {
        SHDISP_DEBUG("in\n");
        regu_lcd_vddi = regulator_get(NULL, "lcd_vddi");
        if (IS_ERR(regu_lcd_vddi)) {
            SHDISP_ERR("lcd_vddi get failed.\n");
            regu_lcd_vddi = NULL;
            return SHDISP_RESULT_FAILURE;
        }
        if (shdisp_api_get_boot_disp_status() == SHDISP_MAIN_DISP_ON) {
            ret = regulator_enable(regu_lcd_vddi);
            if (ret < 0) {
                SHDISP_ERR("lcd_vddi enable failed, ret=%d\n", ret);
                return SHDISP_RESULT_FAILURE;
            }
        }
        SHDISP_DEBUG("out\n");
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_regulator_exit                                                */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_regulator_exit(void)
{
    if (shdisp_api_get_hw_handset() == 1 && shdisp_api_get_hw_revision() >= SHDISP_HW_REV_PP2) {
        SHDISP_DEBUG("in\n");
        shdisp_andy_regulator_off();
        regulator_put(regu_lcd_vddi);
        regu_lcd_vddi = NULL;
        SHDISP_DEBUG("out\n");
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_regulator_on                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_regulator_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    /* IOVCC ON */
    if (shdisp_api_get_hw_handset() == 1 && shdisp_api_get_hw_revision() >= SHDISP_HW_REV_PP2) {
        SHDISP_DEBUG("in\n");
        ret = regulator_enable(regu_lcd_vddi);
        if (ret < 0) {
            SHDISP_ERR("lcd_vddi enable failed, ret=%d\n", ret);
            return SHDISP_RESULT_FAILURE;
        }
        shdisp_SYS_delay_us(2000);

        SHDISP_DEBUG("out\n");
    }
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_andy_regulator_off                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_regulator_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    /* IOVCC OFF */
    if (shdisp_api_get_hw_handset() == 1 && shdisp_api_get_hw_revision() >= SHDISP_HW_REV_PP2) {
        SHDISP_DEBUG("in\n");
        ret = regulator_disable(regu_lcd_vddi);
        if (ret < 0) {
            SHDISP_ERR("lcd_vddi disable failed, ret=%d\n", ret);
            return SHDISP_RESULT_FAILURE;
        }
        shdisp_SYS_delay_us(2000);
        SHDISP_DEBUG("out\n");
    }
    return SHDISP_RESULT_SUCCESS;
}

static int shdisp_andy_mipi_cmd_display_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
    shdisp_clmr_api_hsclk_off();

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_display_on);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out ret=%d\n", ret);
        return ret;
    }

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif
//#ifdef  USE_LINUX
//#endif  /* USE_LINUX */

    SHDISP_DEBUG("out\n");
    return ret;
}

static int shdisp_andy_mipi_cmd_start_display(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");

    ret = shdisp_andy_mipi_cmd_lcd_on_after_black_screen();
    shdisp_clmr_api_auto_pat_ctrl(MIPI_SHARP_CLMR_AUTO_PAT_OFF);

    shdisp_andy_mipi_cmd_display_on();
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif


    shdisp_clmr_api_data_transfer_starts();

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_1);

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    if (drive_freq_lasy_type != SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A) {
        shdisp_andy_sqe_set_drive_freq(drive_freq_lasy_type);
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    shdisp_clmr_api_hsclk_on();

    shdisp_SYS_cmd_delay_us(WAIT_1FRAME_US*5);

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_display_on_start);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out9 ret=%d\n", ret);
        return ret;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
    shdisp_clmr_api_hsclk_off();


#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    SHDISP_DEBUG("out\n");

    return ret;
}

static int shdisp_andy_mipi_cmd_lcd_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned short tmp_vcom1;

    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
   tmp_vcom1 = (unsigned short) mipi_sh_andy_cmd_RegulatorPumpSetting[12][1];
   if (mipi_sh_andy_cmd_RegulatorPumpSetting[14][1] & 0x03){
       tmp_vcom1 |= 0x100;
   }
   mipi_sh_andy_cmd_VCOM1_OFF_Setting[1] = (char) ((tmp_vcom1 / 2) & 0xFF);
   mipi_sh_andy_cmd_VCOM2_OFF_Setting[1] = (char) ((tmp_vcom1 / 2) & 0xFF);
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_display_off);

    diag_tmp_gamma_info_set = 0;

    SHDISP_DEBUG("out\n");
    return ret;
}

/* Test Image Generator ON */
int shdisp_panel_andy_API_TestImageGen(int onoff)
{
    int ret = SHDISP_RESULT_SUCCESS;
    SHDISP_DEBUG("in\n");
    if (onoff) {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_TIG_on);
    }
    else {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_TIG_off);
    }
    SHDISP_DEBUG("out\n");
    return ret;
}

static int shdisp_andy_mipi_cmd_lcd_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;
    unsigned char device_code = 0;

    SHDISP_DEBUG("in\n");


    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    shdisp_clmr_api_hsclk_on();

#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
    ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_initial1);
#else
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_initial1);
#endif

    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out1 ret=%d\n", ret);
        return ret;
    }
    shdisp_FWCMD_safe_finishanddoKick();

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
    shdisp_clmr_api_hsclk_off();

    if (shdisp_api_get_device_code() == 0xff)
    {
        struct dsi_buf *tp = &shdisp_mipi_andy_tx_buf;
        struct dsi_buf *rp = &shdisp_mipi_andy_rx_buf;
        unsigned char addr = mipi_sh_andy_cmd_DeviceCodeRead[0];
        struct dsi_cmd_desc cmd[1];
        char cmd_buf[2+1];

        cmd_buf[0] = addr;
        cmd_buf[1] = 0x00;


        cmd[0].dtype    = DTYPE_GEN_READ1;
        cmd[0].last     = 0x01;
        cmd[0].vc       = 0x00;
        cmd[0].ack      = 0x01;
        cmd[0].wait     = 0x00;
        cmd[0].dlen     = 1;
        cmd[0].payload  = cmd_buf;

        memset(rp->data, 0, sizeof(rp->data));
        if (shdisp_panel_API_mipi_dsi_cmds_rx(tp, rp, cmd, 1) != SHDISP_RESULT_SUCCESS){
        }

        memset(rp->data, 0, sizeof(rp->data));
        rp->data[5] = 0xff;
        if (shdisp_panel_API_mipi_dsi_cmds_rx(tp, rp, cmd, 1) != SHDISP_RESULT_SUCCESS){
            SHDISP_ERR("mipi_dsi_cmds_rx error\n");
            return -ENODEV;
        }
        device_code = rp->data[5];
        SHDISP_DEBUG("device_code = 0x%02x\n", device_code);

        shdisp_api_set_device_code(device_code);
    }
    else {
        device_code = shdisp_api_get_device_code();
        SHDISP_DEBUG("device_code = 0x%02x\n", device_code);
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    shdisp_clmr_api_hsclk_on();

#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
    {

        if (device_code == 0x00){
            ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_initial1_regulator_ts1_0);
            if (ret != SHDISP_RESULT_SUCCESS){
                SHDISP_DEBUG("out1-3 ret=%d\n", ret);
                return ret;
            }
        }
        else if (device_code == 0x01) {
            if (IS_FLICKER_ADJUSTED(shdisp_api_get_alpha_nvram())) {
                ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_initial1_regulator_ts1_1);
                if (ret != SHDISP_RESULT_SUCCESS){
                    SHDISP_DEBUG("out1-3 ret=%d\n", ret);
                    return ret;
                }
            }
            else {
                ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_initial1_regulator_ts1_1_flicker_unadjusted);
                if (ret != SHDISP_RESULT_SUCCESS){
                    SHDISP_DEBUG("out1-3 ret=%d\n", ret);
                    return ret;
                }
            }
        }
        else {
            if (IS_FLICKER_ADJUSTED(shdisp_api_get_alpha_nvram())) {
                ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_initial1_regulator_ts2_0);
                if (ret != SHDISP_RESULT_SUCCESS){
                    SHDISP_DEBUG("out1-3 ret=%d\n", ret);
                    return ret;
                }
            }
            else {
                ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_initial1_regulator_ts2_0_flicker_unadjusted);
                if (ret != SHDISP_RESULT_SUCCESS){
                    SHDISP_DEBUG("out1-3 ret=%d\n", ret);
                    return ret;
                }
            }
        }

        ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_gamma);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out2 ret=%d\n", ret);
            return ret;
        }

        ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_terminal);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out3 ret=%d\n", ret);
            return ret;
        }

        ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_timing);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out4 ret=%d\n", ret);
            return ret;
        }

        ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_initial2);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out5 ret=%d\n", ret);
            return ret;
        }

        ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_power);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out6 ret=%d\n", ret);
            return ret;
        }

        if ((device_code == 0x00) || (device_code == 0x01)){
            ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_sync_ts1_0);
            if (ret != SHDISP_RESULT_SUCCESS){
                SHDISP_DEBUG("out7 ret=%d\n", ret);
                return ret;
            }
        }
        else {
            ret = MIPI_DSI_COMMAND_MLTPKT_TX_CLMR(mipi_sh_andy_cmds_sync_ts2_0);
            if (ret != SHDISP_RESULT_SUCCESS){
                SHDISP_DEBUG("out7 ret=%d\n", ret);
                return ret;
            }
        }
    }
#else

    if (device_code == 0x00){
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_initial1_regulator_ts1_0);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out1-3 ret=%d\n", ret);
            return ret;
        }
    }
    else if (device_code == 0x01) {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_initial1_regulator_ts1_1);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out1-3 ret=%d\n", ret);
            return ret;
        }
    }
    else {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_initial1_regulator_ts2_0);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out1-3 ret=%d\n", ret);
            return ret;
        }
    }

    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_gamma);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out2 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_terminal);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out3 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_timing);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out4 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_initial2);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out5 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_power);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out6 ret=%d\n", ret);
        return ret;
    }
    if ((device_code == 0x00) || (device_code == 0x01)){
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_sync_ts1_0);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out7 ret=%d\n", ret);
            return ret;
        }
    }
    else {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_sync_ts2_0);
        if (ret != SHDISP_RESULT_SUCCESS){
            SHDISP_DEBUG("out7 ret=%d\n", ret);
            return ret;
        }
    }
#endif

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif
    shdisp_clmr_api_custom_blk_init();
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_LOW_POWER_MODE);
    shdisp_clmr_api_hsclk_off();


    SHDISP_DEBUG("out\n");
    return ret;
}

static int shdisp_andy_mipi_cmd_lcd_off_black_screen_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_display_on_balck_screen);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif
    SHDISP_DEBUG("out\n");
    return ret;
}

static int shdisp_andy_mipi_cmd_lcd_on_after_black_screen(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_andy_cmds_display_on_after_balck_screen);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif
    SHDISP_DEBUG("out\n");
    return ret;
}

int shdisp_andy_API_mipi_lcd_on(void)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

//#else
    ret = shdisp_andy_mipi_cmd_lcd_on();
//#endif

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

int shdisp_andy_API_mipi_lcd_off(void)
{
    SHDISP_DEBUG("in\n");
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

//#ifdef USE_LINUX
//#else
    shdisp_andy_mipi_cmd_lcd_off();
//#endif

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    SHDISP_DEBUG("out\n");
    return 0;
}

int shdisp_andy_API_mipi_lcd_on_after_black_screen(void)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");
    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}

int shdisp_andy_API_mipi_lcd_off_black_screen_on(void)
{
    SHDISP_DEBUG("in\n");
    shdisp_clmr_api_auto_pat_ctrl(MIPI_SHARP_CLMR_1HZ_BLACK_ON);
    shdisp_andy_mipi_cmd_lcd_off_black_screen_on();
    SHDISP_DEBUG("out\n");
    return 0;
}

int shdisp_andy_API_mipi_start_display(void)
{
    SHDISP_DEBUG("in\n");
//#ifdef USE_LINUX
//#endif   /* USE_LINUX */
//#ifdef SHDISP_FW_STACK_EXCUTE
//#endif

    shdisp_andy_mipi_cmd_start_display();

//#ifdef SHDISP_FW_STACK_EXCUTE
//#endif
    SHDISP_DEBUG("out\n");
    return 0;
}

int shdisp_andy_API_cabc_init(void)
{
    return 0;
}

int shdisp_andy_API_cabc_indoor_on(void)
{
    return 0;
}

int shdisp_andy_API_cabc_outdoor_on(int lut_level)
{
    return 0;
}

int shdisp_andy_API_cabc_off(int wait_on, int pwm_disable)
{
    return 0;
}

int shdisp_andy_API_cabc_outdoor_move(int lut_level)
{
    return 0;
}


/* ------------------------------------------------------------------------- */
/* shdisp_andy_API_power_on_recovery                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_power_on_recovery(void)
{
#ifdef SHDISP_ANDY_PROVISIONAL_REG_RW
   current_page = 0;
#endif
    SHDISP_DEBUG("in\n");

    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);

    /* IOVCC ON */
    (void)shdisp_andy_regulator_on();

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
    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_SYS_cmd_delay_us(1*1000);
    shdisp_bdic_API_LCD_release_hw_reset();
    shdisp_SYS_cmd_delay_us(10*1000);
    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_SYS_cmd_delay_us(1*1000);
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
/* shdisp_andy_API_power_off_recovery                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_andy_API_power_off_recovery(void)
{
    SHDISP_DEBUG("in\n");

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    shdisp_bdic_API_LCD_m_power_off();
    shdisp_bdic_API_LCD_power_off();
    shdisp_bdic_API_LCD_set_hw_reset();
    shdisp_SYS_cmd_delay_us(5*1000);

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    /* IOVCC OFF */
    (void)shdisp_andy_regulator_off();

    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);
    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


int shdisp_andy_init_phy_gamma(struct shdisp_lcddr_phy_gamma_reg *phy_gamma)
{
    int ret = 0;
    int cnt, idx;
    unsigned int checksum;

    SHDISP_DEBUG("in\n");

    memcpy(mipi_sh_andy_set_val_GAMMAREDposi,         mipi_sh_andy_cmd_GAMMAREDposi,         sizeof(mipi_sh_andy_set_val_GAMMAREDposi));
    memcpy(mipi_sh_andy_set_val_GAMMAREDnega,         mipi_sh_andy_cmd_GAMMAREDnega,         sizeof(mipi_sh_andy_set_val_GAMMAREDnega));
    memcpy(mipi_sh_andy_set_val_GAMMAGREENposi,       mipi_sh_andy_cmd_GAMMAGREENposi,       sizeof(mipi_sh_andy_set_val_GAMMAGREENposi));
    memcpy(mipi_sh_andy_set_val_GAMMAGREENnega,       mipi_sh_andy_cmd_GAMMAGREENnega,       sizeof(mipi_sh_andy_set_val_GAMMAGREENnega));
    memcpy(mipi_sh_andy_set_val_GAMMABLUEposi,        mipi_sh_andy_cmd_GAMMABLUEposi,        sizeof(mipi_sh_andy_set_val_GAMMABLUEposi));
    memcpy(mipi_sh_andy_set_val_GAMMABLUEnega,        mipi_sh_andy_cmd_GAMMABLUEnega,        sizeof(mipi_sh_andy_set_val_GAMMABLUEnega));
    memcpy(mipi_sh_andy_set_val_RegulatorPumpSetting, mipi_sh_andy_cmd_RegulatorPumpSetting, sizeof(mipi_sh_andy_set_val_RegulatorPumpSetting));

    if(phy_gamma == NULL) {
        SHDISP_ERR("phy_gamma is NULL.\n");
        ret = -1;
    }
    else if(phy_gamma->status != SHDISP_LCDDR_GAMMA_STATUS_OK) {
        SHDISP_ERR("gammg status invalid. status=%02x\n", phy_gamma->status);
        ret = -1;
    }
    else {
        checksum = phy_gamma->status;
        for(cnt = 0; cnt < SHDISP_LCDDR_PHY_GAMMA_BUF_MAX; cnt++) {
            checksum = checksum + phy_gamma->buf[cnt];
        }
#if 1
        for(cnt = 0; cnt < SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE; cnt++) {
            checksum = checksum + phy_gamma->applied_voltage[cnt];
        }
#endif
        if((checksum & 0x00FFFFFF) != phy_gamma->chksum) {
            SHDISP_ERR("%s: gammg chksum NG. chksum=%06x calc_chksum=%06x\n", __func__, phy_gamma->chksum, (checksum & 0x00FFFFFF));
            ret = -1;
        }
        else {
            for(cnt = 0; cnt < SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET; cnt++) {
                idx = cnt * 2;
                mipi_sh_andy_set_val_GAMMAREDposi[idx][1]       = ((phy_gamma->buf[cnt] >> 8) & 0x0003);
                mipi_sh_andy_set_val_GAMMAREDposi[idx + 1][1]   = ( phy_gamma->buf[cnt] & 0x00FF);
                mipi_sh_andy_set_val_GAMMAREDnega[idx][1]       = ((phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET    ] >> 8) & 0x0003);
                mipi_sh_andy_set_val_GAMMAREDnega[idx + 1][1]   = ( phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET    ] & 0x00FF);
                mipi_sh_andy_set_val_GAMMAGREENposi[idx][1]     = ((phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET * 2] >> 8) & 0x0003);
                mipi_sh_andy_set_val_GAMMAGREENposi[idx + 1][1] = ( phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET * 2] & 0x00FF);
                mipi_sh_andy_set_val_GAMMAGREENnega[idx][1]     = ((phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET * 3] >> 8) & 0x0003);
                mipi_sh_andy_set_val_GAMMAGREENnega[idx + 1][1] = ( phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET * 3] & 0x00FF);
                mipi_sh_andy_set_val_GAMMABLUEposi[idx][1]      = ((phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET * 4] >> 8) & 0x0003);
                mipi_sh_andy_set_val_GAMMABLUEposi[idx + 1][1]  = ( phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET * 4] & 0x00FF);
                mipi_sh_andy_set_val_GAMMABLUEnega[idx][1]      = ((phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET * 5] >> 8) & 0x0003);
                mipi_sh_andy_set_val_GAMMABLUEnega[idx + 1][1]  = ( phy_gamma->buf[cnt + SHDISP_ANDY_GAMMA_NEGATIVE_OFFSET * 5] & 0x00FF);
            }
#if 1
            cnt = 0;
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_VGH][1]    = phy_gamma->applied_voltage[cnt++];
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_VGL][1]    = phy_gamma->applied_voltage[cnt++];
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_GVDDP][1]  = phy_gamma->applied_voltage[cnt++];
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_GVDDN][1]  = phy_gamma->applied_voltage[cnt++];
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_GVDDP2][1] = phy_gamma->applied_voltage[cnt++];
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_VGHO][1]   = phy_gamma->applied_voltage[cnt++];
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_VGLO][1]   = phy_gamma->applied_voltage[cnt++];
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_AVDDR][1]  = phy_gamma->applied_voltage[cnt++];
            mipi_sh_andy_set_val_RegulatorPumpSetting[SHDISP_ANDY_AVEER][1]  = phy_gamma->applied_voltage[cnt++];
#endif
        }
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
