# print enum index of rk817_battery fields in our source (enum before F_MAX_FIELDS)
import re,sys
src=open(sys.argv[1]).read()
m=re.search(r'enum\s+rk817_battery_fields\s*\{(.*?)F_MAX_FIELDS',src,re.S)
body=re.sub(r'/\*.*?\*/','',m.group(1),flags=re.S)
names=[x.strip() for x in body.replace('\n',' ').split(',') if x.strip()]
want=[int(x) for x in sys.argv[2:]]
for i in want: print(i, names[i])
