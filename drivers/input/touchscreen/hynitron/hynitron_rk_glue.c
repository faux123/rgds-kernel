// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip glue for the radxa Hynitron V3.0 driver: replaces the Allwinner
 * sys_config/init-input layer with the "hynitron,hyn_ts" DT node (the same
 * property names the driver's own DT parser and the stock kernel use).
 */
#include <linux/gpio.h>
#include <linux/of.h>
#include "hynitron_core.h"

static int hyn_glue_irq = -1;

int input_sensor_startup(int *type)
{
	struct device_node *np;
	u32 coords[4];

	np = of_find_compatible_node(NULL, NULL, "hynitron,hyn_ts");
	if (!np || !of_device_is_available(np)) {
		of_node_put(np);
		return -ENODEV;
	}
	config_info.ctp_used = 1;
	of_property_read_string(np, "hynitron,fw-name", &config_info.name);
	if (!of_property_read_u32_array(np, "hynitron,display-coords", coords, 4)) {
		config_info.screen_max_x = coords[2];
		config_info.screen_max_y = coords[3];
	} else {
		/* ctp_get_system_config() rejects 0; real limits come from pdata */
		config_info.screen_max_x = 1;
		config_info.screen_max_y = 1;
	}
	of_node_put(np);
	return 0;
}

int input_sensor_init(int *type)
{
	return 0;
}

void input_sensor_free(int *type)
{
}

int input_set_power_enable(int *type, int enable)
{
	return 0;
}

int input_set_int_enable(int *type, int enable)
{
	if (hyn_glue_irq < 0)
		return -EINVAL;
	if (enable)
		enable_irq(hyn_glue_irq);
	else
		disable_irq_nosync(hyn_glue_irq);
	return 0;
}

int input_request_int(int *type, irq_handler_t handler, unsigned long flags, void *data)
{
	int irq, ret;

	if (!hyn_ts_data || !hyn_ts_data->pdata || !gpio_is_valid(hyn_ts_data->pdata->irq_gpio))
		return -EINVAL;
	irq = gpio_to_irq(hyn_ts_data->pdata->irq_gpio);
	if (irq < 0)
		return irq;
	ret = request_threaded_irq(irq, NULL, handler, flags | IRQF_ONESHOT, "hyn_ts", data);
	if (ret)
		return ret;
	hyn_glue_irq = irq;
	config_info.irq_gpio.gpio = irq;
	return 0;
}
