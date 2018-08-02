/* driver/sharp/wtap/wtap_lis3dsh.h  (W-Tap Sensor Driver)
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

#ifndef WTAP_LIS3DSH_H
#define WTAP_LIS3DSH_H

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#define WTAP_REG_MIN                WTAP_LIS3DSH_REG_OUT_T
#define WTAP_REG_MAX                WTAP_LIS3DSH_REG_OUTS2

/* ------------------------------------------------------------------------- */
/* REGISTER DEFINITION for LIS3DSH                                           */
/* ------------------------------------------------------------------------- */

#define WTAP_LIS3DSH_REG_OUT_T      0x0C
#define WTAP_LIS3DSH_REG_INFO1      0x0D
#define WTAP_LIS3DSH_REG_INFO2      0x0E
#define WTAP_LIS3DSH_REG_WHO_AM_I   0x0F

#define WTAP_LIS3DSH_REG_OFF_X      0x10
#define WTAP_LIS3DSH_REG_OFF_Y      0x11
#define WTAP_LIS3DSH_REG_OFF_Z      0x12
#define WTAP_LIS3DSH_REG_CS_X       0x13
#define WTAP_LIS3DSH_REG_CS_Y       0x14
#define WTAP_LIS3DSH_REG_CS_Z       0x15
#define WTAP_LIS3DSH_REG_LC_L       0x16
#define WTAP_LIS3DSH_REG_LC_H       0x17
#define WTAP_LIS3DSH_REG_STAT       0x18
#define WTAP_LIS3DSH_REG_VCF_1      0x1B
#define WTAP_LIS3DSH_REG_VCF_2      0x1C
#define WTAP_LIS3DSH_REG_VCF_3      0x1D
#define WTAP_LIS3DSH_REG_VCF_4      0x1E
#define WTAP_LIS3DSH_REG_THRS3      0x1F

#define WTAP_LIS3DSH_REG_CTRL1      0x20
#define WTAP_LIS3DSH_REG_CTRL2      0x21
#define WTAP_LIS3DSH_REG_CTRL3      0x22
#define WTAP_LIS3DSH_REG_CTRL4      0x23
#define WTAP_LIS3DSH_REG_CTRL5      0x24
#define WTAP_LIS3DSH_REG_CTRL6      0x25
#define WTAP_LIS3DSH_REG_STATUS     0x27
#define WTAP_LIS3DSH_REG_OUT_X_L    0x28
#define WTAP_LIS3DSH_REG_OUT_X_H    0x29
#define WTAP_LIS3DSH_REG_OUT_Y_L    0x2A
#define WTAP_LIS3DSH_REG_OUT_Y_H    0x2B
#define WTAP_LIS3DSH_REG_OUT_Z_L    0x2C
#define WTAP_LIS3DSH_REG_OUT_Z_H    0x2D
#define WTAP_LIS3DSH_REG_FIFO_CTRL  0x2E

#define WTAP_LIS3DSH_REG_S0_SM1     0x40
#define WTAP_LIS3DSH_REG_S1_SM1     0x41
#define WTAP_LIS3DSH_REG_S2_SM1     0x42
#define WTAP_LIS3DSH_REG_S3_SM1     0x43
#define WTAP_LIS3DSH_REG_S4_SM1     0x44
#define WTAP_LIS3DSH_REG_S5_SM1     0x45
#define WTAP_LIS3DSH_REG_S6_SM1     0x46
#define WTAP_LIS3DSH_REG_S7_SM1     0x47
#define WTAP_LIS3DSH_REG_S8_SM1     0x48
#define WTAP_LIS3DSH_REG_S9_SM1     0x49
#define WTAP_LIS3DSH_REG_S10_SM1    0x4A
#define WTAP_LIS3DSH_REG_S11_SM1    0x4B
#define WTAP_LIS3DSH_REG_S12_SM1    0x4C
#define WTAP_LIS3DSH_REG_S13_SM1    0x4D
#define WTAP_LIS3DSH_REG_S14_SM1    0x4E
#define WTAP_LIS3DSH_REG_S15_SM1    0x4F

#define WTAP_LIS3DSH_REG_TIM4_1     0x50
#define WTAP_LIS3DSH_REG_TIM3_1     0x51
#define WTAP_LIS3DSH_REG_TIM2_1L    0x52
#define WTAP_LIS3DSH_REG_TIM2_1H    0x53
#define WTAP_LIS3DSH_REG_TIM1_1L    0x54
#define WTAP_LIS3DSH_REG_TIM1_1H    0x55
#define WTAP_LIS3DSH_REG_THRS2_1    0x56
#define WTAP_LIS3DSH_REG_THRS1_1    0x57
#define WTAP_LIS3DSH_REG_MASK1_B    0x59
#define WTAP_LIS3DSH_REG_MASK1_A    0x5A
#define WTAP_LIS3DSH_REG_SETT1      0x5B

#define WTAP_LIS3DSH_REG_S0_SM2     0x60
#define WTAP_LIS3DSH_REG_S1_SM2     0x61
#define WTAP_LIS3DSH_REG_S2_SM2     0x62
#define WTAP_LIS3DSH_REG_S3_SM2     0x63
#define WTAP_LIS3DSH_REG_S4_SM2     0x64
#define WTAP_LIS3DSH_REG_S5_SM2     0x65
#define WTAP_LIS3DSH_REG_S6_SM2     0x66
#define WTAP_LIS3DSH_REG_S7_SM2     0x67
#define WTAP_LIS3DSH_REG_S8_SM2     0x68
#define WTAP_LIS3DSH_REG_S9_SM2     0x69
#define WTAP_LIS3DSH_REG_S10_SM2    0x6A
#define WTAP_LIS3DSH_REG_S11_SM2    0x6B
#define WTAP_LIS3DSH_REG_S12_SM2    0x6C
#define WTAP_LIS3DSH_REG_S13_SM2    0x6D
#define WTAP_LIS3DSH_REG_S14_SM2    0x6E
#define WTAP_LIS3DSH_REG_S15_SM2    0x6F

#define WTAP_LIS3DSH_REG_TIM4_2     0x70
#define WTAP_LIS3DSH_REG_TIM3_2     0x71
#define WTAP_LIS3DSH_REG_TIM2_2L    0x72
#define WTAP_LIS3DSH_REG_TIM2_2H    0x73
#define WTAP_LIS3DSH_REG_TIM1_2L    0x74
#define WTAP_LIS3DSH_REG_TIM1_2H    0x75
#define WTAP_LIS3DSH_REG_THRS2_2    0x76
#define WTAP_LIS3DSH_REG_THRS1_2    0x77
#define WTAP_LIS3DSH_REG_DES        0x78
#define WTAP_LIS3DSH_REG_MASK2_B    0x79
#define WTAP_LIS3DSH_REG_MASK2_A    0x7A
#define WTAP_LIS3DSH_REG_SETT2      0x7B
#define WTAP_LIS3DSH_REG_OUTS2      0x7F


#endif /* WTAP_LIS3DSH_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
