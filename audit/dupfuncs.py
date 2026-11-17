#!/usr/bin/env python3
"""dupfuncs.py <our vmlinux> <map.tsv> <out.tsv>: compare the functions strict2 skips because
their name is not unique (sensor_init, sensor_active, ...). For each name with the same number of
copies in both kernels, the k-th copy in stock (by address) is paired with the k-th in ours, then
compared exactly as strict2 does: EXACT, ADDR (differences only in adrp-proven global addresses
whose stock->ours pair is in the proven map), else DIFF."""
import bisect, collections, re, subprocess, sys
sys.path.insert(0, '/extra2/projects/home/rgds-kernel/audit')
import strict2 as Z, strict_audit as S
ours, mapf, outp = sys.argv[1:4]
fwd = {}
for l in open(mapf):
    x, y = (int(v, 16) for v in l.split()); fwd[x] = y
def funcs(elf):
    out = subprocess.run([S.T + 'llvm-nm', '-n', elf], capture_output=True, text=True).stdout
    rows = []
    for l in out.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] in 'tTwW' and not p[2].startswith(('$', '.L')):
            rows.append((int(p[0], 16), p[2]))
    by = collections.defaultdict(list)
    for i, (a, n) in enumerate(rows[:-1]):
        b = rows[i + 1][0]
        if b > a:
            by[n].append((a, b))
    return by
SF, OF = funcs(S.STOCK), funcs(ours)
# name an address by symbol+offset, or by the string it points at when it lands in a string pool
sys.argv = [sys.argv[0], ours]
_src = open('/extra2/projects/home/rgds-kernel/audit/objcmp.py').read().split('cnt_s = collections.Counter')[0]
_g = {}
exec(_src, _g)
def same_meaning(x, y):
    return _g['meaning'](x, _g['rs'], _g['sa'], _g['sn']) == _g['meaning'](y, _g['ro'], _g['oa'], _g['on'])
sd, od = S.disasm(S.STOCK), S.disasm(ours)
def sl(d, a, b):
    i = bisect.bisect_left(d[0], a); j = bisect.bisect_left(d[0], b)
    return d[1][i:j], d[0][i:j]
cnt = collections.Counter(); rows = []
for n in sorted(SF):
    if len(SF[n]) < 2 or len(SF[n]) != len(OF.get(n, ())):
        continue
    for k, ((sa, sb), (oa, ob)) in enumerate(zip(SF[n], OF[n])):
        if sb - sa > 1 << 20 or ob - oa > 1 << 20:
            continue
        s, s_a = sl(sd, sa, sb); o, o_a = sl(od, oa, ob)
        es, ts = Z.norm(n, s, sa, sb, s_a, False); eo, to = Z.norm(n, o, oa, ob, o_a, False)
        if es == eo:
            c = 'EXACT'
        else:
            as_, _ = Z.norm(n, s, sa, sb, s_a, True); ao, _ = Z.norm(n, o, oa, ob, o_a, True)
            if as_ == ao and len(ts) == len(to) and all(fwd.get(x) == y or same_meaning(x, y) for x, y in zip(ts, to)):
                c = 'ADDR'
            elif as_ == ao:
                c = 'ADDR_UNNAMED'   # same code; some address pair could not be named the same
            else:
                c = 'DIFF'
        cnt[c] += 1
        if c in ('DIFF', 'ADDR_UNNAMED'):
            i = next((i for i, (x, y) in enumerate(zip(es, eo)) if x != y), min(len(es), len(eo)))
            rows.append(c + '\t%s\t%d\t%x\t%x\t%d\t%d\t%s | %s' % (n, k, sa, oa, sb - sa, ob - oa,
                        ' '.join(es[i]) if i < len(es) else '-', ' '.join(eo[i]) if i < len(eo) else '-'))
skipped = sorted(n for n in SF if len(SF[n]) >= 2 and len(SF[n]) != len(OF.get(n, ())))
print(dict(cnt), 'names with different copy counts (not compared):', len(skipped), skipped[:40])
open(outp, 'w').write('\n'.join(rows) + '\n')
