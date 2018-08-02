/* drivers/sharp/shsys/sh_smem_read.c
 *
 * Copyright (C) 2012 Sharp Corporation
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
 
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <sharp/sh_smem.h>

static int check_ks(void)
{
	struct vm_area_struct* vm_area;
	struct file* file;
	char* tmp = NULL;
	char* p = NULL;
	int i;
	int ret = -EPERM;

	tmp = vmalloc(PAGE_SIZE + 1);

	if(tmp == NULL) return -ENOMEM;

	read_lock(&tasklist_lock);

	if(current->mm && current->mm->mmap)
	{
		vm_area = current->mm->mmap;

		for(i = 0; i < current->mm->map_count; i++)
		{
			if(!vm_area)
			{
				break;
			}

			file = vm_area->vm_file;

			if(file)
			{
				if(vm_area->vm_flags & VM_EXEC)
				{
					if(vm_area->vm_flags & VM_WRITE)
					{
						ret = -EPERM;
						break;
					}

					if(vm_area->vm_flags & VM_READ)
					{
						memset(tmp, 0, PAGE_SIZE + 1);

						p = d_path(&file->f_path, tmp, PAGE_SIZE);

						if(p == NULL || (long)p == ENAMETOOLONG)
						{
							vm_area = vm_area->vm_next;
							continue;
						}

						if((strstr(p, "/system/bin/") != p) && (strstr(p, "/system/lib/") != p))
						{
							ret = -EPERM;
							break;
						}

						if(strcmp(p, "/system/bin/ks") == 0) ret = 0;
						if(strcmp(p, "/system/bin/qcks") == 0) ret = 0;
						if(strcmp(p, "/system/bin/doprepend") == 0) ret = 0;
					}
					else
					{
						ret = -EPERM;
						break;
					}
				}
			}
			else
			{
				if(vm_area->vm_flags & VM_EXEC)
				{
					const char* name = arch_vma_name(vm_area);
					
					if(name != NULL)
					{
						if(strcmp(name, "[vectors]") != 0)
						{
							ret = -EPERM;
							break;
						}
					}
					else
					{
						ret = -EPERM;
						break;
					}
				}
			}

			vm_area = vm_area->vm_next;
		}
	}

	read_unlock(&tasklist_lock);

	if(tmp != NULL)
	{
		vfree(tmp);
		tmp = NULL;
	}

	return ret;
}

static int sh_smem_read_mode_open(struct inode *inode, struct file *filp)
{
	int ret;

/*	printk("%s\n", __func__);*/
	ret = check_ks();
	if (ret) {
		printk("[SH]sh_smem_read_mode_open: permission denied %d\n", ret);
		return ret;
	}

	return 0;
}

static ssize_t sh_smem_read_mode_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	sharp_smem_common_type *p_sh_smem_common_type = NULL;
	int ret = -1;

/*	printk("%s\n", __func__);*/
	if ((count != sizeof(sharp_smem_common_type))
		&& (count != sizeof(p_sh_smem_common_type->shdiag_DoPrepend))) {
		printk("[SH]sh_smem_read_mode_read: size error\n");
		return -EINVAL;
	}
	
	p_sh_smem_common_type = sh_smem_get_common_address();
	if( p_sh_smem_common_type != NULL){
		/* user aera */
		if (count == sizeof(sharp_smem_common_type)) {
			ret = copy_to_user(buf, (void *)p_sh_smem_common_type, count);
		}
		else if (count == sizeof(p_sh_smem_common_type->shdiag_DoPrepend)) {
			ret = copy_to_user(buf,
							   (void *)&p_sh_smem_common_type->shdiag_DoPrepend,
							   count);
		}
		if (ret != 0) {
			printk( "copy_to_user failed\n" );
			return -EFAULT;
		}
	} else {
		printk("[SH]sh_smem_read_mode_read: smem_alloc FAILE\n");
	}
	return count;
}

static ssize_t sh_smem_read_mode_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	sharp_smem_common_type *p_sh_smem_common_type = NULL;
	int ret = -1;

/*	printk("%s\n", __func__);*/
	if (count != sizeof(p_sh_smem_common_type->shdiag_DoPrepend)) {
		printk("[SH]sh_smem_read_mode_write: size error\n");
		return -EINVAL;
	}

	p_sh_smem_common_type = sh_smem_get_common_address();
	if( p_sh_smem_common_type != NULL){
		/* user aera */
		if (count == sizeof(p_sh_smem_common_type->shdiag_DoPrepend)) {
			ret = copy_from_user((void *)&p_sh_smem_common_type->shdiag_DoPrepend,
								 buf,
								 count);
		}
		if (ret != 0) {
			printk( "copy_from_user failed\n" );
			return -EFAULT;
		}
	} else {
		printk("[SH]sh_smem_read_mode_write: smem_alloc FAILE\n");
	}
	return count;
}

static long sh_smem_read_mode_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int sh_smem_size;

	sh_smem_size = sizeof(sharp_smem_common_type);
	if (copy_to_user((void __user *)arg, &sh_smem_size, sizeof(sh_smem_size))) {
		printk( "copy_to_user failed\n" );
		return -EFAULT;
	}
	return 0;
}

static int sh_smem_read_mode_release(struct inode *inode, struct file *filp)
{
/*	printk("%s\n", __func__);*/
	return 0;
}

static struct file_operations sh_smem_read_mode_fops = {
	.owner		= THIS_MODULE,
	.open		= sh_smem_read_mode_open,
	.read		= sh_smem_read_mode_read,
	.write		= sh_smem_read_mode_write,
	.unlocked_ioctl	= sh_smem_read_mode_ioctl,
	.release	= sh_smem_read_mode_release,
};

static struct miscdevice sh_smem_read_mode_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sh_smem_read",
	.fops = &sh_smem_read_mode_fops,
};

static int __init sh_smem_read_mode_init( void )
{
	int ret;

	ret = misc_register(&sh_smem_read_mode_dev);
	if (ret) {
		printk("fail to misc_register (sh_smem_read_mode_dev) %d\n", ret);
		return ret;
	}

	return 0;
}

module_init(sh_smem_read_mode_init);

MODULE_DESCRIPTION("sh_smem_read");
MODULE_LICENSE("GPL v2");

