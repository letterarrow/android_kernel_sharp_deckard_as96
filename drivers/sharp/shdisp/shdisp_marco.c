/* drivers/sharp/shdisp/shdisp_marco.c  (Display Driver)
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
#include "shdisp_marco.h"
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
#define SHDISP_MARCO_RESET      32
#define SHDISP_MARCO_DISABLE    0
#define SHDISP_MARCO_ENABLE     1

#define SHDISP_MARCO_1V_WAIT                 18000
#define SHDISP_MARCO_VCOM_MIN                0x0048
#define SHDISP_MARCO_VCOM_MAX                0x01C8
#define SHDISP_MARCO_ALPHA_DEFAULT           0x0115
//#define SHDISP_MARCO_CLOCK                   27000000

#define SHDISP_FW_STACK_EXCUTE

#define SHDISP_MARCO_GAMMA_SETTING_A                        0xC7
#define SHDISP_MARCO_GAMMA_SETTING_B                        0xC8
#define SHDISP_MARCO_GAMMA_SETTING_C                        0xC9

#define SHDISP_MARCO_POWER_SETTING                          0xD0
#define SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER       0xD3
#define SHDISP_MARCO_GAMMA_SETTING_SIZE                     24
#define SHDISP_MARCO_POWER_SETTING_SIZE                     14
#define SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE  26
#define SHDISP_MARCO_GAMMA_LEVEL_MIN                        1
#define SHDISP_MARCO_GAMMA_LEVEL_MAX                        12
#define SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET                  12
#define SHDISP_MARCO_VLM1       11
#define SHDISP_MARCO_VLM2       12
#define SHDISP_MARCO_VLM3       13
#define SHDISP_MARCO_VC1        12
#define SHDISP_MARCO_VC2        13
#define SHDISP_MARCO_VC3        15
#define SHDISP_MARCO_VPL        16
#define SHDISP_MARCO_VNL        22
#define SHDISP_MARCO_SVSS_SVDD  26
#define ST1       0x01
#define STA1      0x01
#define ST2       0x06
#define STA2      0x06
#define SW1       0x57
#define SWA1      0x57
#define SW2       0x4E
#define SWA2      0x4E
#define PSWT1     0x08
#define PSWTA1    0x08
#define PSWW1     0x13
#define PSWWA1    0x13
#define PSWG1     0x07
#define PSWGA1    0x07

#define ST1_B     0x01
#define STA1_B    0x01
#define ST2_B     0x06
#define STA2_B    0x06
#define SW1_B     0x5A
#define SWA1_B    0x5A
#define SW2_B     0x51
#define SWA2_B    0x51
#define PSWT1_B   0x08
#define PSWTA1_B  0x08
#define PSWW1_B   0x14
#define PSWWA1_B  0x14
#define PSWG1_B   0x07
#define PSWGA1_B  0x07

#define ST1_C     0x01
#define STA1_C    0x01
#define ST2_C     0x06
#define STA2_C    0x06
#define SW1_C     0x5A
#define SWA1_C    0x5A
#define SW2_C     0x51
#define SWA2_C    0x51
#define PSWT1_C   0x08
#define PSWTA1_C  0x08
#define PSWW1_C   0x14
#define PSWWA1_C  0x14
#define PSWG1_C   0x07
#define PSWGA1_C  0x07

#define ST3       0x00
#define SW3       0x00
#define ST4       0x00
#define SW4       0x00
#define ST5       0x00
#define SNT1      0x00
#define SNT2      0x00
#define SW6       0x00
#define ST7       0x00
#define SW7       0x00
#define ST8       0x00
#define SW8       0x00

#define SNTA1     0x03
#define SNTA2     0xD1

#define STA3      0x00
#define SWA3      0x00
#define STA4      0x00
#define SWA4      0x00
#define STA5      0x00
#define SWA5      0x00
#define STA6      0x00
#define SWA6      0x00
#define STA7      0x00
#define SWA7      0x00
#define STA8      0x00
#define SWA8      0x00

#define ST3_B     ST3
#define SW3_B     SW3
#define ST4_B     ST4
#define SW4_B     SW4
#define ST5_B     ST5
#define SNT1_B    SNT1
#define SNT2_B    SNT2
#define SW6_B     SW6
#define ST7_B     ST7
#define SW7_B     SW7
#define ST8_B     ST8
#define SW8_B     SW8

#define SNTA1_B   SNTA1
#define SNTA2_B   SNTA2

#define STA3_B    STA3
#define SWA3_B    SWA3
#define STA4_B    STA4
#define SWA4_B    SWA4
#define STA5_B    STA5
#define SWA5_B    SWA5
#define STA6_B    STA6
#define SWA6_B    SWA6
#define STA7_B    STA7
#define SWA7_B    SWA7
#define STA8_B    STA8
#define SWA8_B    SWA8

#define ST3_C     ST3
#define SW3_C     SW3
#define ST4_C     ST4
#define SW4_C     SW4
#define ST5_C     ST5
#define SNT1_C    SNT1
#define SNT2_C    SNT2
#define SW6_C     SW6
#define ST7_C     ST7
#define SW7_C     SW7
#define ST8_C     ST8
#define SW8_C     SW8

#define SNTA1_C   SNTA1
#define SNTA2_C   SNTA2

#define STA3_C    STA3
#define SWA3_C    SWA3
#define STA4_C    STA4
#define SWA4_C    SWA4
#define STA5_C    STA5
#define SWA5_C    SWA5
#define STA6_C    STA6
#define SWA6_C    SWA6
#define STA7_C    STA7
#define SWA7_C    SWA7
#define STA8_C    STA8
#define SWA8_C    SWA8

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

static int shdisp_marco_API_init_io(void);
static int shdisp_marco_API_empty_func(void);
static void shdisp_marco_API_set_param(struct shdisp_panel_param_str *param_str);
static int shdisp_marco_API_power_on(void);
static int shdisp_marco_API_power_off(void);
static int shdisp_marco_API_check_upper_unit(void);
static int shdisp_marco_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out);
static int shdisp_marco_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size);
static int shdisp_marco_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size);
static int shdisp_marco_API_diag_set_flicker_param(unsigned short alpha);
static int shdisp_marco_API_diag_get_flicker_param(unsigned short *alpha);
static int shdisp_marco_API_check_recovery(void);
static int shdisp_marco_API_recovery_type(int *type);
static int shdisp_marco_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma);
static int shdisp_marco_API_disp_update(struct shdisp_main_update *update);
static int shdisp_marco_API_disp_clear_screen(struct shdisp_main_clear *clear);
static int shdisp_marco_API_diag_get_flicker_low_param(unsigned short *alpha);
static int shdisp_marco_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
static int shdisp_marco_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
static int shdisp_marco_API_diag_set_gamma(struct shdisp_diag_gamma *gamma);
static int shdisp_marco_API_power_on_recovery(void);
static int shdisp_marco_API_power_off_recovery(void);
static int shdisp_marco_API_set_drive_freq(int type);
static int shdisp_marco_API_get_drive_freq(void);

static char shdisp_marco_mipi_manufacture_id(void);

int shdisp_marco_init_phy_gamma(struct shdisp_lcddr_phy_gamma_reg *phy_gamma);


/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static struct shdisp_panel_param_str marco_param_str;
#ifndef SHDISP_NOT_SUPPORT_FLICKER
static unsigned char marco_wdata[8];
static unsigned char marco_rdata[8];
#endif
static struct dsi_buf shdisp_mipi_marco_tx_buf;
static struct dsi_buf shdisp_mipi_marco_rx_buf;

static char mipi_sh_marco_set_val_gammmaSettingASet[25];
static char mipi_sh_marco_set_val_gammmaSettingBSet[25];
static char mipi_sh_marco_set_val_gammmaSettingCSet[25];
static char mipi_sh_marco_set_val_chargePumpSetting[15];
static char mipi_sh_marco_set_val_internalPower[27];

static int drive_freq_lasy_type = SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A;

/* ------------------------------------------------------------------------- */
/*      packet header                                                        */
/* ------------------------------------------------------------------------- */
/*      LCD ON                                                              */
/*      Initial Setting                                                     */
static char mipi_sh_marco_cmd_protect[2]                = {0xB0, 0x04};
static char mipi_sh_marco_cmd_initial_hs[2]             = {0x00, 0x00};
static char mipi_sh_marco_cmd_deviceCode[1]             = {0xBF};
static char mipi_sh_marco_cmd_interfaceSsetting[7]      = {0xB3,
    0x14, 0x00, 0x00, 0x00, 0x00, 0x00};
static char mipi_sh_marco_cmd_interfaceIdSsetting[3]    = {0xB4, 0x0C, 0x00};
static char mipi_sh_marco_cmd_dsi_Control[3]            = {0xB6, 0x3A, 0xD3};
static char mipi_sh_marco_cmd_externalClockSetting[3]   = {0xBB, 0x00, 0x03};
static char mipi_sh_marco_cmd_slewRateAadjustmen[3]     = {0xC0, 0x00, 0x00};

/*      Timing Setting                                                      */
static char mipi_sh_marco_cmd_displaySetting1common[35] = {0xC1,
    0x04, 0x60, 0x40, 0x40, 0x0C, 0x00, 0x50, 0x02,
    0x00, 0x00, 0x00, 0x5C, 0x63, 0xAC, 0x39, 0x00,
    0x00, 0x50, 0x00, 0x00, 0x00, 0x86, 0x00, 0x42,
    0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x02, 0x00};
static char mipi_sh_marco_cmd_displaySetting2[8]        = {0xC2,
    0x30, 0xF7, 0x80, 0x0C, 0x08, 0x00, 0x00};
static char mipi_sh_marco_cmd_tpcSyncControl[4]         = {0xC3,
    0x00, 0x00, 0x00};
static char mipi_sh_marco_cmd_sourceTimingSetting[23]   = {0xC4,
    0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x06};
static char mipi_sh_marco_cmd_ltpsTimingSetting[41]     = {0xC6,
    ST1,   SW1,   ST2,   SW2,   ST3,   SW3,    ST4,    SW4,
    ST5,   SNT1,  SNT2,  SW6,   ST7,   SW7,    ST8,    SW8,
    PSWT1, PSWW1, PSWG1, SNTA1, SNTA2, STA1,   SWA1,   STA2,
    SWA2,  STA3,  SWA3,  STA4,  SWA4,  STA5,   SWA5,   STA6,
    SWA6,  STA7,  SWA7,  STA8,  SWA8,  PSWTA1, PSWWA1, PSWGA1};

/*      Gamma Setting                                                       */
static char mipi_sh_marco_cmd_gammmaSettingASet[25]     = {0xC7,
    0x00, 0x05, 0x0A, 0x10, 0x1C, 0x30, 0x21, 0x31,
    0x3F, 0x4D, 0x5F, 0x75, 0x00, 0x05, 0x0A, 0x10,
    0x1C, 0x30, 0x21, 0x31, 0x3F, 0x4D, 0x5F, 0x75};
static char mipi_sh_marco_cmd_gammmaSettingBSet[25]     = {0xC8,
    0x00, 0x05, 0x0A, 0x10, 0x1C, 0x30, 0x21, 0x31,
    0x3F, 0x4D, 0x5F, 0x75, 0x00, 0x05, 0x0A, 0x10,
    0x1C, 0x30, 0x21, 0x31, 0x3F, 0x4D, 0x5F, 0x75};
static char mipi_sh_marco_cmd_gammmaSettingCSet[25]     = {0xC9,
    0x00, 0x05, 0x0A, 0x10, 0x1C, 0x30, 0x21, 0x31,
    0x3F, 0x4D, 0x5F, 0x75, 0x00, 0x05, 0x0A, 0x10,
    0x1C, 0x30, 0x21, 0x31, 0x3F, 0x4D, 0x5F, 0x75};

/*      Terminal control Setting                                            */
static char mipi_sh_marco_cmd_panalPinControl[10]       = {0xCB,
    0x66, 0xE0, 0x87, 0x61, 0x00, 0x00, 0x00, 0x00,
    0xC0};
static char mipi_sh_marco_cmd_panelInterfaceControl[2]  = {0xCC, 0x03};
static char mipi_sh_marco_cmd_gpoControl[6]             = {0xCF,
    0x00, 0x00, 0xC1, 0x05, 0x3F};

/*      Power Setting                                                       */
static char mipi_sh_marco_cmd_chargePumpSetting[15]     = {0xD0,
    0x00, 0x00, 0x19, 0x18, 0x99, 0x9C, 0x1C,
    0x01, 0x81, 0x00, 0xBB, 0x59, 0x4D, 0x01};
static char mipi_sh_marco_cmd_internalPower[27]         = {0xD3,
    0x1B, 0x33, 0xBB, 0xCC, 0xC4, 0x33, 0x33, 0x33,
    0x00, 0x01, 0x00, 0xA0, 0xD8, 0xA0, 0x02, 0x3F,
    0x33, 0x33, 0x22, 0x70, 0x02, 0x3F, 0x03, 0x3D,
    0xBF, 0x99};
static char mipi_sh_marco_cmd_vcomSetting[8]            = {0xD5,
    0x06, 0x00, 0x00, 0x01, 0x15, 0x01, 0x15};
static char mipi_sh_marco_cmd_sequencerControl[3]       = {0xD9, 0x20, 0x00};

/*      Synchronization signal Setting                                      */
static char mipi_sh_marco_cmd_panel_output1[3]          = {0xEC, 0x40, 0x50};
static char mipi_sh_marco_cmd_panel_output2[4]          = {0xED,
    0x00, 0x00, 0x00};
static char mipi_sh_marco_cmd_panel_output3[3]          = {0xEE, 0x00, 0x32};
static char mipi_sh_marco_cmd_panel_output4[13]         = {0xEF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};
static char mipi_sh_marco_cmd_panel_output5[26]         = {0xB8,
    0x18, 0x80, 0x18, 0x18, 0xCF, 0x1F, 0x00, 0x0C,
    0x12, 0x6C, 0x11, 0x6C, 0x12, 0x0C, 0x12, 0xDA,
    0x6D, 0xFF, 0xFF, 0x10, 0x67, 0xA3, 0xDB, 0xFB,
    0xFF};
static char mipi_sh_marco_cmd_panel_output6[4]          = {0xD2,
    0x5C, 0x00, 0x00};
static char mipi_sh_marco_cmd_panel_output7[2]          = {0xD6, 0x01};

static char mipi_sh_marco_cmd_panel_output8[21]         = {0xD7,
    0x84, 0xE0, 0x7F, 0xA8, 0xCE, 0x38, 0xFC, 0xC1,
    0x83, 0xE7, 0x83, 0x1F, 0x3C, 0x10, 0xFA, 0xC3,
    0x0F, 0x04, 0x41, 0x20};

/*      Preparation display ON                                              */
static char mipi_sh_marco_cmd_setDisplayOn[2]           = {0x29, 0x00};
static char mipi_sh_marco_cmd_exitSleepMode[2]          = {0x11, 0x00};


/*      LCD OFF                                                             */
/*      Common setting                                                      */
static char mipi_sh_marco_cmd_setDisplayOff[2]          = {0x28, 0x00};
static char mipi_sh_marco_cmd_enterSleepMode[2]         = {0x10, 0x00};
static char mipi_sh_marco_cmd_deepStanby[2]             = {0xB1, 0x01};

/* Test Image Generator ON */
static char mipi_sh_marco_cmd_TestImageGeneratorOn[7]   = {0xDE,
     0x01, 0xff, 0x02, 0x00, 0x00, 0xFF};

/* Test Image Generator OFF */
static char mipi_sh_marco_cmd_TestImageGeneratorOff[7]  = {0xDE,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


/*      Initial Setting 1                                                   */
static struct dsi_cmd_desc mipi_sh_marco_cmds_initial1[] = {
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_protect),
        mipi_sh_marco_cmd_protect}
#if 0
    ,
    {DTYPE_DCS_WRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_wait),
        mipi_sh_marco_cmd_wait},
    {DTYPE_DCS_WRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_wait),
        mipi_sh_marco_cmd_wait}
#endif
};

static struct dsi_cmd_desc mipi_sh_marco_cmds_initial_hs[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_initial_hs),
        mipi_sh_marco_cmd_initial_hs},
};

/*      Device Code Read                                                    */
static struct dsi_cmd_desc mipi_sh_marco_cmds_deviceCode[] = {
    {DTYPE_GEN_READ1, 1, 0, 1, 0, 1, mipi_sh_marco_cmd_deviceCode}
};

/*      Initial Setting 2                                                   */
static struct dsi_cmd_desc mipi_sh_marco_cmds_initial2[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_interfaceSsetting),
        mipi_sh_marco_cmd_interfaceSsetting},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_interfaceIdSsetting),
        mipi_sh_marco_cmd_interfaceIdSsetting},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_dsi_Control),
        mipi_sh_marco_cmd_dsi_Control},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_externalClockSetting),
        mipi_sh_marco_cmd_externalClockSetting},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_slewRateAadjustmen),
        mipi_sh_marco_cmd_slewRateAadjustmen}
};

/*      Timing Setting                                                      */
static struct dsi_cmd_desc mipi_sh_marco_cmds_timing[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_displaySetting1common),
        mipi_sh_marco_cmd_displaySetting1common},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_displaySetting2),
        mipi_sh_marco_cmd_displaySetting2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_tpcSyncControl),
        mipi_sh_marco_cmd_tpcSyncControl},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_sourceTimingSetting),
        mipi_sh_marco_cmd_sourceTimingSetting},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_ltpsTimingSetting),
        mipi_sh_marco_cmd_ltpsTimingSetting}
};

/*      Gamma Setting                                                       */
static struct dsi_cmd_desc mipi_sh_marco_cmds_gamma[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_set_val_gammmaSettingASet),
        mipi_sh_marco_set_val_gammmaSettingASet},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_set_val_gammmaSettingBSet),
        mipi_sh_marco_set_val_gammmaSettingBSet},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_set_val_gammmaSettingCSet),
        mipi_sh_marco_set_val_gammmaSettingCSet}
};

/*      Terminal control Setting                                            */
static struct dsi_cmd_desc mipi_sh_marco_cmds_terminal[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panalPinControl),
        mipi_sh_marco_cmd_panalPinControl},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panelInterfaceControl),
        mipi_sh_marco_cmd_panelInterfaceControl},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_gpoControl),
        mipi_sh_marco_cmd_gpoControl}
};

/*      Power Setting                                                       */
static struct dsi_cmd_desc mipi_sh_marco_cmds_power[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_set_val_chargePumpSetting),
        mipi_sh_marco_set_val_chargePumpSetting},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_set_val_internalPower),
        mipi_sh_marco_set_val_internalPower},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_vcomSetting),
        mipi_sh_marco_cmd_vcomSetting},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_vcomSetting),
        mipi_sh_marco_cmd_vcomSetting},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_sequencerControl),
        mipi_sh_marco_cmd_sequencerControl}
};

/*      Synchronization signal Setting                                      */
static struct dsi_cmd_desc mipi_sh_marco_cmds_sync[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panel_output1),
        mipi_sh_marco_cmd_panel_output1},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panel_output2),
        mipi_sh_marco_cmd_panel_output2},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panel_output3),
        mipi_sh_marco_cmd_panel_output3},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panel_output4),
        mipi_sh_marco_cmd_panel_output4},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panel_output5),
        mipi_sh_marco_cmd_panel_output5},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panel_output6),
        mipi_sh_marco_cmd_panel_output6},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panel_output7),
        mipi_sh_marco_cmd_panel_output7},
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_panel_output8),
        mipi_sh_marco_cmd_panel_output8}
};

/*      Preparation display ON                                              */
static struct dsi_cmd_desc mipi_sh_marco_cmds_display_on[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_setDisplayOn),
        mipi_sh_marco_cmd_setDisplayOn},
};
static struct dsi_cmd_desc mipi_sh_marco_cmds_display_on_start[] = {
    {DTYPE_DCS_WRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_exitSleepMode),
        mipi_sh_marco_cmd_exitSleepMode}
};


static struct dsi_cmd_desc mipi_sh_marco_cmds_display_off[] = {
    {DTYPE_DCS_WRITE1, 1, 0, 0, (WAIT_1FRAME_US*1),
        sizeof(mipi_sh_marco_cmd_setDisplayOff),
        mipi_sh_marco_cmd_setDisplayOff},
    {DTYPE_DCS_WRITE1, 1, 0, 0, (WAIT_1FRAME_US*4),
        sizeof(mipi_sh_marco_cmd_enterSleepMode),
        mipi_sh_marco_cmd_enterSleepMode},
    {DTYPE_GEN_WRITE2, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_deepStanby),
        mipi_sh_marco_cmd_deepStanby}
};

/* Test Image Generator ON */
static struct dsi_cmd_desc mipi_sh_marco_cmds_TIG_on[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_TestImageGeneratorOn),
        mipi_sh_marco_cmd_TestImageGeneratorOn}
};

/* Test Image Generator OFF */
static struct dsi_cmd_desc mipi_sh_marco_cmds_TIG_off[] = {
    {DTYPE_GEN_LWRITE, 1, 0, 0, 0,
        sizeof(mipi_sh_marco_cmd_TestImageGeneratorOff),
        mipi_sh_marco_cmd_TestImageGeneratorOff}
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
    ST1,
    SW1,
    ST2,
    SW2,
    ST3,
    SW3,
    ST4,
    SW4,
    ST5,
    SNT1,
    SNT2,
    SW6,
    ST7,
    SW7,
    ST8,
    SW8,
    PSWT1,
    PSWW1,
    PSWG1,
    SNTA1,
    SNTA2,
    STA1,
    SWA1,
    STA2,
    SWA2,
    STA3,
    SWA3,
    STA4,
    SWA4,
    STA5,
    SWA5,
    STA6,
    SWA6,
    STA7,
    SWA7,
    STA8,
    SWA8,
    PSWTA1,
    PSWWA1,
    PSWGA1,
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
    ST1_B,
    SW1_B,
    ST2_B,
    SW2_B,
    ST3_B,
    SW3_B,
    ST4_B,
    SW4_B,
    ST5_B,
    SNT1_B,
    SNT2_B,
    SW6_B,
    ST7_B,
    SW7_B,
    ST8_B,
    SW8_B,
    PSWT1_B,
    PSWW1_B,
    PSWG1_B,
    SNTA1_B,
    SNTA2_B,
    STA1_B,
    SWA1_B,
    STA2_B,
    SWA2_B,
    STA3_B,
    SWA3_B,
    STA4_B,
    SWA4_B,
    STA5_B,
    SWA5_B,
    STA6_B,
    SWA6_B,
    STA7_B,
    SWA7_B,
    STA8_B,
    SWA8_B,
    PSWTA1_B,
    PSWWA1_B,
    PSWGA1_B,
};

const static unsigned char freq_drive_c[] = {
    (CALI_PLL1CTL_VAL_C & 0x000000FF) >>  0,
    (CALI_PLL1CTL_VAL_C & 0x0000FF00) >>  8,
    (CALI_PLL1CTL_VAL_C & 0x00FF0000) >> 16,
    (CALI_PLL1CTL_VAL_C & 0xFF000000) >> 24,
    (CALI_PLL1CTL2_VAL_C & 0x000000FF) >>  0,
    (CALI_PLL1CTL2_VAL_C & 0x0000FF00) >>  8,
    (CALI_PLL1CTL2_VAL_C & 0x00FF0000) >> 16,
    (CALI_PLL1CTL2_VAL_C & 0xFF000000) >> 24,
    (CALI_PLL1CTL3_VAL_C & 0x000000FF) >>  0,
    (CALI_PLL1CTL3_VAL_C & 0x0000FF00) >>  8,
    (CALI_PLL1CTL3_VAL_C & 0x00FF0000) >> 16,
    (CALI_PLL1CTL3_VAL_C & 0xFF000000) >> 24,
    (CALI_GPDIV_VAL_C & 0x00FF) >> 0,
    (CALI_GPDIV_VAL_C & 0xFF00) >> 8,
    (CALI_PTGHF_VAL_C & 0x00FF) >> 0,
    (CALI_PTGHF_VAL_C & 0xFF00) >> 8,
    (CALI_PTGHP_VAL_C & 0x00FF) >> 0,
    (CALI_PTGHP_VAL_C & 0xFF00) >> 8,
    (CALI_PTGHB_VAL_C & 0x00FF) >> 0,
    (CALI_PTGHB_VAL_C & 0xFF00) >> 8,
    (CALI_PTGVF_VAL_C & 0x00FF) >> 0,
    (CALI_PTGVF_VAL_C & 0xFF00) >> 8,
    (CALI_PTGVP_VAL_C & 0x00FF) >> 0,
    (CALI_PTGVP_VAL_C & 0xFF00) >> 8,
    (CALI_PTGVB_VAL_C & 0x00FF) >> 0,
    (CALI_PTGVB_VAL_C & 0xFF00) >> 8,
    ST1_C,
    SW1_C,
    ST2_C,
    SW2_C,
    ST3_C,
    SW3_C,
    ST4_C,
    SW4_C,
    ST5_C,
    SNT1_C,
    SNT2_C,
    SW6_C,
    ST7_C,
    SW7_C,
    ST8_C,
    SW8_C,
    PSWT1_C,
    PSWW1_C,
    PSWG1_C,
    SNTA1_C,
    SNTA2_C,
    STA1_C,
    SWA1_C,
    STA2_C,
    SWA2_C,
    STA3_C,
    SWA3_C,
    STA4_C,
    SWA4_C,
    STA5_C,
    SWA5_C,
    STA6_C,
    SWA6_C,
    STA7_C,
    SWA7_C,
    STA8_C,
    SWA8_C,
    PSWTA1_C,
    PSWWA1_C,
    PSWGA1_C,
};

static struct shdisp_panel_operations shdisp_marco_fops = {
    shdisp_marco_API_init_io,
    shdisp_marco_API_empty_func,
    shdisp_marco_API_empty_func,
    shdisp_marco_API_set_param,
    shdisp_marco_API_power_on,
    shdisp_marco_API_power_off,
    shdisp_marco_API_empty_func,
    shdisp_marco_API_empty_func,
    shdisp_marco_API_empty_func,
    shdisp_marco_API_empty_func,
    shdisp_marco_API_empty_func,
    shdisp_marco_API_check_upper_unit,
    shdisp_marco_API_check_flicker_param,
    shdisp_marco_API_diag_write_reg,
    shdisp_marco_API_diag_read_reg,
    shdisp_marco_API_diag_set_flicker_param,
    shdisp_marco_API_diag_get_flicker_param,
    shdisp_marco_API_check_recovery,
    shdisp_marco_API_recovery_type,
    shdisp_marco_API_set_abl_lut,
    shdisp_marco_API_disp_update,
    shdisp_marco_API_disp_clear_screen,
    shdisp_marco_API_diag_get_flicker_low_param,
    shdisp_marco_API_diag_set_gamma_info,
    shdisp_marco_API_diag_get_gamma_info,
    shdisp_marco_API_diag_set_gamma,
    shdisp_marco_API_power_on_recovery,
    shdisp_marco_API_power_off_recovery,
    shdisp_marco_API_set_drive_freq,
    NULL,
    NULL,
    NULL,
    shdisp_marco_API_get_drive_freq,
};

/* ------------------------------------------------------------------------- */
/* DEBUG MACRAOS                                                             */
/* ------------------------------------------------------------------------- */
#define SHDISP_MARCO_SW_CHK_UPPER_UNIT

/* ------------------------------------------------------------------------- */
/* MACRAOS                                                                   */
/* ------------------------------------------------------------------------- */
#define MIPI_DSI_COMMAND_TX_CLMR(x)         (shdisp_panel_API_mipi_dsi_cmds_tx(&shdisp_mipi_marco_tx_buf, x, ARRAY_SIZE(x)))
#define MIPI_DSI_COMMAND_TX_CLMR_DUMMY(x)   (shdisp_panel_API_mipi_dsi_cmds_tx(&shdisp_mipi_marco_tx_buf, x, ARRAY_SIZE(x)))
//#define MIPI_DSI_COMMAND_TX_CLMR_DUMMY(x)   (shdisp_panel_API_mipi_dsi_cmds_tx_dummy(x))


/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_create                                                    */
/* ------------------------------------------------------------------------- */
struct shdisp_panel_operations *shdisp_marco_API_create(void)
{
    SHDISP_DEBUG("\n");
    return &shdisp_marco_fops;
}


static int shdisp_marco_mipi_dsi_buf_alloc(struct dsi_buf *dp, int size)
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
/* shdisp_marco_API_init_io                                                   */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_init_io(void)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    unsigned short tmp_alpha;
#endif
    int ret = 0;
    struct shdisp_lcddr_phy_gamma_reg* phy_gamma;

    SHDISP_DEBUG("in\n");
    shdisp_marco_mipi_dsi_buf_alloc(&shdisp_mipi_marco_tx_buf, DSI_BUF_SIZE);
    shdisp_marco_mipi_dsi_buf_alloc(&shdisp_mipi_marco_rx_buf, DSI_BUF_SIZE);

#ifndef SHDISP_NOT_SUPPORT_FLICKER
    tmp_alpha = shdisp_api_get_alpha();
    SHDISP_DEBUG("alpha=0x%04x\n", tmp_alpha);
    mipi_sh_marco_cmd_vcomSetting[4] = ((tmp_alpha >> 8) & 0x01);
    mipi_sh_marco_cmd_vcomSetting[5] = (tmp_alpha & 0xFF);
    mipi_sh_marco_cmd_vcomSetting[6] = ((tmp_alpha >> 8) & 0x01);
    mipi_sh_marco_cmd_vcomSetting[7] = (tmp_alpha & 0xFF);
    SHDISP_DEBUG("bit[8]:VCOMDC=0x%02x bit[7:0]:VCOMDC=0x%02x\n",
        mipi_sh_marco_cmd_vcomSetting[4], mipi_sh_marco_cmd_vcomSetting[5]);
#endif

    phy_gamma = shdisp_api_get_lcddr_phy_gamma();
    ret = shdisp_marco_init_phy_gamma(phy_gamma);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> shdisp_marco_init_phy_gamma.\n");
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_empty_func                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_empty_func(void)
{
    SHDISP_DEBUG("\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_set_param                                                 */
/* ------------------------------------------------------------------------- */
static void shdisp_marco_API_set_param(struct shdisp_panel_param_str *param_str)
{
    SHDISP_DEBUG("in\n");
    marco_param_str.vcom_alpha = param_str->vcom_alpha;
    marco_param_str.vcom_alpha_low = param_str->vcom_alpha_low;
    SHDISP_DEBUG("out\n");

    return;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_power_on                                                  */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_power_on(void)
{
    SHDISP_DEBUG("in\n");
    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);

    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);
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
    shdisp_SYS_cmd_delay_us(1000);

    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_ON);
    shdisp_SYS_cmd_delay_us(3000);
    shdisp_bdic_API_LCD_release_hw_reset();
    shdisp_SYS_cmd_delay_us(1000);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif


    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_power_off                                                 */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_power_off(void)
{
    SHDISP_DEBUG("in\n");
#if 0
    if (shdisp_api_get_clock_info() == 0) {
        shdisp_SYS_Host_control(SHDISP_HOST_CTL_CMD_LCD_CLK_STOP, SHDISP_MARCO_CLOCK);
    }
#endif

    shdisp_clmr_api_display_stop();

    SHDISP_PERFORMANCE("SUSPEND LCDC GPCLK_OFF 0010 START\n");

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_OFF);

    shdisp_bdic_API_LCD_m_power_off();
    shdisp_bdic_API_LCD_power_off();
    shdisp_bdic_API_LCD_set_hw_reset();
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    SHDISP_PERFORMANCE("SUSPEND LCDC GPCLK_OFF 0010 END\n");

    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_OFF);

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_check_upper_unit                                          */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_check_upper_unit(void)
{
#ifndef SHDISP_MARCO_SW_CHK_UPPER_UNIT
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
#endif /* SHDISP_MARCO_SW_CHK_UPPER_UNIT */
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_check_flicker_param                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    unsigned short tmp_alpha = alpha_in;

    SHDISP_DEBUG("in\n");
    if (alpha_out == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha_out.\n");
        return SHDISP_RESULT_FAILURE;
    }

    if ((tmp_alpha & 0xF000) != 0x9000) {
        *alpha_out = SHDISP_MARCO_ALPHA_DEFAULT;
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_SUCCESS;
    }

    tmp_alpha = tmp_alpha & 0x01FF;
    if ((tmp_alpha < SHDISP_MARCO_VCOM_MIN) || (tmp_alpha > SHDISP_MARCO_VCOM_MAX)) {
        *alpha_out = SHDISP_MARCO_ALPHA_DEFAULT;
        SHDISP_DEBUG("out2\n");
        return SHDISP_RESULT_SUCCESS;
    }

    *alpha_out = tmp_alpha;

    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_diag_write_reg                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf, addr, write_data, size);
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
/* shdisp_marco_API_diag_read_reg                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size)
{
    int ret = 0;

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf, &shdisp_mipi_marco_rx_buf, addr, read_data, size);
    if(ret) {
        SHDISP_DEBUG("out1\n");
        return SHDISP_RESULT_FAILURE;
    }
    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_diag_set_flicker_param                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_diag_set_flicker_param(unsigned short alpha)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int i;
    int ret = 0;

    SHDISP_DEBUG("in\n");
    if ((alpha < SHDISP_MARCO_VCOM_MIN) || (alpha > SHDISP_MARCO_VCOM_MAX)) {
        SHDISP_ERR("<INVALID_VALUE> alpha(0x%04X).\n", alpha);
        return SHDISP_RESULT_FAILURE;
    }

    for (i=1; i<=7; i++) {
        marco_rdata[i] = 0;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf, &shdisp_mipi_marco_rx_buf,0xD5, marco_rdata, 7);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    marco_wdata[0] = marco_rdata[0];
    marco_wdata[1] = marco_rdata[1];
    marco_wdata[2] = marco_rdata[2];
    marco_wdata[3] = (unsigned char) ((alpha >> 8) & 0x01);
    marco_wdata[4] = (unsigned char) (alpha & 0xFF);
    marco_wdata[5] = (unsigned char) ((alpha >> 8) & 0x01);
    marco_wdata[6] = (unsigned char) (alpha & 0xFF);

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf, 0xD5, marco_wdata, 7);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    marco_param_str.vcom_alpha = alpha;
    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_diag_get_flicker_param                                    */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_diag_get_flicker_param(unsigned short *alpha)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    int i;
    int ret = 0;

    SHDISP_DEBUG("in\n");
    if (alpha == NULL){
        SHDISP_ERR("<NULL_POINTER> alpha.\n");
        return SHDISP_RESULT_FAILURE;
    }

    for (i=1; i<=7; i++) {
        marco_rdata[i] = 0;
    }
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf, &shdisp_mipi_marco_rx_buf, 0xD5, marco_rdata, 7);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    *alpha = ((marco_rdata[3] & 0x01) << 8) | marco_rdata[4];
    SHDISP_DEBUG("out\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_diag_get_flicker_low_param                               */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_diag_get_flicker_low_param(unsigned short *alpha)
{
#ifndef SHDISP_NOT_SUPPORT_FLICKER
    SHDISP_DEBUG("\n");
#endif
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_check_recovery                                            */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_check_recovery(void)
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
/* shdisp_marco_API_recovery_type                                             */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_recovery_type(int *type)
{
    SHDISP_DEBUG("in\n");
    *type = SHDISP_SUBSCRIBE_TYPE_INT;

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_set_abl_lut                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_set_abl_lut(struct dma_abl_lut_gamma *abl_lut_gammma)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_disp_update                                               */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_disp_update(struct shdisp_main_update *update)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_disp_clear_screen                                         */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_disp_clear_screen(struct shdisp_main_clear *clear)
{
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_diag_set_gamma_info                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    int ret = 0;
    unsigned char marco_rdata[SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE];
    unsigned char marco_wdata[SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE];

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);

    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf,
                                               SHDISP_MARCO_GAMMA_SETTING_A,
                                               gamma_info->gammaR,
                                               SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf,
                                               SHDISP_MARCO_GAMMA_SETTING_B,
                                               gamma_info->gammaG,
                                               SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf,
                                               SHDISP_MARCO_GAMMA_SETTING_C,
                                               gamma_info->gammaB,
                                               SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    memset(marco_wdata, 0, SHDISP_MARCO_POWER_SETTING_SIZE);
    memset(marco_rdata, 0, SHDISP_MARCO_POWER_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_POWER_SETTING,
                                              marco_rdata,
                                              SHDISP_MARCO_POWER_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    memcpy(marco_wdata, marco_rdata, SHDISP_MARCO_POWER_SETTING_SIZE);
    marco_wdata[SHDISP_MARCO_VLM1 - 1] = gamma_info->vlm1;
    marco_wdata[SHDISP_MARCO_VLM2 - 1] = gamma_info->vlm2;
    marco_wdata[SHDISP_MARCO_VLM3 - 1] = gamma_info->vlm3;

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf,
                                               SHDISP_MARCO_POWER_SETTING,
                                               marco_wdata,
                                               SHDISP_MARCO_POWER_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    memset(marco_wdata, 0, SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE);
    memset(marco_rdata, 0, SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER,
                                              marco_rdata,
                                              SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    memcpy(marco_wdata, marco_rdata, SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE);
    marco_wdata[SHDISP_MARCO_VC1 - 1]       = gamma_info->vc1;
    marco_wdata[SHDISP_MARCO_VC2 - 1]       = gamma_info->vc2;
    marco_wdata[SHDISP_MARCO_VC3 - 1]       = gamma_info->vc3;
    marco_wdata[SHDISP_MARCO_VPL - 1]       = gamma_info->vpl;
    marco_wdata[SHDISP_MARCO_VNL - 1]       = gamma_info->vnl;
    marco_wdata[SHDISP_MARCO_SVSS_SVDD - 1] = gamma_info->svss_svdd;

    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf,
                                               SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER,
                                               marco_wdata,
                                               SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
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
/* shdisp_marco_API_diag_get_gamma_info                                      */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info)
{
    int ret = 0;
    unsigned char marco_rdata[SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE];

    SHDISP_DEBUG("in\n");
    if (gamma_info == NULL){
        SHDISP_ERR("<NULL_POINTER> gamma_info.\n");
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    memset(marco_rdata, 0, SHDISP_MARCO_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_GAMMA_SETTING_A,
                                              marco_rdata,
                                              SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    memcpy(gamma_info->gammaR, marco_rdata, SHDISP_MARCO_GAMMA_SETTING_SIZE);

    memset(marco_rdata, 0, SHDISP_MARCO_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_GAMMA_SETTING_B,
                                              marco_rdata,
                                              SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    memcpy(gamma_info->gammaG, marco_rdata, SHDISP_MARCO_GAMMA_SETTING_SIZE);

    memset(marco_rdata, 0, SHDISP_MARCO_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_GAMMA_SETTING_C,
                                              marco_rdata,
                                              SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    memcpy(gamma_info->gammaB, marco_rdata, SHDISP_MARCO_GAMMA_SETTING_SIZE);

    memset(marco_rdata, 0, SHDISP_MARCO_POWER_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_POWER_SETTING,
                                              marco_rdata,
                                              SHDISP_MARCO_POWER_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    gamma_info->vlm1 = marco_rdata[SHDISP_MARCO_VLM1 - 1];
    gamma_info->vlm2 = marco_rdata[SHDISP_MARCO_VLM2 - 1];
    gamma_info->vlm3 = marco_rdata[SHDISP_MARCO_VLM3 - 1];

    memset(marco_rdata, 0, SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER,
                                              marco_rdata,
                                              SHDISP_MARCO_POWER_SETTING_FOR_INTERNAL_POWER_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    gamma_info->vc1       = marco_rdata[SHDISP_MARCO_VC1 - 1];
    gamma_info->vc2       = marco_rdata[SHDISP_MARCO_VC2 - 1];
    gamma_info->vc3       = marco_rdata[SHDISP_MARCO_VC3 - 1];
    gamma_info->vpl       = marco_rdata[SHDISP_MARCO_VPL - 1];
    gamma_info->vnl       = marco_rdata[SHDISP_MARCO_VNL - 1];
    gamma_info->svss_svdd = marco_rdata[SHDISP_MARCO_SVSS_SVDD - 1];

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_diag_set_gamma                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_diag_set_gamma(struct shdisp_diag_gamma *gamma)
{
    int ret = 0;
    unsigned char marco_rwdata[SHDISP_MARCO_GAMMA_SETTING_SIZE];

    SHDISP_DEBUG("in\n");
    if ((gamma->level < SHDISP_MARCO_GAMMA_LEVEL_MIN) || (gamma->level > SHDISP_MARCO_GAMMA_LEVEL_MAX)) {
        SHDISP_ERR("<INVALID_VALUE> gamma->level(%d).\n", gamma->level);
        return SHDISP_RESULT_FAILURE;
    }

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    memset(marco_rwdata, 0, SHDISP_MARCO_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_GAMMA_SETTING_A,
                                              marco_rwdata,
                                              SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    marco_rwdata[gamma->level - 1]                                      = gamma->gammaR_p;
    marco_rwdata[gamma->level + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET - 1] = gamma->gammaR_n;
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf,
                                               SHDISP_MARCO_GAMMA_SETTING_A,
                                               marco_rwdata,
                                               SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    memset(marco_rwdata, 0, SHDISP_MARCO_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_GAMMA_SETTING_B,
                                              marco_rwdata,
                                              SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    marco_rwdata[gamma->level - 1]                                      = gamma->gammaG_p;
    marco_rwdata[gamma->level + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET - 1] = gamma->gammaG_n;
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf,
                                               SHDISP_MARCO_GAMMA_SETTING_B,
                                               marco_rwdata,
                                               SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    memset(marco_rwdata, 0, SHDISP_MARCO_GAMMA_SETTING_SIZE);
    ret = shdisp_panel_API_mipi_diag_read_reg(&shdisp_mipi_marco_tx_buf,
                                              &shdisp_mipi_marco_rx_buf,
                                              SHDISP_MARCO_GAMMA_SETTING_C,
                                              marco_rwdata,
                                              SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_read_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }
    marco_rwdata[gamma->level - 1]                                      = gamma->gammaB_p;
    marco_rwdata[gamma->level + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET - 1] = gamma->gammaB_n;
    ret = shdisp_panel_API_mipi_diag_write_reg(&shdisp_mipi_marco_tx_buf,
                                               SHDISP_MARCO_GAMMA_SETTING_C,
                                               marco_rwdata,
                                               SHDISP_MARCO_GAMMA_SETTING_SIZE);
    if(ret) {
        SHDISP_ERR("<RESULT_FAILURE> mipi_sharp_diag_write_reg.\n");
        return SHDISP_RESULT_FAILURE;
    }

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_set_drive_freq                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_sqe_set_drive_freq(int type)
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
    case SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_C:
        shdisp_FWCMD_buf_add(SHDISP_CLMR_FWCMD_HOST_DSI_TX_FREQ_SET, sizeof(freq_drive_c), (unsigned char*)freq_drive_c);
        drive_freq_lasy_type = type;
        break;
    default:
        SHDISP_ERR("type error.\n");
    }

    SHDISP_DEBUG("done.\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_set_drive_freq                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_set_drive_freq(int type)
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
    
    shdisp_marco_sqe_set_drive_freq(type);
    
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);

    SHDISP_DEBUG("done.\n");
    return ret;
}

/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_get_drive_freq                                           */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_get_drive_freq(void)
{
    return drive_freq_lasy_type;
}

static char shdisp_marco_mipi_manufacture_id(void)
{
    struct dsi_buf *rp, *tp;
    struct dsi_cmd_desc *cmd;
    char *rdata;

    tp = &shdisp_mipi_marco_tx_buf;
    rp = &shdisp_mipi_marco_rx_buf;

    cmd = mipi_sh_marco_cmds_deviceCode;
    shdisp_panel_API_mipi_dsi_cmds_rx(tp, rp, cmd, 5+8);

    rdata = rp->data;

    SHDISP_DEBUG("addr:BFh data: %02X %02X %02X %02X %02X\n", *(rdata+8), *(rdata+9), *(rdata+10), *(rdata+11), *(rdata+12));
    SHDISP_DEBUG("Header: %02X %02X %02X %02X %02X %02X %02X %02X\n", *(rdata+0), *(rdata+1), *(rdata+2), *(rdata+3), *(rdata+4), *(rdata+5), *(rdata+6), *(rdata+7));

#if 0
    {
        /* Device Code Read */
        static char mipi_sh_marco_cmd_b0  = 0xb0;    /* Read */
        static struct dsi_cmd_desc mipi_sh_marco_cmds_b0[] = {
            {DTYPE_GEN_READ1, 1, 0, 1, 0,
                1,
                &mipi_sh_marco_cmd_b0}
        };
        cmd = mipi_sh_marco_cmds_b0;
        shdisp_panel_API_mipi_dsi_cmds_rx(tp, rp, cmd, 1+8);
        rdata = rp->data;
        SHDISP_DEBUG("addr:B0h data: %02X\n", *(rdata+8));
        SHDISP_DEBUG("Header: %02X %02X %02X %02X %02X %02X %02X %02X\n", *(rdata+0), *(rdata+1), *(rdata+2), *(rdata+3), *(rdata+4), *(rdata+5), *(rdata+6), *(rdata+7));

    }
#endif
    return *(rdata+8);
}

static int shdisp_marco_mipi_cmd_start_display(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    /* Preparation display ON */
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_display_on);
    if (ret != SHDISP_RESULT_SUCCESS) {
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_display_on_start);
    shdisp_SYS_cmd_delay_us(WAIT_1FRAME_US*4);
    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}

static int shdisp_marco_mipi_cmd_lcd_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_display_off);
    SHDISP_DEBUG("out\n");
    return ret;
}

/* Test Image Generator ON */
int shdisp_panel_marco_API_TestImageGen(int onoff)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    if (onoff) {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_TIG_on);
    }
    else {
        ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_TIG_off);
    }
    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}

static int shdisp_marco_mipi_cmd_lcd_on(void)
{
    char device_id = 0x00;
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");

    shdisp_panel_API_mipi_set_transfer_mode(SHDISP_DSI_HIGH_SPEED_MODE);
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_initial1);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out1 ret=%d\n", ret);
        return ret;
    }

    MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_initial_hs);
    MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_initial_hs);

    device_id = shdisp_marco_mipi_manufacture_id();
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_initial2);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out2 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_timing);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out3 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_gamma);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out4 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_terminal);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out5 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_power);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out6 ret=%d\n", ret);
        return ret;
    }
    ret = MIPI_DSI_COMMAND_TX_CLMR(mipi_sh_marco_cmds_sync);


    SHDISP_DEBUG("out ret=%d\n", ret);
    return ret;
}

int shdisp_marco_API_mipi_lcd_on(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    ret = shdisp_marco_mipi_cmd_lcd_on();

#ifdef SHDISP_FW_STACK_EXCUTE
    ret = shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
    if (ret != SHDISP_RESULT_SUCCESS){
        SHDISP_DEBUG("out dokick err ret=%d\n", ret);
        return ret;
    }
#endif

    shdisp_clmr_api_custom_blk_init();

    SHDISP_DEBUG("out ret=%d\n", ret);

    return ret;
}

int shdisp_marco_API_mipi_lcd_off(void)
{
    int ret = SHDISP_RESULT_SUCCESS;

    SHDISP_DEBUG("in\n");
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    ret = shdisp_marco_mipi_cmd_lcd_off();

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

int shdisp_marco_API_mipi_start_display(void)
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
    shdisp_panel_API_request_RateCtrl(1, SHDISP_PANEL_RATE_60_0, SHDISP_PANEL_RATE_1);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif

    ret = shdisp_marco_mipi_cmd_start_display();

    if (drive_freq_lasy_type != SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A) {
        shdisp_marco_sqe_set_drive_freq(drive_freq_lasy_type);
    }

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

int shdisp_marco_API_cabc_init(void)
{
    return 0;
}

int shdisp_marco_API_cabc_indoor_on(void)
{
    return 0;
}

int shdisp_marco_API_cabc_outdoor_on(int lut_level)
{
    return 0;
}

int shdisp_marco_API_cabc_off(int wait_on, int pwm_disable)
{
    return 0;
}

int shdisp_marco_API_cabc_outdoor_move(int lut_level)
{
    return 0;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_power_on_recovery                                        */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_power_on_recovery(void)
{
    SHDISP_DEBUG("in\n");

    (void)shdisp_pm_clmr_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);
    (void)shdisp_pm_bdic_power_manager(SHDISP_DEV_TYPE_LCD, SHDISP_DEV_STATE_ON);

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
    shdisp_SYS_cmd_delay_us(1000);

    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_ON);
    shdisp_SYS_cmd_delay_us(10000);
    shdisp_bdic_API_LCD_release_hw_reset();
    shdisp_SYS_cmd_delay_us(1000);
#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_safe_finishanddoKick();
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_NOTHING);
#endif

    SHDISP_DEBUG("out\n");
    return SHDISP_RESULT_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/* shdisp_marco_API_power_off_recovery                                       */
/* ------------------------------------------------------------------------- */
static int shdisp_marco_API_power_off_recovery(void)
{
    SHDISP_DEBUG("in\n");
#if 0
    if (shdisp_api_get_clock_info() == 0) {
        shdisp_SYS_Host_control(SHDISP_HOST_CTL_CMD_LCD_CLK_STOP, SHDISP_MARCO_CLOCK);
    }
#endif

#ifdef SHDISP_FW_STACK_EXCUTE
    shdisp_FWCMD_set_apino(SHDISP_CLMR_FWCMD_APINO_LCD);
#endif
    shdisp_clmr_api_gpclk_ctrl(SHDISP_CLMR_GPCLK_OFF);

    shdisp_bdic_API_LCD_m_power_off();
    shdisp_SYS_cmd_delay_us(10000);
    shdisp_bdic_API_LCD_power_off();
    shdisp_SYS_cmd_delay_us(10000);
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


int shdisp_marco_init_phy_gamma(struct shdisp_lcddr_phy_gamma_reg *phy_gamma)
{
    int ret = 0;
    int cnt;
    int checksum;

    SHDISP_DEBUG("in\n");

    memcpy(mipi_sh_marco_set_val_gammmaSettingASet, mipi_sh_marco_cmd_gammmaSettingASet, sizeof(mipi_sh_marco_set_val_gammmaSettingASet));
    memcpy(mipi_sh_marco_set_val_gammmaSettingBSet, mipi_sh_marco_cmd_gammmaSettingBSet, sizeof(mipi_sh_marco_set_val_gammmaSettingBSet));
    memcpy(mipi_sh_marco_set_val_gammmaSettingCSet, mipi_sh_marco_cmd_gammmaSettingCSet, sizeof(mipi_sh_marco_set_val_gammmaSettingCSet));
    memcpy(mipi_sh_marco_set_val_chargePumpSetting, mipi_sh_marco_cmd_chargePumpSetting, sizeof(mipi_sh_marco_set_val_chargePumpSetting));
    memcpy(mipi_sh_marco_set_val_internalPower,     mipi_sh_marco_cmd_internalPower,     sizeof(mipi_sh_marco_set_val_internalPower));

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
        for(cnt = 0; cnt < SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE; cnt++) {
            checksum = checksum + phy_gamma->applied_voltage[cnt];
        }
        if((checksum & 0xFFFF) != phy_gamma->chksum) {
            pr_err("%s: gammg chksum NG. chksum=%04x calc_chksum=%04x\n", __func__, phy_gamma->chksum, (checksum & 0xFFFF));
            ret = -1;
        }
        else {
            for(cnt = 0; cnt < SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET; cnt++) {
                mipi_sh_marco_set_val_gammmaSettingASet[cnt + 1]                                      = phy_gamma->buf[cnt];
                mipi_sh_marco_set_val_gammmaSettingASet[cnt + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET + 1] = phy_gamma->buf[cnt + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET];
                mipi_sh_marco_set_val_gammmaSettingBSet[cnt + 1]                                      = phy_gamma->buf[cnt + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET * 2];
                mipi_sh_marco_set_val_gammmaSettingBSet[cnt + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET + 1] = phy_gamma->buf[cnt + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET * 3];
                mipi_sh_marco_set_val_gammmaSettingCSet[cnt + 1]                                      = phy_gamma->buf[cnt + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET * 4];
                mipi_sh_marco_set_val_gammmaSettingCSet[cnt + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET + 1] = phy_gamma->buf[cnt + SHDISP_MARCO_GAMMA_NEGATIVE_OFFSET * 5];
            }
            cnt = 0;
            mipi_sh_marco_set_val_chargePumpSetting[SHDISP_MARCO_VLM1]  = phy_gamma->applied_voltage[cnt++];
            mipi_sh_marco_set_val_chargePumpSetting[SHDISP_MARCO_VLM2]  = phy_gamma->applied_voltage[cnt++];
            mipi_sh_marco_set_val_chargePumpSetting[SHDISP_MARCO_VLM3]  = phy_gamma->applied_voltage[cnt++];
            mipi_sh_marco_set_val_internalPower[SHDISP_MARCO_VC1]       = phy_gamma->applied_voltage[cnt++];
            mipi_sh_marco_set_val_internalPower[SHDISP_MARCO_VC2]       = phy_gamma->applied_voltage[cnt++];
            mipi_sh_marco_set_val_internalPower[SHDISP_MARCO_VC3]       = phy_gamma->applied_voltage[cnt++];
            mipi_sh_marco_set_val_internalPower[SHDISP_MARCO_VPL]       = phy_gamma->applied_voltage[cnt++];
            mipi_sh_marco_set_val_internalPower[SHDISP_MARCO_VNL]       = phy_gamma->applied_voltage[cnt++];
            mipi_sh_marco_set_val_internalPower[SHDISP_MARCO_SVSS_SVDD] = phy_gamma->applied_voltage[cnt++];
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
