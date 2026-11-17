#!/usr/bin/env python3
"""xref.py <elf> <lo> <hi>...: functions whose adrp+add/ldr/str pairs (linear, per function)
reference data addresses inside the given ranges. Rough xref for stock, which has no data symbols."""
import re, subprocess, sys, collections
T = '/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin/'
elf = sys.argv[1]
rng = [(int(sys.argv[i], 16), int(sys.argv[i + 1], 16)) for i in range(2, len(sys.argv), 2)]
p = subprocess.Popen([T + 'llvm-objdump', '-d', '--no-show-raw-insn', elf], stdout=subprocess.PIPE, text=True, bufsize=1 << 20)
fn = '?'; page = {}
hits = collections.defaultdict(set)
for l in p.stdout:
    m = re.match(r'^[0-9a-f]{16} <(.+)>:', l)
    if m:
        fn = m.group(1); page = {}; continue
    m = re.match(r'^\s*[0-9a-f]{16}:\s+(\S+)\s*(.*)$', l)
    if not m:
        continue
    op, arg = m.groups()
    regs = [t.strip() for t in arg.split(',')]
    if op == 'adrp':
        v = re.search(r'0x([0-9a-f]+)', arg)
        page['x' + regs[0][1:]] = int(v.group(1), 16); continue
    t = None
    mm = re.search(r'\[(x\d+)(?:, #(0x[0-9a-f]+))?\]', arg)
    if mm and mm.group(1) in page and op.startswith(('ld', 'st')):
        t = page[mm.group(1)] + int(mm.group(2) or '0', 16)
    elif op == 'add' and len(regs) == 3 and ('x' + regs[1][1:]) in page and regs[2].startswith('#'):
        t = page['x' + regs[1][1:]] + int(regs[2][1:].split()[0], 0)
    if t is not None:
        for a, b in rng:
            if a <= t < b:
                hits[t].add(fn)
for t in sorted(hits):
    print('%x %s' % (t, ' '.join(sorted(hits[t]))))
