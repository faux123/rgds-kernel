#!/usr/bin/env python3
"""rodata_check.py <our vmlinux> <map.tsv>: for every mapped (stock, ours) data address that
lies in a file-backed read-only section in both kernels, compare the bytes there: the
NUL-terminated string when the stock byte run looks like text, else 8 bytes. Reports
mismatches. Closes the gap where a matching address could hide changed constant data."""
import re, subprocess, sys
T = '/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin/'
STOCK = '/extra2/projects/home/rgds-kernel/stock/stock.elf'

def sections(elf, readonly):
    out = subprocess.run([T + 'llvm-readelf', '-S', '-W', elf], capture_output=True, text=True).stdout
    secs = []
    for l in out.splitlines():
        m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\S+\s+(\S*)', l)
        if m:
            name, typ, addr, off, size, flags = m.groups()
            ok = typ == 'PROGBITS' and int(addr, 16) and (not readonly or ('W' not in flags or name.startswith('.rodata')) and 'X' not in flags)
            if ok:
                secs.append((int(addr, 16), int(size, 16), int(off, 16), name))
    return secs

def reader(elf, readonly):
    secs = sections(elf, readonly); data = open(elf, 'rb').read()
    def rd(a, n):
        for base, size, off, name in secs:
            if base <= a < base + size:
                return data[off + a - base: off + min(a - base + n, size)], name
        return None, None
    return rd

# stock.elf is one RWX blob, so the read-only test is taken from our build's sections
rs, ro = reader(STOCK, False), reader(sys.argv[1], True)
checked = bad = 0
for line in open(sys.argv[2]):
    x, y = (int(v, 16) for v in line.split())
    bs, ns = rs(x, 256); bo, no = ro(y, 256)
    if bs is None or bo is None:
        continue
    checked += 1
    if bs[:1].isascii() and 32 <= bs[0] < 127 or bs[:1] in (b'\x01',):
        cs = bs.split(b'\0')[0]; co = bo.split(b'\0')[0]
    else:
        cs, co = bs[:8], bo[:8]
    if cs != co:
        bad += 1
        if bad <= 40:
            print('%x %s %r | %x %s %r' % (x, ns, cs[:60], y, no, co[:60]))
print('checked', checked, 'mismatch', bad)

# ---- second pass: the mismatches above are mostly pointer tables. Compare them by meaning:
# a word that points into the kernel image is resolved to symbol+offset (text) or to the string
# it points at (rodata); everything else is compared raw.
import bisect
def syms(elf):
    out = subprocess.run([T + 'llvm-nm', '-n', elf], capture_output=True, text=True).stdout
    a, n = [], []
    for l in out.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] in 'tTdDrRbB':
            a.append(int(p[0], 16)); n.append(p[2])
    return a, n
SS, SO = syms(STOCK), syms(sys.argv[1])
def meaning(v, rd, sy):
    if not (0xffffffc008000000 <= v < 0xffffffc00c000000):
        return ('raw', v)
    s, _ = rd(v, 200)
    if s and 32 <= s[0] < 127 and b'\0' in s[:200]:
        t = s.split(b'\0')[0]
        if len(t) >= 3 and all(32 <= c < 127 or c in (9, 10) for c in t):
            return ('str', t)
    i = bisect.bisect_right(sy[0], v) - 1
    if i >= 0 and sy[1][i] in ('linux_banner', 'kallsyms_token_index', 'task_index_to_char.state_char',
                               'f_midi_shortname', 'f_midi_longname', 'max_tt_usecs') or (s and v - sy[0][i] > 4096):
        # rodata string pool: no symbols, so name the pointer by the bytes it points at
        return ('bytes', (s or b'')[:24].split(b'\0')[0] + b'\0')
    return ('sym', sy[1][i], v - sy[0][i]) if i >= 0 else ('raw', v)
SZ = {}
for l in subprocess.run([T + 'llvm-nm', '-S', '-n', sys.argv[1]], capture_output=True, text=True).stdout.splitlines():
    q = l.split()
    if len(q) == 4:
        SZ[int(q[0], 16)] = int(q[1], 16)
SZK = sorted(SZ)
def room(y):
    # bytes from y to the end of the object that contains it in our build (0 if unknown)
    i = bisect.bisect_right(SZK, y) - 1
    return SZK[i] + SZ[SZK[i]] - y if i >= 0 and SZK[i] + SZ[SZK[i]] > y else 0
bad2 = words = 0
seen = set()
for line in open(sys.argv[2]):
    x, y = (int(v, 16) for v in line.split())
    bs, _ = rs(x, 64); bo, _ = ro(y, 64)
    if bs is None or bo is None or x in seen:
        continue
    seen.add(x)
    head = bs.split(b'\0')[0]
    if len(head) >= 4 and all(32 <= c < 127 or c in (9, 10) for c in head):
        cs = head; co = bo.split(b'\0')[0]
        if cs != co:
            bad2 += 1; print('STR', hex(x), cs[:50], co[:50])
        continue
    if x % 8 != y % 8:
        continue
    a0 = (8 - x % 8) % 8   # compare whole aligned words only; a window straddling a pointer is noise
    for k in range(a0, min(len(bs), len(bo), 24, room(y)) - 7, 8):
        ws = int.from_bytes(bs[k:k+8], 'little'); wo = int.from_bytes(bo[k:k+8], 'little')
        words += 1
        ms, mo = meaning(ws, rs, SS), meaning(wo, ro, SO)
        if ms != mo:
            bad2 += 1
            if bad2 <= 30:
                print('WORD %x+%d %s | %s' % (x, k, str(ms)[:70], str(mo)[:70]))
print('pass2 words', words, 'mismatch', bad2)
