#!/system/bin/sh
# SD read-ahead x IO scheduler A/B on the RG DS microSD (mmcblk0). Restores the original settings.
P=/data/local/tmp/sdprobe; Q=/sys/block/mmcblk0/queue
SD=/mnt/media_rw/3336-3838; NDS=$SD/ROMs/NDS
SEQF="$NDS/Kingdom Hearts - 358-2 Days (US)(M2).nds"
RNDF="$NDS/Dragon Quest IX - Hoshizora no Mamoribito (JP).nds"
OUT=${1:-/data/local/tmp/sd-ab.txt}; ROUNDS=${2:-3}
S0=$(sed 's/.*\[\(.*\)\].*/\1/' $Q/scheduler); R0=$(cat $Q/read_ahead_kb)
drop() { sync; echo 3 > /proc/sys/vm/drop_caches; sleep 1; }
: > "$OUT"
echo "orig sched=$S0 ra=$R0" >> "$OUT"
r=1
while [ $r -le "$ROUNDS" ]; do
  if [ $((r % 2)) -eq 1 ]; then CONFS="mq-deadline:128 bfq:128 mq-deadline:512 bfq:512 mq-deadline:2048 bfq:2048"
  else CONFS="bfq:2048 mq-deadline:2048 bfq:512 mq-deadline:512 bfq:128 mq-deadline:128"; fi
  for c in $CONFS; do
    s=${c%%:*}; ra=${c##*:}
    echo "$s" > $Q/scheduler; echo "$ra" > $Q/read_ahead_kb
    echo "arm round=$r sched=$(sed 's/.*\[\(.*\)\].*/\1/' $Q/scheduler) ra=$(cat $Q/read_ahead_kb) kind=idle" >> "$OUT"
    drop; $P seq "$SEQF" | sed 's/^/  /' >> "$OUT"
    drop; $P rand "$RNDF" 1500 32 $r | sed 's/^/  /' >> "$OUT"
    drop; $P scan "$NDS" 300 16 | sed 's/^/  /' >> "$OUT"
  done
  # Contention: random reads while a 256 MiB write goes to the same card (savestate / download)
  for s in mq-deadline bfq; do
    echo "$s" > $Q/scheduler; echo 512 > $Q/read_ahead_kb
    echo "arm round=$r sched=$(sed 's/.*\[\(.*\)\].*/\1/' $Q/scheduler) ra=$(cat $Q/read_ahead_kb) kind=write" >> "$OUT"
    drop
    dd if=/dev/zero of="$SD/sdbench.tmp" bs=1M count=256 conv=fsync 2>/dev/null &
    W=$!
    sleep 1
    $P rand "$RNDF" 600 32 $((r + 100)) | sed 's/^/  /' >> "$OUT"
    wait $W
    rm -f "$SD/sdbench.tmp"
  done
  r=$((r + 1))
done
echo "$S0" > $Q/scheduler; echo "$R0" > $Q/read_ahead_kb
echo "restored sched=$(cat $Q/scheduler) ra=$(cat $Q/read_ahead_kb)" >> "$OUT"
echo ALL_DONE >> "$OUT"
