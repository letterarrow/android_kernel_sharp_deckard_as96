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
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/fdtable.h>

#include "miyabi_common.h"
#include "miyabi_sysfs.h"
/* -------------------------------------------------------------------- */
#define LOCAL_MIYABI_PACKAGE_MAX_LEN	(32)
#define LOCAL_MIYABI_DATALIST_LENGTH 	(8192)
#define LOCAL_MIYABI_MAX_KILLED_PIDS	(1000)
#define miyabi_printk			if(0)printk

#define CREATE_KOBJECT(dir, file)										\
	static void dir##_release(struct kobject *kobj);					\
	static ssize_t dir##_show(struct kobject *kobj, struct attribute *attr, char *buf); \
	static ssize_t dir##_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len);	\
	static struct kobject dir##_kobj;									\
	static struct kobj_attribute dir##_attribute = __ATTR(file, 0600, NULL, NULL);	\
	static struct attribute *dir##_default_attrs[] = {					\
		&dir##_attribute.attr,											\
		NULL,	/* need to NULL terminate the list of attributes */		\
	};																	\
	static struct sysfs_ops dir##_sysfs_ops = {						\
		.show = dir##_show,											\
		.store = dir##_store,											\
	};																	\
	static struct kobj_type dir##_ktype = {							\
		.release = dir##_release,										\
		.sysfs_ops = &dir##_sysfs_ops,									\
		.default_attrs = dir##_default_attrs,							\
	};																	\
	static kobj_data dir##_data = {										\
		.name = #dir,													\
		.kobj = &dir##_kobj,											\
		.ktype = &dir##_ktype,											\
	};																	\

/* -------------------------------------------------------------------- */
typedef struct
{
	const char *name;
	struct kobject *kobj;
	struct kobj_type *ktype;
} kobj_data;
/* -------------------------------------------------------------------- */
DEFINE_MUTEX( miyabi_sysfs_mutex );
CREATE_KOBJECT(killed, pids);
CREATE_KOBJECT(package, name);
/* -------------------------------------------------------------------- */
static pid_t package_held_pid = -1;
static struct kset* miyabi_sysfs_kset = NULL;
static wait_queue_head_t miyabi_sysfs_q;
static pid_t miyabi_sysfs_pids[LOCAL_MIYABI_MAX_KILLED_PIDS];
static int miyabi_sysfs_pids_pos = 0;
static pid_t miyabi_sysfs_proven_pid = -1;
/* -------------------------------------------------------------------- */
void miyabi_sysfs_task_release(pid_t pid)
{
	mutex_lock(&miyabi_sysfs_mutex);

	if(miyabi_sysfs_pids_pos < LOCAL_MIYABI_MAX_KILLED_PIDS)
	{
		miyabi_sysfs_pids[miyabi_sysfs_pids_pos] = pid;

		miyabi_sysfs_pids_pos++;
	}

	mutex_unlock(&miyabi_sysfs_mutex);

	wake_up_interruptible(&miyabi_sysfs_q);
}
/* -------------------------------------------------------------------- */
int miyabi_sysfs_wait_for(void)
{
	if(wait_event_interruptible(miyabi_sysfs_q, (miyabi_sysfs_pids_pos > 0))) return -ERESTARTSYS;

	return 0;
}
/* -------------------------------------------------------------------- */
void miyabi_sysfs_set_proven_pid(pid_t pid)
{
	if(miyabi_sysfs_proven_pid == -1)
	{
		miyabi_sysfs_proven_pid = pid;
	}
	else
	{
		miyabi_sysfs_proven_pid = -2;
	}

	miyabi_printk("miyabi_sysfs_set_proven_pid : %d\n", miyabi_sysfs_proven_pid);
}
/* -------------------------------------------------------------------- */
pid_t miyabi_sysfs_get_proven_pid(void)
{
	return miyabi_sysfs_proven_pid;
}
/* -------------------------------------------------------------------- */
static void killed_release(struct kobject *kobj)
{
	kfree(kobj);
}
/* -------------------------------------------------------------------- */
static ssize_t killed_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	int ret = 0;
	
	mutex_lock(&miyabi_sysfs_mutex);

	if(miyabi_sysfs_pids_pos > 0)
	{
		memcpy(buf, &miyabi_sysfs_pids[0], sizeof(pid_t) * miyabi_sysfs_pids_pos);

		ret = sizeof(pid_t) * miyabi_sysfs_pids_pos;

		miyabi_sysfs_pids_pos = 0;
	}

	mutex_unlock(&miyabi_sysfs_mutex);
	
	return ret;
}
/* -------------------------------------------------------------------- */
static ssize_t killed_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
	return 0;
}
/* -------------------------------------------------------------------- */
static void package_release(struct kobject *kobj)
{
	kfree(kobj);
}
/* -------------------------------------------------------------------- */
static ssize_t package_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct task_struct* process;
	struct mm_struct* mm;
	struct vm_area_struct* vm_area;
	struct file* file;
	int i;
	char *packbuf = NULL, *pack = NULL;
	char *binbuf = NULL, *bin = NULL;
	char *pathbuf = NULL, *path = NULL;
	char* p_data = NULL;
	int data_len = 0;
	int failed = 0;

	miyabi_printk("package_show()\n");

	if(package_held_pid == -1) return 0;
	if(current->tgid != miyabi_sysfs_proven_pid) return 0;

	miyabi_printk("held pid 2: %d\n", package_held_pid);

	do
	{
		packbuf = kmalloc(PATH_MAX, GFP_KERNEL);

		if(packbuf == NULL) break;

		binbuf = kmalloc(PATH_MAX, GFP_KERNEL);

		if(binbuf == NULL) break;

		pathbuf = kmalloc(PATH_MAX, GFP_KERNEL);

		if(pathbuf == NULL) break;

		p_data = kmalloc(LOCAL_MIYABI_DATALIST_LENGTH, GFP_KERNEL);

		if(p_data == NULL) break;

		/* --- */

		memset(packbuf, 0, PATH_MAX);
		memset(binbuf, 0, PATH_MAX);
		memset(pathbuf, 0, PATH_MAX);
		memset(p_data, 0, PATH_MAX);

		read_lock(&tasklist_lock);

		do
		{
			process = find_task_by_vpid(package_held_pid);

			if(process == NULL) break;
			if(process->mm == NULL) break;
			if(process->mm->mmap == NULL) break;

			bin = miyabi_detect_binary(process, binbuf);

			if(bin == NULL) break;

			if(!(	strstr(bin, LOCAL_MIYABI_DIR_SYSTEMBIN) == bin ||
				strstr(bin, LOCAL_MIYABI_DIR_SYSTEMSHBIN) == bin ||
				strstr(bin, LOCAL_MIYABI_DIR_VENDORBIN) == bin ||
				strstr(bin, LOCAL_MIYABI_DIR_SYSTEMAPP) == bin ||
				strstr(bin, LOCAL_MIYABI_DIR_VENDORAPP) == bin ||
				(strstr(bin, LOCAL_MIYABI_DIR_DATAAPP) == bin && strstr(bin, LOCAL_MIYABI_SUFFIX_APK) != NULL) ||
				(strstr(bin, LOCAL_MIYABI_DIR_DATAAPPPRIVATE) == bin && strstr(bin, LOCAL_MIYABI_SUFFIX_APK) != NULL )
			))
			{
				break;
			}

			if(strcmp(bin, LOCAL_MIYABI_PATH_APPPROCESS) == 0)
			{
				pack = miyabi_detect_package(process, packbuf);

				if(pack == NULL) break;
			}

			mm = process->mm;
			vm_area = mm->mmap;

			for(i = 0; i < mm->map_count; i++, vm_area = vm_area->vm_next)
			{
				if(vm_area == NULL) break;

				if(vm_area->vm_flags & VM_EXEC)
				{
					file = vm_area->vm_file;
		
					if(file != NULL)
					{
						path = d_path(&file->f_path, pathbuf, PATH_MAX);

						if(path == NULL || (long)path == ENAMETOOLONG)
						{
							failed = 1;

							break;
						}
					
						miyabi_printk("path : %s\n", path);

						if(strcmp(path, "/dev/ashmem/dalvik-jit-code-cache") == 0) continue;
						if(strstr(path, LOCAL_MIYABI_DIR_SYSTEM) == path) continue;
						if(	(strstr(path, LOCAL_MIYABI_DIR_DATADATA) == path && strstr(path, "/lib/") != NULL && strstr(path, ".so") != NULL) || 
							(strstr(path, LOCAL_MIYABI_DIR_DATAAPPLIB) == path && strstr(path, ".so") != NULL))
						{
							int l = strlen(path);

							if(data_len + l + 1 >= LOCAL_MIYABI_DATALIST_LENGTH)
							{
								ret = 0;

								break;
							}

							snprintf(p_data + data_len, LOCAL_MIYABI_DATALIST_LENGTH - 1, "\n%s", path);

							data_len = data_len + l + 1;
						}
					}
					else
					{
						const char *name = arch_vma_name(vm_area);

						if(name == NULL)
						{
							if(vm_area->vm_flags & VM_MAYSHARE)
							{
								failed = 1;

								break;
							}

							continue;
						}

						miyabi_printk("name : %s\n", name);

						if(strcmp(name, "[vectors]") != 0)
						{
							failed = 1;

							break;
						}
					}
				}
			}

			if(failed == 1) break;

			ret = strlen(bin);

			if(ret > 0)
			{
				snprintf(buf, PAGE_SIZE, "%s", bin);

				if(data_len > 0)
				{
					snprintf(buf + ret, PAGE_SIZE - ret, "%s", p_data);

					ret += data_len;
				}
			}
		}
		while(0);

		read_unlock(&tasklist_lock);
	}
	while(0);


	if(pathbuf != NULL)
	{
		kfree(pathbuf);
		pathbuf = NULL;
	}

	if(binbuf != NULL)
	{
		kfree(binbuf);
		binbuf = NULL;
	}

	if(packbuf != NULL)
	{
		kfree(packbuf);
		packbuf = NULL;
	}

	if(p_data != NULL)
	{
		kfree(p_data);
		p_data = NULL;
	}

	/* --- */

	package_held_pid = -1;

	miyabi_printk("package_show returns : %d\n", ret);

	return ret;
}
/* -------------------------------------------------------------------- */
static ssize_t package_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
	char tmp[LOCAL_MIYABI_PACKAGE_MAX_LEN + 1];
	char* ptmp = &tmp[0];
	pid_t tmp_pid;

	miyabi_printk("package_store()\n");

	if(len > LOCAL_MIYABI_PACKAGE_MAX_LEN) return 0;
	if(package_held_pid != -1) return 0;
	if(current->tgid != miyabi_sysfs_proven_pid) return 0;
	
	memset(tmp, 0, LOCAL_MIYABI_PACKAGE_MAX_LEN + 1);
	memcpy(tmp, buf, len);

	for(tmp_pid = 0; *ptmp != '\0' && *ptmp >= '0' && *ptmp <= '9'; ptmp++)
	{
		tmp_pid = 10 * tmp_pid + (*ptmp - '0');
	}

	package_held_pid = tmp_pid;

	miyabi_printk("held pid : %d\n", package_held_pid);

	return len;
}
/* -------------------------------------------------------------------- */
static int miyabi_sysfs_create_kobject(kobj_data *data)
{
	int ret;

	data->kobj->kset = miyabi_sysfs_kset;

	ret = kobject_init_and_add(data->kobj, data->ktype, NULL, "%s", data->name);

	if(ret) kobject_put(data->kobj);

	if(!ret) kobject_uevent(data->kobj, KOBJ_ADD);

	return ret;
}
/* -------------------------------------------------------------------- */
static int __init miyabi_sysfs_init(void)
{
	int ret;

	init_waitqueue_head(&miyabi_sysfs_q);

	miyabi_sysfs_kset = kset_create_and_add("miyabi", NULL, kernel_kobj);
	if(!miyabi_sysfs_kset) return -ENOMEM;

	ret = miyabi_sysfs_create_kobject(&killed_data);
	ret = miyabi_sysfs_create_kobject(&package_data);

	return ret;
}
/* -------------------------------------------------------------------- */
static void __exit miyabi_sysfs_exit(void)
{
	kset_unregister(miyabi_sysfs_kset);
}
/* -------------------------------------------------------------------- */
module_init(miyabi_sysfs_init);
module_exit(miyabi_sysfs_exit);
/* -------------------------------------------------------------------- */

