/*
 *  bq24190_charger.c
 *  Samsung BQ24190 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_charger.h>
static int bq24190_i2c_write(struct i2c_client *client, int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		pr_err("%s: Error(%d)\n", __func__, ret);
	return ret;
}

static int bq24190_i2c_read(struct i2c_client *client, int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		pr_err("%s: Error(%d)\n", __func__, ret);
	return ret;
}

static void bq24190_i2c_write_array(struct i2c_client *client, u8 *buf,
				    int size)
{
	int i;
	for (i = 0; i < size; i += 3)
		bq24190_i2c_write(client, (u8) (*(buf + i)), (buf + i) + 1);
}

static void bq24190_test_read(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;
	for (addr = 0; addr <= 0x0A; addr++) {
		bq24190_i2c_read(client, addr, &data);
		pr_debug("bq24190 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}

static void bq24190_enable_charging(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data = 0;
	gpio_set_value(charger->pdata->chg_gpio_en,
		charger->pdata->chg_polarity_en ? 1 : 0);
	data = 0x1d;
	bq24190_i2c_write(client, BQ24190_POWERON_CONFIG, &data);
}

static void bq24190_disable_charging(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data = 0;
	gpio_set_value(charger->pdata->chg_gpio_en,
		charger->pdata->chg_polarity_en ? 0 : 1);
	data = 0x0d;
	bq24190_i2c_write(client, BQ24190_POWERON_CONFIG, &data);
}

static void bq24190_charger_init(struct i2c_client *client)
{
	u8 data = 0;

	/* Set GPIO_TA_EN as HIGH, charging disable */
	bq24190_disable_charging(client);
	mdelay(100);

	/* Actual input current limit to 2A */
	data = 0x36;
	bq24190_i2c_write(client, BQ24190_INPUT_SOURCE_CTRL, &data);

	/* Disable auto termination, Disable timer */
	data = 0x00;
	bq24190_i2c_write(client, BQ24190_TERMINATION_TIMER_CTRL, &data);
}

static int bq24190_get_charging_status(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 data_status = 0;
	u8 data_fault = 0;
	bq24190_i2c_read(client, BQ24190_SYSTEM_STATUS, &data_status);
	pr_info("%s : Status (0x%02x)\n", __func__, data_status);
	bq24190_i2c_read(client, BQ24190_FAULT, &data_fault);
	pr_info("%s : Fault (0x%02x)\n", __func__, data_fault);
	pr_info("%s : EN(%d), nCHG(%d), nCon(%d)\n", __func__,
		gpio_get_value(charger->pdata->chg_gpio_en),
		gpio_get_value(charger->pdata->chg_gpio_status),
		gpio_get_value(charger->pdata->bat_gpio_ta_nconnected));

	/* At least one charge cycle terminated,
	 *Charge current < Termination Current
	 */
	if ((data_status & BQ24190_CHARGING_DONE) == 0x30)
		status = POWER_SUPPLY_STATUS_FULL;
	goto charging_status_end;
	if (data_status & BQ24190_CHARGING_ENABLE)
		status = POWER_SUPPLY_STATUS_CHARGING;
	goto charging_status_end;
	if (data_fault)

		/* if error bit check, ignore the status of charger-ic */
		status = POWER_SUPPLY_STATUS_DISCHARGING;
charging_status_end:return (int)status;
}

static int bq24190_get_charging_health(struct i2c_client *client)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 data_fault = 0;
	bq24190_i2c_read(client, BQ24190_FAULT, &data_fault);
	pr_info("%s : Fault (0x%02x)\n", __func__, data_fault);
	if ((data_fault & BQ24190_CHARGING_DONE) == 0x20)
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	goto charging_health_end;
	if (data_fault)

		/* if error bit check, ignore the status of charger-ic */
		health = POWER_SUPPLY_HEALTH_GOOD;
charging_health_end:return (int)health;
}

static int bq24190_get_charging_current(struct i2c_client *client)
{
	u8 data = 0;
	int get_current = 500;
	bq24190_i2c_read(client, BQ24190_CHARGE_CURRENT_CTRL, &data);
	while (data) {
		if (data & 0x80) {
			data = data & 0x7f;
			get_current += 2048;
			break;
		}
		if (data & 0x40) {
			data = data & 0xbf;
			get_current += 1024;
			break;
		}
		if (data & 0x20) {
			data = data & 0xdf;
			get_current += 512;
			break;
		}
		if (data & 0x10) {
			data = data & 0xef;
			get_current += 256;
			break;
		}
		if (data & 0x08) {
			data = data & 0xf7;
			get_current += 128;
			break;
		}
		if (data & 0x04) {
			data = data & 0xfb;
			get_current += 64;
			break;
		}
	}
	pr_debug("%s: Get charging current as %dmA.\n", __func__, get_current);
	return get_current;
}

static void bq24190_set_charging_current(
				struct i2c_client *client, int set_current)
{
	u8 data = 0;
	if (set_current > 450) {

		/* High-current mode */
		data = 0x48;
		bq24190_i2c_write(client, BQ24190_CHARGE_CURRENT_CTRL, &data);
		udelay(10);
	} else {

		/* USB5 */
		data = 0x00;
		bq24190_i2c_write(client, BQ24190_CHARGE_CURRENT_CTRL, &data);
		udelay(10);
	}
	pr_debug("%s: Set charging current as %dmA.\n", __func__, set_current);
}

static void bq24190_charger_function_conrol(struct i2c_client *client,
					     int charger_type)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data = 0;
	if (charger_type == POWER_SUPPLY_TYPE_BATTERY) {
		bq24190_disable_charging(client);
		charger->is_charging = false;
	} else {
		/* Init bq24190 charger */
		bq24190_charger_init(client);
		switch (charger_type) {
		case POWER_SUPPLY_TYPE_MAINS:
			/* High-current mode */
			data = 0x58;
			bq24190_i2c_write(client, BQ24190_CHARGE_CURRENT_CTRL,
					  &data);
			charger->is_charging = true;
			pr_info("%s : Charging current set as HC.", __func__);
			break;
		case POWER_SUPPLY_TYPE_USB:
			/* USB5 */
			data = 0x00;
			bq24190_i2c_write(client, BQ24190_CHARGE_CURRENT_CTRL,
					  &data);
			charger->is_charging = true;
			pr_info("%s : Charging current set as LOW(USB5).",
				__func__);
			break;
		default:
			break;
		}
		bq24190_enable_charging(client);
	}
}

bool sec_hal_chg_init(struct i2c_client *client)
{
	bq24190_test_read(client);
	return true;
}

bool sec_hal_chg_suspend(struct i2c_client *client)
{
	return true;
}

bool sec_hal_chg_resume(struct i2c_client *client)
{
	return true;
}

bool sec_hal_chg_get_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq24190_get_charging_status(client);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq24190_get_charging_health(client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current)
			val->intval = bq24190_get_charging_current(client);

		else
			val->intval = 0;
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_chg_set_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		bq24190_charger_function_conrol(client, val->intval);
		bq24190_test_read(client);
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		bq24190_set_charging_current(client, val->intval);
		bq24190_test_read(client);
		break;
	default:
		return false;
	}
	return true;
}
