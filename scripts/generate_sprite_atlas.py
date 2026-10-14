"""Bake 16 procedural RGBA sprite layers, transparent edges and no image assets."""
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFilter
import struct,json
ROOT=Path(__file__).resolve().parent.parent
NAMES=['broadleaf','pine','dry_tree','grass','reed','flowers','fern','cloud','muzzle','smoke','spark','dust','impact','rain','leaf_crown','fire']
def generate(kind,seed=1989):
 rng=np.random.default_rng(seed+kind*79);im=Image.new('RGBA',(256,256));d=ImageDraw.Draw(im)
 if kind in (0,1,2,14):
  if kind!=14:
   d.polygon([(117,250),(139,250),(134,103),(123,90)],fill=(92,74,47,255))
   for i in range(12):
    y=int(rng.integers(85,203));x=int(rng.integers(43,210));d.line([(128,y),(x,y-35)],fill=(89,77,51,255),width=3)
  if kind==1:
   for i in range(7):
    y=24+i*23;width=25+i*11;d.polygon([(128,y-15),(128-width,y+52),(128+width,y+52)],fill=(47+i*3,66+i*4,42+i*2,255))
  elif kind!=2:
   for i in range(120):
    x,y=rng.normal(128,42),rng.normal(89,30);r=int(rng.integers(8,27));color=(int(rng.integers(44,84)),int(rng.integers(65,100)),int(rng.integers(35,59)),255)
    d.ellipse((x-r,y-r,x+r,y+r),fill=color)
   im=im.filter(ImageFilter.GaussianBlur(.4))
 elif kind in (3,4,5,6):
  for i in range(48 if kind==3 else 22):
   x=int(rng.integers(25,232));height=int(rng.integers(45,226));tip=x+int(rng.integers(-31,31));c=(int(rng.integers(65,115)),int(rng.integers(83,133)),int(rng.integers(35,76)),255)
   d.polygon([(x-3,255),(x+3,255),(tip,255-height)],fill=c)
   if kind==4:d.ellipse((tip-3,255-height-15,tip+3,255-height+18),fill=(121,105,65,255))
   if kind==5:d.ellipse((tip-5,255-height-5,tip+5,255-height+5),fill=(220,196,112,255))
   if kind==6:
    for j in range(6):yy=245-j*height/7;d.polygon([(x,yy),(x-22,yy-18),(x,yy-5),(x+22,yy-18)],fill=c)
 else:
  yy,xx=np.mgrid[:256,:256];u=(xx-127.5)/127.5;v=(yy-127.5)/127.5;r=np.sqrt(u*u+v*v)
  if kind in (7,9,11,12):
   cloud=np.zeros((256,256),np.float32)
   for _ in range(12):
    cx,cy=rng.uniform(-.5,.5,2);sx,sy=rng.uniform(.12,.4,2);cloud=np.maximum(cloud,np.exp(-(((u-cx)/sx)**2+((v-cy)/sy)**2)*1.5))
   alpha=np.clip(cloud*(1-r*.6),0,1)*235
   shade=np.clip(.68+.22*cloud-v*.13,0,1);base=(232,236,237) if kind==7 else (177,181,177) if kind==9 else (151,129,100)
   rgba=np.zeros((256,256,4),np.uint8)
   for c in range(3):rgba[:,:,c]=np.uint8(shade*base[c])
   rgba[:,:,3]=alpha.astype(np.uint8);im=Image.fromarray(rgba)
  elif kind in (8,10,15):
   alpha=np.exp(-(u*u*4+v*v*2)*5);alpha=np.maximum(alpha,np.exp(-(u*u*150+v*v)*6))
   rgba=np.zeros((256,256,4),np.uint8);rgba[:,:,0]=255;rgba[:,:,1]=np.uint8(125+100*alpha);rgba[:,:,2]=np.uint8(28+120*alpha);rgba[:,:,3]=np.uint8(alpha*255);im=Image.fromarray(rgba)
  elif kind==13:d.line((126,0,130,256),fill=(183,203,221,180),width=3)
 return np.asarray(im)
def main():
 directory=ROOT/'assets/sprites';directory.mkdir(parents=True,exist_ok=True);arrays=[generate(i) for i in range(16)]
 path=directory/'procedural.gzsa';path.write_bytes(b'GZSA'+struct.pack('<III',1,256,16)+b''.join(x.tobytes() for x in arrays))
 resident=sum((max(1,256>>i)**2)*4 for i in range(9))*16
 texture=json.loads((ROOT/'assets/textures/procedural_manifest.json').read_text());combined=texture['max_texture_resident_bytes']+resident
 assert combined<=100_000_000
 (directory/'manifest.json').write_text(json.dumps(dict(seed=1989,version=1,layers=NAMES,disk_bytes=path.stat().st_size,resident_bytes=resident,combined_texture_resident_bytes=combined,budget_bytes=100_000_000),indent=2)+'\n')
 print(f'16 RGBA sprites, {resident/1e6:.2f} MB resident, total textures {combined/1e6:.2f} MB')
if __name__=='__main__':main()
