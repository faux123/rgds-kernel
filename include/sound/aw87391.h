/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Speaker PA hooks exported by sound/soc/rockchip/aw_pa_{left,right}.c (stock
 * RG DS layout), called from rk817_digital_mute.
 */
#ifndef __SOUND_AW87391_H__
#define __SOUND_AW87391_H__

#if IS_ENABLED(CONFIG_SND_SOC_AW87391)
int enable_pa_spk_left(void);
int disable_pa_spk_left(void);
int enable_pa_spk_right(void);
int disable_pa_spk_right(void);
#else
static inline int enable_pa_spk_left(void) { return -1; }
static inline int disable_pa_spk_left(void) { return -1; }
static inline int enable_pa_spk_right(void) { return -1; }
static inline int disable_pa_spk_right(void) { return -1; }
#endif

#endif /* __SOUND_AW87391_H__ */
