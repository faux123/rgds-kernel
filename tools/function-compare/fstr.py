# fstr.py <elf> <func> : list every adrp+add string reference (x0..x5) in order
import re,sys,struct,subprocess
T='/extra2/projects/home/rgds-kernel/toolchain/clang-r487747c/bin/'
p,fn=sys.argv[1],sys.argv[2]; d=open(p,'rb').read()
phoff=struct.unpack_from('<Q',d,0x20)[0]; n=struct.unpack_from('<H',d,0x38)[0]
seg=[struct.unpack_from('<IIQQQQQQ',d,phoff+i*56) for i in range(n)]
def cs(a):
    for t,f,o,v,pa,fs,ms,al in seg:
        if t==1 and v<=a<v+fs:
            x=o+a-v; return d[x:d.index(b'\0',x)].decode('latin1')[:40]
    return '?'
rows=[l.split() for l in subprocess.run([T+'llvm-nm','-n',p],capture_output=True,text=True).stdout.splitlines()]
for i,r in enumerate(rows):
    if len(r)==3 and r[2]==fn: a=int(r[0],16); b=int(rows[i+1][0],16); break
out=subprocess.run([T+'llvm-objdump','-d','--no-show-raw-insn','--start-address=%#x'%a,'--stop-address=%#x'%b,p],capture_output=True,text=True).stdout
pg={}
for l in out.splitlines():
    m=re.match(r'\s*[0-9a-f]+:\s+(\S+)\s+(.*)',l)
    if not m: continue
    op,ar=m.groups()
    if op=='adrp': r=re.match(r'(x\d+), 0x([0-9a-f]+)',ar); pg[r.group(1)]=int(r.group(2),16)
    elif op=='add':
        r=re.match(r'(x\d+), (x\d+), #0x([0-9a-f]+)',ar)
        if r and r.group(2) in pg: print(r.group(1),repr(cs(pg[r.group(2)]+int(r.group(3),16))))
