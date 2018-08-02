/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 * Copyright 2010, 2011 David Jander <david@protonic.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>

#ifdef CONFIG_KEYBOARD_SCKEY
#include <../../../arch/arm/mach-msm/board-8064.h>

#ifdef CONFIG_ANDROID_ENGINEERING
static int shkey_dbg_log = 0;
static int shkey_all_log = 0;
	
module_param( shkey_dbg_log, int , 0600 );
module_param( shkey_all_log, int , 0600 );
	
#define SHKEY_DRV_DBG_LOG(fmt, args...) \
	if( shkey_dbg_log == 1 ){ \
		printk( KERN_DEBUG "[SHKEY][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
	
#define SHKEY_DRV_ALL_LOG(fmt, args...) \
	if( shkey_all_log == 1 ){ \
		printk( KERN_DEBUG "[SHKEY][%d][%s]" fmt "\n", __LINE__, __func__, ## args ); \
	}
#else
#define SHKEY_DRV_DBG_LOG(fmt, args...)
#define SHKEY_DRV_ALL_LOG(fmt, args...)
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif /* CONFIG_KEYBOARD_SCKEY */

struct gpio_button_data {
	const struct gpio_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
	struct work_struct work;
	unsigned int timer_debounce;	/* in msecs */
	unsigned int irq;
	spinlock_t lock;
	bool disabled;
	bool key_pressed;
};

struct gpio_keys_drvdata {
	struct input_dev *input;
	struct mutex disable_lock;
	unsigned int n_buttons;
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	struct gpio_button_data data[0];
};

/*
 * SYSFS interface for enabling/disabling keys and switches:
 *
 * There are 4 attributes under /sys/devices/platform/gpio-keys/
 *	keys [ro]              - bitmap of keys (EV_KEY) which can be
 *	                         disabled
 *	switches [ro]          - bitmap of switches (EV_SW) which can be
 *	                         disabled
 *	disabled_keys [rw]     - bitmap of keys currently disabled
 *	disabled_switches [rw] - bitmap of switches currently disabled
 *
 * Userland can change these values and hence disable event generation
 * for each key (or switch). Disabling a key means its interrupt line
 * is disabled.
 *
 * For example, if we have following switches set up as gpio-keys:
 *	SW_DOCK = 5
 *	SW_CAMERA_LENS_COVER = 9
 *	SW_KEYPAD_SLIDE = 10
 *	SW_FRONT_PROXIMITY = 11
 * This is read from switches:
 *	11-9,5
 * Next we want to disable proximity (11) and dock (5), we write:
 *	11,5
 * to file disabled_switches. Now proximity and dock IRQs are disabled.
 * This can be verified by reading the file disabled_switches:
 *	11,5
 * If we now want to enable proximity (11) switch we write:
 *	5
 * to disabled_switches.
 *
 * We can disable only those keys which don't allow sharing the irq.
 */

/**
 * get_n_events_by_type() - returns maximum number of events per @type
 * @type: type of button (%EV_KEY, %EV_SW)
 *
 * Return value of this function can be used to allocate bitmap
 * large enough to hold all bits for given type.
 */
static inline int get_n_events_by_type(int type)
{
	SHKEY_DRV_ALL_LOG("is called");

	BUG_ON(type != EV_SW && type != EV_KEY);

	SHKEY_DRV_ALL_LOG("is end ");

	return (type == EV_KEY) ? KEY_CNT : SW_CNT;
}

/**
 * gpio_keys_disable_button() - disables given GPIO button
 * @bdata: button data for button to be disabled
 *
 * Disables button pointed by @bdata. This is done by masking
 * IRQ line. After this function is called, button won't generate
 * input events anymore. Note that one can only disable buttons
 * that don't share IRQs.
 *
 * Make sure that @bdata->disable_lock is locked when entering
 * this function to avoid races when concurrent threads are
 * disabling buttons at the same time.
 */
static void gpio_keys_disable_button(struct gpio_button_data *bdata)
{
	SHKEY_DRV_ALL_LOG("is called");

	if (!bdata->disabled) {
		/*
		 * Disable IRQ and possible debouncing timer.
		 */
		disable_irq(bdata->irq);
		if (bdata->timer_debounce)
			del_timer_sync(&bdata->timer);

		bdata->disabled = true;
	}
	SHKEY_DRV_ALL_LOG("is end");
}

/**
 * gpio_keys_enable_button() - enables given GPIO button
 * @bdata: button data for button to be disabled
 *
 * Enables given button pointed by @bdata.
 *
 * Make sure that @bdata->disable_lock is locked when entering
 * this function to avoid races with concurrent threads trying
 * to enable the same button at the same time.
 */
static void gpio_keys_enable_button(struct gpio_button_data *bdata)
{
	SHKEY_DRV_ALL_LOG("is called");

	if (bdata->disabled) {
		enable_irq(bdata->irq);
		bdata->disabled = false;
	}
	SHKEY_DRV_ALL_LOG("is end");
}

/**
 * gpio_keys_attr_show_helper() - fill in stringified bitmap of buttons
 * @ddata: pointer to drvdata
 * @buf: buffer where stringified bitmap is written
 * @type: button type (%EV_KEY, %EV_SW)
 * @only_disabled: does caller want only those buttons that are
 *                 currently disabled or all buttons that can be
 *                 disabled
 *
 * This function writes buttons that can be disabled to @buf. If
 * @only_disabled is true, then @buf contains only those buttons
 * that are currently disabled. Returns 0 on success or negative
 * errno on failure.
 */
static ssize_t gpio_keys_attr_show_helper(struct gpio_keys_drvdata *ddata,
					  char *buf, unsigned int type,
					  bool only_disabled)
{
	int n_events = get_n_events_by_type(type);
	unsigned long *bits;
	ssize_t ret;
	int i;

	SHKEY_DRV_ALL_LOG("is called");

	bits = kcalloc(BITS_TO_LONGS(n_events), sizeof(*bits), GFP_KERNEL);
	if (!bits)
	{
		SHKEY_DRV_ALL_LOG("return = -ENOMEM");
		return -ENOMEM;
	}

	for (i = 0; i < ddata->n_buttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (only_disabled && !bdata->disabled)
			continue;

		__set_bit(bdata->button->code, bits);
	}

	ret = bitmap_scnlistprintf(buf, PAGE_SIZE - 2, bits, n_events);
	buf[ret++] = '\n';
	buf[ret] = '\0';

	kfree(bits);

	SHKEY_DRV_ALL_LOG("is end");

	return ret;
}

/**
 * gpio_keys_attr_store_helper() - enable/disable buttons based on given bitmap
 * @ddata: pointer to drvdata
 * @buf: buffer from userspace that contains stringified bitmap
 * @type: button type (%EV_KEY, %EV_SW)
 *
 * This function parses stringified bitmap from @buf and disables/enables
 * GPIO buttons accordingly. Returns 0 on success and negative error
 * on failure.
 */
static ssize_t gpio_keys_attr_store_helper(struct gpio_keys_drvdata *ddata,
					   const char *buf, unsigned int type)
{
	int n_events = get_n_events_by_type(type);
	unsigned long *bits;
	ssize_t error;
	int i;

	SHKEY_DRV_ALL_LOG("is called");

	bits = kcalloc(BITS_TO_LONGS(n_events), sizeof(*bits), GFP_KERNEL);
	if (!bits)
	{
		SHKEY_DRV_ALL_LOG("return -ENOMEM");
		return -ENOMEM;
	}

	error = bitmap_parselist(buf, bits, n_events);
	if (error)
		goto out;

	/* First validate */
	for (i = 0; i < ddata->n_buttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (test_bit(bdata->button->code, bits) &&
		    !bdata->button->can_disable) {
			error = -EINVAL;
			goto out;
		}
	}

	SHKEY_DRV_ALL_LOG("mutex_lock() is called");
	mutex_lock(&ddata->disable_lock);

	for (i = 0; i < ddata->n_buttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];

		if (bdata->button->type != type)
			continue;

		if (test_bit(bdata->button->code, bits))
			gpio_keys_disable_button(bdata);
		else
			gpio_keys_enable_button(bdata);
	}

	mutex_unlock(&ddata->disable_lock);
	SHKEY_DRV_ALL_LOG("mutex_unlock() was called");

	SHKEY_DRV_ALL_LOG("is end");

out:
	kfree(bits);
	SHKEY_DRV_ALL_LOG("return error");

	return error;
}

#define ATTR_SHOW_FN(name, type, only_disabled)				\
static ssize_t gpio_keys_show_##name(struct device *dev,		\
				     struct device_attribute *attr,	\
				     char *buf)				\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);	\
									\
	return gpio_keys_attr_show_helper(ddata, buf,			\
					  type, only_disabled);		\
}

ATTR_SHOW_FN(keys, EV_KEY, false);
ATTR_SHOW_FN(switches, EV_SW, false);
ATTR_SHOW_FN(disabled_keys, EV_KEY, true);
ATTR_SHOW_FN(disabled_switches, EV_SW, true);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/gpio-keys/keys [ro]
 * /sys/devices/platform/gpio-keys/switches [ro]
 */
static DEVICE_ATTR(keys, S_IRUGO, gpio_keys_show_keys, NULL);
static DEVICE_ATTR(switches, S_IRUGO, gpio_keys_show_switches, NULL);

#define ATTR_STORE_FN(name, type)					\
static ssize_t gpio_keys_store_##name(struct device *dev,		\
				      struct device_attribute *attr,	\
				      const char *buf,			\
				      size_t count)			\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);	\
	ssize_t error;							\
									\
	error = gpio_keys_attr_store_helper(ddata, buf, type);		\
	if (error)							\
		return error;						\
									\
	return count;							\
}

ATTR_STORE_FN(disabled_keys, EV_KEY);
ATTR_STORE_FN(disabled_switches, EV_SW);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/gpio-keys/disabled_keys [rw]
 * /sys/devices/platform/gpio-keys/disables_switches [rw]
 */
static DEVICE_ATTR(disabled_keys, S_IWUSR | S_IRUGO,
		   gpio_keys_show_disabled_keys,
		   gpio_keys_store_disabled_keys);
static DEVICE_ATTR(disabled_switches, S_IWUSR | S_IRUGO,
		   gpio_keys_show_disabled_switches,
		   gpio_keys_store_disabled_switches);

static struct attribute *gpio_keys_attrs[] = {
	&dev_attr_keys.attr,
	&dev_attr_switches.attr,
	&dev_attr_disabled_keys.attr,
	&dev_attr_disabled_switches.attr,
	NULL,
};

static struct attribute_group gpio_keys_attr_group = {
	.attrs = gpio_keys_attrs,
};

static void gpio_keys_gpio_report_event(struct gpio_button_data *bdata)
{
	const struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned int type = button->type ?: EV_KEY;
	int state = (gpio_get_value_cansleep(button->gpio) ? 1 : 0) ^ button->active_low;

	SHKEY_DRV_ALL_LOG("is called");

	if (type == EV_ABS) {
		if (state)
		{
			input_event(input, type, button->code, button->value);
			SHKEY_DRV_DBG_LOG("input_event : code = %d, state = %d", button->code , state);
		}
	} else {
		input_event(input, type, button->code, !!state);
		SHKEY_DRV_DBG_LOG("input_event : code = %d, state = %d", button->code , state);
	}
	input_sync(input);

	SHKEY_DRV_ALL_LOG("input_sync output");
	SHKEY_DRV_ALL_LOG("is end");
}

static void gpio_keys_gpio_work_func(struct work_struct *work)
{
	struct gpio_button_data *bdata =
		container_of(work, struct gpio_button_data, work);

	SHKEY_DRV_ALL_LOG("is called");

	gpio_keys_gpio_report_event(bdata);
	
	SHKEY_DRV_ALL_LOG("is end");
}

static void gpio_keys_gpio_timer(unsigned long _data)
{
	struct gpio_button_data *bdata = (struct gpio_button_data *)_data;

	SHKEY_DRV_ALL_LOG("is called");

	schedule_work(&bdata->work);

	SHKEY_DRV_ALL_LOG("is end");
}

static irqreturn_t gpio_keys_gpio_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;

	SHKEY_DRV_DBG_LOG("is called");

	BUG_ON(irq != bdata->irq);

	if (bdata->timer_debounce)
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(bdata->timer_debounce));
	else
		schedule_work(&bdata->work);

	SHKEY_DRV_DBG_LOG("is end");

	return IRQ_HANDLED;
}

static void gpio_keys_irq_timer(unsigned long _data)
{
	struct gpio_button_data *bdata = (struct gpio_button_data *)_data;
	struct input_dev *input = bdata->input;
	unsigned long flags;

	SHKEY_DRV_ALL_LOG("is called");

	SHKEY_DRV_ALL_LOG("spin_lock_irqsave() is called");
	spin_lock_irqsave(&bdata->lock, flags);

	if (bdata->key_pressed) {
		input_event(input, EV_KEY, bdata->button->code, 0);
		input_sync(input);
		SHKEY_DRV_ALL_LOG("input_sync output");

		bdata->key_pressed = false;
	}
	spin_unlock_irqrestore(&bdata->lock, flags);
	SHKEY_DRV_ALL_LOG("spin_unlock_irqrestore() was called");

	SHKEY_DRV_ALL_LOG("is end");
}

static irqreturn_t gpio_keys_irq_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;
	const struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned long flags;

	SHKEY_DRV_DBG_LOG("is called");

	BUG_ON(irq != bdata->irq);

	SHKEY_DRV_ALL_LOG("spin_lock_irqsave() is called");
	spin_lock_irqsave(&bdata->lock, flags);

	if (!bdata->key_pressed) {
		input_event(input, EV_KEY, button->code, 1);

		input_sync(input);
		SHKEY_DRV_ALL_LOG("input_sync output");

		if (!bdata->timer_debounce) {
			input_event(input, EV_KEY, button->code, 0);
			input_sync(input);
			SHKEY_DRV_ALL_LOG("input_sync output");
			goto out;
		}

		bdata->key_pressed = true;
	}

	if (bdata->timer_debounce)
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(bdata->timer_debounce));

	SHKEY_DRV_DBG_LOG("is end");

out:
	spin_unlock_irqrestore(&bdata->lock, flags);
	SHKEY_DRV_ALL_LOG("spin_unlock_irqrestore() was called");

	SHKEY_DRV_DBG_LOG("is end");

	return IRQ_HANDLED;
}

static int __devinit gpio_keys_setup_key(struct platform_device *pdev,
					 struct input_dev *input,
					 struct gpio_button_data *bdata,
					 const struct gpio_keys_button *button)
{
	const char *desc = button->desc ? button->desc : "gpio_keys";
	struct device *dev = &pdev->dev;
	irq_handler_t isr;
	unsigned long irqflags;
	int irq, error;

	SHKEY_DRV_ALL_LOG("is called");

	bdata->input = input;
	bdata->button = button;
	spin_lock_init(&bdata->lock);

	if (gpio_is_valid(button->gpio)) {

		error = gpio_request(button->gpio, desc);
		if (error < 0) {
			dev_err(dev, "Failed to request GPIO %d, error %d\n",
				button->gpio, error);
			SHKEY_DRV_ALL_LOG("return error");
			return error;
		}

		error = gpio_direction_input(button->gpio);
		if (error < 0) {
			dev_err(dev,
				"Failed to configure direction for GPIO %d, error %d\n",
				button->gpio, error);
			goto fail;
		}

		if (button->debounce_interval) {
			error = gpio_set_debounce(button->gpio,
					button->debounce_interval * 1000);
			/* use timer if gpiolib doesn't provide debounce */
			if (error < 0)
				bdata->timer_debounce =
						button->debounce_interval;
		}

		irq = gpio_to_irq(button->gpio);
		if (irq < 0) {
			error = irq;
			dev_err(dev,
				"Unable to get irq number for GPIO %d, error %d\n",
				button->gpio, error);
			goto fail;
		}
		bdata->irq = irq;

		INIT_WORK(&bdata->work, gpio_keys_gpio_work_func);
		setup_timer(&bdata->timer,
			    gpio_keys_gpio_timer, (unsigned long)bdata);

		isr = gpio_keys_gpio_isr;
		irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	} else {
		if (!button->irq) {
			dev_err(dev, "No IRQ specified\n");
			SHKEY_DRV_ALL_LOG("return -EINVAL");
			return -EINVAL;
		}
		bdata->irq = button->irq;

		if (button->type && button->type != EV_KEY) {
			dev_err(dev, "Only EV_KEY allowed for IRQ buttons.\n");
			SHKEY_DRV_ALL_LOG("return -EINVAL");
			return -EINVAL;
		}

		bdata->timer_debounce = button->debounce_interval;
		setup_timer(&bdata->timer,
			    gpio_keys_irq_timer, (unsigned long)bdata);

		isr = gpio_keys_irq_isr;
		irqflags = 0;
	}

	input_set_capability(input, button->type ?: EV_KEY, button->code);

	/*
	 * If platform has specified that the button can be disabled,
	 * we don't want it to share the interrupt line.
	 */
	if (!button->can_disable)
		irqflags |= IRQF_SHARED;

	error = request_any_context_irq(bdata->irq, isr, irqflags, desc, bdata);
	if (error < 0) {
		dev_err(dev, "Unable to claim irq %d; error %d\n",
			bdata->irq, error);
		goto fail;
	}

	SHKEY_DRV_ALL_LOG("is end");

	return 0;

fail:
	if (gpio_is_valid(button->gpio))
		gpio_free(button->gpio);

	SHKEY_DRV_ALL_LOG("return error");

	return error;
}

static int gpio_keys_open(struct input_dev *input)
{
	struct gpio_keys_drvdata *ddata = input_get_drvdata(input);

	SHKEY_DRV_ALL_LOG("is called");

	SHKEY_DRV_ALL_LOG("is end");

	return ddata->enable ? ddata->enable(input->dev.parent) : 0;
}

static void gpio_keys_close(struct input_dev *input)
{
	struct gpio_keys_drvdata *ddata = input_get_drvdata(input);

	SHKEY_DRV_ALL_LOG("is called");

	if (ddata->disable)
	{
		ddata->disable(input->dev.parent);
	}
	SHKEY_DRV_ALL_LOG("is end");
}

/*
 * Handlers for alternative sources of platform_data
 */
#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static int gpio_keys_get_devtree_pdata(struct device *dev,
			    struct gpio_keys_platform_data *pdata)
{
	struct device_node *node, *pp;
	int i;
	struct gpio_keys_button *buttons;
	u32 reg;

	SHKEY_DRV_ALL_LOG("is called");

	node = dev->of_node;
	if (node == NULL)
	{
		SHKEY_DRV_ALL_LOG("return -ENODEV");
		return -ENODEV;
	}

	memset(pdata, 0, sizeof *pdata);

	pdata->rep = !!of_get_property(node, "autorepeat", NULL);

	/* First count the subnodes */
	pdata->nbuttons = 0;
	pp = NULL;
	while ((pp = of_get_next_child(node, pp)))
		pdata->nbuttons++;

	if (pdata->nbuttons == 0)
	{
		SHKEY_DRV_ALL_LOG("return -ENODEV");
		return -ENODEV;
	}

	buttons = kzalloc(pdata->nbuttons * (sizeof *buttons), GFP_KERNEL);
	if (!buttons)
	{
		SHKEY_DRV_ALL_LOG("return -ENOMEM");
		return -ENOMEM;
	}

	pp = NULL;
	i = 0;
	while ((pp = of_get_next_child(node, pp))) {
		enum of_gpio_flags flags;

		if (!of_find_property(pp, "gpios", NULL)) {
			pdata->nbuttons--;
			dev_warn(dev, "Found button without gpios\n");
			continue;
		}
		buttons[i].gpio = of_get_gpio_flags(pp, 0, &flags);
		buttons[i].active_low = flags & OF_GPIO_ACTIVE_LOW;

		if (of_property_read_u32(pp, "linux,code", &reg)) {
			dev_err(dev, "Button without keycode: 0x%x\n", buttons[i].gpio);
			goto out_fail;
		}
		buttons[i].code = reg;

		buttons[i].desc = of_get_property(pp, "label", NULL);

		if (of_property_read_u32(pp, "linux,input-type", &reg) == 0)
			buttons[i].type = reg;
		else
			buttons[i].type = EV_KEY;

		buttons[i].wakeup = !!of_get_property(pp, "gpio-key,wakeup", NULL);

		if (of_property_read_u32(pp, "debounce-interval", &reg) == 0)
			buttons[i].debounce_interval = reg;
		else
			buttons[i].debounce_interval = 5;

		i++;
	}

	pdata->buttons = buttons;

	SHKEY_DRV_ALL_LOG("is end");

	return 0;

out_fail:
	kfree(buttons);
	SHKEY_DRV_ALL_LOG("return -ENODEV");

	return -ENODEV;
}

static struct of_device_id gpio_keys_of_match[] = {
#ifndef CONFIG_KEYBOARD_SCKEY
	{ .compatible = "gpio-keys", },
#else
	{ .compatible = "SH_gpio-keys", },
#endif /* CONFIG_KEYBOARD_SCKEY */
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_keys_of_match);

#else

static int gpio_keys_get_devtree_pdata(struct device *dev,
			    struct gpio_keys_platform_data *altp)
{
	SHKEY_DRV_ALL_LOG("is called");

	SHKEY_DRV_ALL_LOG("return -ENODEV");
	return -ENODEV;
}

#define gpio_keys_of_match NULL

#endif

static void gpio_remove_key(struct gpio_button_data *bdata)
{
	SHKEY_DRV_ALL_LOG("is called");

	free_irq(bdata->irq, bdata);
	if (bdata->timer_debounce)
		del_timer_sync(&bdata->timer);
	cancel_work_sync(&bdata->work);
	if (gpio_is_valid(bdata->button->gpio))
		gpio_free(bdata->button->gpio);

	SHKEY_DRV_ALL_LOG("is end");
}

static int __devinit gpio_keys_probe(struct platform_device *pdev)
{
	const struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_drvdata *ddata;
	struct device *dev = &pdev->dev;
	struct gpio_keys_platform_data alt_pdata;
	struct input_dev *input;
	int i, error;
	int wakeup = 0;

	SHKEY_DRV_ALL_LOG("is called");

	if (!pdata) {
		error = gpio_keys_get_devtree_pdata(dev, &alt_pdata);
		if (error)
		{
			SHKEY_DRV_ALL_LOG("return error");
			return error;
		}
		pdata = &alt_pdata;
	}

	ddata = kzalloc(sizeof(struct gpio_keys_drvdata) +
			pdata->nbuttons * sizeof(struct gpio_button_data),
			GFP_KERNEL);
	input = input_allocate_device();
	if (!ddata || !input) {
		dev_err(dev, "failed to allocate state\n");
		SHKEY_DRV_ALL_LOG("error = -ENOMEM");
		error = -ENOMEM;
		goto fail1;
	}

	ddata->input = input;
	ddata->n_buttons = pdata->nbuttons;
	ddata->enable = pdata->enable;
	ddata->disable = pdata->disable;
	mutex_init(&ddata->disable_lock);

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	input->name = pdata->name ? : pdev->name;
#ifndef CONFIG_KEYBOARD_SCKEY
	input->phys = "gpio-keys/input0";
#else
	input->phys = "SH_gpio-keys/input0";
#endif /* CONFIG_KEYBOARD_SCKEY */
	input->dev.parent = &pdev->dev;
	input->open = gpio_keys_open;
	input->close = gpio_keys_close;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);

	for (i = 0; i < pdata->nbuttons; i++) {
		const struct gpio_keys_button *button = &pdata->buttons[i];
		struct gpio_button_data *bdata = &ddata->data[i];

		error = gpio_keys_setup_key(pdev, input, bdata, button);
		if (error)
			goto fail2;

		if (button->wakeup)
			wakeup = 1;
	}

	error = sysfs_create_group(&pdev->dev.kobj, &gpio_keys_attr_group);
	if (error) {
		dev_err(dev, "Unable to export keys/switches, error: %d\n",
			error);
		goto fail2;
	}

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			error);
		goto fail3;
	}

	/* get current state of buttons that are connected to GPIOs */
	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];
		if (gpio_is_valid(bdata->button->gpio))
			gpio_keys_gpio_report_event(bdata);
	}
	input_sync(input);
	SHKEY_DRV_ALL_LOG("input_sync output");

	device_init_wakeup(&pdev->dev, wakeup);

	SHKEY_DRV_ALL_LOG("is end");
	return 0;

 fail3:
	SHKEY_DRV_ALL_LOG("fail3");
	sysfs_remove_group(&pdev->dev.kobj, &gpio_keys_attr_group);
 fail2:
	SHKEY_DRV_ALL_LOG("fail2");
	while (--i >= 0)
		gpio_remove_key(&ddata->data[i]);

	platform_set_drvdata(pdev, NULL);
 fail1:
	SHKEY_DRV_ALL_LOG("fail1");
	input_free_device(input);
	kfree(ddata);
	/* If we have no platform_data, we allocated buttons dynamically. */
	if (!pdev->dev.platform_data)
		kfree(pdata->buttons);

	SHKEY_DRV_ALL_LOG("return error");

	return error;
}

static int __devexit gpio_keys_remove(struct platform_device *pdev)
{
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

	SHKEY_DRV_ALL_LOG("is called");

	sysfs_remove_group(&pdev->dev.kobj, &gpio_keys_attr_group);

	device_init_wakeup(&pdev->dev, 0);

	for (i = 0; i < ddata->n_buttons; i++)
		gpio_remove_key(&ddata->data[i]);

	input_unregister_device(input);

	/*
	 * If we had no platform_data, we allocated buttons dynamically, and
	 * must free them here. ddata->data[0].button is the pointer to the
	 * beginning of the allocated array.
	 */
	if (!pdev->dev.platform_data)
		kfree(ddata->data[0].button);

	kfree(ddata);
	SHKEY_DRV_ALL_LOG("is end");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gpio_keys_suspend(struct device *dev)
{
	struct gpio_keys_drvdata *ddata = dev_get_drvdata(dev);
	int i;

	SHKEY_DRV_DBG_LOG("is called");

	if (device_may_wakeup(dev)) {
		for (i = 0; i < ddata->n_buttons; i++) {
			struct gpio_button_data *bdata = &ddata->data[i];
			if (bdata->button->wakeup)
				enable_irq_wake(bdata->irq);
		}
	}

	SHKEY_DRV_DBG_LOG("is end");

	return 0;
}

static int gpio_keys_resume(struct device *dev)
{
	struct gpio_keys_drvdata *ddata = dev_get_drvdata(dev);
	int i;

	SHKEY_DRV_DBG_LOG("is called");

	for (i = 0; i < ddata->n_buttons; i++) {
		struct gpio_button_data *bdata = &ddata->data[i];
		if (bdata->button->wakeup && device_may_wakeup(dev))
			disable_irq_wake(bdata->irq);

#ifndef CONFIG_KEYBOARD_SCKEY
		if (gpio_is_valid(bdata->button->gpio))
		{
			gpio_keys_gpio_report_event(bdata);
		}
#endif /* CONFIG_KEYBOARD_SCKEY */
	}
#ifndef CONFIG_KEYBOARD_SCKEY
	input_sync(ddata->input);
#endif /* CONFIG_KEYBOARD_SCKEY */

	SHKEY_DRV_DBG_LOG("is end");

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(gpio_keys_pm_ops, gpio_keys_suspend, gpio_keys_resume);

static struct platform_driver gpio_keys_device_driver = {
	.probe		= gpio_keys_probe,
	.remove		= __devexit_p(gpio_keys_remove),
	.driver		= {
#ifndef CONFIG_KEYBOARD_SCKEY
		.name	= "gpio-keys",
#else
		.name	= "SH_gpio-keys",
#endif /* CONFIG_KEYBOARD_SCKEY */
		.owner	= THIS_MODULE,
		.pm	= &gpio_keys_pm_ops,
		.of_match_table = gpio_keys_of_match,
	}
};

static int __init gpio_keys_init(void)
{

	SHKEY_DRV_ALL_LOG("is called");

	SHKEY_DRV_ALL_LOG("is end");

	return platform_driver_register(&gpio_keys_device_driver);
}

static void __exit gpio_keys_exit(void)
{

	SHKEY_DRV_ALL_LOG("is called");

	SHKEY_DRV_ALL_LOG("is end");
	
	platform_driver_unregister(&gpio_keys_device_driver);
}

late_initcall(gpio_keys_init);
module_exit(gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>");
MODULE_DESCRIPTION("Keyboard driver for GPIOs");
MODULE_ALIAS("platform:gpio-keys");
