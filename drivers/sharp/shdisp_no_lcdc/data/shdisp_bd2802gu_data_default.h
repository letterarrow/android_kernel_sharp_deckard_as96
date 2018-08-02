/* drivers/sharp/shdisp_no_lcdc/data/shdisp_bd2802gu_data_default.h  (Display Driver)
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

#ifndef SHDISP_BD2802GU_DATA_DEFAULT_H
#define SHDISP_BD2802GU_DATA_DEFAULT_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "../shdisp_bd2802gu.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define SHDISP_LEDC_COL_VARI_KIND            1
#define SHDISP_LEDC_HANDSET_COLOR_WHITE      0x01

#define SHDISP_LEDC_COLOR_KIND      8
#define SHDISP_LEDC_MODE_PTN_NUM    7
#define SHDISP_LEDC_MODE_ANIME_NUM  10
/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static const unsigned char shdisp_ledc_clrvari_index[SHDISP_LEDC_COL_VARI_KIND] = {
    SHDISP_LEDC_HANDSET_COLOR_WHITE,
};

static const unsigned long shdisp_ledc_color_index_tbl[SHDISP_LEDC_COLOR_KIND][NUM_SHDISP_LEDC_COLOR_TBL_INDEX] = {
    {   0,  0,  0,  0   },
    {   1,  0,  0,  1   },
    {   0,  1,  0,  2   },
    {   0,  0,  1,  3   },
    {   1,  1,  0,  4   },
    {   0,  1,  1,  5   },
    {   1,  0,  1,  6   },
    {   1,  1,  1,  7   },
};

static const unsigned char shdisp_ledc_rgb1_tbl[SHDISP_LEDC_COL_VARI_KIND][SHDISP_LEDC_COLOR_KIND][NUM_SHDISP_LEDC_RGB_PARTS] = {
    {
        { 0x00, 0x00, 0x00 },
        { 0x19, 0x00, 0x00 },
        { 0x00, 0x26, 0x00 },
        { 0x00, 0x00, 0x52 },
        { 0x3F, 0x0D, 0x00 },
        { 0x00, 0x26, 0x52 },
        { 0x19, 0x00, 0x3F },
        { 0x18, 0x33, 0x16 }
    }
};

static const unsigned char shdisp_ledc_rgb2_tbl[SHDISP_LEDC_COL_VARI_KIND][SHDISP_LEDC_COLOR_KIND][NUM_SHDISP_LEDC_RGB_PARTS] = {
    {
        { 0x00, 0x00, 0x00 },
        { 0x19, 0x00, 0x00 },
        { 0x00, 0x26, 0x00 },
        { 0x00, 0x00, 0x52 },
        { 0x3F, 0x0D, 0x00 },
        { 0x00, 0x26, 0x52 },
        { 0x19, 0x00, 0x3F },
        { 0x18, 0x33, 0x16 }
    }
};

static const unsigned char shdisp_ledc_ptn_tbl[SHDISP_LEDC_COL_VARI_KIND][SHDISP_LEDC_MODE_PTN_NUM][NUM_SHDISP_LEDC_SETTING_PTN_INDEX] = {
    {
        { 0x00, 0x1F, 0x00, 0x07, 0x00, 0x1F, 0x00, 0x07 },
        { 0xE4, 0x26, 0x00, 0x0B, 0xE4, 0x26, 0x00, 0x0B },
        { 0xE4, 0x26, 0x06, 0x0B, 0xE4, 0x26, 0x06, 0x0B },
        { 0xF4, 0x26, 0x00, 0x02, 0xF4, 0x26, 0x00, 0x02 },
        { 0xF4, 0x26, 0x00, 0x02, 0xF4, 0x00, 0x26, 0x08 },
        { 0xF2, 0x26, 0x00, 0x01, 0xF2, 0x00, 0x26, 0x08 },
        { 0x24, 0x26, 0x00, 0x00, 0x24, 0x26, 0x00, 0x00 }
    }
};

static const unsigned char shdisp_ledc_anime1_tbl[SHDISP_LEDC_COL_VARI_KIND][SHDISP_LEDC_MODE_ANIME_NUM][NUM_SHDISP_LEDC_SETTING_ANIME_INDEX] = {
    {
        { 0xA4, 0x19, 0x00, 0x03, 0x00, 0x26, 0x0D, 0x00, 0x52, 0x0A },
        { 0xA4, 0x19, 0x00, 0x03, 0x00, 0x26, 0x0D, 0x00, 0x52, 0x0A },
        { 0x00, 0x18, 0x00, 0x07, 0x00, 0x00, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 }
    }
};

static const unsigned char shdisp_ledc_anime2_tbl[SHDISP_LEDC_COL_VARI_KIND][SHDISP_LEDC_MODE_ANIME_NUM][NUM_SHDISP_LEDC_SETTING_ANIME_INDEX] = {
    {
        { 0xA4, 0x19, 0x00, 0x03, 0x00, 0x26, 0x0D, 0x00, 0x52, 0x0A },
        { 0xA4, 0x00, 0x19, 0x0C, 0x00, 0x26, 0x0E, 0x00, 0x52, 0x0B },
        { 0x00, 0x18, 0x00, 0x07, 0x00, 0x00, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 },
        { 0x00, 0x00, 0x1F, 0x07, 0x00, 0x1F, 0x07, 0x00, 0x00, 0x07 }
    }
};

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */


#endif /* SHDISP_BD2802GU_DATA_DEFAULT_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
