/*
 * linux/arch/arm/mach-exynos/setup-bts-exynos5250.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Setup IP Bus Traffic Shaper for exynos5250.
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

void set_bts_cpu(void)
{
	set_bts_ip(BTS_CPU, NULL);
}

void set_bts_fbm(void)
{
	set_bts_ip(DDR_R1_FBM, NULL);
	set_bts_ip(DDR_R0_FBM, NULL);
}

#ifdef CONFIG_EXYNOS_C2C
void set_bts_c2c(void)
{
	if (set_bts_ip(BTS_C2C, &exynos_device_c2c.dev) != 0)
		pr_err("%s: failed setup BTS to c2c\n", __func__);
}
#endif

void set_bts_g3d_acp(void)
{
	if (set_bts_ip(BTS_G3D, NULL) != 0)
		pr_err("%s: failed setup BTS to g3d\n", __func__);
}

void set_bts_disp1(void)
{
	if (set_bts_ip(BTS_DISP10, &s5p_device_fimd1.dev) != 0)
		pr_err("%s: failed setup BTS to disp10\n", __func__);

	if (set_bts_ip(BTS_DISP11, &s5p_device_fimd1.dev) != 0)
		pr_err("%s: failed setup BTS to disp11\n", __func__);

	if (set_bts_ip(BTS_TV0, &s5p_device_mixer.dev) != 0)
		pr_err("%s: failed setup BTS to tv0\n", __func__);

	if (set_bts_ip(BTS_TV1, &s5p_device_mixer.dev) != 0)
		pr_err("%s: failed setup BTS to tv1\n", __func__);

	if (set_bts_ip(BTS_ROTATOR, &exynos_device_rotator.dev) != 0)
		pr_err("%s: failed setup BTS to rotator\n", __func__);
}

void set_bts_gscl(void)
{
	if (set_bts_ip(BTS_JPEG, NULL) != 0)
		pr_err("%s: failed setup BTS to jpeg\n", __func__);

	if (set_bts_ip(BTS_GSCL0, &exynos5_device_gsc0.dev) != 0)
		pr_err("%s: failed setup BTS to gscaler0\n", __func__);

	if (set_bts_ip(BTS_GSCL1, &exynos5_device_gsc1.dev) != 0)
		pr_err("%s: failed setup BTS to gscaler1\n", __func__);

	if (set_bts_ip(BTS_GSCL2, &exynos5_device_gsc2.dev) != 0)
		pr_err("%s: failed setup BTS to gscaler2\n", __func__);

	if (set_bts_ip(BTS_GSCL3, &exynos5_device_gsc3.dev) != 0)
		pr_err("%s: failed setup BTS to gscaler3\n", __func__);
}

void set_bts_isp(void)
{
	if (set_bts_ip(BTS_FIMC_ISP, &exynos5_device_fimc_is.dev) != 0)
		pr_err("%s: failed setup BTS to fimc_isp\n", __func__);

	if (set_bts_ip(BTS_FIMC_SCALER_C, &exynos5_device_fimc_is.dev) != 0)
		pr_err("%s: failed setup BTS to fimc_scaler_c\n", __func__);

	if (set_bts_ip(BTS_FIMC_SCALER_P, &exynos5_device_fimc_is.dev) != 0)
		pr_err("%s: failed setup BTS to fimc_scaler_p\n", __func__);

	if (set_bts_ip(BTS_FIMC_FD, &exynos5_device_fimc_is.dev) != 0)
		pr_err("%s: failed setup BTS to fimc_fd\n", __func__);

	if (set_bts_ip(BTS_FIMC_ODC, &exynos5_device_fimc_is.dev) != 0)
		pr_err("%s: failed setup BTS to fimc_odc\n", __func__);

	if (set_bts_ip(BTS_FIMC_DIS0, &exynos5_device_fimc_is.dev) != 0)
		pr_err("%s: failed setup BTS to fimc_dis0\n", __func__);

	if (set_bts_ip(BTS_FIMC_DIS1, &exynos5_device_fimc_is.dev) != 0)
		pr_err("%s: failed setup BTS to fimc_dis1\n", __func__);

	if (set_bts_ip(BTS_FIMC_3DNR, &exynos5_device_fimc_is.dev) != 0)
		pr_err("%s: failed setup BTS to fimc_3dnr\n", __func__);
}

void set_bts_mfc(void)
{
	if (set_bts_ip(BTS_MFC0, &s5p_device_mfc.dev) != 0)
		pr_err("%s: failed setup BTS to mfc0\n", __func__);

	if (set_bts_ip(BTS_MFC1, &s5p_device_mfc.dev) != 0)
		pr_err("%s: failed setup BTS to mfc1\n", __func__);
}

void set_bts_top(void)
{
	if (set_bts_ip(BTS_MDMA1, &exynos_device_mdma.dev) != 0)
		pr_err("%s: failed setup BTS to mdma1\n", __func__);
}

int exynos5250_setup_bts(void)
{
	pr_info("%s: setup Bus Traffic Shaper for EXYNOS5250\n", __func__);

	set_bts_cpu();
	set_bts_fbm();
#ifdef CONFIG_EXYNOS_C2C
	set_bts_c2c();
#endif
	set_bts_g3d_acp();
	set_bts_disp1();
	set_bts_gscl();
	set_bts_isp();
	set_bts_mfc();
	set_bts_top();

	return 0;
}

late_initcall(exynos5250_setup_bts);
