# vmcmp.py <our vmlinux> <func...>: per function, opcode-only diff count and operand diffs (first 6)
import subprocess,sys,re
K='/extra2/projects/home/rgds-kernel'; T=K+'/toolchain/clang-r487747c/bin/'
def tab(path):
    rows=[l.split() for l in subprocess.run([T+'llvm-nm','-n',path],capture_output=True,text=True).stdout.splitlines()]
    rows=[(int(r[0],16),r[2]) for r in rows if len(r)==3 and r[1] in 'tTwW']
    d={}
    from collections import Counter
    c=Counter(n for a,n in rows)
    for i,(a,n) in enumerate(rows[:-1]):
        if c[n]==1: d[n]=(a,rows[i+1][0])
    return d
def ins(path,a,b):
    out=subprocess.run([T+'llvm-objdump','-d','--no-show-raw-insn','--start-address=%#x'%a,'--stop-address=%#x'%b,path],capture_output=True,text=True).stdout
    return [l.split('\t')[1:3] for l in out.splitlines() if re.match(r'\s*[0-9a-f]+:\s',l) and len(l.split('\t'))>2]
S=tab(K+'/stock/stock.elf'); O=tab(sys.argv[1])
for n in sys.argv[2:]:
    if n not in S or n not in O: print(n,'missing'); continue
    s=ins(K+'/stock/stock.elf',*S[n]); o=ins(sys.argv[1],*O[n])
    opd=sum(1 for x,y in zip(s,o) if x[0]!=y[0])+abs(len(s)-len(o))
    norm=lambda v: re.sub(r'0x[0-9a-f]+','N',v[1]) if v[0] in ('adrp','add','ldr','str','ldrb','strb','ldrh','strh','ldp','stp','bl','b','ldrsw','ldur','stur') else v[1]
    ops=[(i,x,y) for i,(x,y) in enumerate(zip(s,o)) if x[0]==y[0] and norm(x)!=norm(y) and not x[0].startswith('b')]
    print(n,'len',len(s),len(o),'opcode-diffs',opd,'operand-diffs',len(ops),'|',' ; '.join('%s %s / %s'%(x[0],x[1],y[1]) for i,x,y in ops[:4]))
