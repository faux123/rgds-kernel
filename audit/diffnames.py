#!/usr/bin/env python3
"""diffnames.py <our vmlinux> <s2 tsv>: for each DIFF function, take every differing instruction
pair that differs only in an immediate, resolve the base register's possible adrp pages on both
sides (may-analysis), and name the resulting addresses by symbol+offset. A function whose every
differing pair names the same object at the same offset differs only in where that global sits."""
import sys, re, subprocess, bisect
sys.path.insert(0, '/extra2/projects/home/rgds-kernel/audit')
import strict2 as Z, strict_audit as S

T = S.T
def syms(elf):
    a, n = [], []
    for l in subprocess.run([T + 'llvm-nm', '-n', elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] not in 'aUwvN' and not p[2].startswith(('$', '.L', '__UNIQUE_ID')):
            a.append(int(p[0], 16)); n.append(p[2])
    return a, n
def name(v, s):
    i = bisect.bisect_right(s[0], v) - 1
    return (s[1][i], v - s[0][i]) if i >= 0 else ('?', v)
ours = sys.argv[1]
SS, OS = syms(S.STOCK), syms(ours)
fns = [l.split('\t')[1] for l in open(sys.argv[2]).read().splitlines()[1:] if l.startswith('DIFF')]
_R = {}
def get(elf, fn):
    if elf not in _R: _R[elf] = S.text_ranges(elf)
    a, b = _R[elf][fn]
    out = subprocess.run([T + 'llvm-objdump', '-d', '--no-show-raw-insn', '--start-address=%#x' % a, '--stop-address=%#x' % b, elf], capture_output=True, text=True).stdout
    seq, ad = [], []
    for l in out.splitlines():
        m = re.match(r'^\s*([0-9a-f]{16}):\s+(\S+)\s*(.*)$', l)
        if m: ad.append(int(m.group(1), 16)); seq.append((m.group(2), m.group(3).strip()))
    return seq, ad
def addrs(ins, pg):
    op, arg = ins
    m = re.search(r'\[(x\d+)(?:, #(0x[0-9a-f]+))?\]', arg)
    if op in S.LDST and m and m.group(1) in pg:
        return {v + int(m.group(2) or '0', 16) for v in pg[m.group(1)] if v is not None}
    p = [t.strip() for t in arg.split(',')]
    if op == 'add' and len(p) == 3 and S.xreg(p[1]) in pg and p[2].startswith('#'):
        return {v + int(p[2][1:], 0) for v in pg[S.xreg(p[1])] if v is not None}
    return set()
for fn in fns:
    try:
        s, sa = get(S.STOCK, fn); o, oa = get(ours, fn)
    except KeyError:
        continue
    if len(s) != len(o):
        print('%-34s length %d/%d' % (fn, len(s), len(o))); continue
    ps, po = Z.may_pages(fn, s, sa), Z.may_pages(fn, o, oa)
    same = other = 0; note = ''
    for k, (x, y) in enumerate(zip(s, o)):
        if x == y or x[0] == 'adrp':
            continue
        rx = re.sub(r'#0x[0-9a-f]+|0x[0-9a-f]+ <[^>]*>', 'I', x[1]); ry = re.sub(r'#0x[0-9a-f]+|0x[0-9a-f]+ <[^>]*>', 'I', y[1])
        if x[0] != y[0] or rx != ry:
            other += 1; note = note or '%s %s | %s %s' % (x[0], x[1][:40], y[0], y[1][:40]); continue
        A, B = addrs(x, ps[k]), addrs(y, po[k])
        if not A and not B:
            if re.search(r'<', x[1]):
                continue          # branch/call target relabel
            other += 1; note = note or 'imm %s | %s' % (x[1][:40], y[1][:40]); continue
        na = {name(v, SS) for v in A}; nb = {name(v, OS) for v in B}
        if na & nb:
            same += 1
        else:
            other += 1; note = note or 'names %s | %s' % (sorted(na)[:2], sorted(nb)[:2])
    print('%-34s same-object %3d  other %3d  %s' % (fn, same, other, note[:90]))
