/*
 * This file is part of dirbtree.
 * dirbtree is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * Vintage Game Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with dirbtree. If not, see <http://www.gnu.org/licenses/>.
 * */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "dirbtree.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huseyin Muhlis Olgac");
MODULE_DESCRIPTION("Kernel Project Experiment");

struct dt_dev_t dt_device;
static int dt_open (struct inode *inode, struct file *filp);
static int dt_release (struct inode *inode, struct file *filp);
static ssize_t dt_read (struct file *filp, char __user *buf, size_t count,
						loff_t *pos);
static ssize_t dt_write (struct file *filp, const char __user *buf,
						size_t count, loff_t *pos);
static long dt_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
static int dt_fasync(int fd, struct file *filp, int mode);
static int dt_proc_open (struct inode *inode, struct file *filp);
static const struct file_operations dt_fops = {
	.open = dt_open,
	.release = dt_release,
	.read = dt_read,
	.write = dt_write,
	.unlocked_ioctl = dt_ioctl,
	.fasync = dt_fasync,
};

static const struct file_operations dt_proc_fops = {
	.open = dt_proc_open,
	.release = single_release,
	.read = seq_read,
	.write = dt_write,
	.llseek = seq_lseek,
};

static int alloc_device (void)
{
	dev_t dev_num;
	int error;
	mutex_init(&dt_device.mutex);
	error = alloc_chrdev_region(&dev_num, 0, 1, "dirbtree");
	if (error)
		goto chrdev_error;
	printk(KERN_DEBUG "Device major number: %llu", (u64)MAJOR(dev_num));
	strcpy(dt_device.data, "EMPTY");
	cdev_init(&dt_device.cdev, &dt_fops);
	dt_device.cdev.owner = THIS_MODULE;
	error = cdev_add(&dt_device.cdev, dev_num, 1);
	if (error)
		goto chrdev_error;
	return 0;
chrdev_error:
	return error;
}

static void free_device (void)
{
	cdev_del(&dt_device.cdev);
	unregister_chrdev_region(dt_device.cdev.dev, 1);
}

static int dirbtree_init (void)
{
	int error;
	struct proc_dir_entry* pde;
	printk(KERN_DEBUG "dirbtree_init\n");
	error = alloc_device();
	if (error)
		goto noalloc;
	pde = proc_create("dirbtree", 0777, NULL, &dt_proc_fops);
	error = -EFAULT;
	if (!pde)
		goto nopde;
	return 0;
nopde:
	printk(KERN_DEBUG "proc_create fail\n");
	free_device();
noalloc:
	return error;
}

static void dirbtree_exit (void)
{
	printk(KERN_DEBUG "dirbtree_exit\n");
	remove_proc_entry("dirbtree", NULL);
	free_device();
}

module_init(dirbtree_init);
module_exit(dirbtree_exit);

static int dt_open (struct inode *inode, struct file *filp)
{
	printk(KERN_DEBUG "opened device\n");
	filp->private_data = &dt_device;
	return 0;
}
static int dt_release (struct inode *inode, struct file *filp)
{
	printk(KERN_DEBUG "released device\n");
	return 0;
}

static ssize_t dt_read (struct file *filp, char __user *buf, size_t count,
		loff_t *pos)
{
	size_t len;
	len = strlen(dt_device.data);
	len = count < len ? count : len;
	if (mutex_lock_interruptible(&dt_device.mutex))
		return -ERESTARTSYS;
	if (current->pid == dt_device.last_pid) {
		printk(KERN_DEBUG "this process again? just return eof..\n");
		mutex_unlock(&dt_device.mutex);
		return 0;
	}
	if (len > 0)
		copy_to_user(buf, dt_device.data, len);
	dt_device.last_pid = current->pid;
	mutex_unlock(&dt_device.mutex);
	printk(KERN_DEBUG "reading: %s\n", dt_device.data);
	pos = 0;
	printk(KERN_DEBUG "nonblock: %u\n", (u32)(filp->f_flags & O_NONBLOCK));
	return len;
}
static ssize_t dt_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	if (count > 31)
		count = 31;
	if (mutex_lock_interruptible(&dt_device.mutex))
		return -ERESTARTSYS;
	copy_from_user(dt_device.data, buf, count);
	dt_device.data[count] = 0;
	dt_device.last_pid = 0;
	mutex_unlock(&dt_device.mutex);
	printk(KERN_DEBUG "writing: %s\n", dt_device.data);
	if(dt_device.async_queue) {
		printk(KERN_DEBUG "SIGIO\n");
		kill_fasync(&dt_device.async_queue, SIGIO, POLL_IN);
	}
	return count;
}
static int _dt_proc_show(struct seq_file *m, void *v)
{
	if (mutex_lock_interruptible(&dt_device.mutex))
		return -ERESTARTSYS;
	seq_printf(m, "Device data: %s\n", dt_device.data);
	mutex_unlock(&dt_device.mutex);
	return 0;
}
static int dt_proc_open (struct inode *inode, struct file *filp)
{
	return single_open(filp, _dt_proc_show, NULL);
}
static long dt_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	printk(KERN_DEBUG "ioctl cmd: %X\n", cmd);
	printk(KERN_DEBUG "sample ioctl cmd: %X\n", DT_IOC_PRINT);
	if (_IOC_TYPE(cmd) != DT_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > DT_IOC_MAXNR) return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;
	switch(cmd) {
		case DT_IOC_PRINT:
			printk(KERN_DEBUG "data: %s\n", dt_device.data);
			break;
	}
	return 0;
}

static int dt_fasync(int fd, struct file *filp, int mode)
{
	return fasync_helper(fd, filp, mode, &dt_device.async_queue);
}

