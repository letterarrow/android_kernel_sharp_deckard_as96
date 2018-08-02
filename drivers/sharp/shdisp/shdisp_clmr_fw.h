/* drivers/sharp/shdisp/shdisp_clmr_fw.h  (Lcdc Driver)
 *
 * Copyright (C) 2013 SHARP CORPORATION
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


#if defined(CONFIG_MACH_LYNX_GP6D) && defined(CONFIG_SHDISP_PANEL_GEMINI)
    #include "shdisp_clmr_fw_gp6d.h"
#elif defined(CONFIG_MACH_LYNX_DL32) && defined(CONFIG_SHDISP_PANEL_ANDY)
    #include "shdisp_clmr_fw_DL32.h"
#else
    #include "shdisp_clmr_fw_default.h"
#endif
