#!/system/bin/sh
# Interleaved BORE A/B on one boot: for each round and load, run bore=0 then bore=1 (order flips
# every round), each arm = lat 5 s + frame 5 s + 100 fork+exec. Output: one block per arm.
B=/data/local/tmp/bore-bench; OUT=${1:-/data/local/tmp/ab.txt}; ROUNDS=${2:-8}
: > "$OUT"
r=1
while [ $r -le "$ROUNDS" ]; do
  for load in 0 4 16; do
    if [ $((r % 2)) -eq 1 ]; then order="0 1"; else order="1 0"; fi
    for s in $order; do
      echo 0 > /proc/sys/kernel/sched_bore; echo "$s" > /proc/sys/kernel/sched_bore
      sleep 2
      t=$(cat /sys/class/thermal/thermal_zone0/temp)
      { echo "arm round=$r load=$load bore=$(cat /proc/sys/kernel/sched_bore) temp=$t gov=$(cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor)"
        $B lat $load 5 | sed 's/^/  /'
        $B frame $load 5 | sed 's/^/  /'
        $B fork $load 100 | sed 's/^/  /'
      } >> "$OUT"
    done
  done
  r=$((r + 1))
done
echo 0 > /proc/sys/kernel/sched_bore
echo "ALL_DONE" >> "$OUT"
