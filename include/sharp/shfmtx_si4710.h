/* include/sharp/shfmtx_si4710.h
 *
 * Copyright (C) 2011 SHARP CORPORATION
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
/* CONFIG_SH_AUDIO_DRIVER newly created */

#ifndef __ASM_ARM_ARCH_SHFMTX_SI4710_H
#define __ASM_ARM_ARCH_SHFMTX_SI4710_H

/* CONFIG_SH_AUDIO_DRIVER start */
#if 1
#define SHFMTX_I2C_NAME "shfmtx_i2c"

struct shfmtx_platform_data {
    uint32_t shfmtx_gpio_reset;
};

struct shfmtx_config_data {
    unsigned char *cmd_data;  /* [mode][cmd_len][cmds..] */
    unsigned int mode_num;
    unsigned int data_len;
};

#endif
/* CONFIG_SH_AUDIO_DRIVER end */
#endif
