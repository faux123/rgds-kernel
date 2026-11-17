#!/usr/bin/env python3
"""Generate a board dts = includes + overrides that turn BASE dtb into STOCK dtb (semantically)."""
import re, struct, sys
sys.path.insert(0, sys.argv[0].rsplit('/', 1)[0])
from normlib import parse, render_tokens

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
def val(path, k, v, tree):
    toks = render_tokens(tree, path, k, v)
    # references: prefer stock label
    return re.sub(r'&(/[^\s>]*)', lambda m: ('&' + sl[m.group(1)][0]) if m.group(1) in sl else '&{%s}' % m.group(1), toks)
def prop(path, k, v, tree, ind):
    if k in ('phandle', 'linux,phandle'): return None
    return '%s%s = %s;' % (ind, k, val(path, k, v, tree)) if v else '%s%s;' % (ind, k)
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
out = [header, '']
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
        if par in B:
            out.append('%s {' % label_ref(par)) if par != '/' else out.append('/ {')
            out += emit_new(p, '\t'); out.append('};'); out.append('')
        continue
    body = []
    for k, v in St[p].items():
        if k in ('phandle', 'linux,phandle'): continue
        if B[p].get(k) != v or val(p, k, v, St) != val(p, k, B[p][k], B):
            body.append(prop(p, k, v, St, '\t'))
    for k in B[p]:
        if k not in St[p] and k not in ('phandle', 'linux,phandle'):
            body.append('\t/delete-property/ %s;' % k)
    newl = [l for l in sl.get(p, []) if l not in bl.get(p, [])]
    if body or newl:
        lab = ''.join(l + ': ' for l in newl)
        out.append('%s%s {' % (lab, label_ref(p)) if p != '/' else '/ {')
        out += body; out.append('};'); out.append('')
print('\n'.join(out + dels))
