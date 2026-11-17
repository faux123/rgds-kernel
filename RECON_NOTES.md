# RG DS stock kernel reconstruction notes (data file)

## Base
- Clone: rockchip-linux/kernel develop-6.1, --filter=blob:none, at /extra2/projects/home/rgds-kernel/src
- Branch rgds-stock-recon at af8041e6d1ed3aac70225688ae3a1cd54b87e0a2 (= 8c22c7c1c443^), 2025-12-05 "ARM: configs: move gst412c to rv1126b-evb.config"
- Makefile at that commit: VERSION 6, PATCHLEVEL 1, SUBLEVEL 141
- rga_drv.h at that commit: DRIVER_REVISION_VERSION 10 (matches stock 1.3.10)

## Toolchain
- No clang-r487747c on desktop (find /extra2 -maxdepth 5 empty).
- Gamma README recipe (sparse clone of HEAD) FAILS now: AOSP main-kernel HEAD dc706f7fe714 (2026-11-10) no longer contains clang-r487747c.
- Gitiles: clang-r487747c exists on refs/heads/android14-release (HTTP 200). Downloading +archive tarball to toolchain/clang-r487747c.

## Overlay (Gamma ba9053b2903c8cbbdc996fb08b4a09982beacd3c), raw files in overlay-ba9053b/
- Commit 37f59eff3870: singleadcjoy.c + Kconfig/Makefile lines (from ba9053b)
- Commit 1a3164c59674: drivers/staging/android timed_output/timed_gpio/rk_timed_gpio (+ hooked into drivers/staging/Kconfig and Makefile; base has no staging/android dir)
- rk817_codec.c in Gamma's tree also carries a later sr_val/ADC-rate rework from its 6.1.172 base: take ONLY the rk817_hp_inserted export hunk.
- Gamma's dts Makefile at ba9053b has no "anbernic" line (grep empty); dts needs its own Makefile line.

## Session 2 (2026-11-16 20:23)
- Toolchain: toolchain/clang-r487747c from AOSP refs/heads/android14-release +archive. `clang --version`: Android (10087095, +pgo, +bolt, +lto, -mlgo, based on r487747c) clang version 17.0.2 (https://android.googlesource.com/toolchain/llvm-project d9f89f4d16663d5012e5c09495f3b30ece3d2362)
- Commits: 2932c5cbc15f rk817 export hunk only; a808661c8a56 dts + Makefile line; 866e02d3cec7 xt_quota2
- olddefconfig diff vs stock: config-diff.txt (297 lines). 71 stock symbols vanished. Key: ACK android14-6.1 features absent from develop-6.1 (ANDROID_VENDOR_HOOKS, ASHMEM, FUSE_BPF, INCREMENTAL_FS, GKI_HIDDEN_*, CPU_FREQ_TIMES, DM_USER, DM_DEFAULT_KEY, PER_VMA_LOCK, ARM_PMUV3, USB_CONFIGFS_F_ACC/AUDIO_SRC, UID_SYS_STATS...)
- Remote heads on rockchip-linux/kernel: only develop-6.1 and develop-6.12 (no android branch).
- Build attempt 1 FAILED: `make[1]: *** [Kbuild:24: include/generated/timeconst.h] Error 127` -> `bc` not installed (command -v bc empty; sudo -n pacman: "sudo: a password is required"). Local gavinhoward/bc 7.0.3 build blocked by the permission classifier. Needs Paul: `sudo pacman -S bc`.

## Session 3 (2026-11-16 20:39)
- 07b03df05d28 sc7u22 accel/gyro integrated; enum count: ACCEL_ID_SC7U22=39 (0x27), GYRO_ID_SC7U22=67 (0x43).
- develop-6.1 history (to af8041e6d1ed): 4391 "ANDROID:" / 851 "FROMGIT:" / 5010 "UPSTREAM:" / 1105 "FROMLIST:" subjects, but fs/incfs never existed there and no ashmem/vendor_hooks infra -> ACK cherry-picks only, no ACK merge.
- ACK SUBLEVEL map (Makefile line 4 via gitiles): 2025-06_r1=138, 2025-07_r1=141, 2025-08_r1=145, 2025-09_r1=145, 2025-12_r1=157.
- Last 141 tag: android14-6.1-2025-07_r13 (tag obj 304c4c13d06f, commit 341eb27890fb, SUBLEVEL 141). Fetching as remote aosp, blobless.
- Fetched tag android14-6.1-2025-07_r13 -> commit 341eb27890fb80325847a445eaa4ece551dc3584 (remote aosp, blobless).
- merge-base --all rgds-stock-recon vs ACK: 58485ff1a74f (Linux 6.1.141, 2025-06-04) and b9e54f8e02e2 (2020-12-14 "Merge tag 'v5.10' into android-mainline") -> develop-6.1 descends from android-mainline 5.10 history; Android-only files were dropped.
- 8388 ACK commits past 58485ff; 123777 Rockchip-side commits past it.
- Branch rgds-ack-merge (from rgds-stock-recon 07b03df05d28): `git merge --no-commit --no-ff android14-6.1-2025-07_r13` -> 206 conflicts (144 content, 60 modify/delete, 2 add/add). Left unresolved, merge in progress. Log: ack-merge.log.

## Session 4 (2026-11-16 20:44)
- Merge committed: 14b8d2d71bd6 on rgds-ack-merge. merge-resolution.tsv: 201 ACK, 3 ROCKCHIP, 2 BOTH (staging/android Kconfig+Makefile), 144 REVIEW. drivers/staging/{Kconfig,Makefile} de-duplicated by hand.
- Build in docker panvk-build:trixie (+apt make bc flex bison libssl-dev libelf-dev), setpriv to uid 1000; script build-ack.sh; log build-ack.log. Stock config copied to stock/ (kbase not mounted).
- olddefconfig vanished list 71 -> 22 (config-diff-ack.txt). New vanished Rockchip symbols (RK628_MISC*, SCSI_UFS_ROCKCHIP, VEHICLE_*, PCIE_FUNC_RKEP, RK803, ROCKCHIP_THUNDER_BOOT_RK628) = Rockchip Kconfig lines lost by taking ACK on content conflicts.
- Build FAILED (stopped): blk-crypto.h bi_skip_dm_default_key, soc-core.c soc_multi_mixer_control, aspm.c pci_clear_and_set_config_dword, sched/core.c wake_q_head.count, page_alloc.c get_cma_migrate_type.
- Diagnosis: git merge used a virtual base including b9e54f8e02e2 (2020 android-mainline), so files Rockchip stripped of Android bits auto-merged to the stripped version; plus ACK-wins on content conflicts dropped Rockchip additions.
- Proposed fix: rebuild rgds-ack-merge as rgds-stock-recon + `git diff 58485ff1a74f android14-6.1-2025-07_r13 | git apply -3` (true base = upstream v6.1.141), then resolve conflicts with base 58485ff.

## Session 5 (2026-11-16 20:47) rgds-ack-v2
- rgds-ack-v2 from rgds-stock-recon: git diff 58485ff1a74f android14-6.1-2025-07_r13 (2109 files) | git apply -3 --index -> 32 conflicted files, resolved by hand (merge-resolution-v2.tsv, 9 REVIEW). Commit 1a375c041462. Also removed dup xt_quota2.o Makefile line and struct track/enum track_item from mm/slub.c (ACK moved them to mm/slab.h); fixed dwc3 core.c function boundary.
- olddefconfig vanished: only PAHOLE_HAS_SPLIT_BTF, ROCKCHIP_EBC_DEV, RTL8821CS (config-diff-ack-v2.txt). PAHOLE_VERSION 121 -> 0 (no pahole in container).
- Build run1 (-k, no KCFLAGS): 437 errors (build-ack-v2.run1.log); 404 from include/linux/version_compat_defs.h (vm_flags_set redefinition) because __ANDROID_COMMON_KERNEL__ unset. ACK build.config.common:11 sets KCFLAGS -D__ANDROID_COMMON_KERNEL__. Run2 started with that KCFLAGS.
- Run2 (KCFLAGS=-D__ANDROID_COMMON_KERNEL__, -k): 32 errors in 10 files (build-ack-v2.log): Mali midgard/mali400 + spi-rockchip direct vma->vm_flags writes (ACK made vm_flags const; need vm_flags_set), cpufreq_interactive.c tracepoint arg count (ACK changed a hook signature), dma-buf heaps duplicates (system_uncached_heap_*, dma_heap_get_dev, dev_ret) from my REVIEW picks, uvcg_frame_type2 redefinition, unused vars tcpm mw / mmc resume_success.

## Session 6 (2026-11-16 20:53) first link
- Fix commits on rgds-ack-v2: 38289af9afa1 dedup (stock kallsyms has one each of system_uncached_heap_ops, dma_heap_get_dev, uvcg_frame_type2); 2ff0842e309b interactive 3-arg cpufreq_frequency_table_target; 754c14230e7c Mali/spi vm_flags_set/clear; 9fd38f2621ca unused vars.
- build-ack-v2.log: BUILD EXIT 0, 0 errors. Image 37,937,160 B (stock 37,767,176 B, +169,984).
- Banner: identical "Linux version 6.1.141 (builder1@dell-PowerEdge-R730XD) (Android (10087095 ... r487747c) clang 17.0.2 ..., LLD 17.0.2) # SMP PREEMPT" (build # and date not set yet).
- symdiff/: text syms stock 127615, ours 127412; only-in-stock 811, only-in-ours 608; clusters in *.clusters.txt.
- Biggest gap: stock joypad driver is far richer than Gamma's singleadcjoy.c (465 only-in-stock syms: remapkey_*, jokstick_cal/show/store, joypad2touchpanel_*, adc_abs/abs_*). Touchscreen: stock uses a different Hynitron (hyn_*) driver version than ours; gt9xx_touch_swap*; rk817_bat differs; sc7u22_set_* range/odr functions missing from the reconstruction.

## Session 7 (2026-11-16 20:56)
- Rockchip base window: first-parent list 0e7abd67ecee..af8041e6d1ed = 681 commits. Earliest stock-absent Kconfig symbols: VIDEO_SC235HAI 33f3c216294c (fp 153, committed 2025-08-18), VIDEO_SC231HAI 8805d031be45 (fp 154), ROCKCHIP_RGA_GENPOOL ae35fd7eec8f (fp 164, 2025-08-20). rk_spdifrx_ bb3fd4709b1f fp 310.
- => stock base is in (0e7abd67ecee, 80afdafe540b] (fp[152] = 80afdafe540bad89b1c9fd6d60bc916b8306c8b4 2025-08-18 "media: i2c: add support sc635hai sync mode"). Up to 152 candidate commits. v3 rebase not yet done.
- build-ack.sh: KBUILD_BUILD_VERSION=31, KBUILD_BUILD_TIMESTAMP="Tue Sep 29 16:47:09 CST 2026". Full banner now identical to stock's. Stock also has a second banner "... # SMP PREEMPT " (empty version/timestamp); ours has both copies with #31.
- 58c7f7986542 sc7u22: tables, set_* and ops made global; now T in our kallsyms like stock. Symdiff now only-in-stock 804, only-in-ours 609.

## Session 8 (2026-11-16 21:00) rgds-ack-v3
- rgds-ack-v3 = rebase of 12 commits (overlay, sc7u22, ACK delta, fixes) onto 80afdafe540b. One conflict: Documentation pixfmt-yuv-planar.rst (took ACK-commit side, doc only). HEAD 011033e1c4aa. Build running -> build-ack-v3.log.
- Narrowing (narrow/scan.tsv: per fp commit, new C function names vs stock kallsyms):
  - PRESENT in stock: fp57 541244ac65bf (2025-08-01) rkisp_aiisp_l2, rkisp_rdbk_aiisp_start (both absent before that commit; ISP=y in stock).
  - ABSENT in stock but present in our v2 kallsyms (built, so not inlined): fp79 cbf6a0d4e256 rockchip_drm_fix_encoder_possible_clones; fp86 cdfc4753bfcf rockchip_check_group_pins; fp99 0d2ba152d375 rockchip_pwm_set_filter{,_v1,_v4}.
  - => stock base in fp57..fp78: 541244ac65bf .. 065dad5d61fc (2025-08-01 .. 2025-08-06). fp61 dd61e4243f21 (rk809/rk817 PM switching) is the next discriminator to test.

## Session 9 (2026-11-16 21:03)
- v3 (base 80afdafe540b) built: Image 37,871,624; text syms 127,281; only-in-stock 725, only-in-ours 391 (v2: 804/609).
- fp61 dd61e4243f21 PRESENT: stock strings have exactly the post-commit SLPPIN set (init/shutdown/suspend/resume ... RST/DN/SLP/NULL_FUN) and lack the removed "Add rk805-pinctrl dev failed!".
- fp64 ec6497d2fc7f ABSENT: "wait for isp idle timeout" (common isp_params.c, ISP=y) in our Image, 0 in stock-strings.
- fp62 65ecb0038c20 (dts only), fp63 ea87d9c92c1b (ox03c10 sensor, not built) not discriminable -> base = fp63 ea87d9c92c1be48d72603d37d012c98b2dad82b6 (window fp61..fp63).
- rgds-ack-v4: clean rebase of 12 commits onto ea87d9c92c1b, + 867f72f4bb5f rk817_battery develop-4.19 rev d1d1ad90afcc, + 4bf87988fab9 time64 port. build-ack-v4.log BUILD EXIT 0.
- v4: only-in-stock 688, only-in-ours 350. rk817: stock-only rk817_bat_lowpwr_check.{correct_count,count}, rk817_bat_update_temperature.last_temp; ours-only rk817_bat_resume_work.

## Session 10 (2026-11-16 21:07)
- 621f7f20f454 gt9xx touch-swap patch (reconstructed from stock disassembly); 30758f3e9a97 drop unused goodix_ts_name (-Werror).
- fc7ebec2bec8 hynitron radxa V3.0 added at drivers/input/touchscreen/hynitron/ (CONFIG_TOUCHSCREEN_HYN=y in stock). Build FAILED: hynitron_core.h:57 '../../init-input.h' file not found (Allwinner sunxi input framework: ctp_config_info, input_sensor_startup/init, input_set_int_enable, input_request_int). 3caecb1629d9 rewires CONFIG_TOUCHSCREEN_HYN back to hyn/ until ported.
- v4b build EXIT 0. only-in-stock 680, only-in-ours 348 (v4 688/350). gt9xx: all touch-swap syms now match; only initcall line differs (stock goodix_ts_init line 3007, ours 3266).

## Session 11 (2026-11-16 21:10) hynitron port
- ac3a24851f4b: hynitron_rk_glue.{c,h} (DT/IRQ stand-in for Allwinner init-input.h), radxa firmware/ headers, radxa build list (no tool_debug), early-suspend only without CONFIG_PM. CONFIG_TOUCHSCREEN_HYN -> hynitron/.
- Stock DTB: no hyn/cst node (dtc grep empty).
- build-ack-v4c.log BUILD EXIT 0. only-in-stock 630, only-in-ours 133 (v4b 680/348).
- hyn_*: stock 30 distinct names, 29 present in ours; missing hyn_find_fw_idx; ours-only hyn_glue_irq.

## Session 12 (2026-11-16 21:13)
- 3d292fac0fec rk817 direct resume (no rk817_bat_resume_work), ce0d5dbce268 lowpwr_check/update_temperature (0xE9 NTC write follows VIRTUAL_TEMPERATURE, real TS ADC path pending).
- build-ack-v4d.log EXIT 0: only-in-stock 628, only-in-ours 135. Ours has rk817_bat_lowpwr_check.{count,correct_count,time}; no rk817_bat_resume_work; last_temp shows as rk817_battery_work.last_temp (update_temperature inlined) vs stock rk817_bat_update_temperature.last_temp.
- rk817 notes: stock has rk817_attrs, rk817_kobj, rk817_soc (absent in ours); stock does NOT export rk817_hp_inserted or rk817_pa_power_on (ours: __ksymtab_ for both).
- Joypad compile check on rgds-joypad-wip 6534328781a2: 1 error (joypad_probe declared static, not defined); list in kbase/joypad/compile-errors.txt. Branch not merged; src back on rgds-ack-v4.

## Session 13 (2026-11-16 21:15)
- 3a3b77462d23 rk817 probe writes 0xE9=0xfa (stock ffffffc008edb9e4).
- Coordinator note: on the RG DS the stock NTC path never runs (no ntc_table in DTB, ntc_size=0); temperature stays 188 = VIRTUAL_TEMPERATURE, so battery temp behaviour matches stock on this board.
- Stock has NO rk817_hp_inserted / rk817_pa_power_on symbols at all.
- rk817_battery.c Anbernic layer (xrefs via narrow/xref.py on stock-rk817.dis):
  - probe @ffffffc008edd1a8: g_battery = battery; rk817_kobj = kobject_create_and_add("rk809", NULL); sysfs_create_file(rk817_kobj, &rk817_attrs); on error DBG "rk817-bat: create rk817 sysfs error"; then fg_init_done = 1.
  - rk817_attrs @ffffffc00a319c80 = struct kobj_attribute { name @ffffffc009c1ce77, mode 0664, show battery_adjust_show @ffffffc008ee17d0, store battery_adjust_store @ffffffc008ee1858 }.
  - shutdown uses rk817_kobj; get_property stores rk817_soc (@..aac), reads fg_init_done, writes led_bat_soc.
  - LED functions only in stock: led_red_on, led_green_on, led_yellow_on, led_chargr_status, get_led_chargr_status, led_update_charger_soc_status, led_update_soc; globals fg_init_done, led_charger_status, led_bat_soc, rk817_soc, led_{red,green,yellow}_status, g_battery.
- 92c331674f30 LED layer (7 functions + 8 globals, addresses in commit msg); factory_led_status __weak (stock defines it @ffffffc00a4d5a94 next to gt9xx_touch_swap_status/touch_data, another object).
- battery_adjust (/sys/rk809/battery_adjust, 0664): show @ffffffc008ee17d0 = regmap_field_read(g_battery->rmap_fields[(0x4a8-0x48)/8 = 140 = NTC_TEMP_CFG in our enum]) -> sprintf "%d\n"; store @ffffffc008ee1858 = regulator_get(NULL, str@ffffffc009c1ce86), sscanf(buf, str@ffffffc009c63702, &c); c=='0': field@0x408 (idx 120 = RESET_MODE?) ...; c=='2': field@0x4c0 <- 0xfa, regulator_disable; c=='1': fields @0x408 and @0x4c8 <- 1, then 100x udelay loop. Fields @0x4c0/0x4c8 are past our F_MAX_FIELDS: stock's enum has >= 3 more fields. Not committed yet.
- v4e build EXIT 0 but vmlinux-to-elf failed: "ValueError: The value of kallsyms_token_index was not found" -> no kallsyms-based diff this round; vmlinux nm confirms all LED symbols present.
- c5430057428e: enum tail per stock reg_fields[140..144] (F_E6_6, F_E4_0_2, F_E4_4_6, NTC_TEMP_CFG=143, F_F4_2); /sys/rk809/battery_adjust (strings: "otg_switch", "%c ", "failed to get sw1 regulator: %d", "enable charging", "disable sw1 failed:%d"); probe/shutdown kobject; get_property CAPACITY reports get_cw2015_soc() (stock has an Anbernic CW2015 gauge driver exporting get_cw2015_soc; weak fallback here).
- Stock EXPORTs: get_cw2015_soc, factory_led_status, get_led_chargr_status, led_chargr_status (__ksymtab_ only in stock).
- v4f build EXIT 0; vmlinux-to-elf works again on v4f. kallsyms method: only-in-stock 608 / only-in-ours 134 (v4d 628/135). vmlinux method (ours = llvm-nm src/vmlinux): v4e 612/51, v4f 607/52 (only-in-ours = our t/T/w/W names absent from all stock names; only-in-stock = stock t/T/w/W absent from all our names).

## Session 14 (2026-11-16 21:24)
- aac79e4a407a gpio_keys: hall sensor (KEY_F12 -> KEY_CHANNELDOWN open / KEY_CHANNELUP close, static state_old_KEY_F9, hall_status_share global), dev_attr_hall_status (0644) = gpio_keys_attrs[4]; addresses in commit. v4g build EXIT 0; kallsyms diff 603/135 (v4f 608/134); all hall symbols match.
- Headphone/PA link: stock has NO rk817_hp_inserted. rk817_digital_mute (@ffffffc00911db7c..) sets the codec spk/hp ctl gpios then calls disable_pa_spk_right/left (mute) or enable_pa_spk_left/right (unmute). These are EXPORTed by two aw87391 amp drivers (aw87391_probe x2, aw87391_kspk_reg, aw87391_dt_ids x2, gtp_i2c_write) linked near rk_hdmi in sound/soc/rockchip area (@ffffffc00914a5b4..aa18). No source in tree; no matching CONFIG name found yet (AW87XXX/AW8738 not set). Joypad range (0x8c6c000-0x8c78000) has no pa_spk calls and no hall_status_share refs.

## Session 15 (2026-11-16 21:27) AW87391
- Source found: TheGammaSqueeze/rg_ds_kernel sound/soc/codecs/aw87391.c + include/sound/aw87391.h (commits d758d5fdf779 add, 8d2b5cde956d staged enable, c0fee8306908 rk817 mute wiring; 2026-09-06/07, after ba9053b). It is Gamma's reconstruction, single driver (compatibles aw,aw87391-left/-right, awinic,aw87391), exports aw87391_speakers_enable/disable; stock instead has two drivers exporting enable_pa_spk_{left,right}/disable_pa_spk_{left,right}. Downloaded to overlay-aw87391/. Also public: ROCKNIX/batocera "ASoC: codecs: Add aw87391 amplifier driver" patches (mainline-style, not checked).
- Plan: import Gamma's driver, add stock-named exports, wire rk817_digital_mute (mute: disable right, left; unmute: enable), drop rk817_hp_inserted/rk817_pa_power_on.
- 4d915bbf4b73 aw87391 driver (Gamma c0fee8306908) + stock-named exports enable/disable_pa_spk_{left,right}; SND_SOC_AW87391 default SND_SOC_RK817 (=y). 3b46c082e1ef rk817_digital_mute drives PAs in stock order; rk817_hp_inserted and rk817_pa_power_on exports removed (joypad gpio-146 gate removed).
- v4h BUILD EXIT 0. vmlinux-to-elf failed again (kallsyms_token_index not found) -> vmlinux method: only-in-stock 594, only-in-ours 63 (v4f same method 607/52). pa_spk exports now match; aw87391 internals still differ (stock: two drivers with aw87391_probe/remove/dt_ids x2 + aw87391_kspk_reg; ours: Gamma single driver aw87391_i2c_probe, pa_enable/disable, speakers_enable/disable, suspend/resume/shutdown, drv_event).
- 7e77814600d4: sound/soc/rockchip/aw_pa_left.c + aw_pa_right.c reconstructed from stock (two copies of one source, initcalls match exactly: __initcall__kmod_aw_pa_left__460_364 / aw_pa_right__460_365). Gamma's aw87391.c removed.
- v4i (vmlinux method): only-in-stock 584, only-in-ours 52 (v4h 594/63).

## Session 16 (2026-11-16 21:36) CW2015
- Stock has cw2015_battery (initcall line 795), cw2017_battery (677), cw221x_battery (965); CONFIG_BATTERY_CW2015/CW2017=y. Our tree's Rockchip cw2015_battery.c matched stock's function names; Anbernic deltas only.
- f031b39f3411: get_cw2015_soc (EXPORT, 50 until init), cw2015_update_soc, cw2015_init_done, charger state via get_led_chargr_status(); line 795 -> initcall name matches. rk817: weak fallback dropped; led_chargr_status/get_led_chargr_status exported.
- v4j vmlinux method: 578 / 51 (v4i 584/52). factory_led_status export + dev_attr_factory_led/joypad_show/store_factory_led belong to the joypad driver (stock-only, for the joypad agent).

## Session 17 (2026-11-16 21:39)
- 749e79c5aeec README.rgds.md + arch/arm64/configs/rgds_v1.21_stock.config (device /proc/config.gz).
- DMA heaps: stock CONFIG_DMABUF_HEAPS_ROCKCHIP is not set, yet stock system_heap/cma_heap objects contain Rockchip names (sys_dma32_heap, sys_uncached_dma32_heap, cma_heap_get_phys, cma_uncached_heap_*, orders.0-2) plus system_heap_worker/system_heap_wq/system_heap_dma_buf_release_sync found in neither our Rockchip base, its develop-6.1 history (git log -S on system_heap.c: none), nor ACK android14-6.1/-lts/android15-6.6 system_heap.c (0 hits). rk_system_heap.c/rk_cma_heap.c (added 2022-09) hold the Rockchip parts.

## Restructure plan (2026-11-16 21:45), not executed yet
- Stock facts used: initcalls timed_output line 110 and rk_timed_gpio line 157 match ours; stock has NO timed_gpio_* symbols (CONFIG_ANDROID_TIMED_GPIO=y builds only rk_timed_gpio there). ACK r13 already contains net/netfilter/xt_quota2.c and our final file is identical to ACK's, so Gamma's xt_quota2 commit is redundant. Stock DTB charger: max_chrg_current 0x7d0 (2000), max_input_current 0x5dc (1500).
- DTB normalized diff (phandles stripped): ~228 lines; seen: sc7u22_acc/gyro aliases+nodes, chosen bootargs ("earlycon=uart8250,mmio32,0xfe660000 console=ttyFIQ0" in stock, empty in ours), CPU OPP table (stock 1560 MHz + opp-j-m-1560000000; ours has 324..1056 entries), LED default-state off vs on, hall-key0.
- Candidate fix-layer list: F1 charger 3000/3500 mA DT; F2 joypad B-key slot (B checks Y slot 4, should use 6); F3 battery_adjust: stock dev_warn((struct device *)kobj) + uses ERR regulator (keep quirk in stock layer, fix on top); F4 (proposed only) anything else found while restoring quirks.

## Branches (2026-11-16 21:50)
- rgds-v1.21-stock (from ea87d9c) and rgds-v1.21 (= stock + FIX commits). rgds-ack-v4 untouched.
- Stock joypad landed (3218c6cf9f6d). gt9xx_touch_swap_status/combo_status now extern in gt9xx.c (stock defines them in the joypad object).

## Fix proposals (joypad FIX CANDIDATEs; NOT committed, for Paul's review)
- J1 (DONE as F2 in rgds-v1.21): key B accepts mask 0 or 4 (Y's slot) instead of 0 or 6 (@ffffffc008c70080, @..700c8).
- J2: joypad_resume (@ffffffc008c7699c) is identical to joypad_suspend and writes 0 to the +0xc byte (@..769c0); resume should restore it (input polling likely stays off after resume).
- J3: joypad_show_adc_z_p / _rz_p (@..71e9c, @..71ed8) read channel 0 (+0x8, LX) instead of their own channel; joypad_show_adc_z/_rz print a constant 0.
- J4: joypad2touchpanel_abs (@..6ecf0) and _abs_z (@..6f624) scale both axes by *_lenght_x; *_lenght_y is clamped but unused.
- J5: joypad_probe continues when amux-count or child count is 0 (only logs "adc key = %d, gpio key = %d error!" @..708d0) and allocates a zero-length adc array.
- J6: amux setup returns -EINVAL instead of PTR_ERR() on devm_iio_channel_get failure (@..70bdc), losing -EPROBE_DEFER.
- J7: joypad_adc_read refuses mux index >= 4 (@..71a54) while the DTB has amux-count = 6, so ABS_Z/ABS_RZ (channels 4/5) always read 0.
- J8: joypad_show_amux_debug (@..71bcc) clears the enable byte (reading the attr stops polling) and always prints adc value 0 (@..71be4).
- J9: joypad_adc_check averages a fixed 6 channels and writes hist[] of channels 4/5 (@..75fd4..762ac) regardless of amux_count; with amux_count < 6 it writes past the adcs array (memory corruption).
- fx-dts dd3197172720 cherry-picked onto rgds-v1.21-stock as 416bd5e96bcb. Stock-branch build: rk3566-anbernic-rg-ds.dtb byte-identical to stock-rsce.dtb (sha1 09717508cb15dcbb20f9f43a8b9bdc7640a0fb74). rgds-v1.21 rebuilt on top (F1 re-applied against the new dts: max_input_current 0xdac, max_chrg_current 0xbb8 in the built DTB).
- Symbol diff (vmlinux method): rgds-v1.21-stock 77 only-in-stock / 37 only-in-ours; rgds-v1.21 77 / 36.
- Remaining stock-only clusters: 35 initcall_ (line-number drift), DMA heaps (system_heap_worker/wq/release_sync, orders.0-2, cma_heap_do_allocate/get_phys, cma_uncached_*, sys_dma32_heap, sys_uncached_dma32_heap), gt9xx gtp_request_*, joypad leftovers (tpd_up/down, touch_id, joypad_attr(s), __ksymtab_gts...), switch_ntc, sd_resume, rk817_bat_update_temperature.last_temp, panel_simple, hyn_find_fw_idx, xfrmi msg.

## Session 18 (2026-11-16 21:52)
- rgds-v1.21-stock += fx-cfg (ebc-dev + RTL8821CS Kconfig), fx-heaps (3 commits), fx-gt9xx (2 commits), joypad touch_id update. Config diff now only -PAHOLE_HAS_SPLIT_BTF (+2 newer ARM64 errata). DTB still byte-identical. vmlinux method: stock 64/23, rgds-v1.21 64/23.

## Gamma audit (2026-11-16 21:55), rgds-v1.21-stock
Tool: audit/fncmp.py (function size + normalized instruction stream, stock.elf vs our vmlinux; differences only in adrp page labels count as identical).
- timed_output.c (Gamma copy): timed_output_dev_register 276/276 (1 adrp label diff), _unregister IDENTICAL, timed_output_init 104/104 (adrp only), enable_show IDENTICAL, enable_store IDENTICAL; initcall timed_output__350_110 = stock. VERIFIED.
- rk_timed_gpio.c (Gamma copy): rk_vibrator_enable/get_time/timer_func/work IDENTICAL; vibrator_probe 336/336, vibrator_remove, driver_init/exit: adrp-only diffs; initcall rk_timed_gpio__384_157 = stock. VERIFIED.
- timed_gpio.c: not in stock, removed.
- Kconfig help texts (joystick SINGLEADCJOY, staging ANDROID_TIMED_*): Gamma wording, not in binary -> rewritten (b7ff307ab72f).
- dts: fx-dts regenerated from stock DTB (no Gamma content). xt_quota2: ACK. aw87391.h/aw_pa_*.c/rk817 codec PA calls: ours from disassembly (no Gamma code); aw87391_remove IDENTICAL, aw87391_probe 444/444 after reorder, enable/disable same size (compiler scheduling differs only).
- Joypad: stock reconstruction; source comments still mention "Gamma's field names" (joypad agent's file) -> ask joypad agent to drop Gamma references/names.

## Session 19 (2026-11-16 22:01)
- rgds-v1.21-stock rebuilt (old tip kept locally as rgds-v1.21-stock-prev): fx-cfg 1c734b04 (help text kept ours), fx-heaps rewritten (ed3f0d7, 9d01aae, 12c79162; efb10ea dropped), fx-dts d40a3e8 + tools 9ec49e2 (+ __pycache__ untracked) + aa70df5 + 4a2c72a, fx-hynitron f18ddc9/0a7fa44/a7f4df0, joypad 2f4e5ef content, README provenance/status. Tip a84a31fd0c3e.
- rgds-v1.21 = stock + F1 (re-applied on new dts lines 529/530) + F3 + F2. Tip 40a9a93104ed (old kept as rgds-v1.21-prev).
- Both build; stock config diff only -PAHOLE_HAS_SPLIT_BTF; dts gate 0; symbols 54/15 (both). audit/vmlinux-stock-a84a31f saved for the function-level metric.

## Function-level metric (2026-11-16 22:06), rgds-v1.21-stock a84a31fd0c3e
Tool: audit/allcmp2.py (nm boundaries, $x/.L mapping symbols ignored, unique names only; objdump instruction streams; ADDR_ONLY = differences only in adrp pages and lo12/offset immediates).
- common functions 69912: IDENTICAL 41633, ADDR_ONLY 27904, SAMESIZE_DIFF 207, DIFFSIZE 168. Lists: audit/funcdiff.tsv; top 30 non-joypad/non-hynitron by |delta|: audit/top30.tsv.
- Top-30 queue by file: rk817_battery.c (8 fns: battery_work 5720->3652, probe 9288->8984, get_property, pm_resume, save_data, get_capacity_uah, init_coulomb_cap, output_info); gt9xx.c (goodix_ts_probe 1440->3200, work_func 1120->844, gtp_read_version 292->140); arch/arm64/mm/mmu.c (mark_rodata_ro, early_fixmap_init, map_kernel, pud_clear_huge/free_pmd_page/set_huge); hid-core.c hid_report_raw_event 2024->40; l2tp_core.c *_dec_refcount, l2tp_ppp.c pppol2tp_ioctl; sd.c sd_resume_runtime/system; eventpoll.c eventpoll_release_file; mremap.c move_pgt_entry; pgsize_migration.c madvise_vma_pad_pages; rfcomm sock.c rfcomm_connect_ind; panel-simple.c panel_simple_probe; v4l2-ioctl.c v4l_enum_fmt. (__exittext_end is a linker label, ignore.)
- The mm/arm64, hid, l2tp, sd, eventpoll, mremap, pgsize_migration, rfcomm entries look like ACK/upstream revision differences -> evidence for the misc agent's ACK-release question.

## Session 20 (2026-11-16 22:10)
- rgds-v1.21-stock tip 1d32503e350f: + fx-cfg RTL8821CS (4d74d65, 4b4c140, 227650a), fx-dts 3e6f0be/87a5e02/e736553 (076d5a6 = our 0638684, skipped), fx-hynitron 9802322 + 774dc4e, fx-gt9xx 340026b..5dd3139 (6 commits), README RTL8821CS row.
- rgds-v1.21 tip d705be1fad2d (F1 re-applied at dts lines 1614/1615 decimal 1500/2000 -> 3500/3000).
- Both build; config diff only -PAHOLE_HAS_SPLIT_BTF; gate.sh 0; symbols 50/13 (stock) and 50/13 (fix).
- allcmp2: IDENTICAL 41635, ADDR_ONLY 27907, SAMESIZE_DIFF 212, DIFFSIZE 162 (prev 41633/27904/207/168).
- Pending decision: ACK r8 delta (fx-ack-r8 2335fc6afbce) offered by the misc agent as replacement for commit 1; held per coordinator's "hold any base change".

## Session 21 (2026-11-16 22:20) ACK r8
- rgds-v1.21-stock rebuilt on fx-ack-r8 2335fc6afbce (r8 delta) + replay of all later stock commits + gt9xx a12ed5d..975cf9e + fx-cfg aic8800/ea6621q/infineon (2651c5f, ee160e7, 2de07cc, e5dafa4). Tip f9f06f83e9c4. Backups: rgds-v1.21-stock-prev, rgds-v1.21-prev.
- Config diff vs stock: only -PAHOLE_HAS_SPLIT_BTF (+ PAHOLE_VERSION 121 -> 0); the two ARM64 errata lines are gone. gate.sh 0.
- vmlinux method: 33 only-in-stock (20 initcall line drift, 5 CortexA53843419 veneers, 8 real: bat_ntc_temp, switch_ntc_temp, rk817_bat_update_temperature.last_temp, joypad_attr_group, joypad_attrs, __ksymtab_gts_dev, panel_simple_parse_panel_timing_node, sd_resume_common) / 8 only-in-ours (5 veneers, stock_gpio_hat, stock_hat_leave, stock_hat_press).
- allcmp2: IDENTICAL 41084, ADDR_ONLY 28614, SAMESIZE_DIFF 146, DIFFSIZE 74 (common 69918).

## gt9xx (fx-gt9xx, tip ca1ce0b4ef48d)
- Same Goodix release as ours (GTP_DRIVER_VERSION "V2.2<2014/01/14>"), built with GTP_COMPATIBLE_MODE 0, DEBUG_SWITCH 1, no gt9xx_update.o; suspend/resume via a global rockchip_panel_notifier_call (panel notifier), no "tp" regulator; goodix_ts_data layout as stock (panel_nb @0x138).
- 22/23 gt9xx functions equal stock in size; work_func, init_panel, probe, request_input_dev, touch_up, reset_guitar match instruction for instruction after normalizing addresses. Left: rockchip_panel_notifier_call 0x330 vs 0x334 (same calls and immediates, block layout only). Stock calls memmove @ffffffc008cc0834 where ours links memcpy; cause not found.
- Orientation globals (gtp_change_x2y, gtp_x/y_reverse, m89or101, bgt9xx) are shrunk by clang to 1-bit "not the initializer" flags; stock bytes are flags, not values. Source values chosen so our flag stores/tests equal stock (commit 6410f4083023). tp-size 911 (RG DS): no swap, no reversal; panel with flip_report_tp mirrors both axes.
- __LINE__ anchors (compiled printk line values, stock vs ours at ca1ce0b4ef48d). Lines in non-compiled regions (#if 0, pen/gesture/ESD/compatible-mode blocks, comments) cannot be recovered from the binary and are approximate; no filler was added to hit an anchor.

| printk | stock | ours |
|---|---|---|
| ID:%d, X:%d, Y:%d, W:%d (touch_down) | 493 | 520 |
| pre_touch:%02x, finger | 678 | 946 |
| Touch Release! | 701 | 1066 |
| <%s>_%d (init_panel entry) | 1148 | 1518 |
| Config Groups' Lengths | 1188 | 1556 |
| CFG_GROUP%d Config Version | 1263 | 1632 |
| <%s>_%d (cfg version < 90) | 1267 | 1637 |
| <%s>_%d (after checksum) | 1326 | 1696 |
| <%s>_%d <%d, %d> | 1333 | 1702 |
| <%s>_%d (before compatible block) | 1335 | 1704 |
| ######## IC Config Version | 1501 | 1868 |
| INT trigger type | 1610 | 1977 |
| <%s>_%d in gtp_request_irq (first) | 1624 | 1991 |
| <%s>_%d in gtp_request_irq (second) | 1654 | 2021 |
| <%s>_%d I2C communication ERROR (probe) | 2563 | 2894 |
| <%s>_%d prob error (probe) | 2640 | 2971 |
| module_init (initcall name) | 3007 | 3335 |

## Session 22 (2026-11-16 22:30)
- stock += fx-cfg 8822cs (121ba08, 1b976c4, bf8fb86), infineon 668a0a3, fx-hynitron e61fb41/b682381/a28aedc, fx-batt 848aa68/fbe7566/e96e246, README rows (vendor Wi-Fi, panel, r8), fx-gt9xx 6410f40/c4d7579/ca1ce0b. Tip 2be24cc49e903.
- fix branch tip 26de1237a4714 (F3 conflict with BAT_LTS_TS rename resolved).
- Both build. config diff only -PAHOLE_HAS_SPLIT_BTF. gate 0. Symbols stock 29/6, fix 29/5. allcmp2: IDENTICAL 40855, ADDR_ONLY 28850, SAMESIZE_DIFF 149, DIFFSIZE 65.

## Session 23 (2026-11-16 22:45)
- Stock branch rebuilt from e72230163d62d^ so the 8822cs history is one commit (SDK copy, bf044eb content; the 91 MB Realtek zip from the tarball commit never enters history). Then replayed, + fx-cfg bcmdhd/8852bs/swt6621s/8821cs RTW_ANDROID 14, fx-hynitron 9a94e6a + 1b05c2b, fx-batt 0186826/5b91d07/def5c1a, joypad c210067 + 5705e12, README vendor rows + build note.
- Tips: rgds-v1.21-stock 9a3052c695a06, rgds-v1.21 a616d4f3de24d (F2 rewritten for the folded table). Backups -prev.
- Both build. Config diff only -PAHOLE_HAS_SPLIT_BTF. gate 0. Symbols 26/3 (stock), 26/2 (fix): 20 initcall drift + 5 veneers + sd_resume_common.
- allcmp2: IDENTICAL 40808, ADDR_ONLY 28893, SAMESIZE_DIFF 158, DIFFSIZE 60.

## Session 24 (2026-11-16 22:55)
- stock += fx-hynitron 2843c54/39d113c, fx-batt 7f1f5e6/df177e9. Tips: rgds-v1.21-stock bad90a7ef42d7, rgds-v1.21 04a21a83303dd.
- Both build; config only -PAHOLE_HAS_SPLIT_BTF; gate 0; symbols 26/2 (stock), 26/1 (fix).
- allcmp2: IDENTICAL 40812, ADDR_ONLY 28889, SAMESIZE_DIFF 158, DIFFSIZE 60. rk817 remaining size diffs: save_data, get_capacity_uah, get_property, output_info, pm_resume, init_coulomb_cap, get_relax_voltage, calc_zero_linek, charge_set_chrg_param, battery_work (-28).

## Session 25 (2026-11-16 23:05)
- stock += joypad 917b12c (per-key keypad2touchpanel_abs), fx-hynitron 47b3633, fx-batt ae8f85a/224bfab/21a7749/98c9f77. F2 now edits KP_KEY(0x131, b, 6, 4) -> 6.
- Tips: rgds-v1.21-stock b4cc8c958aa8a, rgds-v1.21 5c36d3ded5f81. Both build; config only -PAHOLE_HAS_SPLIT_BTF; gate 0; symbols 26/1 (stock), 26/4 (fix: linker veneers).
- allcmp2: IDENTICAL 40813, ADDR_ONLY 28894, SAMESIZE_DIFF 158, DIFFSIZE 54.

## Session 26 (2026-11-16 23:15)
- stock += joypad aa9b80b, fx-batt 14c81e3/48b92e6. Tips: rgds-v1.21-stock 8b6f15756fc6e, rgds-v1.21 67ec8a3e54f43.
- Both build; config only -PAHOLE_HAS_SPLIT_BTF; gate 0; symbols 26/5 (stock, veneers only besides) and 26/4 (fix).
- allcmp2: IDENTICAL 40812, ADDR_ONLY 28887, SAMESIZE_DIFF 167, DIFFSIZE 53.
- Session 27: stock += joypad 992d377 (gpio_check written out). Tips: stock 97d14782225f1, fix cfdce3d911b0e.
- Session 28: stock += fx-batt bb8c837/72bcee0. Tips: stock 63b8c31e856f3, fix 141e834a55175.
- Session 29: stock += joypad b6209f5. Tips: stock 116b7bf808977, fix 558456b01aca7. allcmp2 40814/28899/159/47; symbols 26/2.
- Session 30: stock += joypad 3086845, fx-batt 50c86a4/8528238/c3d950e. Tips: stock b31cc043f6710, fix 0a19568cc3845. allcmp2 40816/28895/168/40; symbols 26/3 (stock), 26/1 (fix).
- Session 31: stock += joypad 0452d9a.
- Session 32: stock += fx-batt 7911fc5/0735e0a/78a3865, fx-hynitron 24ec5c4 (WIP subject reworded). Tips: stock ef503bb940c26, fix d32bdef2a388b. allcmp2 40815/28904/164/36; symbols 26/2.
- Session 33: stock += joypad 767ad89.
- FIX CANDIDATE H1: hynitron HYN_UPDATE_FIRMWARE_FORCE 1 in stock skips fw version/project-id checks on update (0e450d03c3f8).

## fx-batt rk817_battery.c (battery agent, 2026-11-17)
- Tip 7015667adb2d4. All rk817_battery.o functions reconstructed from the stock binary; 39 of 42 are stock size.
- Remaining size gaps are layout only (same calls and field accesses as stock): rk817_battery_work 5704 vs 5720 (block order and register choice), rk817_battery_probe 9292 vs 9288, led_update_charger_soc_status 260 vs 276 (logic identical; a hoisted-lookup rewrite came out 252, reverted). Left as is.
- RG DS charge gate: BAT_LTS_TS (0xE9) only gets 0xfa because the DT has no ntc_table (temperature stays 188); the 5 branch cannot run on this device.
- Session 34: stock += joypad 767ad89, hynitron 0e450d0 (reworded), fx-batt ab4f2fb/7015667. Tips: stock 8ec898aff8dac, fix 25a5c33bd0c09.

## Hynitron: __LINE__ offset in hynitron_update_firmware.c (unproven lead, 2026-11-16)
- Only HYN_FUNC_EXIT carries __LINE__ (w2 of "%s: Exit(%d)"); the first anchor is our line 1792, so nothing above it can be placed.
- Stock line minus our line (fx-hynitron 6b0113ecfe4e): +301 at 1792 (cst3xx_boot_update_fw Exit, inlined in hyn_boot_update_fw); +291 at 2062/2109 (cst3xx_firmware_info, cst8xx_firmware_info); +289 at 2139/2166 (hyn_find_fw_idx, hyn_firmware_info); +291 from 2294 on (hyn_boot_update_fw Exit, all 8 bootloader Exits).
- From hyn_boot_update_fw down, stock spacing equals MTK V2.2 minus cst3240/cst92xx bootloader_enter (Exit gaps 10, 42, 59, 43, 39, 36, 44 match); hyn_detect_bootloader is 5 lines shorter than MTK.
- Unproven candidate for the +301: the MTK V2.2 `#if HYN_EN_AUTO_UPDATE_CST644K` block (MTK lines 1391-1694, about 306 lines) that our tree dropped. It compiles out in stock (config 0, cst644k-only strings absent from the image). NOT restored: the binary proves a line count only, the text would come from a third-party MTK copy, and it is about 5 lines off.
- Session 35: stock += hynitron a33e69e/6b0113e/b697065 (reworded), fx-misc2 49e8cbe (sd resume) + 8b6d054 (schedutil). The schedutil double map_util_freq() was introduced by MY ACK-delta conflict resolution (cpufreq_schedutil.c "BOTH": kept Rockchip's call and ACK's hook+call) -> freq scaled by (util/max)^2 below full load. Other "BOTH"/ACK picks to re-audit against stock (allcmp diffs): tcpm_queue_vdm_unlocked, uvc_function_bind, dma_buf_export (size) and dwc3_gadget_soft_disconnect, cma_heap_do_allocate, xhci_bus_suspend (same size, different code).
- Session 36: stock += fx-misc2 bcbafd3/419addd/1200b2a, joypad 800ae93.
- Session 37: stock += fx-misc2 0405439 (tcpm), joypad 730097b.

## Joypad and Hynitron: remaining gaps are layout only (2026-11-16)
Calls, error strings and behavior match stock in every function below; only compiler block layout or register choice differs.
Tips: rgds-joypad-wip c7aa16de5a03, fx-hynitron b69706553176.
- joypad_gpio_check (stock 4000 / ours 3956): stock re-loads the button array pointer per hat button (@..74fb4), writes the hat press/release tails out per direction (@..750f0..752c0 Y, @..754ec..7564c X), and writes the selector 2 half out separately with csetm (@..75cbc..75de0).
- joypad_adc_check (2728 / 2572): stock keeps "sub w6, w6, w7, asr #1" in each centring case (@..7636c..76410), loads the PS/NS scale in each D1 branch where ours uses csel on the address (@..7651c..765c8), repeats the D1 zero path per axis (@..76678/76690/766a8/766c0), and differs in the D2 clamp layout (@..76784..767b4).
- joypad_probe (3940 / 4060): call multiset and dev_err sites equal stock (15); larger because of the written-out per-axis tuning reads (@ffffffc0097195c0 jump table).
- keypad2touchpanel_abs (3656 / 3648): per-key mode 2/3 mask test chains (@..6fccc..6ff48) laid out differently.
- Hynitron hyn_boot_update_fw and hyn_detect_bootloader: same size, differ only in __LINE__ constants (see the offset map above).
- Session 38: stock += joypad 2f48af0/f97c896, xhci quirk bit 47, dwc3 soft_disconnect (both my merge-resolution errors).
- Session 39: stock += joypad c7aa16d. +1 DIFFSIZE in r9a was clear_page (asm, 84 vs 80 bytes; boundary set by link-layout padding to the next symbol, not code), gone again in r9b. Tips: stock 6409011360d15, fix 7e42fbbaab311.
- Session 40: stock += fx-misc2 a84deff/b529afe/98cbb5e/32549c5/5a6d247 (gpio_keys capability omission was my reconstruction's bug: lid hall events dropped).
- Session 41: clean (mrproper) builds of both branches: BUILD EXIT 0, results unchanged. allcmp2 extended (adr, .word, adr+nop vs adrp+add): SAMESIZE_DIFF 109 -> 78.

## fx-misc3: drivers/ SAMESIZE_DIFF triage (funcdiff.tsv 23:12, vs audit/vmlinux-r9a, 2026-11-16)
- Scope: 70 drivers/ entries (none in fs/); minus misc2/tree-agent files, joypad/hynitron (layout notes above) and gt9xx = 33 functions. All 33 have stock's call multiset.
- Behavior fixes on fx-misc3: 998376540c0c accel_calibration_store limit 4800 (mov w22, #0x12c0 @ffffffc008d02cac); 3b1910dcb96d rk817_charge_set_chrg_param calls led_chargr_status (bl @ffffffc008ee4058; DIFFSIZE 644 -> 676 = stock).
- Order only: 5662104500b1 battery_adjust_store (g_battery load before DBG, mdelay(100)); now equal except a debug __LINE__. F3 replay on the fix branch touches the same function.
- pci_dev 0x808 vs 0x800: fully explained by misc2 32549c5e21e2b (pcie_cap_lock); with it pci_alloc_dev kzalloc 0x800 and msi_lock @0x7a0 as stock.
- Layout only (global-data low-12 offsets, A53 erratum veneers): binder_transaction, binderfs (3), ISP/ISPP/ISP1/CIF/hdmirx register_vdev (17), dmarx_init, sc7u22_set_acc_odr/gyro_odr, psci_suspend_finisher, rockchip_usb_phy_probe, quirk_no_ext_tags, inno_hdmi_phy_probe, rkvdec2_link_worker, crng_reseed.
- tcpm run_state_machine (6284/6272): same calls and logic; stock places one block elsewhere and duplicates a "mov w1, #0xd" tail. Layout only.

### Per-function class (stock address, file): drivers/ SAMESIZE_DIFF triage
- sc7u22_set_acc_odr @ffffffc008cf5c54 (drivers/input/sensors/accel/sc7u22_acc.c): global-data offset (adrp hoisted; checked by hand)
- dmarx_init @ffffffc008e3a374 (drivers/media/platform/rockchip/isp/dmarx.c): global-data offset
- rkisp1_register_dmarx_vdev @ffffffc008e1ef3c (drivers/media/platform/rockchip/isp1/dmarx.c): global-data offset
- init_binderfs @ffffffc009e33954 (drivers/android/binderfs.c): global-data offset (adrp hoisted; checked by hand)
- inno_hdmi_phy_probe @ffffffc008693a58 (drivers/phy/rockchip/phy-rockchip-inno-hdmi-phy.c): global-data offset
- rkisp1_register_params_vdev @ffffffc008e186c8 (drivers/media/platform/rockchip/isp1/isp_params.c): global-data offset
- rkispp_register_stream_vdevs @ffffffc008e93964 (drivers/media/platform/rockchip/ispp/stream.c): global-data offset
- rkisp_register_stream_vdev @ffffffc008e37c2c (drivers/media/platform/rockchip/isp/capture.c): global-data offset
- hdmirx_register_stream_vdev @ffffffc008e9dc68 (drivers/media/platform/rockchip/hdmirx/rk_hdmirx.c): global-data offset
- rkcif_register_scale_vdevs @ffffffc008e0cdb4 (drivers/media/platform/rockchip/cif/cif-scale.c): global-data offset
- rkcif_register_luma_vdev @ffffffc008e04c34 (drivers/media/platform/rockchip/cif/cif-luma.c): global-data offset
- psci_suspend_finisher @ffffffc0095556a8 (drivers/firmware/psci/psci.c): global-data offset (adrp hoisted; checked by hand)
- binderfs_fill_super @ffffffc00906a4a0 (drivers/android/binderfs.c): global-data offset (adrp hoisted; checked by hand)
- pci_scan_single_device @ffffffc0086dd990 (drivers/pci/probe.c): struct pci_dev size, fixed by misc2 32549c5e21e2b
- rkisp_register_pdaf_vdev @ffffffc008e8dddc (drivers/media/platform/rockchip/isp/isp_pdaf.c): global-data offset (adrp hoisted; checked by hand)
- rkisp_register_params_vdev @ffffffc008e348f8 (drivers/media/platform/rockchip/isp/isp_params.c): global-data offset
- sc7u22_set_gyro_odr @ffffffc008cfb3d4 (drivers/input/sensors/gyro/sc7u22_gyro.c): global-data offset (adrp hoisted; checked by hand)
- rkispp_register_params_vdev @ffffffc008e9720c (drivers/media/platform/rockchip/ispp/params.c): global-data offset
- rkisp_register_stats_vdev @ffffffc008e337d0 (drivers/media/platform/rockchip/isp/isp_stats.c): global-data offset
- rkcif_register_stream_vdevs @ffffffc008dedbd4 (drivers/media/platform/rockchip/cif/capture.c): global-data offset
- binderfs_binder_device_create @ffffffc00906a878 (drivers/android/binderfs.c): global-data offset (adrp hoisted; checked by hand)
- accel_calibration_store @ffffffc008d02cac (drivers/input/sensors/sensor-dev.c): BEHAVIOR, fixed 998376540c0c
- rkisp1_register_stats_vdev @ffffffc008e163b8 (drivers/media/platform/rockchip/isp1/isp_stats.c): global-data offset
- rkispp_register_stats_vdev @ffffffc008e97c94 (drivers/media/platform/rockchip/ispp/stats.c): global-data offset
- quirk_no_ext_tags @ffffffc0087036b4 (drivers/pci/quirks.c): global-data offset
- rkvdec2_link_worker @ffffffc008742d94 (drivers/video/rockchip/mpp/mpp_rkvdec2_link.c): erratum veneer + global-data offset (kmalloc cache, adrp hoisted)
- rkcif_register_tools_vdevs @ffffffc008e0f894 (drivers/media/platform/rockchip/cif/cif-tools.c): global-data offset
- rkisp1_register_stream_vdevs @ffffffc008e1c78c (drivers/media/platform/rockchip/isp1/capture.c): global-data offset
- pci_alloc_dev @ffffffc0086dd110 (drivers/pci/probe.c): struct pci_dev size, fixed by misc2 32549c5e21e2b
- battery_adjust_store @ffffffc008ee1858 (drivers/power/supply/rk817_battery.c): statement order + __LINE__, fixed 5662104500b1
- binder_transaction @ffffffc009076eec (drivers/android/binder.c): global-data offset (adrp hoisted; checked by hand)
- crng_reseed @ffffffc0087d7fd0 (drivers/char/random.c): erratum veneer + global-data offset
- rockchip_usb_phy_probe @ffffffc0086b09fc (drivers/phy/rockchip/phy-rockchip-usb.c): global-data offset
- Session 42: stock += fx-misc3 3b1910d/9983765/5662104 (+ sensor-dev line-count keep). allcmp2 normalizes [xN] == [xN,#off]. Non-driver remainder triage: sound/core x6, soundcore x2, compaction x3, __traceiter x5, __secondary_switched, __cpu_up, xfrm_aalg_get_byid = 0 opcode diffs (address noise, now ADDR_ONLY); sha256_block_data_order = same span 0x46040..0x46fd0, stock.elf has no $d mapping so K256 table decodes as instructions (artifact); cpu_resume / __primary_switch = trailing data/padding before next symbol (artifact); proc_tcp_available_ulp = one b vs ldr at a static-branch/alternative site (runtime-patched; not a source difference found).

## Final non-identical function list (2026-11-16, rgds-v1.21-stock b06c3eefabfed, audit/allcmp2.py)
Counts: IDENTICAL 42022, ADDR_ONLY 27844, SAMESIZE_DIFF 46, DIFFSIZE 12 (of 69924 common). Classes below; drivers marked "owner" are still being matched by their agents.

- gt9xx probe, after 656f9dde73824 (0 opcode differences): same DT reads and strings in stock order; remaining operands are addresses, .bss offsets of the gt9xx globals (stock places them after gtp_i2c_connect_client), and __LINE__ constants: goodix_ts_probe (SAMESIZE_DIFF 1440/1440)
- aw_pa: same size, compiler scheduling of the {0x01,0x00} buffer: disable_pa_spk_right (SAMESIZE_DIFF 128/128), enable_pa_spk_right (SAMESIZE_DIFF 192/192), enable_pa_spk_left (SAMESIZE_DIFF 192/192), disable_pa_spk_left (SAMESIZE_DIFF 128/128)
- disassembly artifact (stock.elf has no $d mapping; K256 table decoded as code); same span: sha256_block_data_order (SAMESIZE_DIFF 3984/3984)
- gpio_keys hall: layout (branch targets): gpio_keys_gpio_report_event (SAMESIZE_DIFF 308/308)
- gt9xx (gt9xx agent): gtp_init_panel (SAMESIZE_DIFF 1352/1352), gtp_request_irq (SAMESIZE_DIFF 408/408), goodix_ts_work_func (SAMESIZE_DIFF 1120/1120)
- hynitron (joypad/Hynitron agent): __LINE__ constants and code layout: hyn_platform_data_init (SAMESIZE_DIFF 960/960), hyn_probe (SAMESIZE_DIFF 1560/1560), hyn_irq_init (SAMESIZE_DIFF 292/292), hyn_ts_data_init (SAMESIZE_DIFF 344/344), hynitron_driver_exit (SAMESIZE_DIFF 88/88), hyn_detect_bootloader (SAMESIZE_DIFF 1968/1968), hyn_firmware_info (SAMESIZE_DIFF 140/140), hyn_boot_update_fw (SAMESIZE_DIFF 2664/2664), hyn_pm_suspend (SAMESIZE_DIFF 148/148), hyn_remove (SAMESIZE_DIFF 76/76), hyn_update_firmware_init (SAMESIZE_DIFF 260/260), hyn_find_fw_idx (SAMESIZE_DIFF 720/720), cst8xx_touch_report (SAMESIZE_DIFF 1000/1000), hyn_input_dev_int (SAMESIZE_DIFF 612/612), cst3xx_firmware_info (SAMESIZE_DIFF 784/784), hynitron_driver_init (SAMESIZE_DIFF 132/132), cst8xx_firmware_info (SAMESIZE_DIFF 312/312), hyn_pm_resume (SAMESIZE_DIFF 260/260)
- joypad (joypad agent): code layout, calls equal stock: joypad_adc_check (DIFFSIZE 2728/2572), joypad_probe (DIFFSIZE 3940/4060), joypad_gpio_check (DIFFSIZE 4000/3956), keypad2touchpanel_abs (DIFFSIZE 3656/3648), tenpad2touchpanel_abs (SAMESIZE_DIFF 360/360), g_touch_sync (SAMESIZE_DIFF 856/856), joypad2touchpanel_abs_ext (SAMESIZE_DIFF 1296/1296), joypad_adc_read (SAMESIZE_DIFF 268/268), joypad2touchpanel_abs_z_mode2 (SAMESIZE_DIFF 376/376), joypad2touchpanel_abs_z (SAMESIZE_DIFF 660/660), joypad2touchpanel_abs (SAMESIZE_DIFF 568/568), joypad2touchpanel_abs_z_ext (SAMESIZE_DIFF 1300/1300)
- layout (logic matches stock; battery agent): led_update_charger_soc_status (DIFFSIZE 276/260)
- layout (same device selection, different block order): gtp_touch_up (SAMESIZE_DIFF 92/92)
- layout (tcpm, no vendor hooks; misc2 agent): run_state_machine (DIFFSIZE 6284/6272)
- layout (trailing data/padding before next symbol): __primary_switch (SAMESIZE_DIFF 116/116), cpu_resume (SAMESIZE_DIFF 40/40)
- linker veneer (CortexA53 erratum 843419) replaces one ldr; layout: dw9763_vcm_resume (SAMESIZE_DIFF 456/456)
- linker/layout label: __exittext_end (DIFFSIZE 813252/813240), __entry_tramp_text_end (DIFFSIZE 4096/4096), tramp_vectors (DIFFSIZE 8192/8192), _text (DIFFSIZE 65536/65536)
- one b vs ldr at a runtime-patched alternative/static-branch site; no source difference found: proc_tcp_available_ulp (SAMESIZE_DIFF 212/212)
- rk817 battery/charger (battery agent): layout/register choice: rk817_battery_work (DIFFSIZE 5720/5704), rk817_battery_probe (DIFFSIZE 9288/9292), battery_adjust_store (SAMESIZE_DIFF 524/524)
- rk817 codec: branch targets only: rk817_hw_params (SAMESIZE_DIFF 880/880)
- sc7u22/sensor-dev: layout: sc7u22_set_acc_odr (SAMESIZE_DIFF 364/364), sc7u22_set_gyro_odr (SAMESIZE_DIFF 292/292)
- gt9xx, after b1b59a95bf9a4 (0 opcode differences, same stack layout): branch targets and the GTP_DEBUG __LINE__ (stock 1501): gtp_read_version (SAMESIZE_DIFF 292/292)
- Verified with vmcmp.py (0 opcode differences): sc7u22_set_acc/gyro_odr (orr operand order only), gtp_init_panel and gtp_request_irq (__LINE__ constants, gt9xx.c lines differ by ~370), goodix_ts_work_func, gpio_keys_gpio_report_event, rk817_hw_params, battery_adjust_store (branch targets / __LINE__ only).
- Real differences found and fixed this pass: headset_det_irq_thread debounce 800 ms (c7cf2e4fe0d7e), rk817 PA calls inside rk817_codec_ctl_gpio before the mute delay, usb_role_switch_register sets 'registered' after device_register (stock lacks db8686b24af1). Open (owned by agents): joypad/hynitron/rk817 layout items above.
- Follow-up pass (2026-11-17, stock 656f9dde73824 / fix a24a9f19b27c0): two more real gt9xx differences fixed. b1b59a95bf9a4: gtp_read_version config-version local is a zeroed 16-byte array (stock @ffffffc008cbfa1c stp xzr,xzr,[sp,#0x8]). 656f9dde73824: goodix_ts_probe fails with -EINVAL ('no max-x defined') when ext_lcd_tp, flip_report_tp or com_lcd_tp is missing (stock tbnz @ffffffc008cc0168/0190/01b8 to +0x404); the earlier 'read order' note was wrong, order and strings already matched. The RG DS DTS sets all three properties. Line counts unchanged. allcmp2 categories unchanged (both stay SAMESIZE_DIFF on __LINE__/.bss offsets).

## Strict audit and final classification (2026-11-17, stock tip ea48ebc17736)

Tools: `audit/strict_audit.py` (keeps every immediate, offset, literal word and nop; only
adrp/lo12 pairs proven on every control-flow path, and branch targets, are normalized),
`audit/semtokens.py`, `audit/strictdiff.py`, `audit/maydeltas.py`, Ghidra 12.2 headless with
`ghidra/DecompileFuncs.java`. Controls on every run: stock vs itself = 0 differences; old build
r9a flags all 6 known fixes; build 656f9dd flags the dma-heap ops bug.

Counts (69,923 functions present in both): STRICT_SAME 69,170; ADDR_FLOW 611; ADDR_MAY 67;
SAMESIZE_DIFF 63; DIFFSIZE 12. Every non-identical function is in one class below.

Fixed today from this audit (stock branch): hynitron chip type 3340, 720x1280 default, 40 ms resume
reset, probe error path; rk817 fake-finish rounding; dma_heap_ops get_phys order; rk817 battery
platform data 0x208; hynitron ts data 0x1d0; joypad INPUT_PROP_DIRECT set atomically.

Classes of the 75 SAMESIZE_DIFF/DIFFSIZE functions:
1. `__LINE__` only (debug prints; stock line layout not recoverable): gt9xx (goodix_ts_probe,
   goodix_ts_work_func, gtp_init_panel, gtp_read_version, gtp_request_irq), cst3xx/cst8xx
   (firmware_info x2, touch_report), hynitron (hyn_* 13 functions, hynitron_driver_init/exit),
   battery_adjust_store, rk817_hw_params.
2. Equivalent after path-by-path review (Ghidra pseudocode or instruction trace): all joypad
   functions (adc_check, adc_read, gpio_check, probe, keypad/tenpad2touchpanel_abs, joypad2touchpanel_abs,
   _abs_ext, _abs_z, _abs_z_ext, _abs_z_mode2), g_touch_sync, gpio_keys_gpio_report_event,
   enable/disable_pa_spk_left/right, sc7u22_set_acc/gyro_odr (orr operand order), gtp_touch_up,
   led_update_charger_soc_status, rk817_battery_probe (register choice), rk817_battery_work
   (range checks rewritten, plus __LINE__), elants_a_report, tcp_v6_get_syncookie.
3. Global-variable placement only (adrp page register; the offset inside the page differs because
   .bss/.data objects sit in a different order or one neighbour differs in size): scheduler and
   warn-once flags (__migrate_swap_task, pick_next_task_fair, scheduler_tick, tick_nohz_idle_stop_tick,
   update_dl_revised_wakeup, update_rq_clock, prb_reserve_in_last, free_large_kmalloc, set_regdom,
   early_fixmap_init), cm_monitor_poller, crypt_dtr, thermal_zone_device_update, rk817_charge_probe,
   mir3da_chip_resume, mir3da_write_offset (A53 erratum veneer hides the adrp).
   Example measured: the rk817 battery globals occupy the same 40 bytes in both kernels but in a
   different order (stock: dbg_enable, ..., led_charger_status @+0xc, led_bat_soc @+0x10,
   led_red/green/yellow @+0x18..; ours: led_red first); the function-local statics at
   0xac8..0xad8 are at identical offsets.
4. Linker or disassembly artifacts: __CortexA53843419_* veneer, __entry_tramp_text_end, _text,
   tramp_vectors, __primary_switch, cpu_resume, sha256_block_data_order (stock ELF decodes the K256
   table as code), proc_tcp_available_ulp (runtime-patched branch), run_state_machine (duplicated
   tail, same logic).

ADDR_MAY (67): offset deltas are a constant -864 (arm64 page-table globals) or 896 (warn-once
flags) except a few small deltas (8/16/64) that `maydeltas.py` lists; each of those was traced to an
adrp-based global (rkisp_sd_s_rx_buffer, rkcif_irq_pingpong_v1, hdmirx_*, rockchip_pwm_irq,
snd_card_init, timed_output_dev_register, crypto_create_session, __vb2_perform_fileio, lg4ff_init,
rk818_battery_work, binder_alloc_deferred_release). Global placement, not struct layout.
Correction of an earlier note: the rkisp 0x33c/0x334 difference is a global (adrp x28), not the ISP
device structure. The camera and ISP nodes are disabled in the stock DT in any case.

Not recoverable from the binary: comments, compiled-out code, source line layout, definition order
of globals (class 3 could be reduced by reordering definitions, no behaviour change).

## Round 2 after Gemini's rejection (2026-11-17)

Gemini (reviews/2026-11-17-gemini-strict-classification.md) rejected "high confidence": #LO hid the low
12 bits of same-page accesses, STRICT_SAME used a linear scan, atomics and x30 were not clobbered.
All three were real. New tool `audit/strict2.py`:
- page registers tracked with their page VALUE, must-analysis over the CFG only (no linear scan);
  calls clobber x0-x18 and x30; LSE atomics and exclusives clobber every register they name.
- every abstracted access records its absolute data address in both kernels. Global check: each stock
  address must map to exactly one address of ours and back (bijection). A different field or index
  inside one page breaks it.
- ADDR_MAPPED: a remaining difference is accepted only when every differing operand is an address
  whose pair is in the proven map, or lies between two proven neighbours (within 4 KiB) that moved
  by exactly the same delta. Code entered only through a jump-label nop inherits the state at the
  function's nops.
- `audit/rodata_check.py`: for every mapped address, compares the constant bytes in both kernels by
  meaning (strings byte-for-byte; pointers resolved to symbol+offset or to the string they point at;
  other words raw), limited to the object that contains the address.

Controls (all pass): stock vs itself 120,474 EXACT, 0 conflicts; r9a flags all 6 known fixes;
656f9dd flags total_pools_kb_show; a copy of r5 with one byte of a mapped string changed is caught.

r5 (stock tip ea48ebc17736): EXACT 42,319; ADDR 27,348; ADDR_MAPPED 171; DIFF 85; address
conflicts 0 in either direction (72,889 mapped addresses). The 85 are the classes in the section
above (line numbers 26, equivalent drivers reviewed earlier, linker/boot artifacts 12, and ~22
global accesses whose base the analysis could not connect, each traced by hand to an adrp).

Constant data: 16,409 aligned words and every mapped string compared; 6 differences, all build
metadata, none source:
- build time and date strings (`16:44:45`/`Sep 29 2026` in stock);
- malidrv banner: BUILD_DATE generated by `date` in a zh_CN locale on Anbernic's host;
- rk_vcodec MPP revision string (generated from `git log` of the MPP directory at build time;
  stock's last commit there is Anbernic's "81c59f47ad0 author: Yangliming 2025-08-16 sync rockchip
  code"); all mpp functions are EXACT or ADDR;
- shipped_regdb_certs: same two certificates (wens, sforshee) in the other order, because
  net/wireless/Makefile concatenates `$(wildcard certs/*.hex)` in directory order.

Correction to the round 1 note: the rk817 "range checks rewritten" are the same predicate
(x/10 == 0 and (unsigned)(x+9) <= 18 both mean -9 <= x <= 9 for 32-bit int).

## Round 3 (2026-11-17): data layout by name

`stock.elf`'s symbol table names data objects too (all typed `t`), so `audit/datanames.py`
compares every mapped data address by symbol name and offset in both kernels. Gemini round 2
(reviews/2026-11-17-gemini-round2.md) asked for the root cause of the data shifts; walking the
shift steps (`.data` deltas between consecutive mapped addresses) found real source differences,
all in reconstructed drivers:

| Step | Cause (stock evidence) | Fix (stock branch) |
|---|---|---|
| -800 before net_namespace_list | aw87391 left/right: of_device_id tables non-const in .data after each driver; i2c_driver.id_table = goodix_ts_id | e41c061b8b96 |
| -8 / -32 at joypad | BTN_S defined before joypad_init_temp; joypad_attr_group non-const in .data | 7d36310b110c |
| rk817_battery_probe 4 bytes short | stock zeroes sm_linek_fast/normal in init; fg_init_done set after the closing DBGs; NTC table allocated with battery->dev | 67a8180f9b49 |

After these (build r9): EXACT 45,557 (was 42,319); ADDR 24,201; ADDR_MAPPED 91; DIFF 74; address
conflicts 0. Data: every .data/.bss address matches stock by name and offset; .data shift steps left
are two object-order differences that clang picks from first-reference order (joypad_driver vs
joypadz_count_mode2; rk817_attrs vs the rk817 property tables), size-neutral. Constant data: the
same 6 build-metadata differences. rk817_battery_probe is now the same size as stock; the remaining
difference is one instruction-order pair (ubfiz/ldr).

Publishing note: after tag v1.21-recon-1 the fixes branch was force-pushed when it was replayed on
new stock tips (against the agreement to stop force-pushing). From e41c061b8b96 on, stock commits are
added to both branches as ordinary commits (no force); the fixes branch tree equals stock + the 3
FIX commits (6 lines, 3 files).

## Round 4 (2026-11-17): every named data object compared

`audit/objcmp.py` compares each uniquely named initialized data object (.rodata, .data, init data)
in both kernels word by word, pointers resolved to symbol+offset or to the string they point at,
no alignment skip and no length cap (answers Gemini round 2's constant-data gaps). On build r9:
35,232 objects, 878,435 words, 8 objects differing. Fixes on the stock branch:

| Object | Stock | Tree before | Commit |
|---|---|---|---|
| gtp_dat_gt11 (sent to the GT911 at init) | RG DS config 0x5B, 640x480, 186 bytes, checksum valid | upstream example 0x43 (1200x1920) | 57c205fe11d9 |
| hynitron_i2c_driver.remove | hyn_remove | NULL (__exit_p, built in) | 3b405091fc47 |
| hyn_ts_pm_ops | suspend/resume only | + freeze/thaw/poweroff/restore | 3b405091fc47 |
| rk817 RESET_MODE reg field | 0xa3 bits 0..7 | 0..3 (upstream) | 8629e57d90c6 |
| sc7u22 acc/gyro ops int_status_reg | 0 | -1 (read by the IRQ path) | a718c35ef570 |
| sc7u22_gyro_range order | 125..2000 | 2000..125 | a718c35ef570 |

Build r10 (stock tip a718c35ef570): objects differing 1 (shipped_regdb_certs order, build-machine
directory order); EXACT 45,557, ADDR 24,202, ADDR_MAPPED 91, DIFF 73, address conflicts 0;
constant-data words: the 6 build-metadata items only. Fixes branch rgds-v1.21 = 10fa63ded8ab, still
stock + 3 FIX commits (6 lines, 3 files). None of this is boot-tested yet.

## Round 5 (2026-11-17): answers to Gemini round 3 (reviews/2026-11-17-gemini-round3.md)

Fixed in the tools, then rerun on build r10:
- objcmp.py now pairs repeated static names by occurrence order (only when both kernels have the
  same count; 19 names have different counts and are listed, not compared), checks .bss object
  sizes, and the stock-larger size check is on (the `and False` was a real bug). Result: 41,606
  objects compared, 926,179 words, 1 differing (shipped_regdb_certs order).
- Layout check by gaps (`audit/gapcmp-r10.txt`): for 57,040 paired symbols, the distance to the
  next symbol is identical in 56,882. The 158 others are section-boundary alias symbols (gap 0 on one
  side), anonymous string pools, alignment padding before 64-byte-aligned objects, and build
  metadata (kernel_config_data -16 = the 3 pahole lines, linux_proc_banner, the MPP revision
  string, .notes build-id).

Checked and rejected:
- "cma_activate_area 0x38 vs 0xf8 is a struct-layout divergence": both kernels load the page of
  memstart_addr (adrp) and read memstart_addr itself (stock d04000+0x38, ours d04000+0xd8 in r10).
  Same global, different page offset because the ro_after_init area starts 0xa0 later in ours. The
  same holds for read_cons_helper and hso_probe (same page, same pattern).
- "tramp_vectors is 1 KiB different": both at ffffffc009de2000; the 0x2040 bytes are byte-identical.
  Also byte-identical: _text (64 KiB head), __primary_switch, cpu_resume, sha256_block_data_order,
  __entry_tramp_text_end. The length difference was objdump decoding the stock blob differently.
- run_state_machine (tcpm): the SNK_ATTACH_WAIT block is laid out differently (the
  "disconnected -> SNK_UNATTACHED" and "sink -> SNK_DEBOUNCED" arms swap places); same conditions,
  same targets, 3 instructions longer in stock.

Still open, stated plainly:
- No formal (SMT) equivalence proof for the reconstructed-driver functions that differ in code
  layout (joypad mapping functions, keypad/tenpad, gpio_check/adc_check/probe, rk817_battery_work,
  led_update_charger_soc_status, speaker enable/disable). Reviewed by instruction trace and Ghidra.
- __LINE__ values in gt9xx, hynitron, cst, rk817_battery, rk817 codec: stock's line layout is not
  recoverable from the binary.
- Nothing has run on the device. A boot test needs Paul's go.

## Round 6 (2026-11-17): first boot test on the RG DS, and the duplicate-name gap

Boot test (Paul's go): the running image (stock kernel + Magisk + 12 W DT, sha b07cfac1) with only
the kernel replaced by rgds-v1.21; every changed byte inside the kernel section or the boot header
id; boot-image-gate PASS; dd from adb root with read-back hash; rollback image
`rgds-charge/bootlab/boot-device-20261117.img`. Marker that our kernel runs: /proc/kallsyms
hyn_probe at _text+0xcc2f30 (stock 0xcc2f50); the version banner is identical by design.
Exported-symbol CRCs: all 14,068 present in both, 6 differ (reconstructed-driver prototypes:
enable/disable_pa_spk_left/right, get_cw2015_soc, gts_dev); the only loaded module (8821cs Wi-Fi)
needs 225, all match.

Boot 1 found a regression: no accelerometer. The ICM42607 acc driver expects WHO_AM_I 0x61
(upstream); stock compares with 0x60, which the chip returns. strict2 never compared it because
`sensor_init` is a name shared by many sensor drivers: strict2 and text_ranges() skip every
non-unique function name. `audit/dupfuncs.py` now pairs repeated names by address order: 3,284
functions, 1,980 EXACT, 1,288 same code with addresses naming the same object or string, 16
flagged (the ICM sensor_init among them, as a control). Real fixes from it:

| Function | Stock | Tree before | Commit (stock / fixes) |
|---|---|---|---|
| icm4260x acc sensor_init | WHO_AM_I == 0x60 | 0x61 | 0aa43b716f2e / 633823bfd320 |
| icm4260x_gyro sensor_report_value | each axis / 10 | raw (10x stock) | 398b60e42933 / 6f78f93ecbeb |
| sc7u22 acc/gyro sensor_report_value | u8 register loop | int loop (same values) | 3f4b78df0d4e / d1cfee9e280a |

The rest of the 16: same globals at shifted addresses (cpufreq_driver, memstart_addr, this_client,
rk_remote_pwm_dbg_level, mc32x0_type, kbase strings), and gt9xx rockchip_panel_notifier_call with
the gtp_is_suspend store shared instead of duplicated (same behaviour). The 8-byte .bss shift comes
from the joypad .bss block: identical object set, different order, 8 more bytes of padding in stock.

Device results on rgds-v1.21 d1cfee9e280a (boot 3): 12 input devices as stock (gt9xx-0/1,
retrogame_joypad, virtual_gamepad, gpio-keys, headset-keys, rk817 Headset, adc-keys, gsensor, gyro,
rk805 pwrkey, bt-powerkey); GT911 reports config 0x5B, X_MAX 640 Y_MAX 480; Android lists the
accelerometer and gyroscope; gyro at rest, before -> after the /10 fix: RX mean -82.4 -> -8.0,
RY -5.7 -> -0.5, RZ 34.3 -> 3.4 (device/gyro-*.txt, captured with device/gyroprobe.c); 8821cs
loaded, Wi-Fi up, wireless adb back without hands; charger 0xE4/0xE5 = 0xde/0xdf; rk817 codec and
both aw87391 amps probed. Not checked from adb: touch input, button presses, audible sound, lid,
charge LED in a non-full state, 12 W at the meter.

Paul's hands-on check on rgds-v1.21 d1cfee9e280a (2026-11-17): touch good, power button good, lid
working (open: screen on; close: screen off); speaker and headphones play sound. Still unchecked:
joypad buttons, sticks and triggers (two captures saw no input), charge LED below full, 12 W at the meter.

Joypad on rgds-v1.21 d1cfee9e280a, 2026-11-17 08:22 (Paul pressing, GamepadTest open; GPIO watcher on
/sys/kernel/debug/gpio plus `getevent -lt`, device/joypad-capture-20261117.txt): retrogame_joypad
reported BTN_SOUTH(GAMEPAD)/EAST/NORTH/WEST, START, SELECT, TL, TR, TL2, TR2, HAT0X/HAT0Y -1..1,
both sticks ABS_X/Y/RX/RY -16384..16384, triggers ABS_Z/RZ 0..16384. The GPIO watcher saw the same
presses on the button lines. Not seen in this capture: stick clicks (THUMBL/THUMBR), RG_KEY, volume
keys (adc-keys / gpio-keys silent). Earlier "no joypad input" captures were timing misses, not a bug;
stock reads the same `adc value = 0` in amux_debug at rest.

Second key pass, 2026-11-17 09:33 (GamepadTest open, device/joypad-capture-20261117b.txt and
device/gpio-watch-20261117b.txt): every input reported. retrogame_joypad: A/B/X/Y, START, SELECT,
TL/TR, TL2/TR2, THUMBL/THUMBR (stick clicks), KEY_F10 (RG_KEY, home), HAT0X/Y -1..1, sticks full range,
triggers 0..16384. gpio-keys: KEY_VOLUMEUP, KEY_VOLUMEDOWN. adc-keys: KEY_BACK. The GPIO watcher saw
the matching lines (BTN_THUMBL/R, RG_KEY, ...). Joypad and keys: pass. Left by hand: charge LED below
full and 12 W at the meter.
Charge LED, 2026-11-17 09:38 (Paul's eyes plus driver log): plugged in at 100%: green
(led_update_charger_soc_status charger=1 soc=100); unplugged at 100%: no LED lit (charger=0,
gpio-21 battery_charging low). Charging colour below full and 12 W at the meter: still to check.
