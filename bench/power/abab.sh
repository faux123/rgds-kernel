#!/system/bin/sh
# Unplugged idle current, governor arms A (Anbernic performance) / B (dynamic), 120 s blocks.
A=/data/local/tmp/arm.sh
D=/data/local/tmp/pwr
mkdir -p $D
{ date; cat /sys/class/power_supply/ac/online /sys/class/power_supply/usb/online; settings get system screen_brightness; dumpsys power | grep -m1 mWakefulness; dumpsys window | grep -m1 mCurrentFocus; } > $D/abab-meta.txt 2>&1
sh $A save > /dev/null
n=1
for arm in A B A B; do
	sh $A $arm
	sleep 10	# let clocks settle; not logged
	sh $A log 110 abab-$n-$arm > /dev/null
	n=$((n+1))
done
sh $A restore >> $D/abab-meta.txt
echo END >> $D/abab-meta.txt
