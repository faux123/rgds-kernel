#!/usr/bin/env python3
# try_mtk.py FILE FUNC : replace FUNC's body in our FILE with the MTK V2.2 body; prints nothing on success.
# Keeps our signature line. Use --revert FILE to restore the saved copy.
import sys, re, shutil
OURS='/extra2/projects/home/rgds-kernel/wt-joypad/drivers/input/touchscreen/hynitron/'
MTK='/extra2/projects/home/rgds-charge/kbase/drivers-rev/hynitron_mtk_v22/'
def body(src,start):
    i=src.index('{',start); d=0
    for k in range(i,len(src)):
        if src[k]=='{': d+=1
        elif src[k]=='}':
            d-=1
            if d==0: return i,k+1
f,fn=sys.argv[1],sys.argv[2]
m=open(MTK+f,errors='replace').read().replace('\r','')
mm=re.search(r'^[A-Za-z][^\n;]*\b%s\s*\([^;{]*\)\s*\n?\{'%re.escape(fn),m,re.M)
p=OURS+f; o=open(p,newline='').read(); nl='\r\n' if '\r\n' in o else '\n'
om=re.search(r'^[A-Za-z][^\n;]*\b%s\s*\([^;{]*\)\s*(\r?\n)?\{'%re.escape(fn),o,re.M)
if not mm or not om: print('NOT FOUND', bool(mm), bool(om)); sys.exit(1)
shutil.copy(p,p+'.bak')
mi,mj=body(m,mm.start()); oi,oj=body(o,om.start())
o=o[:oi]+m[mi:mj].replace('\n',nl)+o[oj:]
open(p,'w',newline='').write(o)
