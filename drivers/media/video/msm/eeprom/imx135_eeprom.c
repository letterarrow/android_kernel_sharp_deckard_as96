/* Copyright (c) 2013, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include "msm_camera_eeprom.h"
#include "msm_camera_i2c.h"

DEFINE_MUTEX(imx135_eeprom_mutex);
static struct msm_eeprom_ctrl_t imx135_eeprom_t;
#define DPC_SIZE 128

static const struct i2c_device_id imx135_eeprom_i2c_id[] = {
	{"imx135_eeprom", (kernel_ulong_t)&imx135_eeprom_t},
	{ }
};

static struct i2c_driver imx135_eeprom_i2c_driver = {
	.id_table = imx135_eeprom_i2c_id,
	.probe  = msm_eeprom_i2c_probe,
	.remove = __exit_p(imx135_eeprom_i2c_remove),
	.driver = {
		.name = "imx135_eeprom",
	},
};

static int __init imx135_eeprom_i2c_add_driver(void)
{
	int rc = 0;
	rc = i2c_add_driver(imx135_eeprom_t.i2c_driver);
	return rc;
}

static struct v4l2_subdev_core_ops imx135_eeprom_subdev_core_ops = {
	.ioctl = msm_eeprom_subdev_ioctl,
};

static struct v4l2_subdev_ops imx135_eeprom_subdev_ops = {
	.core = &imx135_eeprom_subdev_core_ops,
};

uint8_t imx135_afcalib_data[4];
uint8_t imx135_wbcalib_data[5];
uint8_t imx135_dpccalib_data[DPC_SIZE*4];
struct msm_calib_af imx135_af_data;
struct msm_calib_wb imx135_wb_data;
struct msm_calib_dpc imx135_dpc_data;

static struct msm_camera_eeprom_info_t imx135_calib_supp_info = {
    #if 0
	{TRUE, sizeof(struct msm_calib_af), 0, 1},
	{TRUE, sizeof(struct msm_calib_wb), 1, 256},
    #else
	{FALSE, 0, 0, 1},
	{FALSE, 0, 0, 1},
    #endif
	{FALSE, 0, 0, 1},
	{TRUE, sizeof(struct msm_calib_dpc), 2, 1},
};

static struct msm_camera_eeprom_read_t imx135_eeprom_read_tbl[] = {
	{0x003B17, &imx135_afcalib_data[0], sizeof(imx135_afcalib_data), 0},
	{0x003B1B, &imx135_wbcalib_data[0], sizeof(imx135_wbcalib_data), 0},
	{0x013B04, &imx135_dpccalib_data[0], 64, 0},
	{0x023B04, &imx135_dpccalib_data[63], 64, 0},
	{0x033B04, &imx135_dpccalib_data[127], 64, 0},
	{0x043B04, &imx135_dpccalib_data[191], 64, 0},
	{0x053B04, &imx135_dpccalib_data[255], 64, 0},
	{0x063B04, &imx135_dpccalib_data[319], 64, 0},
	{0x073B04, &imx135_dpccalib_data[383], 64, 0},
	{0x083B04, &imx135_dpccalib_data[447], 64, 0},
};

static struct msm_camera_eeprom_data_t imx135_eeprom_data_tbl[] = {
	{&imx135_af_data, sizeof(struct msm_calib_af)},
	{&imx135_wb_data, sizeof(struct msm_calib_wb)},
	{&imx135_dpc_data, sizeof(struct msm_calib_dpc)},
};

static void imx135_format_dpcdata(void)
{
	uint16_t i;
	uint16_t msb_xcord, lsb_xcord, msb_ycord, lsb_ycord, count = 0;

	for (i = 0; i < DPC_SIZE; i++) {
		msb_xcord = (imx135_dpccalib_data[i*4] & 0x001F);
		lsb_xcord = imx135_dpccalib_data[i*4+1];
		msb_ycord = (imx135_dpccalib_data[i*4+2] & 0x000F);
		lsb_ycord = imx135_dpccalib_data[i*4+3];
		imx135_dpc_data.snapshot_coord[i].x =
			(msb_xcord << 8) | lsb_xcord;
		imx135_dpc_data.snapshot_coord[i].y =
			(msb_ycord << 8) | lsb_ycord;
	}

	for (i = 0; i < DPC_SIZE; i++)
		if (!((imx135_dpc_data.snapshot_coord[i].x == 0) &&
			(imx135_dpc_data.snapshot_coord[i].y == 0)))
				count++;

	imx135_dpc_data.validcount = count;

	CDBG("%s count = %d\n", __func__, count);
	for (i = 0; i < count; i++)
		CDBG("snapshot_dpc_cord[%d] X= %d Y = %d\n",
		i, imx135_dpc_data.snapshot_coord[i].x,
		imx135_dpc_data.snapshot_coord[i].y);
		
#if !defined(CONFIG_MACH_LYNX_DL35) || !defined(CONFIG_MACH_DECKARD_AS96)
	/* sensor rotate calculation start */
	for (i = 0; i < count; i++){
		imx135_dpc_data.snapshot_coord[i].x = 4200- 1 - imx135_dpc_data.snapshot_coord[i].x;
		imx135_dpc_data.snapshot_coord[i].y = 3120- 1 - imx135_dpc_data.snapshot_coord[i].y;
		CDBG("snapshot_dpc_cord(rotated)[%d] X= %d Y = %d\n",
		i, imx135_dpc_data.snapshot_coord[i].x,
		imx135_dpc_data.snapshot_coord[i].y);
	}
	/* sensor rotate calculation end */
#endif

/*Reg B*/
	for (i = 0; i < count; i++) {
		imx135_dpc_data.preview_coord[i].x =
			imx135_dpc_data.snapshot_coord[i].x;
		imx135_dpc_data.preview_coord[i].y =
			((imx135_dpc_data.snapshot_coord[i].y / 4)*2)
		+ (imx135_dpc_data.snapshot_coord[i].y % 2);
		CDBG("prview_dpc_cord[%d] X= %d Y = %d\n",
		i, imx135_dpc_data.preview_coord[i].x,
		imx135_dpc_data.preview_coord[i].y);
	}

/*Reg E videoHDR */
	for (i = 0; i < count; i++) {
		imx135_dpc_data.video_coord[i].x =
			imx135_dpc_data.snapshot_coord[i].x;
		imx135_dpc_data.video_coord[i].y =
			(((imx135_dpc_data.snapshot_coord[i].y - 424) / 4)*2)
		+ ((imx135_dpc_data.snapshot_coord[i].y - 424) % 2) + 2;
		CDBG("video_dpc_cord[%d] X= %d Y = %d\n",
		i, imx135_dpc_data.video_coord[i].x,
		imx135_dpc_data.video_coord[i].y);
	}
}

static void imx135_format_wbdata(void)
{
	uint16_t wb_msb;
	uint32_t r, gr, gb, b;
	wb_msb = imx135_wbcalib_data[0];
	r = ((wb_msb & 0x00C0) << 2) | imx135_wbcalib_data[1];
	gr = ((wb_msb & 0x0030) << 4) | imx135_wbcalib_data[2];
	b = ((wb_msb & 0x000C) << 6) | imx135_wbcalib_data[3];
	gb = ((wb_msb & 0x0003) << 8) | imx135_wbcalib_data[4];

	CDBG("%s imx135_wbcalib_data[0] =%d r =%d gr = %d gb =%d, b=%d\n",
	__func__, wb_msb, r, gr, gb, b);

    #if 0
	imx135_wb_data.r_over_g = (r * 256) / gr;
	imx135_wb_data.b_over_g = (b * 256) / gb;
	imx135_wb_data.gr_over_gb = (gr * 256) / gb;
    #else
    if(gr != 0)
		imx135_wb_data.r_over_g = (r * 256) / gr;
    if(gb != 0) {
		imx135_wb_data.b_over_g = (b * 256) / gb;
		imx135_wb_data.gr_over_gb = (gr * 256) / gb;
    }
    #endif
	CDBG("%s r_over_g =%d b_over_g =%d gr_over_gb = %d\n", __func__,
		imx135_wb_data.r_over_g, imx135_wb_data.b_over_g,
		imx135_wb_data.gr_over_gb);
}

static void imx135_format_afdata(void)
{
	uint16_t vcmcode_msb;
	vcmcode_msb = imx135_afcalib_data[0];
	imx135_af_data.macro_dac =
		((vcmcode_msb & 0x0030) << 4) | imx135_afcalib_data[1];
	imx135_af_data.start_dac =
		((vcmcode_msb & 0x0003) << 8) | imx135_afcalib_data[3];
	imx135_af_data.inf_dac =
		((vcmcode_msb & 0x000C) << 6) | imx135_afcalib_data[2];

	CDBG("%s startDac =%d MacroDac =%d 50cm = %d\n", __func__,
		imx135_af_data.start_dac, imx135_af_data.macro_dac,
		imx135_af_data.inf_dac);
}

void imx135_format_calibrationdata(void)
{
	imx135_format_afdata();
	imx135_format_wbdata();
	imx135_format_dpcdata();
}

void imx135_set_dev_addr(struct msm_eeprom_ctrl_t *ectrl,
	uint32_t *reg_addr) {
	int32_t rc = 0;

	rc = msm_camera_i2c_write(&ectrl->i2c_client,
		0x3B02, (*reg_addr) >> 16, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc != 0)
		pr_err("%s: Page write error\n", __func__);

	rc = msm_camera_i2c_write(&ectrl->i2c_client,
		0x3B00, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc != 0)
		pr_err("%s: Turn ON read mode write error\n", __func__);

	rc = msm_camera_i2c_poll(&ectrl->i2c_client,
		0x3B01, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc != 0)
		pr_err("%s: Ready ready poll error\n", __func__);

}

static struct msm_eeprom_ctrl_t imx135_eeprom_t = {
	.i2c_driver = &imx135_eeprom_i2c_driver,
	.i2c_addr = 0x20,
	.eeprom_v4l2_subdev_ops = &imx135_eeprom_subdev_ops,

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
	},

	.eeprom_mutex = &imx135_eeprom_mutex,

	.func_tbl = {
		.eeprom_init = NULL,
		.eeprom_release = NULL,
		.eeprom_get_info = msm_camera_eeprom_get_info,
		.eeprom_get_data = msm_camera_eeprom_get_data,
		.eeprom_set_dev_addr = imx135_set_dev_addr,
		.eeprom_format_data = imx135_format_calibrationdata,
	},
	.info = &imx135_calib_supp_info,
	.info_size = sizeof(struct msm_camera_eeprom_info_t),
	.read_tbl = imx135_eeprom_read_tbl,
	.read_tbl_size = ARRAY_SIZE(imx135_eeprom_read_tbl),
	.data_tbl = imx135_eeprom_data_tbl,
	.data_tbl_size = ARRAY_SIZE(imx135_eeprom_data_tbl),
};

subsys_initcall(imx135_eeprom_i2c_add_driver);
MODULE_DESCRIPTION("imx135 EEPROM");
MODULE_LICENSE("GPL v2");
