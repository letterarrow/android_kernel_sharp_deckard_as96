/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "msm.h"
#include "msm_ispif.h"
#include <sharp/sh_smem.h>

#include <sharp/sh_boot_manager.h>

#define SENSOR_NAME "imx111"
#define PLATFORM_DRIVER_NAME "msm_camera_imx111"
#define imx111_obj imx111_##obj
#define imx111_REC_SETTINGS


//the issue of white blink when camera starts.
#define P_INIT_LINE 1902/2  //Bv3.5
#define P_INIT_GAIN ((uint16_t)(256 - 256/ 1.23 +0.5)) //reg = 256 - 256/gain
#define P_INIT_FLEN 0x6D4//0x0996
#define P_INIT_DGAIN 0x0100  //256
#define PIL_H ((0xFF00 & P_INIT_LINE) >> 8)
#define PIL_L (0x00FF & P_INIT_LINE)
#define PIG_H ((0xFF00 & P_INIT_GAIN) >> 8)
#define PIG_L (0x00FF & P_INIT_GAIN)
#define PIF_H ((0xFF00 & P_INIT_FLEN) >> 8)
#define PIF_L (0x00FF & P_INIT_FLEN)

#define FHD_INIT_LINE 951 //Bv3.5
#define FHD_INIT_GAIN ((uint16_t)(256 - 256/ 1.23 +0.5)) //reg = 256 - 256/gain
#define FHD_INIT_FLEN 0x6D4//0x4E6
//#define FHD_INIT_FLEN 0x04A0  //1184 is constraint violation!!

#define IMX111_SENSOR_MCLK_6P75HZ  6750000

#define SH_REV_ES0 0x00
#define SH_REV_ES1 0x02
#define SH_REV_PP1 0x03
#define SH_REV_PP2 0x04
#define SH_REV_MP 0x07

#if 0
#undef CDBG
#define CDBG pr_err
#endif

DEFINE_MUTEX(imx111_mut);
static struct msm_sensor_ctrl_t imx111_s_ctrl;
static uint8_t imx111_curr_mode = 15;	/* Invalid */
static int imx111_ae_lock=0;
#define IMX111_EEPROM_BANK_SEL_REG 0x34C9

static struct msm_camera_i2c_reg_conf imx111_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx111_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx111_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx111_groupoff_settings[] = {
	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx111_prev_settings[] = {
 /*B mode */
 #if 0
/*PLL Setting*/
	{0x0305, 0x01},
	{0x0307, 0x60},
	{0x30A4, 0x02},
	{0x303C, 0x15},
/*Mode Setting*/
	{0x0340, 0x09},
	{0x0341, 0x96},
	{0x0342, 0x06},
	{0x0343, 0xE0},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x30},
	{0x0348, 0x0C},
	{0x0349, 0xD7},
	{0x034A, 0x09},
	{0x034B, 0xCF},
	{0x034C, 0x06},
	{0x034D, 0x68},
	{0x034E, 0x04},
	{0x034F, 0xD0},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x03},
	{0x3033, 0x87},
	{0x303D, 0x10},
	{0x303E, 0x40},
	{0x3048, 0x01},
	{0x304C, 0xB7},
	{0x304D, 0x01},
	{0x309B, 0x28},
	{0x30A1, 0x09},
	{0x30AA, 0x00},
	{0x30B2, 0x05},
	{0x30D5, 0x04},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x08},
	{0x3103, 0x22},
	{0x3104, 0x20},
	{0x3105, 0x00},
	{0x3106, 0x87},
	{0x3107, 0x00},
	{0x3108, 0x03},
	{0x3109, 0x02},
	{0x310A, 0x03},
	{0x315C, 0x3A},
	{0x315D, 0x39},
	{0x316E, 0x3B},
	{0x316F, 0x3A},
	{0x3301, 0x00},
	{0x3318, 0x63},
#else
/*still mode*/
/*PLL Setting*/
	{0x0305, 0x01},
	{0x0307, 0x88},
	{0x30A4, 0x02},
	{0x303C, 0x15},
/*Mode Setting*/
	{0x0340, 0x06},
	{0x0341, 0xD4},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x30},
	{0x0348, 0x0C},
	{0x0349, 0xD7},
	{0x034A, 0x09},
	{0x034B, 0xCF},
	{0x034C, 0x0C},
	{0x034D, 0xD0},
	{0x034E, 0x04},
	{0x034F, 0xD0},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x03},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0x41},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x01},
	{0x304C, 0x57},
	{0x304D, 0x03},
	{0x3064, 0x12},
	{0x3073, 0x00},
	{0x3074, 0x11},
	{0x3075, 0x11},
	{0x3076, 0x11},
	{0x3077, 0x11},
	{0x3079, 0x00},
	{0x307A, 0x00},
	{0x3095, 0x83},
	{0x3099, 0xD2},
	{0x309B, 0x28},
	{0x309C, 0x13},
	{0x309E, 0x00},
	{0x30A0, 0x14},
	{0x30A1, 0x09},
	{0x30AA, 0x03},
	{0x30B2, 0x05},
	{0x30D5, 0x00},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DA, 0x00},
	{0x30DB, 0x00},
	{0x30DC, 0x00},
	{0x30DD, 0x00},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x3108, 0x09},
	{0x3109, 0x08},
	{0x310A, 0x0F},
	{0x315C, 0x5D},
	{0x315D, 0x5C},
	{0x316E, 0x5E},
	{0x316F, 0x5D},
	{0x3301, 0x00},
	{0x3304, 0x05},
	{0x3305, 0x05},
	{0x3306, 0x15},
	{0x3307, 0x02},
	{0x3308, 0x0D},
	{0x3309, 0x07},
	{0x330A, 0x09},
	{0x330B, 0x05},
	{0x330C, 0x08},
	{0x330D, 0x06},
	{0x330E, 0x03},
	{0x3318, 0x60},
	{0x3322, 0x03},
	{0x3342, 0x00},
	{0x3348, 0xE0},
#endif
};


static struct msm_camera_i2c_reg_conf imx111_video_VGA_120fps_settings[] = {
/*D mode */
/*PLL Setting*/
	{0x0305, 0x01},
	{0x0307, 0x60},
	{0x30A4, 0x02},
	{0x303C, 0x15},
/*Mode Setting*/
	{0x0340, 0x02},
	{0x0341, 0x64},
	{0x0342, 0x06},
	{0x0343, 0xE0},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0xA0},
	{0x0348, 0x0C},
	{0x0349, 0xD7},
	{0x034A, 0x09},
	{0x034B, 0x5F},
	{0x034C, 0x06},
	{0x034D, 0x68},
	{0x034E, 0x02},
	{0x034F, 0x30},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x05},
	{0x0387, 0x03},
	{0x3033, 0x87},
	{0x303D, 0x10},
	{0x303E, 0x40},
	{0x3048, 0x01},
	{0x304C, 0xB7},
	{0x304D, 0x01},
	{0x309B, 0x28},
	{0x30A1, 0x09},
	{0x30AA, 0x00},
	{0x30B2, 0x03},
	{0x30D5, 0x04},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x08},
	{0x3103, 0x22},
	{0x3104, 0x20},
	{0x3105, 0x00},
	{0x3106, 0x87},
	{0x3107, 0x00},
	{0x3108, 0x03},
	{0x3109, 0x02},
	{0x310A, 0x03},
	{0x315C, 0x3A},
	{0x315D, 0x39},
	{0x316E, 0x3B},
	{0x316F, 0x3A},
	{0x3301, 0x00},
	{0x3318, 0x63},
};

static struct msm_camera_i2c_reg_conf imx111_video_1080P_30fps_settings[] = {
/*FHD mode */
#if 0
/*PLL Setting*/
	{0x0305, 0x01},
	{0x0307, 0x60},
	{0x30A4, 0x02},
	{0x303C, 0x15},
/*Mode Setting*/
	{0x0340, 0x04},
	{0x0341, 0xE6},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x02},
	{0x0345, 0x36},
	{0x0346, 0x02},
	{0x0347, 0xBA},
	{0x0348, 0x0A},
	{0x0349, 0xA9},
	{0x034A, 0x07},
	{0x034B, 0x45},
	{0x034C, 0x08},
	{0x034D, 0x74},
	{0x034E, 0x04},
	{0x034F, 0x8C},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0x40},
	{0x3048, 0x00},
	{0x304C, 0x57},
	{0x304D, 0x03},
	{0x309B, 0x20},
	{0x30A1, 0x08},
	{0x30AA, 0x02},
	{0x30B2, 0x07},
	{0x30D5, 0x00},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x08},
	{0x3103, 0x22},
	{0x3104, 0x20},
	{0x3105, 0x00},
	{0x3106, 0x87},
	{0x3107, 0x00},
	{0x3108, 0x03},
	{0x3109, 0x02},
	{0x310A, 0x03},
	{0x315C, 0x9D},
	{0x315D, 0x9C},
	{0x316E, 0x9E},
	{0x316F, 0x9D},
	{0x3301, 0x00},
	{0x3318, 0x63},
#else
/*still mode*/
/*PLL Setting*/
	{0x0305, 0x01},
	{0x0307, 0x88},
	{0x30A4, 0x02},
	{0x303C, 0x15},
/*Mode Setting*/
	{0x0340, 0x06},
	{0x0341, 0xD4},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x80},
	{0x0346, 0x01},
	{0x0347, 0xAA},
	{0x0348, 0x0C},
	{0x0349, 0x5F},
	{0x034A, 0x08},
	{0x034B, 0x57},
	{0x034C, 0x0B},
	{0x034D, 0xE0},
	{0x034E, 0x06},
	{0x034F, 0xAE},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0x41},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x57},
	{0x304D, 0x03},
	{0x3064, 0x12},
	{0x3073, 0x00},
	{0x3074, 0x11},
	{0x3075, 0x11},
	{0x3076, 0x11},
	{0x3077, 0x11},
	{0x3079, 0x00},
	{0x307A, 0x00},
	{0x3095, 0x83},
	{0x3099, 0xD2},
	{0x309B, 0x20},
	{0x309C, 0x13},
	{0x309E, 0x00},
	{0x30A0, 0x14},
	{0x30A1, 0x08},
	{0x30AA, 0x03},
	{0x30B2, 0x07},
	{0x30D5, 0x00},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DA, 0x00},
	{0x30DB, 0x00},
	{0x30DC, 0x00},
	{0x30DD, 0x00},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x3108, 0x09},
	{0x3109, 0x08},
	{0x310A, 0x0F},
	{0x315C, 0x5D},
	{0x315D, 0x5C},
	{0x316E, 0x5E},
	{0x316F, 0x5D},
	{0x3301, 0x00},
	{0x3304, 0x05},
	{0x3305, 0x05},
	{0x3306, 0x15},
	{0x3307, 0x02},
	{0x3308, 0x0D},
	{0x3309, 0x07},
	{0x330A, 0x09},
	{0x330B, 0x05},
	{0x330C, 0x08},
	{0x330D, 0x06},
	{0x330E, 0x03},
	{0x3318, 0x60},
	{0x3322, 0x03},
	{0x3342, 0x00},
	{0x3348, 0xE0},
#endif
};

static struct msm_camera_i2c_reg_conf imx111_video_60fps_settings[] = {
/*Bsmall mode*/
/*PLL Setting*/
	{0x0305, 0x01},
	{0x0307, 0x60},
	{0x30A4, 0x02},
	{0x303C, 0x15},
/*Mode Setting*/
	{0x0340, 0x04},
	{0x0341, 0xCA},
	{0x0342, 0x06},
	{0x0343, 0xE0},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x88},
	{0x0348, 0x0C},
	{0x0349, 0xD7},
	{0x034A, 0x09},
	{0x034B, 0x77},
	{0x034C, 0x06},
	{0x034D, 0x68},
	{0x034E, 0x04},
	{0x034F, 0x78},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x03},
	{0x3033, 0x87},
	{0x303D, 0x10},
	{0x303E, 0x40},
	{0x3048, 0x01},
	{0x304C, 0xB7},
	{0x304D, 0x01},
	{0x309B, 0x28},
	{0x30A1, 0x09},
	{0x30AA, 0x00},
	{0x30B2, 0x05},
	{0x30D5, 0x04},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x08},
	{0x3103, 0x22},
	{0x3104, 0x20},
	{0x3105, 0x00},
	{0x3106, 0x87},
	{0x3107, 0x00},
	{0x3108, 0x03},
	{0x3109, 0x02},
	{0x310A, 0x03},
	{0x315C, 0x3A},
	{0x315D, 0x39},
	{0x316E, 0x3B},
	{0x316F, 0x3A},
	{0x3301, 0x00},
	{0x3318, 0x63},
};


static struct msm_camera_i2c_reg_conf imx111_video_90fps_settings[] = {
	/* 90 fps */
	{0x0305, 0x01},
	{0x0307, 0x60},
	{0x30A4, 0x02},
	{0x303C, 0x15},
	{0x0340, 0x03},
	{0x0341, 0x32},
	{0x0342, 0x06},
	{0x0343, 0xE0},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x88},
	{0x0348, 0x0C},
	{0x0349, 0xD7},
	{0x034A, 0x09},
	{0x034B, 0x77},
	{0x034C, 0x06},
	{0x034D, 0x68},
	{0x034E, 0x04},
	{0x034F, 0x78},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x03},
	{0x3033, 0x87},
	{0x303D, 0x10},
	{0x303E, 0x40},
	{0x3048, 0x01},
	{0x304C, 0xB7},
	{0x304D, 0x01},
	{0x309B, 0x28},
	{0x30A1, 0x09},
	{0x30AA, 0x00},
	{0x30B2, 0x05},
	{0x30D5, 0x04},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x08},
	{0x3103, 0x22},
	{0x3104, 0x20},
	{0x3105, 0x00},
	{0x3106, 0x87},
	{0x3107, 0x00},
	{0x3108, 0x03},
	{0x3109, 0x02},
	{0x310A, 0x03},
	{0x315C, 0x3A},
	{0x315D, 0x39},
	{0x316E, 0x3B},
	{0x316F, 0x3A},
	{0x3301, 0x00},
	{0x3318, 0x63},
};

static struct msm_camera_i2c_reg_conf imx111_snap_settings[] = {
#if 0
/*still mode*/
/*PLL Setting*/
	{0x0305, 0x01},
	{0x0307, 0x60},
	{0x30A4, 0x02},
	{0x303C, 0x15},
/*Mode Setting*/
	{0x0340, 0x0A},
	{0x0341, 0x34},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x30},
	{0x0348, 0x0C},
	{0x0349, 0xD7},
	{0x034A, 0x09},
	{0x034B, 0xCF},
	{0x034C, 0x0C},
	{0x034D, 0xD0},
	{0x034E, 0x09},
	{0x034F, 0xA0},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0x40},
	{0x3048, 0x00},
	{0x304C, 0x57},
	{0x304D, 0x03},
	{0x309B, 0x20},
	{0x30A1, 0x08},
	{0x30AA, 0x02},
	{0x30B2, 0x07},
	{0x30D5, 0x00},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x08},
	{0x3103, 0x22},
	{0x3104, 0x20},
	{0x3105, 0x00},
	{0x3106, 0x87},
	{0x3107, 0x00},
	{0x3108, 0x03},
	{0x3109, 0x02},
	{0x310A, 0x03},
	{0x315C, 0x9D},
	{0x315D, 0x9C},
	{0x316E, 0x9E},
	{0x316F, 0x9D},
	{0x3301, 0x00},
	{0x3318, 0x63},
#else
/*still mode*/
/*PLL Setting*/
	{0x0305, 0x01},
	{0x0307, 0x88},
	{0x30A4, 0x02},
	{0x303C, 0x15},
/*Mode Setting*/
	{0x0340, 0x09},
	{0x0341, 0xC6},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x30},
	{0x0348, 0x0C},
	{0x0349, 0xD7},
	{0x034A, 0x09},
	{0x034B, 0xCF},
	{0x034C, 0x0C},
	{0x034D, 0xD0},
	{0x034E, 0x09},
	{0x034F, 0xA0},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0x41},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x57},
	{0x304D, 0x03},
	{0x3064, 0x12},
	{0x3073, 0x00},
	{0x3074, 0x11},
	{0x3075, 0x11},
	{0x3076, 0x11},
	{0x3077, 0x11},
	{0x3079, 0x00},
	{0x307A, 0x00},
	{0x3095, 0x83},
	{0x3099, 0xD2},
	{0x309B, 0x20},
	{0x309C, 0x13},
	{0x309E, 0x00},
	{0x30A0, 0x14},
	{0x30A1, 0x08},
	{0x30AA, 0x03},
	{0x30B2, 0x07},
	{0x30D5, 0x00},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DA, 0x00},
	{0x30DB, 0x00},
	{0x30DC, 0x00},
	{0x30DD, 0x00},
	{0x30DE, 0x00},
	{0x30DF, 0x20},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x3108, 0x09},
	{0x3109, 0x08},
	{0x310A, 0x0F},
	{0x315C, 0x5D},
	{0x315D, 0x5C},
	{0x316E, 0x5E},
	{0x316F, 0x5D},
	{0x3301, 0x00},
	{0x3304, 0x05},
	{0x3305, 0x05},
	{0x3306, 0x15},
	{0x3307, 0x02},
	{0x3308, 0x0D},
	{0x3309, 0x07},
	{0x330A, 0x09},
	{0x330B, 0x05},
	{0x330C, 0x08},
	{0x330D, 0x06},
	{0x330E, 0x03},
	{0x3318, 0x60},
	{0x3322, 0x03},
	{0x3342, 0x00},
	{0x3348, 0xE0},
#endif

};

static struct msm_camera_i2c_reg_conf imx111_recommend_settings[] = {
#if 0
	{0x3080,0x50},
	{0x3087,0x53},
	{0x309D,0x94},
	{0x30C6,0x00},
	{0x30C7,0x00},
	{0x3115,0x0B},
	{0x3118,0x30},
	{0x311D,0x25},
	{0x3121,0x0A},
	{0x3212,0xF2},
	{0x3213,0x0F},
	{0x3215,0x0F},
	{0x3217,0x0B},
	{0x3219,0x0B},
	{0x321B,0x0D},
	{0x321D,0x0D},
	{0x32AA,0x11},
	/* black level setting */
	{0x3032, 0x40},
#else
	{0x0101,0x00},
	{0x3080,0x50},
	{0x3087,0x53},
	{0x309D,0x94},
	{0x30B1,0x00},
	{0x30C6,0x00},
	{0x30C7,0x00},
	{0x3115,0x0B},
	{0x3118,0x30},
	{0x311D,0x25},
	{0x3121,0x0A},
	{0x3212,0xF2},
	{0x3213,0x0F},
	{0x3215,0x0F},
	{0x3217,0x0B},
	{0x3219,0x0B},
	{0x321B,0x0D},
	{0x321D,0x0D},
	{0x32AA,0x11},
	/* black level setting */
	{0x3032, 0x40},
#endif
};

static struct msm_camera_i2c_reg_conf imx111_comm1_settings[] = {
	{0x3035, 0x10},
	{0x303B, 0x14},
	{0x3312, 0x45},
	{0x3313, 0xC0},
	{0x3310, 0x20},
	{0x3310, 0x00},
	{0x303B, 0x04},
	{0x303D, 0x00},
	{0x0100, 0x10},
	{0x3035, 0x00},
};

static struct msm_camera_i2c_reg_conf imx111_comm2_part1_settings[] = {
	{0x0340, 0x0A},
	{0x0341, 0x34},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x034B, 0xCF},
	{0x034C, 0x0C},
	{0x034D, 0xD0},
	{0x034E, 0x09},
	{0x034F, 0xA0},
	{0x0387, 0x01},
	{0x3033, 0x00},
	{0x3048, 0x00},
	{0x304C, 0x57},
	{0x304D, 0x03},
	{0x309B, 0x20},
	{0x30A1, 0x08},
	{0x30AA, 0x02},
	{0x30B2, 0x07},
	{0x30D5, 0x00},
	{0x315C, 0x9D},
	{0x315D, 0x9C},
	{0x316E, 0x9E},
	{0x316F, 0x9D},

};

static struct msm_camera_i2c_reg_conf imx111_comm2_part2_settings[] = {
	{0x30B1, 0x43},
	/*{0x3311, 0x80},
	{0x3311, 0x00},*/
};

static struct msm_camera_i2c_conf_array imx111_comm_confs[] = {
	{&imx111_comm1_settings[0],
	ARRAY_SIZE(imx111_comm1_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx111_comm2_part1_settings[0],
	ARRAY_SIZE(imx111_comm2_part1_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx111_comm2_part2_settings[0],
	ARRAY_SIZE(imx111_comm2_part2_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct v4l2_subdev_info imx111_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx111_init_conf[] = {
	{&imx111_recommend_settings[0],
	ARRAY_SIZE(imx111_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array imx111_confs[] = {
    {&imx111_snap_settings[0],
	ARRAY_SIZE(imx111_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx111_prev_settings[0],
	ARRAY_SIZE(imx111_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx111_video_60fps_settings[0],
	ARRAY_SIZE(imx111_video_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx111_video_90fps_settings[0],
	ARRAY_SIZE(imx111_video_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx111_video_VGA_120fps_settings[0],
	ARRAY_SIZE(imx111_video_VGA_120fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx111_video_1080P_30fps_settings[0],
	ARRAY_SIZE(imx111_video_1080P_30fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx111_snap_settings[0],
	ARRAY_SIZE(imx111_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	
};

static struct msm_sensor_output_info_t imx111_dimensions[] = {
#if 0
	{
		.x_output = 0xCD0, /* 3280 */
		.y_output = 0x9A0, /* 2464 */
		.line_length_pclk = 0xD70, /* 3440 */
		.frame_length_lines = 0xA34, /* 2612 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x668, /* 1640 */
		.y_output = 0x4D0, /* 1232 */
		.line_length_pclk = 0x06E0, /* 1760 */
		.frame_length_lines = 0x0996, /* 2454 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x668, /* 1640 */
		.y_output = 0x478, /* 1144 */
		.line_length_pclk = 0x06E0, /* 1760 */
		.frame_length_lines = 0x04CA, /* 1226 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x668, /* 1640 */
		.y_output = 0x478, /* 1144 */
		.line_length_pclk = 0x06E0, /* 1760 */
		.frame_length_lines = 0x04CA, /* 1226 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x668, /* 1640 */
		.y_output = 0x230, /* 560 */
		.line_length_pclk = 0x06E0, /* 1760 */
		.frame_length_lines = 0x0264, /* 612 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x874, /* 2164 */
		.y_output = 0x48C, /* 1164 */
		.line_length_pclk = 0xD70, /* 3440 */
		.frame_length_lines = 0x4E6, /* 1254 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0xCD0, /* 3280 */
		.y_output = 0x9A0, /* 2464 */
		.line_length_pclk = 0xD70, /* 3440 */
		.frame_length_lines = 0xA34, /* 2612 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
#else
	{/*Still*/
		.x_output = 0xCD0, /* 3280 */
		.y_output = 0x9A0, /* 2464 */
		.line_length_pclk = 0x0D70, /* 3440 */
		.frame_length_lines = 0x09C6, /* 2502 */
		.vt_pixel_clk = 183600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{/*B*/
		.x_output = 0xCD0, /* 3280 */
		.y_output = 0x4D0, /* 1232 */
		.line_length_pclk = 0x0D70, /* 3440 */
		.frame_length_lines = 0x06D4, /* 1748 */
		.vt_pixel_clk = 183600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x668, /* 1640 */
		.y_output = 0x478, /* 1144 */
		.line_length_pclk = 0x06E0, /* 1760 */
		.frame_length_lines = 0x04CA, /* 1226 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x668, /* 1640 */
		.y_output = 0x478, /* 1144 */
		.line_length_pclk = 0x06E0, /* 1760 */
		.frame_length_lines = 0x04CA, /* 1226 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x668, /* 1640 */
		.y_output = 0x230, /* 560 */
		.line_length_pclk = 0x06E0, /* 1760 */
		.frame_length_lines = 0x0264, /* 612 */
		.vt_pixel_clk = 129600000,
		.op_pixel_clk = 129600000,
		.binning_factor = 1,
	},
	{/*FHD*/
		.x_output = 0xBE0, /* 3040 */
		.y_output = 0x6AE, /* 1710 */
		.line_length_pclk = 0xD70, /* 3440 */
		.frame_length_lines = 0x6D4, /* 1748 */
		.vt_pixel_clk = 183600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{/*Still*/
		.x_output = 0xCD0, /* 3280 */
		.y_output = 0x9A0, /* 2464 */
		.line_length_pclk = 0x0D70, /* 3440 */
		.frame_length_lines = 0x09C6, /* 2502 */
		.vt_pixel_clk = 183600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
#endif
};

static struct msm_sensor_output_reg_addr_t imx111_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t imx111_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x0111,
};

static struct msm_sensor_exp_gain_info_t imx111_exp_gain_info = {
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x204,
	.vert_offset = 5,
};

static const struct i2c_device_id imx111_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx111_s_ctrl},
	{ }
};

unsigned char *p_imx111_shdiag_data = NULL;

typedef struct {
	uint32_t fl_lines;
	uint32_t line;
	uint16_t gain;
	uint16_t digital_gain;
} imx111_exp_gain_t;

imx111_exp_gain_t imx111_exp_gain[MSM_SENSOR_INVALID_RES] ={
	{ 0x0A34, 0x0028, 0x0005, 0x0100},
	{ P_INIT_FLEN, P_INIT_LINE, P_INIT_GAIN, 0x0100},
	{ 0x04CA, 0x0041, 0x0003, 0x0100},
	{ 0x04CA, 0x0041, 0x0003, 0x0100},
	{ 0x0264, 0x0041, 0x0003, 0x0100},
	{ FHD_INIT_FLEN, FHD_INIT_LINE, FHD_INIT_GAIN, 0x0100},
	{ 0x0A34, 0x0028, 0x0005, 0x0100},
};

static struct i2c_driver imx111_i2c_driver = {
	.id_table = imx111_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx111_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

int32_t imx111_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	long fps = 0;
	uint16_t ll_pclk;
	uint16_t fl_lines;
	uint32_t delay = 0;

	CDBG("%s res = %d\n", __func__, res);

	if (update_type == MSM_SENSOR_REG_INIT) {

		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		imx111_exp_gain[6].fl_lines = 0x0A34;
		imx111_exp_gain[6].line = 0x0028;
		imx111_exp_gain[6].gain = 0x0005;
		imx111_exp_gain[6].digital_gain = 0x0100;
		imx111_exp_gain[1].fl_lines = P_INIT_FLEN;
		imx111_exp_gain[1].line = P_INIT_LINE;
		imx111_exp_gain[1].gain = P_INIT_GAIN;
		imx111_exp_gain[1].digital_gain = P_INIT_DGAIN;
		imx111_exp_gain[5].fl_lines = FHD_INIT_FLEN;
		imx111_exp_gain[5].line = FHD_INIT_LINE;
		imx111_exp_gain[5].gain = FHD_INIT_GAIN;
		imx111_exp_gain[5].digital_gain = 0x0100;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		if(s_ctrl->curr_res != MSM_SENSOR_INVALID_RES){

			s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
			rc = msm_camera_i2c_read(
					s_ctrl->sensor_i2c_client,
					s_ctrl->sensor_output_reg_addr->line_length_pclk, &ll_pclk,
					MSM_CAMERA_I2C_WORD_DATA);
			rc = msm_camera_i2c_read(
					s_ctrl->sensor_i2c_client,
					s_ctrl->sensor_output_reg_addr->frame_length_lines, &fl_lines,
					MSM_CAMERA_I2C_WORD_DATA);
			if((ll_pclk != 0) && (fl_lines != 0)){
				fps = s_ctrl->msm_sensor_reg->
					output_settings[s_ctrl->curr_res].vt_pixel_clk /
					fl_lines / ll_pclk;
				if(fps != 0)
					delay = 1000 / fps;
			}
			CDBG("%s fps = %ld, frame time = %d\n", __func__, fps, delay);
			delay += 10;
			CDBG("%s delay = %d\n", __func__, delay);
			msleep(delay);
		}

		if (res == 0) {
			msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,
				(struct msm_camera_i2c_reg_conf *)
				imx111_comm_confs[0].conf,
				imx111_comm_confs[0].size,
				imx111_comm_confs[0].data_type);
		} else {
			msm_sensor_write_res_settings(s_ctrl, res);
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
				output_settings[res].op_pixel_clk);
			s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
			if(s_ctrl->curr_res != MSM_SENSOR_INVALID_RES){
				msleep(30);
			}
		}
	}

	return rc;
}

int32_t imx111_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
						struct fps_cfg *fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	s_ctrl->fps_divider = fps->fps_div;

	if (s_ctrl->curr_res != MSM_SENSOR_INVALID_RES) {
		uint16_t fl_read = 0;
		total_lines_per_frame = (uint16_t)
			((s_ctrl->curr_frame_length_lines) *
			s_ctrl->fps_divider/Q10);

		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			&fl_read, MSM_CAMERA_I2C_WORD_DATA);

		CDBG("%s: before_fl = %d, new_fl = %d", __func__, fl_read, total_lines_per_frame);

		if(fl_read < total_lines_per_frame) {
			pr_err("%s: Write new_fl (before_fl = %d, new_fl = %d)", __func__, fl_read, total_lines_per_frame);
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_output_reg_addr->frame_length_lines,
				total_lines_per_frame, MSM_CAMERA_I2C_WORD_DATA);
		}
	}
	return rc;
}

int32_t imx111_sensor_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line, int32_t luma_avg, uint16_t fgain)
{

	uint32_t fl_lines;
	uint8_t offset;
	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	CDBG("\n%s:Gain:%d, Linecount:%d\n", __func__, gain, line);
	if (s_ctrl->curr_res == 0) {
		msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,
			(struct msm_camera_i2c_reg_conf *)
			imx111_comm_confs[1].conf,
			imx111_comm_confs[1].size,
			imx111_comm_confs[1].data_type);

		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			fl_lines, MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			line, MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
			MSM_CAMERA_I2C_WORD_DATA);

		msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client,
			(struct msm_camera_i2c_reg_conf *)
			imx111_comm_confs[2].conf,
			imx111_comm_confs[2].size,
			imx111_comm_confs[2].data_type);

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[s_ctrl->curr_res].op_pixel_clk);
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
	} else {
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			fl_lines, MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			line, MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x020E, imx111_exp_gain[s_ctrl->curr_res].digital_gain,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0210, imx111_exp_gain[s_ctrl->curr_res].digital_gain,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0212, imx111_exp_gain[s_ctrl->curr_res].digital_gain,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0214, imx111_exp_gain[s_ctrl->curr_res].digital_gain,
			MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	}

	return 0;
}

void imx111_sensor_adjust_frame_lines(struct msm_sensor_ctrl_t *s_ctrl)
{

	if(imx111_ae_lock == 1){
		CDBG("%s imx111_ae_lock=%d", __func__, imx111_ae_lock);
		return;
	}

//	if((s_ctrl->curr_res == MSM_SENSOR_RES_QTR) || (s_ctrl->curr_res == MSM_SENSOR_RES_5)){
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines, imx111_exp_gain[s_ctrl->curr_res].fl_lines,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, imx111_exp_gain[s_ctrl->curr_res].line,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->global_gain_addr, imx111_exp_gain[s_ctrl->curr_res].gain,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x020E, imx111_exp_gain[s_ctrl->curr_res].digital_gain,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0210, imx111_exp_gain[s_ctrl->curr_res].digital_gain,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0212, imx111_exp_gain[s_ctrl->curr_res].digital_gain,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0214, imx111_exp_gain[s_ctrl->curr_res].digital_gain,
			MSM_CAMERA_I2C_WORD_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
		
		CDBG("%s fl_lines=%d line=%d gain=%d digital_gain=%d", __func__, imx111_exp_gain[s_ctrl->curr_res].fl_lines, imx111_exp_gain[s_ctrl->curr_res].line, imx111_exp_gain[s_ctrl->curr_res].gain, imx111_exp_gain[s_ctrl->curr_res].digital_gain);
//	}
}

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&imx111_i2c_driver);
}

static struct v4l2_subdev_core_ops imx111_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};
static struct v4l2_subdev_video_ops imx111_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx111_subdev_ops = {
	.core = &imx111_subdev_core_ops,
	.video  = &imx111_subdev_video_ops,
};

/* for ES0 */
static enum msm_camera_vreg_name_t imx111_veg_seq_1[] = {
	CAM_VANA,
	CAM_VDIG,
	CAM_VIO,
	CAM_VAF,
};

#if defined(CONFIG_MACH_LYNX_GP6D)
/* for ES1/PP */
static enum msm_camera_vreg_name_t imx111_veg_seq_2[] = {
	CAM_VANA,
	CAM_VDIG,
	CAM_VAF,
};
#endif

#if 0
void imx111_set_dev_addr(struct msm_camera_eeprom_client *ectrl,
			uint32_t *reg_addr) {
	 int32_t rc = 0;
	 rc = msm_camera_i2c_write(ectrl->i2c_client, 0x34C9,
	  (*reg_addr) >> 16, MSM_CAMERA_I2C_BYTE_DATA);
	  if (rc != 0) {
	  CDBG("%s: Page write error\n", __func__);
	  return;
  }
  (*reg_addr) = (*reg_addr) & 0xFFFF;

  /*rc = msm_camera_i2c_poll(ectrl->i2c_client, 0x34C8, 0x01,
		MSM_CAMERA_I2C_SET_BYTE_MASK);
	if (rc != 0) {
		CDBG("%s: Read Status2 error\n", __func__);
		return;
	}*/
}
#endif /* if 0 */

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", IMX111_SENSOR_MCLK_6P75HZ},
};

static int32_t imx111_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	unsigned short rev;
	enum msm_camera_vreg_name_t *vreg_seq;
	int num_vreg_seq;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	CDBG("%s: %d\n", __func__, __LINE__);
	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

#if defined(CONFIG_MACH_LYNX_GP6D)
	rev = sh_boot_get_hw_revision();
    switch (rev) {
    case SH_REV_ES0:
		vreg_seq = imx111_veg_seq_1;
		num_vreg_seq = ARRAY_SIZE(imx111_veg_seq_1);
        break;
    case SH_REV_ES1:
    case SH_REV_PP1:
    case SH_REV_PP2:
    case SH_REV_MP:
    default:
		vreg_seq = imx111_veg_seq_2;
		num_vreg_seq = ARRAY_SIZE(imx111_veg_seq_2);
        break;
	}
#else
	vreg_seq = imx111_veg_seq_1;
	num_vreg_seq = ARRAY_SIZE(imx111_veg_seq_1);
#endif

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	usleep(1000);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}


	msleep(100);

//	if (data->sensor_platform_info->ext_power_ctrl != NULL)
//		data->sensor_platform_info->ext_power_ctrl(1);

    CDBG("%s: rc value %d\n", __func__, rc);
	imx111_curr_mode = 15;	/* Invalid */
	imx111_ae_lock = 0;

	return rc;
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
}

static int32_t imx111_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	long fps = 0;
	uint16_t ll_pclk;
	uint16_t fl_lines;
	uint32_t delay = 0;
	unsigned short rev;
	enum msm_camera_vreg_name_t *vreg_seq;
	int num_vreg_seq;
	CDBG("%s\n", __func__);

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);

	msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->line_length_pclk, &ll_pclk,
			MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines, &fl_lines,
			MSM_CAMERA_I2C_WORD_DATA);
	CDBG("%s ll_pclk = %d, frame fl_lines = %d\n", __func__, ll_pclk, fl_lines);

	if((ll_pclk != 0) && (fl_lines != 0) && (s_ctrl->curr_res != MSM_SENSOR_INVALID_RES)){
		fps = s_ctrl->msm_sensor_reg->
			output_settings[s_ctrl->curr_res].vt_pixel_clk /
			fl_lines / ll_pclk;
		if(fps != 0)
			delay = 1000 / fps;
	}
	CDBG("%s fps = %ld, frame time = %d\n", __func__, fps, delay);
	msleep(delay);

	usleep(50);

//	if (data->sensor_platform_info->ext_power_ctrl != NULL)
//		data->sensor_platform_info->ext_power_ctrl(0);

#if defined(CONFIG_MACH_LYNX_GP6D)
	rev = sh_boot_get_hw_revision();
    switch (rev) {
    case SH_REV_ES0:
		vreg_seq = imx111_veg_seq_1;
		num_vreg_seq = ARRAY_SIZE(imx111_veg_seq_1);
        break;
    case SH_REV_ES1:
    case SH_REV_PP1:
    case SH_REV_PP2:
    case SH_REV_MP:
    default:
		vreg_seq = imx111_veg_seq_2;
		num_vreg_seq = ARRAY_SIZE(imx111_veg_seq_2);
        break;
	}
#else
	vreg_seq = imx111_veg_seq_1;
	num_vreg_seq = ARRAY_SIZE(imx111_veg_seq_1);
#endif
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);

	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
	return 0;
}

static int imx111_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&imx111_mut);
	CDBG("imx111_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SHDIAG_GET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				rc = -EFAULT;
				break;
			}
			CDBG("%s:%d i2c_read addr=0x%0x\n",__func__,__LINE__,cdata.cfg.i2c_info.addr);
			rc = msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, cdata.cfg.i2c_info.addr , data, cdata.cfg.i2c_info.length);
			CDBG("%s:%d i2c_read data=0x%0x\n",__func__,__LINE__,*(unsigned char *)data);
			if (copy_to_user((void *)cdata.cfg.i2c_info.data,
				data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_to_user error\n",__func__);
				rc = -EFAULT;
				break;
			}
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
		}
			break;
	case CFG_SHDIAG_SET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				rc = -EFAULT;
				break;
			}
			if (copy_from_user(data,
				(void *)cdata.cfg.i2c_info.data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_to_user error\n",__func__);
				rc = -EFAULT;
				break;
			}
			rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client, cdata.cfg.i2c_info.addr, data, cdata.cfg.i2c_info.length);
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))){
				rc = -EFAULT;
				break;
			}
		}
			break;
	case CFG_SH_SET_EXP_GAIN:
		{
			uint32_t fl_lines;
			uint8_t offset;
			long fps = 0;
			uint16_t ll_pclk;
			uint16_t cur_fl_lines;
			uint32_t delay = 0;

			fl_lines = s_ctrl->curr_frame_length_lines;
			fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
			offset = s_ctrl->sensor_exp_gain_info->vert_offset;
			if (cdata.cfg.exp_gain.line > (fl_lines - offset))
				fl_lines = cdata.cfg.exp_gain.line + offset;

			if(imx111_ae_lock == 1){
				CDBG("%s imx111_ae_lock=%d", __func__, imx111_ae_lock);
				break;
			}

			if( imx111_curr_mode == 0 ) {
				rc = msm_camera_i2c_read(
						s_ctrl->sensor_i2c_client,
						s_ctrl->sensor_output_reg_addr->line_length_pclk, &ll_pclk,
						MSM_CAMERA_I2C_WORD_DATA);
				rc = msm_camera_i2c_read(
						s_ctrl->sensor_i2c_client,
						s_ctrl->sensor_output_reg_addr->frame_length_lines, &cur_fl_lines,
						MSM_CAMERA_I2C_WORD_DATA);
				CDBG("%s ll_pclk = %d, frame fl_lines = %d\n", __func__, ll_pclk, cur_fl_lines);
				if((ll_pclk != 0) && (cur_fl_lines != 0) && (s_ctrl->curr_res != MSM_SENSOR_INVALID_RES)){
					fps = s_ctrl->msm_sensor_reg->
						output_settings[s_ctrl->curr_res].vt_pixel_clk /
						cur_fl_lines / ll_pclk;

					if(fps != 0)
						delay = 1000 / fps;
				}
				CDBG("%s fps = %ld, delay = %d\n", __func__, fps, delay);
			}

			s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
				MSM_CAMERA_I2C_WORD_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, cdata.cfg.exp_gain.line,
				MSM_CAMERA_I2C_WORD_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->global_gain_addr, cdata.cfg.exp_gain.gain,
				MSM_CAMERA_I2C_WORD_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x020E, cdata.cfg.exp_gain.digital_gain,
				MSM_CAMERA_I2C_WORD_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x0210, cdata.cfg.exp_gain.digital_gain,
				MSM_CAMERA_I2C_WORD_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x0212, cdata.cfg.exp_gain.digital_gain,
				MSM_CAMERA_I2C_WORD_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				0x0214, cdata.cfg.exp_gain.digital_gain,
				MSM_CAMERA_I2C_WORD_DATA);
			s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
			
			if(s_ctrl->curr_res != MSM_SENSOR_INVALID_RES){
				imx111_exp_gain[s_ctrl->curr_res].fl_lines = fl_lines;
				imx111_exp_gain[s_ctrl->curr_res].line = cdata.cfg.exp_gain.line;
				imx111_exp_gain[s_ctrl->curr_res].gain = cdata.cfg.exp_gain.gain;
				imx111_exp_gain[s_ctrl->curr_res].digital_gain = cdata.cfg.exp_gain.digital_gain;
			}

			CDBG("%s fl_lines=%d line=%d gain=%d digital_gain=%d", __func__, fl_lines, cdata.cfg.exp_gain.line, cdata.cfg.exp_gain.gain, cdata.cfg.exp_gain.digital_gain);
			if( imx111_curr_mode == 0 ) {
				msleep(delay);
			}
		}
		break;
#if 1
	case CFG_SHDIAG_AE_LOCK:
			imx111_ae_lock = cdata.cfg.ae_mode;
		break;
#endif
	default:
		mutex_unlock(&imx111_mut);
		return msm_sensor_config(s_ctrl, argp);
		break;
	}
	
	mutex_unlock(&imx111_mut);
	return rc;
}

static int32_t imx111_sensor_set_sensor_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int mode, int res)
{
	int32_t rc = 0;
	uint32_t fl_lines;
	uint8_t offset;

	CDBG("%s mode=%d res=%d\n", __func__, mode, res);
	if(((imx111_curr_mode != 10) && (mode == 10)) ||
	   ((imx111_curr_mode == 10) && (mode == 2)) ||
	   ((s_ctrl->curr_res == MSM_SENSOR_RES_5) && (res != MSM_SENSOR_RES_5)) ||
	   ((s_ctrl->curr_res != MSM_SENSOR_RES_5) && (res == MSM_SENSOR_RES_5))){
		CDBG("%s mode==SENSOR_MODE_ZSL or SENSOR_MODE_ZSL -> SENSOR_MODE_PREVIEW\n", __func__);

		imx111_curr_mode = mode;
		imx111_exp_gain[6].fl_lines = 0x0A34;
		imx111_exp_gain[6].line = 0x0028;
		imx111_exp_gain[6].gain = 0x0005;
		imx111_exp_gain[6].digital_gain = 0x0100;
		imx111_exp_gain[1].fl_lines = P_INIT_FLEN;
		imx111_exp_gain[1].line = P_INIT_LINE;
		imx111_exp_gain[1].gain = P_INIT_GAIN;
		imx111_exp_gain[1].digital_gain = P_INIT_DGAIN;
		imx111_exp_gain[5].fl_lines = FHD_INIT_FLEN;
		imx111_exp_gain[5].line = FHD_INIT_LINE;
		imx111_exp_gain[5].gain = FHD_INIT_GAIN;
		imx111_exp_gain[5].digital_gain = 0x0100;

		if (s_ctrl->curr_res != res) {

			s_ctrl->curr_frame_length_lines =
				s_ctrl->msm_sensor_reg->
				output_settings[res].frame_length_lines;

			s_ctrl->curr_line_length_pclk =
				s_ctrl->msm_sensor_reg->
				output_settings[res].line_length_pclk;

			CDBG("%s s_ctrl->curr_res = %d\n", __func__, s_ctrl->curr_res);
			if(s_ctrl->curr_res != MSM_SENSOR_INVALID_RES){
				CDBG("%s line[%d]=%d gain[%d]=%d digital_gain[%d]=%d", __func__, 
					s_ctrl->curr_res, imx111_exp_gain[s_ctrl->curr_res].line, 
					s_ctrl->curr_res, imx111_exp_gain[s_ctrl->curr_res].gain, 
					s_ctrl->curr_res, imx111_exp_gain[s_ctrl->curr_res].digital_gain);
				fl_lines = s_ctrl->curr_frame_length_lines;
				fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
				offset = s_ctrl->sensor_exp_gain_info->vert_offset;
				if (imx111_exp_gain[s_ctrl->curr_res].line > (fl_lines - offset))
					fl_lines = imx111_exp_gain[s_ctrl->curr_res].line + offset;
		
				imx111_exp_gain[res].fl_lines = fl_lines;
				imx111_exp_gain[res].line = imx111_exp_gain[s_ctrl->curr_res].line;
				imx111_exp_gain[res].gain = imx111_exp_gain[s_ctrl->curr_res].gain;
				imx111_exp_gain[res].digital_gain = imx111_exp_gain[s_ctrl->curr_res].digital_gain;
			}
			CDBG("%s line[%d]=%d gain[%d]=%d digital_gain[%d]=%d", __func__, 
																res, imx111_exp_gain[res].line, 
																res, imx111_exp_gain[res].gain, 
																res, imx111_exp_gain[res].digital_gain);

			if (s_ctrl->is_csic ||
				!s_ctrl->sensordata->csi_if)
				rc = s_ctrl->func_tbl->sensor_csi_setting(s_ctrl,
					MSM_SENSOR_UPDATE_PERIODIC, res);
			else
				rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
					MSM_SENSOR_UPDATE_PERIODIC, res);
			if (rc < 0)
				return rc;
			s_ctrl->curr_res = res;
		} else {
			long fps = 0;
			uint16_t ll_pclk;
			uint16_t fl_lines;
			uint32_t delay = 0;

			CDBG("%s res=%d s_ctrl->curr_res=%d\n", __func__, res, s_ctrl->curr_res);
			CDBG("%s line[%d]=%d gain[%d]=%d digital_gain[%d]=%d", __func__, 
																res, imx111_exp_gain[res].line, 
																res, imx111_exp_gain[res].gain, 
																res, imx111_exp_gain[res].digital_gain);

			rc = msm_camera_i2c_read(
					s_ctrl->sensor_i2c_client,
					s_ctrl->sensor_output_reg_addr->line_length_pclk, &ll_pclk,
					MSM_CAMERA_I2C_WORD_DATA);
			rc = msm_camera_i2c_read(
					s_ctrl->sensor_i2c_client,
					s_ctrl->sensor_output_reg_addr->frame_length_lines, &fl_lines,
					MSM_CAMERA_I2C_WORD_DATA);

			if (s_ctrl->func_tbl->sensor_adjust_frame_lines)
//				rc = s_ctrl->func_tbl->sensor_adjust_frame_lines(s_ctrl, res);
				s_ctrl->func_tbl->sensor_adjust_frame_lines(s_ctrl);

			if((ll_pclk != 0) && (fl_lines != 0)){
				fps = s_ctrl->msm_sensor_reg->
					output_settings[s_ctrl->curr_res].vt_pixel_clk /
					fl_lines / ll_pclk;
				if(fps != 0)
					delay = 1000 / fps;
			}
			CDBG("%s fps = %ld, frame time = %d\n", __func__, fps, delay);
			delay += 10;
			CDBG("%s delay = %d\n", __func__, delay);
			msleep(delay);

		}
	} else {
		CDBG("%s mode!=SENSOR_MODE_ZSL \n", __func__);
		imx111_curr_mode = mode;
		if (s_ctrl->curr_res != res) {
			s_ctrl->curr_frame_length_lines =
				s_ctrl->msm_sensor_reg->
				output_settings[res].frame_length_lines;

			s_ctrl->curr_line_length_pclk =
				s_ctrl->msm_sensor_reg->
				output_settings[res].line_length_pclk;

			if (s_ctrl->is_csic ||
				!s_ctrl->sensordata->csi_if)
				rc = s_ctrl->func_tbl->sensor_csi_setting(s_ctrl,
					MSM_SENSOR_UPDATE_PERIODIC, res);
			else
				rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
					MSM_SENSOR_UPDATE_PERIODIC, res);
			if (rc < 0)
				return rc;
			s_ctrl->curr_res = res;
		}
	}

	return rc;
}

int imx111_pmic_set_flash_led_current(enum pmic8058_leds id, unsigned mA)
{
	return 0;
}
EXPORT_SYMBOL(imx111_pmic_set_flash_led_current);

static struct msm_sensor_fn_t imx111_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
//	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_set_fps = imx111_sensor_set_fps,
	.sensor_write_exp_gain = imx111_sensor_write_exp_gain1,
	.sensor_write_snapshot_exp_gain = imx111_sensor_write_exp_gain1,
	.sensor_setting = imx111_sensor_setting,
//	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_set_sensor_mode = imx111_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = imx111_sensor_config,
	.sensor_power_up = imx111_sensor_power_up,
	.sensor_power_down = imx111_sensor_power_down,
	.sensor_adjust_frame_lines = imx111_sensor_adjust_frame_lines,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t imx111_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx111_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx111_start_settings),
	.stop_stream_conf = imx111_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx111_stop_settings),
	.group_hold_on_conf = imx111_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx111_groupon_settings),
	.group_hold_off_conf = imx111_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(imx111_groupoff_settings),
	.init_settings = &imx111_init_conf[0],
	.init_size = ARRAY_SIZE(imx111_init_conf),
	.mode_settings = &imx111_confs[0],
	.output_settings = &imx111_dimensions[0],
	.num_conf = ARRAY_SIZE(imx111_confs),
};

static struct msm_sensor_ctrl_t imx111_s_ctrl = {
	.msm_sensor_reg = &imx111_regs,
	.sensor_i2c_client = &imx111_sensor_i2c_client,
	.sensor_i2c_addr = 0x34,
	.sensor_output_reg_addr = &imx111_reg_addr,
	.sensor_id_info = &imx111_id_info,
	.sensor_exp_gain_info = &imx111_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &imx111_mut,
	.sensor_i2c_driver = &imx111_i2c_driver,
	.sensor_v4l2_subdev_info = imx111_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx111_subdev_info),
	.sensor_v4l2_subdev_ops = &imx111_subdev_ops,
	.func_tbl = &imx111_func_tbl,
	.clk_rate = IMX111_SENSOR_MCLK_6P75HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Sony 8MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
