/* linux/arch/arm/mach-exynos/include/mach/bts-exynos5.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5 - Bus Traffic Shaper definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_BTS_EXYNOS5_H
#define __ASM_ARCH_BTS_EXYNOS5_H __FILE__

#define MAX_NUM_REG	64

enum exynos5_bts_block {
	BTS_CPU,
	BTS_C2C,
	BTS_DISP10,
	BTS_DISP11,
	BTS_TV0,
	BTS_TV1,
	BTS_JPEG,
	BTS_FIMC_ISP,
	BTS_FIMC_SCALER_C,
	BTS_FIMC_SCALER_P,
	BTS_FIMC_FD,
	BTS_FIMC_ODC,
	BTS_FIMC_DIS0,
	BTS_FIMC_DIS1,
	BTS_FIMC_3DNR,
	BTS_MDMA1,
	BTS_ROTATOR,
	BTS_GSCL0,
	BTS_GSCL1,
	BTS_GSCL2,
	BTS_GSCL3,
	BTS_MFC0,
	BTS_MFC1,
	BTS_G3D,
	DDR_R1_FBM,
	DDR_R0_FBM,
};

struct exynos5_bts_info {
	char		*clk_name;
	void __iomem	*reg_addr[MAX_NUM_REG];
	unsigned long	set_val[MAX_NUM_REG];
	int		num_reg;
};

extern int exynos5_setup_bts_param(struct exynos5_bts_info *info,
					unsigned long size);
extern int set_bts_ip(enum exynos5_bts_block blk, struct device *dev);

#ifdef CONFIG_CPU_EXYNOS5250
extern void set_bts_cpu(void);
extern void set_bts_fbm(void);
extern void set_bts_c2c(void);
extern void set_bts_g3d_acp(void);
extern void set_bts_disp1(void);
extern void set_bts_gscl(void);
extern void set_bts_isp(void);
extern void set_bts_mfc(void);
extern void set_bts_top(void);
extern int exynos5250_setup_bts(void);
#endif

#endif /* __ASM_ARCH_BTS_EXYNOS5_H */
