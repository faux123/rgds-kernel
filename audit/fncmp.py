#!/usr/bin/env python3
# usage: fncmp.py func...  -> compare stock vs our function size and normalized instruction stream
import re, subprocess, sys
K='/extra2/projects/home/rgds-kernel'
OD=K+'/toolchain/clang-r487747c/bin/llvm-objdump'
def table(path):
    out=subprocess.run([K+'/toolchain/clang-r487747c/bin/llvm-nm','-n',path],capture_output=True,text=True).stdout
    rows=[(int(a,16),n) for a,t,n in (l.split()[:3] for l in out.splitlines() if len(l.split())>=3)]
    return rows
def ranges(rows):
    d={}
    for i,(a,n) in enumerate(rows[:-1]):
        if n not in d: d[n]=(a,rows[i+1][0])
    return d
def dis(path,a,b):
    out=subprocess.run([OD,'-d','--no-show-raw-insn',f'--start-address={a}',f'--stop-address={b}',path],capture_output=True,text=True).stdout
    ins=[]
    for l in out.splitlines():
        m=re.match(r'\s*[0-9a-f]+:\s+(\S+)\s*(.*)',l)
        if not m: continue
        op,arg=m.groups()
        arg=re.sub(r'0x[0-9a-f]{8,}( <([^+>]+)[^>]*>)?',lambda mm:'<'+(mm.group(2) or 'A')+'>',arg)
        arg=re.sub(r'#0x[0-9a-f]+\b','#I',arg) if op in('add','adrp','ldr','str','ldrb','strb','ldrh','strh','ldp','stp','ldur','stur') else arg
        ins.append(op+' '+arg)
    return ins
S=ranges(table(K+'/stock/stock.elf')); O=ranges(table(K+'/src/vmlinux'))
for f in sys.argv[1:]:
    if f not in S or f not in O: print(f'{f:36} stock={"y" if f in S else "-"} ours={"y" if f in O else "-"}'); continue
    sa,sb=S[f]; oa,ob=O[f]
    si,oi=dis(K+'/stock/stock.elf',sa,sb),dis(K+'/src/vmlinux',oa,ob)
    same=sum(1 for x,y in zip(si,oi) if x==y)
    print(f'{f:36} stock={sb-sa:5} ours={ob-oa:5} insn {len(si)}/{len(oi)} same-pos={same} {"IDENTICAL" if si==oi else ""}')
if '--show' in sys.argv[0:1]: pass
