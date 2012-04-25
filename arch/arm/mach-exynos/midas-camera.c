/*
 * camera class init
 */

#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include <linux/regulator/machine.h>

#include <plat/devs.h>
#include <plat/csis.h>
#include <plat/pd.h>
#include <plat/gpio-cfg.h>
#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
#include <plat/fimc-core.h>
#endif

#include <media/exynos_flite.h>
#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
#include <media/s5p_fimc.h>
#endif

#if defined(CONFIG_VIDEO_S5C73M3) || defined(CONFIG_VIDEO_SLP_S5C73M3)
#include <media/s5c73m3_platform.h>
#endif

#if defined(CONFIG_VIDEO_M5MO)
#include <mach/regs-gpio.h>
#include <media/m5mo_platform.h>
#endif

#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
#include <mach/secmem.h>
#endif

#ifdef CONFIG_VIDEO_SR200PC20M
#include <media/sr200pc20m_platform.h>
#endif

struct class *camera_class;

static int __init camera_class_init(void)
{
	camera_class = class_create(THIS_MODULE, "camera");

	return 0;
}

subsys_initcall(camera_class_init);

#if defined(CONFIG_VIDEO_FIMC)
/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * This function also called at fimc_init_camera()
 * Do optimization for cameras on your platform.
 */

#define CAM_CHECK_ERR_RET(x, msg)					\
	if (unlikely((x) < 0)) {					\
		printk(KERN_ERR "\nfail to %s: err = %d\n", msg, x);	\
		return x;						\
	}
#define CAM_CHECK_ERR(x, msg)						\
	if (unlikely((x) < 0)) {					\
		printk(KERN_ERR "\nfail to %s: err = %d\n", msg, x);	\
	}
#define CAM_CHECK_ERR_GOTO(x, out, fmt, ...) \
	if (unlikely((x) < 0)) { \
		printk(KERN_ERR fmt, ##__VA_ARGS__); \
		goto out; \
	}

int s3c_csis_power(int enable)
{
	struct regulator *regulator;
	int ret = 0;

	/* mipi_1.1v ,mipi_1.8v are always powered-on.
	 * If they are off, we then power them on.
	 */
	if (enable) {
		/* VMIPI_1.0V */
		regulator = regulator_get(NULL, "vmipi_1.0v");
		if (IS_ERR(regulator))
			goto error_out;
		regulator_enable(regulator);
		regulator_put(regulator);

		/* VMIPI_1.8V */
		regulator = regulator_get(NULL, "vmipi_1.8v");
		if (IS_ERR(regulator))
			goto error_out;
		regulator_enable(regulator);
		regulator_put(regulator);

		printk(KERN_WARNING "%s: vmipi_1.0v and vmipi_1.8v were ON\n",
		       __func__);
	} else {
		/* VMIPI_1.8V */
		regulator = regulator_get(NULL, "vmipi_1.8v");
		if (IS_ERR(regulator))
			goto error_out;
		if (regulator_is_enabled(regulator)) {
			printk(KERN_WARNING "%s: vmipi_1.8v is on. so OFF\n",
			       __func__);
			ret = regulator_disable(regulator);
		}
		regulator_put(regulator);

		/* VMIPI_1.0V */
		regulator = regulator_get(NULL, "vmipi_1.0v");
		if (IS_ERR(regulator))
			goto error_out;
		if (regulator_is_enabled(regulator)) {
			printk(KERN_WARNING "%s: vmipi_1.1v is on. so OFF\n",
			       __func__);
			ret = regulator_disable(regulator);
		}
		regulator_put(regulator);

		printk(KERN_WARNING "%s: vmipi_1.0v and vmipi_1.8v were OFF\n",
		       __func__);
	}

	return 0;

error_out:
	printk(KERN_ERR "%s: ERROR: failed to check mipi-power\n", __func__);
	return 0;
}

#ifdef WRITEBACK_ENABLED
static int get_i2c_busnum_writeback(void)
{
	return 0;
}

static struct i2c_board_info writeback_i2c_info = {
	I2C_BOARD_INFO("WriteBack", 0x0),
};

static struct s3c_platform_camera writeback = {
	.id		= CAMERA_WB,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.get_i2c_busnum = get_i2c_busnum_writeback,
	.info		= &writeback_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUV444,
	.line_length	= 800,
	.width		= 480,
	.height		= 800,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 480,
		.height	= 800,
	},

	.initialized	= 0,
};
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
#ifdef CONFIG_VIDEO_S5K6A3
static int s5k6a3_gpio_request(void)
{
	int ret = 0;

	/* SENSOR_A2.8V */
	ret = gpio_request(GPIO_CAM_IO_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_CAM_IO_EN)\n");
		return ret;
	}

	ret = gpio_request(GPIO_ISP_CORE_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_ISP_CORE_EN)\n");
		return ret;
	}

	if (system_rev <= 0x08)
		ret = gpio_request(GPIO_CAM_MCLK, "GPJ1");
	else
		ret = gpio_request(GPIO_VTCAM_MCLK, "GPM2");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_VTCAM_MCLK)\n");
		return ret;
	}

	ret = gpio_request(GPIO_CAM_VT_nRST, "GPM1");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_CAM_VT_nRST)\n");
		return ret;
	}

	return ret;
}

static int s5k6a3_power_on(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	s5k6a3_gpio_request();

	/* CAM_ISP_CORE_1.2V */
	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_CORE_EN");

	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_core_1.2v");

	/* CAM_SENSOR_A2.8V */
	ret = gpio_direction_output(GPIO_CAM_IO_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_IO_EN");
	/* delay is needed : external LDO control is slower than MCLK control*/
	udelay(100);

	/* MCLK */
	if (system_rev <= 0x08) {
		ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(GPIO_CAM_MCLK, S5P_GPIO_DRVSTR_LV3);
	} else {
		ret = s3c_gpio_cfgpin(GPIO_VTCAM_MCLK, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(GPIO_VTCAM_MCLK, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(GPIO_VTCAM_MCLK, S5P_GPIO_DRVSTR_LV3);
	}
	CAM_CHECK_ERR_RET(ret, "cfg mclk");

	/* VT_CORE_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable vt_cam_1.8v");

#if 0
	/* CAM_ISP_SENSOR_1.8V */
	regulator = regulator_get(NULL, "cam_isp_sensor_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_sensor_1.8v");
#endif

	/* CAM_ISP_MIPI_1.2V */
	regulator = regulator_get(NULL, "cam_isp_mipi_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_mipi_1.2v");

	/* VT_RESET */
	ret = gpio_direction_output(GPIO_CAM_VT_nRST, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_VT_nRST");

	gpio_free(GPIO_CAM_IO_EN);
	gpio_free(GPIO_ISP_CORE_EN);
	gpio_free(GPIO_CAM_VT_nRST);
	if (system_rev <= 0x08)
		gpio_free(GPIO_CAM_MCLK);
	else
		gpio_free(GPIO_VTCAM_MCLK);

	return ret;
}

static int s5k6a3_power_down(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	s5k6a3_gpio_request();

	/* VT_RESET */
	ret = gpio_direction_output(GPIO_CAM_VT_nRST, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_VT_nRST");

	/* CAM_ISP_MIPI_1.2V */
	regulator = regulator_get(NULL, "cam_isp_mipi_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_mipi_1.2v");

#if 0
	/* CAM_ISP_SENSOR_1.8V */
	regulator = regulator_get(NULL, "cam_isp_sensor_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_sensor_1.8v");
#endif

	/* VT_CORE_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable vt_cam_1.8v");

	/* MCLK */
	if (system_rev <= 0x08) {
		ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_DOWN);

	} else {
		ret = s3c_gpio_cfgpin(GPIO_VTCAM_MCLK, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_VTCAM_MCLK, S3C_GPIO_PULL_DOWN);
	}
	CAM_CHECK_ERR(ret, "cfg mclk");

	/* CAM_SENSOR_A2.8V */
	ret = gpio_direction_output(GPIO_CAM_IO_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_IO_EN");

	/* CAM_ISP_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_core_1.2v");

	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_ISP_CORE_EN");

	gpio_free(GPIO_CAM_IO_EN);
	gpio_free(GPIO_ISP_CORE_EN);
	gpio_free(GPIO_CAM_VT_nRST);
	if (system_rev <= 0x08)
		gpio_free(GPIO_CAM_MCLK);
	else
		gpio_free(GPIO_VTCAM_MCLK);

	return ret;
}

static int s5k6a3_power(int enable)
{
	int ret = 0;

	printk(KERN_ERR "%s %s\n", __func__, enable ? "on" : "down");

	if (enable) {
		ret = s5k6a3_power_on();

		if (unlikely(ret))
			goto error_out;
	} else
		ret = s5k6a3_power_down();

	ret = s3c_csis_power(enable);

error_out:
	return ret;
}

static const char *s5k6a3_get_clk_name(void)
{
	if (system_rev <= 0x08)
		return "sclk_cam0";
	else
		return "sclk_cam1";
}

static struct s3c_platform_camera s5k6a3 = {
	.id		= CAMERA_CSI_D,
	.get_clk_name = s5k6a3_get_clk_name,
	.cam_power	= s5k6a3_power,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_RAW10,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.mipi_lanes	= 1,
	.mipi_settle	= 12,
	.mipi_align	= 24,

	.initialized	= 0,
	.flite_id	= FLITE_IDX_B,
	.use_isp	= true,
	.sensor_index	= 102,
};
#endif
#endif

#if defined(CONFIG_VIDEO_S5C73M3) || defined(CONFIG_VIDEO_SLP_S5C73M3)
static int vddCore = 1150000;
static bool isVddCoreSet;
static void s5c73m3_set_vdd_core(int level)
{
	vddCore = level;
	isVddCoreSet = true;
	printk(KERN_ERR "%s : %d\n", __func__, vddCore);
}

static bool s5c73m3_is_vdd_core_set(void)
{
	return isVddCoreSet;
}

static int s5c73m3_is_isp_reset(void)
{
	int ret = 0;

		ret = gpio_request(GPIO_ISP_RESET, "GPF1");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
		return ret;
	}

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_RESET");
	udelay(10);	/* 200 cycle */
	ret = gpio_direction_output(GPIO_ISP_RESET, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_RESET");
	udelay(10);	/* 200 cycle */

	gpio_free(GPIO_ISP_RESET);

	return ret;
}

static int s5c73m3_gpio_request(void)
{
	int ret = 0;

	ret = gpio_request(GPIO_ISP_STANDBY, "GPM0");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_STANDBY)\n");
		return ret;
	}

	ret = gpio_request(GPIO_ISP_RESET, "GPF1");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
		return ret;
	}

	/* SENSOR_A2.8V */
	ret = gpio_request(GPIO_CAM_IO_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_CAM_IO_EN)\n");
		return ret;
	}

	ret = gpio_request(GPIO_CAM_AF_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_AF_EN)\n");
		return ret;
	}

	ret = gpio_request(GPIO_ISP_CORE_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_ISP_CORE_EN)\n");
		return ret;
	}

	return ret;
}

static int s5c73m3_power_on(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);
	printk(KERN_DEBUG "vddCore : %d\n", vddCore);

	s5c73m3_gpio_request();

	/* CAM_ISP_CORE_1.2V */
	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_CORE_EN");

	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	regulator_set_voltage(regulator, vddCore, vddCore);
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_core_1.2v");

	/* CAM_SENSOR_A2.8V */
	ret = gpio_direction_output(GPIO_CAM_IO_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output IO_EN");

	/* CAM_SENSOR_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_sensor_core_1.2v");
       /* delay is needed : pmu control is slower than gpio control*/
	mdelay(8);

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));
	CAM_CHECK_ERR_RET(ret, "cfg mclk");
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(GPIO_CAM_MCLK, S5P_GPIO_DRVSTR_LV3);

	/* CAM_AF_2.8V */
	ret = gpio_direction_output(GPIO_CAM_AF_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_AF_EN");
	udelay(2000);

#if 0
	/* VT_CORE_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable vt_cam_1.8v");
#endif

	/* CAM_ISP_SENSOR_1.8V */
	regulator = regulator_get(NULL, "cam_isp_sensor_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_sensor_1.8v");

	/* CAM_ISP_MIPI_1.2V */
	regulator = regulator_get(NULL, "cam_isp_mipi_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_mipi_1.2v");
       /* delay is needed : pmu control is slower than gpio control*/
	mdelay(5);

	/* ISP_STANDBY */
	ret = gpio_direction_output(GPIO_ISP_STANDBY, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_STANDBY");
	udelay(100);		/* 2000 cycle */

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_RESET");
	udelay(10);		/* 200 cycle */

	/* EVT0 : set VDD_INT as 1.3V when entering camera */
	/*M0, C1 REV00*/
	if (system_rev == 0x03) {
		regulator = regulator_get(NULL, "vdd_int");
		if (IS_ERR(regulator))
			return -ENODEV;
		regulator_set_voltage(regulator, 1300000, 1300000);
		regulator_put(regulator);
		CAM_CHECK_ERR_RET(ret, "set vdd_int as 1.3V");
	}

	gpio_free(GPIO_ISP_STANDBY);
	gpio_free(GPIO_ISP_RESET);
	gpio_free(GPIO_CAM_IO_EN);
	gpio_free(GPIO_CAM_AF_EN);
	gpio_free(GPIO_ISP_CORE_EN);

	return ret;
}

static int s5c73m3_power_down(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	s5c73m3_gpio_request();

	/* ISP_STANDBY */
	ret = gpio_direction_output(GPIO_ISP_STANDBY, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_STANDBY");
	udelay(2);		/* 40 cycle */

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_RESET");

	/* CAM_AF_2.8V */
	ret = gpio_direction_output(GPIO_CAM_AF_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_AF_EN");

	/* CAM_ISP_MIPI_1.2V */
	regulator = regulator_get(NULL, "cam_isp_mipi_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_mipi_1.2v");
	udelay(10);		/* 200 cycle */

	/* CAM_ISP_SENSOR_1.8V */
	regulator = regulator_get(NULL, "cam_isp_sensor_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_sensor_1.8v");

#if 0
	/* VT_CORE_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable vt_cam_1.8v");
#endif

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_DOWN);
	CAM_CHECK_ERR(ret, "cfg mclk");

	/* CAM_SENSOR_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_sensor_core_1.2v");

	/* CAM_SENSOR_A2.8V */
	ret = gpio_direction_output(GPIO_CAM_IO_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_IO_EN");

	/* CAM_ISP_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_core_1.2v");

	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_ISP_CORE_EN");

	/* EVT0 : set VDD_INT as 1.0V when exiting camera */
	/*M0, C1 REV00*/
	if (system_rev == 0x03) {
		regulator = regulator_get(NULL, "vdd_int");
		if (IS_ERR(regulator))
			return -ENODEV;
		regulator_set_voltage(regulator, 1000000, 1000000);
		regulator_put(regulator);
		CAM_CHECK_ERR_RET(ret, "set vdd_int as 1.0V");
	}

	gpio_free(GPIO_ISP_STANDBY);
	gpio_free(GPIO_ISP_RESET);
	gpio_free(GPIO_CAM_IO_EN);
	gpio_free(GPIO_CAM_AF_EN);
	gpio_free(GPIO_ISP_CORE_EN);

	return ret;
}

static int s5c73m3_power(int enable)
{
	int ret = 0;

	printk(KERN_ERR "%s %s\n", __func__, enable ? "on" : "down");

	if (enable) {
		ret = s5c73m3_power_on();

		if (unlikely(ret))
			goto error_out;
	} else
		ret = s5c73m3_power_down();

	ret = s3c_csis_power(enable);

error_out:
	return ret;
}

static int s5c73m3_get_i2c_busnum(void)
{
	if (system_rev == 0x03) /*M0, M1 REV00*/
		return 18;
	else
		return 0;
}

static const char *s5c73m3_get_clk_name(void)
{
	return "sclk_cam0";
}

static struct s5c73m3_platform_data s5c73m3_plat = {
	.default_width = 640,	/* 1920 */
	.default_height = 480,	/* 1080 */
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
	.set_vdd_core = s5c73m3_set_vdd_core,
	.is_vdd_core_set = s5c73m3_is_vdd_core_set,
	.is_isp_reset = s5c73m3_is_isp_reset,
};

static struct i2c_board_info s5c73m3_i2c_info = {
	I2C_BOARD_INFO("S5C73M3", 0x78 >> 1),
	.platform_data = &s5c73m3_plat,
};

static struct s3c_platform_camera s5c73m3 = {
	.id = CAMERA_CSI_C,
	.get_clk_name = s5c73m3_get_clk_name,
	.get_i2c_busnum = s5c73m3_get_i2c_busnum,
	.cam_power = s5c73m3_power,
	.type = CAM_TYPE_MIPI,
	.fmt = MIPI_CSI_YCBCR422_8BIT,
	.order422 = CAM_ORDER422_8BIT_YCBYCR,
	.info = &s5c73m3_i2c_info,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.srclk_name = "xusbxti",	/* "mout_mpll" */
	.clk_rate = 24000000,	/* 48000000 */
	.line_length = 1920,
	.width = 640,
	.height = 480,
	.window = {
		.left = 0,
		.top = 0,
		.width = 640,
		.height = 480,
	},

	.mipi_lanes = 4,
	.mipi_settle = 12,
	.mipi_align = 32,

	/* Polarity */
	.inv_pclk = 1,
	.inv_vsync = 1,
	.inv_href = 0,
	.inv_hsync = 0,
	.reset_camera = 0,
	.initialized = 0,
};
#endif

#ifdef CONFIG_VIDEO_M5MO
static int m5mo_get_i2c_busnum(void)
{
#ifdef CONFIG_VIDEO_M5MO_USE_SWI2C
	return 25;
#else
	return 0;
#endif
}

static int m5mo_power_on(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	ret = gpio_request(GPIO_CAM_VT_nSTBY, "GPL2");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_VGA_nSTBY)\n");
		return ret;
	}
	ret = gpio_request(GPIO_CAM_VT_nRST, "GPL2");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_VGA_nRST)\n");
		return ret;
	}
	ret = gpio_request(GPIO_ISP_CORE_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(CAM_SENSOR_CORE)\n");
		return ret;
	}
	ret = gpio_request(GPIO_ISP_RESET, "GPY3");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
		return ret;
	}
	/* CAM_VT_nSTBY low */
	ret = gpio_direction_output(GPIO_CAM_VT_nSTBY, 0);
	CAM_CHECK_ERR_RET(ret, "output VT_nSTBY");

	/* CAM_VT_nRST	low */
	gpio_direction_output(GPIO_CAM_VT_nRST, 0);
	CAM_CHECK_ERR_RET(ret, "output VT_nRST");
	udelay(10);

	/* CAM_ISP_CORE_1.2V */
	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_CORE_EN");

	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_core_1.2v");
	udelay(10);
	/* CAM_SENSOR_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator)) {
		CAM_CHECK_ERR_RET(ret, "output Err cam_sensor_core_1.2v");
		return -ENODEV;
	}
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_sensor_core_1.2v");
	udelay(10);

	/* CAM_SENSOR_A2.8V */
	regulator = regulator_get(NULL, "cam_sensor_a2.8v");
	if (IS_ERR(regulator)) {
		CAM_CHECK_ERR_RET(ret, "output Err cam_sensor_a2.8v");
		return -ENODEV;
	}
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_sensor_a2.8v");
	/* it takes about 100us at least during level transition.*/
	udelay(160); /* 130us -> 160us */
	/* VT_CAM_DVDD_1.8V */
	regulator = regulator_get(NULL, "vt_cam_dvdd_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable vt_cam_dvdd_1.8v");
	udelay(10);

	/* CAM_AF_2.8V */
	regulator = regulator_get(NULL, "cam_af_2.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "output cam_af_2.8v");
	mdelay(7);

	/* VT_CAM_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator)) {
		CAM_CHECK_ERR_RET(ret, "output Err vt_cam_1.8v");
		return -ENODEV;
	}
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable vt_cam_1.8v");
	udelay(20);

	/* CAM_ISP_1.8V */
	regulator = regulator_get(NULL, "cam_isp_1.8v");
	if (IS_ERR(regulator)) {
		CAM_CHECK_ERR_RET(ret, "output Err cam_isp_1.8v");
		return -ENODEV;
	}
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_1.8v");
	udelay(120); /* at least */

	/* CAM_ISP_SEN_IO_1.8V */
	regulator = regulator_get(NULL, "cam_isp_sensor_1.8v");
	if (IS_ERR(regulator)) {
		CAM_CHECK_ERR_RET(ret, "output Err cam_isp_sensor_1.8v");
		return -ENODEV;
	}
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_sensor_1.8v");
	udelay(30);

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));
	CAM_CHECK_ERR_RET(ret, "cfg mclk");
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_NONE);
	udelay(70);
	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 1);
	CAM_CHECK_ERR_RET(ret, "output reset");
	mdelay(4);

	gpio_free(GPIO_CAM_VT_nSTBY);
	gpio_free(GPIO_CAM_VT_nRST);
	gpio_free(GPIO_ISP_CORE_EN);
	gpio_free(GPIO_ISP_RESET);

	return ret;
}

static int m5mo_power_down(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	ret = gpio_request(GPIO_CAM_VT_nSTBY, "GPL2");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_VGA_nSTBY)\n");
		return ret;
	}
	ret = gpio_request(GPIO_CAM_VT_nRST, "GPL2");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_VGA_nRST)\n");
		return ret;
	}
	ret = gpio_request(GPIO_ISP_CORE_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(CAM_SENSOR_CORE)\n");
		return ret;
	}
	ret = gpio_request(GPIO_ISP_RESET, "GPY3");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
		return ret;
	}

	/* s3c_i2c0_force_stop(); */

	mdelay(3);

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 0);
	CAM_CHECK_ERR(ret, "output reset");
	mdelay(2);

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_DOWN);
	CAM_CHECK_ERR(ret, "cfg mclk");
	udelay(20);

	/* CAM_AF_2.8V */
	regulator = regulator_get(NULL, "cam_af_2.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_af_2.8v");

	/* CAM_ISP_SEN_IO_1.8V */
	regulator = regulator_get(NULL, "cam_isp_sensor_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable, cam_isp_sensor_1.8v");
	udelay(10);

	/* CAM_ISP_1.8V */
	regulator = regulator_get(NULL, "cam_isp_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_1.8v");
	udelay(500); /* 100us -> 500us */

	/* VT_CAM_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable vt_cam_1.8v");
	udelay(250); /* 10us -> 250us */

	/* VT_CAM_DVDD_1.8V */
	regulator = regulator_get(NULL, "vt_cam_dvdd_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable vt_cam_dvdd_1.8v");
	udelay(300); /*10 -> 300 us */

	/* CAM_SENSOR_A2.8V */
	regulator = regulator_get(NULL, "cam_sensor_a2.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_sensor_a2.8v");
	udelay(800);

	/* CAM_SENSOR_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_sensor_core_1.2v");
	udelay(5);

	/* CAM_ISP_CORE_1.2V */
	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 0);
	CAM_CHECK_ERR(ret, "output ISP_CORE");

	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "disable cam_isp_core_1.2v");

	gpio_free(GPIO_CAM_VT_nSTBY);
	gpio_free(GPIO_CAM_VT_nRST);
	gpio_free(GPIO_ISP_CORE_EN);
	gpio_free(GPIO_ISP_RESET);

	return ret;
}

static int m5mo_flash_power(int enable)
{
/* TODO */
	return 0;
}

static int m5mo_power(int enable)
{
	int ret = 0;

	printk(KERN_ERR "%s %s\n", __func__, enable ? "on" : "down");
	if (enable) {
		ret = m5mo_power_on();
		if (unlikely(ret))
			goto error_out;
	} else
		ret = m5mo_power_down();

	ret = s3c_csis_power(enable);
	m5mo_flash_power(enable);

error_out:
	return ret;
}

static int m5mo_config_isp_irq(void)
{
	s3c_gpio_cfgpin(GPIO_ISP_INT, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_ISP_INT, S3C_GPIO_PULL_NONE);
	return 0;
}

static const char *m5mo_get_clk_name(void)
{
	return "sclk_cam0";
}

static struct m5mo_platform_data m5mo_plat = {
	.default_width = 640, /* 1920 */
	.default_height = 480, /* 1080 */
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
	.config_isp_irq = m5mo_config_isp_irq,
	.irq = IRQ_EINT(24),
};

static struct i2c_board_info  m5mo_i2c_info = {
	I2C_BOARD_INFO("M5MO", 0x1F),
	.platform_data = &m5mo_plat,
};

static struct s3c_platform_camera m5mo = {
	.id		= CAMERA_CSI_C,
	.get_clk_name	= m5mo_get_clk_name,
	.get_i2c_busnum	= m5mo_get_i2c_busnum,
	.cam_power	= m5mo_power, /*smdkv310_mipi_cam0_reset,*/
	.type		= CAM_TYPE_MIPI,
	.fmt		= ITU_601_YCBCR422_8BIT, /*MIPI_CSI_YCBCR422_8BIT*/
	.order422	= CAM_ORDER422_8BIT_CBYCRY,

	.info		= &m5mo_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti", /* "mout_mpll" */
	.clk_rate	= 24000000, /* 48000000 */
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
	.reset_camera	= 0,
	.initialized	= 0,
};
#endif /* #ifdef CONFIG_VIDEO_M5MO */

#ifdef CONFIG_VIDEO_SR200PC20M
static int sr200pc20m_get_i2c_busnum(void)
{
		return 13;
}

static int sr200pc20m_power_on(void)
{
	struct regulator *regulator;
	int ret = 0;

	ret = gpio_request(GPIO_CAM_VT_nSTBY, "GPL2");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_VGA_nSTBY)\n");
		return ret;
	}
	ret = gpio_request(GPIO_CAM_VT_nRST, "GPL2");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_VGA_nRST)\n");
		return ret;
	}
	ret = gpio_request(GPIO_ISP_CORE_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(CAM_SENSOR_CORE)\n");
		return ret;
	}
	ret = gpio_request(GPIO_ISP_RESET, "GPY3");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
		return ret;
	}

	/* CAM_VT_nSTBY low */
	ret = gpio_direction_output(GPIO_CAM_VT_nSTBY, 0);
	CAM_CHECK_ERR_RET(ret, "output VT_nSTBY");

	/* CAM_VT_nRST	low */
	gpio_direction_output(GPIO_CAM_VT_nRST, 0);
	CAM_CHECK_ERR_RET(ret, "output VT_nRST");

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 0);
	CAM_CHECK_ERR(ret, "output reset");

	/* CAM_ISP_CORE_1.2V */
	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_CORE_EN");

	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_core_1.2v");
	/* No delay */

	/* CAM_SENSOR_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator)) {
		CAM_CHECK_ERR_RET(ret, "output Err cam_sensor_core_1.2v");
		return -ENODEV;
	}
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_sensor_core_1.2v");
	udelay(10);

	/* CAM_SENSOR_A2.8V */
	regulator = regulator_get(NULL, "cam_sensor_a2.8v");
	if (IS_ERR(regulator)) {
		CAM_CHECK_ERR_RET(ret, "output Err cam_sensor_a2.8v");
		return -ENODEV;
	}
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_sensor_a2.8v");
	/* it takes about 100us at least during level transition.*/
	udelay(160); /* 130us -> 160us */

	/* VT_CAM_DVDD_1.8V */
	regulator = regulator_get(NULL, "vt_cam_dvdd_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable vt_cam_dvdd_1.8v");
	udelay(10);

	/* VT_CAM_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator)) {
		CAM_CHECK_ERR_RET(ret, "output Err vt_cam_1.8v");
		return -ENODEV;
	}
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable vt_cam_1.8v");
	udelay(20);

	/* CAM_VT_nSTBY high */
	ret = gpio_direction_output(GPIO_CAM_VT_nSTBY, 1);
	CAM_CHECK_ERR_RET(ret, "output VT_nSTBY");
	mdelay(2);

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));
	CAM_CHECK_ERR_RET(ret, "cfg mclk");
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_NONE);
	mdelay(30);

	/* CAM_VT_nRST	high */
	gpio_direction_output(GPIO_CAM_VT_nRST, 1);
	CAM_CHECK_ERR_RET(ret, "output VT_nRST");

	gpio_free(GPIO_CAM_VT_nSTBY);
	gpio_free(GPIO_CAM_VT_nRST);
	gpio_free(GPIO_ISP_CORE_EN);
	gpio_free(GPIO_ISP_RESET);

	return ret;
}

static int sr200pc20m_power_off(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s in\n", __func__);

	ret = gpio_request(GPIO_CAM_VT_nSTBY, "GPL2");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_VGA_nSTBY)\n");
		return ret;
	}
	ret = gpio_request(GPIO_CAM_VT_nRST, "GPL2");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_VGA_nRST)\n");
		return ret;
	}
	ret = gpio_request(GPIO_ISP_CORE_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(CAM_SENSOR_CORE)\n");
		return ret;
	}
	ret = gpio_request(GPIO_ISP_RESET, "GPY3");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
		return ret;
	}

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 0);
	CAM_CHECK_ERR(ret, "output reset");

	/* CAM_VT_nRST low */
	gpio_direction_output(GPIO_CAM_VT_nRST, 0);
	CAM_CHECK_ERR_RET(ret, "output VT_nRST");

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_DOWN);
	CAM_CHECK_ERR(ret, "cfg mclk");
	udelay(20);

	/* CAM_VT_nSTBY low */
	ret = gpio_direction_output(GPIO_CAM_VT_nSTBY, 0);
	CAM_CHECK_ERR_RET(ret, "output VT_nSTBY");
	mdelay(2);

	/* CAM_SENSOR_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_sensor_core_1.2v");
	udelay(5);

	/* CAM_ISP_CORE_1.2V */
	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 0);
	CAM_CHECK_ERR(ret, "output ISP_CORE");

	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "disable cam_isp_core_1.2v");

	/* CAM_SENSOR_A2.8V */
	regulator = regulator_get(NULL, "cam_sensor_a2.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_sensor_a2.8v");
	udelay(800);

	/* VT_CAM_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable vt_cam_1.8v");
	udelay(250); /* 10us -> 250us */

	/* VT_CAM_DVDD_1.8V */
	regulator = regulator_get(NULL, "vt_cam_dvdd_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable vt_cam_dvdd_1.8v");
	udelay(300); /*10 -> 300 us */

	gpio_free(GPIO_CAM_VT_nSTBY);
	gpio_free(GPIO_CAM_VT_nRST);
	gpio_free(GPIO_ISP_CORE_EN);
	gpio_free(GPIO_ISP_RESET);

	return ret;
}

static int sr200pc20m_power(int onoff)
{
	int ret = 0;

	printk(KERN_DEBUG "%s(): %s\n", __func__, onoff ? "on" : "down");

	if (onoff) {
		ret = sr200pc20m_power_on();
		if (unlikely(ret))
			goto error_out;
	} else {
		ret = sr200pc20m_power_off();
	}

	ret = s3c_csis_power(onoff);

error_out:
	return ret;
}

static const char *sr200pc20m_get_clk_name(void)
{
	return "sclk_cam0";
}

static struct sr200pc20m_platform_data sr200pc20m_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
	.streamoff_delay = 0,
	.dbg_level = CAMDBG_LEVEL_DEFAULT,
};

static struct i2c_board_info  sr200pc20m_i2c_info = {
	I2C_BOARD_INFO("SR200PC20M", 0x40 >> 1),
	.platform_data = &sr200pc20m_plat,
};

static struct s3c_platform_camera sr200pc20m = {
	.id		= CAMERA_CSI_D,
	.get_clk_name	= sr200pc20m_get_clk_name,
	.type		= CAM_TYPE_MIPI,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.get_i2c_busnum	= sr200pc20m_get_i2c_busnum,
	.info		= &sr200pc20m_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.line_length	= 640,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 1,
	.mipi_settle = 6,
	.mipi_align	= 32,

	.inv_pclk	= 0,
	.inv_vsync	= 0,
	.inv_href	= 0,
	.inv_hsync	= 0,
	.reset_camera	= 0,
	.initialized	= 0,
	.cam_power	= sr200pc20m_power,
};
#endif /* CONFIG_VIDEO_SR200PC20M */

/* Interface setting */
static struct s3c_platform_fimc fimc_plat = {
	.default_cam = CAMERA_CSI_D,
#ifdef WRITEBACK_ENABLED
	.default_cam = CAMERA_WB,
#endif
	.camera = {
#if defined(CONFIG_VIDEO_S5C73M3) || defined(CONFIG_VIDEO_SLP_S5C73M3)
		&s5c73m3,
#endif
#ifdef CONFIG_VIDEO_S5K6A3
		&s5k6a3,
#endif
#if defined(CONFIG_VIDEO_M5MO)
		&m5mo,
#endif
#if defined(CONFIG_VIDEO_SR200PC20M)
		&sr200pc20m,
#endif
#ifdef WRITEBACK_ENABLED
		&writeback,
#endif
	},
	.hw_ver		= 0x51,
};

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
static void __set_flite_camera_config(struct exynos_platform_flite *data,
			u32 active_index, u32 max_cam)
{
	data->active_cam_index = active_index;
	data->num_clients = max_cam;
}

static void __init smdk4x12_set_camera_flite_platdata(void)
{
	int flite0_cam_index = 0;
	int flite1_cam_index = 0;
#ifdef CONFIG_VIDEO_S5K6A3
	exynos_flite1_default_data.cam[flite1_cam_index++] = &s5k6a3;
#endif
#ifdef CONFIG_VIDEO_SR200PC20M
	exynos_flite1_default_data.cam[flite1_cam_index++] = &sr200pc20m;
#endif
	__set_flite_camera_config(&exynos_flite0_default_data, 0, flite0_cam_index);
	__set_flite_camera_config(&exynos_flite1_default_data, 0, flite1_cam_index);
}
#endif /* CONFIG_VIDEO_EXYNOS_FIMC_LITE */
#endif /* CONFIG_VIDEO_FIMC */

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
static struct i2c_board_info __initdata test_info = {
	I2C_BOARD_INFO("testinfo", 0x0),
};

static struct s5p_fimc_isp_info isp_info[] = {
	{
		.board_info	= &test_info,
		.bus_type	= FIMC_LCD_WB,
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.flags		= FIMC_CLK_INV_VSYNC,
	},
};

static void __init midas_subdev_config(void)
{
	s3c_fimc0_default_data.isp_info[0] = &isp_info[0];
	s3c_fimc0_default_data.isp_info[0]->use_cam = true;
	s3c_fimc0_default_data.isp_info[1] = &isp_info[1];
	s3c_fimc0_default_data.isp_info[1]->use_cam = false;
	s3c_fimc0_default_data.isp_info[2] = &isp_info[1];
	s3c_fimc0_default_data.isp_info[2]->use_cam = false;
	s3c_fimc0_default_data.isp_info[3] = &isp_info[1];
	s3c_fimc0_default_data.isp_info[3]->use_cam = false;
}
#endif	/* CONFIG_VIDEO_SAMSUNG_S5P_FIMC */

void __init midas_camera_init(void)
{
#ifdef CONFIG_VIDEO_FIMC
	s3c_fimc0_set_platdata(&fimc_plat);
	s3c_fimc1_set_platdata(&fimc_plat);
	s3c_fimc2_set_platdata(NULL);
	s3c_fimc3_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
	secmem.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#ifdef CONFIG_VIDEO_FIMC_MIPI
	s3c_csis0_set_platdata(NULL);
	s3c_csis1_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_csis1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	smdk4x12_set_camera_flite_platdata();
	s3c_set_platdata(&exynos_flite0_default_data,
		sizeof(exynos_flite0_default_data), &exynos_device_flite0);
	s3c_set_platdata(&exynos_flite1_default_data,
		sizeof(exynos_flite1_default_data), &exynos_device_flite1);
#endif
#endif /* CONFIG_VIDEO_FIMC */

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
	midas_subdev_config();

	dev_set_name(&s5p_device_fimc0.dev, "s3c-fimc.0");
	dev_set_name(&s5p_device_fimc1.dev, "s3c-fimc.1");
	dev_set_name(&s5p_device_fimc2.dev, "s3c-fimc.2");
	dev_set_name(&s5p_device_fimc3.dev, "s3c-fimc.3");

	clk_add_alias("fimc", "exynos4210-fimc.0", "fimc",
			&s5p_device_fimc0.dev);
	clk_add_alias("fimc", "exynos4210-fimc.1", "fimc",
			&s5p_device_fimc1.dev);
	clk_add_alias("fimc", "exynos4210-fimc.2", "fimc",
			&s5p_device_fimc2.dev);
	clk_add_alias("fimc", "exynos4210-fimc.3", "fimc",
			&s5p_device_fimc3.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.0", "sclk_fimc",
			&s5p_device_fimc0.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.1", "sclk_fimc",
			&s5p_device_fimc1.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.2", "sclk_fimc",
			&s5p_device_fimc2.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.3", "sclk_fimc",
			&s5p_device_fimc3.dev);

	s3c_fimc_setname(0, "exynos4210-fimc");
	s3c_fimc_setname(1, "exynos4210-fimc");
	s3c_fimc_setname(2, "exynos4210-fimc");
	s3c_fimc_setname(3, "exynos4210-fimc");

	s3c_set_platdata(&s3c_fimc0_default_data,
			 sizeof(s3c_fimc0_default_data), &s5p_device_fimc0);
	s3c_set_platdata(&s3c_fimc1_default_data,
			 sizeof(s3c_fimc1_default_data), &s5p_device_fimc1);
	s3c_set_platdata(&s3c_fimc2_default_data,
			 sizeof(s3c_fimc2_default_data), &s5p_device_fimc2);
	s3c_set_platdata(&s3c_fimc3_default_data,
			 sizeof(s3c_fimc3_default_data), &s5p_device_fimc3);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif /* CONFIG_VIDEO_S5P_FIMC */
}
