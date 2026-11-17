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
// SPDX-License-Identifier: GPL-2.0
/*
 * Reconstruction of the Anbernic RG DS stock singleadc-joypad sysfs layer,
 * from stock.elf disassembly.
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

/*
 * Stock layer: every behavior below reproduces the RG DS stock binary, bugs included. Suspected stock bugs are listed
 * here for separate fix commits; nothing in this file fixes them.
 */
/* FIX CANDIDATE: keypad2touchpanel_abs key B accepts a mode 2/3 press when the mask is 0 or 4 (Y's slot) instead of
 * 0 or 6 (tst #0xfffffffb @ffffffc008c70080, @ffffffc008c700c8). Same in the RG556 .ko. */
/* FIX CANDIDATE: joypad_resume (@ffffffc008c7699c) is byte-identical to joypad_suspend and writes 0 to the +0xc byte
 * (strb wzr @ffffffc008c769c0); resume would be expected to restore it. */
/* FIX CANDIDATE: joypad_show_adc_z_p / joypad_show_adc_rz_p (@ffffffc008c71e9c, @ffffffc008c71ed8) read channel 0
 * +0x8 (the LX value) instead of their own channel; joypad_show_adc_z / _rz print a constant 0. */
/* FIX CANDIDATE: joypad2touchpanel_abs (@ffffffc008c6ecf0) and joypad2touchpanel_abs_z (@ffffffc008c6f624) scale
 * both axes by the *_lenght_x value; the *_lenght_y value is clamped but never used there. */
/* FIX CANDIDATE: joypad_probe does not return when amux-count or the child count is 0; it only logs
 * "adc key = %d, gpio key = %d error!" (@ffffffc008c708d0) and allocates a zero-length adc array. */
/* FIX CANDIDATE: amux setup returns -EINVAL instead of PTR_ERR() on a devm_iio_channel_get error
 * (@ffffffc008c70bdc), which loses -EPROBE_DEFER. */
/* FIX CANDIDATE: joypad_adc_read refuses mux indices >= 4 (@ffffffc008c71a54) while the DTB sets amux-count = 6, so
 * channels 4/5 (ABS_Z/ABS_RZ) always read 0. */
/* FIX CANDIDATE: joypad_show_amux_debug (@ffffffc008c71bcc) clears the enable byte, so reading the attr stops
 * polling, and always prints adc value 0 (@ffffffc008c71be4). */
/* FIX CANDIDATE: joypad_adc_check sums and averages a fixed 6 channels and writes hist[] of channels 4/5
 * (@ffffffc008c75fd4..762ac) regardless of amux_count; with amux_count < 6 this writes past the adc array. */

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
extern int hall_status_share;          /* T @ffffffc00a4d56ec. Owned by stock gpio_keys.c: it sits in .bss between evdev and gpio_keys_probe.__key, and joypad_show/store_hall_status (@ffffffc008c6bd34) lie in gpio_keys text just before gpio_keys_resume. Keep extern. */
int factory_led_status;                /* T @ffffffc00a4d5a94 */
EXPORT_SYMBOL(factory_led_status);	/* stock has __ksymtab_factory_led_status (battery LED code reads it) */
int gt9xx_touch_swap_status;           /* T @ffffffc00a4d5a98 */
int gt9xx_touch_combo_status;          /* T @ffffffc00a4d5a9c */

#define DRV(dev) ((char *)dev_get_drvdata(dev))
#define CH_INT(dev, off) (*(int *)(*(char **)(DRV(dev) + 0x20) + (off)))

static ssize_t joypad_show_adc_deadzone(struct device *dev, struct device_attribute *attr, char *buf)
{ return sprintf(buf, "%d\n", *(int *)(DRV(dev) + 0x48)); }
static ssize_t joypad_store_adc_deadzone(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ char *j = DRV(dev);	/* stock reads drvdata before parsing */
  *(int *)(j + 0x48) = simple_strtoul(buf, NULL, 10); return count; }
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
/* Stock keeps dev_attr_hall_status and joypad_show/store_hall_status in gpio_keys.c (text @ffffffc008c6bd34 next to
 * gpio_keys_resume), not in joypad_attrs. Kept here only as a reference for the tree agent's gpio_keys work. */
static struct device_attribute *stock_hall_status_attr __maybe_unused = &dev_attr_hall_status;
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


/* joypad_show_enable @ffffffc008c717ec: sprintf("%d\n", byte +0xc). joypad_store_enable @ffffffc008c71824: under the
 * mutex, +0xc = (simple_strtoul(buf, NULL, 10) != 0), return count. Userspace turns polling on through this attr;
 * probe leaves it 0 (@..71684), as does suspend/resume. */
static ssize_t joypad_show_enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", *((u8 *)dev_get_drvdata(dev) + 0xc));
}
static ssize_t joypad_store_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *j = dev_get_drvdata(dev);

	mutex_lock((struct mutex *)(j + 0x50));
	*(u8 *)(j + 0xc) = simple_strtoul(buf, NULL, 10) != 0;
	mutex_unlock((struct mutex *)(j + 0x50));
	return count;
}
static DEVICE_ATTR(enable, 0644, joypad_show_enable, joypad_store_enable);

/* Names and order = stock joypad_attrs @ffffffc00a2a3cb0 / joypad_attr_group @ffffffc00a2a3c88; (121 entries, read from the ELF). hall_status is NOT in stock's
 * joypad group (it lives with gpio_keys). */
/* tentative definitions; the DEVICE_ATTRs are at the end of the file, after the structs they use */
static struct device_attribute dev_attr_poll_interval, dev_attr_adc_fuzz, dev_attr_adc_flat, dev_attr_adc_cal,
	dev_attr_amux_debug;
static struct attribute *joypad_attrs[] = {
	&dev_attr_poll_interval.attr,
	&dev_attr_adc_fuzz.attr,
	&dev_attr_adc_flat.attr,
	&dev_attr_enable.attr,
	&dev_attr_adc_cal.attr,
	&dev_attr_adc_deadzone.attr,
	&dev_attr_amux_debug.attr,
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
static struct attribute_group joypad_attr_group = { .attrs = joypad_attrs };	/* not const in stock: .data @ffffffc00a2a3c88 */

/* ---- State variables (global, nm type T). Initialized ones live in .data @ffffffc00a2a3b8c.. (values read from ELF);
 * the rest are .bss @ffffffc00a4d5aa4... 4-byte spacing between neighbors => int. Width of each still to be confirmed
 * against its ldr/str when the users are rebuilt. ---- */
int BTN_S = 1;				/* T .data @ffffffc00a2a3b88, initial value 1 read from the ELF; ldr/str w */
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
static noinline int joypad_adc_read(struct stock_amux *amux, void *button)	/* stock calls it out of line (probe, adc_check, store_adc_cal) */
{
	int val = 0;
	int idx = *(int *)((char *)button + 0x58);

	gpiod_set_raw_value(gpio_to_desc(amux->en_gpio), 0);
	if ((unsigned int)idx >= 4) {
		gpiod_set_raw_value(gpio_to_desc(amux->en_gpio), 1);
		return 0;
	}
	{
		/* sel-A {1,1,0,0}, sel-B {1,0,0,1} by idx (stock: unnamed rodata @ffffffc009d024e8 / @ffffffc009d02208) */
		int a, b;

		switch (idx) {	/* clang turns this into two anonymous lookup tables, as stock */
		case 0: a = 1; b = 1; break;
		case 1: a = 1; b = 0; break;
		case 2: a = 0; b = 0; break;
		case 3: a = 0; b = 1; break;
		default: __builtin_unreachable();	/* idx >= 4 returned above */
		}
		gpiod_set_raw_value(gpio_to_desc(amux->sel_a_gpio), a);
		gpiod_set_raw_value(gpio_to_desc(amux->sel_b_gpio), b);
	}
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
__always_inline void jokstick_cal_init(void)	/* T symbol in stock, but inlined into joypad_probe */
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
struct input_dev *joypad_input_g;	/* T @ffffffc00a4d5af0, 8 bytes (ldr x) */
struct input_dev *gts_dev;
EXPORT_SYMBOL(gts_dev);			/* stock has __ksymtab_gts_dev */		/* T @ffffffc00a4d58a8, 8 bytes; passed to input_event in g_touch_sync */
int touch_id;				/* T @ffffffc00a4d58b0 (4 bytes before touch_data); no access with #0x8b0 in the joypad text range, kept for symbol parity */
int touch_data[0x1e0 / 4];		/* T @ffffffc00a4d58b4..@..5a94 = 0x1e0 bytes = 30 slots of 16 */
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
int abxy_mask;				/* T @ffffffc00a4d5aa0, ldr w */
int joypad2touchpanel_abs_ext(int mask, int axis, int value);
void g_touch_sync(void);
int joypad2touchpanel_abs(int axis, int value)
{
	/* clamp written on the globals directly, as stock (each test re-reads the global) */
	if (!remapkey_left_joy_lenght_x)
		remapkey_left_joy_lenght_x = 100;
	if (!remapkey_left_joy_lenght_y)
		remapkey_left_joy_lenght_y = 100;
	if (remapkey_left_joy_lenght_x >= 400)
		remapkey_left_joy_lenght_x = 400;
	if (remapkey_left_joy_lenght_y >= 400)
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
	int *slot = (int *)((char *)touch_data + ((long)id << 4));	/* 16-byte slots (sxtw #4 @..6e5fc) */

	*(u64 *)slot = 0x100000001ULL;			/* str x @..6e604 */
	slot[2] = a2;					/* stp w2, w1 @..6e608 */
	slot[3] = a1;
}
/* g_touch_up @ffffffc008c6e610 (leaf) */
void g_touch_up(int id)
{
	if (!gts_dev)
		return;
	*(u64 *)((char *)touch_data + ((long)id << 4)) = 1;	/* [0]=1,[1]=0 (sbfiz #4 @..6e61c, str @..6e62c) */
}

/* ---- joypad2touchpanel_abs_z @ffffffc008c6f408 (0x294). Returns 0 on every path (exit @..6f474).
 * Mirror of joypad2touchpanel_abs for the right stick, touch slot 1 (touch_data+0x10 = [4..7]).
 * Globals (all ldr/str w, 32-bit): remapkey_right_joy_lenght_x @..57b8, remapkey_right_joy_lenght_y @..57bc,
 * abxy_z_mask @..5ab8, remapkey_right_joy_mode @..5804, joypadz_init (.data @ffffffc00a2a3ba4), joypadz_count (@..3ba8),
 * joypadz @..5ad0, joypadrz @..5ad4, abs_z_mode2_x/_y @..5adc/@..5ae0, abs_z_mode2 (.data @..3bac),
 * remapkey_right_joy_key_x/_y @..57a8/@..57ac. Same /163 and /100 magic as joypad2touchpanel_abs (@..6f608..6f680). */
int abxy_z_mask;			/* T @ffffffc00a4d5ab8, ldr w */
int joypad2touchpanel_abs_z_ext(int mask, int axis, int value);
int joypad2touchpanel_abs_z(int axis, int value)
{
	/* clamp written on the globals directly, as stock (each test re-reads the global) */
	if (!remapkey_right_joy_lenght_x)
		remapkey_right_joy_lenght_x = 100;
	if (!remapkey_right_joy_lenght_y)
		remapkey_right_joy_lenght_y = 100;
	if (remapkey_right_joy_lenght_x >= 400)
		remapkey_right_joy_lenght_x = 400;
	if (remapkey_right_joy_lenght_y >= 400)
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
/* BTN_S: defined above, before joypad_init_temp, as in stock .data (@ffffffc00a2a3b88) */
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
int abxy_mask_l3;			/* T @ffffffc00a4d5aac, ldrsw */
static __always_inline void stock_ext_touch(int slot, int key_x, int key_y)
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

	{
		switch (mask) {	/* each case re-tests gts_dev before writing, as stock (cbz x9 per case) */
		case 3:  if (gts_dev) stock_ext_touch(3,  remapkey_x_x,  remapkey_x_y);  break;
		case 4:  if (gts_dev) stock_ext_touch(4,  remapkey_y_x,  remapkey_y_y);  break;
		case 5:  if (gts_dev) stock_ext_touch(5,  remapkey_a_x,  remapkey_a_y);  break;
		case 6:  if (gts_dev) stock_ext_touch(6,  remapkey_b_x,  remapkey_b_y);  break;
		case 9:  if (gts_dev) stock_ext_touch(9,  remapkey_l2_x, remapkey_l2_y); break;
		case 10: if (gts_dev) stock_ext_touch(10, remapkey_r1_x, remapkey_r1_y); break;
		case 11: if (gts_dev) stock_ext_touch(11, remapkey_r2_x, remapkey_r2_y); break;
		case 12: if (gts_dev) stock_ext_touch(abxy_mask_l3, remapkey_left_joy_mid_x, remapkey_left_joy_mid_y); break;
		case 14: if (gts_dev) stock_ext_touch(14, remapkey_l1_x, remapkey_l1_y); break;
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
int abxy_z_mask_r3;			/* T @ffffffc00a4d5ac4, ldrsw */
static __always_inline void stock_zext_touch(int slot, int key_x, int key_y)
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

	{
		switch (mask) {	/* each case re-tests gts_dev before writing, as stock (cbz x9 per case) */
		case 3:  if (gts_dev) stock_zext_touch(3,  remapkey_x_x,  remapkey_x_y);  break;
		case 4:  if (gts_dev) stock_zext_touch(4,  remapkey_y_x,  remapkey_y_y);  break;
		case 5:  if (gts_dev) stock_zext_touch(5,  remapkey_a_x,  remapkey_a_y);  break;
		case 6:  if (gts_dev) stock_zext_touch(6,  remapkey_b_x,  remapkey_b_y);  break;
		case 9:  if (gts_dev) stock_zext_touch(9,  remapkey_l2_x, remapkey_l2_y); break;
		case 10: if (gts_dev) stock_zext_touch(10, remapkey_r1_x, remapkey_r1_y); break;
		case 11: if (gts_dev) stock_zext_touch(11, remapkey_r2_x, remapkey_r2_y); break;
		case 13: if (gts_dev) stock_zext_touch(abxy_z_mask_r3, remapkey_right_joy_mid_x, remapkey_right_joy_mid_y); break;
		case 14: if (gts_dev) stock_zext_touch(14, remapkey_l1_x, remapkey_l1_y); break;
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
 * No boot-arg overrides are consulted. Field names are descriptive (from the DT property and sysfs
 * names where stock has them); offsets are stock. */

/* ---- joypad_probe: per-channel ADC setup loop @..70918..70b70 (stock string "joypad_adc_setup", inlined in probe).
 * adc = devm_kmalloc(dev, amux_count * 0x64, GFP_KERNEL|__GFP_ZERO) -> joypad+0x20 (@..708ec/@..708f0);
 *   on NULL: dev_err("%s devm_kzmalloc error!", "joypad_adc_setup"), -ENOMEM.
 * For i in 0..amux_count-1, entry e = adc + 0x64*i:
 *   e+0x50 = adc_scale (joypad+0x44)                          @..70988
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
 * mux_index 4 and 5. joypad_adc_read (@..71a14) rejects mux_index >= 4: it sets the mux enable gpio to 1 (mux off)
 * and returns 0. So Z/RZ always read 0 on this board, matching joypad_show_adc_z/rz which print a constant 0.
 * The right stick is NOT on 4/5: it is ABS_RX/ABS_RY on mux channels 2 and 3, read through the normal path.
 */

/* ---- Stock layouts (offsets from the disassembly; unnamed gaps are padding until decoded) ---- */
struct stock_adc_channel {				/* stride 0x64 (devm_kmalloc amux_count*0x64 @..708e4) */
	int value;				/* +0x00 joypad_adc_read result, then the 4-sample average (@..75f08, @..761f0) */
	int shown;				/* +0x04 joypad_show_adc_lx */
	int shown_p;				/* +0x08 joypad_show_adc_lx_p */
	int hist[4];				/* +0x0c..+0x18 sample history (@..75f8c) */
	u8  pad1c[0x34 - 0x1c];
	int sum;				/* +0x34 accumulator, zeroed after averaging (@..76284) */
	int last;				/* +0x38 last reported value (@..764b0) */
	int pad3c;
	int code;				/* +0x40 ABS_* code (switch @..709b8) */
	int max;				/* +0x44 */
	int min;				/* +0x48 */
	int cal;				/* +0x4c centre value subtracted when jokstick_cal_switch_* is 0 (@..7631c) */
	int scale;				/* +0x50 */
	u8  invert;				/* +0x54 */
	u8  pad55[3];
	int mux_index;				/* +0x58 */
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
	struct stock_adc_channel *adc;		/* +0x20 */
	int gpio_key_count;			/* +0x28 */
	u8  pad2c[0x38 - 0x2c];
	int autorepeat;			/* +0x38 */
	int adc_fuzz_v;			/* +0x3c */
	int adc_flat_v;			/* +0x40 */
	int adc_scale;			/* +0x44 */
	int adc_deadzone_v;			/* +0x48 */
	int adc_deadzone_copy;			/* +0x4c copy of +0x48 */
	struct mutex lock;			/* +0x50 (mutex_lock(drvdata+0x50)); lock class name "&joypad->lock" as stock */
	u8  lock_pad[0x20 - sizeof(struct mutex)];
	int amux_debug;				/* +0x70 joypad_store_amux_debug @..71c48 */
	int pad74;
};
void *poll_dev_private;			/* T @ffffffc00a4d5708, 8 bytes (next symbol @..5710) */


/* Inlined into joypad_probe in stock (@..707b8..708d4). Returns 0 or -ENOMEM. */
static int stock_joypad_dt_parse_head(struct device *dev, struct stock_joypad **out)
{
	struct stock_joypad *joypad;

	joypad = devm_kmalloc(dev, 0x78, GFP_KERNEL | __GFP_ZERO);
	if (!joypad) {
		dev_err(dev, "joypad devm_kzmalloc error!");
		return -ENOMEM;
	}
	poll_dev_private = joypad;
	device_property_read_u32(dev, "button-adc-fuzz", &joypad->adc_fuzz_v);
	device_property_read_u32(dev, "button-adc-flat", &joypad->adc_flat_v);
	device_property_read_u32(dev, "button-adc-scale", &joypad->adc_scale);
	device_property_read_u32(dev, "button-adc-deadzone", &joypad->adc_deadzone_v);
	joypad->adc_deadzone_copy = joypad->adc_deadzone_v;
	device_property_read_u32(dev, "amux-count", &joypad->amux_count);
	device_property_read_u32(dev, "poll-interval", &joypad->poll_interval);
	joypad->autorepeat = device_property_present(dev, "autorepeat") & 1;
	joypad->gpio_key_count = device_get_child_node_count(dev);
	if (!joypad->amux_count || !joypad->gpio_key_count)
		dev_err(dev, "adc key = %d, gpio key = %d error!",
			joypad->amux_count, joypad->gpio_key_count);
	/* NOTE: stock continues after this dev_err (no return seen between @..708d0 and @..708d4); verify. */
	*out = joypad;
	return 0;
}

/* joypad_adc_setup, inlined in probe @..708d4..70b70. Both of its error exits (alloc @..70910, amux count
 * @..70c74) branch to @..70d8c, which prints "dt parse error!(err = %d)\n" and returns; neither prints
 * "Joypad amux setup failed". */
static int stock_joypad_adc_setup(struct device *dev, struct stock_joypad *joypad)
{
	int i;

	joypad->adc = devm_kmalloc(dev, (long)joypad->amux_count * 0x64, GFP_KERNEL | __GFP_ZERO);
	if (!joypad->adc) {
		dev_err(dev, "%s devm_kzmalloc error!", "joypad_adc_setup");
		return -ENOMEM;
	}
	for (i = 0; i < joypad->amux_count; i++) {
		struct stock_adc_channel *e = &joypad->adc[i];

		e->scale = joypad->adc_scale;
		e->max = 0x4000;
		e->min = ((i & ~1) == 4) ? 0 : -0x4000;
		if (e->scale) {
			e->max = e->scale << 14;
			e->min = e->min * e->scale;
		}
		e->mux_index = i;
		e->invert = 0;
		if (i > 5) {	/* default branch @..70c58 */
			dev_err(dev, "%s amux count(%d) error!", "joypad_adc_setup", i);
			return -EINVAL;
		}
		/* switch(i) via jump table @ffffffc0097195c0. Each case reads its own -p-tuning; the -n-tuning read sits in
		 * two shared tails, one that sets invert (@..70afc -> @..70950) and one that does not (@..70b54).
		 * A missing property leaves 180 (0xb4 @..70948). */
		{
			const char *nn;

#define ADC_P(CODE, NAME)								\
			e->code = (CODE);						\
			if (device_property_read_u32(dev, "abs_" NAME "-p-tuning", &e->tuning_p)) \
				e->tuning_p = 180;					\
			nn = "abs_" NAME "-n-tuning"
			switch (i) {
			case 0: ADC_P(0, "x");  goto tail_inv;		/* @..709c4 */
			case 1: ADC_P(1, "y");  goto tail_inv;		/* @..70ac0 */
			case 2: ADC_P(3, "rx"); goto tail_plain;	/* @..70a40 */
			case 3: ADC_P(4, "ry"); goto tail_inv;		/* @..70a80 */
			case 4: ADC_P(2, "z");  goto tail_plain;	/* @..70a00 */
			default: ADC_P(5, "rz"); goto tail_plain;	/* @..70b18 */
			}
#undef ADC_P
tail_inv:
			if (device_property_read_u32(dev, nn, &e->tuning_n))
				e->tuning_n = 180;
			e->invert = 1;
			continue;
tail_plain:
			if (device_property_read_u32(dev, nn, &e->tuning_n))
				e->tuning_n = 180;
		}
	}
	return 0;
}

/* Open question 1 resolved: after dev_err("adc key = %d, gpio key = %d error!") @..708d0 the code falls straight
 * into the adc array allocation @..708d4. There is no return on that path in stock. */

/* ---- joypad_amux_setup, inlined in probe @..70b70..70df0. amux struct is 0x40 bytes (devm_kmalloc @..70b80),
 * stored at joypad+0x10: +0x0 iio_channel*, +0x8 sel_a gpio, +0xc sel_b gpio, +0x10 en gpio (same offsets
 * joypad_adc_read uses). Error exits print "Joypad amux setup failed: %d\n" (@..70d88) and then
 * "dt parse error!(err = %d)\n" (@..70d9c) before returning err. */
static __always_inline int stock_amux_gpio(struct device *dev, const char *prop, const char *label,
					   const char *req_fmt, int *gpio_out)
{
	enum of_gpio_flags flags;
	int gpio, ret;

	gpio = of_get_named_gpio_flags(dev->of_node, prop, 0, &flags);	/* dev+0x2a0 = of_node */
	*gpio_out = gpio;
	if ((unsigned int)gpio > 0x1ff)		/* b.hi @..70c28: invalid gpio skips request + direction */
		return 0;
	ret = devm_gpio_request(dev, gpio, label);
	if (ret < 0) {
		/* stock has one literal per gpio (x1 set per path, one shared call @..70d74) */
		dev_err(dev, req_fmt, "joypad_amux_setup", gpio);
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
	ret = stock_amux_gpio(dev, "amux-a-gpios", "amux-sel-a",
			      "%s : failed to request amux-sel-a %d\n", &amux->sel_a_gpio);
	if (ret < 0)
		return ret;
	ret = stock_amux_gpio(dev, "amux-b-gpios", "amux-sel-b",
			      "%s : failed to request amux-sel-b %d\n", &amux->sel_b_gpio);
	if (ret < 0)
		return ret;
	ret = stock_amux_gpio(dev, "amux-en-gpios", "amux-en",
			      "%s : failed to request amux-en %d\n", &amux->en_gpio);
	if (ret < 0)
		return ret;
	if (!dev->of_node)
		return -ENODEV;				/* -19 @..70dfc */
	return 0;
}
/* Per-child GPIO parse starts @..70e04: buttons = devm_kmalloc(gpio_key_count * 0x20) -> joypad+0x30; for each
 * child: of_get_named_gpio_flags(child,"gpios"), of_get_property("label"), devm_gpio_request_one(gpio, 1, label),
 * "linux,code" (required; "Button without keycode: 0x%x\n"), "linux,input-type" (default 1 on failure),
 * "linux,abs-value" (default 0). Field offsets inside the 0x20 slot: NOT yet verified; not written as C. */

_Static_assert(__builtin_offsetof(struct stock_amux, sel_a_gpio) == 0x8, "amux+0x8");
_Static_assert(__builtin_offsetof(struct stock_amux, en_gpio) == 0x10, "amux+0x10");
_Static_assert(sizeof(struct stock_joypad) == 0x78, "joypad 0x78");
_Static_assert(sizeof(struct stock_adc_channel) == 0x64, "adc 0x64");
_Static_assert(__builtin_offsetof(struct stock_adc_channel, mux_index) == 0x58, "adc+0x58");
_Static_assert(__builtin_offsetof(struct stock_joypad, adc_deadzone_v) == 0x48, "jp+0x48");

/* ---- joypad_gpio_setup, inlined in probe @..70e04..70f9c. Button slot is 0x20 bytes (index << 5 @..70e9c):
 *   +0x00 const char *label   (of_get_property(child, "label", NULL), str x @..70ed4)
 *   +0x08 int gpio            (of_get_named_gpio_flags(child, "gpios", 0, &flags), str w @..70ea4)
 *   +0x0c int input_type      ("linux,input-type"; 1 if the read fails, @..70f40)
 *   +0x10 int code            ("linux,code"; required, @..70f00)
 *   +0x14 int abs_value       ("linux,abs-value"; 0 if the read fails, @..70f68)
 *   +0x19 u8                  = ((flags & 1) == 0), i.e. 1 when NOT OF_GPIO_ACTIVE_LOW (tst/cset eq @..70ec0)
 * Error exits: gpio < 0 -> "Failed to get gpio flags, error: %d\n"; request fail -> "Failed to request GPIO %d,
 * error %d\n" (@..712fc); no code -> "Button without keycode: 0x%x\n" (prints the gpio) and -EINVAL; zero children
 * -> -EINVAL. All exits then print "dt parse error!(err = %d)\n" (@..70d9c). */
struct stock_button {
	const char *label;	/* +0x00 */
	int gpio;		/* +0x08 */
	int input_type;		/* +0x0c */
	int code;		/* +0x10 */
	int abs_value;		/* +0x14 */
	u8  raw_not_high;	/* +0x18: (gpiod_get_raw_value != 1), written by joypad_gpio_check @..74f70 */
	u8  not_active_low;	/* +0x19 */
	u8  pad1a[6];
};
_Static_assert(sizeof(struct stock_button) == 0x20, "button 0x20");
_Static_assert(__builtin_offsetof(struct stock_button, not_active_low) == 0x19, "button+0x19");

static __always_inline int stock_joypad_gpio_setup(struct device *dev, struct stock_joypad *joypad, struct stock_button **out)
{
	struct device_node *np = dev->of_node, *child = NULL;
	struct stock_button *buttons;
	int n = 0, ret;

	buttons = devm_kmalloc(dev, (long)joypad->gpio_key_count << 5, GFP_KERNEL | __GFP_ZERO);
	*out = buttons;					/* joypad+0x30 */
	if (!buttons) {
		dev_err(dev, "%s devm_kzmalloc error!", "joypad_gpio_setup");
		return -ENOMEM;
	}
	while ((child = of_get_next_child(np, child))) {
		struct stock_button *b = &buttons[n];
		enum of_gpio_flags flags = 0;

		ret = of_get_named_gpio_flags(child, "gpios", 0, &flags);
		b->gpio = ret;
		if (ret < 0) {	/* stock prints the returned value (w21 = w0 @..70f74), not a re-read of b->gpio */
			dev_err(dev, "Failed to get gpio flags, error: %d\n", ret);
			return ret;
		}
		b->not_active_low = !(flags & 1);
		b->label = of_get_property(child, "label", NULL);
		if ((unsigned int)b->gpio <= 0x1ff) {
			ret = devm_gpio_request_one(dev, b->gpio, 1, b->label);	/* flags 1 = GPIOF_IN */
			if (ret < 0) {
				dev_err(dev, "Failed to request GPIO %d, error %d\n", b->gpio, ret);
				return ret;
			}
		}
		if (of_property_read_u32(child, "linux,code", &b->code) < 0) {
			dev_err(dev, "Button without keycode: 0x%x\n", b->gpio);
			return -EINVAL;
		}
		if (of_property_read_u32(child, "linux,input-type", &b->input_type) < 0)
			b->input_type = 1;
		if (of_property_read_u32(child, "linux,abs-value", &b->abs_value) < 0)
			b->abs_value = 0;
		n++;
	}
	return n ? 0 : -EINVAL;
}

/* ---- joypad_probe @ffffffc008c70780 (body decoded to @..71340; virtual_gamepad part @..7134c..71718 NOT yet). ---- */
struct input_dev *poll_dev_input;	/* T @ffffffc00a4d58a0, 8 bytes */
/* sysfs group: stock @ffffffc00a2a3c88; using joypad_attr_group (generated from the 116 dev_attr_* structs). Not yet checked that its attrs[] order matches joypad_attrs @ffffffc00a2a3cb0. */
/* gpio_hat_value_new/old: defined with joypad_gpio_check below */
int gpio_hat_value_new[16];
int gpio_hat_value_old[16];
int hat_y;					/* T @ffffffc00a4d5b8c */
int hat_x;					/* T @ffffffc00a4d5b88 */
static void joypad_gpio_check(struct input_dev *input);
static void joypad_adc_check(struct input_dev *input);	/* @ffffffc008c75eb4 (0xaa8), stock static */
short lx_g, ly_g, rx_g, ry_g, lt_g, rt_g;	/* T @ffffffc00a4d5b94.. (strh, 16-bit) */
/* joypad_poll @ffffffc008c74ee4: if (poll_dev_private->enable) { gpio_check; adc_check } with input passed through in x0. */
static void joypad_poll(struct input_dev *input)
{
	if (((struct stock_joypad *)poll_dev_private)->enable) {	/* ldrb +0xc @..74ef8 */
		joypad_gpio_check(input);
		joypad_adc_check(input);
	}
}
static int joypad_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stock_joypad *joypad;
	struct stock_amux *amux;
	struct stock_button *buttons;
	struct input_dev *input;
	u32 joypad_revision = 0, joypad_product = 0;	/* locals [x29-0xc], [x29-0x10] */
	int i, err;

	err = stock_joypad_dt_parse_head(dev, &joypad);
	if (err)
		return err;	/* stock: alloc failure returns -ENOMEM without "dt parse error" (@..707e0 -> @..70da0) */
	err = stock_joypad_adc_setup(dev, joypad);
	if (err)
		goto dt_err;	/* @..70914 / @..70c78 -> @..70d8c */
	err = stock_joypad_amux_setup(dev, joypad, &amux);
	if (err) {
		dev_err(dev, "Joypad amux setup failed: %d\n", err);
		goto dt_err;
	}
	*(struct stock_amux **)((char *)joypad + 0x10) = amux;
	err = stock_joypad_gpio_setup(dev, joypad, &buttons);
	if (err)
		goto dt_err;
	*(struct stock_button **)((char *)joypad + 0x30) = buttons;
	dev_info(dev, "%s : adc key cnt = %d, gpio key cnt = %d\n", "joypad_dt_parse",
		 joypad->amux_count, joypad->gpio_key_count);		/* @..70fc0 */

	mutex_init(&joypad->lock);			/* @..70fd8, class "&joypad->lock" */
	platform_set_drvdata(pdev, joypad);				/* pdev+0x88 @..70fe8 */
	err = sysfs_create_group(&dev->kobj, &joypad_attr_group);
	if (err) {
		dev_err(dev, "create sysfs group fail, error: %d\n", err);
		return err;					/* @..71310 -> @..70d98 (no "dt parse error") */
	}

	gts_dev = devm_input_allocate_device(dev);			/* first alloc -> gts_dev @..71008 */
	input = devm_input_allocate_device(dev);			/* second -> poll_dev_input, joypad_input_g */
	poll_dev_input = input;
	joypad_input_g = input;
	device_property_read_string(dev, "joypad-name", &input->name);
	input->phys = "retrogame_joypad/input0";			/* @..71058 */
	device_property_read_u32(dev, "joypad-revision", &joypad_revision);
	device_property_read_u32(dev, "joypad-product", &joypad_product);
	input->id.bustype = 0x19;					/* BUS_HOST; w = 0x484b0019 @..71088 */
	input->id.vendor = 0x484b;
	input->id.product = joypad_product;				/* strh @..71094 */
	input->id.version = joypad_revision;				/* strh @..71098 */
	__set_bit(EV_ABS, input->evbit);				/* orr #0x8 @..7108c */

	for (i = 0; i < joypad->amux_count; i++) {
		struct stock_adc_channel *e = &joypad->adc[i];

		input_set_abs_params(input, e->code, e->min, e->max,
				     joypad->adc_fuzz_v, joypad->adc_flat_v);	/* @..710e8 */
		dev_info(dev, "%s : SCALE = %d, ABS min = %d, max = %d, fuzz = %d, flat = %d, deadzone = %d\n",
			 "joypad_input_setup", e->scale, e->min, e->max,
			 joypad->adc_fuzz_v, joypad->adc_flat_v, joypad->adc_deadzone_v);
		dev_info(dev, "%s : adc tuning_p = %d, adc_tuning_n = %d\n\n",
			 "joypad_input_setup", e->tuning_p, e->tuning_n);
	}
	__set_bit(EV_KEY, input->evbit);				/* orr #0x2 @..7113c */
	for (i = 0; i < joypad->gpio_key_count; i++)
		input_set_capability(input, buttons[i].input_type, buttons[i].code);	/* @..71168 */

	input_set_capability(input, EV_KEY, 0x132);			/* BTN_C   @..7118c */
	input_set_capability(input, EV_KEY, 0x135);			/* BTN_Z */
	input_set_capability(input, EV_KEY, 0x138);			/* BTN_TL2 */
	input_set_capability(input, EV_KEY, 0x139);			/* BTN_TR2 */
	input_set_capability(input, EV_KEY, 0x13c);			/* BTN_MODE */
	input_set_capability(input, EV_KEY, 0x220);			/* BTN_DPAD_UP .. */
	input_set_capability(input, EV_KEY, 0x221);
	input_set_capability(input, EV_KEY, 0x222);
	input_set_capability(input, EV_KEY, 0x223);			/* .. BTN_DPAD_RIGHT @..711fc */
	input_set_abs_params(input, ABS_HAT0X, -1, 1, 0, 0);		/* @..71228 */
	input_set_abs_params(input, ABS_HAT0Y, -1, 1, 0, 0);
	input_set_abs_params(input, ABS_Z, 0, 0x4000, 0x20, 0x20);	/* @..71260 */
	input_set_abs_params(input, ABS_RZ, 0, 0x4000, 0x20, 0x20);
	if (joypad->autorepeat)
		__set_bit(EV_REP, input->evbit);			/* orr #0x100000 @..7128c */

	*(struct device **)joypad = dev;				/* joypad+0x0 @..712a4 */
	err = input_setup_polling(input, joypad_poll);
	if (err)
		goto reg_err;
	input_set_poll_interval(input, joypad->poll_interval);
	err = input_register_device(input);
	if (err)
		goto reg_err;
	if (!gts_dev) {
		dev_err(dev, "no memory for polled device\n");	/* @..712e8, then continues at @..71444 */
	} else {
		/* virtual touch device @..7134c..71440 (gts_dev) */
		__set_bit(BTN_TOUCH, gts_dev->keybit);		/* stock: str 0x400 to keybit word 5 (+0x58) @..71368 */
		input_set_abs_params(gts_dev, ABS_MT_POSITION_X, 0, 640, 0, 0);	/* 0x35, 0x280 */
		input_set_abs_params(gts_dev, ABS_MT_POSITION_Y, 0, 480, 0, 0);	/* 0x36, 0x1e0 */
		input_set_abs_params(gts_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);	/* 0x32 */
		input_set_abs_params(gts_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);	/* 0x30 */
		input_set_abs_params(gts_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);	/* 0x39 @..713dc */
		gts_dev->name = "virtual_gamepad";
		gts_dev->phys = "input/virtual_gamepad";
		gts_dev->id.bustype = 0x18;			/* id word 0x01abcdabcd0018 @..71410 */
		gts_dev->id.vendor = 0xabcd;
		gts_dev->id.product = 0xabcd;
		gts_dev->id.version = 0x1ab;
		set_bit(INPUT_PROP_DIRECT, gts_dev->propbit);	/* atomic, as stock: stset @ffffffc008c71420, LL/SC fallback @..716c8 */
		if (input_register_device(gts_dev))
			printk("Register %s input device failed", gts_dev->name);	/* @..71440, no return */
	}

	jokstick_cal_init();					/* inlined in stock @..71450..71538 */

	/* joypad_open, inlined @..7153c..716a0 */
	for (i = 0; i < joypad->gpio_key_count; i++)
		buttons[i].raw_not_high = buttons[i].not_active_low ^ 1;	/* @..715a0..715a8 */
	mdelay(300);						/* 300 x __const_udelay(0x418958 = 1 ms) @..715c4 */
	for (i = 0; i < joypad->amux_count; i++) {
		struct stock_adc_channel *e = &joypad->adc[i];
		int v = joypad_adc_read(amux, e);

		e->value = v;					/* @..7164c */
		if (!v) {
			dev_err(dev, "%s : saradc channels[%d]!\n", "joypad_open", i);
			continue;
		}
		e->cal = ((i & ~1) == 4) ? 0 : v;		/* @..71610..71618 */
		dev_info(dev, "%s : adc[%d] adc->cal = %d\n", "joypad_open", i, e->cal);
	}
	joypad_adc_check(input);				/* @..7166c */
	joypad_gpio_check(input);				/* @..71670 */
	mutex_lock(&joypad->lock);
	joypad->enable = 0;				/* strb wzr +0xc @..71684 (stock writes 0 here; see report) */
	mutex_unlock(&joypad->lock);
	dev_info(dev, "%s : opened\n", "joypad_open");
	dev_info(dev, "%s : probe success\n", "joypad_probe");
	return 0;

reg_err:
	dev_err(dev, "unable to register polled device, err=%d\n", err);	/* @..71338 */
	dev_err(dev, "input setup failed!(err = %d)\n", err);			/* @..71340 -> @..70d98 */
	return err;
dt_err:
	dev_err(dev, "dt parse error!(err = %d)\n", err);
	return err;
}

/* ---- joypad_gpio_check @ffffffc008c74f14 (0xfa0). Block 1 @..74f34..7502c: sample buttons 0..3 (the d-pad):
 * for each: if gpiod_get_raw_value_cansleep(gpio) < 0 -> dev_err(joypad->dev, "failed to get gpio state\n")
 * (@ffffffc009b5806c, error stubs @..751f4/75208/7521c/75230) and skip; else v = gpiod_get_raw_value(gpio),
 * slot+0x18 = (v != 1), gpio_hat_value_new[i] = 1 - v (str w @..74f74/74fb0/74fec/75028).
 * Blocks 2.. (@..7502c onward: compare against gpio_hat_value_old, hat_y, 25..35 ms usleep_range debounce,
 * remapkey_switch / remapkey_up_mode gating, input_event + *2touchpanel calls) NOT YET WRITTEN. */

/* ---- joypad_gpio_check block 2 @..7502c..75430: hat Y from sw1 "GPIO DPAD-UP" (button 0) and sw2 "GPIO DPAD-DOWN"
 * (button 1), per the stock DTB child order (both linux,input-type 3, linux,code 0x11 = ABS_HAT0Y).
 * hat_y (@ffffffc00a4d5b8c) is internal state: +1 while UP is held, -1 while DOWN is held. The reported value is
 * the opposite sign: UP sends ABS_HAT0Y -1 (@..75364), DOWN sends +1 (@..75684). Debounce = usleep_range(32000, 35000).
 * With remapkey_switch set, the hat drives touch slot 2 (@..58d4) at remapkey_up_x/_y or remapkey_down_x/_y. */
extern int hat_y, hat_x;			/* T @ffffffc00a4d5b8c, @ffffffc00a4d5b88 (defined below) */
/* Hat X (@..75430..757f4) was checked instruction by instruction against hat Y. Same control flow; differences:
 * buttons 2/3 = sw3 "GPIO DPAD-LEFT" / sw4 "GPIO DPAD-RIGHT", state hat_x, ABS_HAT0X (0x10), touch slot 15
 * (@..59a4), remapkey_left_mode/right_mode (@..57d8/@..57dc), remapkey_left_x/_y (@..5750/5754),
 * remapkey_right_x/_y (@..5758/575c). LEFT reports -1 (@..75728), RIGHT +1 (@..75ea8). */

/* ---- tenpad2touchpanel_abs @ffffffc008c6f69c (0x168). Leaf, returns 0. (sel = linux,abs-value selector,
 * axis = ABS_HAT0X 0x10 / ABS_HAT0Y 0x11, pressed). Writes touch slot 2 (@..58d4) for BOTH axes, unlike the hat-X
 * block in joypad_gpio_check which uses slot 15. Guard "mode == 1 && gts_dev" is the ccmp/cset pair (@..6f6d0).
 *   0x11 sel 2 -> UP (remapkey_up_*),  0x11 sel 1 -> DOWN (remapkey_down_*)   (@..6f6b4 / @..6f754)
 *   0x10 sel 2 -> LEFT (remapkey_left_*), 0x10 sel 1 -> RIGHT (remapkey_right_*) (@..6f708 / @..6f798)
 * pressed: slot = {1,1,y,x} (@..6f7dc); released: slot = {1,0} (@..6f7f8). Other sel/axis: nothing. */
noinline int tenpad2touchpanel_abs(int sel, int axis, int pressed)
{
	/* written out per direction as stock (@..6f6ac UP, @..6f754 DOWN, @..6f700 LEFT, @..6f798 RIGHT) */
#define TP_DIR(K)								\
	do {									\
		if (pressed) {							\
			if (remapkey_##K##_mode == 1 && gts_dev) {		\
				*(u64 *)&touch_data[8] = 0x100000001ULL;	\
				touch_data[10] = remapkey_##K##_y;		\
				touch_data[11] = remapkey_##K##_x;		\
			}							\
		} else if (remapkey_##K##_mode == 1 && gts_dev) {		\
			*(u64 *)&touch_data[8] = 1;				\
		}								\
		return 0;							\
	} while (0)
	if (axis == 0x10) {
		if (sel == 1)
			TP_DIR(right);
		if (sel == 2)
			TP_DIR(left);
	} else if (axis == 0x11) {
		if (sel == 1)
			TP_DIR(down);
		if (sel == 2)
			TP_DIR(up);
	}
	return 0;
#undef TP_DIR
}

/* ---- joypad_gpio_check block 4 @..757f4..75e4c: buttons 4.. (only if gpio_key_count >= 5).
 * Slot fields used: gpio +0x08, input_type +0x0c, code +0x10, selector +0x14 (linux,abs-value), last raw +0x18,
 * not_active_low +0x19. "pressed" below = (v == not_active_low) (cset eq @..75b7c etc.). */
extern int remapkey_xbox_switch, remapkey_xbox_switch_bt_tr;	/* @..5714, @..5718 (DEVICE_ATTR vars, defined above) */
/* ---- keypad2touchpanel_abs(code, pressed) @ffffffc008c6f804 (0xe48). Returns 0 on every path.
 * switch(code - 0x130) through the halfword table @ffffffc0097195a0 (15 entries, read from the ELF). Each mapped key has
 * a remapkey_<k>_mode and a touch slot; the slot number is also the value stored in abxy_mask / abxy_z_mask, which the
 * *_ext functions switch on. SELECT/START handle mode 1 only. Verified in full for A (@..6f82c, 6fbbc, 6fda4, 6ffe0, 7002c, 70440, 70494, 704a0,
 * 6ff40, 7063c) and the two thumb keys; the other keys were checked for mode var, slot and x/y loads only.
 *   press:   mode 1 -> slot {1,1,y,x} (gts_dev); mode 2 -> if abxy_mask is 0 or == slot: slot {1,1,y,x} (gts_dev),
 *            abxy_mask = slot; mode 3 -> same with abxy_z_mask.
 *   release: mode 1 -> slot {1,0} (gts_dev); mode 2 -> if abxy_mask == slot: slot {1,0} (gts_dev), abxy_mask = 0
 *            (cleared even without gts_dev, @..6ff48); mode 3 -> same with abxy_z_mask (@..70640).
 *   THUMBL (0x13d): mode 1 as above on slot 12; mode 2 press: if abxy_mask: abxy_mask_l3 = abxy_mask, abxy_mask = 12
 *            (@..6fd4c); mode 2 release: if abxy_mask == 12: slot[abxy_mask_l3] {1,0} (gts_dev), abxy_mask = 0 (@..6ff50).
 *   THUMBR (0x13e): same with mode 3, abxy_z_mask, abxy_z_mask_r3, 13 (@..6fd78, @..6ff9c). */
/* accept: the mask value (besides 0) that still lets a mode 2/3 press through. It equals slot for every key except B:
 * stock B tests `tst mask, #0xfffffffb` (@..70080 for abxy_z_mask, @..700c8 for abxy_mask), i.e. mask in {0, 4},
 * which is Y's slot, then stores 6. Y uses the same test correctly (@..701a8/@..701f0). Kept as in stock.
 * The RG556 .ko keypad2touchpanel_abs has the same 4 `tst ..., #0xfffffffb` sites, so the quirk predates the RG DS. */
static __always_inline void stock_slot_down(int slot, int y, int x)
{
	*(u64 *)&touch_data[slot * 4] = 0x100000001ULL;
	touch_data[slot * 4 + 2] = y;
	touch_data[slot * 4 + 3] = x;
}
static __always_inline void stock_slot_up(int slot) { *(u64 *)&touch_data[(long)slot * 4] = 1; }	/* ldrsw + lsl #4 @..6ff88 */
/* Written out per key, as stock does (each key has its own copy of the logic @ffffffc008c6f828..7064c). */
#define KP_DOWN(SLOT, K)	do { if (gts_dev) stock_slot_down(SLOT, remapkey_##K##_y, remapkey_##K##_x); } while (0)
#define KP_UP(SLOT)		do { if (gts_dev) stock_slot_up(SLOT); } while (0)
#define KP_MASKED(MASK, SLOT, K, ACC)						\
	do {									\
		if (pressed) {							\
			if ((MASK) == (ACC) || (MASK) == 0) {			\
				KP_DOWN(SLOT, K);				\
				(MASK) = (SLOT);				\
			}							\
		} else if ((MASK) == (SLOT)) {					\
			KP_UP(SLOT);						\
			(MASK) = 0;						\
		}								\
	} while (0)
#define KP_KEY(CODE, K, SLOT, ACC)						\
	case CODE:								\
		if (pressed) {							\
			if (remapkey_##K##_mode == 1) {				\
				KP_DOWN(SLOT, K);				\
			} else if (remapkey_##K##_mode == 2) {			\
				if (abxy_mask == (ACC) || abxy_mask == 0) {	\
					KP_DOWN(SLOT, K);			\
					abxy_mask = (SLOT);			\
				}						\
			} else if (remapkey_##K##_mode == 3) {			\
				if (abxy_z_mask == (ACC) || abxy_z_mask == 0) {	\
					KP_DOWN(SLOT, K);			\
					abxy_z_mask = (SLOT);			\
				}						\
			}							\
		} else {							\
			if (remapkey_##K##_mode == 1) {				\
				KP_UP(SLOT);					\
			} else if (remapkey_##K##_mode == 2) {			\
				if (abxy_mask == (SLOT)) {			\
					KP_UP(SLOT);				\
					abxy_mask = 0;				\
				}						\
			} else if (remapkey_##K##_mode == 3) {			\
				if (abxy_z_mask == (SLOT)) {			\
					KP_UP(SLOT);				\
					abxy_z_mask = 0;			\
				}						\
			}							\
		}								\
		return 0;
#define KP_MODE1_ONLY(CODE, K, SLOT)						\
	case CODE:								\
		{	/* mode == 1 && gts_dev folded into one flag (cmp/ccmp/cset @..6fab4) */ \
			bool on = remapkey_##K##_mode == 1 && gts_dev;		\
			if (pressed) {						\
				if (on)						\
					stock_slot_down(SLOT, remapkey_##K##_y, remapkey_##K##_x); \
			} else if (on) {					\
				stock_slot_up(SLOT);				\
			}							\
		}								\
		return 0;
#define KP_THUMB(CODE, K, SLOT, WANT, MASK, SAVED)				\
	case CODE:								\
		if (pressed) {							\
			if (remapkey_##K##_mode == 1) {				\
				KP_DOWN(SLOT, K);				\
			} else if (remapkey_##K##_mode == (WANT)) {		\
				if (MASK) {					\
					(SAVED) = (MASK);			\
					(MASK) = (SLOT);			\
				}						\
			}							\
		} else {							\
			if (remapkey_##K##_mode == 1) {				\
				KP_UP(SLOT);					\
			} else if (remapkey_##K##_mode == (WANT)) {		\
				if ((MASK) == (SLOT)) {				\
					if (gts_dev)				\
						stock_slot_up(SAVED);		\
					(MASK) = 0;				\
				}						\
			}							\
		}								\
		return 0;
int keypad2touchpanel_abs(int code, int pressed)
{
	switch (code) {	/* halfword jump table @ffffffc0097195a0 */
	KP_KEY(0x130, a, 5, 5)
	KP_KEY(0x131, b, 6, 6)
	KP_KEY(0x133, x, 3, 3)
	KP_KEY(0x134, y, 4, 4)
	KP_KEY(0x136, l1, 14, 14)
	KP_KEY(0x137, r1, 10, 10)
	KP_KEY(0x138, l2, 9, 9)
	KP_KEY(0x139, r2, 11, 11)
	KP_MODE1_ONLY(0x13a, select, 7)	/* SELECT/START: mode 1 only (@..6faa8, @..6faec) */
	KP_MODE1_ONLY(0x13b, start, 8)
	KP_THUMB(0x13d, left_joy_mid, 12, 2, abxy_mask, abxy_mask_l3)
	KP_THUMB(0x13e, right_joy_mid, 13, 3, abxy_z_mask, abxy_z_mask_r3)
	default:
		return 0;	/* 0x132, 0x135, 0x13c and outside 0x130..0x13e */
	}
}
static __always_inline void stock_report_trigger(struct stock_button *b, int code, int abs, int pressed)
{
	/* @..75a08 (TL2 0x138 / ABS_Z 2) and @..75a4c (TR2 0x139 / ABS_RZ 5) */
	if (remapkey_xbox_switch_bt_tr == 1) {
		input_event(poll_dev_input, EV_ABS, abs, pressed << 14);	/* @..75b1c */
	} else if (remapkey_xbox_switch_bt_tr == 2) {
		input_event(poll_dev_input, b->input_type, code, pressed);
		input_event(poll_dev_input, EV_ABS, abs, pressed << 14);
	} else {
		input_event(poll_dev_input, b->input_type, code, pressed);	/* @..75b2c / @..75b64 */
	}
}
static __always_inline void stock_gpio_buttons(void)
{
	struct stock_joypad *joypad = poll_dev_private;
	struct device *dev = *(struct device **)joypad;
	int i, v;

	for (i = 4; i < joypad->gpio_key_count; i++) {
		struct stock_button *b = *(struct stock_button **)((char *)joypad + 0x30) + i;
		int pressed, code;

		if (gpiod_get_raw_value_cansleep(gpio_to_desc(b->gpio)) < 0) {
			dev_err(dev, "failed to get gpio state\n");	/* @..759f0 */
			continue;
		}
		v = gpiod_get_raw_value(gpio_to_desc(b->gpio));
		if (v == b->raw_not_high)			/* unchanged @..75870 */
			continue;
		pressed = (v == b->not_active_low);

		if (b->abs_value == 0) {			/* selector 0 @..75888 */
			if (remapkey_switch) {
				keypad2touchpanel_abs(b->code, pressed);	/* @..75824 */
			} else {
				code = b->code;
				if (remapkey_xbox_switch) {	/* byte table @ffffffc0097195d8, read from the ELF */
					switch (code) {
					case 0x130: code = 0x131; break;	/* @..75958 */
					case 0x131: code = 0x130; break;	/* @..75a9c */
					case 0x133: code = 0x134; break;	/* @..75aac */
					case 0x134: code = 0x133; break;	/* @..75abc */
					default: break;			/* 0x132, 0x135..0x137 -> @..75a90 */
					}
				}
				if (code == 0x138)
					stock_report_trigger(b, 0x138, ABS_Z, pressed);
				else if (code == 0x139)
					stock_report_trigger(b, 0x139, ABS_RZ, pressed);
				else
					input_event(poll_dev_input, b->input_type, code, pressed);	/* @..75b80 */
			}
		} else if (!remapkey_switch) {
			/* selector 1/2 without remap: @..7588c -> @..75828 (no event) */
		} else {
			/* selector 1/2 with remapkey_switch (@..758a0..75e48): a button acting as one half of a hat axis.
			 * val = +pressed for sel 1 (cset @..758c0) or -pressed for sel 2 (csetm @..75988).
			 * If the opposite half is still recorded as held (old == -1 for sel 1, +1 for sel 2) and this
			 * one is pressed: mark the neighbour button's last-raw byte (+1 slot for sel 1 @..758e8,
			 * -1 slot for sel 2 @..759ac) as 1, and release touch slot 2 if this direction's mode == 1.
			 * Then tenpad2touchpanel_abs(sel, code, val) and old = val. Only codes 0x10 / 0x11 act. */
			/* written out per selector and axis as stock (four tenpad2touchpanel_abs call sites) */
/* After the neighbour mark stock re-reads code and selector and picks the direction's mode again
			 * (@..758ec..7591c, @..75c44..75c90); pressed is recomputed from b->not_active_low each use (cset). */
#define SEL_NB_RELEASE()								\
			do {								\
				int m_ = 0;						\
				if (b->code == 0x10) {					\
					if (b->abs_value == 1) m_ = remapkey_right_mode; \
					else if (b->abs_value == 2) m_ = remapkey_left_mode; \
					else break;					\
				} else if (b->code == 0x11) {				\
					if (b->abs_value == 1) m_ = remapkey_down_mode; \
					else if (b->abs_value == 2) m_ = remapkey_up_mode; \
					else break;					\
				} else {						\
					break;						\
				}							\
				if (m_ == 1 && gts_dev)					\
					*(u64 *)&touch_data[8] = 1;			\
			} while (0)
#define SEL_HALF(SEL, AX, OPP, NB)							\
			do {								\
				abs_value_##AX##_value = (SEL) == 1 ? (v == b->not_active_low) : -(v == b->not_active_low); \
				if (abs_value_##AX##_value_old == (OPP) && v == b->not_active_low) { \
					(b + (NB))->raw_not_high = 1;			\
					SEL_NB_RELEASE();				\
				}							\
				tenpad2touchpanel_abs(b->abs_value, b->code,	\
					(SEL) == 1 ? (v == b->not_active_low) : -(v == b->not_active_low)); \
				abs_value_##AX##_value_old = (SEL) == 1 ? (v == b->not_active_low) : -(v == b->not_active_low); \
			} while (0)
			if (b->abs_value == 1) {			/* @..758a0 */
				if (b->code == 0x10)
					SEL_HALF(1, x, -1, 1);
				if (b->code == 0x11)
					SEL_HALF(1, y, -1, 1);
			} else if (b->abs_value == 2) {			/* @..75968 */
				if (b->code == 0x10)
					SEL_HALF(2, x, 1, -1);
				if (b->code == 0x11)
					SEL_HALF(2, y, 1, -1);
			}
#undef SEL_HALF
#undef SEL_NB_RELEASE
		}
		b->raw_not_high = (v != 0);			/* strb @..75830 */
	}
}
/* Hat block written out per axis, with stock's shared press tails (Y @..7502c..75430, X @..75430..757f4). */
#define HAT_DEBOUNCE()	usleep_range(32000, 35000)
#define HAT_LEAVE(CODE, SLOT, NK, PK)						\
	do {									\
		if (!remapkey_switch) {						\
			input_event(poll_dev_input, EV_ABS, CODE, 0);		\
		} else {							\
			HAT_DEBOUNCE();						\
			if (remapkey_##NK##_mode == 1 && gts_dev)		\
				stock_slot_up(SLOT);				\
			if (remapkey_##PK##_mode == 1 && gts_dev)		\
				stock_slot_up(SLOT);				\
			g_touch_sync();						\
		}								\
		input_event(poll_dev_input, EV_SYN, SYN_REPORT, 0);		\
	} while (0)
#define HAT_PRESS(CODE, SLOT, K, VAL)						\
	do {									\
		HAT_DEBOUNCE();							\
		if (!remapkey_switch) {						\
			input_event(poll_dev_input, EV_ABS, CODE, VAL);		\
		} else {							\
			HAT_DEBOUNCE();						\
			if (remapkey_##K##_mode == 1 && gts_dev)		\
				stock_slot_down(SLOT, remapkey_##K##_y, remapkey_##K##_x); \
			g_touch_sync();						\
		}								\
		input_event(poll_dev_input, EV_SYN, SYN_REPORT, 0);		\
	} while (0)
/* hat values re-read from gpio_hat_value_new[] at each test, as stock (no locals, @..7505c) */
#define HAT_BLOCK(IDX, ST, CODE, SLOT, NK, PK, L)				\
	do {									\
		if (gpio_hat_value_old[IDX] == gpio_hat_value_new[IDX] && gpio_hat_value_old[(IDX) + 1] == gpio_hat_value_new[(IDX) + 1]) \
			break;							\
		if (!(gpio_hat_value_new[IDX] | gpio_hat_value_new[(IDX) + 1])) {						\
			if (ST) {						\
				ST = 0;						\
				HAT_DEBOUNCE();					\
				HAT_LEAVE(CODE, SLOT, NK, PK);			\
			}							\
		} else if (gpio_hat_value_new[IDX] == 1 && gpio_hat_value_new[(IDX) + 1] == 0) {				\
			if (ST == -1) {						\
				ST = 1;						\
				HAT_DEBOUNCE();					\
				HAT_LEAVE(CODE, SLOT, NK, PK);			\
				goto L##_neg;					\
			} else if (ST == 0) {					\
				ST = 1;						\
				goto L##_neg;					\
			}							\
		} else if (gpio_hat_value_new[IDX] == 0 && gpio_hat_value_new[(IDX) + 1] == 1) {				\
			if (ST == 1) {						\
				ST = -1;					\
				HAT_DEBOUNCE();					\
				HAT_LEAVE(CODE, SLOT, NK, PK);			\
				goto L##_pos;					\
			} else if (ST == 0) {					\
				ST = -1;					\
				goto L##_pos;					\
			}							\
		}								\
		goto L##_copy;							\
L##_neg:	HAT_PRESS(CODE, SLOT, NK, -1);					\
		goto L##_copy;							\
L##_pos:	HAT_PRESS(CODE, SLOT, PK, 1);					\
L##_copy:	gpio_hat_value_old[IDX] = gpio_hat_value_new[IDX];					\
		gpio_hat_value_old[(IDX) + 1] = gpio_hat_value_new[(IDX) + 1];				\
	} while (0)
static noinline void joypad_gpio_check(struct input_dev *input)
{
	struct stock_joypad *joypad = poll_dev_private;
	struct stock_button *b = *(struct stock_button **)((char *)joypad + 0x30);
	struct device *dev = *(struct device **)joypad;
	int i, v;

	for (i = 0; i < 4; i++) {
		if (gpiod_get_raw_value_cansleep(gpio_to_desc(b[i].gpio)) < 0) {
			dev_err(dev, "failed to get gpio state\n");
			continue;
		}
		v = gpiod_get_raw_value(gpio_to_desc(b[i].gpio));
		b[i].raw_not_high = (v != 1);
		gpio_hat_value_new[i] = 1 - v;
	}
	HAT_BLOCK(0, hat_y, ABS_HAT0Y, 2, up, down, haty);	/* block 2 @..7502c: UP reports -1, DOWN +1 */
	HAT_BLOCK(2, hat_x, ABS_HAT0X, 15, left, right, hatx);	/* block 3 @..75430: LEFT -1, RIGHT +1 */
	stock_gpio_buttons();				/* block 4 @..757f4 */
	input_event(poll_dev_input, EV_SYN, SYN_REPORT, 0);	/* tail @..75e5c */
	g_touch_sync();						/* @..75e60 */
	if (remapkey_switch && abs_z_mode2)			/* @..75e64..75e74 */
		joypad2touchpanel_abs_z_mode2();			/* @..75e78 */
}

/* joypad_gpio_check block 2 (@..7502c..75244), DECODED BUT NOT YET C (several targets @..75244..75430 still open).
 * Hat Y from d-pad buttons 0 (up?) and 1 (down?), using gpio_hat_value_new/old[0..1] and hat_y (@ffffffc00a4d5b8c):
 *   if (old[0]==new[0] && old[1]==new[1]) -> @..75430 (no change path, open)
 *   else if (new[0]|new[1]) == 0  (release):
 *       if (hat_y == 0) -> @..75418 (open); else { hat_y = 0; usleep_range(32000, 35000);
 *         if (!remapkey_switch) input_event(poll_dev_input, EV_ABS(3), ABS_HAT0Y(0x11), 0) -> @..75404
 *         else { usleep_range(32000, 35000);
 *                if (remapkey_up_mode == 1 && gts_dev) touch slot 2 (@..58d4) = {1,0};
 *                if (remapkey_down_mode == 1 && gts_dev) touch slot 2 = {1,0}; -> @..75400 } }
 *   else if (new[0]==1 && new[1]==0): if hat_y == -1 -> @..7516c (hat_y = 1, debounce, remap release then
 *       g_touch_sync, -> @..752e0); else if hat_y == 0 { hat_y = 1; -> @..752f4 } else -> @..75418
 *   else if (new[0]==0 && new[1]==1): if hat_y == 1 -> @..75244 (hat_y = -1, debounce ...); else if hat_y == 0
 *       { hat_y = -1; -> @..75398 } else -> @..75418
 * Annotated listing of the whole remaining function: .gpioc.annot.txt (global names resolved). */

/* joypad_gpio_check block 4 (@..757f4..75e4c), PARTIAL DECODE, NOT YET C.
 * Loop i = 4 .. gpio_key_count-1 (only if gpio_key_count >= 5, @..757f8). x26 = buttons + 0x20*i - 8, so in slot terms:
 *   gpio = slot+0x08; last raw value byte = slot+0x18 (compared with the new raw value, @..75870; same byte
 *   block 1 fills with (v != 1) for buttons 0..3); code = slot+0x10; not_active_low = slot+0x19; slot+0x14 used as a
 *   0/1/2 selector (@..75884..7589c; this is the "linux,abs-value" field).
 *   cansleep read < 0 -> "failed to get gpio state\n" (@..759f0), next button.
 *   unchanged -> next button. Changed (v):
 *     selector 0: if !remapkey_switch -> @..75920 (remapkey_xbox_switch, key-code remap via byte jump table
 *        @ffffffc0097195d8 for codes 0x130..0x139, input_event on poll_dev_input), else
 *        keypad2touchpanel_abs(code, v == not_active_low) (@..75824); then slot+0x18 = (v != 0).
 *     selector 1 / 2 with remapkey_switch: code 0x10 (ABS_HAT0X) / 0x11 (ABS_HAT0Y) handled through
 *        abs_value_x_value(_old) / abs_value_y_value(_old) with +1 / -1, and remapkey_*_mode touch paths
 *        (@..758a0..75cf0). Not decoded further yet.
 * NOTE: the annotator's "=0xffffffc00a4d50xx" names in this range are struct-field offsets, not globals. */

/* ---- joypad_adc_check @ffffffc008c75eb4 (0xaa8). Blocks A-D reconstructed from the stock listing. */
static noinline void joypad_adc_check(struct input_dev *input)
{
	struct stock_joypad *joypad = poll_dev_private;
	struct stock_amux *amux = *(struct stock_amux **)((char *)joypad + 0x10);
	struct stock_adc_channel *a;
	int i;

	for (i = 0; i < joypad->amux_count; i++)		/* @..75ef4 */
		joypad->adc[i].value = joypad_adc_read(amux, &joypad->adc[i]);
	a = joypad->adc;
	lx_g = a[0].value; ly_g = a[1].value; rx_g = a[2].value; ry_g = a[3].value;	/* strh @..75f40.. */
	lt_g = 0; rt_g = 0;
	if (count_joypad_data > 3) {				/* @..75f6c: wrap without storing */
		count_joypad_data = 0;
	} else {
		/* written out as stock: each statement re-reads joypad->adc (@..75f7c..75fe4) */
		joypad->adc[0].hist[count_joypad_data] = lx_g;
		joypad->adc[1].hist[count_joypad_data] = ly_g;
		joypad->adc[2].hist[count_joypad_data] = rx_g;
		joypad->adc[3].hist[count_joypad_data] = ry_g;
		joypad->adc[4].hist[count_joypad_data] = lt_g;
		joypad->adc[5].hist[count_joypad_data] = rt_g;
		count_joypad_data++;
	}
	/* Fixed 6 channels and 4 taps regardless of amux_count (unrolled @..75ff8..761d4). */
	joypad->adc[0].sum += joypad->adc[0].hist[0];
	joypad->adc[1].sum += joypad->adc[1].hist[0];
	joypad->adc[2].sum += joypad->adc[2].hist[0];
	joypad->adc[3].sum += joypad->adc[3].hist[0];
	joypad->adc[4].sum += joypad->adc[4].hist[0];
	joypad->adc[5].sum += joypad->adc[5].hist[0];
	joypad->adc[0].sum += joypad->adc[0].hist[1];
	joypad->adc[1].sum += joypad->adc[1].hist[1];
	joypad->adc[2].sum += joypad->adc[2].hist[1];
	joypad->adc[3].sum += joypad->adc[3].hist[1];
	joypad->adc[4].sum += joypad->adc[4].hist[1];
	joypad->adc[5].sum += joypad->adc[5].hist[1];
	joypad->adc[0].sum += joypad->adc[0].hist[2];
	joypad->adc[1].sum += joypad->adc[1].hist[2];
	joypad->adc[2].sum += joypad->adc[2].hist[2];
	joypad->adc[3].sum += joypad->adc[3].hist[2];
	joypad->adc[4].sum += joypad->adc[4].hist[2];
	joypad->adc[5].sum += joypad->adc[5].hist[2];
	joypad->adc[0].sum += joypad->adc[0].hist[3];
	joypad->adc[1].sum += joypad->adc[1].hist[3];
	joypad->adc[2].sum += joypad->adc[2].hist[3];
	joypad->adc[3].sum += joypad->adc[3].hist[3];
	joypad->adc[4].sum += joypad->adc[4].hist[3];
	joypad->adc[5].sum += joypad->adc[5].hist[3];
	joypad->adc[0].value = joypad->adc[0].sum / 4;
	joypad->adc[1].value = joypad->adc[1].sum / 4;
	joypad->adc[2].value = joypad->adc[2].sum / 4;
	joypad->adc[3].value = joypad->adc[3].sum / 4;
	joypad->adc[4].value = joypad->adc[4].sum / 4;
	joypad->adc[5].value = joypad->adc[5].sum / 4;
	joypad->adc[0].sum = 0;
	joypad->adc[1].sum = 0;
	joypad->adc[2].sum = 0;
	joypad->adc[3].sum = 0;
	joypad->adc[4].sum = 0;
	joypad->adc[5].sum = 0;
	/* Block C @..762b0..7644c: centre each stick channel. Table @ffffffc0097195e2 = {0,0xa,0x14,0x1e} words
	 * -> i0 @..76378, i1 @..763a0, i2 @..763c8, i3 @..763f0. Centre = (lr + rl) / 2 (signed, cinc/asr) when the
	 * side's jokstick_cal_switch is set, else the channel's own cal (+0x4c). value = value - centre and
	 * adc_abs_<axis> = |value| (cneg mi @..76330). Channels > 3 only copy value to shown_p. */
	for (i = 0; i < joypad->amux_count; i++) {
		struct stock_adc_channel *e = &a[i];
		int v = e->value, *absv;

		e->shown_p = v;					/* str @..7635c */
		if (i > 3)
			continue;
		switch (i) {
		case 0: if (jokstick_cal_switch_l) v -= (jokstick_cal_lxlr + jokstick_cal_lxrl) / 2; else v -= e->cal; absv = &adc_abs_lx; break;
		case 1: if (jokstick_cal_switch_l) v -= (jokstick_cal_lyud + jokstick_cal_lydu) / 2; else v -= e->cal; absv = &adc_abs_ly; break;
		case 2: if (jokstick_cal_switch_r) v -= (jokstick_cal_rxlr + jokstick_cal_rxrl) / 2; else v -= e->cal; absv = &adc_abs_rx; break;
		case 3: if (jokstick_cal_switch_r) v -= (jokstick_cal_ryud + jokstick_cal_rydu) / 2; else v -= e->cal; absv = &adc_abs_ry; break;
		}
		e->value = v;					/* str @..7632c; each case subtracts its own centre (@..7636c..76410) */
		*absv = v < 0 ? -v : v;
	}
	/* Block D @..76450..7695c. /1000 = smull 0x10624dd3, asr #38 (2^38 / 0x10624dd3 = 1000.0);
	 * *98/100 = mul 0x62, smull 0x51eb851f, asr #37. */
	for (i = 0; i < joypad->amux_count; i++) {
		struct stock_adc_channel *e = &a[i];
		int v = e->value, hi, lo, out = 0, sc;

		/* D1 written out per axis (jump table @ffffffc0097195e6 -> @..764ec/76544/76594/765e4) */
#define D1_AXIS(AX, AY, SW, PS, NS)							\
		do {									\
			out = (adc_abs_##AX > jokstick_cal_deadzone ||			\
			       adc_abs_##AY > jokstick_cal_deadzone);			\
			if (!jokstick_cal_switch_##SW)					\
				goto d1_plain;						\
			if (!out) {	/* zero path joins the shared multiply (@..76684) */	\
				v = 0;							\
				e->value = 0;						\
				e->shown = 0;						\
				sc = jokstick_cal_##NS##_scale;				\
			} else {							\
				v = e->value;						\
				e->shown = v;						\
				sc = v > 0 ? jokstick_cal_##PS##_scale			\
					   : jokstick_cal_##NS##_scale;			\
			}								\
			v = v * sc / 1000;						\
			e->value = v;							\
			v = v * jokstick_cal_scale / 1000;				\
			e->value = v;							\
			goto d1_done;							\
		} while (0)
		switch (i) {
		case 0: D1_AXIS(lx, ly, l, lxr, lxl);
		case 1: D1_AXIS(lx, ly, l, lyd, lyu);
		case 2: D1_AXIS(rx, ry, r, rxr, rxl);
		case 3: D1_AXIS(rx, ry, r, ryd, ryu);
		default: goto d1_done;	/* channels 4/5 keep their value (@..7653c) */
		}
d1_plain:						/* switch off, shared tail @..76634 */
		if (out) {
			v = e->value;				/* ldr @..76638 */
		} else {
			e->value = 0;				/* str wzr @..76640 */
			v = 0;
		}
		v = v * jokstick_cal_scale / 1000;
		e->value = v;
		e->shown = e->invert ? -v : v;
d1_done:
#undef D1_AXIS
		/* D2 clamp @..7671c..767b4 */
		hi = e->max * 98 / 100;
		if (v > hi) {
			v = e->max;
			e->value = v;
		}
		lo = e->min * 98 / 100;
		{
			int w12 = (v < lo) ? e->min : v;		/* csel lt @..76764 */
			int w13 = (w12 < e->max) ? w12 : e->max;	/* csel lt @..7676c */
			int w8 = (w13 > e->min) ? w13 : e->min;		/* csel gt @..76774 */

			if (v < lo || w12 > e->max || w13 < e->min)	/* @..76778..76788 */
				e->value = w8;
			v = w8;
		}
		if (e->shown > e->max || e->shown < e->min)	/* @..76794..767b4: stock clamps min first, then max */
			e->shown = max_t(int, min_t(int, e->shown, e->max), e->min);

		/* D3 @..767b8 */
		if (v != e->last) {
			if (!remapkey_switch) {
				if (e->code != 2 && e->code != 5)	/* @..767f4..76800 */
					input_event(poll_dev_input, EV_ABS, e->code, e->invert ? -v : v);	/* @..76498 */
			} else {
				int last = e->last, r;
				int av = v < 0 ? -v : v;

				if (av < 0x168)					/* 360 @..767dc */
					r = 0;
				else if (last - 0x140 < v && v < last + 0x140)	/* 320 @..76808 */
					r = last;
				else if (last - 0x280 < v && v < last + 0x280)	/* 640 @..76824 */
					r = (3 * last + v) / 4;
				else if (last - 0x500 < v && v < last + 0x500)	/* 1280 @..76858 */
					r = (last + v) / 2;
				else
					r = v;
				e->last = r;					/* @..76894 */
				e->value = r;					/* @..7689c */
				if ((unsigned int)e->code <= 1)
					joypad2touchpanel_abs(e->code, e->invert ? -r : r);	/* @..768b4 */
				if (e->code == 3)
					joypad2touchpanel_abs_z(2, e->invert ? -e->value : e->value);	/* @..768e4 */
				if (e->code == 4)
					joypad2touchpanel_abs_z(5, e->invert ? -e->value : e->value);	/* @..7691c */
			}
		}
		e->last = e->value;				/* @..764a4..764b0 */
	}
	input_event(poll_dev_input, EV_SYN, SYN_REPORT, 0);	/* @..76938 */
}

_Static_assert(sizeof(struct mutex) <= 0x20, "stock struct joypad has 0x20 bytes for the mutex (+0x50..+0x70)");
_Static_assert(__builtin_offsetof(struct stock_joypad, amux_debug) == 0x70, "jp+0x70");

/* ---- Remaining stock attributes (stock @ffffffc008c716e4..71c58, dev_attr_* @ffffffc00a2a4080..4140,
 * all 0644; adc_fuzz/adc_flat have no store). */
static ssize_t joypad_show_poll_interval(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stock_joypad *joypad = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", joypad->poll_interval);		/* @..71704 */
}
static ssize_t joypad_store_poll_interval(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct stock_joypad *joypad = dev_get_drvdata(dev);

	mutex_lock(&joypad->lock);
	joypad->poll_interval = simple_strtoul(buf, NULL, 10);		/* @..71758; polling rate itself not updated */
	mutex_unlock(&joypad->lock);
	return count;
}
static ssize_t joypad_show_adc_fuzz(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ((struct stock_joypad *)dev_get_drvdata(dev))->adc_fuzz_v);	/* +0x3c */
}
static ssize_t joypad_show_adc_flat(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ((struct stock_joypad *)dev_get_drvdata(dev))->adc_flat_v);	/* +0x40 */
}
static ssize_t joypad_show_adc_cal(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stock_joypad *joypad = dev_get_drvdata(dev);
	ssize_t len = 0;
	int i;

	for (i = 0; i < joypad->amux_count; i++)			/* @..718cc */
		len += sprintf(buf + len, "adc[%d]->cal = %d\n", i, joypad->adc[i].cal);
	len += sprintf(buf + len, "adc scale = %d\n", joypad->adc_scale);	/* @..71914 */
	return len;
}
static ssize_t joypad_store_adc_cal(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct stock_joypad *joypad = dev_get_drvdata(dev);
	int i, v;

	if (!simple_strtoul(buf, NULL, 10))				/* @..71968 */
		return count;
	mutex_lock(&joypad->lock);
	for (i = 0; i < joypad->amux_count; i++) {
		struct stock_adc_channel *e = &joypad->adc[i];

		v = joypad_adc_read(*(struct stock_amux **)((char *)joypad + 0x10), e);	/* amux re-read each pass @..719a0 */
		e->value = v;					/* @..719b0 */
		if (v)
			e->cal = v;				/* @..719b8 (no channel 4/5 exception, unlike probe) */
		else
			dev_err(*(struct device **)joypad, "%s : saradc channels[%d]!\n", "joypad_store_adc_cal", i);
	}
	mutex_unlock(&joypad->lock);
	return count;
}
static ssize_t joypad_show_amux_debug(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stock_joypad *joypad = dev_get_drvdata(dev);
	ssize_t len;

	mutex_lock(&joypad->lock);
	if (joypad->enable)
		joypad->enable = 0;					/* @..71bcc: reading this attr stops polling */
	len = sprintf(buf, "amux ch[%d], adc scale = %d, adc value = %d\n",
		      joypad->amux_debug, joypad->adc_scale, 0);	/* @..71be8, value is a literal 0 */
	mutex_unlock(&joypad->lock);
	return len;
}
static ssize_t joypad_store_amux_debug(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct stock_joypad *joypad = dev_get_drvdata(dev);
	int v = simple_strtoul(buf, NULL, 10);

	joypad->amux_debug = (joypad->amux_count < v) ? 0 : v;		/* cmp/csel @..71c3c..71c48, no lock */
	return count;
}
static DEVICE_ATTR(poll_interval, 0644, joypad_show_poll_interval, joypad_store_poll_interval);
static DEVICE_ATTR(adc_fuzz, 0644, joypad_show_adc_fuzz, NULL);
static DEVICE_ATTR(adc_flat, 0644, joypad_show_adc_flat, NULL);
static DEVICE_ATTR(adc_cal, 0644, joypad_show_adc_cal, joypad_store_adc_cal);
static DEVICE_ATTR(amux_debug, 0644, joypad_show_amux_debug, joypad_store_amux_debug);

/* RG556 .ko cross-check of joypad_gpio_check (.ko 0x49e4, size 0x820 vs 0xfa0 on RG DS), by relocation names:
 * shared with RG DS: the selector 1/2 hat-half path (abs_value_x/y_value(_old), 4 x tenpad2touchpanel_abs,
 * remapkey_up/down/left/right_mode, touch_data+0x20), keypad2touchpanel_abs dispatch, remapkey_xbox_switch, and the
 * tail (g_touch_sync, abs_z_mode2 -> joypad2touchpanel_abs_z_mode2). Present only on RG DS: the hat_x/hat_y state
 * machines with gpio_hat_value_new/old and the 32-35 ms usleep_range debounce, and remapkey_xbox_switch_bt_tr
 * (TL2/TR2 as ABS_Z/ABS_RZ). The RG556 revision has no d-pad hat block; RG DS bytes are the authority. */
