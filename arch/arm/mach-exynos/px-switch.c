#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>
#include <mach/usb_switch.h>

struct device *sec_switch_dev;

enum usb_path_t current_path = USB_PATH_NONE;

static struct semaphore usb_switch_sem;

static ssize_t show_usb_sel(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	const char *mode;

	if (current_path & USB_PATH_CP) {
		/* CP */
		mode = "MODEM";
	} else {
		/* AP */
		mode = "PDA";
	}

	pr_err("%s: %s\n", __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t store_usb_sel(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	pr_err("%s: %s\n", __func__, buf);

	if (!strncasecmp(buf, "PDA", 3)) {
		usb_switch_clr_path(USB_PATH_CP);
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		usb_switch_set_path(USB_PATH_CP);
	} else {
		pr_err("%s: wrong usb_sel value(%s)!!\n", __func__, buf);
		return -EINVAL;
	}

	return count;
}

static ssize_t show_uart_sel(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int val_sel;
	const char *mode;

	val_sel = gpio_get_value(GPIO_UART_SEL);

	if (val_sel == 0) {
		/* CP */
		mode = "CP";
	} else {
		/* AP */
		mode = "AP";
	}

	pr_err("%s: %s\n", __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t store_uart_sel(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int uart_sel = -1;

	pr_err("%s: %s\n", __func__, buf);

	if (!strncasecmp(buf, "AP", 2)) {
		uart_sel = 1;
	} else if (!strncasecmp(buf, "CP", 2)) {
		uart_sel = 0;
	} else {
		pr_err("%s: wrong usb_sel value(%s)!!\n", __func__, buf);
		return -EINVAL;
	}

	/* 1 for AP, 0 for CP */
	if (uart_sel == 1)
		gpio_set_value(GPIO_UART_SEL, 1);
	else if (uart_sel == 0)
		gpio_set_value(GPIO_UART_SEL, 0);

	return count;
}

static DEVICE_ATTR(usb_sel, 0664, show_usb_sel, store_usb_sel);
static DEVICE_ATTR(uart_sel, 0664, show_uart_sel, store_uart_sel);

static void pmic_safeout2(int onoff)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, "safeout2");
	BUG_ON(IS_ERR_OR_NULL(regulator));

	if (onoff) {
		if (!regulator_is_enabled(regulator)) {
			regulator_enable(regulator);
		} else {
			pr_err("%s: onoff:%d No change in safeout2\n",
			       __func__, onoff);
		}
	} else {
		if (regulator_is_enabled(regulator)) {
			regulator_force_disable(regulator);
		} else {
			pr_err("%s: onoff:%d No change in safeout2\n",
			       __func__, onoff);
		}
	}

	regulator_put(regulator);
}

static void usb_apply_path(enum usb_path_t path)
{
	pr_err("%s: current gpio before changing : sel1:%d sel2:%d sel3:%d\n",
	       __func__, gpio_get_value(GPIO_USB_SEL1),
	       gpio_get_value(GPIO_USB_SEL2), gpio_get_value(GPIO_USB_SEL3));
	pr_err("%s: target path %x\n", __func__, path);

	/* following checks are ordered according to priority */
	if (path & USB_PATH_ADCCHECK) {
		gpio_set_value(GPIO_USB_SEL1, 0);
		gpio_set_value(GPIO_USB_SEL2, 1);
		/* don't care SEL3 */
		goto out_nochange;
	}
	if (path & USB_PATH_TA) {
		gpio_set_value(GPIO_USB_SEL1, 1);
		gpio_set_value(GPIO_USB_SEL2, 0);
		/* don't care SEL3 */
		goto out_ap;
	}
	if (path & USB_PATH_CP) {
		pr_err("DEBUG: set USB path to CP\n");
		gpio_set_value(GPIO_USB_SEL1, 0);
		gpio_set_value(GPIO_USB_SEL2, 0);
		/* don't care SEL3 */
		mdelay(3);
		goto out_cp;
	}
	if (path & USB_PATH_OTG) {
		gpio_set_value(GPIO_USB_SEL1, 1);
		/* don't care SEL2 */
		gpio_set_value(GPIO_USB_SEL3, 1);
		goto out_ap;
	}
	if (path & USB_PATH_HOST) {
		/* don't care SEL2 */
		gpio_set_value(GPIO_USB_SEL3, 0);
		goto out_ap;
	}

	/* default */
	gpio_set_value(GPIO_USB_SEL1, 1);
	gpio_set_value(GPIO_USB_SEL2, 0);
	gpio_set_value(GPIO_USB_SEL3, 1);

out_ap:
	pr_err("%s: %x safeout2 off\n", __func__, path);
	pmic_safeout2(0);
	goto sysfs_noti;

out_cp:
	pr_err("%s: %x safeout2 on\n", __func__, path);
	pmic_safeout2(1);
	goto sysfs_noti;

out_nochange:
	pr_err("%s: %x safeout2 no change\n", __func__, path);
	return;

sysfs_noti:
	sysfs_notify(&sec_switch_dev->kobj, NULL, "usb_sel");
	return;
}

/*
  Typical usage of usb switch:

  usb_switch_lock();  (alternatively from hard/soft irq context)
  ( or usb_switch_trylock() )
  ...
  usb_switch_set_path(USB_PATH_ADCCHECK);
  ...
  usb_switch_set_path(USB_PATH_TA);
  ...
  usb_switch_unlock(); (this restores previous usb switch settings)
*/
void usb_switch_set_path(enum usb_path_t path)
{
	pr_err("%s: %x current_path before changing\n", __func__, current_path);
	current_path |= path;
	usb_apply_path(current_path);
}

void usb_switch_clr_path(enum usb_path_t path)
{
	pr_err("%s: %x current_path before changing\n", __func__, current_path);
	current_path &= ~path;
	usb_apply_path(current_path);
}

int usb_switch_lock(void)
{
	return down_interruptible(&usb_switch_sem);
}

int usb_switch_trylock(void)
{
	return down_trylock(&usb_switch_sem);
}

void usb_switch_unlock(void)
{
	up(&usb_switch_sem);
}

static int __init usb_switch_init(void)
{
	int ret;

	gpio_request(GPIO_USB_SEL1, "GPIO_USB_SEL1");
	gpio_request(GPIO_USB_SEL2, "GPIO_USB_SEL2");
	gpio_request(GPIO_USB_SEL3, "GPIO_USB_SEL3");
	gpio_request(GPIO_UART_SEL, "GPIO_UART_SEL");

	gpio_export(GPIO_USB_SEL1, 1);
	gpio_export(GPIO_USB_SEL2, 1);
	gpio_export(GPIO_USB_SEL3, 1);
	gpio_export(GPIO_UART_SEL, 1);

	BUG_ON(!sec_class);
	sec_switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	BUG_ON(!sec_switch_dev);
	gpio_export_link(sec_switch_dev, "GPIO_USB_SEL1", GPIO_USB_SEL1);
	gpio_export_link(sec_switch_dev, "GPIO_USB_SEL2", GPIO_USB_SEL2);
	gpio_export_link(sec_switch_dev, "GPIO_USB_SEL3", GPIO_USB_SEL3);
	gpio_export_link(sec_switch_dev, "GPIO_UART_SEL", GPIO_UART_SEL);

	/*init_MUTEX(&usb_switch_sem);*/
	sema_init(&usb_switch_sem, 1);

	if (!gpio_get_value(GPIO_USB_SEL1))
		usb_switch_set_path(USB_PATH_CP);

	ret = device_create_file(sec_switch_dev, &dev_attr_usb_sel);
	BUG_ON(ret);

	ret = device_create_file(sec_switch_dev, &dev_attr_uart_sel);
	BUG_ON(ret);

	return 0;
}

device_initcall(usb_switch_init);
