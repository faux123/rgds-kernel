#!/system/bin/sh
# BORE penalty-offset A/B under 16 nice-0 hogs: does offset 25 (spares ~33 ms bursts, i.e. the
# emulator's ~21 ms frame work) keep BORE's protection against endless hogs?
# Arms per round (order rotates): bore0 = stock CFS, o22 = BORE default, o25 = BORE offset 25.
# Each arm = lat 5 s + frame 5 s. Restores sched_bore=1, offset 22 at exit.
B=/data/local/tmp/bore-bench; OUT=${1:-/data/local/tmp/offset.txt}; ROUNDS=${2:-3}
BORE=/proc/sys/kernel/sched_bore; OFF=/proc/sys/kernel/sched_burst_penalty_offset
restore() { echo 22 > $OFF; echo 1 > $BORE; }
trap restore EXIT
: > "$OUT"
arm() {
  case $1 in
    bore0) echo 22 > $OFF; echo 0 > $BORE ;;
    o22)   echo 0 > $BORE; echo 22 > $OFF; echo 1 > $BORE ;;
    o25)   echo 0 > $BORE; echo 25 > $OFF; echo 1 > $BORE ;;
  esac
  sleep 2
  { echo "arm round=$r name=$1 bore=$(cat $BORE) offset=$(cat $OFF) temp=$(cat /sys/class/thermal/thermal_zone0/temp)"
    $B lat 16 5 | sed 's/^/  /'
    $B frame 16 5 | sed 's/^/  /'
  } >> "$OUT"
}
r=1
while [ $r -le "$ROUNDS" ]; do
  case $((r % 3)) in
    1) order="bore0 o22 o25" ;; 2) order="o25 bore0 o22" ;; 0) order="o22 o25 bore0" ;;
  esac
  for a in $order; do arm $a; done
  r=$((r + 1))
done
echo "ALL_DONE" >> "$OUT"
