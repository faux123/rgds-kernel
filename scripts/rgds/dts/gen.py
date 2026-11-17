#!/usr/bin/env python3
"""Generate a board dts = includes + overrides that turn BASE dtb into STOCK dtb (semantically)."""
import re, struct, sys
sys.path.insert(0, sys.argv[0].rsplit('/', 1)[0])
from normlib import parse, render_tokens, cells, phmap, SPEC

base_dtb, stock_dtb, header = sys.argv[1], sys.argv[2], sys.argv[3]
B, St = parse(base_dtb), parse(stock_dtb)
def syms(n):
    m = {}
    for l, v in n.get('/__symbols__', {}).items(): m.setdefault(v[:-1].decode(), []).append(l)
    return m
sl, bl = syms(St), syms(B)
SKIP = ('/__symbols__', '/__fixups__', '/__local_fixups__')
def skip(p): return any(p == s or p.startswith(s + '/') for s in SKIP)
def label_ref(path):
    if path == '/': return '/'
    ls = bl.get(path)
    return '&' + ls[0] if ls else '&{%s}' % path
PH = {}
def u32(tree, p, name, d=None):
    return struct.unpack('>I', tree[p][name])[0] if name in tree[p] else d
def pin(x): return 'RK_P%s%d' % ('ABCD'[x // 8], x % 8) if x < 32 else str(x)
IRQT = {0: 'IRQ_TYPE_NONE', 1: 'IRQ_TYPE_EDGE_RISING', 2: 'IRQ_TYPE_EDGE_FALLING', 3: 'IRQ_TYPE_EDGE_BOTH', 4: 'IRQ_TYPE_LEVEL_HIGH', 8: 'IRQ_TYPE_LEVEL_LOW'}
HEXPROPS = re.compile(r'^(reg|ranges|dma-ranges|.*mask.*|interrupt-map.*|msi-map|.*_params|bus-range|.*-addr.*|rockchip,.*-reg.*|.*-hw|linux,code)$')
def iparent(tree, p):
    q = p
    while True:
        if 'interrupt-parent' in tree[q]:
            return PH[id(tree)].get(u32(tree, q, 'interrupt-parent'))
        if q == '/': return None
        q = q.rsplit('/', 1)[0] or '/'
def pretty(tree, path, k, v):
    """Return a macro-based rendering, or None to fall back to the generic one."""
    ph = PH.setdefault(id(tree), phmap(tree)); c = cells(v)
    if not c: return None
    lbl = lambda x: '&' + sl[ph[x]][0] if ph.get(x) in sl else None
    isgpio = (k.endswith('gpios') or k.endswith('gpio') or SPEC.get(k) == '#gpio-cells')
    if isgpio and len(c) % 3 == 0:
        out = []
        for i in range(0, len(c), 3):
            h, p_, f = c[i:i+3]; l = lbl(h)
            if not l or 'gpio-controller' not in tree[ph[h]] or u32(tree, ph[h], '#gpio-cells') != 2 or f > 1: return None
            out.append('%s %s %s' % (l, pin(p_), 'GPIO_ACTIVE_LOW' if f else 'GPIO_ACTIVE_HIGH'))
        return '<' + '>, <'.join(out) + '>'
    if k == 'interrupts':
        ip = iparent(tree, path)
        if ip is None: return None
        n = u32(tree, ip, '#interrupt-cells')
        if n == 3 and len(c) % 3 == 0 and all(c[i] in (0, 1) and c[i+2] in IRQT for i in range(0, len(c), 3)):
            return '<' + '>, <'.join('%s %d %s' % (('GIC_SPI', 'GIC_PPI')[c[i]], c[i+1], IRQT[c[i+2]]) for i in range(0, len(c), 3)) + '>'
        if n == 2 and 'gpio-controller' in tree[ip] and len(c) == 2 and c[1] in IRQT:
            return '<%s %s>' % (pin(c[0]), IRQT[c[1]])
        return None
    if k == 'rockchip,pins' and len(c) % 4 == 0:
        out = []
        for i in range(0, len(c), 4):
            b, p_, f, h = c[i:i+4]; l = lbl(h)
            if not l: return None
            out.append('%d %s %s %s' % (b, pin(p_), 'RK_FUNC_GPIO' if f == 0 else str(f), l))
        return '\n'.join('<%s>%s' % (o, ',' if j < len(out) - 1 else '') for j, o in enumerate(out))
    return None
def decimalize(k, s):
    if HEXPROPS.match(k): return s
    return re.sub(r'(?<![\w&/{])0x([0-9a-f]+)\b', lambda m: str(int(m.group(1), 16)), s) if s.startswith('<') else s
def val(path, k, v, tree):
    p = pretty(tree, path, k, v)
    if p is not None: return p
    toks = decimalize(k, render_tokens(tree, path, k, v))
    # references: prefer stock label
    return re.sub(r'&(/[^\s>]*)', lambda m: ('&' + sl[m.group(1)][0]) if m.group(1) in sl else '&{%s}' % m.group(1), toks)
def prop(path, k, v, tree, ind):
    if k in ('phandle', 'linux,phandle'): return None
    if not v: return '%s%s;' % (ind, k)
    s = '%s%s = ' % (ind, k)
    return s + val(path, k, v, tree).replace('\n', '\n' + ind + '\t\t') + ';'
def children(tree, p):
    pre = p.rstrip('/') + '/'
    return [q for q in tree if q.startswith(pre) and '/' not in q[len(pre):] and q != p]
def emit_new(p, ind):
    name = p.rsplit('/', 1)[1]
    lab = ''.join(l + ': ' for l in sl.get(p, []))
    out = ['%s%s%s {' % (ind, lab, name)]
    for k, v in St[p].items():
        s = prop(p, k, v, St, ind + '\t')
        if s: out.append(s)
    for c in children(St, p):
        if not skip(c): out += emit_new(c, ind + '\t')
    out.append(ind + '};')
    return out
# Each change lands in the override block of the nearest labeled ancestor
# (or the root block); unlabeled nodes on the way are nested by name.
class Blk:
    def __init__(self): self.labels, self.props, self.kids, self.new = [], [], {}, []
blocks = {}  # anchor path -> Blk
def anchor(p):
    q = p
    while q != '/' and not bl.get(q):
        q = q.rsplit('/', 1)[0] or '/'
    return q
def blk_for(p):
    a = anchor(p); b = blocks.setdefault(a, Blk())
    rel = p[len(a):].strip('/') if a != '/' else p.strip('/')
    for seg in [s for s in rel.split('/') if s]:
        b = b.kids.setdefault(seg, Blk())
    return b
dels = []
for p in B:
    if skip(p) or p in St: continue
    par = p.rsplit('/', 1)[0] or '/'
    if par in B and par not in St: continue
    dels.append('/delete-node/ %s;' % label_ref(p))
for p in St:
    if skip(p): continue
    if p not in B:
        par = p.rsplit('/', 1)[0] or '/'
        if par in B: blk_for(par).new.append(p)
        continue
    body = []
    for k, v in St[p].items():
        if k in ('phandle', 'linux,phandle'): continue
        if B[p].get(k) != v or val(p, k, v, St) != val(p, k, B[p][k], B):
            body.append((k, v))
    gone = [k for k in B[p] if k not in St[p] and k not in ('phandle', 'linux,phandle')]
    newl = [l for l in sl.get(p, []) if l not in bl.get(p, [])]
    if body or gone or newl:
        b = blk_for(p); b.labels += newl
        b.props += [prop(p, k, v, St, '') for k, v in body] + ['/delete-property/ %s;' % k for k in gone]
        b.path = p
def render(b, ind):
    lines = [ind + s.replace('\n', '\n' + ind) for s in b.props]
    for p in b.new:
        lines += (lines and lines[-1] != ind + '' and [''] or []) + emit_new(p, ind)
    for name, kb in b.kids.items():
        lab = ''.join(l + ': ' for l in kb.labels)
        if lines: lines.append('')
        lines += ['%s%s%s {' % (ind, lab, name)] + render(kb, ind + '\t') + [ind + '};']
    return lines
out = [header, '']
if '/' in blocks:
    out += ['/ {'] + render(blocks.pop('/'), '\t') + ['};', '']
for a in sorted(blocks, key=lambda a: bl[a][0]):
    b = blocks[a]
    lab = ''.join(l + ': ' for l in b.labels)
    out += ['%s&%s {' % (lab, bl[a][0])] + render(b, '\t') + ['};', '']
print('\n'.join(out + dels).rstrip('\n'))
