# Read-only model/protocol check through the hash-checked installed SDK.
exec(open('research/probe-encoder.py').read().split(' outputs=[]')[0])
import time,sys
hook=None
if "--wire-probe" in sys.argv:
 hook=c.WinDLL(str(Path('build/sdk-wire-probe.dll').resolve()))
 hook.install.argtypes=[c.c_void_p];hook.install.restype=c.c_bool
 assert hook.install(base),"SDK callsite differs; refusing instrumentation"
get=c.CFUNCTYPE(c.c_void_p)(base+0x101910)
manager=get()
c.CFUNCTYPE(None,c.c_void_p,c.c_bool)(base+0x102170)(manager,False)
count=c.CFUNCTYPE(c.c_size_t,c.c_void_p)(base+0x101a70)
for _ in range(120):
 if count(manager):break
 time.sleep(.1)
print('SDK device count',count(manager),flush=True)
out=c.create_string_buffer(16)
c.CFUNCTYPE(None,c.c_void_p,c.c_void_p)(base+0x101ee0)(manager,out)
head,size=struct.unpack('<QQ',out.raw)
if not size:
 c.CFUNCTYPE(None,c.c_void_p)(base+0x101990)(manager)
 raise RuntimeError('SDK did not enumerate a camera')
node=c.c_void_p.from_address(head).value
for _ in range(min(size,10)):
 dev=c.c_void_p.from_address(node+16).value
 private=c.c_void_p.from_address(dev).value
 transport=c.c_int.from_address(private+0xc).value
 protocol=c.c_int.from_address(private+0x14d8).value
 print('USB transport',transport,'protocol version',protocol,flush=True)
 node=c.c_void_p.from_address(node).value
if "--power-session" in sys.argv:
 print("READY",flush=True)
 power=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_int)(base+0x24230)
 for action in sys.stdin:
  action=action.strip()
  if action=="quit":break
  if action in ["wake","sleep"]:
   result=power(dev,1 if action=="wake" else 3)
   time.sleep(2 if action=="wake" else 1)
   print("OK" if result==0 else "ERROR SDK power result "+str(result),flush=True)
  elif action=="diagnostics": print("STATUS SDK protocol "+str(protocol)+"\nOK",flush=True)
  else:print("ERROR Unknown command",flush=True)
c.CFUNCTYPE(None,c.c_void_p)(base+0x101990)(manager)
if hook:hook.uninstall()
