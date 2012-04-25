/*
 * include/linux/synaptics_s7301.h
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
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

#ifndef _LINUX_SYNAPTICS_TS_H_
#define _LINUX_SYNAPTICS_TS_H_

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>

#define SYNAPTICS_TS_NAME "synaptics_ts"

#define TS_MAX_X_COORD			1279
#define TS_MAX_Y_COORD			799
#define TS_MAX_Z_TOUCH			255
#define TS_MAX_W_TOUCH			100

#define REG_INTERRUPT_STATUS		0x14
#define REG_FINGER_STATUS		0X15
#define REG_RESET			0x85
#define REG_POINT_INFO			0X18

#define TS_READ_REGS_LEN		66
#define MAX_TOUCH_NUM			10
#define I2C_RETRY_CNT			5

enum {
	TSP_INACTIVE = -1,
	TSP_RELEASE,
	TSP_PRESS,
	TSP_MOVE,
};

struct touch_info {
	int strength;
	int width_major;
	int width_minor;
	int posX;
	int posY;
	int status;
};

/* Variables for F34 functionality */
struct synaptics_ts_fw_block {
	u8 *fw_data;
	u8 *fw_imgdata;
	u8 *config_imgdata;
	u8 *lock_imgdata;
	u16 f01_database;
	u16 f01_commandbase;
	u16 f34_database;
	u16 f34_querybase;
	u16 f34_reflash_blocknum;
	u16 f34_reflash_blockdata;
	u16 f34_reflashquery_boot_id;
	u16 f34_reflashquery_flashpropertyquery;
	u16 f34_reflashquery_fw_blocksize;
	u16 f34_reflashquery_fw_blockcount;
	u16 f34_reflashquery_config_blocksize;
	u16 f34_reflashquery_config_blockcount;
	u16 f34_flashcontrol;
	u16 fw_blocksize;
	u16 fw_blockcount;
	u16 config_blocksize;
	u16 config_blockcount;
	u16 fw_version;
	u16 boot_id;
	u32 imagesize;
	u32 config_imagesize;
};

struct synaptics_platform_data {
	int max_x;
	int max_y;
	int max_pressure;
	int max_width;
	void (*set_power)(bool);
	int (*mux_i2c_set)(int);
};

struct synaptics_ts_data {
	struct i2c_client	*client;
	struct device *dev;
	struct input_dev	*input;
	struct synaptics_platform_data *pdata;
	struct touch_info info[MAX_TOUCH_NUM];
	struct mutex	mutex;
	struct synaptics_ts_fw_block *fw;
	struct wake_lock wakelock;
	struct work_struct fw_update_work;
#if CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
	u8 cmd_status;
	char cmd_result[64];
};

#endif	/* _LINUX_SYNAPTICS_TS_H_ */
