#!/usr/bin/env python3
# callcmp.py OBJ func... : compare call multisets (stock image vs our object, relocations filtered to the function)
import re, subprocess, sys, collections
K='/extra2/projects/home/rgds-kernel'; T=K+'/toolchain/clang-r487747c/bin/'
def stock_calls(fn):
    out=subprocess.run([T+'llvm-nm','-n',K+'/stock/stock.elf'],capture_output=True,text=True).stdout.split('\n')
    rows=[l.split() for l in out if len(l.split())==3]
    for i,r in enumerate(rows):
        if r[2]==fn: a,b=int(r[0],16),int(rows[i+1][0],16); break
    d=subprocess.run([T+'llvm-objdump','-d','--no-show-raw-insn',f'--start-address={a}',f'--stop-address={b}',K+'/stock/stock.elf'],capture_output=True,text=True).stdout
    return collections.Counter(re.findall(r'\bbl\s+0x[0-9a-f]+ <([^>+]+)>',d))
def our_calls(obj,fn):
    nm=subprocess.run([T+'llvm-nm','-S',obj],capture_output=True,text=True).stdout.split('\n')
    a=s=None
    for l in nm:
        p=l.split()
        if len(p)==4 and p[3]==fn: a,s=int(p[0],16),int(p[1],16)
    d=subprocess.run([T+'llvm-objdump','-dr','--no-show-raw-insn',f'--disassemble-symbols={fn}',obj],capture_output=True,text=True).stdout
    c=collections.Counter()
    for m in re.finditer(r'^\s*([0-9a-f]+):\s+R_AARCH64_CALL26\s+(\S+)',d,re.M):
        o=int(m.group(1),16)
        if a<=o<a+s: c[m.group(2)]+=1
    return c
obj=sys.argv[1]
for fn in sys.argv[2:]:
    sc,oc=stock_calls(fn),our_calls(obj,fn)
    keys=sorted(set(sc)|set(oc))
    diff={k:(sc[k],oc[k]) for k in keys if sc[k]!=oc[k]}
    print(fn,'DIFF (stock,ours):',diff if diff else 'none')
