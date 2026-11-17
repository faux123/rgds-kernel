#!/system/bin/sh
# Magisk service.d hook (RG DS): microSD IO settings and zram size. Measured 2026-11-17, see
# bench/sd-ab-20261117-summary.md and RECON_NOTES.md.
#  - microSD: bfq + read_ahead_kb 512. 256 MiB ROM load 4.97 -> 4.49 s; random reads while the card
#    is being written 3.2 -> 22.3 MB/s, worst stall 1.2 s -> 64 ms. Re-applied if the card is swapped.
#  - zram: disksize = MemTotal (was 75%), lz4, page-cluster 0. zram only uses RAM for what it stores.
# Undo: rm /data/adb/service.d/rgds-tune.sh and reboot.
LOG=/data/local/tmp/rgds-tune.log
log() { echo "$(date '+%F %T') $*" >> "$LOG"; }

sd_queue() {  # the microSD slot is the dwmmc controller at fe2b0000
  for b in /sys/block/mmcblk*; do
    case "$(readlink -f "$b/device")" in *fe2b0000.dwmmc*) echo "$b/queue"; return;; esac
  done
}

sd_apply() {
  q=$(sd_queue)
  [ -n "$q" ] || return 0
  grep -q '\[bfq\]' "$q/scheduler" || { echo bfq > "$q/scheduler" && log "sd $q scheduler=bfq"; }
  [ "$(cat "$q/read_ahead_kb")" = 512 ] || { echo 512 > "$q/read_ahead_kb" && log "sd $q read_ahead_kb=512"; }
}

zram_apply() {
  Z=/sys/block/zram0
  [ -e $Z/disksize ] || { log "no zram0"; return; }
  want=$(awk '/^MemTotal/ {print $2 * 1024}' /proc/meminfo)
  echo 0 > /proc/sys/vm/page-cluster
  # Android's own swapon of zram0 can land after sys.boot_completed (seen 2026-11-17: disksize still
  # 0 when this ran). Wait up to 3 minutes for it so the two setups never race.
  n=0
  while ! grep -q zram0 /proc/swaps && [ $n -lt 90 ]; do sleep 2; n=$((n + 1)); done
  have=$(cat $Z/disksize)
  [ "$have" -ge "$want" ] && return
  if grep -q zram0 /proc/swaps; then swapoff /dev/block/zram0 || { log "zram swapoff FAILED"; return; }; fi
  # About 2 GB is free after boot, so moving the swapped pages back into RAM is safe.
  if echo 1 > $Z/reset && echo lz4 > $Z/comp_algorithm \
     && echo "$want" > $Z/disksize && mkswap /dev/block/zram0 > /dev/null && swapon /dev/block/zram0; then
    log "zram disksize $have -> $(cat $Z/disksize)"
  else
    log "zram resize FAILED (have=$have want=$want)"
  fi
}

until [ "$(getprop sys.boot_completed)" = 1 ]; do sleep 2; done
sd_apply
zram_apply
tail -n 100 "$LOG" > "$LOG.tmp" && mv "$LOG.tmp" "$LOG"
# A swapped card comes back with kernel defaults; check every 30 s (two sysfs reads; the timer stops while the device is suspended).
while true; do sleep 30; sd_apply; done
