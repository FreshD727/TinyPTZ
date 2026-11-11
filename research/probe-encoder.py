import ctypes as c, os, struct, json, hashlib
from pathlib import Path
folder=r'C:\Program Files\OBSBOT Center\bin'
assert hashlib.sha256(Path(folder+r"\libdev.dll").read_bytes()).hexdigest()=="e23a445bb8874f2426a53d6269876a8dd5a478d1c9c0a6e1196fc8fffc8bc994", "Unsupported DLL version: RVAs must be re-established"
with os.add_dll_directory(folder):
 lib=c.WinDLL(folder+r'\libdev.dll')
 base=lib._handle
 encode1=c.CFUNCTYPE(None,c.c_void_p,c.c_void_p)(base+0xee320)
 encode3=c.CFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_bool)(base+0xee430)
 crc1=c.CFUNCTYPE(None,c.c_void_p)(base+0xedcb0)
 crc3=c.CFUNCTYPE(None,c.c_void_p)(base+0xedd50)
 outputs=[]
 for protocol in (0,3):
  for kind,payload,receiver,cmd in [('wake',bytes([1,1]) if protocol==0 else struct.pack('<I',0),1,2),('gimbal',struct.pack('<fff',7.0,84.0,0.0),3,5)]:
   msg=c.create_string_buffer(0x1900); out=c.create_string_buffer(0x1900)
   struct.pack_into('<H',msg,0,12+len(payload))
   struct.pack_into('<I',msg,4,1)
   struct.pack_into('<H',msg,8,1)
   msg[0xa]=receiver; struct.pack_into('<H',msg,0xc,11)
   struct.pack_into('<H',msg,0xe,cmd);struct.pack_into('<H',msg,0x10,receiver)
   struct.pack_into('<I',msg,0x14,protocol);struct.pack_into('<I',msg,0x18,0)
   msg[0x1c:0x1c+len(payload)]=payload
   if protocol==0:encode1(msg,out);crc1(out)
   else:encode3(msg,out,False);crc3(out)
   outputs.append({'protocol':protocol,'kind':kind,'frame':out.raw[:60].hex()})
 print(json.dumps(outputs,indent=2))
 Path('research/encoder-reference.json').write_text(json.dumps(outputs,indent=2))
