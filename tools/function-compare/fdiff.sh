#!/bin/bash
# fdiff.sh <obj> <func>: opcode diff of a function in our object vs stock.elf
K=/extra2/projects/home/rgds-kernel; S=/tmp/claude-1000/-home-paul/1533173a-713b-4520-83dd-0a9f4041a1c2/scratchpad
export PATH=$K/toolchain/clang-r487747c/bin:$PATH
o=$1; n=$2
a=$(grep -E " $n\$" $K/symdiff/stock-nm.txt | head -1 | awk '{print $1}'); e=$(grep -A1 -E " $n\$" $K/symdiff/stock-nm.sorted.txt | tail -1 | awk '{print $1}')
llvm-objdump -d --no-show-raw-insn --start-address=0x$a --stop-address=0x$e $K/stock/stock.elf > $S/fd_s.dis
oa=$(llvm-nm -S $o | awk -v n=" $n\$" '$0~n && $3~/[tT]/{print $1; exit}'); oz=$(llvm-nm -S $o | awk -v n=" $n\$" '$0~n && $3~/[tT]/{print $2; exit}')
sec=$(llvm-objdump -t $o | awk -v n="$n" '$NF==n{print $4; exit}')
llvm-objdump -dr --no-show-raw-insn -j $sec --start-address=0x$oa --stop-address=$((0x$oa+0x$oz)) $o > $S/fd_o.dis
echo "stock $((0x$e-0x$a)) ours $((0x$oz))"
diff <(grep -E '^\s*[0-9a-f]+:\s' $S/fd_s.dis | awk -F'\t' '{print $2"\t"$3}' | sed -E 's/0x[0-9a-f]+ <[^>]*>/T/; s/#0x[0-9a-f]+\]/#O]/') <(grep -E '^\s+[0-9a-f]+:\s' $S/fd_o.dis | awk -F'\t' '{print $2"\t"$3}' | sed -E 's/0x[0-9a-f]+ <[^>]*>/T/; s/#0x[0-9a-f]+\]/#O]/') | head -${3:-40}
