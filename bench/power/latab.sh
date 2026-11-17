#!/system/bin/sh
# Wakeup latency at idle (lat, 0 hogs, 15 s) under arms A and B, two rounds, then restore.
cd /data/local/tmp
for r in 1 2; do
	for a in A B; do
		sh arm.sh $a; sleep 3
		echo "ARM=$a round=$r $(sh arm.sh show)"
		./bore-bench lat 0 15
	done
done
sh arm.sh restore
