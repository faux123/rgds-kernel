# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_BCMDHD)	+= rkwifi/
obj-$(CONFIG_RTL8821CS)	+= rtl8821cs/

# Out-of-tree vendor modules that stock ships in /vendor_dlkm/lib/modules,
# imported from Rockchip's Android 14 SDK external/wifi_driver. They have no
# Kconfig entry (stock's .config has none), so they build only on request:
#   make ARCH=arm64 LLVM=1 RGDS_VENDOR_WIFI=m drivers/net/wireless/rockchip_wlan/<dir>/
# The exports below are copied from the SDK's external/wifi_driver/Makefile.
ifeq ($(RGDS_VENDOR_WIFI),m)
export CONFIG_AIC_WLAN_SUPPORT = y
export CONFIG_AIC_FW_PATH = "/vendor/etc/firmware"
export CONFIG_AIC8800_WLAN_SUPPORT = m
obj-m += aic8800/
endif
