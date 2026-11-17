// SPDX-License-Identifier: GPL-2.0
/*
 * Silan SC7U22 accelerometer, Rockchip sensor framework driver.
 *
 * Reconstructed from the stock binary of the Anbernic RG DS kernel
 * (Rockchip develop-6.1 BSP, Linux 6.1.141), which was distributed without
 * source. Register values and control flow follow the disassembly of
 * sc7u22_set_acc_odr, sc7u22_set_acc_range, sensor_active, sensor_init and
 * sensor_report_value; table contents come from the stock .data section.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sensor-dev.h>

#define SC7U22_CHIP_ID_REG	0x01
#define SC7U22_CHIP_ID		0x6a
#define SC7U22_ACC_DATA_REG	0x0c
#define SC7U22_ACC_ODR_REG	0x40
#define SC7U22_ACC_RANGE_REG	0x41
#define SC7U22_SOFT_INIT_REG	0x4a
#define SC7U22_SOFT_INIT_VAL	0xa5
#define SC7U22_PWR_REG		0x7d
#define SC7U22_PWR_ACC_EN	BIT(2)

/*
 * ACCEL_ID_SC7U22 comes from sensor-dev.h (see sensor-dev.h.snippet). In the
 * stock header it sits right after ACCEL_ID_ICM4260X and evaluates to 0x27.
 */

struct sc7u22_code {
	int val;
	u8 code;
};

struct sc7u22_code sc7u22_acc_odr[] = {
	{ 1, 0x2 }, { 3, 0x3 }, { 6, 0x4 }, { 12, 0x5 }, { 25, 0x6 },
	{ 50, 0x7 }, { 100, 0x8 }, { 200, 0x9 }, { 400, 0xa }, { 800, 0xb },
	{ 1600, 0xc },
};

struct sc7u22_code sc7u22_acc_range[] = {
	{ 2, 0x0 }, { 4, 0x1 }, { 8, 0x2 }, { 16, 0x3 },
};

/* Every register update is written twice, as the stock driver does. */
static void sc7u22_write_twice(struct i2c_client *client, int reg, int val)
{
	sensor_write_reg(client, reg, val);
	sensor_write_reg(client, reg, val);
}

/* Picks the first rate >= the request; a request above 1600 Hz is ignored. */
void sc7u22_set_acc_odr(struct i2c_client *client, int rate)
{
	int i, val;

	val = sensor_read_reg(client, SC7U22_ACC_ODR_REG);
	for (i = 0; i < ARRAY_SIZE(sc7u22_acc_odr); i++)
		if (sc7u22_acc_odr[i].val >= rate)
			break;
	if (i == ARRAY_SIZE(sc7u22_acc_odr))
		return;

	val = (val & 0xf0) | sc7u22_acc_odr[i].code;
	sc7u22_write_twice(client, SC7U22_ACC_ODR_REG, val);
}

/* Exact match only; an unknown range leaves the register untouched. */
void sc7u22_set_acc_range(struct i2c_client *client, int range)
{
	int i, val;

	for (i = 0; i < ARRAY_SIZE(sc7u22_acc_range); i++)
		if (sc7u22_acc_range[i].val == range)
			break;
	if (i == ARRAY_SIZE(sc7u22_acc_range))
		return;

	val = sensor_read_reg(client, SC7U22_ACC_RANGE_REG);
	val = (val & 0xfc) | sc7u22_acc_range[i].code;
	sc7u22_write_twice(client, SC7U22_ACC_RANGE_REG, val);
}

static int sensor_active(struct i2c_client *client, int enable, int rate)
{
	int val;

	dev_err(&client->dev, "sc7u22 acc enable:%d,rate:%d\n", enable, rate);
	sc7u22_set_acc_odr(client, rate);

	val = sensor_read_reg(client, SC7U22_PWR_REG) & ~SC7U22_PWR_ACC_EN & 0xff;
	if (enable)
		val |= SC7U22_PWR_ACC_EN;
	sc7u22_write_twice(client, SC7U22_PWR_REG, val);
	dev_err(&client->dev, "sc7u22 acc write reg:0x%x\n", val);

	return 0;
}

static int sensor_init(struct i2c_client *client)
{
	struct sensor_private_data *sensor = i2c_get_clientdata(client);

	sensor->status_cur = SENSOR_OFF;

	sc7u22_write_twice(client, SC7U22_SOFT_INIT_REG, SC7U22_SOFT_INIT_VAL);
	msleep(2);

	sc7u22_set_acc_range(client, 2);
	sc7u22_set_acc_odr(client, 100);

	return 0;
}

static int sensor_report_value(struct i2c_client *client)
{
	struct sensor_private_data *sensor = i2c_get_clientdata(client);
	struct sensor_platform_data *pdata = sensor->pdata;
	struct sensor_axis axis;
	u8 buf[6] = { 0 };
	s16 x, y, z;
	u8 i, reg;	/* u8 counter and register, as stock (cmp ..., uxtb) */

	if (sensor->ops->read_len < 6) {
		dev_err(&client->dev, "%s:length is error,len=%d\n", __func__,
			sensor->ops->read_len);
		return -1;
	}

	/* Byte-wise reads, one register per call, as in the stock driver. */
	reg = sensor->ops->read_reg;
	for (i = 0; i < sensor->ops->read_len; i++)
		buf[i] = sensor_read_reg(client, reg++);

	x = (s16)((buf[0] << 8) | buf[1]);
	y = (s16)((buf[2] << 8) | buf[3]);
	z = (s16)((buf[4] << 8) | buf[5]);

	axis.x = pdata->orientation[0] * x + pdata->orientation[1] * y +
		 pdata->orientation[2] * z;
	axis.y = pdata->orientation[3] * x + pdata->orientation[4] * y +
		 pdata->orientation[5] * z;
	axis.z = pdata->orientation[6] * x + pdata->orientation[7] * y +
		 pdata->orientation[8] * z;

	if (sensor->status_cur == SENSOR_ON) {
		input_report_abs(sensor->input_dev, ABS_X, axis.x);
		input_report_abs(sensor->input_dev, ABS_Y, axis.y);
		input_report_abs(sensor->input_dev, ABS_Z, axis.z);
		input_sync(sensor->input_dev);
	}

	mutex_lock(&sensor->data_mutex);
	sensor->axis = axis;
	mutex_unlock(&sensor->data_mutex);

	if (pdata->irq_enable)
		sensor_read_reg(client, sensor->ops->int_status_reg);

	return 0;
}

struct sensor_operate gsensor_sc7u22_ops = {
	.name		= "sc7u22_acc",
	.type		= SENSOR_TYPE_ACCEL,
	.id_i2c		= ACCEL_ID_SC7U22,
	.read_reg	= SC7U22_ACC_DATA_REG,
	.read_len	= 6,
	.id_reg		= SC7U22_CHIP_ID_REG,
	.id_data	= SC7U22_CHIP_ID,
	.precision	= 16,
	.ctrl_reg	= SC7U22_ACC_ODR_REG,
	/* .int_status_reg left 0: stock ops+0x40 = 0 */
	.range		= { -32768, 32768 },
	.trig		= IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
	.active		= sensor_active,
	.init		= sensor_init,
	.report		= sensor_report_value,
};

static int gsensor_sc7u22_probe(struct i2c_client *client,
				const struct i2c_device_id *devid)
{
	return sensor_register_device(client, NULL, devid, &gsensor_sc7u22_ops);
}

static void gsensor_sc7u22_remove(struct i2c_client *client)
{
	sensor_unregister_device(client, NULL, &gsensor_sc7u22_ops);
}

static const struct i2c_device_id gsensor_sc7u22_id[] = {
	{ "sc7u22_acc", ACCEL_ID_SC7U22 },
	{}
};

static struct i2c_driver gsensor_sc7u22_driver = {
	.probe		= gsensor_sc7u22_probe,
	.remove		= gsensor_sc7u22_remove,
	.shutdown	= sensor_shutdown,
	.id_table	= gsensor_sc7u22_id,
	.driver		= {
		.name	= "gsensor_sc7u22",
#ifdef CONFIG_PM
		.pm	= &sensor_pm_ops,
#endif
	},
};

module_i2c_driver(gsensor_sc7u22_driver);

MODULE_DESCRIPTION("SC7U22 accelerometer driver (reconstructed)");
MODULE_LICENSE("GPL");
