exec(open('research/probe-encoder.py').read().split(' outputs=[]')[0])
results=[]
for subsystem,cmds in [(3,[0xe,0x11,0x13,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x31,0x32,0x44,0x53,0x54,0x56,0x57,0x5a,0x70,0x71,0x72,0x73,0x5f,0x60,0x4a,0x29]),(1,[3,0xf,0x40,0x50,0x58,0x63,0x6a])]:
 for cmd in cmds:
  msg=c.create_string_buffer(0x1900);out=c.create_string_buffer(0x1900)
  struct.pack_into('<H',msg,0,12);struct.pack_into('<I',msg,4,0);struct.pack_into('<H',msg,8,1);msg[0xa]=subsystem;struct.pack_into('<H',msg,0xc,11);struct.pack_into('<H',msg,0xe,cmd);struct.pack_into('<H',msg,0x10,subsystem);struct.pack_into('<I',msg,0x14,3);encode3(msg,out,False);crc3(out)
  frame=out.raw[:60];results.append({'subsystem':subsystem,'index':cmd,'receiver':frame[9],'command':int.from_bytes(frame[10:12],'little'),'frame':frame.hex()})
print(json.dumps(results,indent=2))
Path('research/setting-opcode-map.json').write_text(json.dumps(results,indent=2))
