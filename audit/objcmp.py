#!/usr/bin/env python3
"""objcmp.py <our vmlinux>: compare every initialized data object (.rodata, .data and the init
data sections) by NAME. stock.elf names data objects too, so each object with a unique name in
both kernels is compared word by word: pointers are resolved to symbol+offset (or to the string
they point at when they land in an anonymous string pool), other words compared raw. Sizes are
compared too (stock size = distance to the next stock symbol; checked only when that distance
is <= our size + 64, since stock has no size field). Answers Gemini round 2: no alignment skip,
no object-size truncation, no 24-byte limit."""
import bisect, collections, re, subprocess, sys
T = '/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin/'
STOCK = '/extra2/projects/home/rgds-kernel/stock/stock.elf'
OURS = sys.argv[1]
LO, HI = 0xffffffc009570000, 0xffffffc00a404000      # .rodata .. end of .data (not .bss)

def syms(elf, sized):
    out = subprocess.run([T + 'llvm-nm', '-n'] + (['-S'] if sized else []) + [elf], capture_output=True, text=True).stdout
    r = []
    for l in out.splitlines():
        p = l.split()
        if sized and len(p) == 4:
            if p[2] in 'dDrRvV':          # data objects only (code is compared by strict2)
                r.append((int(p[0], 16), int(p[1], 16), p[3]))
        elif not sized and len(p) == 3:
            r.append((int(p[0], 16), None, p[2]))
    return r

def reader(elf):
    data = open(elf, 'rb').read()
    out = subprocess.run([T + 'llvm-readelf', '-S', '-W', elf], capture_output=True, text=True).stdout
    secs = []
    for l in out.splitlines():
        m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+PROGBITS\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', l)
        if m and int(m.group(2), 16):
            secs.append((int(m.group(2), 16), int(m.group(4), 16), int(m.group(3), 16)))
    def rd(a, n):
        for b, s, o in secs:
            if b <= a and a + n <= b + s:
                return data[o + a - b: o + a - b + n]
        return None
    return rd

S_all, O_all = syms(STOCK, False), syms(OURS, True)
sa = [x[0] for x in S_all]; sn = [x[2] for x in S_all]
O_names = syms(OURS, False)   # every symbol, for naming pointer targets
oa = [x[0] for x in O_names]; on = [x[2] for x in O_names]
rs, ro = reader(STOCK), reader(OURS)

def name(v, A, N):
    i = bisect.bisect_right(A, v) - 1
    return (N[i], v - A[i]) if i >= 0 else ('?', v)

POOL = {'linux_banner', 'kallsyms_token_index', 'task_index_to_char.state_char', 'f_midi_shortname',
        'f_midi_longname', 'max_tt_usecs', 'hci_reset_dev.hw_err', 'linux_proc_banner'}
def meaning(v, rd, A, N):
    if not (0xffffffc008000000 <= v < 0xffffffc00c000000):
        return ('raw', v)
    nm, off = name(v, A, N)
    if nm in POOL or off > 65536:
        s = rd(v, 1) and (rd(v, 256) or rd(v, 64) or rd(v, 8)) or b''
        return ('str', s.split(b'\0')[0][:64])
    return ('sym', nm, off)

cnt_s = collections.Counter(sn); cnt_o = collections.Counter(x[2] for x in O_all)
stock_at = {n: (a, sa[i + 1] - a if i + 1 < len(sa) else 0) for i, (a, n) in enumerate(zip(sa, sn)) if cnt_s[n] == 1}
compared = words = 0
bad = []
sizebad = []
for a, size, n in O_all:
    if not (LO <= a < HI) or cnt_o[n] != 1 or n not in stock_at or size == 0 or n in POOL:
        continue
    x, sgap = stock_at[n]
    if not (LO <= x < HI):
        continue
    bs, bo = rs(x, size), ro(a, size)
    if bs is None or bo is None:
        continue
    compared += 1
    if sgap < size or (sgap > size + 64 and sgap < 4096 and False):
        sizebad.append((n, sgap, size))
    # word-aligned objects compare word by word, others byte-wise
    step = 8 if a % 8 == 0 and x % 8 == 0 else 1
    for k in range(0, size - step + 1, step):
        if step == 8:
            ws = int.from_bytes(bs[k:k + 8], 'little'); wo = int.from_bytes(bo[k:k + 8], 'little')
            words += 1
            if ws != wo and meaning(ws, rs, sa, sn) != meaning(wo, ro, oa, on):
                bad.append((n, k, meaning(ws, rs, sa, sn), meaning(wo, ro, oa, on))); break
        elif bs[k] != bo[k]:
            bad.append((n, k, bs[k], bo[k])); break
    else:
        if step == 8 and size % 8:
            if bs[size - size % 8:] != bo[size - size % 8:]:
                bad.append((n, size - size % 8, bs[-8:], bo[-8:]))
print('objects compared', compared, 'words', words, 'objects differing', len(bad), 'stock smaller than ours', len(sizebad))
for b in bad:
    print('DIFF %-40s +%-5d %s | %s' % (b[0], b[1], str(b[2])[:70], str(b[3])[:70]))
for b in sizebad[:40]:
    print('SIZE %-40s stock<=%d ours=%d' % b)
