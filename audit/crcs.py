#!/usr/bin/env python3
"""crcs.py <elf>: print 'name crc' for every exported symbol (6.1 PREL32 ksymtab + kcrctab)."""
import re, struct, subprocess, sys
T = '/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin/'
elf = sys.argv[1]
data = open(elf, 'rb').read()
secs = []
for l in subprocess.run([T + 'llvm-readelf', '-S', '-W', elf], capture_output=True, text=True).stdout.splitlines():
    m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+PROGBITS\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', l)
    if m and int(m.group(2), 16):
        secs.append((int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)))
def rd(a, n):
    for b, o, s in secs:
        if b <= a < b + s:
            return data[o + a - b:o + a - b + n]
    raise KeyError(hex(a))
sym = {}
for l in subprocess.run([T + 'llvm-nm', elf], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3:
        sym[p[2]] = int(p[0], 16)
def cstr(a):
    return rd(a, 256).split(b'\0')[0].decode()
for tab, crc in (('___ksymtab', '___kcrctab'), ('___ksymtab_gpl', '___kcrctab_gpl')):
    s, e, c = sym['__start' + tab], sym['__stop' + tab], sym['__start' + crc]
    for i, a in enumerate(range(s, e, 12)):
        _, noff, _ = struct.unpack('<iii', rd(a, 12))
        name = cstr(a + 4 + noff)
        print(name, '%08x' % struct.unpack('<I', rd(c + 4 * i, 4))[0])
