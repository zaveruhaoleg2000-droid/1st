"""Deterministic seamless materials and normal maps, within a 100 MB budget.
Requires numpy and Pillow only when regenerating; the game reads GZT1 RGB8.
"""
from pathlib import Path
import argparse, json, struct, hashlib
import numpy as np
from PIL import Image
NAMES=['grass_short','grass_tall','meadow','dirt_dry','mud_wet','sand','gravel','rock','asphalt','concrete','brick','wood_planks','tree_bark','leaves','water','snow','player_uniform','opponent_uniform','skin','armor','rubber','glass','roof','weapon_metal','weapon_wood','fabric','stone','roof_slate','leaves_dry','fence','crate']
COLORS=[(91,104,63),(67,85,48),(112,117,74),(120,99,70),(73,66,50),(165,148,105),(113,111,100),(117,119,110),(62,66,66),(141,139,121),(130,78,59),(117,91,61),(94,81,57),(62,85,46),(42,86,91),(219,224,220),(73,86,49),(106,92,65),(180,138,106),(67,78,55),(31,33,31),(75,113,124),(116,76,55),(60,66,70),(111,76,44),(115,111,88),(126,128,117),(80,88,91),(114,94,54),(112,116,106),(119,101,67)]
def material(index,size,seed):
    rng=np.random.default_rng(seed+index*997);yy,xx=np.mgrid[:size,:size]/size
    noise=np.zeros((size,size),np.float32)
    for frequency,weight in [(2,.4),(4,.24),(8,.16),(16,.12),(32,.08)]:
        for _ in range(3):
            a=int(rng.integers(1,frequency+1));b=int(rng.integers(0,frequency+1));phase=rng.random()*6.283185
            noise+=np.cos((xx*a+yy*b)*6.283185+phase)*weight/3
    grain=rng.random((size,size),dtype=np.float32)-.5
    h=.5+noise*.5+grain*.09; variation=.82+noise*.32+grain*.16
    color=np.asarray(COLORS[index],np.float32)[None,None,:]*variation[:,:,None]
    name=NAMES[index]
    if name in ('brick','roof','roof_slate'):
        rows=yy*12;offset=(np.floor(rows)%2)*.5;vertical=np.mod(xx*6+offset,1);horizontal=np.mod(rows,1)
        mortar=(vertical<.025)|(horizontal<.055);color[mortar]=np.asarray((99,98,84)) if name=='brick' else (45,48,45);h[mortar]-=.18
    if name in ('wood_planks','weapon_wood','tree_bark','crate','fence'):
        lines=np.cos(xx*6.283185*24+noise*4)*.5+.5;color*= (.8+.2*lines[:,:,None]);h+=lines*.08
        if name in ('wood_planks','crate'):seam=np.mod(xx*6,1)<.025;color[seam]*=.4;h[seam]-=.12
    if name in ('player_uniform','opponent_uniform'):
        patches=noise>.06;color[patches]*=.64;color[noise<-.08]*=1.28;h=.5+grain*.03
    if name in ('fabric','player_uniform','opponent_uniform'):
        weave=(np.cos(xx*6.283185*128)+np.cos(yy*6.283185*128))*.025;color*=1+weave[:,:,None]
    if name=='weapon_metal':color*=.75+.25*(np.sin(yy*6.283185*64)*.5+.5)[:,:,None];h=.5+grain*.025
    if name=='skin':h=.5+grain*.012;color=np.asarray(COLORS[index])[None,None,:]*(.92+noise*.07+grain*.03)[:,:,None]
    if name=='asphalt':speckles=grain>.46;color[speckles]+=25
    if name=='leaves' or name=='leaves_dry':h+=np.sin(xx*6.283185*12+yy*6.283185*8)*.05
    return np.uint8(np.clip(color,0,255)),h

def write(path,array):
    h,w=array.shape[:2];path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'GZT1'+struct.pack('<HH',w,h)+array.tobytes())
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parent.parent/'assets/textures');parser.add_argument('--seed',type=int,default=1989);args=parser.parse_args()
    for i,name in enumerate(NAMES):
        pixels,h=material(i,512,args.seed)
        for size in (128,256,512):write(args.output/'runtime'/f'{name}_{size}.gzt',np.asarray(Image.fromarray(pixels).resize((size,size),Image.Resampling.LANCZOS)))
        heights=np.asarray(Image.fromarray(h).resize((256,256),Image.Resampling.BILINEAR))
        dx=(np.roll(heights,-1,1)-np.roll(heights,1,1))*7;dy=(np.roll(heights,-1,0)-np.roll(heights,1,0))*7
        n=np.stack((-dx,-dy,np.ones_like(dx)),axis=2);n/=np.linalg.norm(n,axis=2,keepdims=True)
        write(args.output/'normals'/f'{name}_256.gzt',np.uint8(np.clip((n*.5+.5)*255,0,255)))
    files=list((args.output/'runtime').glob('*.gzt'))+list((args.output/'normals').glob('*.gzt'))
    disk=sum(p.stat().st_size for p in files)
    def mip(n):return sum((max(1,n>>i)**2)*4 for i in range(n.bit_length()))
    resident=mip(512)*32+mip(256)*32+mip(2048)+576*484
    assert disk<=100_000_000 and resident<=100_000_000
    manifest={'generator_version':1,'seed':args.seed,'materials':NAMES,'disk_bytes':disk,'max_texture_resident_bytes':resident,'budget_bytes':100_000_000,'macro_high':2048,'files':{str(p.relative_to(args.output)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}}
    (args.output/'procedural_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'31 materials + normal maps: disk {disk/1e6:.2f} MB; maximum resident texture payload {resident/1e6:.2f} MB')
if __name__=='__main__':main()
