/*
 * bq24190_charger.h
 * Samsung SMB328 Charger Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
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

#ifndef __BQ24190_CHARGER_H
#define __BQ24190_CHARGER_H __FILE__

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#if defined(CONFIG_CHARGER_BQ24191)
#define SEC_CHARGER_I2C_SLAVEADDR	0x6a
#else /* CONFIG_CHARGER_BQ24190 */
#define SEC_CHARGER_I2C_SLAVEADDR	0x6b
#endif

/* BQ24190 Registers. */
#define BQ24190_INPUT_SOURCE_CTRL		0x00
#define BQ24190_POWERON_CONFIG			0x01
#define BQ24190_CHARGE_CURRENT_CTRL		0x02
#define BQ24190_PRECHARGE_TERMINATION	0x03
#define BQ24190_CHARGE_VOLTAGE_CTRL		0x04
#define BQ24190_TERMINATION_TIMER_CTRL	0x05
#define BQ24190_IR_COMP_THERMAL_REG		0x06
#define BQ24190_MISC_OPERATION			0x07
#define BQ24190_SYSTEM_STATUS			0x08
#define BQ24190_FAULT					0x09
#define BQ24190_VENDOR_PART_REV			0x0A

#define BQ24190_CHARGING_ENABLE			0x20
#define BQ24190_CHARGING_DONE			0x30

#endif /* __BQ24190_CHARGER_H */
