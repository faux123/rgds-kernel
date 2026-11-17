#!/usr/bin/env python3
"""Dump a DTB with phandle references resolved to node paths, sorted, phandle props dropped."""
import re, struct, sys

def parse(path):
    b = open(path, 'rb').read()
    magic, tot, off_st, off_str = struct.unpack('>IIII', b[:16])
    assert magic == 0xd00dfeed
    def s(o):
        e = b.index(b'\0', off_str + o); return b[off_str + o:e].decode()
    nodes = {}; stack = []; p = off_st
    while True:
        tok = struct.unpack('>I', b[p:p+4])[0]; p += 4
        if tok == 1:
            e = b.index(b'\0', p); name = b[p:e].decode(); p = (e + 4) & ~3
            stack.append(name)
            path = '/' + '/'.join(x for x in stack if x) if len(stack) > 1 else '/'
            nodes[path] = {}
        elif tok == 2: stack.pop()
        elif tok == 3:
            ln, no = struct.unpack('>II', b[p:p+8]); p += 8
            path = '/' + '/'.join(x for x in stack if x) if len(stack) > 1 else '/'
            nodes[path][s(no)] = b[p:p+ln]; p = (p + ln + 3) & ~3
        elif tok == 4: continue
        elif tok == 9: break
    return nodes

def cells(v): return list(struct.unpack('>%dI' % (len(v)//4), v)) if len(v) % 4 == 0 else None

# property -> name of #cells property in provider (None = list of bare phandles)
SPEC = {'clocks':'#clock-cells','assigned-clocks':'#clock-cells','assigned-clock-parents':'#clock-cells',
 'resets':'#reset-cells','phys':'#phy-cells','power-domains':'#power-domain-cells','dmas':'#dma-cells',
 'iommus':'#iommu-cells','pwms':'#pwm-cells','io-channels':'#io-channel-cells','mboxes':'#mbox-cells',
 'interrupts-extended':'#interrupt-cells','thermal-sensors':'#thermal-sensor-cells','cooling-device':'#cooling-cells',
 'sound-dai':'#sound-dai-cells','nvmem-cells':None,'interconnects':'#interconnect-cells',
 'WIFI,host_wake_irq':'#gpio-cells','BT,wake_host_irq':'#gpio-cells','mux-controls':'#mux-control-cells','hwlocks':'#hwlock-cells','msi-parent':'#msi-cells'}
SINGLE = re.compile(r'^(interrupt-parent|operating-points-v2|remote-endpoint|memory-region|.*-supply|cpu|trip|'
 r'interrupt-affinity|rockchip,grf|backlight|connect|devfreq|devfreq-events|extcon|mmc-pwrseq|monitored-battery|native-mode|rockchip,cif|rockchip,codec|rockchip,cpu|rockchip,cru|rockchip,ethernet|rockchip,hw|rockchip,panel-notifier|rockchip,sram|rockchip,srv|shmem|snps,axi-config|snps,mtl-rx-config|snps,mtl-tx-config|pm_qos|ts|rockchip,sub-cpu|rockchip,codec|rockchip,dmcfreq|rockchip,pmu|ports|cpu-idle-states|next-level-cache|.*phandle.*|rockchip,.*grf|clock-output)$')


def phmap(n):
    ph={}
    for p,pr in n.items():
        for k in ('phandle','linux,phandle'):
            if k in pr: ph[struct.unpack('>I',pr[k])[0]]=p
    return ph
_cache={}
def render_tokens(n,p,k,v):
    ph=_cache.setdefault(id(n),phmap(n))
    def ref(x): return '&'+ph[x] if x in ph else '0x%x'%x
    c=cells(v); r=None
    if c:
        if k in SPEC or k.endswith('gpios') or k.endswith('gpio') or re.match(r'pinctrl-\d+$',k):
            cn=SPEC.get(k,'#gpio-cells') if not k.startswith('pinctrl-') else None
            if k=='nvmem-cells': cn=None
            r=[];i=0;ok=True
            while i<len(c):
                h=c[i]; r.append(ref(h)); i+=1
                if cn is None: continue
                if h not in ph or cn not in n[ph[h]]:
                    if h==0: r[-1]='0x0'; continue
                    ok=False;break
                nc=struct.unpack('>I',n[ph[h]][cn])[0]
                r+=['0x%x'%x for x in c[i:i+nc]]; i+=nc
            if not ok: r=None
        elif k == 'rockchip,pins' and len(c) % 4 == 0 and all(c[i] in ph for i in range(3, len(c), 4)):
            r = [('0x%x' % x) if i % 4 != 3 else ref(x) for i, x in enumerate(c)]
        elif SINGLE.match(k) and all(x in ph for x in c): r=[ref(x) for x in c]
        if k == 'msi-map' and len(c) % 4 == 0 and all(c[i] in ph for i in range(1, len(c), 4)):
            r = [ref(x) if i % 4 == 1 else '0x%x' % x for i, x in enumerate(c)]
        if k == 'interrupt-map':
            u = lambda q, name, d: struct.unpack('>I', n[q][name])[0] if name in n[q] else d
            ca, ci = u(p, '#address-cells', 2), u(p, '#interrupt-cells', 1)
            r = []; i = 0
            while i < len(c):
                r += ['0x%x' % x for x in c[i:i+ca+ci]]; i += ca + ci
                h = c[i]; r.append(ref(h)); i += 1
                q = ph[h]; pa, pi = u(q, '#address-cells', 0), u(q, '#interrupt-cells', 1)
                r += ['0x%x' % x for x in c[i:i+pa+pi]]; i += pa + pi
    if r is not None: return '<'+' '.join(r)+'>'
    if v and v[-1:]==b'\0' and all(32<=x<127 or x==0 for x in v) and b'\0\0' not in v and v[0]!=0:
        return ', '.join('"%s"'%s.replace('\\','\\\\').replace('"','\\"') for s in v[:-1].decode().split('\0'))
    if c is not None: return '<'+' '.join('0x%x'%x for x in c)+'>'
    return '['+v.hex()+']'
