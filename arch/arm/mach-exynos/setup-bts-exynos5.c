/*
 * linux/arch/arm/mach-exynos/setup-bts-exynos5.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Setup IP Bus Traffic Shaper for EXYNOS5.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/err.h>
#include <linux/io.h>
#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/clock.h>

#include <mach/map.h>
#include <mach/bts-exynos5.h>

static struct exynos5_bts_info *bts_info;

static void set_bts_reg(void __iomem *addr, unsigned long val)
{
	__raw_writel(val, addr);
}

int exynos5_setup_bts_param(struct exynos5_bts_info *info,
				unsigned long size)
{
	pr_info("EXYNOS5: Setup Bus Traffic Shaper parameters.\n");

	bts_info = kmalloc(size, GFP_KERNEL);
	if (!bts_info) {
		pr_err("%s: failed allocation memory\n", __func__);
		return -ENOMEM;
	}

	memcpy(bts_info, info, size);

	return 0;
}

int set_bts_ip(enum exynos5_bts_block blk, struct device *dev)
{
	struct clk *clk;
	int i;
	int check_clock = 0;

	if (bts_info[blk].clk_name != NULL) {
		clk = clk_get(dev, bts_info[blk].clk_name);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to get %s clock\n",
					__func__, bts_info[blk].clk_name);
			return -EINVAL;
		}

		clk_enable(clk);

		check_clock = 1;
	}

	for (i = 0; i < bts_info[blk].num_reg; i++)
		set_bts_reg(bts_info[blk].reg_addr[i], bts_info[blk].set_val[i]);

	if (check_clock == 1) {
		clk_disable(clk);
		clk_put(clk);
	}

	return 0;
}
