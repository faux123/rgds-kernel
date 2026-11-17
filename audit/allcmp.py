#!/usr/bin/env python3
# Whole-kernel function comparison: stock.elf vs our vmlinux.
# Categories: IDENTICAL, ADRP (only adrp/page-label diffs), SAMESIZE (same size, other diffs), DIFFSIZE.
import re, subprocess, sys, collections
K='/extra2/projects/home/rgds-kernel'
OD=K+'/toolchain/clang-r487747c/bin/llvm-objdump'
def load(path):
    p=subprocess.Popen([OD,'-d','--no-show-raw-insn',path],stdout=subprocess.PIPE,text=True,bufsize=1<<20)
    funcs={}; cur=None; dup=set()
    hdr=re.compile(r'^([0-9a-f]{16}) <(.+)>:$')
    ins=re.compile(r'^\s*[0-9a-f]+:\s+(\S+)\s*(.*)$')
    lab=re.compile(r'0x[0-9a-f]{8,}( <([^+>]+)[^>]*>)?')
    for l in p.stdout:
        m=hdr.match(l)
        if m:
            name=m.group(2)
            if name in funcs: dup.add(name); cur=None; continue
            cur=[]; funcs[name]=cur; continue
        if cur is None: continue
        m=ins.match(l)
        if not m: continue
        op,arg=m.groups()
        arg=lab.sub(lambda mm:'<'+(mm.group(2) or 'A')+'>',arg)
        cur.append((op,arg))
    for d in dup: funcs.pop(d,None)
    return funcs
def norm_adrp(seq):
    out=[]
    for op,arg in seq:
        if op=='adrp': arg=re.sub(r'<[^>]*>','<P>',arg)
        out.append((op,arg))
    return out
S=load(K+'/stock/stock.elf'); O=load(sys.argv[1])
cnt=collections.Counter(); diffsize=[]
for n in S.keys() & O.keys():
    s,o=S[n],O[n]
    if not s or not o: continue
    if s==o: cnt['IDENTICAL']+=1
    elif len(s)==len(o) and norm_adrp(s)==norm_adrp(o): cnt['ADRP']+=1
    elif len(s)==len(o): cnt['SAMESIZE']+=1
    else: cnt['DIFFSIZE']+=1; diffsize.append((abs(len(s)-len(o)),n,len(s)*4,len(o)*4))
print(dict(cnt), 'common', sum(cnt.values()), 'stock-only-funcs', len(S.keys()-O.keys()), 'ours-only-funcs', len(O.keys()-S.keys()))
diffsize.sort(reverse=True)
with open(K+'/audit/diffsize.tsv','w') as f:
    for d,n,a,b in diffsize: f.write(f'{n}\t{a}\t{b}\t{b-a}\n')
