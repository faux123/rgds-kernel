#!/usr/bin/env python3
# fncmp variant: compare stock functions against a relocatable object (our build).
# usage: fncmp_obj.py OBJ func...   Normalization copied from audit/fncmp.py; text symbols only.
import re, subprocess, sys
K='/extra2/projects/home/rgds-kernel'
NM=K+'/toolchain/clang-r487747c/bin/llvm-nm'; OD=K+'/toolchain/clang-r487747c/bin/llvm-objdump'
def ranges(path, textonly):
    out=subprocess.run([NM,'-n']+(['-S','--defined-only'] if textonly else [])+[path],capture_output=True,text=True).stdout
    rows=[l.split() for l in out.splitlines()]
    d={}
    if textonly:
        for r in rows:
            if len(r)==4 and r[2] in 'tT': d.setdefault(r[3],(int(r[0],16),int(r[0],16)+int(r[1],16)))
    else:
        rr=[(int(r[0],16),r[2]) for r in rows if len(r)>=3]
        for i,(a,n) in enumerate(rr[:-1]): d.setdefault(n,(a,rr[i+1][0]))
    return d
def dis(path,a,b,sym=None):
    # For a relocatable object every function sits in its own section at address 0, so an address range can
    # pull in other functions; disassemble by symbol name there instead.
    args=[OD,'-d','--no-show-raw-insn']+([f'--disassemble-symbols={sym}'] if sym else [f'--start-address={a}',f'--stop-address={b}'])+[path]
    out=subprocess.run(args,capture_output=True,text=True).stdout
    ins=[]
    for l in out.splitlines():
        m=re.match(r'\s*[0-9a-f]+:\s+(\S+)\s*(.*)',l)
        if not m: continue
        op,arg=m.groups()
        arg=re.sub(r'0x[0-9a-f]+( <([^+>]+)[^>]*>)?',lambda mm:'<'+(mm.group(2) or 'A')+'>' if op in('b','bl','adrp') or op.startswith(('b.','cb','tb')) else mm.group(0),arg)
        arg=re.sub(r'#0x[0-9a-f]+\b','#I',arg) if op in('add','adrp','ldr','str','ldrb','strb','ldrh','strh','ldp','stp','ldur','stur') else arg
        if op in('bl','b') : arg=re.sub(r'<[^>]*>','<F>',arg)
        if op.startswith(('b.','cb','tb')) or op=='b': arg=re.sub(r'<[^>]*>','<L>',arg)
        if op=='adrp': arg=arg.split(',')[0]                      # page target differs between image and object
        arg=re.sub(r'\[(x\d+|sp), #I\]',r'[\1]',arg)              # lo12 offsets are 0 in the object (relocated)
        if op=='add' and re.match(r'x\d+, x\d+, #I$',arg): arg=re.sub(r', #I$','',arg)
        ins.append(op+' '+arg)
    return ins
obj=sys.argv[1]
S=ranges(K+'/stock/stock.elf',False); O=ranges(obj,True)
for f in sys.argv[2:]:
    if f not in S or f not in O: print(f'{f:30} stock={"y" if f in S else "-"} ours={"y" if f in O else "-"}'); continue
    sa,sb=S[f]; oa,ob=O[f]
    si,oi=dis(K+'/stock/stock.elf',sa,sb),dis(obj,oa,ob,f)
    same=sum(1 for x,y in zip(si,oi) if x==y)
    print(f'{f:30} stock={sb-sa:5} ours={ob-oa:5} insn {len(si)}/{len(oi)} same-pos={same} {"IDENTICAL" if si==oi else ""}')
