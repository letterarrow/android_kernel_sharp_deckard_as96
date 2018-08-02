/* include/sharp/shdisp_kerl_no_lcdc.h  (Display Driver)
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

#ifndef SHDISP_KERN_NO_LCDC_H
#define SHDISP_KERN_NO_LCDC_H

/* ------------------------------------------------------------------------- */
/* NOT SUPPORT FUNCTION                                                      */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* IOCTL                                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */




#define SHDISP_LCDDR_BUF_MAX    64
struct shdisp_lcddr_reg {
    unsigned char address;
    unsigned char size;
    unsigned char buf[SHDISP_LCDDR_BUF_MAX];
};

#define SHDISP_LCDDR_PHY_GAMMA_BUF_MAX      72

struct shdisp_lcddr_phy_gamma_reg {
    unsigned char status;
    unsigned char buf[SHDISP_LCDDR_PHY_GAMMA_BUF_MAX];
    short chksum;
};

struct shdisp_photo_sensor_adj {
    unsigned char status;
    unsigned short als_adj0;
    unsigned short als_adj1;
    unsigned char als_shift;
    unsigned char clear_offset;
    unsigned char ir_offset;
    unsigned char adc_lclip;
    unsigned char key_backlight;
    unsigned long chksum;
};


struct shdisp_boot_context {
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
    struct shdisp_ledc_status ledc_status;
    unsigned int shdisp_lcd;
    int bdic_reset_port;
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
    int (*write_reg) (unsigned char addr, unsigned char *write_data, unsigned char size);
    int (*read_reg) (unsigned char addr, unsigned char *read_data, unsigned char size);
    int (*set_flicker) (unsigned short alpha);
    int (*get_flicker) (unsigned short *alpha);
    int (*check_recovery) (void);
    int (*recovery_type) (int *type);
    int (*set_abl_lut) (struct dma_abl_lut_gamma *abl_lut_gammma);
    int (*disp_update) (struct shdisp_main_update *update);
    int (*clear_screen) (struct shdisp_main_clear *clear);
    int (*get_flicker_low) (unsigned short *alpha);
};


/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
int shdisp_api_main_disp_ext_clk_on(void);
int shdisp_api_main_disp_ext_clk_off(void);


#endif /* SHDISP_KERN_NO_LCDC_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
