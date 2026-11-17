#!/usr/bin/env python3
"""Dump a DTB with phandle references resolved to node paths, sorted, phandle props dropped."""
import sys
sys.path.insert(0, sys.argv[0].rsplit('/', 1)[0])
from normlib import parse, render_tokens
n = parse(sys.argv[1]); out = []
for p in sorted(n):
    out.append(p + ' {')
    for k in sorted(n[p]):
        if k in ('phandle', 'linux,phandle'): continue
        v = n[p][k]
        out.append('\t%s = %s;' % (k, render_tokens(n, p, k, v)) if v else '\t%s;' % k)
    out.append('}')
print('\n'.join(out))
