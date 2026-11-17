# SPDX-License-Identifier: GPL-2.0
# Hynitron touchscreen, stock RG DS "Rockchip Driver V2.1 20201222" reconstruction.
# Skeleton: Hynitron Spreadtrum V2.2 20210129 (Volkanite/G93_G0910), trimmed to the stock call graph.
# Stock carries no esd/gesture/proximity/tool symbols, so only these objects are built.
obj-$(CONFIG_TOUCHSCREEN_HYN) += hynitron_ts.o
hynitron_ts-objs := hynitron_core.o hynitron_i2c.o hynitron_update_firmware.o
