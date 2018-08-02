/* drivers/sharp/shcamsensor/sh_camsensor.c  (Camera Driver)
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <mach/board.h>

#include <sharp/sh_smem.h>
#include <sharp/sh_camsensor.h>

#define SH_CAMSENSOR_CLASS_NAME		"shcamsensor"
#define SH_CAMSENSOR_DEVICE_NAME	"shcamsensor"
#define SH_CAMSENSOR_NODE_NAME		"shcamsensor"
#define SH_CAMSENSOR_MINORNUM_BASE	0
#define SH_CAMSENSOR_DEVICE_COUNT	1

//#define SH_CAMSENSOR_DEBUG
#ifdef SH_CAMSENSOR_DEBUG
#define CDBG(fmt, args...) pr_info("sh_camsensor: " fmt, ##args)
#else
#define CDBG(fmt, args...) do {} while (0)
#endif

static struct class*	sh_camsensor_class = NULL;
static struct device*	sh_camsensor_class_dev = NULL;
static struct cdev		sh_camsensor_cdev;
static struct mutex		sh_camsensor_mutex_lock;
static dev_t			sh_camsensor_devno = 0;
static int				sh_camsensor_node = 0;


static int shCamSensorSetSmem(struct shcam_smem_ctrl *ctrl)
{
	int rc = -EFAULT;
	sharp_smem_common_type* sh_smem_common = NULL;
	void __user* argp = (void __user*)ctrl->usaddr;

	CDBG("%s. addr %p, ofst %ld, len %ld\n", __func__,
		 argp, ctrl->offset, ctrl->length);

	sh_smem_common = sh_smem_get_common_address();
	if(NULL != sh_smem_common) {
		if(copy_from_user(
				(void*)&sh_smem_common->sh_camOtpData[ctrl->offset],
				argp,
				ctrl->length)) {
			pr_err("[%d] copy_from_user error.\n", __LINE__);
		}
		else {
			rc = 0;
		}
	}

	return rc;
}

static int shCamSensorGetSmem(struct shcam_smem_ctrl *ctrl)
{
	int rc = -EFAULT;
	sharp_smem_common_type* sh_smem_common = NULL;
	void __user* argp = (void __user*)ctrl->usaddr;

	CDBG("%s. addr %p, ofst %ld, len %ld\n", __func__,
		 argp, ctrl->offset, ctrl->length);

	sh_smem_common = sh_smem_get_common_address();
	if(NULL != sh_smem_common) {
		if(copy_to_user(
				argp,
				(void*)&sh_smem_common->sh_camOtpData[ctrl->offset],
				ctrl->length)) {
			pr_err("[%d] copy_to_user() error.\n", __LINE__);
		}
		else {
			rc = 0;
		}
	}

	return rc;
}

static int sh_camsensor_open(struct inode *inode, struct file *filep)
{
	mutex_init(&sh_camsensor_mutex_lock);

	return 0;
}

static int sh_camsensor_release(struct inode *inode, struct file *filep)
{
	mutex_destroy(&sh_camsensor_mutex_lock);

	return 0;
}

static long sh_camsensor_ioctl(struct file *filep,
							unsigned int cmd, unsigned long arg)
{
	int rc = -EINVAL;
	struct shcam_smem_ctrl ctrl;

	mutex_lock(&sh_camsensor_mutex_lock);

	if (copy_from_user(&ctrl,
			(void __user*)arg,
			sizeof(struct shcam_smem_ctrl))) {
		pr_err("[%d] copy_from_user error.\n", __LINE__);

		mutex_unlock(&sh_camsensor_mutex_lock);
		return -EFAULT;
	}

	CDBG("sh_camsensor_ioctl. cmd=%d\n", cmd);
	switch (cmd) {
	case SH_CAMSENSOR_SET_SMEM:
		rc = shCamSensorSetSmem(&ctrl);
		break;
	case SH_CAMSENSOR_GET_SMEM:
		rc = shCamSensorGetSmem(&ctrl);
		break;
	default:
		pr_err("sh_camsensor_ioctls:[%d] default error\n", __LINE__);
		break;
	}
	mutex_unlock(&sh_camsensor_mutex_lock);

	return (long)rc;
}

static const struct file_operations sh_camsensor_fops = {
	.owner			= THIS_MODULE,
	.open			= sh_camsensor_open,
	.release		= sh_camsensor_release,
	.unlocked_ioctl	= sh_camsensor_ioctl,
	.read			= NULL,
	.write			= NULL,
};

static int __init sh_camsensor_drv_start(void)
{
	int rc = -ENODEV;
	dev_t devno;

	CDBG("sh_camsensor_drv_start.\n");
	do {
		if(SH_CAMSENSOR_DEVICE_COUNT <= sh_camsensor_node) {
			pr_err("[%d] device count error. %d\n",
						__LINE__, sh_camsensor_node);
			goto errlevel1;
		}

		if(NULL == sh_camsensor_class) {
			sh_camsensor_class = class_create(
									THIS_MODULE, SH_CAMSENSOR_CLASS_NAME);
			if(IS_ERR(sh_camsensor_class)) {
				rc = PTR_ERR(sh_camsensor_class);
				pr_err("[%d] class create error. %d\n", __LINE__, rc);
				goto errlevel2;
			}
		}

		rc = alloc_chrdev_region(&sh_camsensor_devno,
									SH_CAMSENSOR_MINORNUM_BASE,
									SH_CAMSENSOR_DEVICE_COUNT,
									SH_CAMSENSOR_DEVICE_NAME);
		if(0 > rc) {
			pr_err("[%d] alloc chrdev region error. %d\n", __LINE__, rc);
			goto errlevel3;
		}

		devno = MKDEV(MAJOR(sh_camsensor_devno), SH_CAMSENSOR_MINORNUM_BASE);
		sh_camsensor_class_dev = device_create(sh_camsensor_class,
											NULL, devno,
											NULL, SH_CAMSENSOR_NODE_NAME);
		if(IS_ERR(sh_camsensor_class_dev)) {
			rc = PTR_ERR(sh_camsensor_class_dev);
			pr_err("[%d] device create error. %d\n", __LINE__, rc);
			goto errlevel4;
		}

		cdev_init(&sh_camsensor_cdev, &sh_camsensor_fops);
		sh_camsensor_cdev.owner = THIS_MODULE;

		rc = cdev_add(&sh_camsensor_cdev,
						devno,
						SH_CAMSENSOR_DEVICE_COUNT);
		if(0 > rc) {
			pr_err("[%d] device create error. %d\n", __LINE__, rc);
			goto errlevel5;
		}

		sh_camsensor_node++;

	} while(0);

	CDBG("sh_camsensor_drv_start end.\n");
	return 0;

//errlevel6:
//	cdev_del(&sh_camsensor_cdev);
errlevel5:
	device_destroy(sh_camsensor_class, sh_camsensor_devno);
	sh_camsensor_class_dev = NULL;
errlevel4:
	unregister_chrdev_region(sh_camsensor_devno, SH_CAMSENSOR_DEVICE_COUNT);
	sh_camsensor_devno = 0;
errlevel3:
	class_destroy(sh_camsensor_class);
	sh_camsensor_class = NULL;
errlevel2:
errlevel1:

	return rc;
}

static void __exit sh_camsensor_drv_remove(void)
{
	CDBG("sh_camsensor_drv_remove.\n");

	do {
		if(0 == sh_camsensor_node) {
			CDBG("error. sh_camsensor_node = %d\n", sh_camsensor_node);
			break;
		}

		cdev_del(&sh_camsensor_cdev);

		device_destroy(sh_camsensor_class, sh_camsensor_devno);
		sh_camsensor_class_dev = NULL;

		unregister_chrdev_region(sh_camsensor_devno,
									SH_CAMSENSOR_DEVICE_COUNT);

		class_destroy(sh_camsensor_class);
		sh_camsensor_class = NULL;

		sh_camsensor_node = 0;
	} while(0);

	CDBG("sh_camsensor_drv_remove end.\n");
}

module_init(sh_camsensor_drv_start);
module_exit(sh_camsensor_drv_remove);

MODULE_DESCRIPTION("SHARP CAMERA DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");
