/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/gpio.h>

#include <asm/mach-types.h>

#include <mach/camera.h>
#include <mach/msm_bus_board.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "board-8064.h"

#ifdef CONFIG_MSM_CAMERA

#if 0
static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 1*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 2*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_2, /*active 3*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_5, /*active 4*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_6, /*active 5*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_2, /*active 6*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_3, /*active 7*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*i2c suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},

	{
		.func = GPIOMUX_FUNC_9, /*active 9*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_A, /*active 10*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_6, /*active 11*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_4, /*active 12*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

};
#endif

#if 0
static struct msm_gpiomux_config apq8064_cam_common_configs[] = {
	{
		.gpio = 1,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[12],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 4,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 5,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 34,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 107,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = 10,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[9],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{
		.gpio = 11,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[10],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{
		.gpio = 12,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[11],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{
		.gpio = 13,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[11],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
};
#endif


#define VFE_CAMIF_TIMER1_GPIO 3
#define VFE_CAMIF_TIMER2_GPIO 1

#if defined(CONFIG_IMX074)
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_EXT,
	._fsrc.ext_driver_src.led_en = VFE_CAMIF_TIMER1_GPIO,
	._fsrc.ext_driver_src.led_flash_en = VFE_CAMIF_TIMER2_GPIO,
	._fsrc.ext_driver_src.flash_id = MAM_CAMERA_EXT_LED_FLASH_SC628A,
};
#endif /* defined(CONFIG_IMX074) */

#if defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED)
int shcamled_pmic_set_torch_led_1_current(unsigned mA);
int pmic_set_flash_led_current(enum pmic8058_leds id, unsigned mA)
{
	int ret = 0;
	ret = shcamled_pmic_set_torch_led_1_current(mA);
	return ret;
}

static struct msm_camera_sensor_flash_src sh_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_PMIC,
	._fsrc.pmic_src.num_of_src = 1,
	._fsrc.pmic_src.low_current  = 40,
#if defined(CONFIG_MACH_DECKARD_AS96)
	._fsrc.pmic_src.high_current = 140,
#else
	._fsrc.pmic_src.high_current = 160,
#endif
	._fsrc.pmic_src.led_src_1 = PM8XXX_ID_LED_1,
	._fsrc.pmic_src.led_src_2 = 0,
	._fsrc.pmic_src.pmic_set_current = pmic_set_flash_led_current,
};
#endif /* defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED) */

static struct msm_gpiomux_config apq8064_cam_2d_configs[] = {
};

static struct msm_bus_vectors cam_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 27648000,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 600000000,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 600000000,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_vectors cam_zsl_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 600000000,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_vectors cam_video_ls_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 348192000,
		.ib  = 617103360,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_vectors cam_dual_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 600000000,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_MM_IMEM,
		.ab  = 43200000,
		.ib  = 69120000,
	},
};

static struct msm_bus_vectors cam_low_power_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1451520,
		.ib  = 3870720,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_paths cam_bus_client_config[] = {
	{
		ARRAY_SIZE(cam_init_vectors),
		cam_init_vectors,
	},
	{
		ARRAY_SIZE(cam_preview_vectors),
		cam_preview_vectors,
	},
	{
		ARRAY_SIZE(cam_video_vectors),
		cam_video_vectors,
	},
	{
		ARRAY_SIZE(cam_snapshot_vectors),
		cam_snapshot_vectors,
	},
	{
		ARRAY_SIZE(cam_zsl_vectors),
		cam_zsl_vectors,
	},
	{
		ARRAY_SIZE(cam_video_ls_vectors),
		cam_video_ls_vectors,
	},
	{
		ARRAY_SIZE(cam_dual_vectors),
		cam_dual_vectors,
	},
	{
		ARRAY_SIZE(cam_low_power_vectors),
		cam_low_power_vectors,
	},
};

static struct msm_bus_scale_pdata cam_bus_client_pdata = {
		cam_bus_client_config,
		ARRAY_SIZE(cam_bus_client_config),
		.name = "msm_camera",
};

static struct msm_camera_device_platform_data msm_camera_csi_device_data[] = {
	{
		.csid_core = 0,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
	{
		.csid_core = 1,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
};

#if defined(CONFIG_IMX074) || defined(CONFIG_S5K3L1YX) || defined(CONFIG_MT9M114) || defined(CONFIG_OV2720)
static struct camera_vreg_t apq_8064_cam_vreg[] = {
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
	{"cam_vaf", REG_LDO, 2800000, 2850000, 300000},
};
#endif /* defined(CONFIG_IMX074) || defined(CONFIG_S5K3L1YX) || defined(CONFIG_MT9M114) || defined(CONFIG_OV2720) */

#if 1
#define CAM_MCLK0		5
#define CAM_MCLK1		4
#define CAML_RSTN		PM8921_GPIO_PM_TO_SYS(37)
#define CAMR_RSTN		PM8921_GPIO_PM_TO_SYS(38)
#else
#define CAML_RSTN PM8921_GPIO_PM_TO_SYS(28)
#define CAMR_RSTN 34
#endif

static struct gpio apq8064_common_cam_gpio[] = {
};

static struct gpio apq8064_back_cam_gpio[] = {
	#if 1
	{CAM_MCLK0,		GPIOF_DIR_IN, "CAMIF_MCLK"},
	{CAML_RSTN,		GPIOF_DIR_OUT, "MCAM_RESET"},
	#else
	{5, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{CAML_RSTN, GPIOF_DIR_OUT, "CAM_RESET"},
	#endif
};

static struct msm_gpio_set_tbl apq8064_back_cam_gpio_set_tbl[] = {
	{CAML_RSTN, GPIOF_OUT_INIT_LOW, 10000},
	{CAML_RSTN, GPIOF_OUT_INIT_HIGH, 10000},
};

static struct msm_camera_gpio_conf apq8064_back_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = apq8064_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(apq8064_cam_2d_configs),
	.cam_gpio_common_tbl = apq8064_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(apq8064_common_cam_gpio),
	.cam_gpio_req_tbl = apq8064_back_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(apq8064_back_cam_gpio),
	.cam_gpio_set_tbl = apq8064_back_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(apq8064_back_cam_gpio_set_tbl),
};

static struct gpio apq8064_front_cam_gpio[] = {
	#if 1
	{CAM_MCLK1,		GPIOF_DIR_IN, "CAMIF_MCLK"},
	{CAMR_RSTN,		GPIOF_DIR_OUT, "SCAM_RESET"},
	#else
	{4, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{12, GPIOF_DIR_IN, "CAMIF_I2C_DATA"},
	{13, GPIOF_DIR_IN, "CAMIF_I2C_CLK"},
	{CAMR_RSTN, GPIOF_DIR_OUT, "CAM_RESET"},
	#endif
};

static struct msm_gpio_set_tbl apq8064_front_cam_gpio_set_tbl[] = {
	{CAMR_RSTN, GPIOF_OUT_INIT_LOW, 10000},
	{CAMR_RSTN, GPIOF_OUT_INIT_HIGH, 10000},
};

static struct msm_camera_gpio_conf apq8064_front_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = apq8064_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(apq8064_cam_2d_configs),
	.cam_gpio_common_tbl = apq8064_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(apq8064_common_cam_gpio),
	.cam_gpio_req_tbl = apq8064_front_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(apq8064_front_cam_gpio),
	.cam_gpio_set_tbl = apq8064_front_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(apq8064_front_cam_gpio_set_tbl),
};

#if defined(CONFIG_IMX074) || defined(CONFIG_S5K3L1YX)
static struct msm_camera_i2c_conf apq8064_back_cam_i2c_conf = {
	.use_i2c_mux = 1,
	.mux_dev = &msm8960_device_i2c_mux_gsbi4,
	.i2c_mux_mode = MODE_L,
};
#endif /* defined(CONFIG_IMX074) || defined(CONFIG_S5K3L1YX) */

#if defined(CONFIG_IMX074)
static struct i2c_board_info msm_act_main_cam_i2c_info = {
	I2C_BOARD_INFO("msm_actuator", 0x11),
};

static struct msm_actuator_info msm_act_main_cam_0_info = {
	.board_info     = &msm_act_main_cam_i2c_info,
	.cam_name   = MSM_ACTUATOR_MAIN_CAM_0,
	.bus_id         = APQ_8064_GSBI4_QUP_I2C_BUS_ID,
	.vcm_pwd        = 0,
	.vcm_enable     = 0,
};
#endif /* defined(CONFIG_IMX074) */

#if 0
static struct i2c_board_info msm_act_main_cam1_i2c_info = {
	I2C_BOARD_INFO("msm_actuator", 0x18),
};

static struct msm_actuator_info msm_act_main_cam_1_info = {
	.board_info     = &msm_act_main_cam1_i2c_info,
	.cam_name       = MSM_ACTUATOR_MAIN_CAM_1,
	.bus_id         = APQ_8064_GSBI4_QUP_I2C_BUS_ID,
	.vcm_pwd        = 0,
	.vcm_enable     = 0,
};
#endif

#if defined(CONFIG_BU63164GWL_ACT)
static struct i2c_board_info msm_act_bu63164gwl_i2c_info = {
	I2C_BOARD_INFO("bu63164gwl_act", 0x1C),
};

static struct msm_actuator_info msm_act_bu63164gwl_info = {
	.board_info	= &msm_act_bu63164gwl_i2c_info,
	.cam_name	= MSM_ACTUATOR_BU63164GWL,
	.bus_id		= APQ_8064_GSBI1_QUP_I2C_BUS_ID,
	.vcm_pwd	= 0,
	.vcm_enable	= 0,
};
#endif /* defined(CONFIG_BU63164GWL_ACT) */

#if defined(CONFIG_BU64291GWZ_ACT)
static struct i2c_board_info msm_act_bu64291gwz_i2c_info = {
	I2C_BOARD_INFO("bu64291gwz_act", 0x18),
};

static struct msm_actuator_info msm_act_bu64291gwz_info = {
	.board_info	= &msm_act_bu64291gwz_i2c_info,
	.cam_name	= MSM_ACTUATOR_BU64291GWZ,
	.bus_id		= APQ_8064_GSBI1_QUP_I2C_BUS_ID,
	.vcm_pwd	= 0,
	.vcm_enable	= 0,
};
#endif /* defined(CONFIG_BU64291GWZ_ACT) */

#if defined(CONFIG_MT9M114) || defined(CONFIG_OV2720)
static struct msm_camera_i2c_conf apq8064_front_cam_i2c_conf = {
	.use_i2c_mux = 1,
	.mux_dev = &msm8960_device_i2c_mux_gsbi4,
	.i2c_mux_mode = MODE_L,
};
#endif /* defined(CONFIG_MT9M114) || defined(CONFIG_OV2720) */

#if defined(CONFIG_IMX074)
static struct msm_camera_sensor_flash_data flash_imx074 = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
	.flash_src	= &msm_flash_src
};

static struct msm_camera_csi_lane_params imx074_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx074 = {
	.mount_angle	= 90,
	.cam_vreg = apq_8064_cam_vreg,
	.num_vreg = ARRAY_SIZE(apq_8064_cam_vreg),
	.gpio_conf = &apq8064_back_cam_gpio_conf,
	.i2c_conf = &apq8064_back_cam_i2c_conf,
	.csi_lane_params = &imx074_csi_lane_params,
};

static struct i2c_board_info imx074_eeprom_i2c_info = {
	I2C_BOARD_INFO("imx074_eeprom", 0x34 << 1),
};

static struct msm_eeprom_info imx074_eeprom_info = {
	.board_info     = &imx074_eeprom_i2c_info,
	.bus_id         = APQ_8064_GSBI4_QUP_I2C_BUS_ID,
};

static struct msm_camera_sensor_info msm_camera_sensor_imx074_data = {
	.sensor_name	= "imx074",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_imx074,
	.sensor_platform_info = &sensor_board_info_imx074,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
	.actuator_info = &msm_act_main_cam_0_info,
	.eeprom_info = &imx074_eeprom_info,
};
#endif /* defined(CONFIG_IMX074) */

#if defined(CONFIG_IMX091)
static struct camera_vreg_t apq_8064_imx091_vreg[] = {
	{"cam_vm18",	REG_VS,  0, 0, 0},
	{"cam_vaf",		REG_LDO, 3000000, 3000000, 300000},
	{"cam_avdd",	REG_LDO, 2800000, 2800000, 150000},
	{"cam_vdig2",	REG_VS,  0, 0, 0},
	{"cam_vio",		REG_VS,  0, 0, 0},
};

static struct msm_camera_sensor_flash_data flash_imx091 = {
	#if defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED)
	.flash_type	= MSM_CAMERA_FLASH_LED,
	.flash_src	= &sh_flash_src
	#else
	.flash_type	= MSM_CAMERA_FLASH_NONE,
	#endif /* defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED) */
};

static struct msm_camera_csi_lane_params imx091_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx091 = {
	.mount_angle		= 90,
	.cam_vreg			= apq_8064_imx091_vreg,
	.num_vreg			= ARRAY_SIZE(apq_8064_imx091_vreg),
	.gpio_conf			= &apq8064_back_cam_gpio_conf,
	#if 0
	.i2c_conf			= &apq8064_back_cam_i2c_conf,
	#endif
	.csi_lane_params	= &imx091_csi_lane_params,
};

#if 0
static struct i2c_board_info imx091_eeprom_i2c_info = {
	I2C_BOARD_INFO("imx091_eeprom", 0x21),
};

static struct msm_eeprom_info imx091_eeprom_info = {
	.board_info     = &imx091_eeprom_i2c_info,
	.bus_id         = APQ_8064_GSBI4_QUP_I2C_BUS_ID,
};
#endif

static struct msm_camera_sensor_info msm_camera_sensor_imx091_data = {
	.sensor_name			= "imx091",
	.pdata					= &msm_camera_csi_device_data[0],
	.flash_data				= &flash_imx091,
	.sensor_platform_info	= &sensor_board_info_imx091,
	.csi_if					= 1,
	.camera_type			= BACK_CAMERA_2D,
	.sensor_type			= BAYER_SENSOR,
	#if defined(CONFIG_BU63164GWL_ACT)
	.actuator_info			= &msm_act_bu63164gwl_info,
	#endif
	#if 0
	.actuator_info			= &msm_act_main_cam_1_info,
	.eeprom_info			= &imx091_eeprom_info,
	#endif
};
#endif /* defined(CONFIG_IMX091) */

#if defined(CONFIG_S5K3L1YX)
static struct msm_camera_sensor_flash_data flash_s5k3l1yx = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_csi_lane_params s5k3l1yx_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k3l1yx = {
	.mount_angle	= 90,
	.cam_vreg = apq_8064_cam_vreg,
	.num_vreg = ARRAY_SIZE(apq_8064_cam_vreg),
	.gpio_conf = &apq8064_back_cam_gpio_conf,
	.i2c_conf = &apq8064_back_cam_i2c_conf,
	.csi_lane_params = &s5k3l1yx_csi_lane_params,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k3l1yx_data = {
	.sensor_name	= "s5k3l1yx",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_s5k3l1yx,
	.sensor_platform_info = &sensor_board_info_s5k3l1yx,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
};
#endif /* defined(CONFIG_S5K3L1YX) */

#if defined(CONFIG_MT9M114)
static struct msm_camera_sensor_flash_data flash_mt9m114 = {
	.flash_type = MSM_CAMERA_FLASH_NONE
};

static struct msm_camera_csi_lane_params mt9m114_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0x1,
};

static struct msm_camera_sensor_platform_info sensor_board_info_mt9m114 = {
	.mount_angle = 90,
	.cam_vreg = apq_8064_cam_vreg,
	.num_vreg = ARRAY_SIZE(apq_8064_cam_vreg),
	.gpio_conf = &apq8064_front_cam_gpio_conf,
	.i2c_conf = &apq8064_front_cam_i2c_conf,
	.csi_lane_params = &mt9m114_csi_lane_params,
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9m114_data = {
	.sensor_name = "mt9m114",
	.pdata = &msm_camera_csi_device_data[1],
	.flash_data = &flash_mt9m114,
	.sensor_platform_info = &sensor_board_info_mt9m114,
	.csi_if = 1,
	.camera_type = FRONT_CAMERA_2D,
	.sensor_type = YUV_SENSOR,
};
#endif /* defined(CONFIG_MT9M114) */

#if defined(CONFIG_OV2720)
static struct msm_camera_sensor_flash_data flash_ov2720 = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_csi_lane_params ov2720_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0x3,
};

static struct msm_camera_sensor_platform_info sensor_board_info_ov2720 = {
	.mount_angle	= 0,
	.cam_vreg = apq_8064_cam_vreg,
	.num_vreg = ARRAY_SIZE(apq_8064_cam_vreg),
	.gpio_conf = &apq8064_front_cam_gpio_conf,
	.i2c_conf = &apq8064_front_cam_i2c_conf,
	.csi_lane_params = &ov2720_csi_lane_params,
};

static struct msm_camera_sensor_info msm_camera_sensor_ov2720_data = {
	.sensor_name	= "ov2720",
	.pdata	= &msm_camera_csi_device_data[1],
	.flash_data	= &flash_ov2720,
	.sensor_platform_info = &sensor_board_info_ov2720,
	.csi_if	= 1,
	.camera_type = FRONT_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
};
#endif /* defined(CONFIG_OV2720) */

#if defined(CONFIG_IMX081)
static struct camera_vreg_t apq_8064_imx081_vreg[] = {
	{"cam_vm18",	REG_VS,  0, 0, 0},
	{"cam_vaf",		REG_LDO, 3000000, 3000000, 300000},
	{"cam_avdd",	REG_LDO, 2800000, 2800000, 150000},
	{"cam_vdig2",	REG_VS,  0, 0, 0},
	{"cam_vio",		REG_VS,  0, 0, 0},
};

static struct msm_camera_sensor_flash_data flash_imx081 = {
	#if defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED)
	.flash_type	= MSM_CAMERA_FLASH_LED,
	.flash_src	= &sh_flash_src
	#else
	.flash_type = MSM_CAMERA_FLASH_NONE,
	#endif /* defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED) */
};

static struct msm_camera_csi_lane_params imx081_csi_lane_params = {
	.csi_lane_assign	= 0xE4,
	.csi_lane_mask		= 0xF,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx081 = {
	.mount_angle		= 90,
	.cam_vreg			= apq_8064_imx081_vreg,
	.num_vreg			= ARRAY_SIZE(apq_8064_imx081_vreg),
	.gpio_conf			= &apq8064_back_cam_gpio_conf,
	.csi_lane_params	= &imx081_csi_lane_params,
};

#if 0
static struct i2c_board_info imx081_eeprom_i2c_info = {
	I2C_BOARD_INFO("imx081_eeprom", 0x22),
};

static struct msm_eeprom_info imx081_eeprom_info = {
	.board_info	= &imx081_eeprom_i2c_info,
	.bus_id		= APQ_8064_GSBI1_QUP_I2C_BUS_ID,
};
#endif

static struct msm_camera_sensor_info msm_camera_sensor_imx081_data = {
	.sensor_name			= "imx081",
	.pdata					= &msm_camera_csi_device_data[0],
	.flash_data				= &flash_imx081,
	.sensor_platform_info	= &sensor_board_info_imx081,
	.csi_if					= 1,
	.camera_type			= BACK_CAMERA_2D,
	.sensor_type			= BAYER_SENSOR,
	#if defined(CONFIG_BU63164GWL_ACT)
	.actuator_info			= &msm_act_bu63164gwl_info,
	#endif /* defined(CONFIG_BU63164GWL_ACT) */
	#if 0
	.eeprom_info			= &imx081_eeprom_info,
	#endif
};
#endif /* defined(CONFIG_IMX081) */

#if defined(CONFIG_S5K8AAYX)
static struct camera_vreg_t apq_8064_s5k8aayx_vreg[] = {
	{"cam_vana", REG_LDO, 2800000, 2800000,  85600},
	{"cam_vdig", REG_LDO, 1225000, 1225000, 105000},
	{"cam_vio2", REG_LDO, 1800000, 1800000, 105000},
};

static struct msm_camera_sensor_flash_data flash_s5k8aayx = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_csi_lane_params s5k8aayx_csi_lane_params = {
	.csi_lane_assign	= 0xE4,
	.csi_lane_mask		= 0x1,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k8aayx = {
	.mount_angle		= 270,
	.cam_vreg			= apq_8064_s5k8aayx_vreg,
	.num_vreg			= ARRAY_SIZE(apq_8064_s5k8aayx_vreg),
	.gpio_conf			= &apq8064_front_cam_gpio_conf,
	.csi_lane_params	= &s5k8aayx_csi_lane_params,
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k8aayx_data = {
	.sensor_name			= "s5k8aayx",
	.pdata					= &msm_camera_csi_device_data[1],
	.flash_data				= &flash_s5k8aayx,
	.sensor_platform_info	= &sensor_board_info_s5k8aayx,
	.csi_if					= 1,
	.camera_type			= FRONT_CAMERA_2D,
	.sensor_type			= YUV_SENSOR,
};
#endif /* defined(CONFIG_S5K8AAYX) */

#if defined(CONFIG_IMX135)
static struct camera_vreg_t apq_8064_imx135_vreg[] = {
	{"cam_vaf",		REG_LDO, 3000000, 3000000, 300000, 0},
	{"cam_vio",		REG_VS,  0, 0, 0, 5},
	{"cam_vdig2",	REG_LDO,  1100000, 1100000, 105000, 5},
	{"cam_avdd",	REG_LDO, 2800000, 2800000, 150000, 5},
	
};

static struct msm_camera_sensor_flash_data flash_imx135 = {
	#if defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED)
	.flash_type	= MSM_CAMERA_FLASH_LED,
	.flash_src	= &sh_flash_src
	#else
	.flash_type	= MSM_CAMERA_FLASH_NONE,
	#endif /* defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED) */
};

static struct msm_camera_csi_lane_params imx135_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx135 = {
	.mount_angle		= 90,
	.cam_vreg			= apq_8064_imx135_vreg,
	.num_vreg			= ARRAY_SIZE(apq_8064_imx135_vreg),
	.gpio_conf			= &apq8064_back_cam_gpio_conf,
	#if 0
	.i2c_conf			= &apq8064_back_cam_i2c_conf,
	#endif
	.csi_lane_params	= &imx135_csi_lane_params,
};

static struct i2c_board_info imx135_eeprom_i2c_info = {
	I2C_BOARD_INFO("imx135_eeprom", 0x21),
};

static struct msm_eeprom_info imx135_eeprom_info = {
	.board_info     = &imx135_eeprom_i2c_info,
	#if 0
	.bus_id         = APQ_8064_GSBI4_QUP_I2C_BUS_ID,
	#else
	.bus_id         = APQ_8064_GSBI1_QUP_I2C_BUS_ID,
	#endif
};

static struct msm_camera_sensor_info msm_camera_sensor_imx135_data = {
	.sensor_name			= "imx135",
	.pdata					= &msm_camera_csi_device_data[0],
	.flash_data				= &flash_imx135,
	.sensor_platform_info	= &sensor_board_info_imx135,
	.csi_if					= 1,
	.camera_type			= BACK_CAMERA_2D,
	.sensor_type			= BAYER_SENSOR,
	#if defined(CONFIG_BU63164GWL_ACT)
	.actuator_info			= &msm_act_bu63164gwl_info,
	#elif defined(CONFIG_BU64291GWZ_ACT)
	.actuator_info			= &msm_act_bu64291gwz_info,
	#endif
	#if 0
	.actuator_info			= &msm_act_main_cam_1_info,
	#endif
	.eeprom_info			= &imx135_eeprom_info,
};
#endif /* defined(CONFIG_IMX135) */

#if defined(CONFIG_IMX132)
static struct msm_camera_csi_lane_params imx132_csi_lane_params = {
	.csi_lane_assign	= 0xE4,
	.csi_lane_mask		= 0x3,
};

static struct camera_vreg_t apq_8064_imx132_vreg[] = {
	{"cam_vdig2", REG_VS,  0, 0, 0},
	{"cam_vio", REG_VS,  0, 0, 0},
	{"cam_vana", REG_LDO, 2800000, 2800000,  85600},
};

static struct msm_camera_sensor_flash_data flash_imx132 = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx132 = {
	.mount_angle		= 270,
	.cam_vreg			= apq_8064_imx132_vreg,
	.num_vreg			= ARRAY_SIZE(apq_8064_imx132_vreg),
	.gpio_conf			= &apq8064_front_cam_gpio_conf,
	.csi_lane_params	= &imx132_csi_lane_params,
};

static struct msm_camera_sensor_info msm_camera_sensor_imx132_data = {
	.sensor_name			= "imx132",
	.pdata					= &msm_camera_csi_device_data[1],
	.flash_data				= &flash_imx132,
	.sensor_platform_info	= &sensor_board_info_imx132,
	.csi_if					= 1,
	.camera_type			= FRONT_CAMERA_2D,
	.sensor_type			= BAYER_SENSOR,
};
#endif /* defined(CONFIG_IMX132) */

#if defined(CONFIG_IMX111_ACT)
static struct i2c_board_info imx111_actuator_i2c_info = {
	I2C_BOARD_INFO("imx111_act", 0x18),
};

static struct msm_actuator_info imx111_actuator_info = {
	.board_info	= &imx111_actuator_i2c_info,
	.cam_name	= MSM_ACTUATOR_MAIN_CAM_1,
	.bus_id		= APQ_8064_GSBI1_QUP_I2C_BUS_ID,
	.vcm_pwd	= 0,
	.vcm_enable	= 0,
};
#endif /* defined(CONFIG_IMX111_ACT) */

#if defined(CONFIG_IMX111)
static struct camera_vreg_t apq_8064_imx111_vreg[] = {
	{"cam_vdig",	REG_VS, 0, 0, 0, 5},
	{"cam_vio",		REG_VS, 0, 0, 0, 5},
	{"cam_vana",	REG_LDO, 2800000, 2800000, 85600,  0},
	{"cam_vaf",		REG_LDO, 3000000, 3000000, 300000, 0},
};

static struct msm_camera_sensor_flash_data flash_imx111 = {
	#if defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED)
	.flash_type	= MSM_CAMERA_FLASH_LED,
	.flash_src	= &sh_flash_src
	#else
	.flash_type	= MSM_CAMERA_FLASH_NONE,
	#endif /* defined(CONFIG_MSM_CAMERA_FLASH) && defined(CONFIG_SHCAMLED) */
};

static struct msm_camera_csi_lane_params imx111_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0x3,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx111 = {
	.mount_angle		= 90,
	.cam_vreg			= apq_8064_imx111_vreg,
	.num_vreg			= ARRAY_SIZE(apq_8064_imx111_vreg),
	.gpio_conf			= &apq8064_back_cam_gpio_conf,
	#if 0
	.i2c_conf			= &apq8064_back_cam_i2c_conf,
	#endif
	.csi_lane_params	= &imx111_csi_lane_params,
};

#ifdef CONFIG_IMX111_EEPROM
static struct i2c_board_info imx111_eeprom_i2c_info = {
	I2C_BOARD_INFO("imx111_eeprom", 0x21),
};

static struct msm_eeprom_info imx111_eeprom_info = {
	.board_info     = &imx111_eeprom_i2c_info,
	.bus_id         = APQ_8064_GSBI1_QUP_I2C_BUS_ID,
};
#endif

static struct msm_camera_sensor_info msm_camera_sensor_imx111_data = {
	.sensor_name			= "imx111",
	.pdata					= &msm_camera_csi_device_data[0],
	.flash_data				= &flash_imx111,
	.sensor_platform_info	= &sensor_board_info_imx111,
	.csi_if					= 1,
	.camera_type			= BACK_CAMERA_2D,
	.sensor_type			= BAYER_SENSOR,
	#if defined(CONFIG_IMX111_ACT)
	.actuator_info			= &imx111_actuator_info,
	#endif
#ifdef CONFIG_IMX111_EEPROM
	.eeprom_info = &imx111_eeprom_info,
#endif
	#if 0
	.actuator_info			= &msm_act_main_cam_1_info,
	.eeprom_info			= &imx111_eeprom_info,
	#endif
};
#endif /* defined(CONFIG_IMX111) */

static struct platform_device msm_camera_server = {
	.name = "msm_cam_server",
	.id = 0,
};

void __init apq8064_init_cam(void)
{
	#if 0
	/* for SGLTE2 platform, do not configure i2c/gpiomux gsbi4 is used for
	 * some other purpose */
	if (socinfo_get_platform_subtype() != PLATFORM_SUBTYPE_SGLTE2) {
		msm_gpiomux_install(apq8064_cam_common_configs,
			ARRAY_SIZE(apq8064_cam_common_configs));
	}
	#endif

	#if 0
	if (machine_is_apq8064_cdp()) {
		sensor_board_info_imx074.mount_angle = 0;
		sensor_board_info_mt9m114.mount_angle = 0;
	} else if (machine_is_apq8064_liquid())
		sensor_board_info_imx074.mount_angle = 180;
	#endif

	platform_device_register(&msm_camera_server);
	#if 0
	if (socinfo_get_platform_subtype() != PLATFORM_SUBTYPE_SGLTE2)
		platform_device_register(&msm8960_device_i2c_mux_gsbi4);
	#endif
	platform_device_register(&msm8960_device_csiphy0);
	platform_device_register(&msm8960_device_csiphy1);
	platform_device_register(&msm8960_device_csid0);
	platform_device_register(&msm8960_device_csid1);
	platform_device_register(&msm8960_device_ispif);
	platform_device_register(&msm8960_device_vfe);
	platform_device_register(&msm8960_device_vpe);
}

#ifdef CONFIG_I2C
static struct i2c_board_info apq8064_camera_i2c_boardinfo[] = {
	#if defined(CONFIG_IMX074)
	{
	I2C_BOARD_INFO("imx074", 0x1A),
	.platform_data = &msm_camera_sensor_imx074_data,
	},
	#endif /* defined(CONFIG_IMX074) */
	#if defined(CONFIG_MT9M114)
	{
	I2C_BOARD_INFO("mt9m114", 0x48),
	.platform_data = &msm_camera_sensor_mt9m114_data,
	},
	#endif /* defined(CONFIG_MT9M114) */
	#if defined(CONFIG_OV2720)
	{
	I2C_BOARD_INFO("ov2720", 0x6C),
	.platform_data = &msm_camera_sensor_ov2720_data,
	},
	#endif /* defined(CONFIG_OV2720) */
	#if defined(CONFIG_MSM_CAMERA_FLASH_SC628A)
	{
	I2C_BOARD_INFO("sc628a", 0x6E),
	},
	#endif /* defined(CONFIG_MSM_CAMERA_FLASH_SC628A) */
	#if defined(CONFIG_IMX091)
	{
	I2C_BOARD_INFO("imx091", 0x34),
	.platform_data = &msm_camera_sensor_imx091_data,
	},
	#endif /* defined(CONFIG_IMX091) */
	#if defined(CONFIG_S5K3L1YX)
	{
	I2C_BOARD_INFO("s5k3l1yx", 0x20),
	.platform_data = &msm_camera_sensor_s5k3l1yx_data,
	},
	#endif /* defined(CONFIG_S5K3L1YX) */
	#if defined(CONFIG_IMX081)
	{
	I2C_BOARD_INFO("imx081", 0x34),
	.platform_data = &msm_camera_sensor_imx081_data,
	},
	#endif /* defined(CONFIG_IMX081) */
	#if defined(CONFIG_S5K8AAYX)
	{
	I2C_BOARD_INFO("s5k8aayx", 0x78),
	.platform_data = &msm_camera_sensor_s5k8aayx_data,
	},
	#endif /* defined(CONFIG_S5K8AAYX) */
	#if defined(CONFIG_IMX135)
	{
	I2C_BOARD_INFO("imx135", 0x20),
	.platform_data = &msm_camera_sensor_imx135_data,
	},
	#endif /* defined(CONFIG_IMX135) */
	#if defined(CONFIG_IMX132)
	{
	I2C_BOARD_INFO("imx132", 0x6c),
	.platform_data = &msm_camera_sensor_imx132_data,
	},
	#endif /* defined(CONFIG_IMX132) */
	#if defined(CONFIG_IMX111)
	{
	I2C_BOARD_INFO("imx111", 0x34),
	.platform_data = &msm_camera_sensor_imx111_data,
	},
	#endif /* defined(CONFIG_IMX111) */
};

struct msm_camera_board_info apq8064_camera_board_info = {
	.board_info = apq8064_camera_i2c_boardinfo,
	.num_i2c_board_info = ARRAY_SIZE(apq8064_camera_i2c_boardinfo),
};
#endif
#endif
