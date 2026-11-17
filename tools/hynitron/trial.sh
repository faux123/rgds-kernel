#!/bin/sh
# trial.sh FILE OBJ FUNC: transplant MTK body, build, compare; keep only if size closer to stock or same size with more same-pos.
F=$1; O=$2; FN=$3
K=/extra2/projects/home/rgds-kernel; W=$K/wt-joypad; H=$W/drivers/input/touchscreen/hynitron
S=/tmp/claude-1000/-home-paul/1533173a-713b-4520-83dd-0a9f4041a1c2/scratchpad
TOOLS=/extra2/projects/home/rgds-charge/kbase/hynitron_stock/tools
export PATH=$S/bcshim:$K/toolchain/clang-r487747c/bin:$PATH KCFLAGS="-D__ANDROID_COMMON_KERNEL__"
before=$(python3 -I $TOOLS/fncmp_obj.py $H/$O $FN)
python3 -I $TOOLS/try_mtk.py $F $FN || exit 1
cd $W; if ! make -j8 ARCH=arm64 LLVM=1 drivers/input/touchscreen/hynitron/ >/dev/null 2>$S/trial.err; then echo "BUILD FAIL: $(grep -m2 error $S/trial.err)"; mv $H/$F.bak $H/$F; make -j8 ARCH=arm64 LLVM=1 drivers/input/touchscreen/hynitron/ >/dev/null 2>&1; exit 2; fi
after=$(python3 -I $TOOLS/fncmp_obj.py $H/$O $FN)
echo "before: $before"; echo "after:  $after"
python3 -I - "$before" "$after" <<'PY'
import re,sys
def p(l):
    m=re.search(r'stock=\s*(\d+) ours=\s*(\d+) insn \d+/\d+ same-pos=(\d+)',l); return int(m.group(1)),int(m.group(2)),int(m.group(3))
s,b,bp=p(sys.argv[1]); _,a,ap=p(sys.argv[2])
better = abs(a-s)<abs(b-s) or (abs(a-s)==abs(b-s) and ap>bp)
print('KEEP' if better else 'REVERT'); open('/tmp/claude-1000/-home-paul/1533173a-713b-4520-83dd-0a9f4041a1c2/scratchpad/trial.verdict','w').write('KEEP' if better else 'REVERT')
PY
if [ "$(cat $S/trial.verdict)" = REVERT ]; then mv $H/$F.bak $H/$F; make -j8 ARCH=arm64 LLVM=1 drivers/input/touchscreen/hynitron/ >/dev/null 2>&1; else rm -f $H/$F.bak; fi
