/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip DT/IRQ stand-in for the Allwinner ctp "init-input.h" API that the
 * radxa Hynitron V3.0 driver was written against. Only what the driver uses.
 */
#ifndef __HYNITRON_RK_GLUE_H
#define __HYNITRON_RK_GLUE_H

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/types.h>

#define CTP_TYPE 0

struct hyn_glue_gpio {
	int gpio;
};

struct ctp_config_info {
	int input_type;
	const char *name;
	int int_number;
	struct device *dev;
	int ctp_used;
	u32 twi_id;
	int screen_max_x;
	int screen_max_y;
	int revert_x_flag;
	int revert_y_flag;
	int exchange_x_y_flag;
	struct hyn_glue_gpio irq_gpio;
	struct hyn_glue_gpio wakeup_gpio;
};

extern struct ctp_config_info config_info;

int input_sensor_startup(int *type);
int input_sensor_init(int *type);
void input_sensor_free(int *type);
int input_set_power_enable(int *type, int enable);
int input_set_int_enable(int *type, int enable);
int input_request_int(int *type, irq_handler_t handler, unsigned long flags, void *data);

#endif
