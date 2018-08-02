/*
 * Copyright (C) 2013 Sharp.
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
/* -------------------------------------------------------------------- */
#include <linux/kernel.h>
#include <linux/module.h> 
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "miyabi_common.h"
#include "miyabi_sysfs.h"
/* -------------------------------------------------------------------- */
#define MIYABICDEV_KER_BASEMINOR		(0)
#define MIYABICDEV_KER_MINORCOUNT		(1)
#define MIYABICDEV_KER_DRVNAME		"miyabi_cdev"
#define MIYABICDEV_CLASS_NAME		"cls_miyabi_cdev"
/* -------------------------------------------------------------------- */
typedef struct{
	int major;
	dev_t dev;
	struct cdev miyabi_cdev_cdev;
	struct class* miyabi_cdev_classp;
	int status;
}miyabi_cdev_info_type;
miyabi_cdev_info_type miyabi_cdev_info;
/* -------------------------------------------------------------------- */
static int		miyabi_cdev_open(struct inode* inode, struct file* filp);
static int		miyabi_cdev_close(struct inode* inode, struct file* filp);
static unsigned int	miyabi_cdev_poll(struct file* filp, poll_table* wait);
/* -------------------------------------------------------------------- */
static struct file_operations miyabi_cdev_Ops = {
	.owner   = THIS_MODULE,
	.poll    = miyabi_cdev_poll,
	.open    = miyabi_cdev_open,
	.release = miyabi_cdev_close,
};
/* -------------------------------------------------------------------- */
static int __init miyabi_cdev_ker_init(void)
{
	int sdResult;
	struct device* devp;

	miyabi_cdev_info.major = -1;
	miyabi_cdev_info.miyabi_cdev_classp = NULL;
	miyabi_cdev_info.status = 0;
	
	miyabi_cdev_info.dev = MKDEV(miyabi_cdev_info.major, 0);
	
	sdResult = alloc_chrdev_region( &miyabi_cdev_info.dev, MIYABICDEV_KER_BASEMINOR, MIYABICDEV_KER_MINORCOUNT, MIYABICDEV_KER_DRVNAME );
	if( sdResult < 0 ){
		return -1;
	}
	miyabi_cdev_info.major = sdResult;
	

	cdev_init( &miyabi_cdev_info.miyabi_cdev_cdev, &miyabi_cdev_Ops );
	miyabi_cdev_info.miyabi_cdev_cdev.owner = THIS_MODULE;
	miyabi_cdev_info.miyabi_cdev_cdev.ops = &miyabi_cdev_Ops;

	sdResult = cdev_add(&miyabi_cdev_info.miyabi_cdev_cdev, miyabi_cdev_info.dev, MIYABICDEV_KER_MINORCOUNT);
	if( sdResult < 0 ){
		return -1;
	}

	
	miyabi_cdev_info.miyabi_cdev_classp = class_create( THIS_MODULE, MIYABICDEV_CLASS_NAME );
	if (IS_ERR(miyabi_cdev_info.miyabi_cdev_classp)){
		return -1;
	}

	devp = device_create( miyabi_cdev_info.miyabi_cdev_classp, NULL, miyabi_cdev_info.dev, NULL, MIYABICDEV_KER_DRVNAME );
	sdResult = IS_ERR(devp) ? PTR_ERR(devp) : 0;
	if ( sdResult < 0 ){
		return -1;
	}
	
	return 0;
}
/* -------------------------------------------------------------------- */
static void __exit miyabi_cdev_ker_term( void )
{
	if( miyabi_cdev_info.major < 0){
		return;
	}
	
	device_destroy( miyabi_cdev_info.miyabi_cdev_classp, miyabi_cdev_info.dev );
	class_destroy( miyabi_cdev_info.miyabi_cdev_classp );
	miyabi_cdev_info.miyabi_cdev_classp = NULL;

	cdev_del( &miyabi_cdev_info.miyabi_cdev_cdev );
	unregister_chrdev_region( miyabi_cdev_info.dev, MIYABICDEV_KER_MINORCOUNT );
	miyabi_cdev_info.major = -1;

	return;
}
/* -------------------------------------------------------------------- */
/*  */
/* -------------------------------------------------------------------- */
static int miyabi_cdev_open(struct inode* inode, struct file* filp)
{
	int ret = -EPERM;
	char *binbuf = NULL, *bin = NULL;

	do
	{
		if(miyabi_sysfs_get_proven_pid() != -1) break;

		if(current->parent == NULL) break;

		if(current->parent->pid != 1) break;

		binbuf = kmalloc(PATH_MAX, GFP_KERNEL);

		if(binbuf == NULL)
		{
			ret = -ENOMEM;

			break;
		}

		memset(binbuf, 0, PATH_MAX);

		read_lock(&tasklist_lock);

		bin = miyabi_detect_binary(current, binbuf);

		read_unlock(&tasklist_lock);

		if(bin == NULL)
		{
			ret = -ENOMEM;

			break;
		}

		if(strcmp(bin, "/sbin/dmmd") == 0)
		{
			miyabi_sysfs_set_proven_pid(current->pid);

			ret = 0;
		}

	}
	while(0);

	if(binbuf != NULL)
	{
		kfree(binbuf);
		binbuf = NULL;
	}

	return ret;
}
/* -------------------------------------------------------------------- */
static int miyabi_cdev_close(struct inode* inode, struct file* filp)
{
	return 0;
}
/* -------------------------------------------------------------------- */
static unsigned int miyabi_cdev_poll(struct file* filp, poll_table* wait)
{
	unsigned int retmask = 0;

	miyabi_sysfs_wait_for();
	
	retmask |= (POLLIN | POLLRDNORM);

	return retmask;
}
/* -------------------------------------------------------------------- */
module_init( miyabi_cdev_ker_init );
module_exit( miyabi_cdev_ker_term );
/* -------------------------------------------------------------------- */
MODULE_AUTHOR("SHARP");
MODULE_DESCRIPTION("miyabi_cdev device");
MODULE_LICENSE("GPL2");
/* -------------------------------------------------------------------- */

