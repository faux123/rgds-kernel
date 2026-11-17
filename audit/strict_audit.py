#!/usr/bin/env python3
"""Strict whole-kernel function comparison: stock.elf vs one of our vmlinux builds.

Written after the 2026-11-17 Gemini review of allcmp2.py. allcmp2 erased every hex
immediate on loads/stores/adds, so struct-offset and constant differences counted as
"address only". This version keeps every immediate, offset, literal-pool word and nop.
The only things it normalizes are values that depend on where the linker placed code
or data, never on what the source says:

  * adrp page operands                          -> <P>
  * the low-12 immediate of an add/ldr/str whose base register was set by an adrp
    earlier in the same function (and not overwritten since) -> #LO
  * symbolized targets "0x... <sym+off>": inside the same function -> <SELF+off>;
    elsewhere -> <sym> (call or tail-call target by name)
  * adr operands that point outside the function -> <ADR>

Anything else that differs is reported. Function ranges use text symbols only, in
both binaries, so the boundaries are built the same way on each side.

Usage: strict_audit.py <our vmlinux> <out.tsv>
Prints class counts; writes one row per function that is not STRICT_SAME.
ADDR_FLOW: equal only if a register written by any adrp in the function is treated as
an address base everywhere (adrp in one basic block, use in another). Usually a
WARN_ONCE flag or a global reached through a hoisted adrp, but a reused register can
make it hide a struct offset, so these rows are listed for review, not counted as same.
"""
import bisect
import collections
import re
import subprocess
import sys

K = '/extra2/projects/home/rgds-kernel'
T = K + '/toolchain/clang-r487747c/bin/'
STOCK = K + '/stock/stock.elf'

LDST = {'ldr', 'str', 'ldrb', 'strb', 'ldrh', 'strh', 'ldrsw', 'ldrsb', 'ldrsh',
        'ldur', 'stur', 'ldp', 'stp', 'prfm'}
# ops whose first operand is not a destination register
NO_DEST = {'str', 'strb', 'strh', 'stur', 'stp', 'stlr', 'stlrb', 'stlrh', 'stxr',
           'cmp', 'cmn', 'tst', 'ccmp', 'ccmn', 'cbz', 'cbnz', 'tbz', 'tbnz', 'b', 'bl',
           'br', 'blr', 'ret', 'nop', 'prfm', 'msr', 'hint', 'dmb', 'dsb', 'isb'}


def text_ranges(path):
    out = subprocess.run([T + 'llvm-nm', '-n', '--defined-only', path],
                         capture_output=True, text=True).stdout
    rows = []
    for line in out.splitlines():
        p = line.split()
        if len(p) < 3 or p[1] not in 'tTwW':
            continue
        if p[2].startswith('$') or p[2].startswith('.L'):
            continue
        rows.append((int(p[0], 16), p[2]))
    names = collections.Counter(n for a, n in rows)
    r = {}
    for i, (a, n) in enumerate(rows[:-1]):
        b = rows[i + 1][0]
        if names[n] == 1 and b > a:
            r[n] = (a, b)
    return r


def disasm(path):
    p = subprocess.Popen([T + 'llvm-objdump', '-d', '--no-show-raw-insn', path],
                         stdout=subprocess.PIPE, text=True, bufsize=1 << 20)
    addrs, ins = [], []
    rx = re.compile(r'^\s*([0-9a-f]{16}):\s+(\S+)\s*(.*)$')
    for line in p.stdout:
        m = rx.match(line)
        if m:
            addrs.append(int(m.group(1), 16))
            ins.append((m.group(2), m.group(3).strip()))
    return addrs, ins


SYM = re.compile(r'0x[0-9a-f]+ <([^>+]+)(\+0x[0-9a-f]+)?>')
REG = re.compile(r'^\s*([xw]\d+|sp)')


def base_reg(arg):
    m = re.search(r'\[(x\d+|sp)', arg)
    return m.group(1) if m else None


def xreg(r):
    return 'x' + r[1:] if r and r[0] in 'xw' else r


def norm(fname, seq, lo, hi, addrs, flow_insensitive=False):
    out = []
    page = set()  # x-registers currently holding an adrp page
    if flow_insensitive:
        # every register that any adrp in the function writes; used only for the
        # separately reported ADDR_FLOW class, never for STRICT_SAME
        anyp = {xreg(a.split(',')[0].strip()) for o, a in seq if o == 'adrp'}
    for (op, arg), a in zip(seq, addrs):
        def sym(m):
            name, off = m.group(1), m.group(2) or ''
            return '<SELF%s>' % off if name == fname else '<%s>' % name
        arg = SYM.sub(sym, arg)
        if op == 'adrp':
            d = xreg(arg.split(',')[0].strip())
            page.add(d)
            out.append((op, d + ', <P>'))
            continue
        if op == 'adr':
            d, t = [s.strip() for s in arg.split(',', 1)]
            m = re.match(r'#(-?0x[0-9a-f]+|-?\d+)', t)
            if m:
                tgt = a + int(m.group(1), 0)
                arg = d + (', <SELF+%#x>' % (tgt - lo) if lo <= tgt < hi else ', <ADR>')
            out.append((op, arg))
            page.discard(xreg(d))
            continue
        b = base_reg(arg) if op in LDST else None
        if flow_insensitive:
            page |= anyp
        if op in LDST and b in page:
            # the low 12 bits of a paired adrp address; [xN] is the same with lo12 == 0
            arg = re.sub(r'#0x[0-9a-f]+\]', '#LO]', arg)
            arg = re.sub(r'\[(x\d+)\]', r'[\1, #LO]', arg)
        elif op == 'add':
            parts = [s.strip() for s in arg.split(',')]
            if len(parts) == 3 and xreg(parts[1]) in page and parts[2].startswith('#'):
                arg = '%s, %s, #LO' % (parts[0], parts[1])
        out.append((op, arg))
        # destination bookkeeping: a write to a register ends its "page" status
        if op not in NO_DEST:
            m = REG.match(arg)
            if m:
                page.discard(xreg(m.group(1)))
            if op == 'ldp':
                regs = [s.strip() for s in arg.split(',')[:2]]
                for r in regs:
                    page.discard(xreg(r))
        if op in LDST and '!' in arg or (op in LDST and re.search(r'\],\s*#', arg)):
            page.discard(b)  # writeback changes the base
    return out


def first_diff(s, o):
    for i, (x, y) in enumerate(zip(s, o)):
        if x != y:
            return i, x, y
    return min(len(s), len(o)), None, None


def main():
    ours, outp = sys.argv[1], sys.argv[2]
    sr, orr = text_ranges(STOCK), text_ranges(ours)
    sd, od = disasm(STOCK), disasm(ours)

    def sl(d, a, b):
        i = bisect.bisect_left(d[0], a)
        j = bisect.bisect_left(d[0], b)
        return d[1][i:j], d[0][i:j]

    cnt = collections.Counter()
    rows = []
    for n in sorted(sr.keys() & orr.keys()):
        sa, sb = sr[n]
        oa, ob = orr[n]
        if sb - sa > 1 << 20 or ob - oa > 1 << 20:
            continue
        s, s_a = sl(sd, sa, sb)
        o, o_a = sl(od, oa, ob)
        if not s and not o:
            continue
        ns, no = norm(n, s, sa, sb, s_a), norm(n, o, oa, ob, o_a)
        if ns == no:
            c = 'STRICT_SAME'
        elif norm(n, s, sa, sb, s_a, True) == norm(n, o, oa, ob, o_a, True):
            c = 'ADDR_FLOW'
        elif len(s) == len(o):
            c = 'SAMESIZE_DIFF'
        else:
            c = 'DIFFSIZE'
        cnt[c] += 1
        if c != 'STRICT_SAME':
            i, x, y = first_diff(ns, no)
            nd = sum(1 for p, q in zip(ns, no) if p != q)
            rows.append((c, n, sb - sa, ob - oa, nd, i,
                         ' '.join(x) if x else '-', ' '.join(y) if y else '-'))
    print(dict(cnt), 'common', sum(cnt.values()))
    with open(outp, 'w') as f:
        f.write('class\tfunction\tstock_bytes\tour_bytes\tdiff_insns\tfirst_diff_idx\tstock_insn\tour_insn\n')
        for r in rows:
            f.write('\t'.join(str(v) for v in r) + '\n')


if __name__ == '__main__':
    main()
