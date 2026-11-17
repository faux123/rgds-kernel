#!/usr/bin/env python3
# fdump.py <elf> <function> [first_n]: disassemble one function (text-symbol bounds)
import subprocess,sys
T='/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin/'
elf,fn=sys.argv[1],sys.argv[2]; n=int(sys.argv[3]) if len(sys.argv)>3 else 10**9
rows=[l.split() for l in subprocess.run([T+'llvm-nm','-n',elf],capture_output=True,text=True).stdout.splitlines()]
rows=[(int(r[0],16),r[2]) for r in rows if len(r)==3 and r[1] in 'tTwW']
for i,(a,name) in enumerate(rows):
    if name==fn:
        b=rows[i+1][0]; break
out=subprocess.run([T+'llvm-objdump','-d','--no-show-raw-insn','--start-address=%#x'%a,'--stop-address=%#x'%b,elf],capture_output=True,text=True).stdout
import re; ins=[l for l in out.splitlines() if re.match(r'\s*[0-9a-f]{16}:', l)]
for l in ins[:n]:
    addr,rest=l.split(':',1); print('%04x %s'%(int(addr,16)-a,' '.join(rest.split())))
