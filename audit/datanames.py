#!/usr/bin/env python3
"""datanames.py <our vmlinux> <map.tsv>: stock.elf's symbol table carries data objects too (typed
't'), so every mapped (stock, ours) data address can be named on both sides. Reports pairs whose
symbol name or offset inside the symbol differ. Static names repeat across files, so a name match
is necessary, not sufficient; the offset must match as well."""
import bisect, collections, subprocess, sys
T = '/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin/'
def syms(elf):
    a, n = [], []
    for l in subprocess.run([T + 'llvm-nm', '-n', elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] not in 'aUwvN' and not p[2].startswith(('$', '.L', '__UNIQUE_ID')):
            a.append(int(p[0], 16)); n.append(p[2])
    return a, n
S, O = syms('/extra2/projects/home/rgds-kernel/stock/stock.elf'), syms(sys.argv[1])
def name(v, s):
    i = bisect.bisect_right(s[0], v) - 1
    return (s[1][i], v - s[0][i]) if i >= 0 else ('?', v)
bad = collections.Counter(); ex = {}
n = 0
for l in open(sys.argv[2]):
    x, y = (int(v, 16) for v in l.split())
    if x < 0xffffffc009570000:
        continue                       # text addresses are covered by the code comparison
    n += 1
    a, b = name(x, S), name(y, O)
    if a != b:
        k = (a[0], b[0])
        bad[k] += 1; ex.setdefault(k, (x, a, y, b))
print('data pairs', n, 'name/offset mismatches', sum(bad.values()), 'distinct', len(bad))
for k, c in bad.most_common():
    x, a, y, b = ex[k]
    print('%4d  stock %x %s+%d | ours %x %s+%d' % (c, x, a[0], a[1], y, b[0], b[1]))
