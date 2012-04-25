/*
 *  sec_battery.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_battery.h>

char *sec_bat_charging_mode_str[] = {
	"None",
	"Normal",
	"Additional",
	"Re-Charging"
};

char *sec_bat_status_str[] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not-charging",
	"Full"
};

char *sec_bat_health_str[] = {
	"Unknown",
	"Good",
	"Overheat",
	"Dead",
	"OverVoltage",
	"UnspecFailure",
	"Cold",
	"UnderVoltage"
};

static int sec_bat_set_charge(
				struct sec_battery_info *battery,
				bool enable)
{
	union power_supply_propval val;
	int ret;

	if ((battery->cable_type != POWER_SUPPLY_TYPE_BATTERY) &&
		(battery->health != POWER_SUPPLY_HEALTH_GOOD)) {
		pr_info("%s: Battery is NOT good!\n", __func__);
		return -EPERM;
	}

	if (enable) {
		val.intval = battery->cable_type;
		/*Reset charging start time only in initial charging start */
		if (battery->charging_start_time == 0) {
			battery->charging_start_time = jiffies;
			battery->charging_next_time =
				battery->pdata->charging_reset_time;
		}
	} else {
		val.intval = POWER_SUPPLY_TYPE_BATTERY;
		battery->charging_start_time = 0;
		battery->charging_passed_time = 0;
		battery->charging_next_time = 0;
		battery->full_check_cnt = 0;
	}

	battery->temp_high_cnt = 0;
	battery->temp_low_cnt = 0;
	battery->temp_recover_cnt = 0;

	psy_do_property("sec-charger", set,
		POWER_SUPPLY_PROP_ONLINE, val);

	psy_do_property("sec-fuelgauge", set,
		POWER_SUPPLY_PROP_ONLINE, val);

	return 0;
}

static int sec_bat_get_adc_data(struct sec_battery_info *battery,
			int adc_ch, int count)
{
	int adc_data;
	int adc_max;
	int adc_min;
	int adc_total;
	int i;

	adc_data = 0;
	adc_max = 0;
	adc_min = 0;
	adc_total = 0;

	for (i = 0; i < count; i++) {
		mutex_lock(&battery->adclock);
		adc_data = adc_read(battery->pdata, adc_ch);
		mutex_unlock(&battery->adclock);

		if (adc_data < 0)
			goto err;

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (count - 2);
err:
	return adc_data;
}

static unsigned long calculate_average_adc(
			struct sec_battery_info *battery,
			int channel, int adc)
{
	unsigned int cnt = 0;
	int total_adc = 0;
	int average_adc = 0;
	int index = 0;

	cnt = battery->adc_sample[channel].cnt;
	total_adc = battery->adc_sample[channel].total_adc;

	if (adc <= 0) {
		pr_err("%s : Invalid ADC : %d\n", __func__, adc);
		adc = battery->adc_sample[channel].average_adc;
	}

	if (cnt < ADC_SAMPLE_COUNT) {
		battery->adc_sample[channel].adc_arr[cnt] = adc;
		battery->adc_sample[channel].index = cnt;
		battery->adc_sample[channel].cnt = ++cnt;

		total_adc += adc;
		average_adc = total_adc / cnt;
	} else {
		index = battery->adc_sample[channel].index;
		if (++index >= ADC_SAMPLE_COUNT)
			index = 0;

		total_adc = total_adc -
			battery->adc_sample[channel].adc_arr[index] + adc;
		average_adc = total_adc / ADC_SAMPLE_COUNT;

		battery->adc_sample[channel].adc_arr[index] = adc;
		battery->adc_sample[channel].index = index;
	}

	battery->adc_sample[channel].total_adc = total_adc;
	battery->adc_sample[channel].average_adc = average_adc;

	return average_adc;
}

static int sec_bat_get_adc_value(
		struct sec_battery_info *battery, int channel)
{
	int adc;

	adc = sec_bat_get_adc_data(battery, channel,
		battery->pdata->adc_check_count);

	if (adc <= 0) {
		pr_err("%s: Error in ADC\n", __func__);
		return adc;
	}

	return calculate_average_adc(battery, channel, adc);
}

static int sec_bat_get_charger_type_adc
				(struct sec_battery_info *battery)
{
	/* N.B. If check_ta_conn() is true something valid is
	connceted to the device for charging.
	By default this something is considered to be USB.*/
	int result = POWER_SUPPLY_TYPE_BATTERY;

	int vol = 0;
	int adc;
	int i;

	battery->pdata->cable_switch_check();

	for (i = 0; i < ADC_SAMPLE_COUNT; i++)
		adc = sec_bat_get_adc_value(battery,
			battery->pdata->cable_check_adc_channel);

	battery->pdata->cable_switch_normal();

	vol = (adc * battery->pdata->cable_check_max_voltage) /
		battery->pdata->adc_max_value;

	for (i = 0; i < SEC_SIZEOF_POWER_SUPPLY_TYPE; i++)
		if ((vol > battery->pdata->cable_adc_value[i].min) &&
			(vol < battery->pdata->cable_adc_value[i].max))
			break;
	if (i >= SEC_SIZEOF_POWER_SUPPLY_TYPE)
		pr_err("%s : Invalid ADC value, default BATTERY\n", __func__);
	else
		result = i;

	pr_debug("%s : result(%d), vol(%d)\n", __func__, result, vol);

	return result;
}

static bool sec_bat_check_vf_adc(struct sec_battery_info *battery)
{
	int adc;

	adc = sec_bat_get_adc_data(battery,
		battery->pdata->check_adc_channel,
		battery->pdata->adc_check_count);

	if (adc <= 0) {
		pr_err("%s: VF ADC error\n", __func__);
		adc = battery->check_adc_value;
	} else
		battery->check_adc_value = adc;

	if ((battery->check_adc_value < battery->pdata->check_adc_max) &&
		(battery->check_adc_value > battery->pdata->check_adc_min))
		return true;
	else
		return false;
}

static bool sec_bat_check_by_psy(struct sec_battery_info *battery)
{
	char *psy_name;
	union power_supply_propval value;
	bool ret;
	ret = true;

	switch (battery->pdata->battery_check_type) {
	case SEC_BATTERY_CHECK_PMIC:
		psy_name = battery->pdata->pmic_name;
		break;
	case SEC_BATTERY_CHECK_FUELGAUGE:
		psy_name = "sec-fuelgauge";
		break;
	case SEC_BATTERY_CHECK_CHARGER:
		psy_name = "sec-charger";
		break;
	default:
		pr_err("%s: Invalid Battery Check Type\n",
			__func__);
		ret = false;
		goto battery_check_error;
		break;
	}

	psy_do_property(psy_name, get,
		POWER_SUPPLY_PROP_PRESENT, value);
	ret = (bool)value.intval;

battery_check_error:
	return ret;
}

static bool sec_bat_check(struct sec_battery_info *battery)
{
	bool ret;
	ret = true;

	switch (battery->pdata->battery_check_type) {
	case SEC_BATTERY_CHECK_ADC:
		ret = sec_bat_check_vf_adc(battery);
		break;
	case SEC_BATTERY_CHECK_CALLBACK:
		ret = battery->pdata->check_callback();
		break;
	case SEC_BATTERY_CHECK_PMIC:
	case SEC_BATTERY_CHECK_FUELGAUGE:
	case SEC_BATTERY_CHECK_CHARGER:
		ret = sec_bat_check_by_psy(battery);
		break;
	case SEC_BATTERY_CHECK_NONE:
		pr_debug("%s: No Check\n", __func__);
	default:
		break;
	}

	return ret;
}

static bool sec_bat_battery_cable_check(struct sec_battery_info *battery)
{
	bool cable_changed = false;

	if (!sec_bat_check(battery)) {
		if (battery->check_count < battery->pdata->check_count)
			battery->check_count++;
		else {
			pr_err("%s: Battery Disconnected\n", __func__);
			battery->present = false;
			battery->health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			battery->pdata->check_result_callback();
			return false;
		}
	} else
		battery->check_count = 0;

	battery->present = true;

	pr_debug("%s: Battery Connected\n", __func__);

	if (battery->pdata->cable_check_type &
		SEC_BATTERY_CABLE_CHECK_POLLING) {
		/* check cable status */
		switch (battery->pdata->cable_source_type) {
		case SEC_BATTERY_CABLE_SOURCE_CALLBACK:
			if (battery->cable_type !=
				battery->pdata->check_cable_status())
				cable_changed = true;
			break;
		case SEC_BATTERY_CABLE_SOURCE_ADC:
			if (battery->cable_type !=
				sec_bat_get_charger_type_adc(battery))
				cable_changed = true;
			break;
		case SEC_BATTERY_CABLE_SOURCE_EXTERNAL:
			pr_debug("%s: Cable Type Defined\n", __func__);
		default:
			break;
		}

		if (cable_changed) {
			pr_info("%s: Cable was Changed\n", __func__);
			wake_lock(&battery->cable_wake_lock);
			queue_work(battery->monitor_wqueue,
				&battery->cable_work);
		}
	}
	return true;
};

static bool sec_bat_ovp_uvlo_by_psy(struct sec_battery_info *battery)
{
	char *psy_name;
	union power_supply_propval value;
	bool ret;
	ret = true;

	switch (battery->pdata->ovp_uvlo_check_type) {
	case SEC_BATTERY_OVP_UVLO_PMICPOLLING:
		psy_name = battery->pdata->pmic_name;
		break;
	case SEC_BATTERY_OVP_UVLO_CHGPOLLING:
		psy_name = "sec-charger";
		break;
	default:
		pr_err("%s: Invalid OVP/UVLO Check Type\n",
			__func__);
		ret = false;
		goto ovp_uvlo_check_error;
		break;
	}

	psy_do_property(psy_name, get,
		POWER_SUPPLY_PROP_HEALTH, value);

	if (value.intval != POWER_SUPPLY_HEALTH_GOOD) {
		battery->health = value.intval;
		ret = false;
	}

ovp_uvlo_check_error:
	return ret;
}

static bool sec_bat_ovp_uvlo(struct sec_battery_info *battery)
{
	bool ret;
	ret = true;

	switch (battery->pdata->ovp_uvlo_check_type) {
	case SEC_BATTERY_OVP_UVLO_CALLBACK:
		ret = battery->pdata->ovp_uvlo_callback();
		break;
	case SEC_BATTERY_OVP_UVLO_PMICPOLLING:
	case SEC_BATTERY_OVP_UVLO_CHGPOLLING:
		ret = sec_bat_ovp_uvlo_by_psy(battery);
		break;
	case SEC_BATTERY_OVP_UVLO_PMICINT:
	case SEC_BATTERY_OVP_UVLO_CHGINT:
		/* nothing for interrupt check */
	default:
		break;
	}

	return ret;
}

static bool sec_bat_check_recharge(struct sec_battery_info *battery)
{
	if ((battery->status == POWER_SUPPLY_STATUS_FULL) &&
		(battery->charging_mode == SEC_BATTERY_CHARGING_NONE)) {
		if ((battery->pdata->recharge_condition_type &
			SEC_BATTERY_RECHARGE_CONDITION_SOC) &&
			(battery->capacity >
			battery->pdata->recharge_condition_soc)) {
			pr_info("%s: Re-charging by SOC\n", __func__);
			return true;
		}

		if ((battery->pdata->recharge_condition_type &
			SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL) &&
			(battery->voltage_avg >
			battery->pdata->recharge_condition_avgvcell)) {
			pr_info("%s: Re-charging by average VCELL\n", __func__);
			return true;
		}

		if ((battery->pdata->recharge_condition_type &
			SEC_BATTERY_RECHARGE_CONDITION_VCELL) &&
			(battery->voltage_now >
			battery->pdata->recharge_condition_vcell)) {
			pr_info("%s: Re-charging by VCELL\n", __func__);
			return true;
		}
	}

	return false;
}

static bool sec_bat_ovp_uvlo_result(struct sec_battery_info *battery)
{
	bool ret;

	ret = true;

	pr_err("%s: Abnormal Charging Voltage\n", __func__);
	battery->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	ret = battery->pdata->ovp_uvlo_result_callback(battery->health);

	return ret;
}

static bool sec_bat_voltage_check(struct sec_battery_info *battery)
{
	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		pr_debug("%s: Charging Disabled\n", __func__);
		return true;
	}

	/* OVP/UVLO check */
	if (sec_bat_ovp_uvlo(battery))
		pr_debug("%s: Normal Charging Voltage\n", __func__);
	else
		sec_bat_ovp_uvlo_result(battery);

	/* Re-Charging check */
	if (sec_bat_check_recharge(battery)) {
		battery->charging_mode = SEC_BATTERY_CHARGING_RECHARGING;
		sec_bat_set_charge(battery, true);
	}

	return true;
};

static bool sec_bat_get_temperature_by_adc(
				struct sec_battery_info *battery,
				enum power_supply_property psp,
				union power_supply_propval *value)
{
	int temp = 0;
	int temp_adc;
	int low = 0;
	int high = 0;
	int mid = 0;
	int channel;
	sec_bat_adc_table_data_t *temp_adc_table;
	unsigned int temp_adc_table_size;

	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
		channel = battery->pdata->temp_adc_channel;
		temp_adc_table = battery->pdata->temp_adc_table;
		temp_adc_table_size =
			battery->pdata->temp_adc_table_size;
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		channel = battery->pdata->temp_amb_adc_channel;
		temp_adc_table = battery->pdata->temp_amb_adc_table;
		temp_adc_table_size =
			battery->pdata->temp_amb_adc_table_size;
		break;
	default:
		pr_err("%s: Invalid Property\n", __func__);
		return false;
	}

	temp_adc = sec_bat_get_adc_value(battery, channel);
	if (temp_adc < 0)
		return true;
	battery->temp_adc = temp_adc;

	high = temp_adc_table_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (temp_adc_table[mid].adc > temp_adc)
			high = mid - 1;
		else if (temp_adc_table[mid].adc < temp_adc)
			low = mid + 1;
		else
			break;
	}
	temp = temp_adc_table[mid].temperature;

	value->intval = temp;

	pr_debug("%s: Temp(%d), Temp-ADC(%d)\n",
		__func__, temp, temp_adc);

	return true;
}

static bool sec_bat_temperature(
				struct sec_battery_info *battery)
{
	bool ret;
	ret = true;

	if (battery->pdata->event_check && battery->event) {
		battery->temp_high_threshold =
			battery->pdata->temp_high_threshold_event;
		battery->temp_high_recovery =
			battery->pdata->temp_high_recovery_event;
		battery->temp_low_recovery =
			battery->pdata->temp_low_recovery_event;
		battery->temp_low_threshold =
			battery->pdata->temp_low_threshold_event;
	} else if (battery->pdata->is_lpm()) {
		battery->temp_high_threshold =
			battery->pdata->temp_high_threshold_lpm;
		battery->temp_high_recovery =
			battery->pdata->temp_high_recovery_lpm;
		battery->temp_low_recovery =
			battery->pdata->temp_low_recovery_lpm;
		battery->temp_low_threshold =
			battery->pdata->temp_low_threshold_lpm;
	} else {
		battery->temp_high_threshold =
			battery->pdata->temp_high_threshold_normal;
		battery->temp_high_recovery =
			battery->pdata->temp_high_recovery_normal;
		battery->temp_low_recovery =
			battery->pdata->temp_low_recovery_normal;
		battery->temp_low_threshold =
			battery->pdata->temp_low_threshold_normal;
	}

	pr_info("%s: HT(%d), HR(%d), LT(%d), LR(%d)\n",
		__func__, battery->temp_high_threshold,
		battery->temp_high_recovery,
		battery->temp_low_threshold,
		battery->temp_low_recovery);
	return ret;
}

static bool sec_bat_temperature_check(
				struct sec_battery_info *battery)
{
	int temp_value;

	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		pr_debug("%s: Charging Disabled\n", __func__);
		return true;
	}

	sec_bat_temperature(battery);

	switch (battery->pdata->temp_check_type) {
	case SEC_BATTERY_TEMP_CHECK_ADC:
		temp_value = battery->temp_adc;
		break;
	case SEC_BATTERY_TEMP_CHECK_TEMP:
		temp_value = battery->temperature;
		break;
	default:
		pr_err("%s: Invalid Temp Check Type\n", __func__);
		return true;
	}

	if (temp_value >= battery->temp_high_threshold) {
		if (battery->health != POWER_SUPPLY_HEALTH_OVERHEAT) {
			if (battery->temp_high_cnt <
				battery->pdata->temp_check_count)
				battery->temp_high_cnt++;
			pr_debug("%s: high count = %d\n",
				__func__, battery->temp_high_cnt);
		}
	} else if ((temp_value <= battery->temp_high_recovery) &&
				(temp_value >= battery->temp_low_recovery)) {
		if (battery->health == POWER_SUPPLY_HEALTH_OVERHEAT ||
		    battery->health == POWER_SUPPLY_HEALTH_COLD) {
			if (battery->temp_recover_cnt <
				battery->pdata->temp_check_count)
				battery->temp_recover_cnt++;
			pr_debug("%s: recovery count = %d\n",
				__func__, battery->temp_recover_cnt);
		}
	} else if (temp_value <= battery->temp_low_threshold) {
		if (battery->health != POWER_SUPPLY_HEALTH_COLD) {
			if (battery->temp_low_cnt <
				battery->pdata->temp_check_count)
				battery->temp_low_cnt++;
			pr_debug("%s: low count = %d\n",
				__func__, battery->temp_low_cnt);
		}
	} else {
		battery->temp_high_cnt = 0;
		battery->temp_low_cnt = 0;
		battery->temp_recover_cnt = 0;
	}

	if (battery->temp_high_cnt >=
		battery->pdata->temp_check_count)
		battery->health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (battery->temp_low_cnt >=
		battery->pdata->temp_check_count)
		battery->health = POWER_SUPPLY_HEALTH_COLD;
	else if (battery->temp_recover_cnt >=
		battery->pdata->temp_check_count)
		battery->health = POWER_SUPPLY_HEALTH_GOOD;

	if ((battery->health == POWER_SUPPLY_HEALTH_OVERHEAT) ||
		(battery->health == POWER_SUPPLY_HEALTH_COLD)) {
		pr_info("%s: Insafe Temperature\n", __func__);
		battery->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		/* change charging current to battery (default 0mA) */
		sec_bat_set_charge(battery, false);
		return false;
	} else {
		pr_debug("%s: Safe Temperature\n", __func__);
		/* if recovered from not charging */
		if ((battery->health == POWER_SUPPLY_HEALTH_GOOD) &&
			(battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
			if (battery->charging_mode ==
				SEC_BATTERY_CHARGING_RECHARGING)
				battery->status = POWER_SUPPLY_STATUS_FULL;
			else	/* Normal Charging */
				battery->status = POWER_SUPPLY_STATUS_CHARGING;
			/* turn on charger by cable type */
			sec_bat_set_charge(battery, true);
		}
		return true;
	}
};

static void sec_bat_event_expired_timer_func(unsigned long param)
{
	struct sec_battery_info *battery =
		(struct sec_battery_info *)param;

	battery->event &= (~battery->event_wait);
	pr_info("%s: event expired (0x%x)\n", __func__, battery->event);
}

static void sec_bat_event_set(
	struct sec_battery_info *battery, int event, int enable)
{
	if (!battery->pdata->event_check)
		return;

	/* ignore duplicated deactivation of same event
	 * only if the event is one last event
	 */
	if (!enable && (battery->event == battery->event_wait)) {
		pr_info("%s: ignore duplicated deactivation of same event\n",
			__func__);
		return;
	}

	del_timer_sync(&battery->event_expired_timer);
	battery->event &= (~battery->event_wait);

	if (enable) {
		battery->event_wait = 0;
		battery->event |= event;

		pr_info("%s: event set (0x%x)\n", __func__, battery->event);
	} else {
		if (battery->event == 0) {
			pr_debug("%s: nothing to clear\n", __func__);
			return;	/* nothing to clear */
		}
		battery->event_wait = event;
		mod_timer(&battery->event_expired_timer,
			jiffies + (battery->pdata->event_waiting_time * HZ));
		pr_info("%s: start timer (curr 0x%x, wait 0x%x)\n",
			__func__, battery->event, battery->event_wait);
	}
}

static bool sec_bat_time_management(
				struct sec_battery_info *battery)
{
	if (battery->charging_start_time == 0) {
		pr_debug("%s: Charging Disabled\n", __func__);
		return true;
	}

	if (jiffies >= battery->charging_start_time)
		battery->charging_passed_time =
			jiffies - battery->charging_start_time;
	else
		battery->charging_passed_time =
			0xFFFFFFFF - battery->charging_start_time + jiffies;

	switch (battery->status) {
	case POWER_SUPPLY_STATUS_FULL:
		if (battery->charging_mode == SEC_BATTERY_CHARGING_RECHARGING &&
			time_after(battery->charging_passed_time,
			(unsigned long)(battery->pdata->recharging_total_time * HZ))) {
			pr_debug("%s: Recharging Timer Expired\n", __func__);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			if (sec_bat_set_charge(battery, false)) {
				pr_err("%s: Fail to Set Charger\n",
					__func__);
				return true;
			}

			return false;
		}
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if (battery->charging_mode == SEC_BATTERY_CHARGING_NORMAL &&
			time_after(battery->charging_passed_time, (unsigned long)
			(battery->pdata->charging_total_time * HZ))) {
			pr_debug("%s: Charging Timer Expired\n", __func__);
			battery->status = POWER_SUPPLY_STATUS_FULL;
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			if (sec_bat_set_charge(battery, false)) {
				pr_err("%s: Fail to Set Charger\n",
					__func__);
				return true;
			}

			return false;
		}
		if (battery->pdata->charging_reset_time) {
			if (time_after(battery->charging_passed_time,
				battery->charging_next_time * HZ)) {
				/*reset current in charging status */
				battery->charging_next_time =
					battery->charging_passed_time +
					(battery->pdata->charging_reset_time *
					HZ);

				pr_debug("%s: Reset charging current\n",
					__func__);
				if (sec_bat_set_charge(battery, true)) {
					pr_err("%s: Fail to Set Charger\n",
						__func__);
					return true;
				}
			}
		}
		break;
	default:
		pr_err("%s: Undefine Battery Status\n", __func__);
		return true;
	}

	pr_info("%s: Charging Time : %u secs\n", __func__,
		jiffies_to_msecs(battery->charging_passed_time) / 1000);

	return true;
}

static bool sec_bat_check_fullcharged(
				struct sec_battery_info *battery)
{
	int current_adc;
	bool ret;
	int err;

	ret = false;

	if (battery->pdata->full_condition_type &
		SEC_BATTERY_FULL_CONDITION_SOC) {
		if (battery->capacity <
			battery->pdata->full_condition_soc) {
			pr_debug("%s: Not enough SOC (%d%%)\n",
				__func__, battery->capacity);
			goto not_full_charged;
		}
	}

	if (battery->pdata->full_condition_type &
		SEC_BATTERY_FULL_CONDITION_AVGVCELL) {
		if (battery->voltage_avg <
			battery->pdata->full_condition_avgvcell) {
			pr_debug("%s: Not enough AVGVCELL (%dmV)\n",
				__func__, battery->voltage_avg);
			goto not_full_charged;
		}
	}

	switch (battery->pdata->full_check_type) {
	case SEC_BATTERY_FULLCHARGED_ADC_DUAL:
		if (battery->charging_mode ==
			SEC_BATTERY_CHARGING_2ND) {
			current_adc =
				sec_bat_get_adc_value(battery,
				battery->pdata->full_check_adc_channel);

			pr_debug("%s: Current ADC (%d)\n",
				__func__, current_adc);

			if (current_adc < 0)
				break;
			battery->current_adc = current_adc;

			if (battery->current_adc <
				battery->pdata->full_check_adc_2nd) {
				battery->full_check_cnt++;
				pr_debug("%s: Full Check 2nd (%d)\n",
					__func__, battery->full_check_cnt);
			} else
				battery->full_check_cnt = 0;
			break;
		}
	case SEC_BATTERY_FULLCHARGED_ADC:
		current_adc =
			sec_bat_get_adc_value(battery,
			battery->pdata->full_check_adc_channel);

		pr_debug("%s: Current ADC (%d)\n",
			__func__, current_adc);

		if (current_adc < 0)
			break;
		battery->current_adc = current_adc;

		if (battery->current_adc <
			battery->pdata->full_check_adc_1st) {
			battery->full_check_cnt++;
			pr_debug("%s: Full Check ADC (%d)\n",
				__func__, battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_FG_CURRENT_DUAL:
		if (battery->charging_mode ==
			SEC_BATTERY_CHARGING_2ND) {
			if (battery->current_avg <
				battery->pdata->full_check_current_2nd) {
				battery->full_check_cnt++;
				pr_debug("%s: Full Check Current 2nd (%d)\n",
					__func__, battery->full_check_cnt);
			} else
				battery->full_check_cnt = 0;
			break;
		}
	case SEC_BATTERY_FULLCHARGED_FG_CURRENT:
		if (battery->current_avg <
			battery->pdata->full_check_current_1st) {
			battery->full_check_cnt++;
			pr_debug("%s: Full Check Current (%d)\n",
				__func__, battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_CHGGPIO:
		err = gpio_request(
			battery->pdata->chg_gpio_full_check,
			"GPIO_CHG_FULL");
		if (err) {
			pr_err("%s: Error in Request of GPIO\n",
				__func__);
			break;
		}
		if (!(gpio_get_value(
			battery->pdata->chg_gpio_full_check) ^
			!battery->pdata->chg_polarity_full_check)) {
			battery->full_check_cnt++;
			pr_debug("%s: Full Check GPIO (%d)\n",
				__func__, battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		gpio_free(battery->pdata->chg_gpio_full_check);
		break;

	case SEC_BATTERY_FULLCHARGED_CHGINT:
		break;

	default:
		pr_err("%s: Invalid Full Check\n", __func__);
		break;
	}

	if (battery->full_check_cnt >
		battery->pdata->full_check_count) {
		battery->full_check_cnt = 0;
		ret = true;
	}

not_full_charged:
	return ret;
}

static void sec_bat_do_fullcharged(
				struct sec_battery_info *battery)
{
	union power_supply_propval value;

	if (((battery->pdata->full_check_type ==
		SEC_BATTERY_FULLCHARGED_ADC_DUAL) ||
		(battery->pdata->full_check_type ==
		SEC_BATTERY_FULLCHARGED_FG_CURRENT_DUAL)) &&
		(battery->charging_mode ==
		SEC_BATTERY_CHARGING_NORMAL)) {
		battery->charging_mode = SEC_BATTERY_CHARGING_2ND;
	} else {
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		sec_bat_set_charge(battery, false);

		value.intval = POWER_SUPPLY_STATUS_FULL;
		psy_do_property("sec-fuelgauge", set,
			POWER_SUPPLY_PROP_STATUS, value);
	}

	battery->status = POWER_SUPPLY_STATUS_FULL;
}

static bool sec_bat_fullcharged_check(
				struct sec_battery_info *battery)
{
	if (battery->charging_mode == SEC_BATTERY_CHARGING_NONE) {
		pr_debug("%s: No Need to Check Full-Charged\n", __func__);
		return true;
	}

	if (sec_bat_check_fullcharged(battery))
		sec_bat_do_fullcharged(battery);

	pr_info("%s: Charging Mode : %s\n", __func__,
		sec_bat_charging_mode_str[battery->charging_mode]);

	return true;
};

static void sec_bat_get_battery_info(
				struct sec_battery_info *battery)
{
	union power_supply_propval value;

	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	battery->voltage_now = value.intval;

	value.intval = SEC_BATTEY_VOLTAGE_AVERAGE;
	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	battery->voltage_avg = value.intval;

	value.intval = SEC_BATTEY_VOLTAGE_VFOCV;
	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	battery->voltage_vfocv = value.intval;

	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value);
	battery->current_now = value.intval;

	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_CURRENT_AVG, value);
	battery->current_avg = value.intval;

	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_CAPACITY, value);
	battery->capacity = value.intval;

	switch (battery->pdata->thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
		psy_do_property("sec-fuelgauge", get,
			POWER_SUPPLY_PROP_TEMP, value);
		battery->temperature = value.intval;

		psy_do_property("sec-fuelgauge", get,
			POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
		battery->temper_amb = value.intval;
		break;
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		battery->pdata->get_temperature_callback(
			POWER_SUPPLY_PROP_TEMP, &value);
		psy_do_property("sec-fuelgauge", set,
			POWER_SUPPLY_PROP_TEMP, value);
		battery->temperature = value.intval;

		battery->pdata->get_temperature_callback(
			POWER_SUPPLY_PROP_TEMP_AMBIENT, &value);
		psy_do_property("sec-fuelgauge", set,
			POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
		battery->temper_amb = value.intval;
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		sec_bat_get_temperature_by_adc(battery,
			POWER_SUPPLY_PROP_TEMP, &value);
		psy_do_property("sec-fuelgauge", set,
			POWER_SUPPLY_PROP_TEMP, value);
		battery->temperature = value.intval;

		sec_bat_get_temperature_by_adc(battery,
			POWER_SUPPLY_PROP_TEMP_AMBIENT, &value);
		psy_do_property("sec-fuelgauge", set,
			POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
		battery->temper_amb = value.intval;
		break;
	default:
		break;
	}

	pr_info("%s: %s, SOC(%d%%)\n", __func__,
		battery->present ? "Connected" : "Disconnected",
		battery->capacity);
	pr_info("%s: Vnow(%dmV), Vavg(%dmV), Vocv(%dmV)\n",
		__func__, battery->voltage_now,
		battery->voltage_avg, battery->voltage_vfocv);
	pr_info("%s: Inow(%dmA), Iavg(%dmA), Iadc(%d)\n",
		__func__, battery->current_now,
		battery->current_avg, battery->current_adc);
	pr_info("%s: Tbat(%d.%d), Tamb(%d.%d)\n",
		__func__, battery->temperature / 10,
		battery->temperature % 10,
		battery->temper_amb / 10,
		battery->temper_amb % 10);
};

static void sec_bat_polling_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(
		work, struct sec_battery_info, polling_work.work);

	wake_lock(&battery->monitor_wake_lock);
	queue_work(battery->monitor_wqueue, &battery->monitor_work);
}

static void sec_bat_program_alarm(
				struct sec_battery_info *battery, int seconds)
{
	ktime_t low_interval = ktime_set(seconds, 0);
	ktime_t slack = ktime_set(10, 0);
	ktime_t next;

	next = ktime_add(battery->last_poll_time, low_interval);
	alarm_start_range(&battery->polling_alarm,
		next, ktime_add(next, slack));
}

static void sec_bat_alarm(struct alarm *alarm)
{
	struct sec_battery_info *battery = container_of(alarm,
				struct sec_battery_info, polling_alarm);

	wake_lock(&battery->monitor_wake_lock);
	queue_work(battery->monitor_wqueue, &battery->monitor_work);
}

static void sec_bat_monitor_work(
				struct work_struct *work)
{
	struct sec_battery_info *battery =
		container_of(work, struct sec_battery_info,
		monitor_work);
	unsigned long flags;

	sec_bat_get_battery_info(battery);

	/* 0. test mode */
	if (battery->test_activated)
		goto continue_monitor;

	/* 1. battery check */
	if (!sec_bat_battery_cable_check(battery))
		goto continue_monitor;

	/* 2. voltage check */
	if (!sec_bat_voltage_check(battery))
		goto continue_monitor;

	if (!battery->long_polling_activated &&
		((battery->pdata->normal_polling_time /
		battery->pdata->short_polling_time) >
		battery->polling_count++))
		goto continue_monitor;
	battery->polling_count = 0;

	/* 3. time management */
	if (!sec_bat_time_management(battery))
		goto continue_monitor;

	/* 4. temperature check */
	if (!sec_bat_temperature_check(battery))
		goto continue_monitor;

	/* 5. full charging check */
	sec_bat_fullcharged_check(battery);

continue_monitor:
	pr_info("%s: Status(%s), Health(%s)\n", __func__,
		sec_bat_status_str[battery->status],
		sec_bat_health_str[battery->health]);

	battery->test_activated = false;
	power_supply_changed(&battery->psy_bat);

	if (battery->long_polling_activated) {
		if (battery->pdata->long_polling_time)
			battery->polling_time =
				battery->pdata->long_polling_time;
		else {
			pr_err("%s: No Long Polling Time\n", __func__);
			battery->polling_time =
				battery->pdata->short_polling_time;
		}
	}

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		if (battery->pdata->monitor_initial_count) {
			battery->pdata->monitor_initial_count--;
			schedule_delayed_work(&battery->polling_work, HZ);
		} else
			schedule_delayed_work(&battery->polling_work,
				battery->polling_time * HZ);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		battery->last_poll_time = alarm_get_elapsed_realtime();
		local_irq_save(flags);
		if (battery->pdata->monitor_initial_count) {
			battery->pdata->monitor_initial_count--;
			sec_bat_program_alarm(battery, 1);
		} else
			sec_bat_program_alarm(battery, battery->polling_time);
		local_irq_restore(flags);
		break;
	case SEC_BATTERY_MONITOR_TIMER:
		break;
	default:
		break;
	}

	wake_unlock(&battery->monitor_wake_lock);

	return;
}

static void sec_bat_cable_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, cable_work);
	union power_supply_propval value;

	switch (battery->pdata->cable_source_type) {
	case SEC_BATTERY_CABLE_SOURCE_CALLBACK:
		battery->cable_type =
			battery->pdata->check_cable_status();
		break;
	case SEC_BATTERY_CABLE_SOURCE_ADC:
		battery->cable_type =
			sec_bat_get_charger_type_adc(battery);
		break;
	case SEC_BATTERY_CABLE_SOURCE_EXTERNAL:
		pr_debug("%s: Cable Type Defined\n", __func__);
	default:
		break;
	}

	if (battery->cable_type >= SEC_SIZEOF_POWER_SUPPLY_TYPE) {
		pr_err("%s: Invalid cable type\n", __func__);
		goto end_of_cable_work;
	}

	if (battery->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		battery->status = POWER_SUPPLY_STATUS_DISCHARGING;
		battery->health = POWER_SUPPLY_HEALTH_GOOD;

		if (sec_bat_set_charge(battery, false))
			goto end_of_cable_work;

		wake_lock_timeout(&battery->vbus_wake_lock, HZ * 5);
	} else {
		battery->charging_mode = SEC_BATTERY_CHARGING_NORMAL;
		battery->status = POWER_SUPPLY_STATUS_CHARGING;

		if (sec_bat_set_charge(battery, true))
			goto end_of_cable_work;

		/* No need for wakelock in Alarm */
		if (battery->pdata->polling_type != SEC_BATTERY_MONITOR_ALARM)
			wake_lock(&battery->vbus_wake_lock);
	}

	power_supply_changed(&battery->psy_ac);
	power_supply_changed(&battery->psy_usb);

end_of_cable_work:
	wake_unlock(&battery->cable_wake_lock);
}

static ssize_t sec_bat_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);
	const ptrdiff_t offset = attr - sec_battery_attrs;
	int i = 0;

	switch (offset) {
	case BATT_EVENT_BATT_RESET_SOC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_2G_CALL) ? 1 : 0);
		break;
		break;
	case BATT_EVENT_BATT_READ_RAW_SOC:
		break;
	case BATT_EVENT_BATT_READ_ADJ_SOC:
		break;
	case BATT_EVENT_BATT_TYPE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			battery->pdata->vendor);
		break;
	case BATT_EVENT_BATT_VFOCV:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->voltage_vfocv);
		break;
	case BATT_EVENT_BATT_VOL_ADC:
		break;
	case BATT_EVENT_BATT_VOL_ADC_CAL:
		break;
	case BATT_EVENT_BATT_VOL_AVER:
		break;
	case BATT_EVENT_BATT_VOL_ADC_AVER:
		break;
	case BATT_EVENT_BATT_TEMP_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->temp_adc);
		break;
	case BATT_EVENT_BATT_TEMP_AVER:
		break;
	case BATT_EVENT_BATT_TEMP_ADC_AVER:
		break;
	case BATT_EVENT_BATT_VF_ADC:
		break;

	case BATT_EVENT_BATT_LP_CHARGING:
		break;
	case BATT_EVENT_SIOP_ACTIVATED:
		break;
	case BATT_EVENT_BATT_CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->cable_type);
		break;
	case BATT_EVENT_FG_REG_DUMP:
		break;
	case BATT_EVENT_FG_RESET_CAP:
		break;
	case BATT_EVENT_AUTH:
		break;
	case BATT_EVENT_CHG_CURRENT_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->current_adc);
		break;
	case BATT_EVENT_WC_ADC:
		break;
	case BATT_EVENT_WC_STATUS:
		break;

	case BATT_EVENT_2G_CALL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_2G_CALL) ? 1 : 0);
		break;
	case BATT_EVENT_3G_CALL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_3G_CALL) ? 1 : 0);
		break;
	case BATT_EVENT_MUSIC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_MUSIC) ? 1 : 0);
		break;
	case BATT_EVENT_VIDEO:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_VIDEO) ? 1 : 0);
		break;
	case BATT_EVENT_BROWSER:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_BROWSER) ? 1 : 0);
		break;
	case BATT_EVENT_HOTSPOT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_HOTSPOT) ? 1 : 0);
		break;
	case BATT_EVENT_CAMERA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_CAMERA) ? 1 : 0);
		break;
	case BATT_EVENT_CAMCORDER:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_CAMCORDER) ? 1 : 0);
		break;
	case BATT_EVENT_DATA_CALL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_DATA_CALL) ? 1 : 0);
		break;
	case BATT_EVENT_WIFI:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_WIFI) ? 1 : 0);
		break;
	case BATT_EVENT_WIBRO:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_WIBRO) ? 1 : 0);
		break;
	case BATT_EVENT_LTE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->event & EVENT_LTE) ? 1 : 0);
		break;
	case BATT_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->event);
		break;
	default:
		i = -EINVAL;
	}

	return i;
}

static ssize_t sec_bat_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);
	const ptrdiff_t offset = attr - sec_battery_attrs;
	int ret = 0;
	int x = 0;

	switch (offset) {
	case BATT_EVENT_BATT_RESET_SOC:
		break;
	case BATT_EVENT_BATT_READ_RAW_SOC:
		break;
	case BATT_EVENT_BATT_READ_ADJ_SOC:
		break;
	case BATT_EVENT_BATT_TYPE:
		break;
	case BATT_EVENT_BATT_VFOCV:
		break;
	case BATT_EVENT_BATT_VOL_ADC:
		break;
	case BATT_EVENT_BATT_VOL_ADC_CAL:
		break;
	case BATT_EVENT_BATT_VOL_AVER:
		break;
	case BATT_EVENT_BATT_VOL_ADC_AVER:
		break;
	case BATT_EVENT_BATT_TEMP_ADC:
		break;
	case BATT_EVENT_BATT_TEMP_AVER:
		break;
	case BATT_EVENT_BATT_TEMP_ADC_AVER:
		break;
	case BATT_EVENT_BATT_VF_ADC:
		break;

	case BATT_EVENT_BATT_LP_CHARGING:
		break;
	case BATT_EVENT_SIOP_ACTIVATED:
		break;
	case BATT_EVENT_BATT_CHARGING_SOURCE:
		break;
	case BATT_EVENT_FG_REG_DUMP:
		break;
	case BATT_EVENT_FG_RESET_CAP:
		break;
	case BATT_EVENT_AUTH:
		break;
	case BATT_EVENT_CHG_CURRENT_ADC:
		break;
	case BATT_EVENT_WC_ADC:
		break;
	case BATT_EVENT_WC_STATUS:
		break;

	case BATT_EVENT_2G_CALL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_2G_CALL, x);
			ret = count;
		}
		break;
	case BATT_EVENT_3G_CALL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_3G_CALL, x);
			ret = count;
		}
		break;
	case BATT_EVENT_MUSIC:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_MUSIC, x);
			ret = count;
		}
		break;
	case BATT_EVENT_VIDEO:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_VIDEO, x);
			ret = count;
		}
		break;
	case BATT_EVENT_BROWSER:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_BROWSER, x);
			ret = count;
		}
		break;
	case BATT_EVENT_HOTSPOT:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_HOTSPOT, x);
			ret = count;
		}
		break;
	case BATT_EVENT_CAMERA:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_CAMERA, x);
			ret = count;
		}
		break;
	case BATT_EVENT_CAMCORDER:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_CAMCORDER, x);
			ret = count;
		}
		break;
	case BATT_EVENT_DATA_CALL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_DATA_CALL, x);
			ret = count;
		}
		break;
	case BATT_EVENT_WIFI:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_WIFI, x);
			ret = count;
		}
		break;
	case BATT_EVENT_WIBRO:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_WIBRO, x);
			ret = count;
		}
		break;
	case BATT_EVENT_LTE:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_event_set(battery, EVENT_LTE, x);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int sec_bat_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sec_battery_attrs); i++) {
		rc = device_create_file(dev, &sec_battery_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_battery_attrs[i]);
create_attrs_succeed:
	return rc;
}

static int sec_bat_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);

	pr_debug("%s: (%d,%d)\n", __func__, psp, val->intval);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if ((battery->pdata->full_check_type ==
			SEC_BATTERY_FULLCHARGED_CHGINT) &&
			(val->intval == POWER_SUPPLY_STATUS_FULL))
			sec_bat_do_fullcharged(battery);
		battery->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		battery->health = val->intval;
		switch (battery->health) {
		/* OVP/UVLO from Interrupt */
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			sec_bat_ovp_uvlo_result(battery);
			break;
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* cable is attached or detached */
		pr_info("%s: Cable was Changed(%d)\n", __func__, val->intval);
		battery->cable_type = val->intval;
		wake_lock(&battery->cable_wake_lock);
		queue_work(battery->monitor_wqueue, &battery->cable_work);
		break;
	default:
		return -EINVAL;
	}

	battery->test_activated = true;
	return 0;
}

static int sec_bat_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = battery->status;
		break;
	/* charging mode (differ from power supply) */
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = battery->charging_mode;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = battery->health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = battery->present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = battery->cable_type;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = battery->pdata->technology;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery->voltage_now;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = battery->voltage_avg;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = battery->current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = battery->current_avg;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = battery->capacity;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->temperature;
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = battery->temper_amb;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sec_usb_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = (battery->cable_type == POWER_SUPPLY_TYPE_USB);

	return 0;
}

static int sec_ac_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct sec_battery_info *battery =
		container_of(psy, struct sec_battery_info, psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (battery->cable_type == POWER_SUPPLY_TYPE_MAINS);

	return 0;
}

static irqreturn_t sec_bat_irq_thread(int irq, void *irq_data)
{
	struct sec_battery_info *battery = irq_data;

	if (battery->pdata->cable_check_type &
		SEC_BATTERY_CABLE_CHECK_INT) {
		wake_lock(&battery->cable_wake_lock);
		queue_work(battery->monitor_wqueue, &battery->cable_work);
	}

	return IRQ_HANDLED;
}

static int __devinit sec_battery_probe(struct platform_device *pdev)
{
	sec_battery_platform_data_t *pdata = dev_get_platdata(&pdev->dev);
	struct sec_battery_info *battery;
	int irq = 0;
	int ret = 0;

	pr_debug("%s: SEC Battery Driver Loading\n", __func__);

	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;

	platform_set_drvdata(pdev, battery);

	battery->dev = &pdev->dev;
	battery->pdata = pdata;

	mutex_init(&battery->adclock);
	pr_debug("%s: ADC init\n", __func__);
	adc_init(pdev, pdata, pdata->cable_check_adc_channel);
	adc_init(pdev, pdata, pdata->check_adc_channel);
	adc_init(pdev, pdata, pdata->temp_adc_channel);
	adc_init(pdev, pdata, pdata->temp_amb_adc_channel);
	adc_init(pdev, pdata, pdata->full_check_adc_channel);

	wake_lock_init(&battery->monitor_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-monitor");
	wake_lock_init(&battery->cable_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-cable");
	wake_lock_init(&battery->vbus_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-vbus");

	/* initialization of battery info */
	battery->status = POWER_SUPPLY_STATUS_DISCHARGING;
	battery->health = POWER_SUPPLY_HEALTH_GOOD;
	battery->present = false;

	battery->long_polling_activated = false;
	battery->polling_count = 0;
	battery->polling_time = pdata->short_polling_time;

	battery->check_count = 0;
	battery->check_adc_count = 0;
	battery->check_adc_value = 0;

	battery->charging_start_time = 0;
	battery->charging_passed_time = 0;
	battery->charging_next_time = 0;

	setup_timer(&battery->event_expired_timer,
		sec_bat_event_expired_timer_func, (unsigned long)battery);

	battery->temp_high_threshold =
		pdata->temp_high_threshold_normal;
	battery->temp_high_recovery =
		pdata->temp_high_recovery_normal;
	battery->temp_low_recovery =
		pdata->temp_low_recovery_normal;
	battery->temp_low_threshold =
		pdata->temp_low_threshold_normal;

	battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
	battery->cable_type = POWER_SUPPLY_TYPE_BATTERY;
	battery->test_activated = false;

	battery->psy_bat.name = "battery",
	battery->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY,
	battery->psy_bat.properties = sec_battery_props,
	battery->psy_bat.num_properties = ARRAY_SIZE(sec_battery_props),
	battery->psy_bat.get_property = sec_bat_get_property,
	battery->psy_bat.set_property = sec_bat_set_property,
	battery->psy_usb.name = "usb",
	battery->psy_usb.type = POWER_SUPPLY_TYPE_USB,
	battery->psy_usb.supplied_to = supply_list,
	battery->psy_usb.num_supplicants = ARRAY_SIZE(supply_list),
	battery->psy_usb.properties = sec_power_props,
	battery->psy_usb.num_properties = ARRAY_SIZE(sec_power_props),
	battery->psy_usb.get_property = sec_usb_get_property,
	battery->psy_ac.name = "ac",
	battery->psy_ac.type = POWER_SUPPLY_TYPE_MAINS,
	battery->psy_ac.supplied_to = supply_list,
	battery->psy_ac.num_supplicants = ARRAY_SIZE(supply_list),
	battery->psy_ac.properties = sec_power_props,
	battery->psy_ac.num_properties = ARRAY_SIZE(sec_power_props),
	battery->psy_ac.get_property = sec_ac_get_property;

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &battery->psy_bat);
	if (ret) {
		pr_err("%s: Failed to Register psy_bat\n",
			__func__);
		goto err_wake_lock;
	}

	ret = power_supply_register(&pdev->dev, &battery->psy_usb);
	if (ret) {
		pr_err("%s: Failed to Register psy_usb\n",
			__func__);
		goto err_supply_unreg_bat;
	}

	ret = power_supply_register(&pdev->dev, &battery->psy_ac);
	if (ret) {
		pr_err("%s: Failed to Register psy_ac\n", __func__);
		goto err_supply_unreg_usb;
	}

	/* create work queue */
	battery->monitor_wqueue =
	    create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!battery->monitor_wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_supply_unreg_ac;
	}
	INIT_WORK(&battery->monitor_work, sec_bat_monitor_work);
	INIT_WORK(&battery->cable_work, sec_bat_cable_work);

	if (battery->pdata->bat_gpio_irq) {
		irq = gpio_to_irq(battery->pdata->bat_gpio_irq);
		ret = request_threaded_irq(irq, NULL,
				sec_bat_irq_thread,
				battery->pdata->bat_irq_attr,
				"battery-irq", battery);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
			return ret;
		}

		ret = enable_irq_wake(irq);
		if (ret < 0)
			pr_err("%s: Failed to Enable Wakeup Source(%d)\n"
				, __func__, ret);
	}

	ret = sec_bat_create_attrs(battery->psy_bat.dev);
	if (ret) {
		pr_err("%s : Failed to create_attrs\n", __func__);
		goto err_supply_unreg_ac;
	}

	switch (pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		INIT_DELAYED_WORK_DEFERRABLE(&battery->polling_work,
			sec_bat_polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		battery->last_poll_time = alarm_get_elapsed_realtime();
		alarm_init(&battery->polling_alarm,
			ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
			sec_bat_alarm);
		break;
	default:
		break;
	}

	wake_lock(&battery->monitor_wake_lock);
	queue_work(battery->monitor_wqueue, &battery->monitor_work);

	pdata->initial_check();

	pr_debug("%s: SEC Battery Driver Loaded\n", __func__);
	return 0;

err_supply_unreg_ac:
	power_supply_unregister(&battery->psy_ac);
err_supply_unreg_usb:
	power_supply_unregister(&battery->psy_usb);
err_supply_unreg_bat:
	power_supply_unregister(&battery->psy_bat);
err_wake_lock:
	wake_lock_destroy(&battery->monitor_wake_lock);
	wake_lock_destroy(&battery->cable_wake_lock);
	wake_lock_destroy(&battery->vbus_wake_lock);
	mutex_destroy(&battery->adclock);
	kfree(battery);

	return ret;
}

static int __devexit sec_battery_remove(struct platform_device *pdev)
{
	struct sec_battery_info *battery = platform_get_drvdata(pdev);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		cancel_delayed_work(&battery->polling_work);
	case SEC_BATTERY_MONITOR_ALARM:
		alarm_cancel(&battery->polling_alarm);
		break;
	default:
		break;
	}

	flush_workqueue(battery->monitor_wqueue);
	destroy_workqueue(battery->monitor_wqueue);
	wake_lock_destroy(&battery->monitor_wake_lock);
	wake_lock_destroy(&battery->cable_wake_lock);
	wake_lock_destroy(&battery->vbus_wake_lock);

	mutex_destroy(&battery->adclock);
	adc_exit(battery->pdata,
		battery->pdata->cable_check_adc_channel);
	adc_exit(battery->pdata,
		battery->pdata->check_adc_channel);
	adc_exit(battery->pdata,
		battery->pdata->temp_adc_channel);
	adc_exit(battery->pdata,
		battery->pdata->temp_amb_adc_channel);
	adc_exit(battery->pdata,
		battery->pdata->full_check_adc_channel);

	power_supply_unregister(&battery->psy_ac);
	power_supply_unregister(&battery->psy_usb);
	power_supply_unregister(&battery->psy_bat);

	kfree(battery);

	return 0;
}

static int sec_battery_prepare(struct device *dev)
{

	return 0;
}

static int sec_battery_suspend(struct device *dev)
{
	struct sec_battery_info *battery
		= dev_get_drvdata(dev);

	cancel_work_sync(&battery->monitor_work);

	if (battery->pdata->polling_type ==
		SEC_BATTERY_MONITOR_WORKQUEUE)
		cancel_delayed_work(&battery->polling_work);

	return 0;
}

static int sec_battery_resume(struct device *dev)
{
	struct sec_battery_info *battery
		= dev_get_drvdata(dev);

	wake_lock(&battery->monitor_wake_lock);
	queue_work(battery->monitor_wqueue,
		&battery->monitor_work);

	if (battery->pdata->polling_type ==
		SEC_BATTERY_MONITOR_WORKQUEUE)
		schedule_delayed_work(&battery->polling_work,
			battery->polling_time * HZ);

	return 0;
}

static void sec_battery_complete(struct device *dev)
{

	return;
}

static void sec_battery_shutdown(struct device *dev)
{
}

static const struct dev_pm_ops sec_battery_pm_ops = {
	.prepare = sec_battery_prepare,
	.suspend = sec_battery_suspend,
	.resume = sec_battery_resume,
	.complete = sec_battery_complete,
};

static struct platform_driver sec_battery_driver = {
	.driver = {
		   .name = "sec-battery",
		   .owner = THIS_MODULE,
		   .pm = &sec_battery_pm_ops,
		   .shutdown = sec_battery_shutdown,
		   },
	.probe = sec_battery_probe,
	.remove = __devexit_p(sec_battery_remove),
};

static int __init sec_battery_init(void)
{
	return platform_driver_register(&sec_battery_driver);
}

static void __exit sec_battery_exit(void)
{
	platform_driver_unregister(&sec_battery_driver);
}

late_initcall(sec_battery_init);
module_exit(sec_battery_exit);

MODULE_DESCRIPTION("Samsung Battery Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
