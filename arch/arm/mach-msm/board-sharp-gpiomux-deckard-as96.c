/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include "devices.h"
#include "board-8064.h"


static struct gpiomux_setting ap2mdm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_status_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_errfatal_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_pblrdy = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ap2mdm_soft_reset_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ap2mdm_wakeup = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config mdm_configs[] __initdata = {
	/* AP2MDM_STATUS */
	{
#ifdef CONFIG_SHSYS_CUST
		.gpio = 62,
#else
		.gpio = 48,
#endif
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* MDM2AP_STATUS */
	{
		.gpio = 49,
		.settings = {
			[GPIOMUX_ACTIVE] = &mdm2ap_status_cfg,
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_cfg,
		}
	},
	/* MDM2AP_ERRFATAL */
	{
		.gpio = 19,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_errfatal_cfg,
		}
	},
	/* AP2MDM_ERRFATAL */
	{
		.gpio = 18,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* AP2MDM_SOFT_RESET, aka AP2MDM_PON_RESET_N */
	{
		.gpio = 27,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_soft_reset_cfg,
		}
	},
	/* AP2MDM_WAKEUP */
	{
		.gpio = 35,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_wakeup,
		}
	},
	/* MDM2AP_PBL_READY*/
	{
#ifdef CONFIG_SHSYS_CUST
		.gpio = 84,
#else
		.gpio = 46,
#endif
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_pblrdy,
		}
	},
};

static struct gpiomux_setting hsic_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting hsic_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting hsic_wakeup_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hsic_wakeup_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config apq8064_hsic_configs[] = {
	{
		.gpio = 88,               /*HSIC_STROBE */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 89,               /* HSIC_DATA */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_sus_cfg,
		},
	},
	{
		.gpio = 47,              /* wake up */
		.settings = {
			[GPIOMUX_ACTIVE] = &hsic_wakeup_act_cfg,
			[GPIOMUX_SUSPENDED] = &hsic_wakeup_sus_cfg,
		},
	},
};

/* suspended */
static struct gpiomux_setting sh_sus_func1_np_2ma_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_sus_func1_np_6ma_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_sus_func1_pd_2ma_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting sh_sus_func1_pu_2ma_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct gpiomux_setting sh_sus_func2_np_6ma_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_sus_func2_np_8ma_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_sus_func2_pd_8ma_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting sh_sus_func2_pu_6ma_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct gpiomux_setting sh_sus_func3_np_2ma_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_sus_func4_np_2ma_cfg = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_sus_gpio_np_2ma_in_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting sh_sus_gpio_np_2ma_out_high_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};
static struct gpiomux_setting sh_sus_gpio_np_2ma_out_low_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};
static struct gpiomux_setting sh_sus_gpio_pd_2ma_in_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting sh_sus_gpio_pu_2ma_in_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting sh_sus_gpio_pu_6ma_in_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

/* active */
static struct gpiomux_setting sh_act_func1_np_2ma_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_act_func1_np_6ma_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_act_func1_pd_2ma_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting sh_act_func1_pu_2ma_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct gpiomux_setting sh_act_func2_np_6ma_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_act_func2_np_8ma_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_act_func2_pd_8ma_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting sh_act_func2_pu_6ma_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct gpiomux_setting sh_act_func3_np_2ma_cfg = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_act_func4_np_2ma_cfg = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting sh_act_gpio_np_2ma_in_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting sh_act_gpio_np_2ma_out_high_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};
static struct gpiomux_setting sh_act_gpio_np_2ma_out_low_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};
static struct gpiomux_setting sh_act_gpio_pd_2ma_in_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting sh_act_gpio_pu_2ma_in_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};
static struct gpiomux_setting sh_act_gpio_pu_6ma_in_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config sh_apq8064_gpio_configs[] __initdata = {
	{
		.gpio = 0,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_pd_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_pd_2ma_cfg,
		},
	},
	{
		.gpio = 1,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_out_low_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_out_low_cfg,
		},
	},
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 4,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_np_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_np_6ma_cfg,
		},
	},
	{
		.gpio = 5,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_6ma_cfg,
		},
	},
	{
		.gpio = 6,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 7,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_pd_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_pd_2ma_cfg,
		},
	},
	{
		.gpio = 8,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 9,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 10,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 11,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_pu_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_pu_2ma_cfg,
		},
	},
	{
		.gpio = 12,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 13,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 14,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_np_8ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_np_8ma_cfg,
		},
	},
	{
		.gpio = 15,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_pd_8ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_pd_8ma_cfg,
		},
	},
	{
		.gpio = 16,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_np_8ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_np_8ma_cfg,
		},
	},
	{
		.gpio = 17,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_np_8ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_np_8ma_cfg,
		},
	},
	{
		.gpio = 20,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 21,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 22,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 23,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 26,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pu_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 28,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 29,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pu_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 31,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_in_cfg,
		},
	},
	{
		.gpio = 32,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 34,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 36,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_in_cfg,
		},
	},
	{
		.gpio = 37,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pu_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pu_2ma_in_cfg,
		},
	},
	{
		.gpio = 38,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_in_cfg,
		},
	},
	{
		.gpio = 39,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 40,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 41,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 42,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_in_cfg,
		},
	},
	{
		.gpio = 43,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_6ma_cfg,
		},
	},
	{
		.gpio = 44,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_2ma_cfg,
		},
	},
	{
		.gpio = 45,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_6ma_cfg,
		},
	},
	{
		.gpio = 46,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_np_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_np_6ma_cfg,
		},
	},
	{
		.gpio = 48,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 50,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 51,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pu_6ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pu_6ma_in_cfg,
		},
	},
	{
		.gpio = 52,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pu_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pu_2ma_in_cfg,
		},
	},
	{
		.gpio = 53,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pu_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pu_2ma_in_cfg,
		},
	},
	{
		.gpio = 54,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pu_6ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pu_6ma_in_cfg,
		},
	},
	{
		.gpio = 55,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_pu_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_pu_2ma_cfg,
		},
	},
	{
		.gpio = 56,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_pu_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_pu_2ma_cfg,
		},
	},
	{
		.gpio = 57,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func1_pu_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func1_pu_2ma_cfg,
		},
	},
	{
		.gpio = 58,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_in_cfg,
		},
	},
	{
		.gpio = 59,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 60,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 61,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_in_cfg,
		},
	},
	{
		.gpio = 63,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_pu_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_pu_6ma_cfg,
		},
	},
	{
		.gpio = 64,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_pu_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_pu_6ma_cfg,
		},
	},
	{
		.gpio = 65,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_pu_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_pu_6ma_cfg,
		},
	},
	{
		.gpio = 66,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_pu_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_pu_6ma_cfg,
		},
	},
	{
		.gpio = 67,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_pu_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_pu_6ma_cfg,
		},
	},
	{
		.gpio = 68,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func2_np_6ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func2_np_6ma_cfg,
		},
	},
	{
		.gpio = 69,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 70,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 71,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 72,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 77,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_in_cfg,
		},
	},
	{
		.gpio = 81,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_out_low_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_out_high_cfg,
		},
	},
	{
		.gpio = 82,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func4_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func4_np_2ma_cfg,
		},
	},
	{
		.gpio = 83,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_func3_np_2ma_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_func3_np_2ma_cfg,
		},
	},
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
	{
		.gpio = 86,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_np_2ma_out_high_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_np_2ma_out_high_cfg,
		},
	},
	{
		.gpio = 87,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sh_act_gpio_pd_2ma_in_cfg,
			[GPIOMUX_SUSPENDED] = &sh_sus_gpio_pd_2ma_in_cfg,
		},
	},
};

void __init apq8064_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err("msm_gpiomux_init failed %d\n", rc);
		return;
	}

	msm_gpiomux_install(mdm_configs,
			ARRAY_SIZE(mdm_configs));

	msm_gpiomux_install(apq8064_hsic_configs,
			ARRAY_SIZE(apq8064_hsic_configs));

	sh_msm_gpiomux_install(sh_apq8064_gpio_configs,
			ARRAY_SIZE(sh_apq8064_gpio_configs));
}
