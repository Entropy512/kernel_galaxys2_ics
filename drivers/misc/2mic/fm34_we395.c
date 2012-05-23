/* drivers/misc/2mic/fm34_we395.c - fm34_we395 voice processor driver
 *
 * Copyright (C) 2012 Samsung Corporation.
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

#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/i2c/fm34_we395.h>

#include "fm34_we395.h"

struct fm34_data {
	struct fm34_platform_data	*pdata;
	struct device	*dev;
	struct i2c_client	*client;
	enum fm34_we395_mode	curr_mode;
};

static void fm34_reset_parameter(struct fm34_data *fm34)
{
	gpio_set_value(fm34->pdata->gpio_rst, 0);
	usleep_range(10000, 10000);
	gpio_set_value(fm34->pdata->gpio_pwdn, 1);
	gpio_set_value(fm34->pdata->gpio_bp, 1);
	msleep(50);
	gpio_set_value(fm34->pdata->gpio_rst, 1);
	msleep(50);
}

static int fm34_set_mode(struct fm34_data *fm34, enum fm34_we395_mode mode)
{
	int ret = 0;
	unsigned char *i2c_cmd;
	int size = 0;

	dev_dbg(fm34->dev, "%s : mode %d\n", __func__, mode);

	if (fm34->curr_mode == mode) {
		dev_dbg(fm34->dev, "skip %s\n", __func__);
		return ret;
	}

	switch (mode) {
	case fm34_bypass_mode:
		usleep_range(20000, 20000);
		gpio_set_value(fm34->pdata->gpio_pwdn, 0);
		return ret;
	case fm34_handset_mode:
		i2c_cmd = handset_ns_cmd;
		size = sizeof(handset_ns_cmd);
		break;
	case fm34_loud_mode:
		i2c_cmd = loud_ns_cmd;
		size = sizeof(loud_ns_cmd);
		break;
	case fm34_ftm_loopback_mode:
		i2c_cmd = ftm_loopback_cmd;
		size = sizeof(ftm_loopback_cmd);
		break;
	default:
		break;
	}

	fm34_reset_parameter(fm34);

	if (size) {
		ret = i2c_master_send(fm34->client, i2c_cmd, size);
		if (ret < 0) {
			dev_err(fm34->dev, "i2c_master_send failed %d\n", ret);
			return ret;
		}
		fm34->curr_mode = mode;
	}

	return ret;
}

static int fm34_init_parameter(struct fm34_data *fm34)
{
	int ret = 0;
	unsigned char *i2c_cmd;
	int size = 0;

	fm34_reset_parameter(fm34);

	i2c_cmd = bypass_cmd;
	size = sizeof(bypass_cmd);

	ret = i2c_master_send(fm34->client, i2c_cmd, size);
	if (ret < 0) {
		dev_err(fm34->dev, "i2c_master_send failed %d\n", ret);
		return ret;
	}

	fm34->curr_mode = fm34_bypass_mode;

	usleep_range(20000, 20000);
	gpio_set_value(fm34->pdata->gpio_pwdn, 0);

	return ret;
}

static int __devinit fm34_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct fm34_data *fm34;
	struct fm34_platform_data *pdata;
	int ret = 0;

	fm34 = kzalloc(sizeof(*fm34), GFP_KERNEL);
	if (fm34 == NULL) {
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	fm34->client = client;
	i2c_set_clientdata(client, fm34);

	fm34->dev = &client->dev;
	dev_set_drvdata(fm34->dev, fm34);

	fm34->pdata = client->dev.platform_data;
	pdata = fm34->pdata;

	fm34->curr_mode = -1;

	ret = gpio_request(pdata->gpio_rst, "FM34_RESET");
	if (ret < 0) {
		dev_err(fm34->dev, "error requesting reset gpio\n");
		goto err_gpio_reset;
	}
	gpio_direction_output(pdata->gpio_rst, 1);

	ret = gpio_request(pdata->gpio_pwdn, "FM34_PWDN");
	if (ret < 0) {
		dev_err(fm34->dev, "error requesting pwdn gpio\n");
		goto err_gpio_pwdn;
	}
	gpio_direction_output(pdata->gpio_pwdn, 1);

	ret = gpio_request(pdata->gpio_bp, "FM34_BYPASS");
	if (ret < 0) {
		dev_err(fm34->dev, "error requesting bypass gpio\n");
		goto err_gpio_bp;
	}
	gpio_direction_output(pdata->gpio_bp, 1);

	if (fm34->pdata->set_mclk != NULL)
		fm34->pdata->set_mclk(true, false);

	ret = fm34_init_parameter(fm34);
	if (ret < 0)
		dev_err(fm34->dev, "fm34_init_parameter failed %d\n", ret);

	return 0;

err_gpio_bp:
	gpio_free(pdata->gpio_pwdn);
err_gpio_pwdn:
	gpio_free(pdata->gpio_rst);
err_gpio_reset:
	kfree(fm34);
err_kzalloc:
	return ret;
}

static int __devexit fm34_remove(struct i2c_client *client)
{
	struct fm34_data *fm34 = i2c_get_clientdata(client);
	i2c_set_clientdata(client, NULL);
	gpio_free(fm34->pdata->gpio_rst);
	gpio_free(fm34->pdata->gpio_pwdn);
	kfree(fm34);
	return 0;
}

static int fm34_suspend(struct device *dev)
{
	return 0;
}

static int fm34_resume(struct device *dev)
{
	return 0;
}

static const struct i2c_device_id fm34_id[] = {
	{ "fm34_we395", 0 },
	{ }
};

static const struct dev_pm_ops fm34_pm_ops = {
	.suspend = fm34_suspend,
	.resume = fm34_resume,
};

static struct i2c_driver fm34_driver = {
	.probe = fm34_probe,
	.remove = __devexit_p(fm34_remove),
	.id_table = fm34_id,
	.driver = {
		.name = "fm34_we395",
		.owner = THIS_MODULE,
		.pm = &fm34_pm_ops,
	},
};

static int __init fm34_init(void)
{
	pr_info("%s\n", __func__);

	return i2c_add_driver(&fm34_driver);
}

static void __exit fm34_exit(void)
{
	i2c_del_driver(&fm34_driver);
}

module_init(fm34_init);
module_exit(fm34_exit);

MODULE_DESCRIPTION("fm34 voice processor driver");
MODULE_LICENSE("GPL");
