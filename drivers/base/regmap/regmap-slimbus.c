/* CONFIG_SH_AUDIO_DRIVER newly created */ /*03-001*/
/*
 * Register map access API - Slimbus support
 *
 * Copyright (c) 2012, Wolfson Microelectronics.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/slimbus/slimbus.h>
#include <linux/pm_runtime.h>

// BOUGE!! Messy external reference & illegal context
//	whatever I do none will be proper, hence so be it with minimum changes
struct slim_device *g_wm_slim_handler;
static int regmap_slimbus_burst_write(struct slim_device *slim,
				    const void *r, size_t reg_size,
				    const void *val, size_t val_size)
{
	int ret;

	if (val_size % 2 != 0)
		return -ENOTSUPP;

	ret = slim_change_val_elements(slim, r, reg_size, val, val_size);
	if (ret != 0) {
		dev_err(&slim->dev, "Failed to burst write\n");
		return ret;
	}

	return 0;
}

static int regmap_slimbus_gather_write(struct device *dev,
				    const void *r, size_t reg_size,
				    const void *val, size_t val_size)
{
	struct slim_device *slim = container_of(dev, struct slim_device, dev);
	struct slim_ele_access msg;
	u32 reg;
	u16 buf;
	int ret;
	const u16 *dat = val;

	BUG_ON(reg_size != 4);
	if (val_size > 2)
		return regmap_slimbus_burst_write(slim, r, reg_size, val, val_size);

	reg = be32_to_cpup(r);

	memset(&msg, 0, sizeof(msg));
	msg.num_bytes = 2;

	while (val_size > 0) {
		buf = (reg >> 8) & 0xffff;
		msg.start_offset = 0x800;

		ret = slim_change_val_element(slim, &msg, (void *)&buf, sizeof(buf));
		if (ret != 0) {
			dev_err(&slim->dev, "Failed to page register %04x: %04x: %d\n",
				reg, be16_to_cpup(val), ret);
			return ret;
		}

		msg.start_offset = 0xa00 + ((reg & 0xff) * 2);
		buf = be16_to_cpup(dat);

		ret = slim_change_val_element(slim, &msg, (void *)&buf, sizeof(buf));
		if (ret != 0) {
			dev_err(&slim->dev, "Failed to write register %04x: %04x: %d\n",
				reg, buf, ret);
			return ret;
		}

		val_size -= 2;
		reg += 1;
		dat += 1;
	}

	return 0;
}

static int regmap_slimbus_write(struct device *dev, const void *data, size_t count)
{
	BUG_ON(count < 4);

	return regmap_slimbus_gather_write(dev, data, 4, data + 4, count - 4);
}

static int regmap_slimbus_read(struct device *dev,
			    const void *r, size_t reg_size,
			    void *val, size_t val_size)
{
	struct slim_device *slim = container_of(dev, struct slim_device, dev);
	struct slim_ele_access msg;
	u32 reg;
	u16 buf;
	int ret;
	u16 *dat = val;

	BUG_ON(reg_size != 4);
	if (val_size % 2 != 0)
		return -ENOTSUPP;

	reg = be32_to_cpup(r);

	memset(&msg, 0, sizeof(msg));
	msg.num_bytes = 2;

	while (val_size > 0) {
		buf = (reg >> 8) & 0xffff;
		msg.start_offset = 0x800;

		ret = slim_change_val_element(slim, &msg, (void *)&buf, sizeof(buf));
		if (ret != 0) {
			dev_err(&slim->dev, "Failed to page register %x: %d\n",
				reg, ret);
			return ret;
		}

		msg.start_offset = 0xa00 | ((reg & 0xff) * 2);
		ret = slim_request_val_element(slim, &msg, (void *)&buf, sizeof(buf));
		if (ret != 0) {
			dev_err(&slim->dev, "Failed to read register %x: %d\n",
				reg, ret);
			WARN_ON(1);
			return ret;
		}
		buf = cpu_to_be16(buf);
		memcpy(dat, &buf, sizeof(buf));

		val_size -= 2;
		reg += 1;
		dat += 1;
	}

	return ret;
}

static struct regmap_bus regmap_slimbus = {
	.write = regmap_slimbus_write,
	.gather_write = regmap_slimbus_gather_write,
	.read = regmap_slimbus_read,
};

/**
 * regmap_init_slimbus(): Initialise register map
 *
 * @dev: Device that will be interacted with
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer to
 * a struct regmap.
 */
struct regmap *regmap_init_slimbus(struct slim_device *dev,
				   const struct regmap_config *config)
{
	return regmap_init(&dev->dev, &regmap_slimbus, config);
}
EXPORT_SYMBOL_GPL(regmap_init_slimbus);

/**
 * devm_regmap_init_slimbus(): Initialise managed register map
 *
 * @dev: Device that will be interacted with
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer
 * to a struct regmap.  The regmap will be automatically freed by the
 * device management code.
 */
struct regmap *devm_regmap_init_slimbus(struct slim_device *dev,
				        const struct regmap_config *config)
{
	g_wm_slim_handler = dev;
	return devm_regmap_init(&dev->dev, &regmap_slimbus, config);
}
EXPORT_SYMBOL_GPL(devm_regmap_init_slimbus);

MODULE_LICENSE("GPL v2");
