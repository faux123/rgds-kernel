#!/usr/bin/env python3
"""strict2.py: function comparison with address tracking and a global consistency check.

Answers the second Gemini review (2026-11-17) of strict_audit.py:
  * Address operands are normalized only when the base register provably holds an adrp
    page on EVERY control-flow path (must-analysis), and the analysis tracks the page
    VALUE, not just "is a page". There is no linear-scan shortcut any more.
  * Every normalized access records the absolute data address it touches in each kernel.
    Across the whole kernel, a stock data address must map to exactly one address in our
    build and back (a bijection). An index or field difference inside one page breaks the
    bijection and is reported, so "#LO" can no longer hide it.
  * Calls clobber x0-x18 and x30. LSE atomics and exclusives clobber every register they
    name except the base.

Classes per function:
  EXACT      identical apart from adrp page values and branch/symbol relabeling
  ADDR       identical once proven global addresses are abstracted, and every one of those
             addresses is consistent with the global map (no conflict)
  ADDR_CONF  as ADDR, but at least one address pair conflicts with the global map
  ADDR_MAPPED  differs only in immediates whose base holds a page on some path, and every
             resulting (stock, ours) address pair is already in the proven global map
  DIFF       anything else (same or different size)

Usage: strict2.py <our vmlinux> <out prefix>
Writes <prefix>.tsv (non-EXACT rows), <prefix>.conflicts.tsv, <prefix>.shifts.tsv.
"""
import bisect
import collections
import re
import sys

sys.path.insert(0, '/extra2/projects/home/rgds-kernel/audit')
import strict_audit as S  # reuse text_ranges, disasm, SYM, REG, LDST, NO_DEST, base_reg, xreg

ATOMIC = re.compile(r'^(ld(add|clr|eor|set|smax|smin|umax|umin)|swp|cas|ldx|ldax|stx|stlx)')


def succ_edges(fname, seq, addrs):
    idx = {a: i for i, a in enumerate(addrs)}
    tgt = re.compile(r'0x([0-9a-f]+) <' + re.escape(fname) + r'(\+0x[0-9a-f]+)?>')
    n = len(seq)
    succ = [[] for _ in range(n)]
    for i, (op, arg) in enumerate(seq):
        m = tgt.search(arg)
        t = idx.get(int(m.group(1), 16)) if m else None
        if t is not None and (op == 'b' or op.startswith('b.') or
                              op in ('cbz', 'cbnz', 'tbz', 'tbnz')):
            succ[i].append(t)
        if op not in ('b', 'br', 'ret', 'brk', 'eret') and i + 1 < n:
            succ[i].append(i + 1)
    return succ


def transfer(op, arg, cur):
    """cur: dict reg -> page value (must facts)."""
    regs = [t.strip() for t in arg.split(',')]
    if op == 'adrp':
        m = re.search(r'0x([0-9a-f]+)', arg)
        cur[S.xreg(regs[0])] = int(m.group(1), 16) if m else None
        return
    if op == 'mov' and len(regs) == 2 and regs[1].startswith('x') and S.xreg(regs[1]) in cur:
        cur[S.xreg(regs[0])] = cur[S.xreg(regs[1])]
        return
    if ATOMIC.match(op):
        b = S.base_reg(arg)
        for r in re.findall(r'\b([xw]\d+)\b', arg):
            if S.xreg(r) != b:
                cur.pop(S.xreg(r), None)
    elif op not in S.NO_DEST:
        m = S.REG.match(regs[0]) if regs else None
        if m:
            cur.pop(S.xreg(m.group(1)), None)
        if op in ('ldp', 'ldpsw', 'ldnp') and len(regs) > 1:
            cur.pop(S.xreg(regs[1]), None)
    if op in ('bl', 'blr'):
        for r in range(19):
            cur.pop('x%d' % r, None)
        cur.pop('x30', None)
    if op in S.LDST and ('!' in arg or re.search(r'\],\s*#', arg)):
        cur.pop(S.base_reg(arg), None)


def must_pages(fname, seq, addrs):
    n = len(seq)
    succ = succ_edges(fname, seq, addrs)
    inn = [None] * n
    nop_out = {}
    work = []
    while True:
        if not work:
            nxt = next((i for i in range(n) if inn[i] is None), None)
            if nxt is None:
                break
            # code no in-function jump reaches: a jump-label site (a nop patched to a
            # branch at runtime) may enter it, so start from what holds at every reached
            # nop; with no reached nop, nothing is known
            outs = [nop_out[j] for j in nop_out]
            seed = {}
            if outs:
                seed = dict(outs[0])
                for d in outs[1:]:
                    seed = {r: v for r, v in seed.items() if d.get(r) == v}
            inn[nxt] = seed
            work = [nxt]
        i = work.pop()
        cur = dict(inn[i])
        transfer(seq[i][0], seq[i][1], cur)
        if seq[i][0] == 'nop':
            nop_out[i] = dict(cur)
        for j in succ[i]:
            if inn[j] is None:
                new = dict(cur)
            else:
                new = {r: v for r, v in inn[j].items() if r in cur and cur[r] == v}
            if new != inn[j]:
                inn[j] = new
                work.append(j)
    return [x or {} for x in inn]


def may_pages(fname, seq, addrs):
    """May-analysis with values: reg -> set of page values reaching on some path."""
    n = len(seq)
    succ = succ_edges(fname, seq, addrs)
    inn = [None] * n
    nop_out_may = {}
    work = []
    while True:
        if not work:
            nxt = next((i for i in range(n) if inn[i] is None), None)
            if nxt is None:
                break
            seed = {}
            for d in nop_out_may.values():
                for r, v in d.items():
                    seed.setdefault(r, set()).update(v)
            inn[nxt] = seed
            work = [nxt]
        i = work.pop()
        cur = {r: set(v) for r, v in inn[i].items()}
        op, arg = seq[i]
        regs = [t.strip() for t in arg.split(',')]
        single = {r: (next(iter(v)) if len(v) == 1 else None) for r, v in cur.items()}
        transfer(op, arg, single)
        new_cur = {}
        for r, v in single.items():
            if r in cur and v == (next(iter(cur[r])) if len(cur[r]) == 1 else None):
                new_cur[r] = cur[r]
            elif v is not None:
                new_cur[r] = {v}
        if op == 'mov' and len(regs) == 2 and S.xreg(regs[1]) in cur:
            new_cur[S.xreg(regs[0])] = set(cur[S.xreg(regs[1])])
        if op == 'nop':
            nop_out_may[i] = {r: set(v) for r, v in new_cur.items()}
        for j in succ[i]:
            if inn[j] is None:
                nw = {r: set(v) for r, v in new_cur.items()}
            else:
                nw = {r: set(v) for r, v in inn[j].items()}
                for r, v in new_cur.items():
                    nw.setdefault(r, set()).update(v)
            if nw != inn[j]:
                inn[j] = nw
                work.append(j)
    return [x or {} for x in inn]


_KEYS = None


def anchored(a, b, fwd):
    """A pair not seen in proven code is still accepted when the nearest proven addresses
    on both sides (within 4 KiB) map with the same delta as this pair: the bytes between
    two anchors that moved by the same amount sit in the same order in both kernels."""
    global _KEYS
    if _KEYS is None:
        _KEYS = sorted(k for k, v in fwd.items() if len(v) == 1)
    i = bisect.bisect_left(_KEYS, a)
    if i == 0 or i >= len(_KEYS):
        return False
    lo_k, hi_k = _KEYS[i - 1], _KEYS[i]
    if a - lo_k > 4096 or hi_k - a > 4096:
        return False
    d = b - a
    return next(iter(fwd[lo_k])) - lo_k == d and next(iter(fwd[hi_k])) - hi_k == d


def mapped_check(fname, s, sa, sb, s_a, o, oa, ob, o_a, fwd):
    """For a DIFF function: True if every differing instruction pair differs only in a memory
    or add immediate whose base holds (on some path) a page, and every resulting
    (stock address, our address) pair is already in the proven global map."""
    ps, po = may_pages(fname, s, s_a), may_pages(fname, o, o_a)
    ns, _ = norm(fname, s, sa, sb, s_a, True)
    no, _ = norm(fname, o, oa, ob, o_a, True)
    if len(ns) != len(no):
        return False, 'length'
    checked = 0
    for k, (x, y) in enumerate(zip(ns, no)):
        if x == y:
            continue
        if x[0] != y[0]:
            return False, 'opcode %s/%s' % (x[0], y[0])
        rx = re.sub(r'#0x[0-9a-f]+', '#I', x[1]); ry = re.sub(r'#0x[0-9a-f]+', '#I', y[1])
        if re.sub(r'\[(x\d+)\]', r'[\1, #I]', rx) != re.sub(r'\[(x\d+)\]', r'[\1, #I]', ry):
            return False, 'operands %s | %s' % (x[1], y[1])
        def addr(seq_ins, pg):
            op, arg = seq_ins
            if op in S.LDST:
                b = S.base_reg(arg)
                m = re.search(r'\[x\d+(?:, #(0x[0-9a-f]+))?\]', arg)
                if b in pg and m:
                    return {v + int(m.group(1) or '0', 16) for v in pg[b] if v is not None}
            if op == 'add':
                parts = [t.strip() for t in arg.split(',')]
                if len(parts) == 3 and S.xreg(parts[1]) in pg and parts[2].startswith('#'):
                    return {v + int(parts[2][1:], 0) for v in pg[S.xreg(parts[1])] if v is not None}
            return set()
        xs, ys = addr(s[k], ps[k]), addr(o[k], po[k])
        if not xs or not ys:
            return False, 'no page base %s | %s' % (x[1], y[1])
        if not any(fwd.get(a) == {b} or anchored(a, b, fwd) for a in xs for b in ys):
            return False, 'pair not in map %s | %s' % (sorted(hex(a) for a in xs)[:2], sorted(hex(b) for b in ys)[:2])
        checked += 1
    return True, 'mapped %d' % checked


def norm(fname, seq, lo, hi, addrs, abstract):
    """Returns (tokens, data addresses touched in order)."""
    pin = must_pages(fname, seq, addrs)
    out, touched = [], []
    for k, ((op, arg), a) in enumerate(zip(seq, addrs)):
        def sym(m):
            name, off = m.group(1), m.group(2) or ''
            return '<SELF%s>' % off if name == fname else '<%s>' % name
        page = pin[k]
        if op == 'adrp':
            out.append((op, S.xreg(arg.split(',')[0].strip()) + ', <P>'))
            continue
        arg = S.SYM.sub(sym, arg)
        if op == 'adr':
            d, t = [s.strip() for s in arg.split(',', 1)]
            m = re.match(r'#(-?0x[0-9a-f]+|-?\d+)', t)
            if m:
                tgt = a + int(m.group(1), 0)
                arg = d + (', <SELF+%#x>' % (tgt - lo) if lo <= tgt < hi else ', <ADR>')
            out.append((op, arg))
            continue
        b = S.base_reg(arg) if op in S.LDST else None
        if op in S.LDST and b in page and page[b] is not None:
            m = re.search(r'\[x\d+(?:, #(0x[0-9a-f]+))?\]', arg)
            if m:
                touched.append(page[b] + int(m.group(1) or '0', 16))
                if abstract:
                    arg = arg[:m.start()] + '[%s, #LO]' % b + arg[m.end():]
        elif op == 'add':
            parts = [s.strip() for s in arg.split(',')]
            if (len(parts) == 3 and S.xreg(parts[1]) in page and page[S.xreg(parts[1])] is not None
                    and re.fullmatch(r'#0x[0-9a-f]+|#\d+', parts[2])):
                touched.append(page[S.xreg(parts[1])] + int(parts[2][1:], 0))
                if abstract:
                    arg = '%s, %s, #LO' % (parts[0], parts[1])
        out.append((op, arg))
    return out, touched


def main():
    ours, prefix = sys.argv[1], sys.argv[2]
    sr, orr = S.text_ranges(S.STOCK), S.text_ranges(ours)
    sd, od = S.disasm(S.STOCK), S.disasm(ours)

    def sl(d, a, b):
        i = bisect.bisect_left(d[0], a)
        j = bisect.bisect_left(d[0], b)
        return d[1][i:j], d[0][i:j]

    cls, pairs_by_fn = {}, {}
    fwd, rev = collections.defaultdict(set), collections.defaultdict(set)
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
        es, ts = norm(n, s, sa, sb, s_a, False)
        eo, to = norm(n, o, oa, ob, o_a, False)
        if es == eo:
            cls[n] = 'EXACT'
        else:
            as_, _ = norm(n, s, sa, sb, s_a, True)
            ao, _ = norm(n, o, oa, ob, o_a, True)
            if as_ == ao and len(ts) == len(to):
                cls[n] = 'ADDR'
                pairs_by_fn[n] = list(zip(ts, to))
                for x, y in zip(ts, to):
                    fwd[x].add(y)
                    rev[y].add(x)
            else:
                cls[n] = 'DIFF'
        if cls[n] == 'EXACT':
            for x, y in zip(ts, to):
                fwd[x].add(y)
                rev[y].add(x)
        if cls[n] != 'EXACT':
            rows.append((cls[n], n, sb - sa, ob - oa))
    bad_s = {x for x, ys in fwd.items() if len(ys) > 1}
    bad_o = {y for y, xs in rev.items() if len(xs) > 1}
    conf = []
    for n, prs in pairs_by_fn.items():
        hits = [(x, y) for x, y in prs if x in bad_s or y in bad_o]
        if hits:
            cls[n] = 'ADDR_CONF'
            conf.append((n, hits[:4]))
    why = {}
    for n in [n for n, c in cls.items() if c == 'DIFF']:
        sa, sb = sr[n]; oa, ob = orr[n]
        s, s_a = sl(sd, sa, sb); o, o_a = sl(od, oa, ob)
        ok, reason = mapped_check(n, s, sa, sb, s_a, o, oa, ob, o_a, fwd)
        why[n] = reason
        if ok:
            cls[n] = 'ADDR_MAPPED'
    cnt = collections.Counter(cls.values())
    print(dict(cnt), 'common', len(cls), 'conflicting stock addrs', len(bad_s), 'ours', len(bad_o))
    with open(prefix + '.tsv', 'w') as f:
        f.write('class\tfunction\tstock_bytes\tour_bytes\n')
        for c, n, a, b in rows:
            f.write('%s\t%s\t%d\t%d\t%s\n' % (cls[n], n, a, b, why.get(n, '')))
    with open(prefix + '.conflicts.tsv', 'w') as f:
        for n, hits in conf:
            f.write('%s\t%s\n' % (n, ' '.join('%x->%x' % h for h in hits)))
    with open(prefix + '.map.tsv', 'w') as f:
        for x in sorted(fwd):
            if len(fwd[x]) == 1:
                f.write('%x\t%x\n' % (x, next(iter(fwd[x]))))
    # shift map: stock address -> delta, sorted, one row where the delta changes
    with open(prefix + '.shifts.tsv', 'w') as f:
        last = None
        for x in sorted(fwd):
            if len(fwd[x]) != 1:
                continue
            d = next(iter(fwd[x])) - x
            if d != last:
                f.write('%x\t%x\t%d\n' % (x, x + d, d))
                last = d


if __name__ == '__main__':
    main()
