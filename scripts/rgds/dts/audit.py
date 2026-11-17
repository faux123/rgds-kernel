import sys, struct, re
sys.path.insert(0, sys.argv[0].rsplit("/", 1)[0]); from normlib import parse, cells, phmap, SPEC, SINGLE
st, ge = parse(sys.argv[1]), parse(sys.argv[2])
sph, gph = phmap(st), phmap(ge); gpath = {v: k for k, v in gph.items()}
resolved = lambda k: k in SPEC or k.endswith("gpio") or k.endswith("gpios") or re.match(r"pinctrl-\d+$", k) or SINGLE.match(k) or k in ("rockchip,pins","msi-map","interrupt-map")
hits = {}
for p, pr in st.items():
    if p.startswith('/__'): continue
    for k, v in pr.items():
        if k in ('phandle','linux,phandle') or resolved(k): continue
        c = cells(v)
        if not c or len(v) < 4: continue
        for x in c:
            if x in sph and gpath.get(sph[x]) != x:  # a stock phandle number whose node got a new number
                hits.setdefault(k, set()).add(p); break
for k in sorted(hits): print(k, len(hits[k]), sorted(hits[k])[:2])
