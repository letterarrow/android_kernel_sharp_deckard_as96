/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#define RATE_8_to_1
#define STATS_NO_LIM
#define TRMODE 1  //0: off 1: atr 2:ftr

#include "msm_sensor.h"
#include "msm.h"
#define SENSOR_NAME "imx135"
#define PLATFORM_DRIVER_NAME "msm_camera_imx135"
#define imx135_obj imx135_##obj

DEFINE_MUTEX(imx135_mut);

/* Register addresses for HDR control */
#define SHORT_SHUTTER_WORD_ADDR		0x0230
#define SHORT_GAIN_BYTE_ADDR		0x0233
#define ABS_GAIN_R_WORD_ADDR		0x0714
#define ABS_GAIN_B_WORD_ADDR		0x0716
#define LSC_ENABLE_BYTE_ADDR		0x0700
#define EN_LSC_BYTE_ADDR			0x4500
#define RAM_SEL_TOGGLE_BYTE_ADDR	0x3A63
#define LSC_TABLE_START_ADDR		0x4800
#define TC_OUT_NOISE_WORD_ADDR		0x4452
#define TC_OUT_MID_WORD_ADDR		0x4454
#if 1
#define TC_OUT_SAT_WORD_ADDR		0x4456
#endif
#define TC_NOISE_BRATE_REGADDR		0x4458
#define TC_MID_BRATE_REGADDR		0x4459
#if 1
#define TC_SAT_BRATE_REGADDR		0x445A
#endif
#define TC_SWITCH_BYTE_ADDR			0x446C

#if 1
#define TC_ATR_TEMPORAL_BYTE_ADDR	0x445B
#define TC_WD_INTEG_RATIO_BYTE_ADDR	0x0239
#define TC_DEBUG_AVE_WORD_ADDR		0x4470
#define TC_DEBUG_MIN_WORD_ADDR		0x4472
#define TC_DEBUG_MAX_WORD_ADDR		0x4474
#define TC_DEBUG_SEL_BYTE_ADDR		0x4476
#define TC_WBLMT_WORD_ADDR			0x441E
#define TC_AESAT_WORD_ADDR			0x4446
#endif

#define LSC_TABLE_LEN_BYTES			504
#if 1
#define LSC_TX_MAX_SIZE				128
#endif

/* Adaptive tone reduction parameters */
#define INIT_ATR_OUT_NOISE	0xA0
#define INIT_ATR_OUT_MID	0x800
#define ATR_OFFSET			0x00
#define THRESHOLD_DEAD_ZONE	10
#define THRESHOLD_0			20
#define THRESHOLD_1			50

#if 1
/* LSC enable/disable threshold */
#define THRESHOLD_DEAD_ZONE_LSC_1 10
#define THRESHOLD_DEAD_ZONE_LSC_2 30

#include <sharp/sh_boot_manager.h>

#define SH_REV_ES0 0x00
#define SH_REV_ES1 0x02
#define SH_REV_PP1 0x03
#define SH_REV_PP2 0x04
#define SH_REV_MP 0x07

#if defined(CONFIG_MACH_DECKARD_AS96)
#define SH_REV_CMP    SH_REV_ES1
#else
#define SH_REV_CMP    SH_REV_PP1
#endif

/* the issue of white blink when camera starts */
#define P_INIT_LINE_PREV 728
#define P_INIT_GAIN_PREV ((uint8_t)(256 - 256/ 1.97 +0.5)) //reg = 256 - 256/gain
#define PIL_H_PREV ((0xFF00 & P_INIT_LINE_PREV) >> 8)
#define PIL_L_PREV (0x00FF & P_INIT_LINE_PREV)

#define P_INIT_LINE_SNAP 1169
#define P_INIT_GAIN_SNAP ((uint8_t)(256 - 256/ 1.23 +0.5)) //reg = 256 - 256/gain
#define PIL_H_SNAP ((0xFF00 & P_INIT_LINE_SNAP) >> 8)
#define PIL_L_SNAP (0x00FF & P_INIT_LINE_SNAP)

#endif

static struct msm_sensor_ctrl_t imx135_s_ctrl;

static struct msm_camera_i2c_reg_conf imx135_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx135_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx135_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx135_groupoff_settings[] = {
	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx135_recommend_settings[] = {
	/*Global Settings*/
#if defined(CONFIG_MACH_LYNX_DL32) || defined(CONFIG_MACH_TDN)
	{0x0101, 0x03},
#else
	{0x0101, 0x00},
#endif
	{0x0105, 0x01},
	{0x0110, 0x00},
	{0x0220, 0x01},
	{0x3302, 0x11},
	{0x3833, 0x20},
	{0x3893, 0x00},
	{0x3906, 0x08},
	{0x3907, 0x01},
	{0x391B, 0x01},
	{0x3C09, 0x01},
	{0x600A, 0x00},
	{0x3008, 0xB0},
	{0x320A, 0x00},
	{0x320D, 0x10},
	{0x3216, 0x2E},
	{0x322C, 0x02},
	{0x3409, 0x0C},
	{0x340C, 0x2D},
	{0x3411, 0x39},
	{0x3414, 0x1E},
	{0x3427, 0x04},
	{0x3480, 0x1E},
	{0x3484, 0x1E},
	{0x3488, 0x1E},
	{0x348C, 0x1E},
	{0x3490, 0x1E},
	{0x3494, 0x1E},
	{0x3511, 0x8F},
	{0x364F, 0x2D},
#if 1 // Raja recommend setting (HDR setting)
	/*Defect Correction Recommended Setting */
	{0x380A, 0x00},
	{0x380B, 0x00},
	{0x4103, 0x00},
	/*Color Artifact Recommended Setting */
	{0x4243, 0x9A},
	{0x4330, 0x01},
	{0x4331, 0x90},
	{0x4332, 0x02},
	{0x4333, 0x58},
	{0x4334, 0x03},
	{0x4335, 0x20},
	{0x4336, 0x03},
	{0x4337, 0x84},
	{0x433C, 0x01},
	{0x4340, 0x02},
	{0x4341, 0x58},
	{0x4342, 0x03},
	{0x4343, 0x52},
	/*Moir reduction Parameter Setting	*/
	{0x4364, 0x0B},
	{0x4368, 0x00},
	{0x4369, 0x0F},
	{0x436A, 0x03},
	{0x436B, 0xA8},
	{0x436C, 0x00},
	{0x436D, 0x00},
	{0x436E, 0x00},
	{0x436F, 0x06},
	/*CNR parameter setting	*/
	{0x4281, 0x21},
	{0x4282, 0x18},
	{0x4283, 0x04},
	{0x4284, 0x08},
	{0x4287, 0x7F},
	{0x4288, 0x08},
	{0x428B, 0x7F},
	{0x428C, 0x08},
	{0x428F, 0x7F},
	{0x4297, 0x00},
	{0x4298, 0x7E},
	{0x4299, 0x7E},
	{0x429A, 0x7E},
	{0x42A4, 0xFB},
	{0x42A5, 0x7E},
	{0x42A6, 0xDF},
	{0x42A7, 0xB7},
	{0x42AF, 0x03},
	/*ARNR Parameter Setting*/
	{0x4207, 0x03},
	{0x4216, 0x08},
	{0x4217, 0x08},
	/*DLC Parameter Setting*/
	{0x4218, 0x00},
	{0x421B, 0x20},
	{0x421F, 0x04},
	{0x4222, 0x02},
	{0x4223, 0x22},
	{0x422E, 0x54},
	{0x422F, 0xFB},
	{0x4230, 0xFF},
	{0x4231, 0xFE},
	{0x4232, 0xFF},
	{0x4235, 0x58},
	{0x4236, 0xF7},
	{0x4237, 0xFD},
	{0x4239, 0x4E},
	{0x423A, 0xFC},
	{0x423B, 0xFD},
	/*HDR Setting*/
	{0x4300, 0x00},
	{0x4316, 0x12},
	{0x4317, 0x22},
	{0x4318, 0x00},
	{0x4319, 0x00},
	{0x431A, 0x00},
	{0x4324, 0x03},
	{0x4325, 0x20},
	{0x4326, 0x03},
	{0x4327, 0x84},
	{0x4328, 0x03},
	{0x4329, 0x20},
	{0x432A, 0x03},
	{0x432B, 0x20},
	{0x432C, 0x01},
	{0x432D, 0x01},
	{0x4338, 0x02},
	{0x4339, 0x00},
	{0x433A, 0x00},
	{0x433B, 0x02},
	{0x435A, 0x03},
	{0x435B, 0x84},
	{0x435E, 0x01},
	{0x435F, 0xFF},
	{0x4360, 0x01},
	{0x4361, 0xF4},
	{0x4362, 0x03},
	{0x4363, 0x84},
	{0x437B, 0x01},
#if 0
	{0x4401, 0x3F},
#else
	{0x4401, 0x03}, // STATS Limit to 1023
#endif
	{0x4402, 0xFF},
	{0x4404, 0x13},
	{0x4405, 0x26},
	{0x4406, 0x07},
	{0x4408, 0x20},
	{0x4409, 0xE5},
	{0x440A, 0xFB},
	{0x440C, 0xF6},
	{0x440D, 0xEA},
	{0x440E, 0x20},
	{0x4410, 0x00},
	{0x4411, 0x00},
	{0x4412, 0x3F},
	{0x4413, 0xFF},
	{0x4414, 0x1F},
	{0x4415, 0xFF},
	{0x4416, 0x20},
	{0x4417, 0x00},
	{0x4418, 0x1F},
	{0x4419, 0xFF},
	{0x441A, 0x20},
	{0x441B, 0x00},
	{0x441C, 0x00}, /* tone mode */
	{0x441D, 0x40},
	{0x441E, 0x1E},
	{0x441F, 0x38},
	{0x4420, 0x01},
	{0x4444, 0x00},
	{0x4445, 0x00},
	{0x4446, 0x1D},
	{0x4447, 0xF9},
	{0x4452, 0x00},
	{0x4453, 0xA0},
	{0x4454, 0x08},
	{0x4455, 0x00},
	{0x4456, 0x0F},
	{0x4457, 0xFF},
	{0x4458, 0x18},
	{0x4459, 0x18},
	{0x445A, 0x3F},
	{0x445B, 0x00},
	{0x445C, 0x00},
	{0x445D, 0x28},
	{0x445E, 0x01},
	{0x445F, 0x90},
	{0x4460, 0x00},
	{0x4461, 0x60},
	{0x4462, 0x00},
	{0x4463, 0x00},
	{0x4464, 0x00},
	{0x4465, 0x00},
	{0x446C, 0x01},  /* turn_off */
	{0x446D, 0x00},
	{0x446E, 0x00},
	/*LSC Setting*/
	{0x452A, 0x02},
	/*White Balance Setting */
	{0x0712, 0x01},
	{0x0713, 0x00},
	{0x0714, 0x01},
	{0x0715, 0x00},
	{0x0716, 0x01},
	{0x0717, 0x00},
	{0x0718, 0x01},
	{0x0719, 0x00},
	/*Shading setting*/
	{0x4500, 0x1F},
#endif
};

static struct msm_camera_i2c_reg_conf imx135_snap_settings[] = {
	{0x011E, 0x06},/* Clock Setting */
	{0x011F, 0xC0},
	{0x0301, 0x05},
	{0x0303, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x0305, 0x01},
#else
	{0x0305, 0x06},
#endif
	{0x0309, 0x05},
	{0x030B, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x030C, 0x00},
	{0x030D, 0x76},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x030C, 0x02},
	{0x030D, 0xC7},
#else
	{0x030C, 0x02},
	{0x030D, 0xC2},
#endif
	{0x030E, 0x01},
	{0x3209, 0x02},
	{0x3408, 0x0F},
	{0x3A06, 0x11},
	{0x0108, 0x03},	/* Mode setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0391, 0x11},
	{0x0392, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x4082, 0x01},
	{0x4083, 0x01},
	{0x7006, 0x04},
	{0x0700, 0x00},	/* OptionalFunction setting */
//	{0x3A63, 0x00},
	{0x4100, 0xE8},
	{0x4203, 0xFF},
	{0x4344, 0x00},
	{0x441C, 0x00}, /* tone mode */
	{0x0340, 0x0D},	/* Size setting	*/
	{0x0341, 0x8E},
#if defined(CONFIG_MACH_TDN)
	{0x0342, 0x11},
	{0x0343, 0xEC},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x0342, 0x12},
	{0x0343, 0x00},
#else
	{0x0342, 0x11},
	{0x0343, 0xDE},
#endif
	{0x0344, 0x00},
	{0x0345, 0x04},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x10},
	{0x0349, 0x6B},
	{0x034A, 0x0C},
	{0x034B, 0x2F},
	{0x034C, 0x10},
	{0x034D, 0x68},
	{0x034E, 0x0C},
	{0x034F, 0x30},
	{0x0350, 0x00},
	{0x0351, 0x00},
	{0x0352, 0x00},
	{0x0353, 0x00},
	{0x0354, 0x10},
	{0x0355, 0x68},
	{0x0356, 0x0C},
	{0x0357, 0x30},
	{0x301D, 0x30},
	{0x3310, 0x10},
	{0x3311, 0x68},
	{0x3312, 0x0C},
	{0x3313, 0x30},
	{0x331C, 0x01},
#if defined(CONFIG_MACH_DECKARD_AS96) || defined(CONFIG_MACH_TDN)
	{0x331D, 0xB8},
#else
	{0x331D, 0x90},
#endif
	{0x4084, 0x00},
	{0x4085, 0x00},
	{0x4086, 0x00},
	{0x4087, 0x00},
	{0x4400, 0x00},
	{0x0830, 0x7F},	/* Global Timing Setting */
	{0x0831, 0x37},
	{0x0832, 0x67},
	{0x0833, 0x37},
	{0x0834, 0x37},
	{0x0835, 0x3F},
	{0x0836, 0xC7},
	{0x0837, 0x3F},
	{0x0839, 0x1F},
	{0x083A, 0x17},
	{0x083B, 0x02},
#if 0
	{0x0202, 0x0C},	/* Integration Time Setting */
	{0x0203, 0x50},
	{0x0205, 0x00},	/* Gain Setting	*/
#else
	{0x0202, PIL_H_SNAP},	/* Integration Time Setting */
	{0x0203, PIL_L_SNAP},
	{0x0205, P_INIT_GAIN_SNAP},	/* Gain Setting	*/
#endif
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x0230, 0x00},	/* HDR Setting */
	{0x0231, 0x00},
	{0x0233, 0x00},
	{0x0234, 0x00},
	{0x0235, 0x40},
	{0x0238, 0x00},
	{0x0239, 0x04},
	{0x023B, 0x00},
	{0x023C, 0x01},
	{0x33B0, 0x04},
	{0x33B1, 0x00},
	{0x33B3, 0x00},
	{0x33B4, 0x01},
	{0x3800, 0x00},
};
#if 0
static struct msm_camera_i2c_reg_conf imx135_LSCTable_settings[] = {
	{0x4800, 0x01},
	{0x4801, 0x00},
	{0x4802, 0x01},
	{0x4803, 0x00},
	{0x4804, 0x01},
	{0x4805, 0x00},
	{0x4806, 0x01},
	{0x4807, 0xEA},
	{0x4808, 0x01},
	{0x4809, 0xCB},
	{0x480A, 0x01},
	{0x480B, 0xA1},
	{0x480C, 0x01},
	{0x480D, 0x94},
	{0x480E, 0x01},
	{0x480F, 0x7A},
	{0x4810, 0x01},
	{0x4811, 0x7F},
	{0x4812, 0x01},
	{0x4813, 0x6C},
	{0x4814, 0x01},
	{0x4815, 0x8C},
	{0x4816, 0x01},
	{0x4817, 0x76},
	{0x4818, 0x01},
	{0x4819, 0xBC},
	{0x481A, 0x01},
	{0x481B, 0x9D},
	{0x481C, 0x01},
	{0x481D, 0xF9},
	{0x481E, 0x01},
	{0x481F, 0xCF},
	{0x4820, 0x02},
	{0x4821, 0xCB},
	{0x4822, 0x02},
	{0x4823, 0x7B},
	{0x4824, 0x02},
	{0x4825, 0x61},
	{0x4826, 0x02},
	{0x4827, 0x1F},
	{0x4828, 0x01},
	{0x4829, 0xCC},
	{0x482A, 0x01},
	{0x482B, 0xA1},
	{0x482C, 0x01},
	{0x482D, 0x7A},
	{0x482E, 0x01},
	{0x482F, 0x60},
	{0x4830, 0x01},
	{0x4831, 0x42},
	{0x4832, 0x01},
	{0x4833, 0x35},
	{0x4834, 0x01},
	{0x4835, 0x2E},
	{0x4836, 0x01},
	{0x4837, 0x27},
	{0x4838, 0x01},
	{0x4839, 0x40},
	{0x483A, 0x01},
	{0x483B, 0x36},
	{0x483C, 0x01},
	{0x483D, 0x6C},
	{0x483E, 0x01},
	{0x483F, 0x5B},
	{0x4840, 0x01},
	{0x4841, 0xB3},
	{0x4842, 0x01},
	{0x4843, 0x95},
	{0x4844, 0x02},
	{0x4845, 0x24},
	{0x4846, 0x01},
	{0x4847, 0xF2},
	{0x4848, 0x02},
	{0x4849, 0x17},
	{0x484A, 0x01},
	{0x484B, 0xDE},
	{0x484C, 0x01},
	{0x484D, 0xA3},
	{0x484E, 0x01},
	{0x484F, 0x7F},
	{0x4850, 0x01},
	{0x4851, 0x4A},
	{0x4852, 0x01},
	{0x4853, 0x39},
	{0x4854, 0x01},
	{0x4855, 0x18},
	{0x4856, 0x01},
	{0x4857, 0x11},
	{0x4858, 0x01},
	{0x4859, 0x08},
	{0x485A, 0x01},
	{0x485B, 0x07},
	{0x485C, 0x01},
	{0x485D, 0x15},
	{0x485E, 0x01},
	{0x485F, 0x14},
	{0x4860, 0x01},
	{0x4861, 0x3E},
	{0x4862, 0x01},
	{0x4863, 0x35},
	{0x4864, 0x01},
	{0x4865, 0x8A},
	{0x4866, 0x01},
	{0x4867, 0x72},
	{0x4868, 0x01},
	{0x4869, 0xF0},
	{0x486A, 0x01},
	{0x486B, 0xC8},
	{0x486C, 0x02},
	{0x486D, 0x04},
	{0x486E, 0x01},
	{0x486F, 0xCF},
	{0x4870, 0x01},
	{0x4871, 0x94},
	{0x4872, 0x01},
	{0x4873, 0x74},
	{0x4874, 0x01},
	{0x4875, 0x3E},
	{0x4876, 0x01},
	{0x4877, 0x2D},
	{0x4878, 0x01},
	{0x4879, 0x0E},
	{0x487A, 0x01},
	{0x487B, 0x08},
	{0x487C, 0x01},
	{0x487D, 0x00},
	{0x487E, 0x01},
	{0x487F, 0x00},
	{0x4880, 0x01},
	{0x4881, 0x0D},
	{0x4882, 0x01},
	{0x4883, 0x0B},
	{0x4884, 0x01},
	{0x4885, 0x33},
	{0x4886, 0x01},
	{0x4887, 0x2A},
	{0x4888, 0x01},
	{0x4889, 0x7D},
	{0x488A, 0x01},
	{0x488B, 0x69},
	{0x488C, 0x01},
	{0x488D, 0xE3},
	{0x488E, 0x01},
	{0x488F, 0xBF},
	{0x4890, 0x02},
	{0x4891, 0x14},
	{0x4892, 0x01},
	{0x4893, 0xDF},
	{0x4894, 0x01},
	{0x4895, 0xA8},
	{0x4896, 0x01},
	{0x4897, 0x85},
	{0x4898, 0x01},
	{0x4899, 0x54},
	{0x489A, 0x01},
	{0x489B, 0x3F},
	{0x489C, 0x01},
	{0x489D, 0x22},
	{0x489E, 0x01},
	{0x489F, 0x16},
	{0x48A0, 0x01},
	{0x48A1, 0x11},
	{0x48A2, 0x01},
	{0x48A3, 0x0C},
	{0x48A4, 0x01},
	{0x48A5, 0x20},
	{0x48A6, 0x01},
	{0x48A7, 0x19},
	{0x48A8, 0x01},
	{0x48A9, 0x47},
	{0x48AA, 0x01},
	{0x48AB, 0x3B},
	{0x48AC, 0x01},
	{0x48AD, 0x90},
	{0x48AE, 0x01},
	{0x48AF, 0x77},
	{0x48B0, 0x01},
	{0x48B1, 0xF7},
	{0x48B2, 0x01},
	{0x48B3, 0xCE},
	{0x48B4, 0x02},
	{0x48B5, 0x58},
	{0x48B6, 0x02},
	{0x48B7, 0x13},
	{0x48B8, 0x01},
	{0x48B9, 0xD7},
	{0x48BA, 0x01},
	{0x48BB, 0xA9},
	{0x48BC, 0x01},
	{0x48BD, 0x8B},
	{0x48BE, 0x01},
	{0x48BF, 0x6B},
	{0x48C0, 0x01},
	{0x48C1, 0x55},
	{0x48C2, 0x01},
	{0x48C3, 0x42},
	{0x48C4, 0x01},
	{0x48C5, 0x41},
	{0x48C6, 0x01},
	{0x48C7, 0x35},
	{0x48C8, 0x01},
	{0x48C9, 0x4F},
	{0x48CA, 0x01},
	{0x48CB, 0x41},
	{0x48CC, 0x01},
	{0x48CD, 0x7D},
	{0x48CE, 0x01},
	{0x48CF, 0x67},
	{0x48D0, 0x01},
	{0x48D1, 0xBD},
	{0x48D2, 0x01},
	{0x48D3, 0x9A},
	{0x48D4, 0x02},
	{0x48D5, 0x31},
	{0x48D6, 0x01},
	{0x48D7, 0xFC},
	{0x48D8, 0x03},
	{0x48D9, 0x66},
	{0x48DA, 0x02},
	{0x48DB, 0xF2},
	{0x48DC, 0x02},
	{0x48DD, 0x2A},
	{0x48DE, 0x01},
	{0x48DF, 0xEE},
	{0x48E0, 0x01},
	{0x48E1, 0xDC},
	{0x48E2, 0x01},
	{0x48E3, 0xB0},
	{0x48E4, 0x01},
	{0x48E5, 0xAD},
	{0x48E6, 0x01},
	{0x48E7, 0x8C},
	{0x48E8, 0x01},
	{0x48E9, 0x9B},
	{0x48EA, 0x01},
	{0x48EB, 0x7F},
	{0x48EC, 0x01},
	{0x48ED, 0xA6},
	{0x48EE, 0x01},
	{0x48EF, 0x89},
	{0x48F0, 0x01},
	{0x48F1, 0xCC},
	{0x48F2, 0x01},
	{0x48F3, 0xA7},
	{0x48F4, 0x02},
	{0x48F5, 0x0C},
	{0x48F6, 0x01},
	{0x48F7, 0xDB},
	{0x48F8, 0x02},
	{0x48F9, 0xF8},
	{0x48FA, 0x02},
	{0x48FB, 0xA3},
	{0x48FC, 0x03},
	{0x48FD, 0x1A},
	{0x48FE, 0x02},
	{0x48FF, 0xCA},
	{0x4900, 0x01},
	{0x4901, 0xEA},
	{0x4902, 0x01},
	{0x4903, 0xBC},
	{0x4904, 0x01},
	{0x4905, 0xA2},
	{0x4906, 0x01},
	{0x4907, 0x80},
	{0x4908, 0x01},
	{0x4909, 0x79},
	{0x490A, 0x01},
	{0x490B, 0x60},
	{0x490C, 0x01},
	{0x490D, 0x69},
	{0x490E, 0x01},
	{0x490F, 0x54},
	{0x4910, 0x01},
	{0x4911, 0x75},
	{0x4912, 0x01},
	{0x4913, 0x5E},
	{0x4914, 0x01},
	{0x4915, 0x9A},
	{0x4916, 0x01},
	{0x4917, 0x7E},
	{0x4918, 0x01},
	{0x4919, 0xCC},
	{0x491A, 0x01},
	{0x491B, 0xA9},
	{0x491C, 0x02},
	{0x491D, 0x76},
	{0x491E, 0x02},
	{0x491F, 0x3F},
	{0x4920, 0x02},
	{0x4921, 0x24},
	{0x4922, 0x01},
	{0x4923, 0xE7},
	{0x4924, 0x01},
	{0x4925, 0xA5},
	{0x4926, 0x01},
	{0x4927, 0x7D},
	{0x4928, 0x01},
	{0x4929, 0x60},
	{0x492A, 0x01},
	{0x492B, 0x46},
	{0x492C, 0x01},
	{0x492D, 0x34},
	{0x492E, 0x01},
	{0x492F, 0x25},
	{0x4930, 0x01},
	{0x4931, 0x26},
	{0x4932, 0x01},
	{0x4933, 0x1C},
	{0x4934, 0x01},
	{0x4935, 0x35},
	{0x4936, 0x01},
	{0x4937, 0x28},
	{0x4938, 0x01},
	{0x4939, 0x5A},
	{0x493A, 0x01},
	{0x493B, 0x45},
	{0x493C, 0x01},
	{0x493D, 0x95},
	{0x493E, 0x01},
	{0x493F, 0x73},
	{0x4940, 0x01},
	{0x4941, 0xF2},
	{0x4942, 0x01},
	{0x4943, 0xC8},
	{0x4944, 0x01},
	{0x4945, 0xE4},
	{0x4946, 0x01},
	{0x4947, 0xAF},
	{0x4948, 0x01},
	{0x4949, 0x84},
	{0x494A, 0x01},
	{0x494B, 0x60},
	{0x494C, 0x01},
	{0x494D, 0x3B},
	{0x494E, 0x01},
	{0x494F, 0x27},
	{0x4950, 0x01},
	{0x4951, 0x12},
	{0x4952, 0x01},
	{0x4953, 0x0B},
	{0x4954, 0x01},
	{0x4955, 0x06},
	{0x4956, 0x01},
	{0x4957, 0x04},
	{0x4958, 0x01},
	{0x4959, 0x14},
	{0x495A, 0x01},
	{0x495B, 0x0D},
	{0x495C, 0x01},
	{0x495D, 0x36},
	{0x495E, 0x01},
	{0x495F, 0x26},
	{0x4960, 0x01},
	{0x4961, 0x76},
	{0x4962, 0x01},
	{0x4963, 0x58},
	{0x4964, 0x01},
	{0x4965, 0xCA},
	{0x4966, 0x01},
	{0x4967, 0xA3},
	{0x4968, 0x01},
	{0x4969, 0xD6},
	{0x496A, 0x01},
	{0x496B, 0xA2},
	{0x496C, 0x01},
	{0x496D, 0x78},
	{0x496E, 0x01},
	{0x496F, 0x57},
	{0x4970, 0x01},
	{0x4971, 0x31},
	{0x4972, 0x01},
	{0x4973, 0x20},
	{0x4974, 0x01},
	{0x4975, 0x0A},
	{0x4976, 0x01},
	{0x4977, 0x04},
	{0x4978, 0x01},
	{0x4979, 0x01},
	{0x497A, 0x01},
	{0x497B, 0x00},
	{0x497C, 0x01},
	{0x497D, 0x0C},
	{0x497E, 0x01},
	{0x497F, 0x08},
	{0x4980, 0x01},
	{0x4981, 0x2D},
	{0x4982, 0x01},
	{0x4983, 0x20},
	{0x4984, 0x01},
	{0x4985, 0x6B},
	{0x4986, 0x01},
	{0x4987, 0x4F},
	{0x4988, 0x01},
	{0x4989, 0xC3},
	{0x498A, 0x01},
	{0x498B, 0x9A},
	{0x498C, 0x01},
	{0x498D, 0xE5},
	{0x498E, 0x01},
	{0x498F, 0xAE},
	{0x4990, 0x01},
	{0x4991, 0x89},
	{0x4992, 0x01},
	{0x4993, 0x62},
	{0x4994, 0x01},
	{0x4995, 0x42},
	{0x4996, 0x01},
	{0x4997, 0x2C},
	{0x4998, 0x01},
	{0x4999, 0x18},
	{0x499A, 0x01},
	{0x499B, 0x0F},
	{0x499C, 0x01},
	{0x499D, 0x0B},
	{0x499E, 0x01},
	{0x499F, 0x06},
	{0x49A0, 0x01},
	{0x49A1, 0x18},
	{0x49A2, 0x01},
	{0x49A3, 0x10},
	{0x49A4, 0x01},
	{0x49A5, 0x3C},
	{0x49A6, 0x01},
	{0x49A7, 0x29},
	{0x49A8, 0x01},
	{0x49A9, 0x7A},
	{0x49AA, 0x01},
	{0x49AB, 0x5B},
	{0x49AC, 0x01},
	{0x49AD, 0xCF},
	{0x49AE, 0x01},
	{0x49AF, 0xA6},
	{0x49B0, 0x02},
	{0x49B1, 0x1B},
	{0x49B2, 0x01},
	{0x49B3, 0xDC},
	{0x49B4, 0x01},
	{0x49B5, 0xAF},
	{0x49B6, 0x01},
	{0x49B7, 0x7F},
	{0x49B8, 0x01},
	{0x49B9, 0x6E},
	{0x49BA, 0x01},
	{0x49BB, 0x4D},
	{0x49BC, 0x01},
	{0x49BD, 0x42},
	{0x49BE, 0x01},
	{0x49BF, 0x2E},
	{0x49C0, 0x01},
	{0x49C1, 0x33},
	{0x49C2, 0x01},
	{0x49C3, 0x23},
	{0x49C4, 0x01},
	{0x49C5, 0x40},
	{0x49C6, 0x01},
	{0x49C7, 0x2D},
	{0x49C8, 0x01},
	{0x49C9, 0x66},
	{0x49CA, 0x01},
	{0x49CB, 0x4A},
	{0x49CC, 0x01},
	{0x49CD, 0x9C},
	{0x49CE, 0x01},
	{0x49CF, 0x75},
	{0x49D0, 0x01},
	{0x49D1, 0xFC},
	{0x49D2, 0x01},
	{0x49D3, 0xCE},
	{0x49D4, 0x03},
	{0x49D5, 0x00},
	{0x49D6, 0x02},
	{0x49D7, 0x99},
	{0x49D8, 0x01},
	{0x49D9, 0xF3},
	{0x49DA, 0x01},
	{0x49DB, 0xB9},
	{0x49DC, 0x01},
	{0x49DD, 0xB3},
	{0x49DE, 0x01},
	{0x49DF, 0x83},
	{0x49E0, 0x01},
	{0x49E1, 0x8C},
	{0x49E2, 0x01},
	{0x49E3, 0x68},
	{0x49E4, 0x01},
	{0x49E5, 0x7D},
	{0x49E6, 0x01},
	{0x49E7, 0x5C},
	{0x49E8, 0x01},
	{0x49E9, 0x87},
	{0x49EA, 0x01},
	{0x49EB, 0x65},
	{0x49EC, 0x01},
	{0x49ED, 0xA5},
	{0x49EE, 0x01},
	{0x49EF, 0x7E},
	{0x49F0, 0x01},
	{0x49F1, 0xDA},
	{0x49F2, 0x01},
	{0x49F3, 0xAC},
	{0x49F4, 0x02},
	{0x49F5, 0xA3},
	{0x49F6, 0x02},
	{0x49F7, 0x5B},
	{RAM_SEL_TOGGLE_BYTE_ADDR, 0x01}, //lsc table 1 is set to valid
};           
#endif

static struct msm_camera_i2c_reg_conf imx135_prev_settings[] = {
	{0x011E, 0x06},	/* Clock Setting */
	{0x011F, 0xC0},
	{0x0301, 0x05},
	{0x0303, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x0305, 0x01},
#else
	{0x0305, 0x06},
#endif
	{0x0309, 0x05},
	{0x030B, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x030C, 0x00},
	{0x030D, 0x76},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x030C, 0x02},
	{0x030D, 0xC7},
#else
	{0x030C, 0x02},
	{0x030D, 0xC2},
#endif
	{0x030E, 0x01},
	{0x3209, 0x02},
	{0x3408, 0x0F},
	{0x3A06, 0x11},
	{0x0108, 0x03},	/* Mode setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x01},
	{0x0391, 0x12},
	{0x0392, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x4082, 0x01},
	{0x4083, 0x01},
	{0x7006, 0x04},
	{0x0700, 0x00},	/* OptionalFunction setting */
//	{0x3A63, 0x00},
	{0x4100, 0xE8},
	{0x4203, 0xFF},
	{0x4344, 0x00},
	{0x441C, 0x00}, /* tone mode */
	{0x0340, 0x08},	/* Size setting	*/
	{0x0341, 0xE4},
#if defined(CONFIG_MACH_TDN)
	{0x0342, 0x11},
	{0x0343, 0xEC},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x0342, 0x12},
	{0x0343, 0x00},
#else
	{0x0342, 0x11},
	{0x0343, 0xDE},
#endif
	{0x0344, 0x00},
	{0x0345, 0x04},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x10},
	{0x0349, 0x6B},
	{0x034A, 0x0C},
	{0x034B, 0x2F},
	{0x034C, 0x10},
	{0x034D, 0x68},
	{0x034E, 0x06},
	{0x034F, 0x18},
	{0x0350, 0x00},
	{0x0351, 0x00},
	{0x0352, 0x00},
	{0x0353, 0x00},
	{0x0354, 0x10},
	{0x0355, 0x68},
	{0x0356, 0x06},
	{0x0357, 0x18},
	{0x301D, 0x30},
	{0x3310, 0x10},
	{0x3311, 0x68},
	{0x3312, 0x06},
	{0x3313, 0x18},
	{0x331C, 0x01},
#if defined(CONFIG_MACH_DECKARD_AS96) || defined(CONFIG_MACH_TDN)
	{0x331D, 0xB8},
#else
	{0x331D, 0x90},
#endif
	{0x4084, 0x00},
	{0x4085, 0x00},
	{0x4086, 0x00},
	{0x4087, 0x00},
	{0x4400, 0x00},
	{0x0830, 0x7F},	/* Global Timing Setting */
	{0x0831, 0x37},
	{0x0832, 0x67},
	{0x0833, 0x37},
	{0x0834, 0x37},
	{0x0835, 0x3F},
	{0x0836, 0xC7},
	{0x0837, 0x3F},
	{0x0839, 0x1F},
	{0x083A, 0x17},
	{0x083B, 0x02},
#if 0
	{0x0202, 0x08},	/* Integration Time Setting */
	{0x0203, 0xE0},
	{0x0205, 0x00},	/* Gain Setting	*/
#else
	{0x0202, PIL_H_PREV},	/* Integration Time Setting */
	{0x0203, PIL_L_PREV},
	{0x0205, P_INIT_GAIN_PREV},	/* Gain Setting	*/
#endif
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x0230, 0x00},	/* HDR Setting */
	{0x0231, 0x00},
	{0x0233, 0x00},
	{0x0234, 0x00},
	{0x0235, 0x40},
	{0x0238, 0x00},
	{0x0239, 0x04},
	{0x023B, 0x00},
	{0x023C, 0x01},
	{0x33B0, 0x04},
	{0x33B1, 0x00},
	{0x33B3, 0x00},
	{0x33B4, 0x01},
	{0x3800, 0x00},
};

static struct msm_camera_i2c_reg_conf imx135_fast_prev_settings[] = {
	{0x011E, 0x06},	/* Clock Setting */
	{0x011F, 0xC0},
	{0x0301, 0x05},
	{0x0303, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x0305, 0x01},
#else
	{0x0305, 0x06},
#endif
	{0x0309, 0x05},
	{0x030B, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x030C, 0x00},
	{0x030D, 0x76},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x030C, 0x02},
	{0x030D, 0xC7},
#else
	{0x030C, 0x02},
	{0x030D, 0xC2},
#endif
	{0x030E, 0x01},
	{0x3209, 0x02},
	{0x3408, 0x0F},
	{0x3A06, 0x11},
	{0x0108, 0x03},	/* Mode setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x01},
	{0x0391, 0x14},
	{0x0392, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x4082, 0x01},
	{0x4083, 0x01},
	{0x7006, 0x04},
	{0x0700, 0x00},	/* OptionalFunction setting */
//	{0x3A63, 0x00},
	{0x4100, 0xE8},
	{0x4203, 0xFF},
	{0x4344, 0x00},
	{0x441C, 0x00}, /* tone mode */
	{0x0340, 0x03},	/* Size setting	*/
	{0x0341, 0x30},
#if defined(CONFIG_MACH_TDN)
	{0x0342, 0x11},
	{0x0343, 0xEC},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x0342, 0x12},
	{0x0343, 0x00},
#else
	{0x0342, 0x11},
	{0x0343, 0xDE},
#endif
	{0x0344, 0x00},
	{0x0345, 0x04},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x10},
	{0x0349, 0x6B},
	{0x034A, 0x0C},
	{0x034B, 0x2F},
	{0x034C, 0x10},
	{0x034D, 0x68},
	{0x034E, 0x03},
	{0x034F, 0x0C},
	{0x0350, 0x00},
	{0x0351, 0x00},
	{0x0352, 0x00},
	{0x0353, 0x00},
	{0x0354, 0x10},
	{0x0355, 0x68},
	{0x0356, 0x03},
	{0x0357, 0x0C},
	{0x301D, 0x30},
	{0x3310, 0x10},
	{0x3311, 0x68},
	{0x3312, 0x03},
	{0x3313, 0x0C},
	{0x331C, 0x01},
#if defined(CONFIG_MACH_DECKARD_AS96) || defined(CONFIG_MACH_TDN)
	{0x331D, 0xB8},
#else
	{0x331D, 0x90},
#endif
	{0x4084, 0x00},
	{0x4085, 0x00},
	{0x4086, 0x00},
	{0x4087, 0x00},
	{0x4400, 0x00},
	{0x0830, 0x7F},	/* Global Timing Setting */
	{0x0831, 0x37},
	{0x0832, 0x67},
	{0x0833, 0x37},
	{0x0834, 0x37},
	{0x0835, 0x3F},
	{0x0836, 0xC7},
	{0x0837, 0x3F},
	{0x0839, 0x1F},
	{0x083A, 0x17},
	{0x083B, 0x02},
	{0x0202, 0x03},	/* Integration Time Setting */
	{0x0203, 0x2C},
	{0x0205, 0x00},	/* Gain Setting	*/
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x0230, 0x00},	/* HDR Setting */
	{0x0231, 0x00},
	{0x0233, 0x00},
	{0x0234, 0x00},
	{0x0235, 0x40},
	{0x0238, 0x00},
	{0x0239, 0x04},
	{0x023B, 0x00},
	{0x023C, 0x01},
	{0x33B0, 0x04},
	{0x33B1, 0x00},
	{0x33B3, 0x00},
	{0x33B4, 0x01},
	{0x3800, 0x00},
	{RAM_SEL_TOGGLE_BYTE_ADDR, 0x01}, //lsc table 1 is set to valid
};

static struct msm_camera_i2c_reg_conf imx135_hdr_settings[] = {
	{0x011E, 0x06},	/* Clock Setting */
	{0x011F, 0xC0},
	{0x0301, 0x05},
	{0x0303, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x0305, 0x01},
#else
	{0x0305, 0x06},
#endif
	{0x0309, 0x05},
	{0x030B, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x030C, 0x00},
	{0x030D, 0x76},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x030C, 0x02},
	{0x030D, 0xC7},
#else
	{0x030C, 0x02},
	{0x030D, 0xC2},
#endif
	{0x030E, 0x01},
	{0x3209, 0x02},
	{0x3408, 0x0F},
	{0x3A06, 0x11},
	{0x0108, 0x03},	/* Mode setting */
	{0x0112, 0x0E},
	{0x0113, 0x0A},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0391, 0x11},
	{0x0392, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x4082, 0x01},
	{0x4083, 0x01},
	{0x7006, 0x04},
	{0x0700, 0x01},	/* OptionalFunction setting */  /* LSC ON */
//	{0x3A63, 0x00},
	{0x4100, 0xF8},
	{0x4203, 0xFF},
	{0x4344, 0x00},
	{0x441C, TRMODE}, /* tone mode */
	{0x0340, 0x0C},	/* Size setting	*/
	{0x0341, 0x54},
#if defined(CONFIG_MACH_TDN)
	{0x0342, 0x11},
	{0x0343, 0xEC},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x0342, 0x12},
	{0x0343, 0x00},
#else
	{0x0342, 0x11},
	{0x0343, 0xDE},
#endif
	{0x0344, 0x00},
	{0x0345, 0x04},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x10},
	{0x0349, 0x6B},
	{0x034A, 0x0C},
	{0x034B, 0x2F},
	{0x034C, 0x10},
	{0x034D, 0x68},
	{0x034E, 0x06},
	{0x034F, 0x18},
	{0x0350, 0x00},
	{0x0351, 0x00},
	{0x0352, 0x00},
	{0x0353, 0x00},
	{0x0354, 0x10},
	{0x0355, 0x68},
	{0x0356, 0x06},
	{0x0357, 0x18},
	{0x301D, 0x30},
	{0x3310, 0x10},
	{0x3311, 0x68},
	{0x3312, 0x06},
	{0x3313, 0x18},
	{0x331C, 0x09},
#if defined(CONFIG_MACH_DECKARD_AS96) || defined(CONFIG_MACH_TDN)
	{0x331D, 0x9C},
#else
	{0x331D, 0x60},
#endif
	{0x4084, 0x00},
	{0x4085, 0x00},
	{0x4086, 0x00},
	{0x4087, 0x00},
	{0x4400, 0x00},
	{0x0830, 0x7F},	/* Global Timing Setting */
	{0x0831, 0x37},
	{0x0832, 0x67},
	{0x0833, 0x37},
	{0x0834, 0x37},
	{0x0835, 0x3F},
	{0x0836, 0xC7},
	{0x0837, 0x3F},
	{0x0839, 0x1F},
	{0x083A, 0x17},
	{0x083B, 0x02},
	{0x0202, 0x0C},	/* Integration Time Setting */
	{0x0203, 0x50},
	{0x0205, 0x00},	/* Gain Setting	*/
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x0230, 0x00},	/* HDR Setting */
	{0x0231, 0x00},
	{0x0233, 0x00},
	{0x0234, 0x00},
	{0x0235, 0x40},
	{0x0238, 0x01}, /*Direct 1 / Auto 0*/
#ifdef RATE_8_to_1
	{0x0239, 0x04},
#else  //16:1
	{0x0239, 0x08},
#endif
#ifdef STATS_NO_LIM
	{0x4401, 0x3F},
#else
	{0x4401, 0x03}, // STATS Limit to 1023
#endif
	{0x023B, 0x00},
	{0x023C, 0x01},
	//{0x33B0, 0x10},
	//{0x33B1, 0x68}, //2096 stats data
	{0x33B0, 0x08},
	{0x33B1, 0x30}, //2096 stats data
	{0x33B3, 0x01}, // 1 line 
	{0x33B4, 0x00}, //VC=0-0x35 DT
	{0x3800, 0x00}, //0-MIPI
};

static struct msm_camera_i2c_reg_conf imx135_hdr_16_9_settings[] = {
	{0x011E, 0x06},	/* Clock Setting */
	{0x011F, 0xC0},
	{0x0301, 0x05},
	{0x0303, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x0305, 0x01},
#else
	{0x0305, 0x06},
#endif
	{0x0309, 0x05},
	{0x030B, 0x01},
#if defined(CONFIG_MACH_TDN)
	{0x030C, 0x00},
	{0x030D, 0x76},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x030C, 0x02},
	{0x030D, 0xC7},
#else
	{0x030C, 0x02},
	{0x030D, 0xC2},
#endif
	{0x030E, 0x01},
	{0x3209, 0x02},
	{0x3408, 0x0F},
	{0x3A06, 0x11},
	{0x0108, 0x03},	/* Mode setting */
	{0x0112, 0x0E},
	{0x0113, 0x0A},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0391, 0x11},
	{0x0392, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x4082, 0x01},
	{0x4083, 0x01},
	{0x7006, 0x04},
	{0x0700, 0x01},	/* OptionalFunction setting */ /* LSC ON */
//	{0x3A63, 0x00},
	{0x4100, 0xF8},
	{0x4203, 0xFF},
	{0x4344, 0x00},
	{0x441C, TRMODE}, /* tone mode */
	{0x0340, 0x09},	/* Size setting	*/
	{0x0341, 0x0A},
#if defined(CONFIG_MACH_TDN)
	{0x0342, 0x11},
	{0x0343, 0xEC},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{0x0342, 0x12},
	{0x0343, 0x00},
#else
	{0x0342, 0x11},
	{0x0343, 0xDE},
#endif
	{0x0344, 0x00},
	{0x0345, 0x04},
	{0x0346, 0x01},
	{0x0347, 0xA8},
	{0x0348, 0x10},
	{0x0349, 0x6B},
	{0x034A, 0x0A},
	{0x034B, 0x8B},
	{0x034C, 0x10},
	{0x034D, 0x68},
	{0x034E, 0x04},
	{0x034F, 0x72},
	{0x0350, 0x00},
	{0x0351, 0x00},
	{0x0352, 0x00},
	{0x0353, 0x00},
	{0x0354, 0x10},
	{0x0355, 0x68},
	{0x0356, 0x04},
	{0x0357, 0x72},
	{0x301D, 0x30},
	{0x3310, 0x10},
	{0x3311, 0x68},
	{0x3312, 0x04},
	{0x3313, 0x72},
	{0x331C, 0x09},
#if defined(CONFIG_MACH_DECKARD_AS96) || defined(CONFIG_MACH_TDN)
	{0x331D, 0x9C},
#else
	{0x331D, 0x60},
#endif
	{0x4084, 0x00},
	{0x4085, 0x00},
	{0x4086, 0x00},
	{0x4087, 0x00},
	{0x4400, 0x00}, /* [0] 16 x 16 (default)[1] 8 x 8[2] 4 x 4[3] 1 x 1 */
	{0x0830, 0x7F},	/* Global Timing Setting */
	{0x0831, 0x37},
	{0x0832, 0x67},
	{0x0833, 0x37},
	{0x0834, 0x37},
	{0x0835, 0x3F},
	{0x0836, 0xC7},
	{0x0837, 0x3F},
	{0x0839, 0x1F},
	{0x083A, 0x17},
	{0x083B, 0x02},
	{0x0202, 0x09},	/* Integration Time Setting */
	{0x0203, 0x06},
	{0x0205, 0x00},	/* Gain Setting	*/
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x0230, 0x00},	/* HDR Setting */
	{0x0231, 0x00},
	{0x0233, 0x00},
	{0x0234, 0x00},
	{0x0235, 0x40},
	{0x0238, 0x01}, /*Direct 1 / Auto 0*/
#ifdef RATE_8_to_1
	{0x0239, 0x04},
#else  //16:1
	{0x0239, 0x08},
#endif
#ifdef STATS_NO_LIM
	{0x4401, 0x3F},
#else
	{0x4401, 0x03}, // STATS Limit to 1023
#endif
	{0x023B, 0x00},
	{0x023C, 0x01},
	//{0x33B0, 0x10},
	//{0x33B1, 0x68}, //2096 stats data
	{0x33B0, 0x08},
	{0x33B1, 0x30}, //2096 stats data
	{0x33B3, 0x01}, // 1 line 
	{0x33B4, 0x00}, //VC=0-0x35 DT
	{0x3800, 0x00}, //0-MIPI
};

static struct v4l2_subdev_info imx135_subdev_info[] = {
	{
	.code		= V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace	= V4L2_COLORSPACE_JPEG,
	.fmt		= 1,
	.order		= 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx135_init_conf[] = {
	{&imx135_recommend_settings[0],
	ARRAY_SIZE(imx135_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
#if 0
	{&imx135_LSCTable_settings[0],
	ARRAY_SIZE(imx135_LSCTable_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
#endif
};

static struct msm_camera_i2c_conf_array imx135_confs[] = {
	{&imx135_snap_settings[0],
	ARRAY_SIZE(imx135_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx135_prev_settings[0],
	ARRAY_SIZE(imx135_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx135_fast_prev_settings[0],
	ARRAY_SIZE(imx135_fast_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx135_hdr_settings[0],
	ARRAY_SIZE(imx135_hdr_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx135_hdr_16_9_settings[0],
	ARRAY_SIZE(imx135_hdr_16_9_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t imx135_dimensions[] = {
#if defined(CONFIG_MACH_TDN)
	/* RES0 snapshot(FULL SIZE) */
	{
		.x_output = 4200,
		.y_output = 3120,
		.line_length_pclk = 4588,
		.frame_length_lines = 3470,
		.vt_pixel_clk = 318600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES1 4:3 preview(1H 1/2V SIZE) */
	{
		.x_output = 4200,
		.y_output = 1560,
		.line_length_pclk = 4588,
		.frame_length_lines = 2276,
		.vt_pixel_clk = 318600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES2 fast preview */
	{
		.x_output = 4200,
		.y_output = 780,
		.line_length_pclk = 4588,
		.frame_length_lines = 816,
		.vt_pixel_clk = 318600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES3 4:3 HDR mode */
	{
		.x_output = 4200,
		.y_output = 1560,
		.line_length_pclk = 4588,
		.frame_length_lines = 3156,
		.vt_pixel_clk = 318600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES4 16:9 HDR mode */
	{
		.x_output = 4200,
		.y_output = 1138,
		.line_length_pclk = 4588,
		.frame_length_lines = 2314,
		.vt_pixel_clk = 318600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	/* RES0 snapshot(FULL SIZE) */
	{
		.x_output = 4200,
		.y_output = 3120,
		.line_length_pclk = 4608,
		.frame_length_lines = 3470,
		.vt_pixel_clk = 319950000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES1 4:3 preview(1H 1/2V SIZE) */
	{
		.x_output = 4200,
		.y_output = 1560,
		.line_length_pclk = 4608,
		.frame_length_lines = 2276,
		.vt_pixel_clk = 319950000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES2 fast preview */
	{
		.x_output = 4200,
		.y_output = 780,
		.line_length_pclk = 4608,
		.frame_length_lines = 816,
		.vt_pixel_clk = 319950000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES3 4:3 HDR mode */
	{
		.x_output = 4200,
		.y_output = 1560,
		.line_length_pclk = 4608,
		.frame_length_lines = 3156,
		.vt_pixel_clk = 319950000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES4 16:9 HDR mode */
	{
		.x_output = 4200,
		.y_output = 1138,
		.line_length_pclk = 4608,
		.frame_length_lines = 2314,
		.vt_pixel_clk = 319950000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
#else
	/* RES0 snapshot(FULL SIZE) */
	{
		.x_output = 4200,
		.y_output = 3120,
		.line_length_pclk = 4574,
		.frame_length_lines = 3470,
		.vt_pixel_clk = 317700000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES1 4:3 preview(1H 1/2V SIZE) */
	{
		.x_output = 4200,
		.y_output = 1560,
		.line_length_pclk = 4574,
		.frame_length_lines = 2276,
		.vt_pixel_clk = 317700000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES2 fast preview */
	{
		.x_output = 4200,
		.y_output = 780,
		.line_length_pclk = 4574,
		.frame_length_lines = 816,
		.vt_pixel_clk = 317700000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES3 4:3 HDR mode */
	{
		.x_output = 4200,
		.y_output = 1560,
		.line_length_pclk = 4574,
		.frame_length_lines = 3156,
		.vt_pixel_clk = 317700000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* RES4 16:9 HDR mode */
	{
		.x_output = 4200,
		.y_output = 1138,
		.line_length_pclk = 4574,
		.frame_length_lines = 2314,
		.vt_pixel_clk = 317700000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
#endif
};

static struct msm_sensor_output_reg_addr_t imx135_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t imx135_id_info = {
#if 0
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x0087,
#else
	.sensor_id_reg_addr = 0x0016,
	.sensor_id = 0x0135,
#endif
};

static struct msm_sensor_exp_gain_info_t imx135_exp_gain_info = {
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x205,
	.vert_offset = 4,
};

#if 1
static enum msm_camera_vreg_name_t imx135_veg_seq[] = {
	CAM_VDIG,
	CAM_VIO,
	CAM_VANA,
	CAM_VAF,
};
static enum msm_camera_vreg_name_t imx135_veg_seq_none[] = {
	CAM_VDIG,
	CAM_VANA,
	CAM_VAF,
};
#endif

static const struct i2c_device_id imx135_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx135_s_ctrl},
	{ }
};

static struct i2c_driver imx135_i2c_driver = {
	.id_table = imx135_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx135_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

#if 1
static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_675HZ},
};

typedef struct {
	uint16_t line;
	uint16_t gain;
	uint16_t digital_gain;
} imx135_exp_gain_t;

imx135_exp_gain_t imx135_exp_gain ={
	.line = P_INIT_LINE_PREV,
	.gain = P_INIT_GAIN_PREV,
	.digital_gain =  0x0100,
};

int32_t imx135_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
    #if 1
	unsigned short rev;
	enum msm_camera_vreg_name_t *vreg_seq;
	int num_vreg_seq;
    #endif
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

    #if 1
	rev = sh_boot_get_hw_revision();

	if(rev < SH_REV_CMP) {
		vreg_seq = imx135_veg_seq;
		num_vreg_seq = ARRAY_SIZE(imx135_veg_seq);
    } else {
		vreg_seq = imx135_veg_seq_none;
		num_vreg_seq = ARRAY_SIZE(imx135_veg_seq_none);
    }
    #endif

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
    #if 0
			NULL, 0, s_ctrl->reg_ptr, 1);
    #else
			vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 1);
    #endif
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
    #if 0
			NULL, 0, s_ctrl->reg_ptr, 1);
    #else
			vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 1);
    #endif
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}

	usleep(5000);

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	usleep_range(1000, 2000);

	return rc;

enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
    #if 0
			NULL, 0, s_ctrl->reg_ptr, 0);
    #else
			vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
    #endif

enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
    #if 0
		NULL, 0, s_ctrl->reg_ptr, 0);
    #else
		vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
    #endif
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
}

int32_t imx135_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
    #if 1
	unsigned short rev;
	enum msm_camera_vreg_name_t *vreg_seq;
	int num_vreg_seq;
    #endif
	CDBG("%s\n", __func__);

	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);

    #if 1
	rev = sh_boot_get_hw_revision();

	if(rev < SH_REV_CMP) {
		vreg_seq = imx135_veg_seq;
		num_vreg_seq = ARRAY_SIZE(imx135_veg_seq);
    } else {
		vreg_seq = imx135_veg_seq_none;
		num_vreg_seq = ARRAY_SIZE(imx135_veg_seq_none);
    }
    #endif

	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
    #if 0
		NULL, 0, s_ctrl->reg_ptr, 0);
    #else
		vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
    #endif
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
    #if 0
		NULL, 0, s_ctrl->reg_ptr, 0);
    #else
		vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
    #endif
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
	return 0;
}

static int imx135_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	uint32_t fl_lines;
	uint8_t offset;
	uint16_t shortshutter_gain = 0;
	uint32_t shortshutter = 0;
	uint16_t shortshutter_expratio = 8;
//	uint16_t atr_out_noise = 0;
//	uint16_t atr_out_mid = 0;

	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&imx135_mut);
	CDBG("imx135_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SHDIAG_GET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				mutex_unlock(&imx135_mut);
				return -EFAULT;
			}
			CDBG("%s:%d i2c_read addr=0x%0x\n",__func__,__LINE__,cdata.cfg.i2c_info.addr);
			rc = msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, cdata.cfg.i2c_info.addr , data, cdata.cfg.i2c_info.length);
			CDBG("%s:%d i2c_read data=0x%0x\n",__func__,__LINE__,*(unsigned char *)data);
			if (copy_to_user((void *)cdata.cfg.i2c_info.data,
				data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				mutex_unlock(&imx135_mut);
				CDBG("%s copy_to_user error\n",__func__);
				return -EFAULT;
			}
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))) {
				mutex_unlock(&imx135_mut);
				return -EFAULT;
			}
		}
		break;
	case CFG_SHDIAG_SET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				mutex_unlock(&imx135_mut);
				return -EFAULT;
			}
			if (copy_from_user(data,
				(void *)cdata.cfg.i2c_info.data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_to_user error\n",__func__);
				mutex_unlock(&imx135_mut);
				return -EFAULT;
			}
			CDBG("%s:%d i2c_write addr=0x%0x\n",__func__,__LINE__,cdata.cfg.i2c_info.addr);
			rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client, cdata.cfg.i2c_info.addr, data, cdata.cfg.i2c_info.length);
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))) {
				mutex_unlock(&imx135_mut);
				return -EFAULT;
			}
		}
		break;
	case CFG_SH_SET_EXP_GAIN:
		{
			CDBG("%s:%d CFG_SH_SET_EXP_GAIN\n",__func__,__LINE__);
			fl_lines = s_ctrl->curr_frame_length_lines;
			fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
			offset = s_ctrl->sensor_exp_gain_info->vert_offset;
			if (cdata.cfg.exp_gain.line > (fl_lines - offset))
				fl_lines = cdata.cfg.exp_gain.line + offset;

			s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);

			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
				MSM_CAMERA_I2C_WORD_DATA);
		
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, cdata.cfg.exp_gain.line,
				MSM_CAMERA_I2C_WORD_DATA);

			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->global_gain_addr, cdata.cfg.exp_gain.gain,
				MSM_CAMERA_I2C_BYTE_DATA);
				
			CDBG("%s: exposure: fl_lines,%d,line,%d,gain,%d,digital gain,%d\n",__func__,fl_lines,cdata.cfg.exp_gain.line,cdata.cfg.exp_gain.gain,cdata.cfg.exp_gain.digital_gain);
			
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

			/* For video HDR mode */
			#if 0
			if (s_ctrl->curr_res == MSM_SENSOR_RES_2) {
				uint16_t luma_10bit = cdata.cfg.exp_gain.luma_avg * 4; // max luma is 256 (8bit)
			#else
			if (s_ctrl->curr_res == MSM_SENSOR_RES_4) {
			#endif
				
				/* Short shutter update */
				msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					SHORT_GAIN_BYTE_ADDR, shortshutter_gain,
					MSM_CAMERA_I2C_BYTE_DATA);
				
				shortshutter = cdata.cfg.exp_gain.line * cdata.cfg.exp_gain.fgain / cdata.cfg.exp_gain.digital_gain * 100 / ( shortshutter_expratio * 100 + 4 );
				//shortshutter = (cdata.cfg.exp_gain.line * cdata.cfg.exp_gain.fgain) / (Q8 * shortshutter_expratio);
				
				CDBG("%s:  shortshutter pre,%d,line,%d,fgain,%d,digital gain,%d\n",__func__,shortshutter,cdata.cfg.exp_gain.line,cdata.cfg.exp_gain.fgain,cdata.cfg.exp_gain.digital_gain);
				if( shortshutter > cdata.cfg.exp_gain.line )
				{
					pr_err("%s:  shortshutter is clipped to line\n",__func__);
					shortshutter = cdata.cfg.exp_gain.line;
				}	
				CDBG("%s:  shortshutter final,%d,line,%d,fgain,%d,digital gain,%d\n",__func__,shortshutter,cdata.cfg.exp_gain.line,cdata.cfg.exp_gain.fgain,cdata.cfg.exp_gain.digital_gain);
				msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					SHORT_SHUTTER_WORD_ADDR, shortshutter,
					MSM_CAMERA_I2C_WORD_DATA);
			}

			s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
		}
		break;
	case CFG_HDR_UPDATE:
		{
			if (s_ctrl->func_tbl->
			sensor_hdr_update == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
					sensor_hdr_update(
					   s_ctrl,
					   &(cdata.cfg.hdr_update_parm));
		}
		break;
	default:
		mutex_unlock(&imx135_mut);
		return msm_sensor_config(s_ctrl, argp);
		break;
	}
	
	mutex_unlock(&imx135_mut);
	return rc;
}
#endif

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&imx135_i2c_driver);
}

static struct v4l2_subdev_core_ops imx135_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx135_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx135_subdev_ops = {
	.core = &imx135_subdev_core_ops,
	.video  = &imx135_subdev_video_ops,
};

int32_t imx135_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t gain, uint32_t line, int32_t luma_avg, uint16_t fgain)
{
	uint32_t fl_lines;
	uint8_t offset;
	uint16_t shortshutter_gain = 0;
	uint32_t shortshutter = 0;
	uint16_t shortshutter_expratio = 8;

    #if 0
	uint16_t atr_out_noise = 0;
	uint16_t atr_out_mid = 0;
    #endif

	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;
	CDBG("%s luma_avg=%d\n", __func__, luma_avg);
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_BYTE_DATA);

	/* For video HDR mode */
    #if 0
	if (s_ctrl->curr_res == MSM_SENSOR_RES_2) {
    #else
	if (s_ctrl->curr_res == MSM_SENSOR_RES_4) {
    #endif
		/* Short shutter update */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			SHORT_GAIN_BYTE_ADDR, shortshutter_gain,
			MSM_CAMERA_I2C_BYTE_DATA);

		shortshutter = (line * fgain) / (Q8 * shortshutter_expratio);
		CDBG("longtshutter =%d, shortshutter=%d, longgain =%d\n",
			line, shortshutter, gain);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			SHORT_SHUTTER_WORD_ADDR, shortshutter,
			MSM_CAMERA_I2C_WORD_DATA);

#if 0
		/* Adaptive tone curve parameters update */
		if (luma_avg < THRESHOLD_DEAD_ZONE) {
			/* change to fixed tone curve */
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				TC_SWITCH_BYTE_ADDR, 0x01,
				MSM_CAMERA_I2C_BYTE_DATA);
		} else {
			/* change to adaptive tone curve */
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					TC_SWITCH_BYTE_ADDR, 0x00,
					MSM_CAMERA_I2C_BYTE_DATA);
			if (luma_avg < THRESHOLD_0) {
				atr_out_noise = 0;
				atr_out_mid = 0;
			} else if (luma_avg < THRESHOLD_1) {
				atr_out_noise =
					INIT_ATR_OUT_NOISE *
					(luma_avg - THRESHOLD_0)/
					(THRESHOLD_1 - THRESHOLD_0);
				atr_out_mid = INIT_ATR_OUT_MID *
					(luma_avg - THRESHOLD_0)/
					(THRESHOLD_1 - THRESHOLD_0);
			} else {
				atr_out_noise = INIT_ATR_OUT_NOISE;
				atr_out_mid = INIT_ATR_OUT_MID;
			}
			atr_out_noise += ATR_OFFSET;
			atr_out_mid += ATR_OFFSET;
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				TC_OUT_NOISE_WORD_ADDR, atr_out_noise,
				MSM_CAMERA_I2C_WORD_DATA);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				TC_OUT_MID_WORD_ADDR, atr_out_mid,
				MSM_CAMERA_I2C_WORD_DATA);
		}
#endif
	}

	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t imx135_hdr_update(struct msm_sensor_ctrl_t *s_ctrl,
	struct sensor_hdr_update_parm_t *update_parm)
{
	int i;
	int32_t wb_lmt, ae_sat;
	switch (update_parm->type) {
	case SENSOR_HDR_UPDATE_AWB:
		CDBG("%s: SENSOR_HDR_UPDATE_AWB\n", __func__);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			ABS_GAIN_R_WORD_ADDR, update_parm->awb_gain_r,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			ABS_GAIN_B_WORD_ADDR, update_parm->awb_gain_b,
			MSM_CAMERA_I2C_WORD_DATA);
		CDBG("%s: awb gains updated r=0x%x, b=0x%x\n", __func__,
			 update_parm->awb_gain_r, update_parm->awb_gain_b);
		break;
	case SENSOR_HDR_UPDATE_LSC:
		/* step1: write knot points to LSC table */
#if 0
		for (i = 0; i < LSC_TABLE_LEN_BYTES; i++) {
			CDBG("lsc[%d] = %x\n", i, update_parm->lsc_table[i]);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				LSC_TABLE_START_ADDR + i,
				update_parm->lsc_table[i],
				MSM_CAMERA_I2C_BYTE_DATA);
		}
#else
		for (i = 0; i < (LSC_TABLE_LEN_BYTES / LSC_TX_MAX_SIZE); i++) {
			CDBG("%s i=%d Addr =0x%0x", __func__,
				i,LSC_TABLE_START_ADDR + LSC_TX_MAX_SIZE*i);

			msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
				LSC_TABLE_START_ADDR + (LSC_TX_MAX_SIZE*i),
				&update_parm->lsc_table[(LSC_TX_MAX_SIZE*i)],
				LSC_TX_MAX_SIZE );
		}

		if(0 < (LSC_TABLE_LEN_BYTES % LSC_TX_MAX_SIZE)) {
			CDBG("%s i=%d Addr =0x%0x", __func__,
				i,LSC_TABLE_START_ADDR + LSC_TX_MAX_SIZE*i);

			msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
				LSC_TABLE_START_ADDR + (LSC_TX_MAX_SIZE*i),
				&update_parm->lsc_table[(LSC_TX_MAX_SIZE*i)],
				(LSC_TABLE_LEN_BYTES - (LSC_TX_MAX_SIZE*i)) );
		}
#endif
		/* step2: LSC enable */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			EN_LSC_BYTE_ADDR, 0x1F, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			LSC_ENABLE_BYTE_ADDR, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
		/* step3: RAM_SEL_TOGGLE */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			RAM_SEL_TOGGLE_BYTE_ADDR, 0x01,
			MSM_CAMERA_I2C_BYTE_DATA);

		CDBG("%s: lsc table updated\n", __func__);
		break;
#if 1
	case SENSOR_HDR_UPDATE_ATR:
		CDBG("%s: SENSOR_HDR_UPDATE_ATR\n", __func__);
		if(update_parm->atr_params.tc_out_noise >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_OUT_NOISE_WORD_ADDR, update_parm->atr_params.tc_out_noise,
			MSM_CAMERA_I2C_WORD_DATA);
		if(update_parm->atr_params.tc_out_mid >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_OUT_MID_WORD_ADDR, update_parm->atr_params.tc_out_mid,
			MSM_CAMERA_I2C_WORD_DATA);
		if(update_parm->atr_params.tc_out_sat >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_OUT_SAT_WORD_ADDR, update_parm->atr_params.tc_out_sat,
			MSM_CAMERA_I2C_WORD_DATA);
		if(update_parm->atr_params.tc_noise_brate >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_NOISE_BRATE_REGADDR, update_parm->atr_params.tc_noise_brate,
			MSM_CAMERA_I2C_BYTE_DATA);
		if(update_parm->atr_params.tc_mid_brate >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_MID_BRATE_REGADDR, update_parm->atr_params.tc_mid_brate,
			MSM_CAMERA_I2C_BYTE_DATA);
		if(update_parm->atr_params.tc_sat_brate >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_SAT_BRATE_REGADDR, update_parm->atr_params.tc_sat_brate,
			MSM_CAMERA_I2C_BYTE_DATA);
		if(update_parm->atr_params.turn_off >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_SWITCH_BYTE_ADDR, update_parm->atr_params.turn_off,
			MSM_CAMERA_I2C_BYTE_DATA);
		if(update_parm->atr_params.atr_temporal >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_ATR_TEMPORAL_BYTE_ADDR, update_parm->atr_params.atr_temporal,
			MSM_CAMERA_I2C_BYTE_DATA);
		if(update_parm->atr_params.integ_ratio >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_WD_INTEG_RATIO_BYTE_ADDR, update_parm->atr_params.integ_ratio,
			MSM_CAMERA_I2C_BYTE_DATA);
		
		if(update_parm->atr_params.manual_en >= 0){
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				TC_DEBUG_SEL_BYTE_ADDR, update_parm->atr_params.manual_en,
				MSM_CAMERA_I2C_BYTE_DATA);
		}else if(update_parm->atr_params.manual_en > 0){
			if(update_parm->atr_params.manual_ave >= 0)
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				TC_DEBUG_AVE_WORD_ADDR, update_parm->atr_params.manual_ave,
				MSM_CAMERA_I2C_WORD_DATA);
			if(update_parm->atr_params.manual_min >= 0)
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				TC_DEBUG_MIN_WORD_ADDR, update_parm->atr_params.manual_min,
				MSM_CAMERA_I2C_WORD_DATA);
			if(update_parm->atr_params.manual_max >= 0)
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				TC_DEBUG_MAX_WORD_ADDR, update_parm->atr_params.manual_max,
				MSM_CAMERA_I2C_WORD_DATA);
		}

		wb_lmt = (int32_t)((1023-64) * update_parm->atr_params.integ_ratio + 64);
		ae_sat = (int32_t)((1023-64) * update_parm->atr_params.integ_ratio + 1);
		if(wb_lmt >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_WBLMT_WORD_ADDR, wb_lmt,
			MSM_CAMERA_I2C_WORD_DATA);
		if(ae_sat >= 0)
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			TC_AESAT_WORD_ADDR, ae_sat,
			MSM_CAMERA_I2C_WORD_DATA);

		CDBG("%s: tc_out=%d, %d, %d\n", __func__,
		  update_parm->atr_params.tc_out_noise,
		  update_parm->atr_params.tc_out_mid,
		  update_parm->atr_params.tc_out_sat);
		CDBG("%s: brate=%d, %d, %d turnOff=%d\n", __func__,
		  update_parm->atr_params.tc_noise_brate,
		  update_parm->atr_params.tc_mid_brate,
		  update_parm->atr_params.tc_sat_brate,
		  update_parm->atr_params.turn_off);
		CDBG("%s: manual_en=%d, ave = %d, min = %d max=%d\n", __func__,
		  update_parm->atr_params.manual_en,
		  update_parm->atr_params.manual_ave,
		  update_parm->atr_params.manual_min,
		  update_parm->atr_params.manual_max);
		CDBG("%s: atr_temporal = %d, integ_ratio = %d, blend_L = %d, blend_H = %d, afd_status = %d\n", __func__,
		  update_parm->atr_params.atr_temporal,
		  update_parm->atr_params.integ_ratio,
		  update_parm->atr_params.blend_L,
		  update_parm->atr_params.blend_H,
		  update_parm->atr_params.afd_status);
		CDBG("%s: wb_lmt=%d, ae_sat = %d\n", __func__, wb_lmt, ae_sat);
		break;
#endif
	default:
		pr_err("%s: invalid HDR update type %d\n",
			   __func__, update_parm->type);
		return -EINVAL;
	}
	return 0;
}

#if 1
void imx135_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	if (s_ctrl->curr_res >= 3 ) // videoHDR
	{
		int32_t rc = -EFAULT;
		
		uint16_t lsc_table = 0;
		rc = msm_camera_i2c_read(
		s_ctrl->sensor_i2c_client,
		0x3A63, &lsc_table,
		MSM_CAMERA_I2C_BYTE_DATA);
		
		CDBG("%s: videoHDR mode. RAM_SEL_STATUS (0x3A63) = 0x%x\n",__func__,lsc_table);
		
        if( (0x0001 & (lsc_table >> 4)) == 0 ) // if LSC Table2 is used
		{
			CDBG("%s: lsc_table 2 is used\n",__func__);
			
			CDBG("%s: set to lsc_table 1\n",__func__);
			
			// set to LSC Table1 forcely
			rc = msm_camera_i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3A63, 0x01,
			MSM_CAMERA_I2C_BYTE_DATA);
		}
    }

  msm_sensor_start_stream( s_ctrl );
}

void imx135_sensor_adjust_exp_gain(struct msm_sensor_ctrl_t *s_ctrl)
{
	
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, imx135_exp_gain.line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, imx135_exp_gain.gain,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x020E, imx135_exp_gain.digital_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x0210, imx135_exp_gain.digital_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x0212, imx135_exp_gain.digital_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x0214, imx135_exp_gain.digital_gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	
	CDBG("%s line=%d gain=%d digital_gain=%d", __func__, imx135_exp_gain.line, imx135_exp_gain.gain, imx135_exp_gain.digital_gain);

	return;
}

int32_t imx135_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	uint16_t line = 0;
	uint16_t gain = 0;
	uint16_t digital_gain = 0;

	CDBG("%s: %d \n", __func__, __LINE__);
	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);

		imx135_exp_gain.line = P_INIT_LINE_PREV;
		imx135_exp_gain.gain = P_INIT_GAIN_PREV;
		imx135_exp_gain.digital_gain = 0x0100;

	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {

    	if(s_ctrl->curr_res != MSM_SENSOR_INVALID_RES){
			msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, &line,
				MSM_CAMERA_I2C_WORD_DATA);
			msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->global_gain_addr, &gain,
				MSM_CAMERA_I2C_BYTE_DATA);
			msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
				0x020E, &digital_gain,
				MSM_CAMERA_I2C_WORD_DATA);
	
			imx135_exp_gain.line = line;
			imx135_exp_gain.gain = gain;
			imx135_exp_gain.digital_gain = digital_gain;
			
			CDBG("%s:%d line = %d, gain = %d, digi_gain = %d\n", __func__, __LINE__, line, gain, digital_gain);
    	}	

		msm_sensor_write_res_settings(s_ctrl, res);
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
			
    	if(s_ctrl->curr_res != MSM_SENSOR_INVALID_RES){
			imx135_sensor_adjust_exp_gain(s_ctrl);
		}

	}
	return rc;
}
#endif

static struct msm_sensor_fn_t imx135_func_tbl = {
#if 0
	.sensor_start_stream = msm_sensor_start_stream,
#else
	.sensor_start_stream = imx135_start_stream,
#endif
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = imx135_write_exp_gain,
	.sensor_write_snapshot_exp_gain = imx135_write_exp_gain,
#if 0
	.sensor_setting = msm_sensor_setting,
#else
	.sensor_setting = imx135_sensor_setting,
#endif
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
#if 0
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
#else
	.sensor_config = imx135_sensor_config,
	.sensor_power_up = imx135_sensor_power_up,
	.sensor_power_down = imx135_sensor_power_down,
#endif
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_hdr_update = imx135_hdr_update,
};

static struct msm_sensor_reg_t imx135_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx135_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx135_start_settings),
	.stop_stream_conf = imx135_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx135_stop_settings),
	.group_hold_on_conf = imx135_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx135_groupon_settings),
	.group_hold_off_conf = imx135_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(imx135_groupoff_settings),
	.init_settings = &imx135_init_conf[0],
	.init_size = ARRAY_SIZE(imx135_init_conf),
	.mode_settings = &imx135_confs[0],
	.output_settings = &imx135_dimensions[0],
	.num_conf = ARRAY_SIZE(imx135_confs),
};

static struct msm_sensor_ctrl_t imx135_s_ctrl = {
	.msm_sensor_reg = &imx135_regs,
	.sensor_i2c_client = &imx135_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &imx135_reg_addr,
	.sensor_id_info = &imx135_id_info,
	.sensor_exp_gain_info = &imx135_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &imx135_mut,
	.sensor_i2c_driver = &imx135_i2c_driver,
	.sensor_v4l2_subdev_info = imx135_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx135_subdev_info),
	.sensor_v4l2_subdev_ops = &imx135_subdev_ops,
	.func_tbl = &imx135_func_tbl,
#if 0
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
#else
	.clk_rate = MSM_SENSOR_MCLK_675HZ,
#endif
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Sony 13MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
