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
#include <sharp/sh_boot_manager.h>
#define SENSOR_NAME "imx132"
#define PLATFORM_DRIVER_NAME "msm_camera_imx132"
#define imx132_obj imx132_##obj

#define REG_DIGITAL_GAIN_GREEN_R  0x020E
#define REG_DIGITAL_GAIN_RED      0x0210
#define REG_DIGITAL_GAIN_BLUE     0x0212
#define REG_DIGITAL_GAIN_GREEN_B  0x0214

#define CONFIG_MSM_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSM_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug("imx132: " fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#if 1
#include <sharp/sh_boot_manager.h>

#define SH_REV_ES0 0x00
#define SH_REV_ES1 0x02
#define SH_REV_PP1 0x03
#define SH_REV_PP2 0x04
#define SH_REV_MP 0x07

#if defined(CONFIG_MACH_LYNX_GP6D) || defined(CONFIG_MACH_DECKARD_AS96)
#define SH_REV_CMP    SH_REV_ES1
#else
#define SH_REV_CMP    SH_REV_PP1
#endif

#endif

DEFINE_MUTEX(imx132_mut);
static struct msm_sensor_ctrl_t imx132_s_ctrl;

static struct msm_camera_i2c_reg_conf imx132_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx132_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx132_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx132_groupoff_settings[] = {
	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx132_snap_settings[] = {
    {0x0340, 0x06},
    {0x0341, 0x62},
    {0x0342, 0x0C},
#if defined(CONFIG_MACH_TDN)
    {0x0343, 0x98},
#elif defined(CONFIG_MACH_DECKARD_AS96)
    {0x0343, 0x10},
#else
    {0x0343, 0x46},
#endif
    {0x0344, 0x00},
    {0x0345, 0x1C},
    {0x0346, 0x00},
    {0x0347, 0x3C},
    {0x0348, 0x07},
    {0x0349, 0x9B},
    {0x034A, 0x04},
    {0x034B, 0x73},
    {0x034C, 0x07},
    {0x034D, 0x80},
    {0x034E, 0x04},
    {0x034F, 0x38},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x303D, 0x10},
    {0x303E, 0x4A},
    {0x3040, 0x08},
    {0x3041, 0x97},
    {0x3048, 0x00},
    {0x304C, 0x2F},
    {0x304D, 0x02},
    {0x3064, 0x92},
    {0x306A, 0x10},
    {0x3095, 0xD0},
    {0x3099, 0x03},
    {0x309B, 0x00},
    {0x309E, 0x41},
    {0x30A0, 0x10},
    {0x30A1, 0x0B},
    {0x30B2, 0x00},
    {0x30D5, 0x00},
    {0x30D6, 0x00},
    {0x30D7, 0x00},
    {0x30D8, 0x00},
    {0x30D9, 0x00},
    {0x30DA, 0x00},
    {0x30DB, 0x00},
    {0x30DC, 0x00},
    {0x30DD, 0x00},
    {0x30DE, 0x00},
    {0x3102, 0x0C},
    {0x3103, 0x33},
    {0x3104, 0x30},
    {0x3105, 0x00},
    {0x3106, 0xCA},
    {0x3107, 0x00},
    {0x3108, 0x06},
    {0x3109, 0x04},
    {0x310A, 0x04},
    {0x315C, 0x3D},
    {0x315D, 0x3C},
    {0x316E, 0x3E},
    {0x316F, 0x3D},
    {0x3301, 0x00},
#if defined(CONFIG_MACH_DECKARD_AS96)
    {0x3304, 0x05},
    {0x3305, 0x05},
    {0x3306, 0x18},
    {0x3307, 0x01},
    {0x3308, 0x0D},
    {0x3309, 0x06},
    {0x330A, 0x0A},
    {0x330B, 0x05},
    {0x330C, 0x0B},
    {0x330D, 0x06},
    {0x330E, 0x01},
    {0x3318, 0x42},
#else
    {0x3304, 0x06},
    {0x3305, 0x06},
    {0x3306, 0x1B},
    {0x3307, 0x02},
    {0x3308, 0x0E},
    {0x3309, 0x07},
    {0x330A, 0x0C},
    {0x330B, 0x06},
    {0x330C, 0x0B},
    {0x330D, 0x07},
    {0x330E, 0x01},
    {0x3318, 0x41},
#endif
    {0x3322, 0x09},
    {0x3342, 0x00},
    {0x3348, 0xE0},
};

static struct msm_camera_i2c_reg_conf imx132_prev_settings[] = {
    {0x0340, 0x06},
    {0x0341, 0x62},
    {0x0342, 0x0C},
#if defined(CONFIG_MACH_TDN)
    {0x0343, 0x98},
#elif defined(CONFIG_MACH_DECKARD_AS96)
    {0x0343, 0x10},
#else
    {0x0343, 0x46},
#endif
    {0x0344, 0x00},
    {0x0345, 0x1C},
    {0x0346, 0x00},
    {0x0347, 0x3C},
    {0x0348, 0x07},
    {0x0349, 0x9B},
    {0x034A, 0x04},
    {0x034B, 0x73},
    {0x034C, 0x07},
    {0x034D, 0x80},
    {0x034E, 0x04},
    {0x034F, 0x38},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x303D, 0x10},
    {0x303E, 0x4A},
    {0x3040, 0x08},
    {0x3041, 0x97},
    {0x3048, 0x00},
    {0x304C, 0x2F},
    {0x304D, 0x02},
    {0x3064, 0x92},
    {0x306A, 0x10},
    {0x3095, 0xD0},
    {0x3099, 0x03},
    {0x309B, 0x00},
    {0x309E, 0x41},
    {0x30A0, 0x10},
    {0x30A1, 0x0B},
    {0x30B2, 0x00},
    {0x30D5, 0x00},
    {0x30D6, 0x00},
    {0x30D7, 0x00},
    {0x30D8, 0x00},
    {0x30D9, 0x00},
    {0x30DA, 0x00},
    {0x30DB, 0x00},
    {0x30DC, 0x00},
    {0x30DD, 0x00},
    {0x30DE, 0x00},
    {0x3102, 0x0C},
    {0x3103, 0x33},
    {0x3104, 0x30},
    {0x3105, 0x00},
    {0x3106, 0xCA},
    {0x3107, 0x00},
    {0x3108, 0x06},
    {0x3109, 0x04},
    {0x310A, 0x04},
    {0x315C, 0x3D},
    {0x315D, 0x3C},
    {0x316E, 0x3E},
    {0x316F, 0x3D},
    {0x3301, 0x00},
#if defined(CONFIG_MACH_DECKARD_AS96)
    {0x3304, 0x05},
    {0x3305, 0x05},
    {0x3306, 0x18},
    {0x3307, 0x01},
    {0x3308, 0x0D},
    {0x3309, 0x06},
    {0x330A, 0x0A},
    {0x330B, 0x05},
    {0x330C, 0x0B},
    {0x330D, 0x06},
    {0x330E, 0x01},
    {0x3318, 0x42},
#else
    {0x3304, 0x06},
    {0x3305, 0x06},
    {0x3306, 0x1B},
    {0x3307, 0x02},
    {0x3308, 0x0E},
    {0x3309, 0x07},
    {0x330A, 0x0C},
    {0x330B, 0x06},
    {0x330C, 0x0B},
    {0x330D, 0x07},
    {0x330E, 0x01},
    {0x3318, 0x41},
#endif
    {0x3322, 0x09},
    {0x3342, 0x00},
    {0x3348, 0xE0},
};

static struct msm_camera_i2c_reg_conf imx132_gloabal_settings[] = {
    {0x3087, 0x53},
    {0x308B, 0x5A},
    {0x3094, 0x11},
    {0x309D, 0xA4},
    {0x30AA, 0x01},
    {0x30C6, 0x00},
    {0x30C7, 0x00},
    {0x3118, 0x2F},
    {0x312A, 0x00},
    {0x312B, 0x0B},
    {0x312C, 0x0B},
    {0x312D, 0x13},
#if defined(CONFIG_MACH_LYNX_GP6D)
	{0x0101, 0x00},
#else
	{0x0101, 0x03},
#endif
};

static struct msm_camera_i2c_reg_conf imx132_pll_settings[] = {
    {0x0305, 0x01},
#if defined(CONFIG_MACH_TDN)
    {0x0307, 0x77},
#elif defined(CONFIG_MACH_DECKARD_AS96)
    {0x0307, 0x72},
#else
    {0x0307, 0x74},
#endif
    {0x30A4, 0x02},
    {0x303C, 0x15},
};

static struct v4l2_subdev_info imx132_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx132_init_conf[] = {
	{&imx132_gloabal_settings[0],
	ARRAY_SIZE(imx132_gloabal_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx132_pll_settings[0],
	ARRAY_SIZE(imx132_pll_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array imx132_confs[] = {
	{&imx132_snap_settings[0],
	ARRAY_SIZE(imx132_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx132_prev_settings[0],
	ARRAY_SIZE(imx132_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t imx132_dimensions[] = {
#if defined(CONFIG_MACH_TDN)
	{
		.x_output = 1920,
		.y_output = 1080,
		.line_length_pclk = 3224,
		.frame_length_lines = 1634,
		.vt_pixel_clk = 160650000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{
		.x_output = 1920,
		.y_output = 1080,
		.line_length_pclk = 3224,
		.frame_length_lines = 1634,
		.vt_pixel_clk = 160650000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
#elif defined(CONFIG_MACH_DECKARD_AS96)
	{
		.x_output = 1920,
		.y_output = 1080,
		.line_length_pclk = 3088,
		.frame_length_lines = 1634,
		.vt_pixel_clk = 153900000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{
		.x_output = 1920,
		.y_output = 1080,
		.line_length_pclk = 3088,
		.frame_length_lines = 1634,
		.vt_pixel_clk = 153900000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
#elif defined(CONFIG_MACH_LYNX_GP6D)
	{
		.x_output = 1920,
		.y_output = 1080,
		.line_length_pclk = 3142,
		.frame_length_lines = 1634,
		.vt_pixel_clk = 156600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{
		.x_output = 1920,
		.y_output = 1080,
		.line_length_pclk = 3142,
		.frame_length_lines = 1634,
		.vt_pixel_clk = 156600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
#else
	{
		.x_output = 1920,
		.y_output = 1080,
		.line_length_pclk = 3142,
		.frame_length_lines = 1634,
		.vt_pixel_clk = 156600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{
		.x_output = 1920,
		.y_output = 1080,
		.line_length_pclk = 3142,
		.frame_length_lines = 1634,
		.vt_pixel_clk = 156600000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
#endif
};

static struct msm_sensor_output_reg_addr_t imx132_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t imx132_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x0132,
};

static struct msm_sensor_exp_gain_info_t imx132_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
//	.global_gain_addr = 0x0204,
	.global_gain_addr = 0x0205,
	.vert_offset = 5,
};

#if 1
static enum msm_camera_vreg_name_t imx132_veg_seq_1[] = {
	CAM_VIO,
	CAM_VDIG,
	CAM_VANA,
};

static enum msm_camera_vreg_name_t imx132_veg_seq_2[] = {
	CAM_VDIG,
	CAM_VANA,
};
#endif

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_675HZ},
};

int32_t imx132_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
#if 1
	unsigned short rev;
	enum msm_camera_vreg_name_t *vreg_seq;
	int num_vreg_seq;
#endif
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

#if 1
	rev = sh_boot_get_hw_revision();

	if(rev < SH_REV_CMP) {
		vreg_seq = imx132_veg_seq_1;
		num_vreg_seq = ARRAY_SIZE(imx132_veg_seq_1);
    } else {
		vreg_seq = imx132_veg_seq_2;
		num_vreg_seq = ARRAY_SIZE(imx132_veg_seq_2);
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

	usleep(2000);

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

	usleep(600);
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

int32_t imx132_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
#if 1
	unsigned short rev;
	enum msm_camera_vreg_name_t *vreg_seq;
	int num_vreg_seq;
#endif
	CDBG("%s\n", __func__);

#if 1
	rev = sh_boot_get_hw_revision();

	if(rev < SH_REV_CMP) {
		vreg_seq = imx132_veg_seq_1;
		num_vreg_seq = ARRAY_SIZE(imx132_veg_seq_1);
    } else {
		vreg_seq = imx132_veg_seq_2;
		num_vreg_seq = ARRAY_SIZE(imx132_veg_seq_2);
    }
#endif

	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);

#if 0
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		NULL, 0, s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		NULL, 0, s_ctrl->reg_ptr, 0);
#else
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		vreg_seq, num_vreg_seq, s_ctrl->reg_ptr, 0);
#endif
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
	return 0;
}

static int imx132_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	
	uint32_t fl_lines;
	uint8_t offset;
	
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&imx132_mut);
	CDBG("imx132_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SHDIAG_GET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				mutex_unlock(&imx132_mut);
				return -EFAULT;
			}
			CDBG("%s:%d i2c_read addr=0x%0x\n",__func__,__LINE__,cdata.cfg.i2c_info.addr);
			rc = msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, cdata.cfg.i2c_info.addr , data, cdata.cfg.i2c_info.length);
			CDBG("%s:%d i2c_read data=0x%0x\n",__func__,__LINE__,*(unsigned char *)data);
			if (copy_to_user((void *)cdata.cfg.i2c_info.data,
				data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				mutex_unlock(&imx132_mut);
				CDBG("%s copy_to_user error\n",__func__);
				return -EFAULT;
			}
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))) {
				mutex_unlock(&imx132_mut);
				return -EFAULT;
			}
		}
			break;
	case CFG_SHDIAG_SET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				mutex_unlock(&imx132_mut);
				return -EFAULT;
			}
			if (copy_from_user(data,
				(void *)cdata.cfg.i2c_info.data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_to_user error\n",__func__);
				mutex_unlock(&imx132_mut);
				return -EFAULT;
			}
			rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client, cdata.cfg.i2c_info.addr, data, cdata.cfg.i2c_info.length);
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data))) {
				mutex_unlock(&imx132_mut);
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
		}
		break;
	default:
		mutex_unlock(&imx132_mut);
		return msm_sensor_config(s_ctrl, argp);
		break;
	}
	
	mutex_unlock(&imx132_mut);
	return rc;
}

static const struct i2c_device_id imx132_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx132_s_ctrl},
	{ }
};

static struct i2c_driver imx132_i2c_driver = {
	.id_table = imx132_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx132_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&imx132_i2c_driver);
}

static struct v4l2_subdev_core_ops imx132_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx132_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx132_subdev_ops = {
	.core = &imx132_subdev_core_ops,
	.video  = &imx132_subdev_video_ops,
};

static struct msm_sensor_fn_t imx132_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = msm_sensor_write_exp_gain2,
	.sensor_write_snapshot_exp_gain = msm_sensor_write_exp_gain2,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = imx132_sensor_config,
	.sensor_power_up = imx132_sensor_power_up,
	.sensor_power_down = imx132_sensor_power_down,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t imx132_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx132_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx132_start_settings),
	.stop_stream_conf = imx132_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx132_stop_settings),
	.group_hold_on_conf = imx132_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx132_groupon_settings),
	.group_hold_off_conf = imx132_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(imx132_groupoff_settings),
	.init_settings = &imx132_init_conf[0],
	.init_size = ARRAY_SIZE(imx132_init_conf),
	.mode_settings = &imx132_confs[0],
	.output_settings = &imx132_dimensions[0],
	.num_conf = ARRAY_SIZE(imx132_confs),
};

static struct msm_sensor_ctrl_t imx132_s_ctrl = {
	.msm_sensor_reg = &imx132_regs,
	.sensor_i2c_client = &imx132_sensor_i2c_client,
	.sensor_i2c_addr = 0x6c,
	.sensor_output_reg_addr = &imx132_reg_addr,
	.sensor_id_info = &imx132_id_info,
	.sensor_exp_gain_info = &imx132_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &imx132_mut,
	.sensor_i2c_driver = &imx132_i2c_driver,
	.sensor_v4l2_subdev_info = imx132_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx132_subdev_info),
	.sensor_v4l2_subdev_ops = &imx132_subdev_ops,
	.func_tbl = &imx132_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_675HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("SONY 2M sensor driver");
MODULE_LICENSE("GPL v2");
