/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_sensor.h"
#include "msm.h"
#include "msm_ispif.h"

#include "s5k8aayx.h"

DEFINE_MUTEX(s5k8aayx_mut);
static struct msm_sensor_ctrl_t s5k8aayx_s_ctrl;
#define SENSOR_NAME "s5k8aayx"

#define CONFIG_MSM_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSM_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug("s5k8aayx: " fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
#define Q8    0x00000100

/* New 1.2M CMOS product ID register address */
#define REG_S5K8AAYX_CHIP_ID                    0x0040
#define REG_S5K8AAYX_REVISION                   0x0042

#define S5K8AAYX_CHIP_ID                        0x08AA
/* New 1.2M CMOS product ID */

/* Color bar pattern selection */
#define S5K8AAYX_COLOR_BAR_PATTERN_SEL_REG      0x82
/* Color bar enabling control */
#define S5K8AAYX_COLOR_BAR_ENABLE_REG           0x601

#define S5K8AAYX_SENSOR_MCLK_13P5HZ     13500000

#define REG_TC_GP_NewConfigSync			0x01A6
#define REG_TC_GP_ErrorPrevConfig		0x01AE
#define REG_TC_GP_ErrorCapConfig		0x01B4
#define REG_TC_DBG_AutoAlgEnBits		0x0408
#define S5K8AAYX_WRITEALGO(client,a,b,c)  s5k8aayx_autoalgbits_ctrl(client,a,b,c,\
									sizeof(c)/sizeof(c[0]))

/*============================================================================
							DATA DECLARATIONS
============================================================================*/


static struct msm_camera_i2c_conf_array s5k8aayx_init_conf[] = {
	{&s5k8aayx_init_module_settings_array[0],
	ARRAY_SIZE(s5k8aayx_init_module_settings_array), 0, MSM_CAMERA_I2C_WORD_DATA}
};

static struct msm_camera_i2c_conf_array s5k8aayx_setting_conf[] = {
	{&s5k8aayx_sensor_init_settings_array[0],
	ARRAY_SIZE(s5k8aayx_sensor_init_settings_array), 0, MSM_CAMERA_I2C_WORD_DATA}
};

static struct msm_camera_i2c_conf_array s5k8aayx_prev_confs[] = {
	{&s5k8aayx_prev_full_array[0], /* FULL */
	ARRAY_SIZE(s5k8aayx_prev_full_array), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k8aayx_prev_full_array[0], /* QTR */
	ARRAY_SIZE(s5k8aayx_prev_full_array), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k8aayx_prev_hd720_array[0],/* HD720 */
	ARRAY_SIZE(s5k8aayx_prev_hd720_array), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k8aayx_prev_vga_array[0],  /* VGA */
	ARRAY_SIZE(s5k8aayx_prev_vga_array), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k8aayx_prev_qvga_array[0], /* QVGA */
	ARRAY_SIZE(s5k8aayx_prev_qvga_array), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k8aayx_prev_qcif_array[0], /* QCIF */
	ARRAY_SIZE(s5k8aayx_prev_qcif_array), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_sensor_output_info_t s5k8aayx_dimensions[] = {
	{ /* FULL */
		.x_output = 0x500, /* 1280 */
		.y_output = 0x3C0, /*  960 */
		.line_length_pclk = 0x57A, /* 1402 */
		.frame_length_lines = 0x436, /* 1078 */
		.vt_pixel_clk = 54000000,
		.op_pixel_clk = 90700000,
		.binning_factor = 1,
	},
	{ /* QTR */
		.x_output = 0x500, /* 1280 */
		.y_output = 0x3C0, /*  960 */
		.line_length_pclk = 0x57A, /* 1402 */
		.frame_length_lines = 0x436, /* 1078 */
		.vt_pixel_clk = 54000000,
		.op_pixel_clk = 90700000,
		.binning_factor = 1,
	},
	{ /* HD720 */
		.x_output = 0x500, /* 1280 */
		.y_output = 0x2D0, /*  720 */
		.line_length_pclk = 0x57A, /* 1402 */
		.frame_length_lines = 0x436, /* 1078 */
		.vt_pixel_clk = 54000000,
		.op_pixel_clk = 90700000,
		.binning_factor = 1,
	},
	{ /* VGA */
		.x_output = 0x280, /* 640 */
		.y_output = 0x1E0, /* 480 */
		.line_length_pclk = 0x57A, /* 1402 */
		.frame_length_lines = 0x436, /* 1078 */
		.vt_pixel_clk = 54000000,
		.op_pixel_clk = 90700000,
		.binning_factor = 1,
	},
	{ /* QVGA */
		.x_output = 0x140, /* 320 */
		.y_output = 0x0F0, /* 240 */
		.line_length_pclk = 0x57A, /* 1402 */
		.frame_length_lines = 0x436, /* 1078 */
		.vt_pixel_clk = 54000000,
		.op_pixel_clk = 90700000,
		.binning_factor = 1,
	},
	{ /* QCIF */
		.x_output = 0x0B0, /* 176 */
		.y_output = 0x090, /* 144 */
		.line_length_pclk = 0x57A, /* 1402 */
		.frame_length_lines = 0x436, /* 1078 */
		.vt_pixel_clk = 54000000,
		.op_pixel_clk = 90700000,
		.binning_factor = 1,
	},
};


static int32_t s5k8aayx_i2c_reg_read(struct msm_camera_i2c_client *client,
										unsigned short raddr,
										unsigned short *rdata)
{
	int32_t rc = -EIO;
	unsigned short work = 0xFF;

	rc = msm_camera_i2c_write(client, 0x002C, 0x7000, MSM_CAMERA_I2C_WORD_DATA);
	if(0 > rc) {
		pr_err("[%d] i2c_write failed!.\n", __LINE__);
		return rc;
	}
	rc = msm_camera_i2c_write(client, 0x002E, raddr, MSM_CAMERA_I2C_WORD_DATA);
	if(0 > rc) {
		pr_err("[%d] i2c_write failed!.\n", __LINE__);
		return rc;
	}
	rc = msm_camera_i2c_read(client, 0x0F12, &work, MSM_CAMERA_I2C_WORD_DATA);
	if(0 > rc) {
		pr_err("[%d] i2c_read failed!.\n", __LINE__);
		return rc;
	}
	else {
		*rdata = work;
		rc = 0;
	}

	return rc;
}

static int s5k8aayx_get_snapshot_info(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	uint8_t read_buf[4];

	CDBG("%s: \n", __func__);

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x002C, 0x7000, 2);
	if (rc < 0) {
		pr_err("[%d]:msm_camera_i2c_write_seq write failed!.add = 0x002C, dat=0x7000\n",
				 __LINE__);
		return rc;
	}
	
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x002E, 0x20DC, 2);
	if (rc < 0) {
		pr_err("[%d]:msm_camera_i2c_write_seq write failed!.add = 0x002E, dat=0x20DC\n",
				 __LINE__);
		return rc;
	}

	rc = msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, 0x0F12, read_buf, 4);
	if (rc < 0) {
		pr_err("[%d]:msm_camera_i2c_read_seq read failed!. add = 0x0F12\n", __LINE__);
		return rc;
	}
	CDBG("[0]=0x%02x [1]=0x%02x [2]=0x%02x [3]=0x%02x \n", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
	cdata->cfg.exp_value = (read_buf[1] | (read_buf[0] << 8) | (read_buf[3] << 16) | (read_buf[2] << 24));

	return rc;
}

static int s5k8aayx_get_shdenoise_info(struct msm_sensor_ctrl_t *s_ctrl, struct sensor_cfg_data *cdata)
{
	int rc = 0;
	uint8_t read_buf[4];

	CDBG("%s: \n", __func__);

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x002C, 0x7000, 2);
	if (rc < 0) {
		pr_err("[%d]:msm_camera_i2c_write_seq write failed!.add = 0x002C, dat=0x7000\n",
				 __LINE__);
		return rc;
	}
	
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x002E, 0x20E0, 2);
	if (rc < 0) {
		pr_err("[%d]:msm_camera_i2c_write_seq write failed!.add = 0x002E, dat=0x20E0\n",
				 __LINE__);
		return rc;
	}

	rc = msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, 0x0F12, read_buf, 4);
	if (rc < 0) {
		pr_err("[%d]:msm_camera_i2c_read_seq read failed!. add = 0x0F12\n", __LINE__);
		return rc;
	}
	CDBG("[0]=0x%02x [1]=0x%02x [2]=0x%02x [3]=0x%02x \n", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
	cdata->cfg.gain = (read_buf[1] | (read_buf[0] << 8) | (read_buf[3] << 16) | (read_buf[2] << 24));

	return rc;
}


static int s5k8aayx_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&s5k8aayx_mut);
	CDBG("s5k8aayx_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SHDIAG_GET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				return -EFAULT;
			}
			CDBG("%s:%d i2c_read addr=0x%0x\n",__func__,__LINE__,cdata.cfg.i2c_info.addr);
			rc = msm_camera_i2c_read(
					s_ctrl->sensor_i2c_client,
					cdata.cfg.i2c_info.addr, data,
					MSM_CAMERA_I2C_WORD_DATA);
			CDBG("%s:%d i2c_read data=0x%0x\n",__func__,__LINE__,*(unsigned short *)data);
			if (copy_to_user((void *)cdata.cfg.i2c_info.data,
				data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_to_user error\n",__func__);
				return -EFAULT;
			}
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			return -EFAULT;
		}
			break;
	case CFG_SHDIAG_SET_I2C_DATA:
		{
			void *data;
			data = kmalloc(cdata.cfg.i2c_info.length, GFP_KERNEL);
			if(data == NULL){
				return -EFAULT;
			}
			if (copy_from_user(data,
				(void *)cdata.cfg.i2c_info.data,
				cdata.cfg.i2c_info.length)){
				kfree(data);
				CDBG("%s copy_to_user error\n",__func__);
				return -EFAULT;
			}
			CDBG("%s addr = 0x%x\n",__func__, cdata.cfg.i2c_info.addr);
			rc = msm_camera_i2c_write(
					s_ctrl->sensor_i2c_client,
					cdata.cfg.i2c_info.addr, *(uint16_t *)data,
					MSM_CAMERA_I2C_WORD_DATA);
			CDBG("%s *(uint16_t *)data = 0x%x\n",__func__, *(uint16_t *)data);
			kfree(data);
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			return -EFAULT;
		}
			break;
	case CFG_GET_SNAPSHOT_INFO:
		CDBG("%s: CFG_GET_SNAPSHOT_INFO !\n", __func__);
		rc = s5k8aayx_get_snapshot_info(s_ctrl, &cdata);
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_SH_GET_DENOISE_INFO:
		CDBG("%s: CFG_SH_GET_DENOISE_INFO !\n", __func__);
		rc = s5k8aayx_get_shdenoise_info(s_ctrl, &cdata);
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	default:
		mutex_unlock(&s5k8aayx_mut);
		return msm_sensor_config(s_ctrl, argp);
		break;
	}
	
	mutex_unlock(&s5k8aayx_mut);
	return rc;

}

#if 0
int32_t s5k8aayx_sensor_write_prev_settings(struct msm_sensor_ctrl_t *s_ctrl)
#else
int32_t s5k8aayx_sensor_write_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
#endif
{
	int32_t rc=0;
	int32_t i;
	uint16_t size;
	struct msm_camera_i2c_reg_conf *reg_conf_tbl;
	uint16_t addr_pos,burst_num;
	unsigned char buf[128];

	#if 0
	reg_conf_tbl = s5k8aayx_prev_conf[0].conf;
	size = s5k8aayx_prev_conf[0].size;
	#else
	reg_conf_tbl = s5k8aayx_setting_conf[0].conf;
	size = s5k8aayx_setting_conf[0].size;
	#endif
	addr_pos = 0;
	burst_num = 0;
	for (i = 0; i < size; i++) {
		if(reg_conf_tbl->reg_addr == 0x0028){
			if(burst_num > 0){
				addr_pos = 0x0F12;
				CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
				rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
																addr_pos,
																buf,
																burst_num);
			}
			buf[0] = (unsigned char) ((reg_conf_tbl->reg_data) >> 8);
			buf[1] = (unsigned char) ((reg_conf_tbl->reg_data) & 0x00FF);
			burst_num = 2;
			addr_pos = reg_conf_tbl->reg_addr;
			CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
			rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
															addr_pos,
															buf,
															burst_num);
			addr_pos = 0;
			burst_num = 0;
		} else if(reg_conf_tbl->reg_addr == 0x002A){
			if(burst_num > 0){
				addr_pos = 0x0F12;
				CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
				rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
																addr_pos,
																buf,
																burst_num);
			}
			addr_pos = reg_conf_tbl->reg_addr;
			burst_num = 2;
			buf[0] = (unsigned char) ((reg_conf_tbl->reg_data) >> 8);
			buf[1] = (unsigned char) ((reg_conf_tbl->reg_data) & 0x00FF);
			CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
			rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
															addr_pos,
															buf,
															burst_num);
			addr_pos = 0;
			burst_num = 0;

		} else if(reg_conf_tbl->reg_addr == 0x0F12){
			buf[burst_num] = (unsigned char) ((reg_conf_tbl->reg_data) >> 8);
			burst_num++;
			buf[burst_num] = (unsigned char) ((reg_conf_tbl->reg_data) & 0x00FF);
			burst_num++;
			
			if(burst_num >= 128){
				addr_pos = reg_conf_tbl->reg_addr;
				CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
				rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
																addr_pos,
																buf,
																burst_num);
//				addr_pos = addr_pos + burst_num;
				burst_num = 0;
			}
		} else {
			CDBG("%s ERROR? reg_addr=0x%0x burst_num=%d", __func__, reg_conf_tbl->reg_addr, burst_num);
		}
		reg_conf_tbl++;
	}
	if(burst_num > 0){
		addr_pos = 0x0F12;
		CDBG("%s %d addr_pos=0x%0x burst_num=%d", __func__, __LINE__, addr_pos, burst_num);
		rc = msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
														addr_pos,
														buf,
														burst_num);
	}
	usleep_range(0,0);
	return rc;
}

static int s5k8aayx_mode_upadate_chk(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int cnt = 0;
	unsigned short work = 0;

	while(1) {
		work = 0;
		rc = s5k8aayx_i2c_reg_read(s_ctrl->sensor_i2c_client,
									REG_TC_GP_NewConfigSync, &work);
		if(0 != rc) {
			return rc;
		}
		else {
			if(work != 0x0000)
			{
				if(cnt == 2000){
					pr_err("%s update timeout error!! reg=0x%x\n",
							__func__, work);
					return -ETIMEDOUT;
				}
				cnt++;
				usleep(200);
				continue;
			}
			else
			{
				CDBG("%s Check OK. cnt=%d\n", __func__, cnt);
				break;
			}
		}
	}
	return 0;
}

int32_t s5k8aayx_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	unsigned short regErr = 0;

	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		msm_sensor_write_init_settings(s_ctrl);
		usleep(1000);
		s5k8aayx_sensor_write_init_settings(s_ctrl);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("%s:[%d] res = %d\n", __func__, __LINE__, res);
		if (s_ctrl->curr_res != res) {
			if (s_ctrl->curr_res == MSM_SENSOR_INVALID_RES) {
				usleep(140000);
			}
			msm_sensor_write_conf_array(s_ctrl->sensor_i2c_client,
									&s5k8aayx_prev_confs[res] ,0);

			rc = s5k8aayx_i2c_reg_read(s_ctrl->sensor_i2c_client,
										REG_TC_GP_ErrorPrevConfig, &regErr);
			if (rc < 0 || regErr != 0) {
				pr_err("REG_TC_GP_ErrorPrevConfig = 0x%04x, rc=%d\n",
						regErr, rc);
			}
		}
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);

		if (s_ctrl->curr_res != res &&
			s_ctrl->curr_res != MSM_SENSOR_INVALID_RES) {
			usleep(1000);
			rc = s5k8aayx_mode_upadate_chk(s_ctrl);
			usleep(1000);
		}
	}
	return rc;
}

static struct v4l2_subdev_info s5k8aayx_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", S5K8AAYX_SENSOR_MCLK_13P5HZ},
};

static int32_t s5k8aayx_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
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

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			NULL, 0, s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			NULL, 0, s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
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

	usleep(1000);

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio en failed\n", __func__);
		goto config_gpio_en_failed;
	}

	usleep(1000);

	return rc;
config_gpio_en_failed:
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);

//config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			NULL, 0, s_ctrl->reg_ptr, 0);

enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			NULL, 0, s_ctrl->reg_ptr, 0);

config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);

request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;

}

static int32_t s5k8aayx_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s\n", __func__);

	usleep(3000);

	msm_camera_config_gpio_table(data, 0);

	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);

	usleep(10);

	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			NULL, 0, s_ctrl->reg_ptr, 0);

	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		NULL, 0, s_ctrl->reg_ptr, 0);

	msm_camera_request_gpio_table(data, 0);

	usleep(1000);

	kfree(s_ctrl->reg_ptr);
	return 0;

}

int32_t s5k8aayx_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	rc = msm_camera_i2c_write(
			s_ctrl->sensor_i2c_client,
			0xFCFC, 0x0000,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: write id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("msm_sensor id: %d\n", chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

static struct msm_sensor_id_info_t s5k8aayx_id_info = {
	.sensor_id_reg_addr = REG_S5K8AAYX_CHIP_ID,
	.sensor_id = S5K8AAYX_CHIP_ID,
};


static const struct i2c_device_id s5k8aayx_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k8aayx_s_ctrl},
	{ }
};

static struct i2c_driver s5k8aayx_i2c_driver = {
	.id_table = s5k8aayx_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k8aayx_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int s5k8aayx_reg_srch(unsigned short sreg, 
						   struct msm_camera_i2c_reg_conf *ptb, int32_t len)
{
	int i;
	unsigned short setadr = 0;

	for(i = 0; i < len; i++) {
		switch(ptb[i].reg_addr) {
		case S5K8AAYX_SETADDR:
			setadr = ptb[i].reg_data;
			break;

		case S5K8AAYX_READWRITE:
			if (setadr != 0) {
				if (setadr == sreg) {
					return i;
				}
				setadr += sizeof(unsigned short);
			}
			break;

		default:
			setadr = 0;
			break;
		}
	}
	return -1;
}

static int s5k8aayx_autoalgbits_ctrl(struct msm_sensor_ctrl_t *s_ctrl, int bctl, unsigned short reqbit,
									struct msm_camera_i2c_reg_conf *ptb, int32_t len)
{
	int rc = 0, idx;
	unsigned short rreg = 0, maskbit = 0;
	CDBG("%s bctl:%d reqbit:0x%x\n", __func__, bctl , reqbit);

	idx = s5k8aayx_reg_srch(REG_TC_DBG_AutoAlgEnBits, ptb, len);

	if (idx >= 0) {
		rc = s5k8aayx_i2c_reg_read(s_ctrl->sensor_i2c_client, REG_TC_DBG_AutoAlgEnBits, &rreg);
		if (rc != 0) {
			pr_err("s5k8aayx:[%d] reg read error!!\n", __LINE__);
			return rc;
		}
    	CDBG("%s rreg:0x%x\n", __func__, rreg);

		if (bctl) {
			maskbit = rreg | reqbit;		/* on */
		}
		else {
			maskbit = rreg & (~reqbit);		/* off */
		}

		maskbit |= 0x0100;	/* bit8 on */

		ptb[idx].reg_data = maskbit;
		CDBG("%s X maskbit:0x%x\n", __func__, maskbit);
	}

	return rc;
}

int s5k8aayx_aec_lock_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0, bctl;
	CDBG("%s value:%d\n", __func__, value);

	if (value) bctl = 0;	/* lock */
	else bctl = 1;			/* unlock */

	switch(value) {
		case MSM_V4L2_UNLOCK:
			rc = S5K8AAYX_WRITEALGO( s_ctrl, bctl, 
				S5K8AAYX_ALGORITHMSBIT_AE,s5k8aayx_algorithms_unlock);
		break;
		case MSM_V4L2_LOCK:
			rc = S5K8AAYX_WRITEALGO( s_ctrl, bctl, 
				S5K8AAYX_ALGORITHMSBIT_AE,s5k8aayx_algorithms_lock);
		break;
	}
	if (rc < 0) {
		pr_err("auto algorithm faield\n");
		return rc;
	}

	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	if (rc < 0) {
		pr_err("write faield\n");
		return rc;
	}

	CDBG("%s X\n", __func__);
	return rc;
}

int s5k8aayx_awb_lock_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0, bctl;
	CDBG("%s value:%d\n", __func__, value);

	if (value) bctl = 0;	/* lock */
	else bctl = 1;			/* unlock */

	switch(value) {
		case MSM_V4L2_UNLOCK:
			rc = S5K8AAYX_WRITEALGO( s_ctrl, bctl, 
				S5K8AAYX_ALGORITHMSBIT_AWB,s5k8aayx_algorithms_unlock);
		break;
		case MSM_V4L2_LOCK:
			rc = S5K8AAYX_WRITEALGO( s_ctrl, bctl, 
				S5K8AAYX_ALGORITHMSBIT_AWB,s5k8aayx_algorithms_lock);
		break;
	}
	if (rc < 0) {
		pr_err("auto algorithm faield\n");
		return rc;
	}

	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	if (rc < 0) {
		CDBG("write faield\n");
		return rc;
	}

	CDBG("%s X\n", __func__);
	return rc;
}

int s5k8aayx_antibanding_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	CDBG("%s E value:%d\n", __func__, value);

	switch(value) {
		case MSM_V4L2_POWER_LINE_OFF:
			rc = S5K8AAYX_WRITEALGO( s_ctrl, 0, 
				S5K8AAYX_ALGORITHMSBIT_FLICKER,s5k8aayx_antibanding_off);
		break;
		case MSM_V4L2_POWER_LINE_60HZ:
			rc = S5K8AAYX_WRITEALGO( s_ctrl, 0, 
				S5K8AAYX_ALGORITHMSBIT_FLICKER,s5k8aayx_antibanding_60Hz);
		break;
		case MSM_V4L2_POWER_LINE_50HZ:
			rc = S5K8AAYX_WRITEALGO( s_ctrl, 0, 
				S5K8AAYX_ALGORITHMSBIT_FLICKER,s5k8aayx_antibanding_50Hz);
		break;
		case MSM_V4L2_POWER_LINE_AUTO:
			rc = S5K8AAYX_WRITEALGO( s_ctrl, 1, 
				S5K8AAYX_ALGORITHMSBIT_FLICKER,s5k8aayx_antibanding_auto);
		break;
	}
	if (rc < 0) {
		pr_err("auto algorithm faield\n");
		return rc;
	}

	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	if (rc < 0) {
		pr_err("write faield\n");
		return rc;
	}

	CDBG("%s X\n", __func__);
	return rc;
}

int s5k8aayx_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	if (rc < 0) {
		CDBG("write faield\n");
		return rc;
	}
	return rc;
}

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&s5k8aayx_i2c_driver);
}

static struct v4l2_subdev_core_ops s5k8aayx_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops s5k8aayx_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k8aayx_subdev_ops = {
	.core = &s5k8aayx_subdev_core_ops,
	.video  = &s5k8aayx_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k8aayx_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = NULL,
	.sensor_group_hold_off = NULL,
	.sensor_set_fps = NULL,
	.sensor_write_exp_gain = NULL,
	.sensor_write_snapshot_exp_gain = NULL,
	.sensor_setting = s5k8aayx_sensor_setting,
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = s5k8aayx_sensor_config,
	.sensor_power_up = s5k8aayx_sensor_power_up,
	.sensor_power_down = s5k8aayx_sensor_power_down,
	.sensor_match_id = s5k8aayx_sensor_match_id,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t s5k8aayx_regs = {
	.default_data_type = MSM_CAMERA_I2C_WORD_DATA,
#if 0
	.start_stream_conf = NULL,
	.start_stream_conf_size = 0,
	.stop_stream_conf = NULL,
	.stop_stream_conf_size = 0,
#else
	.start_stream_conf = s5k8aayx_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(s5k8aayx_start_settings),
	.stop_stream_conf = s5k8aayx_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(s5k8aayx_stop_settings),
#endif
	.group_hold_on_conf = NULL,
	.group_hold_on_conf_size = 0,
	.group_hold_off_conf = NULL,
	.group_hold_off_conf_size = 0,
	.init_settings = &s5k8aayx_init_conf[0],
	.init_size = ARRAY_SIZE(s5k8aayx_init_conf),
	.mode_settings = NULL,
	.output_settings = &s5k8aayx_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k8aayx_prev_confs),
};

static struct msm_sensor_ctrl_t s5k8aayx_s_ctrl = {
	.msm_sensor_reg = &s5k8aayx_regs,
	.msm_sensor_v4l2_ctrl_info = s5k8aayx_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(s5k8aayx_v4l2_ctrl_info),
	.sensor_i2c_client = &s5k8aayx_sensor_i2c_client,
	.sensor_i2c_addr = 0x78,
	.sensor_output_reg_addr = NULL,
	.sensor_id_info = &s5k8aayx_id_info,
	.sensor_exp_gain_info = NULL,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
//	.csi_params = &s5k8aayx_csi_params_array[0],
	.msm_sensor_mutex = &s5k8aayx_mut,
	.sensor_i2c_driver = &s5k8aayx_i2c_driver,
	.sensor_v4l2_subdev_info = s5k8aayx_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k8aayx_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k8aayx_subdev_ops,
	.func_tbl = &s5k8aayx_func_tbl,
	.clk_rate = S5K8AAYX_SENSOR_MCLK_13P5HZ,
};

module_init(msm_sensor_init_module);

MODULE_DESCRIPTION("Samsung 1.2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
