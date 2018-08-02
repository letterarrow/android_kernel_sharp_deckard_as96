/* drivers/sharp/shdisp/shdisp_null_panel.h  (Display Driver)
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

#ifndef SHDISP_NULL_PANEL_H
#define SHDISP_NULL_PANEL_H

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

struct shdisp_panel_operations *shdisp_null_panel_API_create(void);

int shdisp_null_panel_API_mipi_lcd_on(void);
int shdisp_null_panel_API_mipi_lcd_off(void);
int shdisp_null_panel_API_mipi_start_display(void);

int shdisp_null_panel_API_cabc_init(void);
int shdisp_null_panel_API_cabc_indoor_on(void);
int shdisp_null_panel_API_cabc_outdoor_on(int lut_level);
int shdisp_null_panel_API_cabc_off(int wait_on, int pwm_disable);
int shdisp_null_panel_API_cabc_outdoor_move(int lut_level);

#endif /* SHDISP_NULL_PANEL_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */

