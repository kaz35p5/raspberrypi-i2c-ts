// SPDX-License-Identifier: GPL-2.0
/*
 * Raspberry Pi i2c touchscreen driver
 *
 * Copyright (C) 2015, 2017 Raspberry Pi
 * Copyright (C) 2018 Nicolas Saenz Julienne <nsaenzjulienne@suse.de>
 * Copyright (C) 2024 WADA Kazuhiro <kaz35p5@gmail.com>
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>

#define RPI_TS_DEFAULT_WIDTH	800
#define RPI_TS_DEFAULT_HEIGHT	480

#define RPI_TS_MAX_SUPPORTED_POINTS	10

#define RPI_TS_FTS_TOUCH_DOWN		0
#define RPI_TS_FTS_TOUCH_CONTACT	2

#define RPI_TS_POLL_INTERVAL		17	/* 60fps */

#define RPI_TS_NPOINTS_REG_INVALIDATE	99

struct rpi_i2c_ts {
	struct i2c_client *client;
	struct input_dev *input;
	struct touchscreen_properties prop;
	struct regmap *regmap;
	int known_ids;
};

struct rpi_ts_regs {
	u8 device_mode;
	u8 gesture_id;
	u8 num_points;
	struct rpi_ts_touch {
		u8 xh;
		u8 xl;
		u8 yh;
		u8 yl;
		u8 pressure; /* Not supported */
		u8 area;     /* Not supported */
	} point[RPI_TS_MAX_SUPPORTED_POINTS];
};

static void rpi_i2c_ts_poll(struct input_dev *input)
{
	struct rpi_i2c_ts *ts = input_get_drvdata(input);
	struct device *dev = &ts->client->dev;
	struct rpi_ts_regs regs;
	int modified_ids = 0;
	long released_ids;
	int event_type;
	int touchid;
	int x, y;
	int i;
	int error;

	error = regmap_bulk_read(ts->regmap, offsetof(struct rpi_ts_regs, num_points), &regs.num_points, 1);
	if (error) {
		dev_err_ratelimited(dev, "Unable to fetch data, error: %d\n", error);
		return;
	}

	if (regs.num_points == RPI_TS_NPOINTS_REG_INVALIDATE ||
	    (regs.num_points == 0 && ts->known_ids == 0))
		return;

	for (i = 0; i < regs.num_points; i++) {
		error = regmap_bulk_read(ts->regmap, offsetof(struct rpi_ts_regs, point[i]), &regs.point[i], 4);
		if (error) {
			dev_err_ratelimited(dev, "Unable to fetch data, error: %d\n", error);
			return;
		}

		x = (((int)regs.point[i].xh & 0xf) << 8) + regs.point[i].xl;
		y = (((int)regs.point[i].yh & 0xf) << 8) + regs.point[i].yl;
		touchid = (regs.point[i].yh >> 4) & 0xf;
		event_type = (regs.point[i].xh >> 6) & 0x03;

		modified_ids |= BIT(touchid);

		if (event_type == RPI_TS_FTS_TOUCH_DOWN ||
		    event_type == RPI_TS_FTS_TOUCH_CONTACT) {
			input_mt_slot(input, touchid);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, 1);
			touchscreen_report_pos(input, &ts->prop, x, y, true);
		}
	}

	released_ids = ts->known_ids & ~modified_ids;
	for_each_set_bit(i, &released_ids, RPI_TS_MAX_SUPPORTED_POINTS) {
		input_mt_slot(input, i);
		input_mt_report_slot_inactive(input);
		modified_ids &= ~(BIT(i));
	}
	ts->known_ids = modified_ids;

	input_mt_sync_frame(input);
	input_sync(input);
}



static const struct regmap_config rpi_ts_i2c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int rpi_i2c_ts_probe(struct i2c_client *client)
{
	struct input_dev *input;
	struct rpi_i2c_ts *ts;
	// unsigned int data;
	int ret;

	dev_dbg(&client->dev, "probing for Raspberry Pi TS I2C\n");

	ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);
    if (!ts) {
        return -ENOMEM;
	}

	ts->regmap = devm_regmap_init_i2c(client, &rpi_ts_i2c_regmap_config);
	if (IS_ERR(ts->regmap)) {
		ret = PTR_ERR(ts->regmap);
		dev_err(&client->dev, "regmap allocation failed, %d\n", ret);
		return ret;
	}

	// ret = regmap_read(ts->regmap, 2, &data);
	// if (ret < 0) {
	// 	dev_err(&client->dev, "failed to read reg, %d\n", ret);
	// 	return ret;
	// }

	input = devm_input_allocate_device(&client->dev);
	if (!input) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	ts->client = client;
	ts->input = input;
	input_set_drvdata(input, ts);
	i2c_set_clientdata(client, ts);

	input->name = "raspberrypi-i2c-ts";
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	input_set_abs_params(input, ABS_MT_POSITION_X, 0,
			     RPI_TS_DEFAULT_WIDTH, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0,
			     RPI_TS_DEFAULT_HEIGHT, 0, 0);

	touchscreen_parse_properties(input, true, &ts->prop);

	ret = input_mt_init_slots(input, RPI_TS_MAX_SUPPORTED_POINTS,
				    INPUT_MT_DIRECT);
	if (ret) {
		dev_err(&client->dev, "could not init mt slots, %d\n", ret);
		return ret;
	}

	ret = input_setup_polling(input, rpi_i2c_ts_poll);
	if (ret) {
		dev_err(&client->dev, "could not set up polling mode, %d\n", ret);
		return ret;
	}

	input_set_poll_interval(input, RPI_TS_POLL_INTERVAL);

	ret = input_register_device(input);
	if (ret) {
		dev_err(&client->dev, "could not register input device, %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id rpi_i2c_ts_match[] = {
	{ .compatible = "raspberrypi,i2c-ts", },
	{},
};
MODULE_DEVICE_TABLE(of, rpi_i2c_ts_match);

static struct i2c_driver rpi_i2c_ts_driver = {
	.driver = {
		.name = "raspberrypi-i2c-ts",
		.of_match_table = rpi_i2c_ts_match,
	},
	.probe = rpi_i2c_ts_probe,
};
module_i2c_driver(rpi_i2c_ts_driver);

MODULE_AUTHOR("WADA Kazuhiro");
MODULE_AUTHOR("Gordon Hollingworth");
MODULE_AUTHOR("Nicolas Saenz Julienne <nsaenzjulienne@suse.de>");
MODULE_DESCRIPTION("Raspberry Pi i2c touchscreen driver");
MODULE_LICENSE("GPL v2");
