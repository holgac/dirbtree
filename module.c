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
#include "dirbtree.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huseyin Muhlis Olgac");
MODULE_DESCRIPTION("Kernel Project Experiment");

struct dt_dev_t dt_device;
int dt_open (struct inode *inode, struct file *filp);
int dt_release (struct inode *inode, struct file *filp);
ssize_t dt_read (struct file *filp, char __user *buf, size_t count,
		loff_t *pos);
ssize_t dt_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos);

struct file_operations dt_fops = {
	.open = dt_open,
	.release = dt_release,
	.read = dt_read,
	.write = dt_write,
};
static int alloc_device (void)
{
	dev_t dev_num;
	int error;
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
	printk(KERN_DEBUG "dirbtree_init\n");
	error = alloc_device();
	return error;
}

static void dirbtree_exit (void)
{
	printk(KERN_DEBUG "dirbtree_exit\n");
	free_device();
}

module_init(dirbtree_init);
module_exit(dirbtree_exit);

int dt_open (struct inode *inode, struct file *filp)
{
	printk(KERN_DEBUG "opened device\n");
	filp->private_data = &dt_device;
	return 0;
}
int dt_release (struct inode *inode, struct file *filp)
{
	printk(KERN_DEBUG "released device\n");
	return 0;
}

ssize_t dt_read (struct file *filp, char __user *buf, size_t count,
		loff_t *pos)
{
	size_t len;
	len = strlen(dt_device.data);
	len = count < len ? count : len;
	if (len > 0)
	{
		copy_to_user(buf, dt_device.data, len);
	}
	printk(KERN_DEBUG "reading: %s\n", dt_device.data);
	pos = 0;
	return len;
}
ssize_t dt_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	if (count > 31)
		count = 31;
	copy_from_user(dt_device.data, buf, count);
	dt_device.data[count] = 0;
	printk(KERN_DEBUG "writing: %s\n", dt_device.data);
	return count;
}

