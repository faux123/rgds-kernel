// SPDX-License-Identifier: GPL-2.0
/*
 * Silan SC7U22 gyroscope, Rockchip sensor framework driver.
 *
 * Reconstructed from the stock binary of the Anbernic RG DS kernel
 * (Rockchip develop-6.1 BSP, Linux 6.1.141), which was distributed without
 * source. Register values and control flow follow the disassembly of
 * sc7u22_set_gyro_odr, sc7u22_set_gyro_range, sensor_active, sensor_init and
 * sensor_report_value; table contents come from the stock .data section.
 */
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sensor-dev.h>

#define SC7U22_CHIP_ID_REG	0x01
#define SC7U22_CHIP_ID		0x6a
#define SC7U22_GYR_DATA_REG	0x12
#define SC7U22_GYR_ODR_REG	0x42
#define SC7U22_GYR_RANGE_REG	0x43
#define SC7U22_PWR_REG		0x7d
#define SC7U22_PWR_GYR_EN	BIT(1)

/*
 * GYRO_ID_SC7U22 comes from sensor-dev.h (see sensor-dev.h.snippet). In the
 * stock header it sits right after GYRO_ID_ICM4260X and evaluates to 0x43.
 */

struct sc7u22_code {
	int val;
	u8 code;
};

static const struct sc7u22_code sc7u22_gyro_odr[] = {
	{ 25, 0x6 }, { 50, 0x7 }, { 100, 0x8 }, { 200, 0x9 }, { 400, 0xa },
	{ 800, 0xb }, { 1600, 0xc }, { 3200, 0xd },
};

/* Range in dps. Only the first entry (2000) is confirmed by position. */
static const struct sc7u22_code sc7u22_gyro_range[] = {
	{ 2000, 0x0 }, { 1000, 0x1 }, { 500, 0x2 }, { 250, 0x3 }, { 125, 0x4 },
};

/* Every register update is written twice, as the stock driver does. */
static void sc7u22_write_twice(struct i2c_client *client, int reg, int val)
{
	sensor_write_reg(client, reg, val);
	sensor_write_reg(client, reg, val);
}

/* Picks the first rate >= the request; a request above 3200 Hz is ignored. */
static void sc7u22_set_gyro_odr(struct i2c_client *client, int rate)
{
	int i, val;

	val = sensor_read_reg(client, SC7U22_GYR_ODR_REG);
	for (i = 0; i < ARRAY_SIZE(sc7u22_gyro_odr); i++)
		if (sc7u22_gyro_odr[i].val >= rate)
			break;
	if (i == ARRAY_SIZE(sc7u22_gyro_odr))
		return;

	val = (val & 0xf0) | sc7u22_gyro_odr[i].code;
	sc7u22_write_twice(client, SC7U22_GYR_ODR_REG, val);
}

/* Exact match only; an unknown range leaves the register untouched. */
static void sc7u22_set_gyro_range(struct i2c_client *client, int range)
{
	int i, val;

	for (i = 0; i < ARRAY_SIZE(sc7u22_gyro_range); i++)
		if (sc7u22_gyro_range[i].val == range)
			break;
	if (i == ARRAY_SIZE(sc7u22_gyro_range))
		return;

	val = sensor_read_reg(client, SC7U22_GYR_RANGE_REG);
	val = (val & 0xf8) | sc7u22_gyro_range[i].code;
	sc7u22_write_twice(client, SC7U22_GYR_RANGE_REG, val);
}

static int sensor_active(struct i2c_client *client, int enable, int rate)
{
	int val;

	/* Unlike the accel node, the stock gyro active() does not touch the ODR. */
	dev_err(&client->dev, "sc7u22 gyro enable:%d,rate:%d\n", enable, rate);

	val = sensor_read_reg(client, SC7U22_PWR_REG) & ~SC7U22_PWR_GYR_EN & 0xff;
	if (enable)
		val |= SC7U22_PWR_GYR_EN;
	sc7u22_write_twice(client, SC7U22_PWR_REG, val);
	dev_err(&client->dev, "sc7u22 gyro write reg:0x%x\n", val);

	return 0;
}

static int sensor_init(struct i2c_client *client)
{
	struct sensor_private_data *sensor = i2c_get_clientdata(client);

	sensor->status_cur = SENSOR_OFF;

	sc7u22_set_gyro_range(client, 2000);
	sc7u22_set_gyro_odr(client, 200);

	return 0;
}

static int sensor_report_value(struct i2c_client *client)
{
	struct sensor_private_data *sensor = i2c_get_clientdata(client);
	struct sensor_platform_data *pdata = sensor->pdata;
	struct sensor_axis axis;
	u8 buf[6] = { 0 };
	int x, y, z;
	int i, reg;

	if (sensor->ops->read_len < 6) {
		dev_err(&client->dev, "%s:length is error,len=%d\n", __func__,
			sensor->ops->read_len);
		return -1;
	}

	/* Byte-wise reads, one register per call, as in the stock driver. */
	reg = sensor->ops->read_reg;
	for (i = 0; i < sensor->ops->read_len; i++)
		buf[i] = sensor_read_reg(client, (reg + i) & 0xff);

	/* Raw counts scaled by 2/3 (signed, truncating), as the stock code does. */
	x = (s16)((buf[0] << 8) | buf[1]) * 2 / 3;
	y = (s16)((buf[2] << 8) | buf[3]) * 2 / 3;
	z = (s16)((buf[4] << 8) | buf[5]) * 2 / 3;

	axis.x = pdata->orientation[0] * x + pdata->orientation[1] * y +
		 pdata->orientation[2] * z;
	axis.y = pdata->orientation[3] * x + pdata->orientation[4] * y +
		 pdata->orientation[5] * z;
	axis.z = pdata->orientation[6] * x + pdata->orientation[7] * y +
		 pdata->orientation[8] * z;

	if (sensor->status_cur == SENSOR_ON) {
		input_report_rel(sensor->input_dev, REL_RX, axis.x);
		input_report_rel(sensor->input_dev, REL_RY, axis.y);
		input_report_rel(sensor->input_dev, REL_RZ, axis.z);
		input_sync(sensor->input_dev);
	}

	mutex_lock(&sensor->data_mutex);
	sensor->axis = axis;
	mutex_unlock(&sensor->data_mutex);

	if (pdata->irq_enable)
		sensor_read_reg(client, sensor->ops->int_status_reg);

	return 0;
}

static struct sensor_operate gyro_sc7u22_ops = {
	.name		= "sc7u22_gyro",
	.type		= SENSOR_TYPE_GYROSCOPE,
	.id_i2c		= GYRO_ID_SC7U22,
	.read_reg	= SC7U22_GYR_DATA_REG,
	.read_len	= 6,
	.id_reg		= SC7U22_CHIP_ID_REG,
	.id_data	= SC7U22_CHIP_ID,
	.precision	= 16,
	.ctrl_reg	= SC7U22_GYR_ODR_REG,
	.int_status_reg	= SENSOR_UNKNOW_DATA,
	.range		= { -32768, 32768 },
	.trig		= IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
	.active		= sensor_active,
	.init		= sensor_init,
	.report		= sensor_report_value,
};

static int gyro_sc7u22_probe(struct i2c_client *client,
				const struct i2c_device_id *devid)
{
	return sensor_register_device(client, NULL, devid, &gyro_sc7u22_ops);
}

static void gyro_sc7u22_remove(struct i2c_client *client)
{
	sensor_unregister_device(client, NULL, &gyro_sc7u22_ops);
}

static const struct i2c_device_id gyro_sc7u22_id[] = {
	{ "sc7u22_gyro", GYRO_ID_SC7U22 },
	{}
};

static struct i2c_driver gyro_sc7u22_driver = {
	.probe		= gyro_sc7u22_probe,
	.remove		= gyro_sc7u22_remove,
	.shutdown	= sensor_shutdown,
	.id_table	= gyro_sc7u22_id,
	.driver		= {
		.name	= "gyro_sc7u22",
#ifdef CONFIG_PM
		.pm	= &sensor_pm_ops,
#endif
	},
};

module_i2c_driver(gyro_sc7u22_driver);

MODULE_DESCRIPTION("SC7U22 gyroscope driver (reconstructed)");
MODULE_LICENSE("GPL");
