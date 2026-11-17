// SPDX-License-Identifier: GPL-2.0
/*
 * Reconstruction of the Anbernic RG DS stock singleadc-joypad sysfs layer,
 * from stock.elf disassembly. Skeleton merge into gamma_rg_ds_kernel_singleadcjoy.c pending.
 *
 * Simple-int family template, verified on:
 *   remapkey_show_a_x  @ffffffc008c7216c / remapkey_store_a_x  @ffffffc008c721a0 (var +0x730 = remapkey_a_x @ffffffc00a4d5730)
 *   remapkey_show_b_x  @ffffffc008c721e4 (var +0x738)
 *   jokstick_show_lxl  @ffffffc008c7432c / jokstick_store_lxl  @ffffffc008c74360 (var +0x83c = jokstick_cal_lxl @ffffffc00a4d583c)
 *   joypad_show_touch_mode @ffffffc008c71f14 / joypad_store_touch_mode @ffffffc008c71f48 (var +0xa98)
 * show: sprintf(buf, "%d\n", var) [fmt @ffffffc009b8b4ae reads "%d\n"], return (ssize_t)ret.
 * store: var = simple_strtoul(buf, NULL, 10); return count.  No locking, no range check.
 * Variables are global (nm type T/D), not static.
 * joypad_show_touch_mode loads its fmt from @ffffffc009cb008d, which reads
 * "gt9xx_touch_swap" in this ELF: UNRESOLVED, do not assume "%d\n" there.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio_keys.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/property.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>

#define STOCK_INT_SHOW(fn, var) \
static ssize_t fn(struct device *dev, struct device_attribute *attr, char *buf) \
{ return sprintf(buf, "%d\n", var); }
#define STOCK_INT_STORE(fn, var) \
static ssize_t fn(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) \
{ var = simple_strtoul(buf, NULL, 10); return count; }

/* remapkey_<k>: show/store pair, attr "remapkey_<k>", mode 0644 (all 62 per dev_attr table) */
#define REMAPKEY(k) int remapkey_##k; \
	STOCK_INT_SHOW(remapkey_show_##k, remapkey_##k) \
	STOCK_INT_STORE(remapkey_store_##k, remapkey_##k) \
	static DEVICE_ATTR(remapkey_##k, 0644, remapkey_show_##k, remapkey_store_##k);
/* jokstick_cal_<k>: functions jokstick_show_<k>/jokstick_store_<k>, attr "jokstick_cal_<k>", 0644 */
#define JOKSTICK(k) int jokstick_cal_##k; \
	STOCK_INT_SHOW(jokstick_show_##k, jokstick_cal_##k) \
	STOCK_INT_STORE(jokstick_store_##k, jokstick_cal_##k) \
	static DEVICE_ATTR(jokstick_cal_##k, 0644, jokstick_show_##k, jokstick_store_##k);

/* Full per-attribute table (name, mode, show, store) read from stock dev_attr_* structs: */
/* see stock_sysfs_attrs.generated.h; instantiation list to be generated from it. */

/*
 * joypad_* family. x0 = dev; [dev+0x78] = dev->driver_data (assumed struct device layout, 6.1 arm64).
 * drvdata+0x48 = adc_deadzone (int)        joypad_show/store_adc_deadzone @ffffffc008c71b20/@..71b58
 * drvdata+0x20 = pointer to per-channel array, stride 0x64:
 *   adc_lx  ch[0]+0x4,  adc_lx_p ch[0]+0x8   @..71c5c/@..71dac
 *   adc_ly  +0x68,      adc_ly_p +0x6c       @..71c98/@..71de8
 *   adc_rx  +0xcc,      adc_rx_p +0xd0       @..71cd4/@..71e24
 *   adc_ry  +0x130,     adc_ry_p +0x134      @..71d10/@..71e60
 *   adc_z, adc_rz: print constant 0 (mov w2, wzr) @..71d4c/@..71d7c
 *   adc_z_p, adc_rz_p: print ch[0]+0x8 (same field as adc_lx_p; stock quirk) @..71e9c/@..71ed8
 * Field names of the channel struct are UNKNOWN; offsets used raw until the probe is rebuilt.
 */
extern int hall_status_share;          /* T @ffffffc00a4d56ec */
int factory_led_status;                /* T @ffffffc00a4d5a94 */
int gt9xx_touch_swap_status;           /* T @ffffffc00a4d5a98 */
int gt9xx_touch_combo_status;          /* T @ffffffc00a4d5a9c */

#define DRV(dev) ((char *)dev_get_drvdata(dev))
#define CH_INT(dev, off) (*(int *)(*(char **)(DRV(dev) + 0x20) + (off)))

static ssize_t joypad_show_adc_deadzone(struct device *dev, struct device_attribute *attr, char *buf)
{ return sprintf(buf, "%d\n", *(int *)(DRV(dev) + 0x48)); }
static ssize_t joypad_store_adc_deadzone(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ *(int *)(DRV(dev) + 0x48) = simple_strtoul(buf, NULL, 10); return count; }
#define ADC_SHOW(fn, off) static ssize_t fn(struct device *dev, struct device_attribute *attr, char *buf) \
{ return sprintf(buf, "%d\n", CH_INT(dev, off)); }
ADC_SHOW(joypad_show_adc_lx, 0x4)    ADC_SHOW(joypad_show_adc_lx_p, 0x8)
ADC_SHOW(joypad_show_adc_ly, 0x68)   ADC_SHOW(joypad_show_adc_ly_p, 0x6c)
ADC_SHOW(joypad_show_adc_rx, 0xcc)   ADC_SHOW(joypad_show_adc_rx_p, 0xd0)
ADC_SHOW(joypad_show_adc_ry, 0x130)  ADC_SHOW(joypad_show_adc_ry_p, 0x134)
ADC_SHOW(joypad_show_adc_z_p, 0x8)   ADC_SHOW(joypad_show_adc_rz_p, 0x8)
static ssize_t joypad_show_adc_z(struct device *dev, struct device_attribute *attr, char *buf)
{ return sprintf(buf, "%d\n", 0); }
static ssize_t joypad_show_adc_rz(struct device *dev, struct device_attribute *attr, char *buf)
{ return sprintf(buf, "%d\n", 0); }

/* @ffffffc008c6bd34; store @..6bd68 parses with simple_strtoul and discards the result */
STOCK_INT_SHOW(joypad_show_hall_status, hall_status_share)
static ssize_t joypad_store_hall_status(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ simple_strtoul(buf, NULL, 10); return count; }
/* @ffffffc008c74e6c / @..74ea0 */
STOCK_INT_SHOW(joypad_show_factory_led, factory_led_status)
STOCK_INT_STORE(joypad_store_factory_led, factory_led_status)
/* fmt @ffffffc009cb008d = "gt9xx_touch_swap_status %d\n"; @..71f14 / @..71f48 */
static ssize_t joypad_show_touch_mode(struct device *dev, struct device_attribute *attr, char *buf)
{ return sprintf(buf, "gt9xx_touch_swap_status %d\n", gt9xx_touch_swap_status); }
STOCK_INT_STORE(joypad_store_touch_mode, gt9xx_touch_swap_status)
/* fmt @ffffffc009cbeee7 = "gt9xx_touch_combo_status %d\n"; @..71f8c */
static ssize_t joypad_show_touch_mode_combo(struct device *dev, struct device_attribute *attr, char *buf)
{ return sprintf(buf, "gt9xx_touch_combo_status %d\n", gt9xx_touch_combo_status); }
STOCK_INT_STORE(joypad_store_touch_mode_combo, gt9xx_touch_combo_status)

#define JATTR(n, sh, st) static DEVICE_ATTR(n, 0644, sh, st);
JATTR(adc_deadzone, joypad_show_adc_deadzone, joypad_store_adc_deadzone)
JATTR(adc_lx, joypad_show_adc_lx, NULL) JATTR(adc_lx_p, joypad_show_adc_lx_p, NULL)
JATTR(adc_ly, joypad_show_adc_ly, NULL) JATTR(adc_ly_p, joypad_show_adc_ly_p, NULL)
JATTR(adc_rx, joypad_show_adc_rx, NULL) JATTR(adc_rx_p, joypad_show_adc_rx_p, NULL)
JATTR(adc_ry, joypad_show_adc_ry, NULL) JATTR(adc_ry_p, joypad_show_adc_ry_p, NULL)
JATTR(adc_z, joypad_show_adc_z, NULL)   JATTR(adc_z_p, joypad_show_adc_z_p, NULL)
JATTR(adc_rz, joypad_show_adc_rz, NULL) JATTR(adc_rz_p, joypad_show_adc_rz_p, NULL)
JATTR(hall_status, joypad_show_hall_status, joypad_store_hall_status)
JATTR(factory_led, joypad_show_factory_led, joypad_store_factory_led)
JATTR(touch_mode, joypad_show_touch_mode, joypad_store_touch_mode)
JATTR(touch_mode_combo, joypad_show_touch_mode_combo, joypad_store_touch_mode_combo)

/* ---- Generated: one line per stock attribute, in stock .data order (dev_attr_* addresses) ---- */
/* joypad family, hand-written below: hall_status @ffffffc00a2a3a40 */
/* joypad family, hand-written below: adc_deadzone @ffffffc00a2a4120 */
/* joypad family, hand-written below: adc_lx @ffffffc00a2a4160 */
/* joypad family, hand-written below: adc_ly @ffffffc00a2a4180 */
/* joypad family, hand-written below: adc_rx @ffffffc00a2a41a0 */
/* joypad family, hand-written below: adc_ry @ffffffc00a2a41c0 */
/* joypad family, hand-written below: adc_z @ffffffc00a2a41e0 */
/* joypad family, hand-written below: adc_rz @ffffffc00a2a4200 */
/* joypad family, hand-written below: adc_lx_p @ffffffc00a2a4220 */
/* joypad family, hand-written below: adc_ly_p @ffffffc00a2a4240 */
/* joypad family, hand-written below: adc_rx_p @ffffffc00a2a4260 */
/* joypad family, hand-written below: adc_ry_p @ffffffc00a2a4280 */
/* joypad family, hand-written below: adc_z_p @ffffffc00a2a42a0 */
/* joypad family, hand-written below: adc_rz_p @ffffffc00a2a42c0 */
/* joypad family, hand-written below: touch_mode @ffffffc00a2a42e0 */
/* joypad family, hand-written below: touch_mode_combo @ffffffc00a2a4300 */
REMAPKEY(switch)	/* @ffffffc00a2a4320 */
REMAPKEY(x_x)	/* @ffffffc00a2a4340 */
REMAPKEY(y_x)	/* @ffffffc00a2a4360 */
REMAPKEY(a_x)	/* @ffffffc00a2a4380 */
REMAPKEY(b_x)	/* @ffffffc00a2a43a0 */
REMAPKEY(up_x)	/* @ffffffc00a2a43c0 */
REMAPKEY(down_x)	/* @ffffffc00a2a43e0 */
REMAPKEY(left_x)	/* @ffffffc00a2a4400 */
REMAPKEY(right_x)	/* @ffffffc00a2a4420 */
REMAPKEY(select_x)	/* @ffffffc00a2a4440 */
REMAPKEY(start_x)	/* @ffffffc00a2a4460 */
REMAPKEY(l1_x)	/* @ffffffc00a2a4480 */
REMAPKEY(l2_x)	/* @ffffffc00a2a44a0 */
REMAPKEY(r1_x)	/* @ffffffc00a2a44c0 */
REMAPKEY(r2_x)	/* @ffffffc00a2a44e0 */
REMAPKEY(left_joy_key_x)	/* @ffffffc00a2a4500 */
REMAPKEY(left_joy_mid_x)	/* @ffffffc00a2a4520 */
REMAPKEY(left_joy_lenght_x)	/* @ffffffc00a2a4540 */
REMAPKEY(right_joy_key_x)	/* @ffffffc00a2a4560 */
REMAPKEY(right_joy_mid_x)	/* @ffffffc00a2a4580 */
REMAPKEY(right_joy_lenght_x)	/* @ffffffc00a2a45a0 */
REMAPKEY(x_y)	/* @ffffffc00a2a45c0 */
REMAPKEY(y_y)	/* @ffffffc00a2a45e0 */
REMAPKEY(a_y)	/* @ffffffc00a2a4600 */
REMAPKEY(b_y)	/* @ffffffc00a2a4620 */
REMAPKEY(up_y)	/* @ffffffc00a2a4640 */
REMAPKEY(down_y)	/* @ffffffc00a2a4660 */
REMAPKEY(left_y)	/* @ffffffc00a2a4680 */
REMAPKEY(right_y)	/* @ffffffc00a2a46a0 */
REMAPKEY(select_y)	/* @ffffffc00a2a46c0 */
REMAPKEY(start_y)	/* @ffffffc00a2a46e0 */
REMAPKEY(l1_y)	/* @ffffffc00a2a4700 */
REMAPKEY(l2_y)	/* @ffffffc00a2a4720 */
REMAPKEY(r1_y)	/* @ffffffc00a2a4740 */
REMAPKEY(r2_y)	/* @ffffffc00a2a4760 */
REMAPKEY(left_joy_key_y)	/* @ffffffc00a2a4780 */
REMAPKEY(left_joy_mid_y)	/* @ffffffc00a2a47a0 */
REMAPKEY(left_joy_lenght_y)	/* @ffffffc00a2a47c0 */
REMAPKEY(right_joy_key_y)	/* @ffffffc00a2a47e0 */
REMAPKEY(right_joy_mid_y)	/* @ffffffc00a2a4800 */
REMAPKEY(right_joy_lenght_y)	/* @ffffffc00a2a4820 */
REMAPKEY(x_mode)	/* @ffffffc00a2a4840 */
REMAPKEY(y_mode)	/* @ffffffc00a2a4860 */
REMAPKEY(a_mode)	/* @ffffffc00a2a4880 */
REMAPKEY(b_mode)	/* @ffffffc00a2a48a0 */
REMAPKEY(up_mode)	/* @ffffffc00a2a48c0 */
REMAPKEY(down_mode)	/* @ffffffc00a2a48e0 */
REMAPKEY(left_mode)	/* @ffffffc00a2a4900 */
REMAPKEY(right_mode)	/* @ffffffc00a2a4920 */
REMAPKEY(select_mode)	/* @ffffffc00a2a4940 */
REMAPKEY(start_mode)	/* @ffffffc00a2a4960 */
REMAPKEY(l1_mode)	/* @ffffffc00a2a4980 */
REMAPKEY(l2_mode)	/* @ffffffc00a2a49a0 */
REMAPKEY(r1_mode)	/* @ffffffc00a2a49c0 */
REMAPKEY(r2_mode)	/* @ffffffc00a2a49e0 */
REMAPKEY(left_joy_mid_mode)	/* @ffffffc00a2a4a00 */
REMAPKEY(right_joy_mid_mode)	/* @ffffffc00a2a4a20 */
REMAPKEY(left_joy_mode)	/* @ffffffc00a2a4a40 */
REMAPKEY(right_joy_mode)	/* @ffffffc00a2a4a60 */
REMAPKEY(xbox_switch)	/* @ffffffc00a2a4a80 */
REMAPKEY(xbox_switch_bt_tr)	/* @ffffffc00a2a4aa0 */
REMAPKEY(tr_mode)	/* @ffffffc00a2a4ac0 */
JOKSTICK(switch_l)	/* @ffffffc00a2a4ae0 */
JOKSTICK(switch_r)	/* @ffffffc00a2a4b00 */
JOKSTICK(scale)	/* @ffffffc00a2a4b20 */
JOKSTICK(scale_auto)	/* @ffffffc00a2a4b40 */
JOKSTICK(deadzone)	/* @ffffffc00a2a4b60 */
JOKSTICK(lxll)	/* @ffffffc00a2a4b80 */
JOKSTICK(lxlr)	/* @ffffffc00a2a4ba0 */
JOKSTICK(lxrl)	/* @ffffffc00a2a4bc0 */
JOKSTICK(lxrr)	/* @ffffffc00a2a4be0 */
JOKSTICK(lyuu)	/* @ffffffc00a2a4c00 */
JOKSTICK(lyud)	/* @ffffffc00a2a4c20 */
JOKSTICK(lydu)	/* @ffffffc00a2a4c40 */
JOKSTICK(lydd)	/* @ffffffc00a2a4c60 */
JOKSTICK(lxl)	/* @ffffffc00a2a4c80 */
JOKSTICK(lxl_scale)	/* @ffffffc00a2a4ca0 */
JOKSTICK(lxr)	/* @ffffffc00a2a4cc0 */
JOKSTICK(lxr_scale)	/* @ffffffc00a2a4ce0 */
JOKSTICK(lyu)	/* @ffffffc00a2a4d00 */
JOKSTICK(lyu_scale)	/* @ffffffc00a2a4d20 */
JOKSTICK(lyd)	/* @ffffffc00a2a4d40 */
JOKSTICK(lyd_scale)	/* @ffffffc00a2a4d60 */
JOKSTICK(rxll)	/* @ffffffc00a2a4d80 */
JOKSTICK(rxlr)	/* @ffffffc00a2a4da0 */
JOKSTICK(rxrl)	/* @ffffffc00a2a4dc0 */
JOKSTICK(rxrr)	/* @ffffffc00a2a4de0 */
JOKSTICK(ryuu)	/* @ffffffc00a2a4e00 */
JOKSTICK(ryud)	/* @ffffffc00a2a4e20 */
JOKSTICK(rydu)	/* @ffffffc00a2a4e40 */
JOKSTICK(rydd)	/* @ffffffc00a2a4e60 */
JOKSTICK(rxl)	/* @ffffffc00a2a4e80 */
JOKSTICK(rxl_scale)	/* @ffffffc00a2a4ea0 */
JOKSTICK(rxr)	/* @ffffffc00a2a4ec0 */
JOKSTICK(rxr_scale)	/* @ffffffc00a2a4ee0 */
JOKSTICK(ryu)	/* @ffffffc00a2a4f00 */
JOKSTICK(ryu_scale)	/* @ffffffc00a2a4f20 */
JOKSTICK(ryd)	/* @ffffffc00a2a4f40 */
JOKSTICK(ryd_scale)	/* @ffffffc00a2a4f60 */
/* joypad family, hand-written below: factory_led @ffffffc00a2a4f80 */

static struct attribute *joypad_stock_attrs[] = {
	&dev_attr_hall_status.attr,
	&dev_attr_adc_deadzone.attr,
	&dev_attr_adc_lx.attr,
	&dev_attr_adc_ly.attr,
	&dev_attr_adc_rx.attr,
	&dev_attr_adc_ry.attr,
	&dev_attr_adc_z.attr,
	&dev_attr_adc_rz.attr,
	&dev_attr_adc_lx_p.attr,
	&dev_attr_adc_ly_p.attr,
	&dev_attr_adc_rx_p.attr,
	&dev_attr_adc_ry_p.attr,
	&dev_attr_adc_z_p.attr,
	&dev_attr_adc_rz_p.attr,
	&dev_attr_touch_mode.attr,
	&dev_attr_touch_mode_combo.attr,
	&dev_attr_remapkey_switch.attr,
	&dev_attr_remapkey_x_x.attr,
	&dev_attr_remapkey_y_x.attr,
	&dev_attr_remapkey_a_x.attr,
	&dev_attr_remapkey_b_x.attr,
	&dev_attr_remapkey_up_x.attr,
	&dev_attr_remapkey_down_x.attr,
	&dev_attr_remapkey_left_x.attr,
	&dev_attr_remapkey_right_x.attr,
	&dev_attr_remapkey_select_x.attr,
	&dev_attr_remapkey_start_x.attr,
	&dev_attr_remapkey_l1_x.attr,
	&dev_attr_remapkey_l2_x.attr,
	&dev_attr_remapkey_r1_x.attr,
	&dev_attr_remapkey_r2_x.attr,
	&dev_attr_remapkey_left_joy_key_x.attr,
	&dev_attr_remapkey_left_joy_mid_x.attr,
	&dev_attr_remapkey_left_joy_lenght_x.attr,
	&dev_attr_remapkey_right_joy_key_x.attr,
	&dev_attr_remapkey_right_joy_mid_x.attr,
	&dev_attr_remapkey_right_joy_lenght_x.attr,
	&dev_attr_remapkey_x_y.attr,
	&dev_attr_remapkey_y_y.attr,
	&dev_attr_remapkey_a_y.attr,
	&dev_attr_remapkey_b_y.attr,
	&dev_attr_remapkey_up_y.attr,
	&dev_attr_remapkey_down_y.attr,
	&dev_attr_remapkey_left_y.attr,
	&dev_attr_remapkey_right_y.attr,
	&dev_attr_remapkey_select_y.attr,
	&dev_attr_remapkey_start_y.attr,
	&dev_attr_remapkey_l1_y.attr,
	&dev_attr_remapkey_l2_y.attr,
	&dev_attr_remapkey_r1_y.attr,
	&dev_attr_remapkey_r2_y.attr,
	&dev_attr_remapkey_left_joy_key_y.attr,
	&dev_attr_remapkey_left_joy_mid_y.attr,
	&dev_attr_remapkey_left_joy_lenght_y.attr,
	&dev_attr_remapkey_right_joy_key_y.attr,
	&dev_attr_remapkey_right_joy_mid_y.attr,
	&dev_attr_remapkey_right_joy_lenght_y.attr,
	&dev_attr_remapkey_x_mode.attr,
	&dev_attr_remapkey_y_mode.attr,
	&dev_attr_remapkey_a_mode.attr,
	&dev_attr_remapkey_b_mode.attr,
	&dev_attr_remapkey_up_mode.attr,
	&dev_attr_remapkey_down_mode.attr,
	&dev_attr_remapkey_left_mode.attr,
	&dev_attr_remapkey_right_mode.attr,
	&dev_attr_remapkey_select_mode.attr,
	&dev_attr_remapkey_start_mode.attr,
	&dev_attr_remapkey_l1_mode.attr,
	&dev_attr_remapkey_l2_mode.attr,
	&dev_attr_remapkey_r1_mode.attr,
	&dev_attr_remapkey_r2_mode.attr,
	&dev_attr_remapkey_left_joy_mid_mode.attr,
	&dev_attr_remapkey_right_joy_mid_mode.attr,
	&dev_attr_remapkey_left_joy_mode.attr,
	&dev_attr_remapkey_right_joy_mode.attr,
	&dev_attr_remapkey_xbox_switch.attr,
	&dev_attr_remapkey_xbox_switch_bt_tr.attr,
	&dev_attr_remapkey_tr_mode.attr,
	&dev_attr_jokstick_cal_switch_l.attr,
	&dev_attr_jokstick_cal_switch_r.attr,
	&dev_attr_jokstick_cal_scale.attr,
	&dev_attr_jokstick_cal_scale_auto.attr,
	&dev_attr_jokstick_cal_deadzone.attr,
	&dev_attr_jokstick_cal_lxll.attr,
	&dev_attr_jokstick_cal_lxlr.attr,
	&dev_attr_jokstick_cal_lxrl.attr,
	&dev_attr_jokstick_cal_lxrr.attr,
	&dev_attr_jokstick_cal_lyuu.attr,
	&dev_attr_jokstick_cal_lyud.attr,
	&dev_attr_jokstick_cal_lydu.attr,
	&dev_attr_jokstick_cal_lydd.attr,
	&dev_attr_jokstick_cal_lxl.attr,
	&dev_attr_jokstick_cal_lxl_scale.attr,
	&dev_attr_jokstick_cal_lxr.attr,
	&dev_attr_jokstick_cal_lxr_scale.attr,
	&dev_attr_jokstick_cal_lyu.attr,
	&dev_attr_jokstick_cal_lyu_scale.attr,
	&dev_attr_jokstick_cal_lyd.attr,
	&dev_attr_jokstick_cal_lyd_scale.attr,
	&dev_attr_jokstick_cal_rxll.attr,
	&dev_attr_jokstick_cal_rxlr.attr,
	&dev_attr_jokstick_cal_rxrl.attr,
	&dev_attr_jokstick_cal_rxrr.attr,
	&dev_attr_jokstick_cal_ryuu.attr,
	&dev_attr_jokstick_cal_ryud.attr,
	&dev_attr_jokstick_cal_rydu.attr,
	&dev_attr_jokstick_cal_rydd.attr,
	&dev_attr_jokstick_cal_rxl.attr,
	&dev_attr_jokstick_cal_rxl_scale.attr,
	&dev_attr_jokstick_cal_rxr.attr,
	&dev_attr_jokstick_cal_rxr_scale.attr,
	&dev_attr_jokstick_cal_ryu.attr,
	&dev_attr_jokstick_cal_ryu_scale.attr,
	&dev_attr_jokstick_cal_ryd.attr,
	&dev_attr_jokstick_cal_ryd_scale.attr,
	&dev_attr_factory_led.attr,
	NULL,
};
static const struct attribute_group joypad_stock_attr_group = { .attrs = joypad_stock_attrs };

/* ---- State variables (global, nm type T). Initialized ones live in .data @ffffffc00a2a3b8c.. (values read from ELF);
 * the rest are .bss @ffffffc00a4d5aa4... 4-byte spacing between neighbors => int. Width of each still to be confirmed
 * against its ldr/str when the users are rebuilt. ---- */
int joypad_init_temp = 1;      /* @ffffffc00a2a3b8c */
int joypad_count_temp = 10;    /* @..3b90 */
int joypadxy_init = 1;         /* @..3b94 */
int joypadxy_count = 5;        /* @..3b98 */
int joypadz_init_temp = 1;     /* @..3b9c */
int joypadz_count_temp = 10;   /* @..3ba0 */
int joypadz_init = 1;          /* @..3ba4 */
int joypadz_count = 10;        /* @..3ba8 */
int abs_z_mode2 = 1;           /* @..3bac */
int joypadx_temp, joypady_temp;                        /* @ffffffc00a4d5aa4, @..5aa8 */
int joypadx, joypady;                                  /* @..5ab0, @..5ab4 */
int joypadz_temp, joypadrz_temp;                       /* @..5abc, @..5ac0 */
int joypadz_temp_mode2, joypadrz_temp_mode2;           /* @..5ac8, @..5acc */
int joypadz, joypadrz;                                 /* @..5ad0, @..5ad4 */
int abs_z_mode2_mask, abs_z_mode2_x, abs_z_mode2_y;    /* @..5ad8, @..5adc, @..5ae0 */
int abs_z_mode2_x_base, abs_z_mode2_y_base;            /* @..5ae4, @..5ae8 */
int abs_value_x_value, abs_value_x_value_old;          /* @..5af8, @..5afc */
int abs_value_y_value, abs_value_y_value_old;          /* @..5b00, @..5b04 */
int count_joypad_data;                                 /* @..5b90 */
int adc_abs_lx, adc_abs_ly, adc_abs_rx, adc_abs_ry;    /* @..5bac..@..5bb8 */
/* joypadz_count_mode2 is a function-local static of joypad2touchpanel_abs_z_mode2 (@ffffffc00a2a3bb0, .data). */

/*
 * joypad_adc_read @ffffffc008c71a14. Arg0 = amux/gpio block: +0x0 iio_channel*, +0x8 sel-A gpio, +0xc sel-B gpio,
 * +0x10 enable gpio. Arg1 = channel descriptor, int at +0x58 = amux index.
 * Tables @ffffffc009d024e8 = {1,1,0,0} (sel-A), @ffffffc009d02208 = {1,0,0,1} (sel-B).
 * Scale: (raw << 15) / 1800, from smull by 0x91a2b3c5 + add + asr #10 @..71ab8..71ad4 (== /1800, signed 32-bit).
 * usleep_range_state(10, 20, TASK_UNINTERRUPTIBLE=2) @..71a98.
 * Struct type names are placeholders; offsets are stock.
 */
struct stock_amux { struct iio_channel *chan; int sel_a_gpio; int sel_b_gpio; int en_gpio; };
static const int stock_amux_sel_a[4] = { 1, 1, 0, 0 };
static const int stock_amux_sel_b[4] = { 1, 0, 0, 1 };
static int joypad_adc_read(struct stock_amux *amux, void *button)
{
	int val = 0;
	int idx = *(int *)((char *)button + 0x58);

	gpiod_set_raw_value(gpio_to_desc(amux->en_gpio), 0);
	if ((unsigned int)idx >= 4) {
		gpiod_set_raw_value(gpio_to_desc(amux->en_gpio), 1);
		return 0;
	}
	gpiod_set_raw_value(gpio_to_desc(amux->sel_a_gpio), stock_amux_sel_a[idx]);
	gpiod_set_raw_value(gpio_to_desc(amux->sel_b_gpio), stock_amux_sel_b[idx]);
	usleep_range(10, 20);
	iio_read_channel_processed(amux->chan, &val);	/* return value ignored in stock */
	return (val << 15) / 1800;
}

/* joypad_suspend @ffffffc008c7695c and joypad_resume @ffffffc008c7699c are byte-identical:
 * lock mutex at drvdata+0x50, store byte 0 at drvdata+0xc, unlock, return 0. (Resume also writes 0: stock quirk.) */
static int joypad_suspend(struct device *dev)
{
	char *j = dev_get_drvdata(dev);
	mutex_lock((struct mutex *)(j + 0x50));
	*(u8 *)(j + 0xc) = 0;
	mutex_unlock((struct mutex *)(j + 0x50));
	return 0;
}
static int joypad_resume(struct device *dev)
{
	char *j = dev_get_drvdata(dev);
	mutex_lock((struct mutex *)(j + 0x50));
	*(u8 *)(j + 0xc) = 0;
	mutex_unlock((struct mutex *)(j + 0x50));
	return 0;
}
/* joypad_pm_ops @ffffffc009719780: only slots 2,3 (.suspend, .resume) non-zero. */
static const struct dev_pm_ops joypad_pm_ops = {
	.suspend = joypad_suspend,
	.resume = joypad_resume,
};

/* platform_driver @ffffffc00a2a3bb8 (contents not yet decoded: of_match table, name, probe, remove). */
static int joypad_probe(struct platform_device *pdev);
/* (definition of joypad_driver follows jokstick_cal_init below) */
static struct platform_driver joypad_driver;

/* joypad_driver_init @ffffffc009e28ce8: register; on failure printk("%s platform register failed.\n", __func__)
 * (fmt @ffffffc009c38017, no KERN_ prefix byte; __func__ @..38035 = "joypad_driver_init"), then unregister, return err. */
static int __init joypad_driver_init(void)
{
	int ret = platform_driver_register(&joypad_driver);
	if (ret) {
		printk("%s platform register failed.\n", __func__);
		platform_driver_unregister(&joypad_driver);
	}
	return ret;
}
/* joypad_driver_exit @ffffffc009e45e3c */
static void __exit joypad_driver_exit(void)
{
	platform_driver_unregister(&joypad_driver);
}
module_init(joypad_driver_init);	/* stock: __initcall__kmod_singleadcjoy__459_3667_joypad_driver_init6 */
module_exit(joypad_driver_exit);

/* jokstick_cal_init @ffffffc008c7064c: leaf, 36 32-bit stores (str w*), no loads. jokstick_cal_scale_auto (@ffffffc00a4d5814) is NOT written. */
void jokstick_cal_init(void)
{
	jokstick_cal_switch_l = 0;	/* @ffffffc008c70660 */
	jokstick_cal_switch_r = 0;	/* @ffffffc008c70668 */
	jokstick_cal_scale = 3000;	/* @ffffffc008c70670 */
	jokstick_cal_deadzone = 1000;	/* @ffffffc008c706a0 */
	jokstick_cal_lxll = 0;	/* @ffffffc008c706a8 */
	jokstick_cal_lxlr = 16384;	/* @ffffffc008c706b0 */
	jokstick_cal_lxrl = 16384;	/* @ffffffc008c706b8 */
	jokstick_cal_lxrr = 32768;	/* @ffffffc008c70678 */
	jokstick_cal_lyuu = 0;	/* @ffffffc008c706c0 */
	jokstick_cal_lyud = 16384;	/* @ffffffc008c706c8 */
	jokstick_cal_lydu = 16384;	/* @ffffffc008c706d0 */
	jokstick_cal_lydd = 32768;	/* @ffffffc008c70684 */
	jokstick_cal_lxl = 16384;	/* @ffffffc008c706d8 */
	jokstick_cal_lxl_scale = 1000;	/* @ffffffc008c706e0 */
	jokstick_cal_lxr = 16384;	/* @ffffffc008c706e8 */
	jokstick_cal_lxr_scale = 1000;	/* @ffffffc008c706f0 */
	jokstick_cal_lyu = 16384;	/* @ffffffc008c706f8 */
	jokstick_cal_lyu_scale = 1000;	/* @ffffffc008c70700 */
	jokstick_cal_lyd = 16384;	/* @ffffffc008c70708 */
	jokstick_cal_lyd_scale = 1000;	/* @ffffffc008c70710 */
	jokstick_cal_rxll = 0;	/* @ffffffc008c70718 */
	jokstick_cal_rxlr = 16384;	/* @ffffffc008c70720 */
	jokstick_cal_rxrl = 16384;	/* @ffffffc008c70728 */
	jokstick_cal_rxrr = 32768;	/* @ffffffc008c7068c */
	jokstick_cal_ryuu = 0;	/* @ffffffc008c70730 */
	jokstick_cal_ryud = 16384;	/* @ffffffc008c70738 */
	jokstick_cal_rydu = 16384;	/* @ffffffc008c70740 */
	jokstick_cal_rydd = 32768;	/* @ffffffc008c70694 */
	jokstick_cal_rxl = 16384;	/* @ffffffc008c70748 */
	jokstick_cal_rxl_scale = 1000;	/* @ffffffc008c70750 */
	jokstick_cal_rxr = 16384;	/* @ffffffc008c70758 */
	jokstick_cal_rxr_scale = 1000;	/* @ffffffc008c70760 */
	jokstick_cal_ryu = 16384;	/* @ffffffc008c70768 */
	jokstick_cal_ryu_scale = 1000;	/* @ffffffc008c70770 */
	jokstick_cal_ryd = 16384;	/* @ffffffc008c70774 */
	jokstick_cal_ryd_scale = 1000;	/* @ffffffc008c70778 */
}

/* joypadz_count_mode2: function-local static of joypad2touchpanel_abs_z_mode2, .data @ffffffc00a2a3bb0 = 5. */

/* of_device_id table joypad_of_match @ffffffc0097195f0 (200-byte entries): [0] compatible "singleadc-joypad", data NULL; [1] sentinel. */
static const struct of_device_id joypad_of_match[] = {
	{ .compatible = "singleadc-joypad", },
	{},
};
/* platform_driver @ffffffc00a2a3bb8: +0x0 probe = joypad_probe (@ffffffc008c70780); remove/shutdown/suspend/resume NULL;
 * driver.name @+0x30 -> @ffffffc009ba2b76 "singleadc-joypad"; owner NULL (built-in); of_match_table @+0x58; pm @+0xa8. */
static struct platform_driver joypad_driver = {
	.probe = joypad_probe,
	.driver = {
		.name = "singleadc-joypad",
		.of_match_table = joypad_of_match,
		.pm = &joypad_pm_ops,
	},
};

/* ---- joypad2touchpanel_abs_z_mode2 @ffffffc008c6f290 (0x178). Leaf; returns 0 on every path. ----
 * Globals and access widths seen (all ldr/str w = 32-bit unless noted):
 *   joypad_input_g   @ffffffc00a4d5af0  ldr x (pointer, only NULL-tested)      @..6f294
 *   joypadz_count_mode2 (static) @ffffffc00a2a3bb0 ldr/str w                     @..6f2a0/2ac/2e0
 *   remapkey_right_joy_lenght_x @ffffffc00a4d57b8 ldr w (divisor)                @..6f2cc
 *   abs_z_mode2_x/_y @..5adc/@..5ae0 ldr w; abs_z_mode2_x_base/_y_base @..5ae4/@..5ae8 ldr/str w
 *   abs_z_mode2 (.data) @ffffffc00a2a3bac ldr/str w; abs_z_mode2_mask @..5ad8 str w
 *   gts_dev @ffffffc00a4d58a8 ldr x (pointer, only NULL-tested)
 *   remapkey_right_joy_key_x/_y @..57a8/@..57ac ldr w
 *   touch_data @ffffffc00a4d58b4: writes at +0x10 (str x / 8 bytes), +0x18 and +0x1c (stp w). Layout unknown, modelled as int[].
 * Division by 2000: smull by 0x10624dd3, asr #39, + sign bit @..6f330..6f36c.
 */
extern struct input_dev *joypad_input_g;	/* T @ffffffc00a4d5af0 */
extern void *gts_dev;				/* T @ffffffc00a4d58a8, type unknown */
extern int touch_data[];			/* T @ffffffc00a4d58b4, layout unknown */
int joypad2touchpanel_abs_z_mode2(void)
{
	static int joypadz_count_mode2 = 5;	/* @ffffffc00a2a3bb0 */
	int x, y;

	if (!joypad_input_g)
		return 0;
	if (--joypadz_count_mode2 > 0)
		return 0;
	joypadz_count_mode2 = 200 / remapkey_right_joy_lenght_x;	/* sdiv @..6f2d0 */

	if (!(abs_z_mode2_x | abs_z_mode2_y)) {
		if (abs_z_mode2 != 1)
			return 0;
		if (gts_dev)
			*(u64 *)&touch_data[4] = 1;	/* str x @..6f310: [4]=1,[5]=0 */
		abs_z_mode2 = 0;
		abs_z_mode2_x_base = 0;
		abs_z_mode2_y_base = 0;
		return 0;
	}
	x = abs_z_mode2_x_base + abs_z_mode2_x / 2000;
	y = abs_z_mode2_y_base + abs_z_mode2_y / 2000;
	abs_z_mode2_x_base = x;
	abs_z_mode2_y_base = y;
	if (x > 300 || y > 300 || y < -300 || x < -300) {	/* cmp/cmn #0x12c,#0x12d @..6f370..6f39c */
		if (gts_dev)
			*(u64 *)&touch_data[4] = 1;
		abs_z_mode2_x_base = 0;
		abs_z_mode2_y_base = 0;
		return 0;
	}
	if (gts_dev) {
		*(u64 *)&touch_data[4] = 0x100000001ULL;	/* [4]=1,[5]=1 @..6f3c0 */
		touch_data[6] = remapkey_right_joy_key_y + y;	/* stp @..6f3cc */
		touch_data[7] = remapkey_right_joy_key_x + x;
	}
	abs_z_mode2_mask = 1;
	return 0;
}

/* ---- joypad2touchpanel_abs @ffffffc008c6eb44 (0x238). Returns 0 on every path (exit @..6ed64).
 * touch_data+0x140 (@ffffffc00a4d59f4) is written as 8 bytes then two ints (str x / stp w), modelled as touch_data[80..83].
 * Divisions: smull 0x0c907da5 asr #35 => /163 (2^35/m = 162.99999993); smull 0x51eb851f asr #37 => /100.
 * Both axes multiply by remapkey_left_joy_lenght_x (single load @..6ecf0), y's own length is unused here (stock).
 * Widths: remapkey_left_joy_lenght_x/_y, remapkey_left_joy_mode, remapkey_left_joy_key_x/_y, abxy_mask,
 * joypadx/joypady, joypadxy_init/count: all 32-bit (ldr/str w). gts_dev: 64-bit pointer. */
extern int abxy_mask;				/* T @ffffffc00a4d5aa0 */
int joypad2touchpanel_abs_ext(int mask, int axis, int value);
void g_touch_sync(void);
int joypad2touchpanel_abs(int axis, int value)
{
	int lx = remapkey_left_joy_lenght_x, ly = remapkey_left_joy_lenght_y;	/* @..6eb64, @..6eb78 */

	if (!lx)
		remapkey_left_joy_lenght_x = lx = 100;
	if (!ly)
		remapkey_left_joy_lenght_y = ly = 100;
	if (lx >= 400)
		remapkey_left_joy_lenght_x = 400;
	if (ly >= 400)
		remapkey_left_joy_lenght_y = 400;

	if (abxy_mask) {				/* @..6eb9c */
		joypad2touchpanel_abs_ext(abxy_mask, axis, value);
		return 0;
	}
	if (remapkey_left_joy_mode != 2)		/* @..6ebe0 */
		return 0;

	if (joypadxy_init == 1 && (joypadx | joypady)) {	/* @..6ebec..6ec0c */
		if (gts_dev) {
			*(u64 *)&touch_data[80] = 0x100000001ULL;
			touch_data[82] = remapkey_left_joy_key_y;	/* stp w8(key_y),w9(key_x) @..6ec3c */
			touch_data[83] = remapkey_left_joy_key_x;
		}
		joypadxy_init = 0;
		g_touch_sync();				/* @..6ec44 */
	}
	if (axis == 0)
		joypadx = value;
	else if (axis == 1)
		joypady = value;

	if (--joypadxy_count)				/* @..6ec74 */
		return 0;

	if (!(joypadx | joypady)) {			/* @..6ec90 */
		if (!joypadxy_init) {
			if (gts_dev)
				*(u64 *)&touch_data[80] = 1;	/* [80]=1,[81]=0 @..6ecb8 */
			joypadxy_init = 1;
		}
	} else if (gts_dev) {
		int len = remapkey_left_joy_lenght_x;	/* @..6ecf0 */
		*(u64 *)&touch_data[80] = 0x100000001ULL;	/* @..6ed50 */
		touch_data[82] = (len * (joypady / 163)) / 100 + remapkey_left_joy_key_y;
		touch_data[83] = (len * (joypadx / 163)) / 100 + remapkey_left_joy_key_x;
	}
	joypadxy_count = 1;				/* @..6ed60 */
	return 0;
}

/* touch_data @ffffffc00a4d58b4 is an array of 16-byte slots (g_touch_down indexes with `w0, sxtw #4`).
 * Slot layout from the stores: +0x0 int (set 1), +0x4 int (1 = down, 0 = up), +0x8 int (arg2), +0xc int (arg1).
 * So touch_data[4..7] above = slot 1, touch_data[80..83] = slot 20. Field names unknown. */
/* g_touch_down @ffffffc008c6e5e8 (leaf) */
void g_touch_down(int id, int a1, int a2)
{
	if (!gts_dev)
		return;
	*(u64 *)&touch_data[id * 4] = 0x100000001ULL;	/* str x @..6e604 */
	touch_data[id * 4 + 2] = a2;			/* stp w2, w1 @..6e608 */
	touch_data[id * 4 + 3] = a1;
}
/* g_touch_up @ffffffc008c6e610 (leaf) */
void g_touch_up(int id)
{
	if (!gts_dev)
		return;
	*(u64 *)&touch_data[id * 4] = 1;		/* [0]=1,[1]=0 @..6e62c */
}

/* ---- joypad2touchpanel_abs_z @ffffffc008c6f408 (0x294). Returns 0 on every path (exit @..6f474).
 * Mirror of joypad2touchpanel_abs for the right stick, touch slot 1 (touch_data+0x10 = [4..7]).
 * Globals (all ldr/str w, 32-bit): remapkey_right_joy_lenght_x @..57b8, remapkey_right_joy_lenght_y @..57bc,
 * abxy_z_mask @..5ab8, remapkey_right_joy_mode @..5804, joypadz_init (.data @ffffffc00a2a3ba4), joypadz_count (@..3ba8),
 * joypadz @..5ad0, joypadrz @..5ad4, abs_z_mode2_x/_y @..5adc/@..5ae0, abs_z_mode2 (.data @..3bac),
 * remapkey_right_joy_key_x/_y @..57a8/@..57ac. Same /163 and /100 magic as joypad2touchpanel_abs (@..6f608..6f680). */
extern int abxy_z_mask;				/* T @ffffffc00a4d5ab8 */
int joypad2touchpanel_abs_z_ext(int mask, int axis, int value);
int joypad2touchpanel_abs_z(int axis, int value)
{
	int lx = remapkey_right_joy_lenght_x, ly = remapkey_right_joy_lenght_y;

	if (!lx)
		remapkey_right_joy_lenght_x = lx = 100;
	if (!ly)
		remapkey_right_joy_lenght_y = ly = 100;
	if (lx >= 400)
		remapkey_right_joy_lenght_x = 400;
	if (ly >= 400)
		remapkey_right_joy_lenght_y = 400;

	if (abxy_z_mask) {				/* @..6f460 */
		joypad2touchpanel_abs_z_ext(abxy_z_mask, axis, value);
		return 0;
	}
	if (remapkey_right_joy_mode == 2) {		/* @..6f4c0: relative mode, consumed by _abs_z_mode2 */
		if (axis == 2)
			abs_z_mode2_x = value;
		else if (axis == 5)
			abs_z_mode2_y = value;
		if (abs_z_mode2_x | abs_z_mode2_y)
			abs_z_mode2 = 1;		/* @..6f58c */
		return 0;
	}
	if (remapkey_right_joy_mode != 3)		/* @..6f4b8 */
		return 0;

	if (joypadz_init == 1 && (joypadz | joypadrz)) {	/* @..6f4e8..6f508 */
		if (gts_dev) {
			*(u64 *)&touch_data[4] = 0x100000001ULL;
			touch_data[6] = remapkey_right_joy_key_y;	/* stp w8(key_y @57ac), w9(key_x @57a8) */
			touch_data[7] = remapkey_right_joy_key_x;
		}
		joypadz_init = 0;
		g_touch_sync();				/* @..6f540 */
	}
	if (axis == 2)
		joypadz = value;
	else if (axis == 5)
		joypadrz = value;

	if (--joypadz_count)				/* @..6f5a8 */
		return 0;

	if (!(joypadz | joypadrz)) {
		if (!joypadz_init) {
			if (gts_dev)
				*(u64 *)&touch_data[4] = 1;
			joypadz_init = 1;
		}
	} else if (gts_dev) {
		int len = remapkey_right_joy_lenght_x;	/* @..6f624, used for both axes (stock) */
		*(u64 *)&touch_data[4] = 0x100000001ULL;
		touch_data[6] = (len * (joypadrz / 163)) / 100 + remapkey_right_joy_key_y;
		touch_data[7] = (len * (joypadz / 163)) / 100 + remapkey_right_joy_key_x;
	}
	joypadz_count = 1;				/* @..6f694 */
	return 0;
}

/* ---- g_touch_sync @ffffffc008c6e290 (0x358). touch_data has 30 slots (loop bound 0x1e0 / 0x1e).
 * Slot fields confirmed by use here: +0x0 = "active" (summed, cleared), +0x4 = "down" (summed),
 * +0x8 = reported as ABS_MT_POSITION_Y (0x36), +0xc = reported as ABS_MT_POSITION_X (0x35).
 * So g_touch_down(id, x, y). Event codes are literal immediates: EV_KEY=1/BTN_TOUCH=0x14a, EV_ABS=3,
 * 0x35/0x36/0x30/0x32/0x39 = MT X/Y/TOUCH_MAJOR/WIDTH_MAJOR/TRACKING_ID, EV_SYN=0 code 2 = SYN_MT_REPORT, 0/0/0 = sync. */
struct stock_touch_slot { int active; int down; int y; int x; };
extern int BTN_S;		/* T .data @ffffffc00a2a3b88 (nm name BTN_S), ldr/str w */
void g_touch_sync(void)
{
	struct input_dev *in = gts_dev;
	struct stock_touch_slot *s = (struct stock_touch_slot *)touch_data;
	int i, sum_active = 0, sum_down = 0;

	if (!in)
		return;
	for (i = 0; i < 30; i++)
		sum_active += s[i].active;		/* unrolled @..6e2b8..6e39c */
	if (!sum_active)
		return;
	for (i = 0; i < 30; i++)
		sum_down += s[i].down;			/* unrolled @..6e3b0..6e498 */
	if (!sum_down) {
		for (i = 0; i < 30; i++)		/* @..6e4b4 */
			if (s[i].active)
				s[i].active = 0;
		input_event(in, 1, 0x14a, 0);		/* @..6e5a0 */
		BTN_S = 0;
		input_event(gts_dev, 0, 2, 0);		/* @..6e5bc */
	} else {
		if (!BTN_S) {		/* @..6e4c8 */
			input_event(in, 1, 0x14a, 1);
			BTN_S = 1;
		}
		for (i = 0; i < 30; i++) {
			if (!s[i].active || !s[i].down)
				continue;
			input_event(gts_dev, 3, 0x35, s[i].x);
			input_event(gts_dev, 3, 0x36, s[i].y);
			input_event(gts_dev, 3, 0x30, 40);
			input_event(gts_dev, 3, 0x32, 40);
			input_event(gts_dev, 3, 0x39, i);
			input_event(gts_dev, 0, 2, 0);
		}
	}
	input_event(gts_dev, 0, 0, 0);			/* @..6e5d0 */
}

/* ---- joypad2touchpanel_abs_ext @ffffffc008c6e634 (0x510). Leaf (no calls). Returns 0 on every path.
 * Left stick drives a touch at the position of the face/shoulder button selected by `mask` (= abxy_mask).
 * switch(mask - 3), 12 entries, jump table @ffffffc009719570 (ldrh, target = 0x..6e6b8 + 4*entry):
 *   3 X(@..6e6b8) 4 Y(@..6e72c) 5 A(@..6e7a0) 6 B(@..6e814) 7,8 none 9 L2(@..6e888) 10 R1(@..6e8fc)
 *   11 R2(@..6e970) 12 left_joy_mid(@..6e9e4, slot = abxy_mask_l3) 13 none 14 L1(@..6ea94); >14 or <3: none.
 * In every fixed case the touch slot index equals mask (slot addr 0x..58b4 + 16*mask, e.g. X -> @..58e4 = slot 3).
 * Scale: /163 then * remapkey_left_joy_lenght_x then /100, as in joypad2touchpanel_abs. joypadx_temp/joypady_temp read
 * with ldrsw (32-bit signed). joypad_count_temp (.data @ffffffc00a2a3b90) is decremented; reload value 2 (@..6eb34). */
extern int abxy_mask_l3;			/* T @ffffffc00a4d5aac */
static void stock_ext_touch(int slot, int key_x, int key_y)
{
	struct stock_touch_slot *s = (struct stock_touch_slot *)touch_data + slot;
	int len = remapkey_left_joy_lenght_x;

	*(u64 *)s = 0x100000001ULL;
	s->y = (len * (joypady_temp / 163)) / 100 + key_y;
	s->x = (len * (joypadx_temp / 163)) / 100 + key_x;
}
int joypad2touchpanel_abs_ext(int mask, int axis, int value)
{
	if (!mask)
		return 0;				/* @..6e634 */
	if (gts_dev)
		*(u64 *)&touch_data[80] = 1;		/* slot 20 up, @..6e650 */
	if (axis == 0)
		joypadx_temp = value;			/* @..6e674 */
	else if (axis == 1)
		joypady_temp = value;
	if (--joypad_count_temp)			/* @..6e680 */
		return 0;

	if (gts_dev) {	/* every case re-tests gts_dev (cbz x9) before writing */
		switch (mask) {
		case 3:  stock_ext_touch(3,  remapkey_x_x,  remapkey_x_y);  break;
		case 4:  stock_ext_touch(4,  remapkey_y_x,  remapkey_y_y);  break;
		case 5:  stock_ext_touch(5,  remapkey_a_x,  remapkey_a_y);  break;
		case 6:  stock_ext_touch(6,  remapkey_b_x,  remapkey_b_y);  break;
		case 9:  stock_ext_touch(9,  remapkey_l2_x, remapkey_l2_y); break;
		case 10: stock_ext_touch(10, remapkey_r1_x, remapkey_r1_y); break;
		case 11: stock_ext_touch(11, remapkey_r2_x, remapkey_r2_y); break;
		case 12: stock_ext_touch(abxy_mask_l3, remapkey_left_joy_mid_x, remapkey_left_joy_mid_y); break;
		case 14: stock_ext_touch(14, remapkey_l1_x, remapkey_l1_y); break;
		default: break;
		}
	}
	joypad_count_temp = 2;				/* @..6eb38 */
	return 0;
}

/* ---- joypad2touchpanel_abs_z_ext @ffffffc008c6ed7c (0x514). Leaf. Returns 0 on every path.
 * Checked against joypad2touchpanel_abs_ext case by case (not assumed). Differences from _abs_ext:
 *   - entry "up" write goes to slot 1 (@..58c4, str x @..6ed98), not slot 20;
 *   - axis 2 -> joypadz_temp (@..5abc), axis 5 -> joypadrz_temp (@..5ac0) (ldrsw when read);
 *   - counter joypadz_count_temp (.data @ffffffc00a2a3ba0), reload 2 (@..6f284);
 *   - length remapkey_right_joy_lenght_x (@..57b8);
 *   - jump table @ffffffc009719588: mid-stick case is mask 13 (@..6f130: remapkey_right_joy_mid_x/_y @..57b0/@..57b4,
 *     slot = abxy_z_mask_r3 @..5ac4); mask 12 does nothing here.
 * Same as _abs_ext: fixed cases 3 X,4 Y,5 A,6 B,9 L2,10 R1,11 R2,14 L1 with the same remapkey_* keys and slot == mask
 * (e.g. case 3 @..6ee04 loads remapkey_x_x @..5720 / remapkey_x_y @..5724, slot @..58e4); same /163,/100 magic
 * and y-into-+0x8, x-into-+0xc layout (tail @..6f250..6f27c). */
extern int abxy_z_mask_r3;			/* T @ffffffc00a4d5ac4 */
static void stock_zext_touch(int slot, int key_x, int key_y)
{
	struct stock_touch_slot *s = (struct stock_touch_slot *)touch_data + slot;
	int len = remapkey_right_joy_lenght_x;

	*(u64 *)s = 0x100000001ULL;
	s->y = (len * (joypadrz_temp / 163)) / 100 + key_y;
	s->x = (len * (joypadz_temp / 163)) / 100 + key_x;
}
int joypad2touchpanel_abs_z_ext(int mask, int axis, int value)
{
	if (!mask)
		return 0;				/* @..6ed7c */
	if (gts_dev)
		*(u64 *)&touch_data[4] = 1;		/* slot 1 up */
	if (axis == 2)
		joypadz_temp = value;
	else if (axis == 5)
		joypadrz_temp = value;
	if (--joypadz_count_temp)			/* @..6edcc */
		return 0;

	if (gts_dev) {
		switch (mask) {
		case 3:  stock_zext_touch(3,  remapkey_x_x,  remapkey_x_y);  break;
		case 4:  stock_zext_touch(4,  remapkey_y_x,  remapkey_y_y);  break;
		case 5:  stock_zext_touch(5,  remapkey_a_x,  remapkey_a_y);  break;
		case 6:  stock_zext_touch(6,  remapkey_b_x,  remapkey_b_y);  break;
		case 9:  stock_zext_touch(9,  remapkey_l2_x, remapkey_l2_y); break;
		case 10: stock_zext_touch(10, remapkey_r1_x, remapkey_r1_y); break;
		case 11: stock_zext_touch(11, remapkey_r2_x, remapkey_r2_y); break;
		case 13: stock_zext_touch(abxy_z_mask_r3, remapkey_right_joy_mid_x, remapkey_right_joy_mid_y); break;
		case 14: stock_zext_touch(14, remapkey_l1_x, remapkey_l1_y); break;
		default: break;
		}
	}
	joypadz_count_temp = 2;
	return 0;
}

/* ---- joypad_probe @ffffffc008c70780, opening block (dt parse is inlined into probe in stock), decoded @..707b8..708d0:
 *   joypad = devm_kmalloc(dev, 0x78, 0xdc0 = GFP_KERNEL|__GFP_ZERO);  struct joypad is 0x78 bytes.
 *   on NULL: dev_err(dev, "joypad devm_kzmalloc error!") (@ffffffc009c29081), return -12 (-ENOMEM).
 *   pointer saved to global @ffffffc00a4d5708 (str x @..707fc).
 *   Properties read with device_property_read_u32 (array len 1), DT names verbatim:
 *     "button-adc-fuzz"     -> +0x3c   (@..70804)
 *     "button-adc-flat"     -> +0x40   (@..7081c)
 *     "button-adc-scale"    -> +0x44   (@..70834)
 *     "button-adc-deadzone" -> +0x48   (@..7084c); then +0x4c = +0x48 (str @..70868)
 *     "amux-count"          -> +0x18   (@..70870)
 *     "poll-interval"       -> +0x8    (@..7088c)
 *   +0x38 = device_property_present(dev, "autorepeat") & 1   (@..708a8)
 *   +0x28 = device_get_child_node_count(dev)                  (@..708b8)
 *   if (!amux_count || !child_count) dev_err(dev, "adc key = %d, gpio key = %d error!", ...) (@..708d0).
 * Stock differs from the Gamma port here: no boot-arg overrides (g_button_adc_*) are consulted.
 * Field names below are Gamma's; offsets are stock. */

/* ---- joypad_probe: per-channel ADC setup loop @..70918..70b70 (Gamma: joypad_adc_setup, inlined in stock).
 * adcs = devm_kmalloc(dev, amux_count * 0x64, GFP_KERNEL|__GFP_ZERO) -> joypad+0x20 (@..708ec/@..708f0);
 *   on NULL: dev_err("%s devm_kzmalloc error!", "joypad_adc_setup"), -ENOMEM.
 * For i in 0..amux_count-1, entry e = adcs + 0x64*i:
 *   e+0x50 = bt_adc_scale (joypad+0x44)                          @..70988
 *   e+0x44 = 0x4000 (max); e+0x48 = (i==4||i==5) ? 0 : -0x4000 (min)   @..70974..7098c
 *   if (scale) { e+0x44 = scale << 14; e+0x48 = min * scale; }    @..70994..7099c
 *   e+0x58 = i  (amux channel)                                    @..709a4
 *   e+0x54 = 0 (byte, invert)                                     @..709a8
 *   switch (i) via table @ffffffc0097195c0 (i>5 -> @..70c58):
 *     e+0x40 = axis code: i0 0 (ABS_X), i1 1 (ABS_Y), i2 3 (ABS_RX), i3 4 (ABS_RY), i4 2 (ABS_Z), i5 5 (ABS_RZ)
 *     then reads "<axis>-p-tuning" into e+0x5c and "<axis>-n-tuning" into its pair; if the property read
 *     returns non-zero (absent), the field is set to 0xb4 = 180 (@..709ec etc.).
 *   on loop exit e+0x54 byte set to 1 in one path (@..70954; condition not yet decoded).
 *
 * amux indices 4 and 5: amux-count = 6 in the stock DTB, so channels 4 (ABS_Z) and 5 (ABS_RZ) exist with
 * amux_ch 4 and 5. joypad_adc_read (@..71a14) rejects amux_ch >= 4: it sets the mux enable gpio to 1 (mux off)
 * and returns 0. So Z/RZ always read 0 on this board, matching joypad_show_adc_z/rz which print a constant 0.
 * The right stick is NOT on 4/5: it is ABS_RX/ABS_RY on mux channels 2 and 3, read through the normal path.
 */

/* ---- Stock layouts (offsets from the disassembly; unnamed gaps are padding until decoded) ---- */
struct stock_bt_adc {				/* stride 0x64 (devm_kmalloc amux_count*0x64 @..708e4) */
	u8  pad0[0x40];				/* +0x4 value, +0x8 value_p (joypad_show_adc_lx/_lx_p) live here */
	int code;				/* +0x40 ABS_* code (switch @..709b8) */
	int max;				/* +0x44 */
	int min;				/* +0x48 */
	int pad4c;
	int scale;				/* +0x50 */
	u8  invert;				/* +0x54 */
	u8  pad55[3];
	int amux_ch;				/* +0x58 */
	int tuning_p;				/* +0x5c */
	int tuning_n;				/* +0x60 */
};
struct stock_joypad {				/* devm_kmalloc(dev, 0x78) @..707c4 */
	u8  pad0[0x8];
	int poll_interval;			/* +0x08 */
	u8  enable;				/* +0x0c (suspend/resume) */
	u8  pad0d[0x18 - 0x0d];
	int amux_count;				/* +0x18 */
	int pad1c;
	struct stock_bt_adc *adcs;		/* +0x20 */
	int bt_gpio_count;			/* +0x28 */
	u8  pad2c[0x38 - 0x2c];
	int auto_repeat;			/* +0x38 */
	int bt_adc_fuzz;			/* +0x3c */
	int bt_adc_flat;			/* +0x40 */
	int bt_adc_scale;			/* +0x44 */
	int bt_adc_deadzone;			/* +0x48 */
	int bt_adc_deadzone2;			/* +0x4c copy of +0x48 */
	u8  lock[0x78 - 0x50];			/* +0x50 struct mutex (mutex_lock(drvdata+0x50)) */
};
extern void *poll_dev_private;			/* T @ffffffc00a4d5708 */

/* Per channel: axis code, p/n tuning property names, invert byte (set via the shared tail @..70afc -> @..70950). */
static const struct { int code; const char *p, *n; u8 invert; } stock_adc_axis[6] = {
	{ 0, "abs_x-p-tuning",  "abs_x-n-tuning",  1 },	/* i0 @..709c4, tail @..70afc */
	{ 1, "abs_y-p-tuning",  "abs_y-n-tuning",  1 },	/* i1 @..70ac0, falls into @..70afc */
	{ 3, "abs_rx-p-tuning", "abs_rx-n-tuning", 0 },	/* i2 @..70a40, tail @..70b54 */
	{ 4, "abs_ry-p-tuning", "abs_ry-n-tuning", 1 },	/* i3 @..70a80, tail @..70afc */
	{ 2, "abs_z-p-tuning",  "abs_z-n-tuning",  0 },	/* i4 @..70a00, tail @..70b54 */
	{ 5, "abs_rz-p-tuning", "abs_rz-n-tuning", 0 },	/* i5 @..70b18, falls into @..70b54 */
};

/* Inlined into joypad_probe in stock (@..707b8..70b70). Returns 0 or -errno. */
static int stock_joypad_dt_parse_head(struct device *dev, struct stock_joypad **out)
{
	struct stock_joypad *joypad;
	int i;

	joypad = devm_kmalloc(dev, 0x78, GFP_KERNEL | __GFP_ZERO);
	if (!joypad) {
		dev_err(dev, "joypad devm_kzmalloc error!");
		return -ENOMEM;
	}
	poll_dev_private = joypad;
	device_property_read_u32(dev, "button-adc-fuzz", &joypad->bt_adc_fuzz);
	device_property_read_u32(dev, "button-adc-flat", &joypad->bt_adc_flat);
	device_property_read_u32(dev, "button-adc-scale", &joypad->bt_adc_scale);
	device_property_read_u32(dev, "button-adc-deadzone", &joypad->bt_adc_deadzone);
	joypad->bt_adc_deadzone2 = joypad->bt_adc_deadzone;
	device_property_read_u32(dev, "amux-count", &joypad->amux_count);
	device_property_read_u32(dev, "poll-interval", &joypad->poll_interval);
	joypad->auto_repeat = device_property_present(dev, "autorepeat") & 1;
	joypad->bt_gpio_count = device_get_child_node_count(dev);
	if (!joypad->amux_count || !joypad->bt_gpio_count)
		dev_err(dev, "adc key = %d, gpio key = %d error!",
			joypad->amux_count, joypad->bt_gpio_count);
	/* NOTE: stock continues after this dev_err (no return seen between @..708d0 and @..708d4); verify. */

	joypad->adcs = devm_kmalloc(dev, (long)joypad->amux_count * 0x64, GFP_KERNEL | __GFP_ZERO);
	if (!joypad->adcs) {
		dev_err(dev, "%s devm_kzmalloc error!", "joypad_adc_setup");
		return -ENOMEM;
	}
	for (i = 0; i < joypad->amux_count; i++) {
		struct stock_bt_adc *e = &joypad->adcs[i];

		e->scale = joypad->bt_adc_scale;
		e->max = 0x4000;
		e->min = ((i & ~1) == 4) ? 0 : -0x4000;
		if (e->scale) {
			e->max = e->scale << 14;
			e->min = e->min * e->scale;
		}
		e->amux_ch = i;
		e->invert = 0;
		if (i > 5)
			continue;	/* default branch @..70c58: not decoded yet */
		e->code = stock_adc_axis[i].code;
		if (device_property_read_u32(dev, stock_adc_axis[i].p, &e->tuning_p))
			e->tuning_p = 180;		/* 0xb4 @..70948 */
		if (device_property_read_u32(dev, stock_adc_axis[i].n, &e->tuning_n))
			e->tuning_n = 180;
		e->invert = stock_adc_axis[i].invert;
	}
	*out = joypad;
	return 0;
}

/* Open question 1 resolved: after dev_err("adc key = %d, gpio key = %d error!") @..708d0 the code falls straight
 * into the adcs allocation @..708d4. There is no return on that path in stock. */

/* ---- joypad_amux_setup, inlined in probe @..70b70..70df0. amux struct is 0x40 bytes (devm_kmalloc @..70b80),
 * stored at joypad+0x10: +0x0 iio_channel*, +0x8 sel_a gpio, +0xc sel_b gpio, +0x10 en gpio (same offsets
 * joypad_adc_read uses). Error exits print "Joypad amux setup failed: %d\n" (@..70d88) and then
 * "dt parse error!(err = %d)\n" (@..70d9c) before returning err. */
static int stock_amux_gpio(struct device *dev, const char *prop, const char *label, int *gpio_out)
{
	enum of_gpio_flags flags;
	int gpio, ret;

	gpio = of_get_named_gpio_flags(dev->of_node, prop, 0, &flags);	/* dev+0x2a0 = of_node */
	*gpio_out = gpio;
	if ((unsigned int)gpio > 0x1ff)		/* b.hi @..70c28: invalid gpio skips request + direction */
		return 0;
	ret = devm_gpio_request(dev, gpio, label);
	if (ret < 0) {
		dev_err(dev, "%s : failed to request %s %d\n", "joypad_amux_setup", label, gpio);
		/* stock has one literal per gpio: "%s : failed to request amux-sel-a %d\n" etc. */
		return ret;
	}
	return gpiod_direction_output_raw(gpio_to_desc(gpio), 0);	/* all three driven 0 */
}
static int stock_joypad_amux_setup(struct device *dev, struct stock_joypad *joypad, struct stock_amux **out)
{
	struct stock_amux *amux;
	enum iio_chan_type type;
	int ret;

	amux = devm_kmalloc(dev, 0x40, GFP_KERNEL | __GFP_ZERO);
	*out = amux;					/* joypad+0x10 */
	if (!amux) {
		dev_err(dev, "%s amux devm_kzmalloc error!", "joypad_amux_setup");
		return -ENOMEM;
	}
	amux->chan = devm_iio_channel_get(dev, "amux_adc");
	if (IS_ERR(amux->chan)) {
		dev_err(dev, "iio channel get error\n");
		return -EINVAL;				/* stock returns -22, not PTR_ERR (@..70bdc) */
	}
	if (!amux->chan->indio_dev)
		return -ENXIO;				/* -6 @..70bec */
	if (iio_get_channel_type(amux->chan, &type))
		return -EINVAL;
	if (type != IIO_VOLTAGE) {
		dev_err(dev, "Incompatible channel type %d\n", type);
		return -EINVAL;
	}
	ret = stock_amux_gpio(dev, "amux-a-gpios", "amux-sel-a", &amux->sel_a_gpio);
	if (ret < 0)
		return ret;
	ret = stock_amux_gpio(dev, "amux-b-gpios", "amux-sel-b", &amux->sel_b_gpio);
	if (ret < 0)
		return ret;
	ret = stock_amux_gpio(dev, "amux-en-gpios", "amux-en", &amux->en_gpio);
	if (ret < 0)
		return ret;
	if (!dev->of_node)
		return -ENODEV;				/* -19 @..70dfc */
	return 0;
}
/* Per-child GPIO parse starts @..70e04: buttons = devm_kmalloc(bt_gpio_count * 0x20) -> joypad+0x30; for each
 * child: of_get_named_gpio_flags(child,"gpios"), of_get_property("label"), devm_gpio_request_one(gpio, 1, label),
 * "linux,code" (required; "Button without keycode: 0x%x\n"), "linux,input-type" (default 1 on failure),
 * "linux,abs-value" (default 0). Field offsets inside the 0x20 slot: NOT yet verified; not written as C. */

_Static_assert(__builtin_offsetof(struct stock_amux, sel_a_gpio) == 0x8, "amux+0x8");
_Static_assert(__builtin_offsetof(struct stock_amux, en_gpio) == 0x10, "amux+0x10");
_Static_assert(sizeof(struct stock_joypad) == 0x78, "joypad 0x78");
_Static_assert(sizeof(struct stock_bt_adc) == 0x64, "adc 0x64");
_Static_assert(__builtin_offsetof(struct stock_bt_adc, amux_ch) == 0x58, "adc+0x58");
_Static_assert(__builtin_offsetof(struct stock_joypad, bt_adc_deadzone) == 0x48, "jp+0x48");
