"""Exercise real X11 keyboard/mouse events in a Linux or Wine game window.
Run under xvfb-run; use an isolated runtime directory containing assets.
"""
import argparse, ctypes as C, json, os, signal, struct, subprocess, time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--exe',required=True);p.add_argument('--runner');p.add_argument('--runtime',required=True,type=Path);a=p.parse_args()
x=C.CDLL('libX11.so.6');xt=C.CDLL('libXtst.so.6');P=C.c_void_p;W=C.c_ulong
for name,args,ret in [('XOpenDisplay',[C.c_char_p],P),('XDefaultRootWindow',[P],W),('XQueryTree',[P,W,C.POINTER(W),C.POINTER(W),C.POINTER(C.POINTER(W)),C.POINTER(C.c_uint)],C.c_int),('XFetchName',[P,W,C.POINTER(C.c_char_p)],C.c_int),('XInternAtom',[P,C.c_char_p,C.c_int],W),('XGetWindowProperty',[P,W,W,C.c_long,C.c_long,C.c_int,W,C.POINTER(W),C.POINTER(C.c_int),C.POINTER(W),C.POINTER(W),C.POINTER(P)],C.c_int),('XFree',[P],C.c_int),('XSetInputFocus',[P,W,C.c_int,W],C.c_int),('XStringToKeysym',[C.c_char_p],W),('XKeysymToKeycode',[P,W],C.c_uint),('XFlush',[P],C.c_int),('XCloseDisplay',[P],C.c_int)]:
 f=getattr(x,name);f.argtypes=args;f.restype=ret
for name,args in [('XTestFakeKeyEvent',[P,C.c_uint,C.c_int,W]),('XTestFakeButtonEvent',[P,C.c_uint,C.c_int,W]),('XTestFakeRelativeMotionEvent',[P,C.c_int,C.c_int,W])]:getattr(xt,name).argtypes=args

def windows(d,parent):
 root=W();par=W();children=C.POINTER(W)();count=C.c_uint()
 if not x.XQueryTree(d,parent,C.byref(root),C.byref(par),C.byref(children),C.byref(count)):return []
 result=[]
 for i in range(count.value):
  w=children[i];name=C.c_char_p();x.XFetchName(d,w,C.byref(name))
  title=name.value or b''
  if name.value:x.XFree(C.cast(name,P))
  if not title:
   actual=W();fmt=C.c_int();items=W();remaining=W();data=P()
   x.XGetWindowProperty(d,w,x.XInternAtom(d,b'_NET_WM_NAME',0),0,1024,0,0,C.byref(actual),C.byref(fmt),C.byref(items),C.byref(remaining),C.byref(data))
   if data.value:title=C.string_at(data,items.value);x.XFree(data)
  if b'GRAY ZONE' in title.upper():result.append(w)
  result.extend(windows(d,w))
 if children:x.XFree(children)
 return result

d=x.XOpenDisplay(None);assert d,'X11 display missing';runtime=a.runtime.resolve();runtime.mkdir(parents=True,exist_ok=True)
assert (runtime/'assets').is_dir(),'Runtime assets missing'
command=([a.runner] if a.runner else [])+[str(Path(a.exe).resolve()),'--renderer=vulkan']
output=(runtime/'interactive-runtime.log').open('wb');process=subprocess.Popen(command,cwd=runtime,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)

def key(name,down=True):
 code=x.XKeysymToKeycode(d,x.XStringToKeysym(name.encode()));assert code,name
 xt.XTestFakeKeyEvent(d,code,int(down),0);x.XFlush(d)
def press(name):key(name);time.sleep(.10);key(name,False);time.sleep(.65)
def button(number,down):xt.XTestFakeButtonEvent(d,number,int(down),0);x.XFlush(d)
def snapshot():
 save=runtime/'saves/slot_01.sav';previous=save.stat().st_mtime_ns if save.exists() else 0;press('F5')
 deadline=time.monotonic()+5
 while time.monotonic()<deadline:
  if save.exists() and save.stat().st_mtime_ns!=previous:break
  time.sleep(.1)
 data=save.read_bytes();assert data[:5]==b'GZCP2';meta=struct.unpack_from('<I',data,9)[0];sim=data[21+meta:];assert sim[:4]==b'GZ89'
 version=struct.unpack_from('<I',sim,4)[0];assert version==3
 tick,shots=struct.unpack_from('<QQ',sim,8);squads,soldiers=struct.unpack_from('<II',sim,32);individual=struct.unpack_from('<I',sim,len(sim)-4)[0]
 offset=62+squads*31+individual*47
 xx,zz=struct.unpack_from('<dd',sim,offset+8);ammo=struct.unpack_from('<H',sim,offset+26)[0];magazine=struct.unpack_from('<H',sim,offset+32)[0]
 return dict(tick=tick,shots=shots,mode=sim[40],individual=individual,x=xx,z=zz,ammo=ammo,magazine=magazine,weapon=sim[offset+31],facing=struct.unpack_from('<f',sim,offset+38)[0])
try:
 deadline=time.monotonic()+20;found=[]
 while time.monotonic()<deadline and not found:
  assert process.poll() is None,'Game exited at startup';found=windows(d,x.XDefaultRootWindow(d));time.sleep(.2)
 assert found,'Game window missing';x.XSetInputFocus(d,found[-1],2,0);x.XFlush(d);time.sleep(1)
 press('Return');press('Return');press('Return');press('2');before=snapshot();assert before['mode']==1,before
 xt.XTestFakeRelativeMotionEvent(d,60,15,0);x.XFlush(d);key('w');time.sleep(1.2);key('w',False);button(1,True);time.sleep(.8);button(1,False);moved=snapshot()
 assert abs(moved['x']-before['x'])+abs(moved['z']-before['z'])>.3,(before,moved)
 assert moved['ammo']<before['ammo'] and moved['shots']>before['shots'],(before,moved)
 assert abs(moved['facing']-before['facing'])>.01,'Mouse look not applied'
 press('space');paused=snapshot();key('w');button(1,True);time.sleep(1.2);key('w',False);button(1,False);still=snapshot()
 assert paused==still,('Pause did not freeze gameplay',paused,still)
 press('space');press('r');time.sleep(3);reloaded=snapshot();assert reloaded['magazine']==30,reloaded
 press('f');sidearm=snapshot();assert sidearm['weapon']==4 and sidearm['ammo']==36,sidearm
 press('f');main=snapshot();assert main['weapon']==0 and main['ammo']==reloaded['ammo'],main
 press('Tab');other=snapshot();assert other['individual']!=main['individual'],other
 press('Escape');tactical=snapshot();assert tactical['mode']==0,tactical
 print(json.dumps(dict(status='PASS',initial=before,manual_fire=moved,paused=still,reloaded=reloaded,sidearm=sidearm,selected=other,returned=tactical),ensure_ascii=False))
 press('Escape');press('Down');press('Down');press('Down');press('Return');process.wait(timeout=8);assert process.returncode==0
finally:
 if process.poll() is None:os.killpg(process.pid,signal.SIGTERM);process.wait(timeout=10)
 output.close();x.XCloseDisplay(d)
