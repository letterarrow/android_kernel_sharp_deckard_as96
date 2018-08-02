/* drivers/sharp/shcamsensor/sh_sensor.h  (Camera Driver)
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

#ifndef __SH_CAMSENSOR_H
#define __SH_CAMSENSOR_H

#define SH_CAMSENSOR_MAGIC			'c'
#define SH_CAMSENSOR_BASE			0
#define SH_CAMSENSOR_FILE_NAME		"/dev/shcamsensor"

#define SH_CAMSENSOR_SET_SMEM \
	_IOW(SH_CAMSENSOR_MAGIC, SH_CAMSENSOR_BASE + 1, void*)
#define SH_CAMSENSOR_GET_SMEM \
	_IOR(SH_CAMSENSOR_MAGIC, SH_CAMSENSOR_BASE + 2, void*)


struct shcam_smem_ctrl {
	void *usaddr;
	unsigned long offset;
	unsigned long length;
};

#endif /* __SH_CAMSENSOR_H */
