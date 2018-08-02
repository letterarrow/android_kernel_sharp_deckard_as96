/* drivers/sharp/shdisp_no_lcdc/shdisp_bd2802gu.h  (Display Driver)
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

#ifndef SHDISP_BD2802GU_H
#define SHDISP_BD2802GU_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

#include <sharp/shdisp_kerl.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#define LEDC_REG_IC_CTRL                            0x00
#define LEDC_REG_RGB_CTRL                           0x01
#define LEDC_REG_RGB1_TIME                          0x02
#define LEDC_REG_R1_VAL1                            0x03
#define LEDC_REG_R1_VAL2                            0x04
#define LEDC_REG_R1_PTRN                            0x05
#define LEDC_REG_G1_VAL1                            0x06
#define LEDC_REG_G1_VAL2                            0x07
#define LEDC_REG_G1_PTRN                            0x08
#define LEDC_REG_B1_VAL1                            0x09
#define LEDC_REG_B1_VAL2                            0x0A
#define LEDC_REG_B1_PTRN                            0x0B
#define LEDC_REG_RGB2_TIME                          0x0C
#define LEDC_REG_R2_VAL1                            0x0D
#define LEDC_REG_R2_VAL2                            0x0E
#define LEDC_REG_R2_PTRN                            0x0F
#define LEDC_REG_G2_VAL1                            0x10
#define LEDC_REG_G2_VAL2                            0x11
#define LEDC_REG_G2_PTRN                            0x12
#define LEDC_REG_B2_VAL1                            0x13
#define LEDC_REG_B2_VAL2                            0x14
#define LEDC_REG_B2_PTRN                            0x15

#define LEDC_REG_RGB_CTRL_VAL_RGB12_OFF             0x00
#define LEDC_REG_RGB_CTRL_VAL_RGB1_REPEAT           0x01
#define LEDC_REG_RGB_CTRL_VAL_RGB1_1SHOT            0x02
#define LEDC_REG_RGB_CTRL_VAL_RGB2_REPEAT           0x10
#define LEDC_REG_RGB_CTRL_VAL_RGB2_1SHOT            0x20

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

enum {
    SHDISP_LEDC_COLOR_NONE,
    SHDISP_LEDC_COLOR_RED,
    SHDISP_LEDC_COLOR_GREEN,
    SHDISP_LEDC_COLOR_BLUE,
    SHDISP_LEDC_COLOR_YELLOW,
    SHDISP_LEDC_COLOR_CYAN,
    SHDISP_LEDC_COLOR_MAGENTA,
    SHDISP_LEDC_COLOR_PINK,
    SHDISP_LEDC_COLOR_WHITE,
    NUM_SHDISP_LEDC_COLOR
};

enum {
    SHDISP_LEDC_LED_INDEX_RGB1,
    SHDISP_LEDC_LED_INDEX_RGB2,
    NUM_SHDISP_LEDC_LED_INDEX
};

enum {
    SHDISP_LEDC_COLOR_TBL_INDEX_R,
    SHDISP_LEDC_COLOR_TBL_INDEX_G,
    SHDISP_LEDC_COLOR_TBL_INDEX_B,
    SHDISP_LEDC_COLOR_TBL_INDEX_COLOR,
    NUM_SHDISP_LEDC_COLOR_TBL_INDEX
};

enum {
    SHDISP_LEDC_RGB_PARTS_RED,
    SHDISP_LEDC_RGB_PARTS_GREEN,
    SHDISP_LEDC_RGB_PARTS_BLUE,
    NUM_SHDISP_LEDC_RGB_PARTS
};

enum {
    SHDISP_LEDC_SETTING_PTN_RGB1_TIME,
    SHDISP_LEDC_SETTING_PTN_RGB1_VAL1,
    SHDISP_LEDC_SETTING_PTN_RGB1_VAL2,
    SHDISP_LEDC_SETTING_PTN_RGB1_PTRN,
    SHDISP_LEDC_SETTING_PTN_RGB2_TIME,
    SHDISP_LEDC_SETTING_PTN_RGB2_VAL1,
    SHDISP_LEDC_SETTING_PTN_RGB2_VAL2,
    SHDISP_LEDC_SETTING_PTN_RGB2_PTRN,
    NUM_SHDISP_LEDC_SETTING_PTN_INDEX
};

enum {
    SHDISP_LEDC_SETTING_ANIME_RGB_TIME,
    SHDISP_LEDC_SETTING_ANIME_R_VAL1,
    SHDISP_LEDC_SETTING_ANIME_R_VAL2,
    SHDISP_LEDC_SETTING_ANIME_R_PTRN,
    SHDISP_LEDC_SETTING_ANIME_G_VAL1,
    SHDISP_LEDC_SETTING_ANIME_G_VAL2,
    SHDISP_LEDC_SETTING_ANIME_G_PTRN,
    SHDISP_LEDC_SETTING_ANIME_B_VAL1,
    SHDISP_LEDC_SETTING_ANIME_B_VAL2,
    SHDISP_LEDC_SETTING_ANIME_B_PTRN,
    NUM_SHDISP_LEDC_SETTING_ANIME_INDEX
};


/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

void shdisp_ledc_API_set_status_addr(struct shdisp_ledc_status* state_str);
int shdisp_ledc_API_init(void);
int shdisp_ledc_API_exit(void);
int shdisp_ledc_API_power_on(void);
int shdisp_ledc_API_power_off(void);
void shdisp_ledc_API_exist(int* ledc_is_exist);
void shdisp_ledc_API_set_rgb(struct shdisp_ledc_rgb *ledc_rgb);
void shdisp_ledc_API_set_color(struct shdisp_ledc_req *ledc_req);
int shdisp_ledc_API_set_clrvari_index( int clrvari );
unsigned char shdisp_ledc_API_get_color_index_and_reedit(struct shdisp_ledc_req *ledc_req, int ledidx);
void shdisp_ledc_API_DIAG_write_reg(unsigned char reg, unsigned char val);
void shdisp_ledc_API_DIAG_read_reg(unsigned char reg, unsigned char *val);

#endif /* SHDISP_BD2802GU_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
