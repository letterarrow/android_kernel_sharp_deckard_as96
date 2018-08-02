/* include/sharp/shdiag_smd.h
 *
 * Copyright (C) 2010 Sharp Corporation
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

#ifndef _SHDIAG_SMD_H_
#define _SHDIAG_SMD_H_

/*
 * Defines
 */

#define SHDIAG_SMD_DEVFILE "/dev/smd_read"

#define SHDIAG_IOC_MAGIC 's'
#define SHDIAG_IOCTL_SET_QRDATA           _IOW  (SHDIAG_IOC_MAGIC,  0, struct shdiag_qrdata)
#define SHDIAG_IOCTL_SET_QXDMFLG          _IOW  (SHDIAG_IOC_MAGIC,  1, unsigned char)
#define SHDIAG_IOCTL_SET_PROADJ           _IOW  (SHDIAG_IOC_MAGIC,  2, struct shdiag_procadj)
#define SHDIAG_IOCTL_GET_HW_REVISION      _IOR  (SHDIAG_IOC_MAGIC,  3, unsigned long)
#define SHDIAG_IOCTL_SET_HAPTICSCAL       _IOW  (SHDIAG_IOC_MAGIC,  4, struct shdiag_hapticscal)
#define SHDIAG_IOCTL_SET_CPRXHILOOFFSET   _IOW  (SHDIAG_IOC_MAGIC,  5, struct shdiag_cprxhilow)
#define SHDIAG_IOCTL_SET_CPRXCALOFFSET    _IOW  (SHDIAG_IOC_MAGIC,  6, struct shdiag_cprxcal)
/*
 * TYPES
 */

struct smem_comm_mode {
	unsigned long BootMode;
	unsigned long UpDateFlg;
};

#define SHDIAG_QRDATA_SIZE    0x80
struct shdiag_qrdata {
    unsigned char buf[SHDIAG_QRDATA_SIZE];
};

struct shdiag_procadj {
    unsigned long proxcheckdata_min;
    unsigned long proxcheckdata_max;
};

#define SHDIAG_HAPTICSCAL_SIZE 0x03
struct shdiag_hapticscal {
	unsigned char buf[SHDIAG_HAPTICSCAL_SIZE];
};

#define SHDIAG_CPRXHILOW_SIZE     0x30
#define SHDIAG_CPRXHILOW_SIZE_ONE 0x04
struct shdiag_cprxhilow {
	unsigned char stage;
	unsigned short buf[2];
};

#define SHDIAG_CPRXCAL_SIZE     0x18
struct shdiag_cprxcal {
	unsigned char stage;
	unsigned short buf;
};

/*End of File*/
#endif /* _SHDIAG_SMD_H_ */
