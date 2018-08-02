/* include/sharp/wtap_kerl.h  (W-Tap Sensor Driver)
 *
 * Copyright (C) 2012 SHARP CORPORATION All rights reserved.
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
/* SHARP W-TAP SENSOR DRIVER FOR KERNEL MODE                                 */
/* ------------------------------------------------------------------------- */

#ifndef WTAP_KERL_H
#define WTAP_KERL_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

#include <linux/ioctl.h>

/* ------------------------------------------------------------------------- */
/* MACRAOS                                                                   */
/* ------------------------------------------------------------------------- */

#define WTAP_DEVNAME    "/dev/wtap"

#define WTAP_REG_TBL_INIT 13
#define WTAP_REG_TBL_SETTING_STATE 61
#define WTAP_REG_TBL_ENABLE 1
#define WTAP_REG_TBL_DISABLE 3

#define WTAP_REG_TBL_INIT_DIAG 6
#define WTAP_REG_TBL_SETTING_STATE_DIAG 0
#define WTAP_REG_TBL_ENABLE_DIAG 1
#define WTAP_REG_TBL_DISABLE_DIAG 3


#define WTAP_DEF_SETTING_AXIS           WTAP_AXIS_Z;
#define WTAP_DEF_SETTING_STRENGTH       WTAP_STRENGTH_03
#define WTAP_DEF_SETTING_INTERVAL       WTAP_INTERVAL_02

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
    WTAP_RESULT_SUCCESS,
    WTAP_RESULT_FAILURE,
    WTAP_RESULT_FAILURE_USER,
    WTAP_RESULT_FAILURE_INT,
    NUM_WTAP_RESULT
};

enum {
    WTAP_AXIS_NONE  = 0x00,
    WTAP_AXIS_Z     = 0x01,
    WTAP_AXIS_Y     = 0x02,
    WTAP_AXIS_X     = 0x04,
    NUM_WTAP_AXIS   = 0x08
};

enum {
    WTAP_STRENGTH_00,
    WTAP_STRENGTH_01,
    WTAP_STRENGTH_02,
    WTAP_STRENGTH_03,
    WTAP_STRENGTH_04,
    WTAP_STRENGTH_05,
    NUM_WTAP_STRENGTH
};

enum {
    WTAP_INTERVAL_00,
    WTAP_INTERVAL_01,
    WTAP_INTERVAL_02,
    WTAP_INTERVAL_03,
    NUM_WTAP_INTERVAL
};

enum {
    WTAP_USER_CANCEL,
    WTAP_INTRRUPT_WTAP,
    WTAP_SYSTEM_BUSY,
    WTAP_SYSTEM_ERROR,
    WTAP_INTRRUPT_SENSOR_READ,
    NUM_WTAP_INT
};

struct w_tap_setting_param {
    unsigned char axis;
    unsigned char strength;
    unsigned char interval;
};

struct w_tap_read_data {
    unsigned char Result;
    signed short out_x;
    signed short out_y;
    signed short out_z;
};

struct wtap_sensor_reg {
    unsigned char addr;
    unsigned char val;
};

struct wtap_setting_data {
    unsigned short version;
    unsigned char init_num;
    unsigned char setting_num;
    unsigned char enable_num;
    unsigned char disable_num;
    unsigned char wtap_reg_initialize_tbl[WTAP_REG_TBL_INIT][2];
    unsigned char wtap_reg_setting_state_tbl[WTAP_REG_TBL_SETTING_STATE][2];
    unsigned char wtap_reg_enable_tbl[WTAP_REG_TBL_ENABLE][2];
    unsigned char wtap_reg_disable_tbl[WTAP_REG_TBL_DISABLE][2];
    struct w_tap_setting_param wtap_setting_state;
};

/* ------------------------------------------------------------------------- */
/* IOCTL                                                                     */
/* ------------------------------------------------------------------------- */

#define WTAP_IOC_MAGIC 0xAA

#define WTAP_IOCTL_SETTING_WRITE    _IOW  (WTAP_IOC_MAGIC,  0, struct wtap_setting_data)
#define WTAP_IOCTL_SETTING_READ     _IOR  (WTAP_IOC_MAGIC,  1, struct w_tap_setting_param)
#define WTAP_IOCTL_SETTING_CLEAR    _IO   (WTAP_IOC_MAGIC,  2)
#define WTAP_IOCTL_READ             _IOR  (WTAP_IOC_MAGIC,  3, struct w_tap_read_data)
#define WTAP_IOCTL_READ_STOP        _IO   (WTAP_IOC_MAGIC,  4)
#define WTAP_IOCTL_REG_WRITE        _IOW  (WTAP_IOC_MAGIC,  5, struct wtap_sensor_reg)
#define WTAP_IOCTL_REG_READ         _IOWR (WTAP_IOC_MAGIC,  6, struct wtap_sensor_reg)
#define WTAP_IOCTL_DIAG_READ        _IOR  (WTAP_IOC_MAGIC,  7, struct w_tap_read_data)
#define WTAP_IOCTL_DATA_SET         _IOW (WTAP_IOC_MAGIC,   8, struct wtap_setting_data)
#define WTAP_IOCTL_WAKELOCK         _IO   (WTAP_IOC_MAGIC,  9)

/* ------------------------------------------------------------------------- */
/* DATA                                                                      */
/* ------------------------------------------------------------------------- */

static const unsigned char wtap_reg_initialize_diag_tbl[WTAP_REG_TBL_INIT_DIAG][2] =
{
    { 0x20, 0x5F },
    { 0x23, 0x01 },
    { 0xFF, 0x05 },
    { 0x23, 0x8C },
    { 0x24, 0xC8 },
    { 0x25, 0x10 },
};

static const unsigned char wtap_reg_active_diag_tbl[WTAP_REG_TBL_ENABLE_DIAG][2] =
{
    { 0x20, 0x57 },
};

static const unsigned char wtap_reg_standby_diag_tbl[WTAP_REG_TBL_DISABLE_DIAG][2] =
{
    { 0x21, 0x00 },
    { 0x22, 0x00 },
    { 0x20, 0x07 },
};

#endif /* WTAP_KERL_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
