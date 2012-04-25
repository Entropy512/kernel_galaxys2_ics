/*
 *  atmel_mxt1386_debug.c - Atmel maXTouch Touchscreen Controller
 *
 *  Version 0.1
 *
 *  Copyright (C) 2010 samsung electrics
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/synaptics_s7301.h>
#include "synaptics_fw_updater.h"
#define PROCESS_CMD(cmd) process_##cmd(ts)
#define SYNAPTICS_FW "/data/firmware/synaptics_fw"

const char *cmd_list[] = {
	"fw_update",
	"fw_ver_kernel",
	"fw_ver_ic",
	"get_threshold",
	"module_off",
	"module_on",
	"get_chip_vendor",
	"get_chip_name",
	"get_reference_line",
	"get_scan_time_line",
	"get_delta_line",
	"get_x_num",
	"get_y_num",
	NULL
};

static int synaptics_ts_load_fw(struct synaptics_ts_data *ts)
{
	struct file *fp;
	mm_segment_t old_fs;
	u8 *fw_data;
	u16 fw_size, nread;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(SYNAPTICS_FW, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		printk(KERN_ERR "[TSP] failed to open %s.\n", SYNAPTICS_FW);
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	fw_data = kmalloc(fw_size, GFP_KERNEL);
	memset(fw_data, 0, fw_size);

	printk(KERN_DEBUG "[TSP] start, file path %s, size %u Bytes\n",
	       SYNAPTICS_FW, fw_size);

	nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
	printk(KERN_ERR "[TSP] nread %u Bytes\n", nread);
	if (nread != fw_size) {
		printk(KERN_ERR
		       "[TSP] failed to read firmware file, nread %u Bytes\n",
		       nread);
		goto read_err;
	}

	disable_irq(ts->client->irq);
	wake_lock(&ts->wakelock);
	synaptics_fw_updater(ts, fw_data);
	wake_unlock(&ts->wakelock);
	enable_irq(ts->client->irq);

	kfree(fw_data);

	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;

 open_err:
	kfree(fw_data);
	set_fs(old_fs);
	return -ENOENT;

 read_err:
	kfree(fw_data);
	filp_close(fp, current->files);
	set_fs(old_fs);
	return -EIO;
}

static void process_fw_update(struct synaptics_ts_data *ts)
{
	synaptics_ts_load_fw(ts);
}

static ssize_t synaptics_ts_show_cmd_status(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);
		return sprintf(buf, "%d\n", ts->cmd_status);
}

static ssize_t synaptics_ts_show_cmd_result(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);
		return sprintf(buf, "%s\n", ts->cmd_result);
}

static ssize_t synaptics_ts_store_cmd(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t size)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);
	char cmd[16];

	if (sscanf(buf, "%s", cmd) == 1) {
#if 0
		int i = 0;
		for (i = 0; NULL != cmd_list[i]; i++) {
			if (strcmp((char *)cmd_list[i], (char *)cmd)) {
				PROCESS_CMD(cmd_list[i]);
				break;
			}
		}
#else
		disable_irq(ts->client->irq);
		process_fw_update(ts);
		enable_irq(ts->client->irq);
#endif
	}
	return size;
}

static DEVICE_ATTR(cmd_status, S_IRUGO,
	synaptics_ts_show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO,
	synaptics_ts_show_cmd_result, NULL);
static DEVICE_ATTR(cmd, S_IRUGO | S_IWUSR,
	NULL, synaptics_ts_store_cmd);

static struct attribute *synaptics_ts_attributes[] = {
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_cmd.attr,
	NULL,
};

struct attribute_group synaptics_ts_attr_group = {
	.attrs = synaptics_ts_attributes,
};

MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL");

