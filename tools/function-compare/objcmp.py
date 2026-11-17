# objcmp.py <obj>: for every text function in obj that also exists (unique name) in stock,
# compare size and opcode sequence (mnemonics only). Prints non-matching functions.
import subprocess,sys,re,collections
K='/extra2/projects/home/rgds-kernel'; T=K+'/toolchain/clang-r487747c/bin/'
obj=sys.argv[1]
rows=[l.split() for l in open(K+'/symdiff/stock-nm.sorted.txt') if len(l.split())==3]
cnt=collections.Counter(r[2] for r in rows)
st={}
srt=sorted((int(a,16),n) for a,t,n in rows if t in 'tT')
for i,(a,n) in enumerate(srt):
    if cnt[n]==1 and i+1<len(srt): st[n]=(a,srt[i+1][0])
def ops(path,a,b,sec=None):
    cmd=[T+'llvm-objdump','-d','--no-show-raw-insn','--start-address=%#x'%a,'--stop-address=%#x'%b]
    if sec: cmd+=['-j',sec]
    out=subprocess.run(cmd+[path],capture_output=True,text=True).stdout
    return [l.split('\t')[1] for l in out.splitlines() if re.match(r'\s*[0-9a-f]+:\s',l) and len(l.split('\t'))>2]
nm=subprocess.run([T+'llvm-nm','-S','--defined-only',obj],capture_output=True,text=True).stdout
syms=subprocess.run([T+'llvm-objdump','-t',obj],capture_output=True,text=True).stdout
secof={}
for l in syms.splitlines():
    p=l.split()
    if len(p)>=6 and p[-1]: secof[p[-1]]=p[-3] if p[-3].startswith('.') else None
bad=0; tot=0
for l in nm.splitlines():
    p=l.split()
    if len(p)!=4 or p[2] not in 'tT' or p[3] not in st: continue
    n=p[3]; oa=int(p[0],16); oz=int(p[1],16); sa,sb=st[n]; tot+=1
    if oz!=sb-sa:
        print('SIZE',n,sb-sa,oz); bad+=1; continue
    so=ops(K+'/stock/stock.elf',sa,sb); oo=ops(obj,oa,oa+oz,secof.get(n,'.text'))
    if so!=oo:
        d=sum(1 for x,y in zip(so,oo) if x!=y); print('CODE',n,sb-sa,'opdiffs',d); bad+=1
print('checked',tot,'mismatch',bad)
