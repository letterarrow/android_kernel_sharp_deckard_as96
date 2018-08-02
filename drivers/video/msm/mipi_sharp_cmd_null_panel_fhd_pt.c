/* drivers/video/msm/mipi_sharp_cmd_null_panel_fhd_pt.c  (Display Driver)
 *
 * Copyright (C) 2012-2013 SHARP CORPORATION
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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include <sharp/shdisp_kerl.h>
#include "mipi_sharp_clmr.h"

static int __init mipi_cmd_sharp_null_panel_fhd_pt_init(void)
{
	return 0;
}

module_init(mipi_cmd_sharp_null_panel_fhd_pt_init);
