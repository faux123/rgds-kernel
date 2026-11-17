#!/usr/bin/env python3
# For ADDR_MAY functions: the offset deltas between stock and ours on the differing loads/stores
# (strict, all-paths analysis). A delta shared by many unrelated functions is a data-section shift;
# an odd small delta in one driver points at a struct layout difference.
import sys, re, subprocess, collections
sys.path.insert(0, '/extra2/projects/home/rgds-kernel/audit')
import strict_audit as S
ours, tsv = sys.argv[1], sys.argv[2]
names = [l.split('\t')[1] for l in open(tsv).read().splitlines()[1:] if l.startswith('ADDR_MAY')]
sr, orr = S.text_ranges(S.STOCK), S.text_ranges(ours)
def get(elf, r, fn):
    a, b = r[fn]
    out = subprocess.run([S.T+'llvm-objdump','-d','--no-show-raw-insn','--start-address=%#x'%a,'--stop-address=%#x'%b,elf],capture_output=True,text=True).stdout
    seq, ad = [], []
    for l in out.splitlines():
        m = re.match(r'^\s*([0-9a-f]{16}):\s+(\S+)\s*(.*)$', l)
        if m: ad.append(int(m.group(1),16)); seq.append((m.group(2), m.group(3).strip()))
    return S.norm(fn, seq, a, b, ad, True)
for fn in names:
    s, o = get(S.STOCK, sr, fn), get(ours, orr, fn)
    d = collections.Counter()
    for x, y in zip(s, o):
        if x != y and x[0] == y[0]:
            a = re.findall(r'#(0x[0-9a-f]+)\]', x[1]); b = re.findall(r'#(0x[0-9a-f]+)\]', y[1])
            if a and b: d[int(a[0],16) - int(b[0],16)] += 1
    print('%-36s %s' % (fn, dict(d)))
