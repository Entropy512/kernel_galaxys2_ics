#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/sensor/sensors_core.h>
#include <linux/sensor/ak8975.h>
#include <linux/sensor/k3dh.h>
#include <linux/sensor/gp2a.h>
#include <linux/sensor/lsm330dlc_accel.h>
#include <linux/sensor/lsm330dlc_gyro.h>
#include <linux/sensor/lps331ap.h>
#include <linux/sensor/cm36651.h>
#include <linux/sensor/cm3663.h>
#include <linux/sensor/bh1721.h>

#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include "midas.h"

static int accel_get_position(void);

static struct accel_platform_data accel_pdata = {
	.accel_get_position = accel_get_position,
};

static struct i2c_board_info i2c_devs1[] __initdata = {
#ifdef CONFIG_SENSORS_LSM330DLC
	{
		I2C_BOARD_INFO("lsm330dlc_accel", (0x32 >> 1)),
		.platform_data = &accel_pdata,
	},
	{
		I2C_BOARD_INFO("lsm330dlc_gyro", (0xD6 >> 1)),
	},
#elif defined(CONFIG_SENSORS_K3DH)
	{
		I2C_BOARD_INFO("k3dh", 0x19),
		.platform_data	= &accel_pdata,
	},
#endif
};

static int accel_get_position(void)
{
	int position = 0;

#if defined(CONFIG_MACH_C1) && !defined(CONFIG_MACH_C1_KOR_SKT)
	if (system_rev == 3 || system_rev == 0)
		position = 7; /* bottom/lower-left */
	else if (system_rev == 2)
		position = 3; /* top/lower-left */
	else
		position = 2; /* top/lower-right */
#elif defined(CONFIG_MACH_C1VZW)
	if (system_rev == 1)
		position = 3; /* top/lower-left */
	else
		position = 2; /* top/lower-right */
#elif defined(CONFIG_MACH_C1CTC)
		position = 2; /* top/lower-right */
#elif defined(CONFIG_MACH_M0) && !defined(CONFIG_MACH_M0_CMCC)
	if (system_rev == 3 || system_rev == 0)
		position = 6; /* bottom/lower-right */
	else if (system_rev == 1 || system_rev == 2\
		|| system_rev == 4 || system_rev == 5)
		position = 0; /* top/upper-left */
	else
		position = 2; /* top/lower-right */
#elif defined(CONFIG_MACH_M0_CMCC)
	position = 0; /* top/upper-left  */
#elif defined(CONFIG_MACH_C1_KOR_SKT)
	if (system_rev == 2)
		position = 3; /* top/lower-left */
	else
		position = 2; /* top/lower-right */
#elif defined(CONFIG_MACH_S2PLUS)
	position = 3; /* top/lower-left */
#elif defined(CONFIG_MACH_P4NOTE)
	position = 0; /* top/upper-left */
#else /* Common */
	position = 2; /* top/lower-right */
#endif
	return position;
}

#if defined(CONFIG_SENSORS_LSM330DLC) || \
	defined(CONFIG_SENSORS_K3DH)
static void accel_gpio_init(void)
{
	int ret = gpio_request(GPIO_ACC_INT, "accelerometer_irq");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio lsm330dlc_accel_irq\n");

	/* Accelerometer sensor interrupt pin initialization */
	s3c_gpio_cfgpin(GPIO_ACC_INT, S3C_GPIO_INPUT);
	gpio_set_value(GPIO_ACC_INT, 2);
	s3c_gpio_setpull(GPIO_ACC_INT, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(GPIO_ACC_INT, S5P_GPIO_DRVSTR_LV1);
	i2c_devs1[0].irq = gpio_to_irq(GPIO_ACC_INT);
}
#endif

#ifdef CONFIG_SENSORS_LSM330DLC
static void gyro_gpio_init(void)
{
	int ret = gpio_request(GPIO_GYRO_INT, "lsm330dlc_gyro_irq");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio lsm330dlc_gyro_irq\n");

	ret = gpio_request(GPIO_GYRO_DE, "lsm330dlc_gyro_data_enable");

	if (ret)
		printk(KERN_ERR
		       "Failed to request gpio lsm330dlc_gyro_data_enable\n");

	/* Gyro sensor interrupt pin initialization */
#if 0
	s5p_register_gpio_interrupt(GPIO_GYRO_INT);
	s3c_gpio_cfgpin(GPIO_GYRO_INT, S3C_GPIO_SFN(0xF));
#else
	s3c_gpio_cfgpin(GPIO_GYRO_INT, S3C_GPIO_INPUT);
#endif
	gpio_set_value(GPIO_GYRO_INT, 2);
	s3c_gpio_setpull(GPIO_GYRO_INT, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_drvstr(GPIO_GYRO_INT, S5P_GPIO_DRVSTR_LV1);
#if 0
	i2c_devs1[1].irq = gpio_to_irq(GPIO_GYRO_INT); /* interrupt */
#else
	i2c_devs1[1].irq = -1; /* polling */
#endif

	/* Gyro sensor data enable pin initialization */
	s3c_gpio_cfgpin(GPIO_GYRO_DE, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_GYRO_DE, 0);
	s3c_gpio_setpull(GPIO_GYRO_DE, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_drvstr(GPIO_GYRO_DE, S5P_GPIO_DRVSTR_LV1);
}
#endif

#if defined(CONFIG_SENSORS_GP2A) || defined(CONFIG_SENSORS_CM36651) || \
	defined(CONFIG_SENSORS_CM3663)
static int proximity_leda_on(bool onoff)
{
	printk(KERN_INFO "%s, onoff = %d\n", __func__, onoff);

	gpio_set_value(GPIO_PS_ALS_EN, onoff);

	return 0;
}

/* Depends window, threshold is needed to be set */
static u8 cm36651_get_threshold(void)
{
	u8 threshold = 17;

	/* Add model config and threshold here. */
#if defined(CONFIG_MACH_M0)
	if (system_rev >= 12)
		threshold = 11;
#elif defined(CONFIG_MACH_C1_KOR_SKT) || defined(CONFIG_MACH_C1_KOR_KT) ||\
	defined(CONFIG_MACH_C1_KOR_LGT)
	if (system_rev >= 6)
		threshold = 11;
#endif

	return threshold;
}

static struct cm36651_platform_data cm36651_pdata = {
	.cm36651_led_on = proximity_leda_on,
	.cm36651_get_threshold = cm36651_get_threshold,
	.irq = GPIO_PS_ALS_INT,
};

static struct cm3663_platform_data cm3663_pdata = {
	.proximity_power = proximity_leda_on,
};

static struct i2c_board_info i2c_devs9_emul[] __initdata = {
	{
		I2C_BOARD_INFO("gp2a", (0x72 >> 1)),
	},
	{
		I2C_BOARD_INFO("cm36651", (0x30 >> 1)),
		.platform_data = &cm36651_pdata,
	},
	{
		I2C_BOARD_INFO("cm3663", (0x20)),
		.irq = GPIO_PS_ALS_INT,
		.platform_data = &cm3663_pdata,
	}
};


static struct gp2a_platform_data gp2a_pdata = {
	.gp2a_led_on	= proximity_leda_on,
	.p_out = GPIO_PS_ALS_INT,
};

static struct platform_device opt_gp2a = {
	.name = "gp2a-opt",
	.id = -1,
	.dev = {
		.platform_data = &gp2a_pdata,
	},
};

static void optical_gpio_init(void)
{
	int ret = gpio_request(GPIO_PS_ALS_EN, "optical_power_supply_on");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio optical power supply.\n");

	/* configuring for gp2a gpio for LEDA power */
	s3c_gpio_cfgpin(GPIO_PS_ALS_EN, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_PS_ALS_EN, 0);
	s3c_gpio_setpull(GPIO_PS_ALS_EN, S3C_GPIO_PULL_NONE);
}
#endif

#ifdef CONFIG_SENSORS_BH1721
static struct i2c_board_info i2c_devs9_emul[] __initdata = {
	{
		I2C_BOARD_INFO("bh1721fvc", 0x23),
	},
};
#endif

#ifdef CONFIG_SENSORS_AK8975C
static struct akm8975_platform_data akm8975_pdata = {
	.gpio_data_ready_int = GPIO_MSENSOR_INT,
};

static struct i2c_board_info i2c_devs10_emul[] __initdata = {
	{
		I2C_BOARD_INFO("ak8975", 0x0C),
		.platform_data = &akm8975_pdata,
	},
};

static void ak8975c_gpio_init(void)
{
	int ret = gpio_request(GPIO_MSENSOR_INT, "gpio_akm_int");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio akm_int.\n");

	s5p_register_gpio_interrupt(GPIO_MSENSOR_INT);
	s3c_gpio_setpull(GPIO_MSENSOR_INT, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(GPIO_MSENSOR_INT, S3C_GPIO_SFN(0xF));
	i2c_devs10_emul[0].irq = gpio_to_irq(GPIO_MSENSOR_INT);
}
#endif

#ifdef CONFIG_SENSORS_LPS331
static void lps331_gpio_init(void)
{
	int ret = gpio_request(GPIO_BARO_INT, "lps331_irq");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio lps331_irq\n");

	s3c_gpio_cfgpin(GPIO_BARO_INT, S3C_GPIO_INPUT);
	gpio_set_value(GPIO_BARO_INT, 2);
	s3c_gpio_setpull(GPIO_BARO_INT, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(GPIO_BARO_INT, S5P_GPIO_DRVSTR_LV1);
}

static struct lps331ap_platform_data lps331ap_pdata = {
	.irq =	GPIO_BARO_INT,
};

static struct i2c_board_info i2c_devs11_emul[] __initdata = {
	{
		I2C_BOARD_INFO("lps331ap", 0x5D),
		.platform_data = &lps331ap_pdata,
	},
};
#endif

static int __init midas_sensor_init(void)
{
	int ret = 0;

	/* Gyro & Accelerometer Sensor */
#if defined(CONFIG_SENSORS_LSM330DLC)
	accel_gpio_init();
	gyro_gpio_init();
#elif defined(CONFIG_SENSORS_K3DH)
	accel_gpio_init();
#endif
	i2c_add_devices(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	/* Optical Sensor */
#if defined(CONFIG_SENSORS_GP2A) || defined(CONFIG_SENSORS_CM36651) || \
	defined(CONFIG_SENSORS_CM3663)
	optical_gpio_init();
	i2c_add_devices(9, i2c_devs9_emul, ARRAY_SIZE(i2c_devs9_emul));
#elif defined(CONFIG_SENSORS_BH1721)
	i2c_add_devices(9, i2c_devs9_emul, ARRAY_SIZE(i2c_devs9_emul));
#endif

#if defined(CONFIG_SENSORS_GP2A)
	ret = platform_device_register(&opt_gp2a);
	if (ret < 0)
		printk(KERN_ERR "failed to register opt_gp2a\n");
#endif

	/* Magnetic Sensor */
#ifdef CONFIG_SENSORS_AK8975C
	ak8975c_gpio_init();
	i2c_add_devices(10, i2c_devs10_emul, ARRAY_SIZE(i2c_devs10_emul));
#endif

	/* Pressure Sensor */
#ifdef CONFIG_SENSORS_LPS331
	lps331_gpio_init();
	i2c_add_devices(11, i2c_devs11_emul, ARRAY_SIZE(i2c_devs11_emul));
#endif

	return ret;
}
module_init(midas_sensor_init);
