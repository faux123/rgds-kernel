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
- Android Common Kernel: the delta of `android14-6.1-2025-07_r13`
  (SUBLEVEL 141) over upstream `v6.1.141` (`58485ff1a74f`), applied with
  `git apply -3`, plus the build fixes that the merge needed.
- Anbernic additions on top, one commit each (see the provenance table).

## Branches

- `rgds-v1.21-stock`: only what the stock kernel does, including its quirks.
  Commits titled `INTERIM (not stock)` or `TODO (stand-in, not stock)` mark
  parts whose stock source is not reconstructed yet.
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
| Joypad (singleadc-joypad) | `drivers/input/joystick/singleadcjoy.c` | INTERIM: TheGammaSqueeze/rg_ds_kernel `ba9053b290`. The stock driver is being reconstructed from disassembly and will replace it. |
| Vibrator | `drivers/staging/android/timed_output.*`, `rk_timed_gpio.c` | TheGammaSqueeze/rg_ds_kernel `ba9053b290`; matches stock's initcalls (stock has no `timed_gpio`) |
| Board device tree | `arch/arm64/boot/dts/rockchip/rk3566-anbernic-rg-ds.dts` | TheGammaSqueeze/rg_ds_kernel `ba9053b290` |
| xt_quota2 | `net/netfilter/xt_quota2.c` | Android Common Kernel (part of the ACK delta) |
| SC7U22 accelerometer and gyroscope | `drivers/input/sensors/{accel,gyro}/sc7u22_*.c` | Reconstructed from stock disassembly |
| RK817 battery | `drivers/power/supply/rk817_battery.c` | Rockchip `develop-4.19` revision (`d1d1ad90afcc`), ported to 6.1, plus stock-only parts reconstructed from disassembly: resume handling, low-power check, field table tail, LED layer, `/sys/rk809/battery_adjust`, CW2015 SOC reporting |
| CW2015 gauge | `drivers/power/supply/cw2015_battery.c` | Rockchip base driver plus Anbernic SOC sharing and charger state, reconstructed from disassembly |
| Speaker amplifiers (AW87391) | `sound/soc/rockchip/aw_pa_left.c`, `aw_pa_right.c` | Reconstructed from stock disassembly |
| RK817 codec | `sound/soc/codecs/rk817_codec.c` | Rockchip base plus the stock amplifier calls in `rk817_digital_mute`, from disassembly |
| GT9xx touchscreen | `drivers/input/touchscreen/gt9xx/gt9xx.c` | Rockchip base plus Anbernic touch-swap globals, reconstructed from disassembly |
| Hynitron touchscreen | `drivers/input/touchscreen/hynitron/` | TODO stand-in: radxa/allwinner-bsp V3.0 (closest public version; stock is "Rockchip Driver V2.1 20201222", source not found) plus a Rockchip device tree and IRQ glue |
| Hall sensor | `drivers/input/keyboard/gpio_keys.c` | Rockchip base plus the stock hall-sensor handling and `hall_status` attribute, from disassembly |

Commit messages name the stock addresses that each reconstruction is based on.

## Status

- The tree builds with the stock configuration. Three stock configuration
  symbols have no source yet: `RTL8821CS` (the Wi-Fi driver is an external
  module), `ROCKCHIP_EBC_DEV`, and `PAHOLE_HAS_SPLIT_BTF` (host tool only).
- Symbol comparison against the stock kernel's kallsyms: 578 stock symbols are
  not yet in this build and 51 symbols in this build are not in stock. Most of
  the remaining stock-only symbols belong to the joypad driver.
- Nothing in this tree has been tested on hardware yet.

## Flashing warning

The RG DS boot partition uses a Rockchip RSCE resource image that stores a SHA1
for every entry. If you replace the kernel or the device tree, update every
entry's hash. Never flash a boot image without first checking every hash in it;
a mismatch can leave the device unable to boot.
