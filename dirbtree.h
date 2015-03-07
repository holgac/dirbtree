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

#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/sched.h>

struct dt_dev_t {
	struct mutex mutex;
	struct cdev cdev;
	char data[32];
	pid_t last_pid;
	struct fasync_struct* async_queue;
};
#define DT_IOC_MAGIC 't'

#define DT_IOC_PRINT _IO(DT_IOC_MAGIC, 0)
#define DT_IOC_MAXNR 1

