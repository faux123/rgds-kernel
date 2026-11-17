// SPDX-License-Identifier: GPL-2.0
/*
 * Anbernic RG DS AW87391 speaker PA, left channel.
 *
 * Reconstructed from the stock RG DS kernel (stock.elf). Stock builds two
 * copies of the same source, aw_pa_left.c and aw_pa_right.c, which differ only
 * in their strings and compatibles; the i2c_driver and id table keep the
 * goodix names they were copied from. Addresses of this (left) copy:
 *   disable_pa_spk_left @ffffffc00914a5b4, enable_pa_spk_left @ffffffc00914a634,
 *   gtp_i2c_write @ffffffc00914a6f4, aw87391_probe @ffffffc00914a7d4,
 *   aw87391_remove @ffffffc00914a990, aw87391_kspk_reg @ffffffc00982e828.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

static u8 dev_init_done;
static struct i2c_client *pa_dev;

/* {reg, value} written in order on every speaker enable */
static u8 aw87391_kspk_reg[][2] = {
	{ 0x64, 0x3a },
	{ 0x02, 0x58 },
	{ 0x04, 0x45 },
	{ 0x05, 0x4e },
	{ 0x5d, 0x00 },
	{ 0x5e, 0xb4 },
	{ 0x5f, 0x30 },
	{ 0x60, 0x39 },
	{ 0x61, 0x10 },
	{ 0x62, 0x03 },
	{ 0x63, 0x7d },
	{ 0x65, 0xa0 },
	{ 0x66, 0x21 },
	{ 0x67, 0x41 },
	{ 0x68, 0x3b },
	{ 0x6e, 0x00 },
	{ 0x6f, 0x00 },
	{ 0x70, 0x00 },
	{ 0x71, 0x00 },
	{ 0x72, 0x34 },
	{ 0x73, 0x06 },
	{ 0x74, 0x10 },
	{ 0x75, 0x00 },
	{ 0x7a, 0x00 },
	{ 0x7b, 0x00 },
	{ 0x7c, 0x00 },
	{ 0x7d, 0x00 },
	{ 0x03, 0x00 },
	{ 0x01, 0x07 },
	{ 0x01, 0x3f },
	{ 0x03, 0x01 },
};

static int gtp_i2c_write(struct i2c_client *client, u8 *buf)
{
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
	int ret = -1;
	int retries;

	for (retries = 0; retries < 5; retries++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
	}
	return ret;
}

int disable_pa_spk_left(void)
{
	u8 buf[2];

	if (dev_init_done == 1) {
		printk(" disable_pa_spk_left \n");
		buf[0] = 0x01;
		buf[1] = 0x00;
		gtp_i2c_write(pa_dev, buf);
	}
	return -1;
}
EXPORT_SYMBOL(disable_pa_spk_left);

int enable_pa_spk_left(void)
{
	u8 buf[2];
	int i;

	if (dev_init_done == 1) {
		printk(" enable_pa_spk_left \n");
		for (i = 0; i < ARRAY_SIZE(aw87391_kspk_reg); i++) {
			buf[0] = aw87391_kspk_reg[i][0];
			buf[1] = aw87391_kspk_reg[i][1];
			gtp_i2c_write(pa_dev, buf);
		}
		for (i = 0; i < 50; i++)
			udelay(1000);
	}
	return -1;
}
EXPORT_SYMBOL(enable_pa_spk_left);

static int aw87391_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	u8 buf[2];
	int ret = -1;
	int retries;

	printk("%s -left() start\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	pa_dev = client;
	buf[0] = 0;
	buf[1] = 0;
	{
	struct i2c_msg msgs[2] = {
		{ .addr = client->addr, .flags = 0, .len = 1, .buf = &buf[0] },
		{ .addr = client->addr, .flags = I2C_M_RD, .len = 1, .buf = &buf[1] },
	};

	for (retries = 0; retries < 5; retries++) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
	}
	if (ret != 2) {
		printk("I2C Read: 0x%02X, %d bytes failed, errcode: %d! Process reset.", buf[1], 2, ret);
		if (ret < 0) {
			printk("AW_PA_LEFT read version failed");
			goto reset;
		}
	}
	if (buf[1] == 0xc1)
		dev_init_done = 1;
	printk("AW_PA_LEFT IC Version: %02x", buf[1]);
	}

reset:
	buf[0] = 0x00;
	buf[1] = 0xaa;
	udelay(5000);
	if (gtp_i2c_write(client, buf) < 0)
		printk("AW_PA_LEFT read version failed");
	return 0;
}

static void aw87391_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);
}

static const struct i2c_device_id goodix_ts_id[] = {
	{ "aw87391-left", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, goodix_ts_id);

static struct of_device_id aw87391_dt_ids[] = {	/* not const in stock: .data after the driver */
	{ .compatible = "aw,aw87391-left" },
	{ }
};

static struct i2c_driver goodix_ts_driver = {
	.probe = aw87391_probe,
	.remove = aw87391_remove,
	.id_table = goodix_ts_id,	/* stock: i2c_driver+0xc8 = goodix_ts_id */
	.driver = {
		.name = "aw87391-left",
		.of_match_table = aw87391_dt_ids,
	},
};


























































































































































































module_i2c_driver(goodix_ts_driver);

MODULE_LICENSE("GPL");
