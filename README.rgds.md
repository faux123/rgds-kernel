# Anbernic RG DS kernel source (reconstructed)

This tree rebuilds the kernel that ships on the Anbernic RG DS (RK3566),
firmware V1.21: Linux 6.1.141, Android 14. Anbernic did not publish the GPL
source for this kernel, so the source here is reconstructed from the shipped
binary and from public trees.

The stock kernel reports:

```
Linux version 6.1.141 (builder1@dell-PowerEdge-R730XD) (Android (10087095, +pgo, +bolt, +lto, -mlgo, based on r487747c) clang version 17.0.2 (https://android.googlesource.com/toolchain/llvm-project d9f89f4d16663d5012e5c09495f3b30ece3d2362), LLD 17.0.2) #31 SMP PREEMPT Tue Sep 29 16:47:09 CST 2026
```

## Base

- Rockchip BSP: `rockchip-linux/kernel` branch `develop-6.1` at
  `ea87d9c92c1be48d72603d37d012c98b2dad82b6` (2025-08-04). The stock binary
  contains code from `dd61e4243f21` (rk809/rk817 PMIC sleep switching) and lacks
  code from `ec6497d2fc7f` (isp idle wait), which pins the base to this window.
- Android Common Kernel: the delta of `android14-6.1-2025-07_r8`
  (SUBLEVEL 141) over upstream `v6.1.141` (`58485ff1a74f`), applied with
  `git apply -3`, plus the build fixes that the merge needed.
- Anbernic additions on top, one commit each (see the provenance table).

## Branches

- `rgds-v1.21-stock`: only what the stock kernel does, including its quirks.
  Commits titled `INTERIM (not stock)` or `TODO (stand-in, not stock)` mark
  stand-ins that later commits on the same branch replace.
- `rgds-v1.21`: `rgds-v1.21-stock` plus a short series of fixes, one commit
  each, so every change from stock behavior is visible on its own.

## Build

1. Get the toolchain the stock kernel used, Android clang `r487747c`. It is no
   longer on the AOSP prebuilts `main` branch; take it from
   `android14-release`:

   ```
   mkdir clang-r487747c
   curl -L https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/android14-release/clang-r487747c.tar.gz \
     | tar -xz -C clang-r487747c
   export PATH=$PWD/clang-r487747c/bin:$PATH
   ```

2. The host needs `bc`, `flex`, `bison`, and the OpenSSL and libelf headers.

3. Use the stock configuration (the device's `/proc/config.gz`, committed as
   `arch/arm64/configs/rgds_v1.21_stock.config`):

   ```
   cp arch/arm64/configs/rgds_v1.21_stock.config .config
   make ARCH=arm64 LLVM=1 olddefconfig
   ```

4. Build. `KCFLAGS` matches the ACK build environment (`build.config.common`);
   the `KBUILD_BUILD_*` variables reproduce the stock version banner byte for
   byte:

   ```
   export KCFLAGS=-D__ANDROID_COMMON_KERNEL__
   export KBUILD_BUILD_USER=builder1 KBUILD_BUILD_HOST=dell-PowerEdge-R730XD
   export KBUILD_BUILD_VERSION=31 KBUILD_BUILD_TIMESTAMP="Tue Sep 29 16:47:09 CST 2026"
   make -j"$(nproc)" ARCH=arm64 LLVM=1 Image dtbs
   ```

The vendor Wi-Fi and Bluetooth modules on the device report vermagic
`6.1.141 SMP preempt mod_unload modversions aarch64`.

## Provenance

| Area | Files | Source |
|---|---|---|
| Joypad (singleadc-joypad) | `drivers/input/joystick/singleadcjoy.c` | Reconstructed from stock disassembly (the interim Gamma driver it replaced remains in history) |
| Vibrator | `drivers/staging/android/timed_output.*`, `rk_timed_gpio.c` | Copied from TheGammaSqueeze/rg_ds_kernel `ba9053b290`; verified instruction-identical to the stock binary function by function, initcall lines match (stock has no `timed_gpio`) |
| Board device tree | `arch/arm64/boot/dts/rockchip/rk3566-anbernic-rg-ds.dts` | Rebuilt from the stock DTB on the Rockchip SoC dtsi; no semantic difference from stock (tools in `scripts/rgds/dts/`) |
| xt_quota2 | `net/netfilter/xt_quota2.c` | Android Common Kernel (part of the ACK delta) |
| Wi-Fi (RTL8821CS) | `drivers/net/wireless/rockchip_wlan/rtl8821cs/` | Realtek v5.15.9.3 from the Rockchip SDK (rkwifibt), built as a module (`CONFIG_RTL8821CS=m`, stock option set); the built `8821cs.o` matches the stock `8821cs.ko` (5044 functions, every size equal). Stock ships it out of tree, so its config has `RTL8821CS` unset |
| Wi-Fi (RTL8822CS) | `drivers/net/wireless/rockchip_wlan/rtl8822cs/` | Realtek v5.15.8.3-17 release tarball (public upload `libc0607/rtl8822cs-20240221`), one labelled build fix, stock option set; 5344/5344 symbols, every function the same size as the stock `8822cs.ko` (1901680 bytes) |
| Wi-Fi/BT (AIC8800) | `drivers/net/wireless/rockchip_wlan/aic8800/` | Rockchip SDK `external/wifi_driver` (public mirror `TinkerBoard-Android/rockchip-android-external-wifi_driver`); `aic8800_bsp` 128/128, `aic8800_btlpm` 5/5, `aic8800_fdrv` 508/508 symbols, all sizes equal to stock |
| Wi-Fi (Seekwave EA6621Q) | `drivers/net/wireless/rockchip_wlan/ea6621q/` | Same SDK mirror; `skw` 421/421, `skw_sdio` 102/102, `skw_bootcoms` 44/44 symbols, all sizes equal to stock |
| Wi-Fi (Infineon CYW88459) | `drivers/net/wireless/rockchip_wlan/infineon/` | Same SDK mirror (DHD 100.10.130), minus one log line stock lacks; 2232/2232 symbols, all sizes equal to stock `cyw88459.ko` (1137520 bytes) |
| Panel (panel-simple) | `drivers/gpu/drm/panel/panel-simple.c` | Rockchip base plus stock's `enable1` GPIO and fixed prepare reset sequence, reconstructed from disassembly (0 opcode differences in probe/prepare/unprepare) |
| DMA heaps | `drivers/dma-buf/heaps/rk_system_heap.c`, `rk_cma_heap.c` | Rockchip heaps built as stock does, ported to 6.1 |
| SC7U22 accelerometer and gyroscope | `drivers/input/sensors/{accel,gyro}/sc7u22_*.c` | Reconstructed from stock disassembly |
| RK817 battery | `drivers/power/supply/rk817_battery.c` | Rockchip `develop-4.19` revision (`d1d1ad90afcc`), ported to 6.1, plus stock-only parts reconstructed from disassembly: resume handling, low-power check, field table tail, LED layer, `/sys/rk809/battery_adjust`, CW2015 SOC reporting |
| CW2015 gauge | `drivers/power/supply/cw2015_battery.c` | Rockchip base driver plus Anbernic SOC sharing and charger state, reconstructed from disassembly |
| Speaker amplifiers (AW87391) | `sound/soc/rockchip/aw_pa_left.c`, `aw_pa_right.c` | Reconstructed from stock disassembly |
| RK817 codec | `sound/soc/codecs/rk817_codec.c` | Rockchip base plus the stock amplifier calls in `rk817_digital_mute`, from disassembly |
| GT9xx touchscreen | `drivers/input/touchscreen/gt9xx/` | Rockchip base built like stock, plus Anbernic touch-swap globals and the stock panel-notifier suspend/resume, reconstructed from disassembly |
| Hynitron touchscreen | `drivers/input/touchscreen/hynitron/` | Hynitron V2.2 20210129 skeleton, trimmed and matched function by function to the stock V2.1 binary (names, firmware tables, touch reporting, sleep sequence) |
| Hall sensor | `drivers/input/keyboard/gpio_keys.c` | Rockchip base plus the stock hall-sensor handling and `hall_status` attribute, from disassembly |

Commit messages name the stock addresses that each reconstruction is based on.

## Status

- The tree builds with the stock configuration. The only stock configuration
  symbol without a match is `PAHOLE_HAS_SPLIT_BTF`, which depends on the
  build host's pahole.
- The board DTB has no semantic difference from the stock DTB
  (`scripts/rgds/dts/gate.sh`).
- Symbol comparison against the stock kernel's kallsyms: 54 stock symbols are
  not in this build (most are initcall names whose line numbers differ) and
  15 symbols in this build are not in stock.
- Nothing in this tree has been tested on hardware yet.

## Flashing warning

The RG DS boot partition uses a Rockchip RSCE resource image that stores a SHA1
for every entry. If you replace the kernel or the device tree, update every
entry's hash. Never flash a boot image without first checking every hash in it;
a mismatch can leave the device unable to boot.
