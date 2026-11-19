#!/system/bin/sh
# RETIRED 2026-11-19: main f5e53bcdcb74 boots with sched_bore=1. Only needed on kernels before it.
# Magisk service.d hook: arm BORE at boot on the RG DS kernel (main).
# The kernel boots with kernel.sched_bore=0 (stock CFS); this turns BORE on once Android is up.
# Disable: rm /data/adb/service.d/rgds-bore.sh, or `sysctl kernel.sched_bore=0` for this boot.
LOG=/data/local/tmp/rgds-bore.log
F=/proc/sys/kernel/sched_bore
if [ ! -w "$F" ]; then
  echo "$(date) no $F: kernel without BORE, nothing to do" >> "$LOG"
  exit 0
fi
until [ "$(getprop sys.boot_completed)" = 1 ]; do sleep 2; done
echo 1 > $F
echo "$(date) uptime $(cut -d' ' -f1 /proc/uptime)s sched_bore=$(cat $F)" >> "$LOG"
# keep the log bounded: last 50 boots
tail -n 50 "$LOG" > "$LOG.tmp" && mv "$LOG.tmp" "$LOG"
