/* drivers/input/touchscreen/synaptics_s7301.c
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

#include <linux/synaptics_s7301.h>
#include "synaptics_fw_updater.h"
#include "synaptics_sysfs.h"

#define DEBUG_PRINT			1
#define FW_DOWNLOAD_ON_BOOT		1

#define REPORT_MT(x, y, strength, width_major, width_minor) \
do {     \
	input_report_abs(ts->input, ABS_MT_POSITION_X, x);	\
	input_report_abs(ts->input, ABS_MT_POSITION_Y,	y);	\
	input_report_abs(ts->input, ABS_MT_PRESSURE, strength);	\
	input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR, width_major);	\
	input_report_abs(ts->input, ABS_MT_WIDTH_MINOR, width_minor);	\
} while (0)

#if 0
static int synaptics_write_data(struct i2c_client *client,
	u8 addr, u8 *buf, u16 count)
{
	struct i2c_msg msg;
	int ret, i;
	u8 data[256];

	data[0] = addr;

	for (i = 1; i <= count; i++)
		data[i] = *buf++;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count + 1;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	return (ret == 1) ? count : ret;
}
#endif
static int synaptics_read_data(struct i2c_client *client,
	u8 addr, u8 *buf, u16 count)
{
	struct i2c_msg msg[2];
	int ret;

	msg[0].addr = client->addr;
	msg[0].flags = 0x00;
	msg[0].len = 1;
	msg[0].buf = &addr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = count;
	msg[1].buf = buf;

	ret = i2c_transfer(client->adapter, msg, 2);

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	return (ret == 1) ? count : ret;
}

static void forced_release_fingers(struct synaptics_ts_data *ts)
{
	int i;
	printk(KERN_DEBUG "[TSP] %s\n", __func__);
	for (i = 0; i < MAX_TOUCH_NUM; ++i) {
		if (TSP_INACTIVE == ts->info[i].status)
			continue;

		input_mt_slot(ts->input, i);
		input_mt_report_slot_state(ts->input,
			MT_TOOL_FINGER, 0);

		ts->info[i].status = TSP_INACTIVE;
	}
	input_sync(ts->input);
	return ;
}

static void fw_update_work_func(struct work_struct *work)
{
	struct synaptics_ts_data *ts =
	    container_of(work, struct synaptics_ts_data, fw_update_work);

	printk(KERN_DEBUG "[TSP] firmware update start\n");
	disable_irq(ts->client->irq);
	wake_lock(&ts->wakelock);
	synaptics_fw_updater(ts, NULL);
	wake_unlock(&ts->wakelock);
	enable_irq(ts->client->irq);
	printk(KERN_DEBUG "[TSP] firmware update finish\n");
}

static int init_synaptics_tsp(struct synaptics_ts_data *ts)
{
	int ret = 0, i = 0;

	for (i = 0; i < MAX_TOUCH_NUM; ++i)
		ts->info[i].status = TSP_INACTIVE;

	i = 0;
	while (i < 5) {
		ret = i2c_smbus_read_byte_data(ts->client,
				REG_INTERRUPT_STATUS);
		if (ret < 0) {
			pr_err("[TSP] read reg_data failed(%d) ret : %d\n",
				i++, ret);
			msleep(100);
		} else
			return 0;
	}

	return ret;
}

static void read_points(struct synaptics_ts_data *ts)
{
	int ret = 0, i, j;
	int id;
	u8 buf[TS_READ_REGS_LEN];
	u8 point[5] = {0, };

	ret = synaptics_read_data(ts->client,
		REG_INTERRUPT_STATUS, buf, 4);
	if (ret < 0) {
		pr_err("[TSP] ts_irq_event: i2c failed\n");
		return ;
	}

	for (i = 1, id = 0; (i < 4 && id < MAX_TOUCH_NUM); i++) {
		for (j = 0; j < MAX_TOUCH_NUM; j++) {
			if (((buf[i] >> (2 * j)) & 0x01) ||
				ts->info[id].strength > 0) {

				ret = synaptics_read_data(ts->client,
					(REG_POINT_INFO + (id * 5)),
					point, 5);
				if (ret < 0) {
					pr_err("[TSP] read point failed\n");
					return ;
				}

				ts->info[id].posX =
					(point[0] << 4) +
					(point[2] & 0x0F);
				ts->info[id].posY =
					(point[1] << 4) +
					((point[2] & 0xF0) >> 4);

				if (((point[3] & 0xF0) >> 4) >
					(point[3] & 0x0F)) {
					ts->info[id].width_major =
						((point[3] & 0xF0) >> 4);
					ts->info[id].width_minor =
						(point[3] & 0x0F);
				} else {
					ts->info[id].width_minor =
						((point[3] & 0xF0) >> 4);
					ts->info[id].width_major =
						(point[3] & 0x0F);
				}

				ts->info[id].strength = point[4];
				if (ts->info[id].strength) {
					if (TSP_PRESS <= ts->info[id].status)
						ts->info[id].status = TSP_MOVE;
					else
						ts->info[id].status = TSP_PRESS;
				} else if (TSP_PRESS <= ts->info[id].status)
					ts->info[id].status = TSP_RELEASE;
			}
			id++;
		}
	}

	for (i = 0; i < MAX_TOUCH_NUM; ++i) {
		if (TSP_INACTIVE == ts->info[i].status)
			continue;

		input_mt_slot(ts->input, i);
		input_mt_report_slot_state(ts->input,
			MT_TOOL_FINGER,
			!!ts->info[i].strength);

		switch (ts->info[i].status) {
		case TSP_PRESS:
#if DEBUG_PRINT
			printk(KERN_DEBUG "[TSP] ID: %d, x: %d, y: %d, z: %d, status : %d\n",
				i,
				ts->info[i].posX,
				ts->info[i].posY,
				ts->info[i].strength,
				ts->info[i].status);
#else
			printk(KERN_DEBUG "synaptics %d P\n", i);
#endif
		case TSP_MOVE:
			REPORT_MT(
				ts->info[i].posX,
				ts->info[i].posY,
				ts->info[i].strength,
				ts->info[i].width_major,
				ts->info[i].width_minor);
				break;

		case TSP_RELEASE:
#if DEBUG_PRINT
			printk(KERN_DEBUG "[TSP] ID: %d\n", i);
#else
			printk(KERN_DEBUG "synaptics %d R\n", i);
#endif
			ts->info[i].status = TSP_INACTIVE;
			break;
		default:
			break;
		}
	}
	input_sync(ts->input);
}

static irqreturn_t synaptics_ts_irq_handler(int irq, void *_data)
{
	struct synaptics_ts_data *ts = (struct synaptics_ts_data *)_data;
	read_points(ts);
	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_ts_early_suspend(struct early_suspend *h)
{
	struct synaptics_ts_data *ts =
		container_of(h, struct synaptics_ts_data, early_suspend);
	printk(KERN_DEBUG "[TSP] %s\n", __func__);
	disable_irq(ts->client->irq);
	forced_release_fingers(ts);
	if (!wake_lock_active(&ts->wakelock))
		ts->pdata->set_power(0);
}

static void synaptics_ts_late_resume(struct early_suspend *h)
{
	struct synaptics_ts_data *ts =
		container_of(h, struct synaptics_ts_data, early_suspend);

	printk(KERN_DEBUG "[TSP] %s\n", __func__);
	if (!wake_lock_active(&ts->wakelock))
		ts->pdata->set_power(1);
	mdelay(300);
	init_synaptics_tsp(ts);
	enable_irq(ts->client->irq);
}
#endif

static int __init synaptics_ts_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct synaptics_ts_data *ts;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[TSP] failed to check i2c functionality!\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct synaptics_ts_data), GFP_KERNEL);
	if (unlikely(ts == NULL)) {
		pr_err("[TSP] failed to allocate the synaptics_ts_data.\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->pdata = client->dev.platform_data;
	ts->input = input_allocate_device();
	if (!ts->input) {
		pr_err("[TSP] failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

#if 0
	ts->input->name = client->name;
#else
	ts->input->name = "sec_touchscreen";
#endif
	__set_bit(EV_ABS, ts->input->evbit);
	__set_bit(EV_KEY, ts->input->evbit);
	__set_bit(MT_TOOL_FINGER, ts->input->keybit);
	__set_bit(INPUT_PROP_DIRECT, ts->input->propbit);

	input_mt_init_slots(ts->input, MAX_TOUCH_NUM - 1);
	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0,
			     ts->pdata->max_x, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0,
			     ts->pdata->max_y, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_PRESSURE, 0,
			     ts->pdata->max_pressure, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_WIDTH_MAJOR, 0,
			     ts->pdata->max_width, 0, 0);

	ret = input_register_device(ts->input);
	if (ret) {
		pr_err("[TSP] failed to register input device\n");
		ret = -ENOMEM;
		goto err_input_register_device_failed;
	}

	mutex_init(&ts->mutex);
	wake_lock_init(&ts->wakelock, WAKE_LOCK_SUSPEND, "touch");
	INIT_WORK(&ts->fw_update_work, fw_update_work_func);
	ret = init_synaptics_tsp(ts);
	if (ret)
		goto err_input_register_device_failed;

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = synaptics_ts_early_suspend;
	ts->early_suspend.resume = synaptics_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#if FW_DOWNLOAD_ON_BOOT
	schedule_work(&ts->fw_update_work);
#endif

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
				 synaptics_ts_irq_handler,
				 IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				 client->name, ts);
		if (ret > 0) {
			pr_err("[TSP] failed to request threaded irq %d, ret %d\n",
				client->irq, ret);
			goto err_request_irq;
		}
	}

	ts->dev = device_create(sec_class, NULL, 0, ts, "sec_touchscreen");
	if (IS_ERR(ts->dev)) {
		pr_err("[TSP] failedailed to create device for the sysfs\n");
		ret = -ENODEV;
		goto err_sysfs_create_group;
	}

	ret = sysfs_create_group(&ts->dev->kobj, &synaptics_ts_attr_group);
	if (ret) {
		pr_err("[TSP] failed to create sysfs group\n");
		goto err_sysfs_create_group;
	}

	return 0;
err_sysfs_create_group:
err_request_irq:
	free_irq(client->irq, ts);
err_input_register_device_failed:
	input_unregister_device(ts->input);
err_input_dev_alloc_failed:
	input_free_device(ts->input);
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int synaptics_ts_remove(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	input_unregister_device(ts->input);
	kfree(ts);
	return 0;
}

static const struct i2c_device_id synaptics_ts_id[] = {
	{SYNAPTICS_TS_NAME, 0},
	{}
};

static struct i2c_driver synaptics_ts_driver = {
	.driver = {
		   .name = SYNAPTICS_TS_NAME,
	},
	.id_table = synaptics_ts_id,
	.probe = synaptics_ts_probe,
	.remove = __devexit_p(synaptics_ts_remove),
};

static int __devinit synaptics_ts_init(void)
{
	return i2c_add_driver(&synaptics_ts_driver);
}

static void __exit synaptics_ts_exit(void)
{
	i2c_del_driver(&synaptics_ts_driver);
}

MODULE_DESCRIPTION("Driver for Synaptics S7301 Touchscreen Controller");
MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL");

module_init(synaptics_ts_init);
module_exit(synaptics_ts_exit);

