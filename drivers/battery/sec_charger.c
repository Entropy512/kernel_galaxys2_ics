/*
 *  sec_charger.c
 *  Samsung Mobile Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_charger.h>

static int sec_chg_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct sec_charger_info *charger =
		container_of(psy, struct sec_charger_info, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_HEALTH:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (!sec_hal_chg_get_property(charger->client, psp, val))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sec_chg_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct sec_charger_info *charger =
		container_of(psy, struct sec_charger_info, psy_chg);

	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY)
			charger->is_charging = false;
		else
			charger->is_charging = true;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (!sec_hal_chg_set_property(charger->client, psp, val))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void sec_chg_isr_work(struct work_struct *work)
{
	struct sec_charger_info *charger =
		container_of(work, struct sec_charger_info, isr_work.work);
	struct power_supply *psy_bat =
	    get_power_supply_by_name("sec-battery");
	union power_supply_propval val;
	int ret;

	pr_info("%s: Charger Interrupt\n", __func__);

	if (charger->pdata->full_check_type ==
		SEC_BATTERY_FULLCHARGED_CHGINT) {
		if (!sec_hal_chg_get_property(charger->client,
			POWER_SUPPLY_PROP_STATUS, &val))
			return;

		switch (val.intval) {
		case POWER_SUPPLY_STATUS_DISCHARGING:
			pr_err("%s: Interrupted but Discharging\n",
				__func__);
			break;

		case POWER_SUPPLY_STATUS_NOT_CHARGING:
			pr_err("%s: Interrupted but NOT Charging\n",
				__func__);
			break;

		case POWER_SUPPLY_STATUS_FULL:
			pr_info("%s: Interrupted by Full\n",
				__func__);
			ret = psy_bat->set_property(psy_bat,
				POWER_SUPPLY_PROP_STATUS, &val);
			if (ret) {
				pr_err("%s: Fail to Set Battery(%d)\n",
					__func__, ret);
				return;
			}
			break;

		case POWER_SUPPLY_STATUS_CHARGING:
			pr_err("%s: Interrupted but Charging\n",
				__func__);
			break;

		case POWER_SUPPLY_STATUS_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Status\n", __func__);
			break;
		}
	}

	if (charger->pdata->ovp_uvlo_check_type ==
		SEC_BATTERY_OVP_UVLO_CHGINT) {
		if (!sec_hal_chg_get_property(charger->client,
			POWER_SUPPLY_PROP_HEALTH, &val))
			return;

		switch (val.intval) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_COLD:
			pr_err("%s: Interrupted but Hot/Cold\n",
				__func__);
			break;

		case POWER_SUPPLY_HEALTH_DEAD:
			pr_err("%s: Interrupted but Dead\n",
				__func__);
			break;

		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			pr_info("%s: Interrupted by OVP/UVLO\n",
				__func__);
			ret = psy_bat->set_property(psy_bat,
				POWER_SUPPLY_PROP_HEALTH, &val);
			if (ret) {
				pr_err("%s: Fail to Set Battery(%d)\n",
					__func__, ret);
				return;
			}
			break;

		case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
			pr_err("%s: Interrupted but Unspec\n",
				__func__);
			break;

		case POWER_SUPPLY_HEALTH_GOOD:
			pr_err("%s: Interrupted but Good\n",
				__func__);
			break;

		case POWER_SUPPLY_HEALTH_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Health\n", __func__);
			break;
		}
	}
}


static irqreturn_t sec_chg_irq_thread(int irq, void *irq_data)
{
	struct sec_charger_info *charger = irq_data;

	if ((charger->pdata->full_check_type ==
		SEC_BATTERY_FULLCHARGED_CHGINT) ||
		(charger->pdata->ovp_uvlo_check_type ==
		SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

static int __devinit sec_charger_probe(
						struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter =
		to_i2c_adapter(client->dev.parent);
	struct sec_charger_info *charger;
	int irq = 0;
	int ret = 0;

	pr_debug("%s: SEC Charger Driver Loading\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->client = client;
	charger->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, charger);

	charger->psy_chg.name		= "sec-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_BATTERY;
	charger->psy_chg.get_property	= sec_chg_get_property;
	charger->psy_chg.set_property	= sec_chg_set_property;
	charger->psy_chg.properties	= sec_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(sec_charger_props);

	ret = power_supply_register(&client->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_free;
	}

	if (!charger->pdata->chg_gpio_init()) {
		pr_err("%s: Failed to Initialize GPIO\n", __func__);
		goto err_free;
	}

	if (!sec_hal_chg_init(charger->client)) {
		pr_err("%s: Failed to Initialize Charger\n", __func__);
		goto err_free;
	}

	if (charger->pdata->chg_gpio_irq) {
		irq = gpio_to_irq(charger->pdata->chg_gpio_irq);
		ret = request_threaded_irq(irq, NULL,
				sec_chg_irq_thread,
				charger->pdata->chg_irq_attr,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
			return ret;
		}

		if (charger->pdata->full_check_type ==
			SEC_BATTERY_FULLCHARGED_CHGINT) {
			ret = enable_irq_wake(irq);
			if (ret < 0)
				pr_err("%s: Failed to Enable Wakeup Source(%d)\n",
				__func__, ret);
		}

		INIT_DELAYED_WORK_DEFERRABLE(
			&charger->isr_work, sec_chg_isr_work);
	}

	pr_debug("%s: SEC Charger Driver Loaded\n", __func__);
	return 0;

err_free:
	kfree(charger);

	return ret;
}

static int __devexit sec_charger_remove(
						struct i2c_client *client)
{
	return 0;
}

static int sec_charger_suspend(struct i2c_client *client,
				pm_message_t state)
{
	if (!sec_hal_chg_suspend(client))
		pr_err("%s: Failed to Suspend Charger\n", __func__);

	return 0;
}

static int sec_charger_resume(struct i2c_client *client)
{
	if (!sec_hal_chg_resume(client))
		pr_err("%s: Failed to Resume Charger\n", __func__);

	return 0;
}

static int sec_charger_shutdown(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id sec_charger_id[] = {
	{"sec-charger", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sec_charger_id);

static struct i2c_driver sec_charger_driver = {
	.driver = {
		.name	= "sec-charger",
	},
	.probe	= sec_charger_probe,
	.remove	= __devexit_p(sec_charger_remove),
	.suspend	= sec_charger_suspend,
	.resume		= sec_charger_resume,
	.shutdown	= sec_charger_shutdown,
	.id_table	= sec_charger_id,
};

static int __init sec_charger_init(void)
{
	return i2c_add_driver(&sec_charger_driver);
}

static void __exit sec_charger_exit(void)
{
	i2c_del_driver(&sec_charger_driver);
}

module_init(sec_charger_init);
module_exit(sec_charger_exit);

MODULE_DESCRIPTION("Samsung Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
