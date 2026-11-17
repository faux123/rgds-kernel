#!/usr/bin/env python3
"""dupdiff.py <our vmlinux> <stock addr> <our addr> <stock len> <our len>: diff one function by address."""
import re, subprocess, sys, difflib
sys.path.insert(0, '/extra2/projects/home/rgds-kernel/audit')
import strict2 as Z, strict_audit as S
ours = sys.argv[1]; sa, oa, sl, ol = int(sys.argv[2], 16), int(sys.argv[3], 16), int(sys.argv[4]), int(sys.argv[5])
def get(elf, a, n):
    out = subprocess.run([S.T + 'llvm-objdump', '-d', '--no-show-raw-insn', '--start-address=%#x' % a, '--stop-address=%#x' % (a + n), elf], capture_output=True, text=True).stdout
    seq, ad = [], []
    for l in out.splitlines():
        m = re.match(r'^\s*([0-9a-f]{16}):\s+(\S+)\s*(.*)$', l)
        if m: ad.append(int(m.group(1), 16)); seq.append((m.group(2), m.group(3).strip()))
    return [re.sub(r'<SELF\+0x[0-9a-f]+>', '<L>', o + ' ' + g) for o, g in Z.norm('?', seq, a, a + n, ad, True)[0]]
for l in difflib.unified_diff(get(S.STOCK, sa, sl), get(ours, oa, ol), lineterm='', n=3):
    print(l)
