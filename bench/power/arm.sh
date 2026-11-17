#!/system/bin/sh
# Governor A/B arm for RG DS battery-drain work. Run as root on the device.
#   arm.sh save              -> snapshot governors + node perms to /data/local/tmp/pwr/saved
#   arm.sh A                 -> Anbernic state: performance on cpu, gpu, vop, rkvdec
#   arm.sh B                 -> dynamic: cpu schedutil, gpu+vop simple_ondemand
#   arm.sh restore           -> put back exactly what `save` recorded
#   arm.sh log <secs> <tag>  -> sample every 2 s into /data/local/tmp/pwr/<tag>.tsv
CPU=/sys/devices/system/cpu/cpufreq/policy0
GPU=/sys/class/devfreq/fde60000.gpu
VOP=/sys/class/devfreq/fe040000.vop
VDEC=/sys/class/devfreq/fdf80200.rkvdec
BAT=/sys/class/power_supply/battery
D=/data/local/tmp/pwr
mkdir -p $D

case "$1" in
save)
	{
		echo "cpu $(cat $CPU/scaling_governor) $(stat -c '%U:%G %a' $CPU/scaling_governor)"
		echo "gpu $(cat $GPU/governor) $(stat -c '%U:%G %a' $GPU/governor)"
		echo "vop $(cat $VOP/governor) $(stat -c '%U:%G %a' $VOP/governor)"
		echo "vdec $(cat $VDEC/governor) $(stat -c '%U:%G %a' $VDEC/governor)"
	} > $D/saved
	cat $D/saved ;;
A)
	echo performance > $CPU/scaling_governor
	for n in $GPU $VOP $VDEC; do echo performance > $n/governor; done ;;
B)
	echo schedutil > $CPU/scaling_governor
	echo simple_ondemand > $GPU/governor
	echo simple_ondemand > $VOP/governor ;;
restore)
	while read -r what gov own mode; do
		case $what in
		cpu) f=$CPU/scaling_governor ;; gpu) f=$GPU/governor ;;
		vop) f=$VOP/governor ;; vdec) f=$VDEC/governor ;; *) continue ;;
		esac
		echo "$gov" > "$f"; chown "$own" "$f"; chmod "$mode" "$f"
	done < $D/saved
	sh /data/local/tmp/arm.sh show ;;
show)
	echo "cpu $(cat $CPU/scaling_governor) gpu $(cat $GPU/governor) vop $(cat $VOP/governor) vdec $(cat $VDEC/governor)" ;;
log)
	secs=${2:-600}; tag=${3:-run}; out=$D/$tag.tsv
	echo "t_s	uA	uV	cap	cpu_khz	gpu_hz	status	fg" > $out
	t0=$(cut -d. -f1 /proc/uptime); end=$((t0 + secs))
	while [ "$(cut -d. -f1 /proc/uptime)" -lt $end ]; do
		t=$(( $(cut -d. -f1 /proc/uptime) - t0 ))
		echo "$t	$(cat $BAT/current_now)	$(cat $BAT/voltage_now)	$(cat $BAT/capacity)	$(cat $CPU/scaling_cur_freq)	$(cat $GPU/cur_freq)	$(cat $BAT/status)	$(cat $CPU/scaling_governor)" >> $out
		sleep 2
	done
	echo "wrote $out ($(($(wc -l < $out) - 1)) samples)" ;;
*)
	echo "usage: $0 save|A|B|restore|show|log <secs> <tag>"; exit 1 ;;
esac
