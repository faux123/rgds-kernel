#!/system/bin/sh
# Frame benchmark under arms A and B (2 hogs, 20 s), two rounds, then restore.
cd /data/local/tmp
for r in 1 2; do
	for a in A B; do
		sh arm.sh $a; sleep 3
		echo "ARM=$a round=$r $(sh arm.sh show)"
		./bore-bench frame 2 20
	done
done
sh arm.sh restore
