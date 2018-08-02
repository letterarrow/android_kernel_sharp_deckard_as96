/* drivers/sharp/shdisp/shdisp_panel.h  (Display Driver)
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

#ifndef SHDISP_PANEL_API_H
#define SHDISP_PANEL_API_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "../../video/msm/msm_fb.h"
#include "../../video/msm/mipi_dsi.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
    SHDISP_DSI_LOW_POWER_MODE = 0,
    SHDISP_DSI_LOW_POWER_MODE_MS,
    SHDISP_DSI_LOW_POWER_MODE_SL,
    SHDISP_DSI_LOW_POWER_MODE_BOTH,

    SHDISP_DSI_HIGH_SPEED_MODE,
    SHDISP_DSI_HIGH_SPEED_MODE_MS,
    SHDISP_DSI_HIGH_SPEED_MODE_SL,
    SHDISP_DSI_HIGH_SPEED_MODE_BOTH,

    SHDISP_DSI_TRANSFER_MODE_MAX
};

enum {
    SHDISP_PANEL_RATE_60_0, /* 60   Hz */
    SHDISP_PANEL_RATE_40_0, /* 40   Hz */
    SHDISP_PANEL_RATE_30_0, /* 30   Hz */
    SHDISP_PANEL_RATE_24_0, /* 24   Hz */
    SHDISP_PANEL_RATE_20_0, /* 20   Hz */
    SHDISP_PANEL_RATE_15_0, /* 15   Hz */
    SHDISP_PANEL_RATE_10_0, /* 10   Hz */
    SHDISP_PANEL_RATE_5_0,  /*  5   Hz */
    SHDISP_PANEL_RATE_4_0,  /*  4   Hz */
    SHDISP_PANEL_RATE_3_0,  /*  3   Hz */
    SHDISP_PANEL_RATE_2_5,  /*  2.5 Hz */
    SHDISP_PANEL_RATE_2_0,  /*  2   Hz */
    SHDISP_PANEL_RATE_1_5,  /*  1.5 Hz */
    SHDISP_PANEL_RATE_1,    /*  1   Hz */
    SHDISP_PANEL_RATE_0_8,  /*  0.8 Hz */
    SHDISP_PANEL_RATE_0_6,  /*  0.6 Hz */
};

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

int shdisp_panel_API_check_panel_info(void);

int shdisp_panel_API_SQE_write_reg(struct shdisp_lcddr_reg *panel_reg);
int shdisp_panel_API_SQE_read_reg(struct shdisp_lcddr_reg *panel_reg);

void shdisp_panel_API_create(void);
int shdisp_panel_API_init_io(void);
int shdisp_panel_API_exit_io(void);
int shdisp_panel_API_init_isr(void);
void shdisp_panel_API_set_param(struct shdisp_panel_param_str *param_str);
int shdisp_panel_API_power_on(void);
int shdisp_panel_API_power_off(void);
int shdisp_panel_API_disp_init_1st(void);
int shdisp_panel_API_disp_init_2nd(void);
int shdisp_panel_API_disp_on(void);
int shdisp_panel_API_sleep(void);
int shdisp_panel_API_deep_standby(void);
int shdisp_panel_API_diag_write_reg(int cog, unsigned char addr, unsigned char *write_data, unsigned char size);
int shdisp_panel_API_diag_read_reg(int cog, unsigned char addr, unsigned char *read_data, unsigned char size);
#ifndef SHDISP_NOT_SUPPORT_FLICKER
int shdisp_panel_API_diag_set_flicker_param(unsigned short alpha);
int shdisp_panel_API_diag_get_flicker_param(unsigned short *alpha);
int shdisp_panel_API_diag_get_flicker_low_param(unsigned short *alpha);
int shdisp_panel_API_diag_set_flicker_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param);
int shdisp_panel_API_diag_get_flicker_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param);
int shdisp_panel_API_diag_get_flicker_low_param_multi_cog(struct shdisp_diag_flicker_param *flicker_param);
#endif /* SHDISP_NOT_SUPPORT_FLICKER */
int shdisp_panel_API_check_recovery(void);
int shdisp_panel_API_get_recovery_type(int *type);

int shdisp_panel_API_cabc_init(void);
int shdisp_panel_API_cabc_indoor_on(void);
int shdisp_panel_API_cabc_outdoor_on(int lut_level);
int shdisp_panel_API_cabc_off(int wait_on, int pwm_disable);
int shdisp_panel_API_cabc_outdoor_move(int lut_level);

int shdisp_panel_API_check_upper_unit(void);
int shdisp_panel_API_check_flicker_param(unsigned short alpha_in, unsigned short *alpha_out);
int shdisp_panel_API_disp_update(struct shdisp_main_update *update);
int shdisp_panel_API_disp_clear_screen(struct shdisp_main_clear *clear);
int shdisp_panel_API_mipi_diag_write_reg(struct dsi_buf *tp, unsigned char addr, unsigned char *write_data, unsigned char size);
#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
int shdisp_panel_API_mipi_diag_mltshortpkt_write_reg(struct dsi_buf *tp, unsigned char * addrs, unsigned char * write_data, unsigned int size);
#endif
int shdisp_panel_API_mipi_diag_read_reg(struct dsi_buf *tp, struct dsi_buf *rp, unsigned char addr, unsigned char *read_data, unsigned char size);
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
int shdisp_panel_API_mipi_diag_write_reg_multi_cog(struct dsi_buf *tp, unsigned char addr, unsigned char *write_data, unsigned char size, int mode);
int shdisp_panel_API_mipi_diag_read_reg_multi_cog(struct dsi_buf *tp, struct dsi_buf *rp, unsigned char addr, unsigned char *read_data, unsigned char size, int mode);
#endif

int shdisp_panel_API_mipi_cmd_lcd_on(void);
int shdisp_panel_API_mipi_cmd_lcd_off(void);
int shdisp_panel_API_mipi_cmd_start_display(void);
int shdisp_panel_API_mipi_cmd_lcd_on_after_black_screen(void);
int shdisp_panel_API_mipi_cmd_lcd_off_black_screen_on(void);
int shdisp_panel_API_mipi_cmd_stop_prepare(void);
void shdisp_panel_API_request_RateCtrl( int ctrl, unsigned char maxFR, unsigned char minFR );
int shdisp_panel_API_mipi_dsi_cmds_tx(struct dsi_buf *tp, struct dsi_cmd_desc *cmds, int cnt);
void shdisp_panel_API_mipi_dsi_cmds_tx_dummy(struct dsi_cmd_desc *cmds);
#ifndef SHDISP_NOT_SUPPORT_COMMAND_MLTPKT_TX_CLMR
int shdisp_panel_API_mipi_dsi_cmds_mltshortpkt_tx(struct dsi_buf *tp, struct dsi_cmd_desc *cmds, int cnt);
#endif
int shdisp_panel_API_mipi_dsi_cmds_rx(struct dsi_buf *tp, struct dsi_buf *rp, struct dsi_cmd_desc *cmds, unsigned char size);
#if defined(CONFIG_SHDISP_PANEL_GEMINI)
int shdisp_panel_API_mipi_dsi_cmds_rx2(struct dsi_buf *tp, struct dsi_buf *rp, struct dsi_cmd_desc *cmds, unsigned char size);
#endif
int shdisp_panel_API_mipi_set_transfer_mode(int mode);

int shdisp_panel_API_diag_set_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
int shdisp_panel_API_diag_get_gamma_info(struct shdisp_diag_gamma_info *gamma_info);
int shdisp_panel_API_diag_set_gamma(struct shdisp_diag_gamma *gamma);

int shdisp_panel_API_power_on_recovery(void);
int shdisp_panel_API_power_off_recovery(void);

int shdisp_panel_API_set_drive_freq(int type);
int shdisp_panel_API_get_drive_freq(void);
#endif /* SHDISP_PANEL_API_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */

