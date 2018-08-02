/* CONFIG_SH_AUDIO_DRIVER newly created */ /*03-001*/
/*
 * arizona-slimbus.c  --  Arizona Slimbus bus interface
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/err.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/slimbus/slimbus.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <linux/mfd/arizona/core.h>

#include "arizona.h"

static const u8 wm5102_if_ea[] = { 0x00, 0x7f, 0x02, 0x51, 0x2f, 0x01 };
static const u8 wm5102_gd_ea[] = { 0x00, 0x00, 0x02, 0x51, 0x2f, 0x01 };


static int arizona_slim_get_laddr(struct slim_device *sb,
				  const u8 *e_addr, u8 e_len, u8 *laddr)
{
	int ret, i;

	for (i = 0; i < 5; i++) {
		msleep(20);
		ret = slim_get_logical_addr(sb, e_addr, e_len, laddr);
		if (!ret)
			break;
	}

	return ret;
}

static __devinit int arizona_slim_probe(struct slim_device *slim)
{
	struct arizona *arizona;
	//const struct regmap_config *regmap_config;
	int ret;
	u8 la;
	struct arizona_pdata *pdata;

#if 0
	switch (id->driver_data) {
#ifdef CONFIG_MFD_WM5102
	case WM5102:
		regmap_config = &wm5102_i2c_regmap;
		break;
#endif
#ifdef CONFIG_MFD_WM5110
	case WM5110:
		regmap_config = &wm5110_i2c_regmap;
		break;
#endif
	default:
		dev_err(&slim->dev, "Unknown device type %ld\n",
			id->driver_data);
		return -EINVAL;
	}
#endif

	if (!slim->ctrl) {
		dev_err(&slim->dev, "No slimbus control data\n");
		return -EINVAL;
	}

	arizona = devm_kzalloc(&slim->dev, sizeof(*arizona), GFP_KERNEL);
	if (arizona == NULL)
		return -ENOMEM;
	
	/* Reset the device so it reannounces itself on the bus */
	pdata = dev_get_platdata(&slim->dev);
	if (pdata == NULL || !pdata->reset) {
		dev_err(&slim->dev,
			"Slimbus requires reset line specified in pdata\n");
		return -EINVAL;
	}

	ret = gpio_request_one(pdata->reset, GPIOF_DIR_OUT | GPIOF_INIT_HIGH,
			       "arizona slimbus probe reset");
	if (ret != 0) {
		dev_err(&slim->dev, "Failed to obtain gpio reset: %d\n", ret);
	}
	gpio_free(pdata->reset);

	arizona->dev = &slim->dev;

	slim_set_clientdata(slim, arizona);

	msleep(100);

	ret = arizona_slim_get_laddr(slim, wm5102_if_ea, 6, &la);
	if (ret != 0) {
		dev_err(&slim->dev, "Failed to get interface LA: %d\n", ret);
		return ret;
	}

	/* The framework doesn't do this for us */
	slim->laddr = la;

	dev_dbg(&slim->dev, "%02x%02x%02x%02x%02x%02x LA %x %x\n",
		slim->e_addr[0], slim->e_addr[1], slim->e_addr[2],
		slim->e_addr[3], slim->e_addr[4], slim->e_addr[5],
		slim->laddr, la);

	arizona->slim_gd.name = "wm5102-gd";
	memcpy(arizona->slim_gd.e_addr, wm5102_gd_ea, sizeof(wm5102_gd_ea));

	ret = slim_add_device(slim->ctrl, &arizona->slim_gd);
	if (ret != 0) {
		dev_err(&slim->dev, "Failed to add generic device: %d\n", ret);
		return ret;
	}

	ret = arizona_slim_get_laddr(&arizona->slim_gd, wm5102_gd_ea, 6, &la);
	if (ret != 0) {
		dev_err(&slim->dev, "Failed to get generic LA: %d\n", ret);
		return ret;
	}
	arizona->slim_gd.laddr = la;
	dev_dbg(&slim->dev, "Generic device LA: %d\n", la);

#if 0
	{
	struct slim_ele_access msg;
	u16 buf;

	memset(&msg, 0, sizeof(msg));
	msg.start_offset = 0;
	msg.num_bytes = 2;

	msg.start_offset = 0x800;
	buf = 0;
	ret = slim_change_val_element(slim, &msg, (void *)&buf, sizeof(buf));
	if (ret != 0) {
		dev_err(&slim->dev, "Failed to page: %d\n", ret);
	}

	msg.start_offset = 0xa00;
	ret = slim_request_val_element(slim, &msg, (void*)&buf, sizeof(buf));
	if (ret != 0) {
		dev_err(&slim->dev, "Failed to read register: %d\n", ret);
		WARN_ON(1);
	}
	dev_crit(&slim->dev, "Read %x\n", buf);
	}
#endif

	arizona->slim = slim;

	arizona->regmap = devm_regmap_init_slimbus(slim, &wm5102_i2c_regmap);
	if (IS_ERR(arizona->regmap)) {
		ret = PTR_ERR(arizona->regmap);
		dev_err(&slim->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	if (0) {
		unsigned int val;

		ret = regmap_read(arizona->regmap, 0, &val);
		if (ret != 0)
			dev_err(arizona->dev, "Failed to read: %d\n", ret);
		dev_err(arizona->dev, "READ ID %x\n", val);

		ret = regmap_write(arizona->regmap, 0xe3c, 0xbeef);
		if (ret != 0)
			dev_err(arizona->dev, "Write failed: %d\n", ret);

		ret = regmap_read(arizona->regmap, 0xe3c, &val);
		if (ret != 0)
			dev_err(arizona->dev, "Failed to read: %d\n", ret);
		dev_err(arizona->dev, "READ %x\n", val);
	}

	arizona->type = WM5102;
	arizona->irq = MSM_GPIO_TO_INT(42);

	ret = slim_reservemsg_bw(slim, 8 * 1024, true);
	if (ret != 0) {
		dev_warn(&slim->dev, "Failed to reserve bandwidth: %d\n", ret);
	}

	return arizona_dev_init(arizona);
}

static int __devexit arizona_slim_remove(struct slim_device *slim)
{
	//arizona_dev_exit(arizona);
	return 0;
}

static const struct slim_device_id arizona_slim_id[] = {
	{ "wm5102", WM5102 },
	{ "wm5110", WM5110 },
	{ }
};

static struct slim_driver arizona_slim_driver = {
	.driver = {
		.name	= "arizona",
		.owner	= THIS_MODULE,
		.pm	= &arizona_pm_ops,
	},
	.probe		= arizona_slim_probe,
	.remove		= __devexit_p(arizona_slim_remove),
	.id_table	= arizona_slim_id,
};

static __init int arizona_slim_init(void)
{
	return slim_driver_register(&arizona_slim_driver);
}
module_init(arizona_slim_init);

static __exit void arizona_slim_exit(void)
{
}
module_exit(arizona_slim_exit);

MODULE_DESCRIPTION("Arizona Slimbus interface");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
