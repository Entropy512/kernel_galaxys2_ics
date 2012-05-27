/*
 * Driver model for sensor
 *
 * Copyright (C) 2008 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LINUX_SENSORS_CORE_H_INCLUDED
#define __LINUX_SENSORS_CORE_H_INCLUDED

extern struct device *sensors_classdev_register(char *sensors_name);
extern void sensors_classdev_unregister(struct device *dev);

struct accel_platform_data {
	int (*accel_get_position) (void);
};
#endif	/* __LINUX_SENSORS_CORE_H_INCLUDED */
