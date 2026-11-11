from pathlib import Path
import re
exports=Path('research/libdev-exports.txt').read_text()
lines=Path('research/libdev-full-disassembly.txt').read_text().splitlines()
funcs=[]
for line in exports.splitlines():
 m=re.search(r'([0-9A-F]{8}) \?(cameraSet\w+U)@',line)
 if m:funcs.append((int(m[1],16),m[2]))
for rva,name in funcs:
 start=next((i for i,l in enumerate(lines) if l.startswith(f'  {0x180000000+rva:016X}:')),None)
 if start is None: continue
 print('\n'+name+' '+hex(rva))
 for l in lines[start:start+90]:
  if ': ret' in l or re.search(r': ret\s*$',l):break
  if 'rsp+' in l or 'mov         edx,' in l or 'call ' in l:print(l)
