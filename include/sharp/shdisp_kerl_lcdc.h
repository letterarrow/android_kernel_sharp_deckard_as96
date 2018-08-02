/* include/sharp/shdisp_kerl_lcdc.h  (Display Driver)
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

#ifndef SHDISP_KERN_CLMR_H
#define SHDISP_KERN_CLMR_H

/* ------------------------------------------------------------------------- */
/* NOT SUPPORT FUNCTION                                                      */
/* ------------------------------------------------------------------------- */

#define SHDISP_NOT_SUPPORT_PANEL

#define SHDISP_NOT_SUPPORT_CABC

#if defined(CONFIG_MACH_LYNX_DL32) || defined(CONFIG_MACH_TDN) || defined(CONFIG_MACH_DECKARD_AS96)
//#define SHDISP_NOT_SUPPORT_DET
#else
#define SHDISP_NOT_SUPPORT_DET
#endif /* defined(CONFIG_MACH_LYNX_DL32) || defined(CONFIG_MACH_TDN) || defined(CONFIG_MACH_DECKARD_AS96) */

#if defined(CONFIG_MACH_TDN) || defined(CONFIG_MACH_LYNX_DL32) || defined(CONFIG_MACH_DECKARD_AS96) || defined(CONFIG_SHDISP_PANEL_GEMINI)
#else
#define SHDISP_NOT_SUPPORT_NO_OS
#endif

#if defined(CONFIG_SHDISP_PANEL_RYOMA) && defined(CONFIG_USES_SHLCDC)
#define SHDISP_USE_EXT_CLOCK
#endif

#if defined(CONFIG_SHDISP_PANEL_ANDY)
#else
#define SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
#endif

//#define SHDISP_NOT_SUPPORT_PLLONCTL_REG

#if defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_RYOMA)
#define CONFIG_SHDISP_PANEL_IGZO
#endif /* defined(CONFIG_SHDISP_PANEL_ANDY) || defined(CONFIG_SHDISP_PANEL_RYOMA) */

#define SHDISP_NOT_SUPPORT_EWB_2SURFACE

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* IOCTL                                                                     */
/* ------------------------------------------------------------------------- */
#define SHDISP_IOC_MAGIC 's'
#define SHDISP_IOCTL_LCDC_WRITE_REG                     _IOW  (SHDISP_IOC_MAGIC, 100, struct shdisp_diag_lcdc_reg)
#define SHDISP_IOCTL_LCDC_READ_REG                      _IOR  (SHDISP_IOC_MAGIC, 101, struct shdisp_diag_lcdc_reg)
#define SHDISP_IOCTL_LCDC_I2C_WRITE                     _IOW  (SHDISP_IOC_MAGIC, 102, struct shdisp_diag_lcdc_i2c)
#define SHDISP_IOCTL_LCDC_I2C_READ                      _IOR  (SHDISP_IOC_MAGIC, 103, struct shdisp_diag_lcdc_i2c)
#define SHDISP_IOCTL_LCDC_POW_CTL                       _IOW  (SHDISP_IOC_MAGIC, 104, int)
#define SHDISP_IOCTL_BDIC_POW_CTL                       _IOW  (SHDISP_IOC_MAGIC, 105, int)
#define SHDISP_IOCTL_SET_GAMMA_INFO                     _IOW  (SHDISP_IOC_MAGIC, 106, struct shdisp_diag_gamma_info)
#define SHDISP_IOCTL_GET_GAMMA_INFO                     _IOWR (SHDISP_IOC_MAGIC, 107, struct shdisp_diag_gamma_info)
#define SHDISP_IOCTL_SET_GAMMA                          _IOW  (SHDISP_IOC_MAGIC, 108, struct shdisp_diag_gamma)
#define SHDISP_IOCTL_SET_EWB_TBL                        _IOW  (SHDISP_IOC_MAGIC, 109, struct shdisp_diag_ewb_tbl)
#define SHDISP_IOCTL_SET_EWB                            _IOW  (SHDISP_IOC_MAGIC, 110, struct shdisp_diag_set_ewb)
#define SHDISP_IOCTL_READ_EWB                           _IOWR (SHDISP_IOC_MAGIC, 111, struct shdisp_diag_read_ewb)
#define SHDISP_IOCTL_SET_EWB_TBL2                       _IOW  (SHDISP_IOC_MAGIC, 112, struct shdisp_diag_ewb_tbl)
#define SHDISP_IOCTL_LCDC_SET_TRV_PARAM                 _IOW  (SHDISP_IOC_MAGIC, 113, struct shdisp_trv_param)
#define SHDISP_IOCTL_LCDC_SET_DBC_PARAM                 _IOW  (SHDISP_IOC_MAGIC, 114, struct shdisp_main_dbc)
#define SHDISP_IOCTL_LCDC_SET_FLICKER_TRV               _IOW  (SHDISP_IOC_MAGIC, 115, struct shdisp_flicker_trv)
#define SHDISP_IOCTL_LCDC_FW_CMD_WRITE                  _IOW  (SHDISP_IOC_MAGIC, 116, struct shdisp_diag_fw_cmd)
#define SHDISP_IOCTL_LCDC_FW_CMD_READ                   _IOR  (SHDISP_IOC_MAGIC, 117, struct shdisp_diag_fw_cmd)
#define SHDISP_IOCTL_LCDC_SET_PIC_ADJ_PARAM             _IOW  (SHDISP_IOC_MAGIC, 118, struct shdisp_main_pic_adj)
#define SHDISP_IOCTL_LCDC_SET_AE_PARAM                  _IOW  (SHDISP_IOC_MAGIC, 119, struct shdisp_main_ae)
#define SHDISP_IOCTL_LCDC_SET_PIC_ADJ_AP_TYPE           _IOW  (SHDISP_IOC_MAGIC, 120, unsigned short)
#define SHDISP_IOCTL_LCDC_SET_DRIVE_FREQ                _IOW  (SHDISP_IOC_MAGIC, 121, struct shdisp_main_drive_freq)
#define SHDISP_IOCTL_SET_FLICKER_PARAM_MULTI_COG        _IOW  (SHDISP_IOC_MAGIC, 122, struct shdisp_diag_flicker_param)
#define SHDISP_IOCTL_GET_FLICKER_PARAM_MULTI_COG        _IOWR (SHDISP_IOC_MAGIC, 123, struct shdisp_diag_flicker_param)
#define SHDISP_IOCTL_GET_FLICKER_LOW_PARAM_MULTI_COG    _IOWR (SHDISP_IOC_MAGIC, 124, struct shdisp_diag_flicker_param)

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define SHDISP_LCDC_EWB_TBL_SIZE                256

#if defined(CONFIG_SHDISP_PANEL_ANDY)
    #define SHDISP_PANEL_GAMMA_TBL_SIZE             60
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    #define SHDISP_PANEL_ANALOG_GAMMA_TBL_SIZE      24
    #define SHDISP_PANEL_GAMMA_TBL_SIZE             18
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_RYOMA)
    #define SHDISP_PANEL_GAMMA_TBL_SIZE             24
#else
    #define SHDISP_PANEL_GAMMA_TBL_SIZE             24
#endif

#define SHDISP_LCDDR_GAMMA_STATUS_OK            0x96
#define SHDISP_LCDDR_GAMMA_STATUS_OK_2          0x97

enum {
    SHDISP_MAIN_DISP_PIC_ADJ_MODE_00,
    SHDISP_MAIN_DISP_PIC_ADJ_MODE_01,
    SHDISP_MAIN_DISP_PIC_ADJ_MODE_02,
    SHDISP_MAIN_DISP_PIC_ADJ_MODE_03,
    SHDISP_MAIN_DISP_PIC_ADJ_MODE_04,
    SHDISP_MAIN_DISP_PIC_ADJ_MODE_05,
    SHDISP_MAIN_DISP_PIC_ADJ_MODE_06,
    NUM_SHDISP_MAIN_DISP_PIC_ADJ_MODE
};

enum {
    SHDISP_LCDC_TRV_REQ_STOP,
    SHDISP_LCDC_TRV_REQ_START,
    SHDISP_LCDC_TRV_REQ_SET_PARAM,
    NUM_SHDISP_LCDC_TRV_REQ
};
enum {
    SHDISP_LCDC_TRV_STRENGTH_00,
    SHDISP_LCDC_TRV_STRENGTH_01,
    SHDISP_LCDC_TRV_STRENGTH_02,
    NUM_SHDISP_LCDC_TRV_STRENGTH
};
enum {
    SHDISP_LCDC_TRV_ADJUST_00,
    SHDISP_LCDC_TRV_ADJUST_01,
    SHDISP_LCDC_TRV_ADJUST_02,
    SHDISP_LCDC_TRV_ADJUST_03,
    SHDISP_LCDC_TRV_ADJUST_04,
    SHDISP_LCDC_TRV_ADJUST_05,
    SHDISP_LCDC_TRV_ADJUST_06,
    SHDISP_LCDC_TRV_ADJUST_07,
    SHDISP_LCDC_TRV_ADJUST_08,
    SHDISP_LCDC_TRV_ADJUST_09,
    SHDISP_LCDC_TRV_ADJUST_10,
    SHDISP_LCDC_TRV_ADJUST_11,
    SHDISP_LCDC_TRV_ADJUST_12,
    NUM_SHDISP_LCDC_TRV_ADJUST
};
#define SHDISP_LCDC_TRV_DATA_HEADER_SIZE        16
#define SHDISP_LCDC_TRV_DATA_HEADER_LENGTH_SIZE 4
#define SHDISP_LCDC_TRV_DATA_HEADER_H_SIZE      2
#define SHDISP_LCDC_TRV_DATA_HEADER_Y_SIZE      2

enum {
    SHDISP_LCDC_FLICKER_TRV_OFF,
    SHDISP_LCDC_FLICKER_TRV_ON,
    NUM_SHDISP_LCDC_FLICKER_TRV_REQUEST
};
enum {
    SHDISP_LCDC_FLICKER_TRV_COLUMN,
    SHDISP_LCDC_FLICKER_TRV_DOT1H,
    SHDISP_LCDC_FLICKER_TRV_DOT2H,
    NUM_SHDISP_LCDC_FLICKER_TRV_TYPE,
};

enum {
    SHDISP_MAIN_DISP_DBC_MODE_OFF,
    SHDISP_MAIN_DISP_DBC_MODE_DBC,
    NUM_SHDISP_MAIN_DISP_DBC_MODE
};
enum {
    SHDISP_MAIN_DISP_DBC_AUTO_MODE_OFF,
    SHDISP_MAIN_DISP_DBC_AUTO_MODE_ON,
    NUM_SHDISP_MAIN_DISP_DBC_AUTO_MODE
};

enum {
    SHDISP_MAIN_DISP_AE_TIME_DAYTIME,
    SHDISP_MAIN_DISP_AE_TIME_MIDNIGHT,
    SHDISP_MAIN_DISP_AE_TIME_MORNING,
    NUM_SHDISP_MAIN_DISP_AE_TIME
};

enum {
    SHDISP_LCDC_PIC_ADJ_AP_NORMAL,
    SHDISP_LCDC_PIC_ADJ_AP_1SEG,
    SHDISP_LCDC_PIC_ADJ_AP_FULLSEG,
    SHDISP_LCDC_PIC_ADJ_AP_TMM,
    SHDISP_LCDC_PIC_ADJ_AP_CAM,
    NUM_SHDISP_LCDC_PIC_ADJ_AP
};

enum {
    SHDISP_MAIN_DISP_DRIVE_FREQ_DEFAULT,
    SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_A,
    SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_B,
    SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE_C,
    NUM_SHDISP_MAIN_DISP_DRIVE_FREQ_TYPE
};
/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

enum {
    SHDISP_CLMR_IS_NOT_EXIST,
    SHDISP_CLMR_IS_EXIST,
    NUM_CLMR_EXIST_STATUS
};

enum {
    SHDISP_DIAG_COG_ID_NONE,
    SHDISP_DIAG_COG_ID_MASTER,
    SHDISP_DIAG_COG_ID_SLAVE,
    SHDISP_DIAG_COG_ID_BOTH,
    NUM_SHDISP_DIAG_COG_ID
};


struct shdisp_clmr_status {
    int clmr_is_exist;
    int power_status;
    unsigned long users;
};

struct shdisp_bdic_status {
    int bdic_is_exist;
    int power_status;
    unsigned long users;
};

struct shdisp_diag_fw_cmd {
    unsigned short  cmd;
    unsigned short  write_count;
    unsigned char   write_val[512];
    unsigned short  read_count;
    unsigned char   read_val[8];
};

struct shdisp_clmr_ewb {
    unsigned char  valR[SHDISP_LCDC_EWB_TBL_SIZE];
    unsigned char  valG[SHDISP_LCDC_EWB_TBL_SIZE];
    unsigned char  valB[SHDISP_LCDC_EWB_TBL_SIZE];
    unsigned short red_chksum;
    unsigned short green_chksum;
    unsigned short blue_chksum;
    unsigned short ewb_lut_status;
};

struct shdisp_clmr_ewb_accu {
    unsigned short valR[SHDISP_LCDC_EWB_TBL_SIZE];
    unsigned short valG[SHDISP_LCDC_EWB_TBL_SIZE];
    unsigned short valB[SHDISP_LCDC_EWB_TBL_SIZE];
};

#if defined(CONFIG_SHDISP_PANEL_ANDY)
    #define SHDISP_LCDDR_PHY_GAMMA_BUF_MAX          (SHDISP_PANEL_GAMMA_TBL_SIZE * 3)
    #define SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE       9
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    #define SHDISP_LCDDR_PHY_ANALOG_GAMMA_BUF_MAX   SHDISP_PANEL_ANALOG_GAMMA_TBL_SIZE
    #define SHDISP_LCDDR_PHY_GAMMA_BUF_MAX          (SHDISP_PANEL_GAMMA_TBL_SIZE * 3)
    #define SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE       5
    struct shdisp_phy_gamma_sub {
        unsigned char analog_gamma[SHDISP_LCDDR_PHY_ANALOG_GAMMA_BUF_MAX];
        unsigned char gamma[SHDISP_LCDDR_PHY_GAMMA_BUF_MAX];
        unsigned char applied_voltage[SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE];
    };
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_RYOMA)
    #define SHDISP_LCDDR_PHY_GAMMA_BUF_MAX          (SHDISP_PANEL_GAMMA_TBL_SIZE * 3)
    #define SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE       9
#else
    #define SHDISP_LCDDR_PHY_GAMMA_BUF_MAX          (SHDISP_PANEL_GAMMA_TBL_SIZE * 3)
    #define SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE       9
#endif

struct shdisp_lcddr_phy_gamma_reg {
#if defined(CONFIG_SHDISP_PANEL_ANDY)
    unsigned char  status;
    unsigned short buf[SHDISP_LCDDR_PHY_GAMMA_BUF_MAX];
    unsigned char  applied_voltage[SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE];
    unsigned int   chksum;
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    unsigned char  status;
    struct shdisp_phy_gamma_sub master;
    struct shdisp_phy_gamma_sub slave;
    unsigned short chksum;
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_RYOMA)
    unsigned char  status;
    unsigned char  buf[SHDISP_LCDDR_PHY_GAMMA_BUF_MAX];
    unsigned char  applied_voltage[SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE];
    unsigned short chksum;
#else
    unsigned char  status;
    unsigned char  buf[SHDISP_LCDDR_PHY_GAMMA_BUF_MAX];
    unsigned char  applied_voltage[SHDISP_LCDDR_APPLIED_VOLTAGE_SIZE];
    unsigned short chksum;
#endif
};

struct shdisp_als_adjust {
    unsigned short als_adj0;
    unsigned short als_adj1;
    unsigned char als_shift;
    unsigned char clear_offset;
    unsigned char ir_offset;
};

struct shdisp_photo_sensor_adj {
    unsigned char status;
    unsigned char key_backlight;
    unsigned long chksum;
    struct shdisp_als_adjust als_adjust[2];
};

struct shdisp_boot_context {
    int driver_is_initialized;
    unsigned short hw_handset;
    unsigned short hw_revision;
    unsigned char device_code;
    int handset_color;
    int upper_unit_is_connected;
    int main_disp_status;
    struct shdisp_main_bkl_ctl main_bkl;
    struct shdisp_tri_led tri_led;
    unsigned short alpha;
    unsigned short alpha_low;
    unsigned short alpha_nvram;
    struct shdisp_photo_sensor_adj photo_sensor_adj;
    struct shdisp_lcddr_phy_gamma_reg lcddr_phy_gamma;
    struct shdisp_ledc_status ledc_status;
    struct shdisp_clmr_status clmr_status;
    struct shdisp_bdic_status bdic_status;
    struct shdisp_clmr_ewb clmr_ewb[2];
    unsigned short slave_alpha;
    unsigned short slave_alpha_low;
    unsigned char pll_on_ctl_count;
};

struct shdisp_kernel_context {
    int driver_is_initialized;
    unsigned short hw_revision;
    int handset_color;
    int upper_unit_is_connected;
    int bdic_is_exist;
    int main_disp_status;
    struct shdisp_main_bkl_ctl main_bkl;
    struct shdisp_tri_led tri_led;
    unsigned short alpha;
    unsigned short alpha_low;
    struct shdisp_photo_sensor_adj photo_sensor_adj;
    unsigned short dma_lut_status;
    struct dma_argc_lut_gamma argc_lut_gamma;
    struct dma_abl_lut_gamma abl_lut_gamma;
    struct shdisp_lcddr_phy_gamma_reg lcddr_phy_gamma;
    struct shdisp_lcddr_phy_gamma_reg lcddr_rom_gamma;
    int dtv_status;
    int thermal_status;
    int eco_bkl_status;
    int usb_chg_status;
    struct shdisp_ledc_status ledc_status;
    unsigned int shdisp_lcd;
    unsigned char dbgTraceF;
    int bdic_reset_port;
};

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
struct shdisp_diag_gamma_info_sub {
    unsigned char   analog_gamma[SHDISP_PANEL_ANALOG_GAMMA_TBL_SIZE];
    unsigned char   gammaR[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   gammaG[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   gammaB[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   vsps_vsns;
    unsigned char   vghs;
    unsigned char   vgls;
    unsigned char   vph_vpl;
    unsigned char   vnh_vnl;
};
#endif

struct shdisp_diag_gamma_info {
#if defined(CONFIG_SHDISP_PANEL_ANDY)
    unsigned short  gammaR[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned short  gammaG[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned short  gammaB[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   vgh;
    unsigned char   vgl;
    unsigned char   gvddp;
    unsigned char   gvddn;
    unsigned char   gvddp2;
    unsigned char   vgho;
    unsigned char   vglo;
    unsigned char   avddr;
    unsigned char   aveer;
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    struct shdisp_diag_gamma_info_sub   master_info;
    struct shdisp_diag_gamma_info_sub   slave_info;
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_RYOMA)
    unsigned char   gammaR[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   gammaG[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   gammaB[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   vlm1;
    unsigned char   vlm2;
    unsigned char   vlm3;
    unsigned char   vc1;
    unsigned char   vc2;
    unsigned char   vc3;
    unsigned char   vpl;
    unsigned char   vnl;
    unsigned char   svss_svdd;
#else
    unsigned char   gammaR[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   gammaG[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   gammaB[SHDISP_PANEL_GAMMA_TBL_SIZE];
    unsigned char   vlm1;
    unsigned char   vlm2;
    unsigned char   vlm3;
    unsigned char   vc1;
    unsigned char   vc2;
    unsigned char   vc3;
    unsigned char   vpl;
    unsigned char   vnl;
    unsigned char   svss_svdd;
#endif
};

struct shdisp_diag_gamma {
#if defined(CONFIG_SHDISP_PANEL_ANDY)
    unsigned char   level;
    unsigned short  gammaR_p;
    unsigned short  gammaR_n;
    unsigned short  gammaG_p;
    unsigned short  gammaG_n;
    unsigned short  gammaB_p;
    unsigned short  gammaB_n;
#elif defined(CONFIG_SHDISP_PANEL_GEMINI)
    unsigned char   level;
    unsigned char   master_gamma_p;
    unsigned char   master_gamma_n;
    unsigned char   slave_gamma_p;
    unsigned char   slave_gamma_n;
#elif defined(CONFIG_SHDISP_PANEL_MARCO) || defined(CONFIG_SHDISP_PANEL_RYOMA)
    unsigned char   level;
    unsigned char   gammaR_p;
    unsigned char   gammaR_n;
    unsigned char   gammaG_p;
    unsigned char   gammaG_n;
    unsigned char   gammaB_p;
    unsigned char   gammaB_n;
#else
    unsigned char   level;
    unsigned char   gammaR_p;
    unsigned char   gammaR_n;
    unsigned char   gammaG_p;
    unsigned char   gammaG_n;
    unsigned char   gammaB_p;
    unsigned char   gammaB_n;
#endif
};

#if defined(CONFIG_SHDISP_PANEL_GEMINI)
    #define SHDISP_TRV_DATA_MAX    (51200 * 2)
#else
    #define SHDISP_TRV_DATA_MAX    51200
#endif
struct shdisp_clmr_trv_info {
    int            status;
    int            strength;
    int            adjust;
    unsigned int   data_size;
    unsigned short hw;
    unsigned short y_size;
    unsigned char  data[SHDISP_TRV_DATA_MAX];
};
struct shdisp_trv_data_header {
    unsigned int   data_size;
    unsigned short hw;
    unsigned short y_size;
    unsigned int   reserve[2];
};

#define SHDISP_LCDDR_BUF_MAX    64
struct shdisp_lcddr_reg {
    unsigned char address;
    unsigned char size;
    unsigned char buf[SHDISP_LCDDR_BUF_MAX];
    int      cog;
};

struct shdisp_diag_lcdc_reg {
    unsigned short reg;
    unsigned long  value;
};

struct shdisp_diag_lcdc_i2c {
    unsigned char   slave_addr;
    unsigned char   buf[256];
    unsigned short  size;
    unsigned long   timeout;
};

struct shdisp_diag_ewb_tbl {
    unsigned char  valR[SHDISP_LCDC_EWB_TBL_SIZE];
    unsigned char  valG[SHDISP_LCDC_EWB_TBL_SIZE];
    unsigned char  valB[SHDISP_LCDC_EWB_TBL_SIZE];
};

struct shdisp_diag_set_ewb {
    unsigned short  valR;
    unsigned short  valG;
    unsigned short  valB;
};

struct shdisp_diag_read_ewb {
    unsigned char   level;
    unsigned short  valR;
    unsigned short  valG;
    unsigned short  valB;
};

struct shdisp_main_pic_adj {
    int mode;
};

struct shdisp_trv_param {
    int            request;
    int            strength;
    int            adjust;
    unsigned char  *data;
};


struct shdisp_diag_flicker_param {
    unsigned short  master_alpha;
    unsigned short  slave_alpha;
};

struct shdisp_main_ae {
    unsigned char time;
};

struct shdisp_main_common_info {
    unsigned short mode;
};

struct shdisp_flicker_trv {
    int request;
    unsigned char level;
    unsigned char type;
};

struct shdisp_main_drive_freq {
    int freq;
};

struct shdisp_panel_operations {
    int (*init_io) (void);
    int (*exit_io) (void);
    int (*init_isr) (void);
    void (*set_param) (struct shdisp_panel_param_str *param_str);
    int (*power_on) (void);
    int (*power_off) (void);
    int (*init_1st) (void);
    int (*init_2nd) (void);
    int (*disp_on) (void);
    int (*sleep) (void);
    int (*deep_standby) (void);
    int (*check_upper_unit) (void);
    int (*check_flicker) (unsigned short alpha_in, unsigned short *alpha_out);
    int (*write_reg) (int cog, unsigned char addr, unsigned char *write_data, unsigned char size);
    int (*read_reg) (int cog, unsigned char addr, unsigned char *read_data, unsigned char size);
    int (*set_flicker) (unsigned short alpha);
    int (*get_flicker) (unsigned short *alpha);
    int (*check_recovery) (void);
    int (*recovery_type) (int *type);
    int (*set_abl_lut) (struct dma_abl_lut_gamma *abl_lut_gamma);
    int (*disp_update) (struct shdisp_main_update *update);
    int (*clear_screen) (struct shdisp_main_clear *clear);
    int (*get_flicker_low) (unsigned short *alpha);
    int (*set_gamma_info) (struct shdisp_diag_gamma_info *gamma_info);
    int (*get_gamma_info) (struct shdisp_diag_gamma_info *gamma_info);
    int (*set_gamma) (struct shdisp_diag_gamma *gamma);
    int (*power_on_recovery) (void);
    int (*power_off_recovery) (void);
    int (*set_drive_freq) (int type);
    int (*set_flicker_multi_cog) (struct shdisp_diag_flicker_param *flicker_param);
    int (*get_flicker_multi_cog) (struct shdisp_diag_flicker_param *flicker_param);
    int (*get_flicker_low_multi_cog) (struct shdisp_diag_flicker_param *flicker_param);
    int (*get_drive_freq) (void);
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
int shdisp_ioctl_lcdc_write_reg(void __user *argp);
int shdisp_ioctl_lcdc_read_reg(void __user *argp);
int shdisp_ioctl_lcdc_devchk(void);
int shdisp_ioctl_lcdc_i2c_write(void __user *argp);
int shdisp_ioctl_lcdc_i2c_read(void __user *argp);

int shdisp_ioctl_lcdc_set_ewb_tbl(void __user *argp);
int shdisp_ioctl_lcdc_set_ewb(void __user *argp);
int shdisp_ioctl_lcdc_read_ewb(void __user *argp);
int shdisp_ioctl_lcdc_set_ewb_tbl2(void __user *argp);
int shdisp_ioctl_lcdc_set_pic_adj_param(void __user *argp);
int shdisp_ioctl_lcdc_set_trv_param(void __user *argp);
int shdisp_ioctl_lcdc_set_dbc_param(void __user *argp);
int shdisp_ioctl_lcdc_set_ae_param(void __user *argp);
int shdisp_ioctl_lcdc_set_pic_adj_ap_type(void __user *argp);
int shdisp_ioctl_lcdc_set_flicker_trv(void __user *argp);
int shdisp_ioctl_lcdc_fw_cmd_write(void __user *argp);
int shdisp_ioctl_lcdc_fw_cmd_read(void __user *argp);
int shdisp_ioctl_lcdc_set_drive_freq(void __user *argp);


struct shdisp_clmr_ewb_accu* shdisp_api_get_clmr_ewb_accu(unsigned char no);

int shdisp_api_main_mipi_cmd_lcd_probe(void);
int shdisp_api_main_mipi_cmd_lcd_on(void);
int shdisp_api_main_mipi_cmd_lcd_off(void);
int shdisp_api_main_mipi_cmd_lcd_start_display(void);
int shdisp_api_check_det(void);
void shdisp_api_requestrecovery(void);
int shdisp_api_main_mipi_cmd_lcd_stop_prepare(void);
int shdisp_api_main_mipi_cmd_lcd_off_black_screen_on(void);
int shdisp_api_main_mipi_cmd_lcd_on_after_black_screen(void);
int shdisp_api_main_mipi_cmd_is_fw_timeout(void);

int shdisp_api_do_lcdc_mipi_dsi_det_recovery(void);

unsigned short shdisp_api_get_hw_handset(void);
unsigned char shdisp_api_get_device_code(void);
int shdisp_api_set_device_code(unsigned char device_code);
unsigned short shdisp_api_get_alpha_nvram(void);
unsigned short shdisp_api_get_slave_alpha(void);
unsigned short shdisp_api_get_slave_alpha_low(void);
int shdisp_api_do_recovery_set_bklmode_luxmode(void);

unsigned char shdisp_api_get_pll_on_ctl_count(void);
int shdisp_api_set_pll_on_ctl_count(unsigned char pll_on_ctl_count);
void shdisp_api_set_pll_on_ctl_reg(int ctrl);

#endif /* SHDISP_KERN_CLMR_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
