#!/usr/bin/env python3
# strictdiff.py <our vmlinux> <func> [flow]: show the instruction pairs that still differ after
# strict normalization (or the flow-insensitive variant with "flow").
import sys, re, subprocess
sys.path.insert(0, '/extra2/projects/home/rgds-kernel/audit')
import strict_audit as S
ours, fn = sys.argv[1], sys.argv[2]; flow = len(sys.argv) > 3
def get(elf):
    r = S.text_ranges(elf)[fn]
    out = subprocess.run([S.T+'llvm-objdump','-d','--no-show-raw-insn','--start-address=%#x'%r[0],'--stop-address=%#x'%r[1],elf],capture_output=True,text=True).stdout
    seq, ad = [], []
    for l in out.splitlines():
        m = re.match(r'^\s*([0-9a-f]{16}):\s+(\S+)\s*(.*)$', l)
        if m: ad.append(int(m.group(1),16)); seq.append((m.group(2), m.group(3).strip()))
    return S.norm(fn, seq, r[0], r[1], ad, flow), r[0], ad
s, sa, sad = get(S.STOCK); o, oa, oad = get(ours)
for i, (x, y) in enumerate(zip(s, o)):
    if x != y: print('%04x  %-44s | %s' % (sad[i]-sa, ' '.join(x), ' '.join(y)))
if len(s) != len(o): print('length', len(s), len(o))
