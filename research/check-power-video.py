"""Hardware regression: retain only aggregate statistics, never camera frames on disk."""
import collections, json, shutil, statistics, subprocess, sys, threading, time, queue
from pathlib import Path
cli = str(Path(sys.argv[1]).resolve())
controller=subprocess.Popen(([sys.executable, "-u", cli, "--power-session"] if cli.endswith(".py") else [cli,"power-session"]),stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
responses=queue.Queue()
def read_controller():
    for line in controller.stdout:
        if cli.endswith(".py") and not line.startswith(("READY","OK","ERROR ","STATUS ")): continue
        responses.put(line.rstrip())
threading.Thread(target=read_controller,daemon=True).start()
if responses.get(timeout=20)!="READY": raise RuntimeError("Control session did not start")
def command(name):
    controller.stdin.write(name+"\n");controller.stdin.flush()
    result=[]
    while True:
        line=responses.get(timeout=20)
        if line=="OK": return "\n".join(result)
        if line.startswith("ERROR "): raise RuntimeError(line)
        result.append(line)
command("wake")
ff = subprocess.Popen([shutil.which("ffmpeg"), "-hide_banner", "-loglevel", "error",
    "-f", "dshow", "-i", "video=OBSBOT Tiny SE StreamCamera", "-an",
    "-c:v", "copy", "-flush_packets", "1", "-f", "mjpeg", "pipe:1"],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
frames = collections.deque(maxlen=90)
def read():
    buffer=b""
    while True:
        data=ff.stdout.read(4096)
        if not data: break
        buffer+=data
        while True:
            start=buffer.find(b"\xff\xd8")
            end=buffer.find(b"\xff\xd9",max(0,start))
            if start<0 or end<0: break
            frames.append((time.monotonic(),buffer[start:end+2]))
            buffer=buffer[end+2:]
thread=threading.Thread(target=read,daemon=True);thread.start()
errors=[]
def drain_errors():
    while True:
        line=ff.stderr.readline()
        if not line: break
        if len(errors)<10: errors.append(line.decode(errors="replace"))
error_thread=threading.Thread(target=drain_errors,daemon=True);error_thread.start()
def sample(allow_stale=False):
    time.sleep(1)
    deadline=time.monotonic()+(0 if allow_stale else 10)
    while True:
        now=time.monotonic(); batch=[f for t,f in frames if now-t<2]
        if batch or now>=deadline: break
        time.sleep(.1)
    fresh=len(batch)
    if not batch and allow_stale: batch=[f for _,f in list(frames)[-5:]]
    if not batch: raise RuntimeError("No current video frames after wake; retained="+str(len(frames))+", capture exit="+str(ff.poll()))
    jpeg=batch[-1]
    decoded=subprocess.run([shutil.which("ffmpeg"),"-hide_banner","-loglevel","error",
        "-f","mjpeg","-i","pipe:0","-frames:v","1","-vf","scale=32:18,format=gray",
        "-f","rawvideo","pipe:1"],input=jpeg,capture_output=True,timeout=10)
    mean=list(decoded.stdout)
    if len(mean)!=576: raise RuntimeError("Cannot decode captured JPEG")
    return mean,{"fresh_frames":fresh,"frames":len(batch),"jpeg_bytes":len(jpeg),
        "mean":round(statistics.mean(mean),3),"spatial_sd":round(statistics.pstdev(mean),3)}
results=[]
try:
    time.sleep(2)
    _,baseline=sample(); print("baseline",json.dumps(baseline),flush=True)
    for cycle in range(3):
        command("sleep"); print(command("diagnostics"),flush=True); asleep,sm=sample(allow_stale=True)
        command("wake"); print(command("diagnostics"),flush=True); awake,wm=sample()
        command("wake")
        repeated,repeat_metrics=sample()
        if repeat_metrics["fresh_frames"] < 5:
            raise RuntimeError("Repeated explicit Wake interrupted capture")
        difference=sum(abs(a-b) for a,b in zip(asleep,awake))/576
        if difference < 5 or wm["fresh_frames"] < 5:
            raise RuntimeError("Wake did not restore distinct, continuously arriving camera frames")
        result={"cycle":cycle+1,"sleep":sm,"wake":wm,"repeated_wake":repeat_metrics,"sleep_wake_image_mad":round(difference,3)}
        results.append(result);print(json.dumps(result),flush=True)
    print("PASS: three sleep/wake cycles plus repeated explicit Wake restored fresh camera frames distinct from the sleep screen without reopening capture",flush=True)
finally:
    try: command("wake")
    finally:
        try: ff.stdin.write(b"q\n");ff.stdin.flush();ff.wait(timeout=5)
        except Exception: ff.kill();ff.wait()
        thread.join(timeout=2)
        error_thread.join(timeout=2)
        err="".join(errors)
        if err: print("capture diagnostics:",err[:1500],flush=True)
        controller.stdin.write("quit\n");controller.stdin.flush();controller.wait(timeout=10)
