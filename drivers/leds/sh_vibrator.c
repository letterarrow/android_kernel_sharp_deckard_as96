#define DEBUG
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

struct sh_vib_i2c {
	struct i2c_client	*client;
	struct regulator	*reg_vcc;
	struct gpio_desc 	*gpio;
	struct led_classdev	cdev;
	struct mutex		lock;
};

int sh_vib_i2c_write(struct i2c_client *client,
					u8 reg, u8 data)
{
	u8 buf[2];
	int rc;
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};

	buf[0] = reg;
	buf[1] = data;

	rc = i2c_transfer(client->adapter, &msg, 1);
	if (rc != 1) {
		dev_err(&client->dev,
				"writing to reg %d is failed(%d).\n", reg, rc);
		rc = -1;
	}

	return rc;
}

static int sh_vib_i2c_set_blocking(struct led_classdev *led_cdev,
							enum led_brightness brightness)
{
	struct sh_vib_i2c *sh_vib = container_of(led_cdev, struct sh_vib_i2c, cdev);
	struct i2c_client *client = sh_vib->client;
	int rc;

	mutex_lock(&sh_vib->lock);
	if (brightness) {
		pr_debug("sh_vib_i2c: vibrator becomes on.\n");
		if (!regulator_is_enabled(sh_vib->reg_vcc)) {
			pr_debug("sh_vib_i2c: regulator is disabled.\n");
			rc = regulator_enable(sh_vib->reg_vcc);
			if (rc) {
				dev_err(&client->dev, "enabling regulator is failed.\n");
				mutex_unlock(&sh_vib->lock);
				return -1;
			}
			pr_debug("sh_vib_i2c: regulator becomes enabled.\n");
		} else {
			pr_debug("sh_vib_i2c: regulator is already enabled.\n");
		}
		usleep_range(300, 350);

		if ((sh_vib_i2c_write(client, 0x01, 0x07) < 0) ||
			(sh_vib_i2c_write(client, 0x02, 0x0f) < 0)) {
			dev_err(&client->dev, "sh_vib_i2c_write is failed.\n");
			regulator_disable(sh_vib->reg_vcc);
			mutex_unlock(&sh_vib->lock);
			return -1;
		}
		
		gpiod_set_value(sh_vib->gpio, 1);

	} else {
		pr_debug("sh_vib_i2c: vibrator becomes off.\n");

		gpiod_set_value(sh_vib->gpio, 0);

		if (regulator_is_enabled(sh_vib->reg_vcc)) {
			pr_debug("sh_vib_i2c: regulator is enabled.\n");
			msleep(30);
			regulator_disable(sh_vib->reg_vcc);
			pr_debug("sh_vib_i2c: regulator becomes disabled.\n");
		} else {
			pr_debug("sh_vib_i2c: regulator is already disabled.\n");
		}
	}

	mutex_unlock(&sh_vib->lock);

	return 0;
}

static enum led_brightness sh_vib_i2c_get(struct led_classdev *cled)
{
	return cled->brightness;
}

static int sh_vib_i2c_probe(struct i2c_client *client, 
							const struct i2c_device_id *id)
{
	struct sh_vib_i2c *sh_vib;
	struct device_node *np = client->dev.of_node;
	int rc;

	sh_vib = devm_kzalloc(&client->dev, sizeof(*sh_vib), GFP_KERNEL);
	if (!sh_vib)
		return -ENOMEM;
	
	i2c_set_clientdata(client, sh_vib);
	sh_vib->client = client;

	sh_vib->reg_vcc = devm_regulator_get(&client->dev, "vcc");
	if (IS_ERR(sh_vib->reg_vcc)) {
		dev_err(&client->dev, "Geting regulator is failed.\n");
		return -1;
	}

	sh_vib->gpio = devm_gpiod_get(&client->dev, "sw", GPIOD_ASIS);
	if (IS_ERR(sh_vib->gpio)) {
		dev_err(&client->dev, "Getting GPIO is failed.\n");
		return -1;
	}

	mutex_init(&sh_vib->lock);

	sh_vib->cdev.name = of_get_property(np, "label", NULL) ? : np->name;
	sh_vib->cdev.brightness = LED_OFF;
	sh_vib_i2c_set_blocking(&sh_vib->cdev, LED_OFF);
	sh_vib->cdev.max_brightness = LED_ON;
	sh_vib->cdev.brightness_set_blocking = sh_vib_i2c_set_blocking;
	sh_vib->cdev.brightness_get = sh_vib_i2c_get;
	sh_vib->cdev.default_trigger = 
		of_get_property(np, "linux,default-trigger", NULL);

	rc = devm_led_classdev_register(&client->dev, &sh_vib->cdev);
	if (rc) {
		dev_err(&client->dev, "Registration of led is failed.\n");
		return rc;
	}

	return 0;
}

static const struct i2c_device_id sh_vib_i2c_id[] = {
	{ "sh_vib_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sh_vib_i2c_id);

static const struct of_device_id sh_vib_i2c_of_id[] = {
	{ .compatible = "sharp,sh_vib_i2c", },
	{ }
};
MODULE_DEVICE_TABLE(of, sh_vib_i2c_of_id);

static struct i2c_driver sh_vib_i2c_driver = {
	.probe  = sh_vib_i2c_probe,
	.id_table = sh_vib_i2c_id,
	.driver = {
		.name = "sh_vib_i2c",
		.of_match_table = sh_vib_i2c_of_id,
	},
};
module_i2c_driver(sh_vib_i2c_driver);

MODULE_DESCRIPTION("SHARP linear vibrator driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("letterarrow <letterarrow@users.noreply.github.com>");