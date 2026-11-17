#!/usr/bin/env python3
# Whole-kernel function comparison by address ranges (nm boundaries), stock.elf vs our vmlinux.
import re, subprocess, sys, collections, bisect
K='/extra2/projects/home/rgds-kernel'
T=K+'/toolchain/clang-r487747c/bin/'
def nmtab(path, ours):
    out=subprocess.run([T+'llvm-nm','-n','--defined-only',path],capture_output=True,text=True).stdout
    rows=[]
    for l in out.splitlines():
        p=l.split()
        if len(p)<3: continue
        a,t,n=int(p[0],16),p[1],p[2]
        if n.startswith('$') or n.startswith('.L'): continue
        rows.append((a,t,n))
    return rows
def ranges(rows):
    names=collections.Counter(n for a,t,n in rows)
    r={}
    for i,(a,t,n) in enumerate(rows[:-1]):
        if t not in 'tTwW' or names[n]>1: continue
        b=rows[i+1][0]
        if b>a: r[n]=(a,b)
    return r
def disasm(path):
    p=subprocess.Popen([T+'llvm-objdump','-d','--no-show-raw-insn',path],stdout=subprocess.PIPE,text=True,bufsize=1<<20)
    addrs=[]; ins=[]
    rx=re.compile(r'^\s*([0-9a-f]{16}):\s+(\S+)\s*(.*)$')
    lab=re.compile(r'0x[0-9a-f]{8,}( <([^+>]+)[^>]*>)?')
    for l in p.stdout:
        m=rx.match(l)
        if not m: continue
        a,op,arg=m.groups()
        addrs.append(int(a,16)); ins.append((op,lab.sub(lambda mm:'<'+(mm.group(2) or 'A')+'>',arg)))
    return addrs,ins
def sl(D,a,b):
    addrs,ins=D; i=bisect.bisect_left(addrs,a); j=bisect.bisect_left(addrs,b); return ins[i:j]
def addrnorm(seq):
    # address-only noise: adrp/adr page or label operands, lo12/offset immediates,
    # literal-pool .word values, and adrp+add vs adr+nop (adr expanded, nop dropped)
    out=[]
    for op,arg in seq:
        if op=='nop': continue
        if op=='adr':
            r=arg.split(',')[0].strip()
            out.append(('adrp','<P>')); out.append(('add',f'{r}, {r}, #I')); continue
        if op in ('.word','.xword','.short','.byte'): out.append((op,'I')); continue
        if op=='adrp': arg='<P>'
        elif op in ('add','ldr','str','ldrb','strb','ldrh','strh','ldrsw','ldp','stp','ldur','stur','ldrsb','ldrsh'):
            arg=re.sub(r'#0x[0-9a-f]+','#I',arg)
            arg=re.sub(r'\[(x\d+|sp)\]',r'[\1, #I]',arg)  # [xN] == [xN, #off]: lo12 that happens to be 0
        elif op in ('bl','b'):
            pass
        out.append((op,arg))
    return out
Sr=ranges(nmtab(K+'/stock/stock.elf',False)); Or=ranges(nmtab(sys.argv[1],True))
SD=disasm(K+'/stock/stock.elf'); OD=disasm(sys.argv[1])
cnt=collections.Counter(); rows=[]
for n in Sr.keys() & Or.keys():
    sa,sb=Sr[n]; oa,ob=Or[n]
    if sb-sa > 1<<20 or ob-oa > 1<<20: continue
    s,o=sl(SD,sa,sb),sl(OD,oa,ob)
    if not s and not o: continue
    if s==o: c='IDENTICAL'
    elif addrnorm(s)==addrnorm(o): c='ADDR_ONLY'
    elif len(s)==len(o): c='SAMESIZE_DIFF'
    else: c='DIFFSIZE'
    cnt[c]+=1
    if c in ('DIFFSIZE','SAMESIZE_DIFF'): rows.append((c,n,(sb-sa),(ob-oa)))
print(dict(cnt),'common',sum(cnt.values()))
with open(K+'/audit/funcdiff.tsv','w') as f:
    for c,n,a,b in sorted(rows,key=lambda r:-abs(r[3]-r[2])): f.write(f'{c}\t{n}\t{a}\t{b}\t{b-a}\n')
