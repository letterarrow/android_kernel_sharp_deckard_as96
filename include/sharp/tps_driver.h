/* include/sharp/tps_driver.h
 *
 * Copyright (C) 2012 SHARP CORPORATION
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
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
	TPS_DRIVER_SUCCESS = 0,
	TPS_DRIVER_FAILURE
};

enum {
	TPS_DRIVER_USER_NONE = 0x00,
	TPS_DRIVER_USER_USB,
	TPS_DRIVER_USER_HML,
	TPS_DRIVER_USER_CAMLED,
	TPS_DRIVER_USER_BOTH,
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
int tps_driver_register_user(int user);
int tps_driver_unregister_user(int user);
int tps_driver_get_user(void);
/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
