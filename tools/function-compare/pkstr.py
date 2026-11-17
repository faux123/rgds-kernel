# usage: pkstr.py <disasm file> <stock.elf>  -> prints the format strings passed in x0 to _printk/dev_* calls
import re, sys
elf = open(sys.argv[2], 'rb').read()
BASE, OFF = 0xffffffc008000000, 0x1c0
def cstr(a):
    o = a - BASE + OFF
    e = elf.index(b'\0', o)
    return elf[o:e].decode('latin1')
page = {}
for line in open(sys.argv[1]):
    m = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s+(.*)', line)
    if not m: continue
    addr, op, args = m.groups()
    if op == 'adrp':
        r = re.match(r'(x\d+), 0x([0-9a-f]+)', args); page[r.group(1)] = int(r.group(2), 16)
    elif op == 'add':
        r = re.match(r'(x\d+), (x\d+), #0x([0-9a-f]+)$', args)
        if r and r.group(2) in page: page[r.group(1)] = page[r.group(2)] + int(r.group(3), 16)
    elif op == 'bl':
        reg = 'x0' if 'printk' in args else 'x1'
        a = page.get(reg)
        print(addr, args.split('<')[-1].rstrip('>'), repr(cstr(a)) if a else '?')
