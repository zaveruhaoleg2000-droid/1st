#!/usr/bin/env python3
"""Deterministic, fully offline, tileable texture library for Gray Zone: 1989.

Requires: numpy, Pillow. No internet downloads / external assets.
Generates a selection of material albedo PNGs at 128/256/512/1024,
512px normal+roughness maps, and compact GZT1 byte-RGB runtime textures.
"""
import argparse
import json
import math
import struct
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

SIZE = 1024
LEVELS = (128, 256, 512, 1024)
MATS = [
    ('grass_short', 'Коротка трава', (63, 93, 48), 0.85, 4.0, 'grass'),
    ('grass_tall',  'Висока трава',  (80, 101, 51), 0.90, 5.0, 'grass_tall'),
    ('meadow',      'Луг',          (86, 108, 55), 0.85, 3.5, 'meadow'),
    ('dirt_dry',    'Суха земля',   (115, 93, 65), 0.93, 4.0, 'earth'),
    ('mud_wet',     'Мокрий ґрунт',(66, 59, 47), 0.68, 3.0, 'mud'),
    ('sand',        'Пісок',        (171, 153, 114),0.95, 3.0, 'sand'),
    ('gravel',      'Гравій',       (116, 113, 105),0.93,4.0, 'gravel'),
    ('rock',        'Камінь',       (107, 111, 107),0.88,4.0, 'rock'),
    ('asphalt',     'Асфальт',      (66, 68, 67),  0.87,3.0, 'asphalt'),
    ('concrete',    'Бетон',        (145, 143, 132),0.91,2.0, 'concrete'),
    ('brick',       'Цегляна стіна',(125, 72, 55),0.83,4.0, 'brick'),
    ('wood_planks', 'Дошки',       (129, 93, 59), 0.72,3.0, 'planks'),
    ('tree_bark',   'Кора дерева',  (78, 71, 56), 0.93,3.0, 'bark'),
    ('leaves',      'Листя',        (49, 86, 40), 0.83,3.0, 'leaves'),
    ('water',       'Вода',         (45, 98, 117),0.28,1.5, 'water'),
    ('snow',        'Сніг',         (206, 214, 214),0.80,1.8, 'snow'),
]

def periodic_value_noise(n, cells, rng):
    # Toroidal/cyclic bilinear interpolation (last edge samples converge to the first).
    grid = rng.random((cells, cells)).astype(np.float32)
    co = np.arange(n, dtype=np.float32) * (cells / float(n))
    i = np.floor(co).astype(np.int32)
    t = co - i
    t = t*t*(3-2*t)
    i1 = (i+1)%cells
    x0 = grid[i[:, None], i[None, :]]
    x1 = grid[i1[:, None], i[None, :]]
    x2 = grid[i[:, None], i1[None, :]]
    x3 = grid[i1[:, None], i1[None, :]]
    a = x0 + (x1-x0)*t[:, None]
    b = x2 + (x3-x2)*t[:, None]
    return (a + (b-a)*t[None, :]) * 2.0 - 1.0


def generate_material(name, base, kind):
    rng = np.random.default_rng(sum((i+1)*ord(c) for i,c in enumerate(name)) + 1989)
    n = SIZE
    # x/y coordinate arrays for material-specific world-periodic patterns.
    Y, X = np.meshgrid(np.arange(n, dtype=np.float32), np.arange(n, dtype=np.float32), indexing='ij')
    n1 = periodic_value_noise(n, 5, rng)
    n2 = periodic_value_noise(n, 13, rng)
    n3 = periodic_value_noise(n, 34, rng)
    n4 = periodic_value_noise(n, 92, rng)
    n5 = rng.normal(0, 1, (n, n)).astype(np.float32)
    f = n1*.42 + n2*.31 + n3*.18 + n4*.09
    relief = f*.48 + n3*.11 + n4*.08 + n5*.025
    shade = f*43 + n3*10 + n4*5 + n5*7
    tint = np.array(base, np.float32)
    im = tint[None,None,:] + shade[...,None] * np.array([1.03,1.03,.89],np.float32)
    roughness = np.clip(.5+f*.09,0,1)
    if kind in ('grass','grass_tall','meadow'):
        blade = np.sin(2*np.pi*(X/23 + f*.55)) * np.sin(2*np.pi*(Y/85+n2*.35))
        im += blade[...,None] * np.array([5,11,2],np.float32)
        highlights = np.maximum(0,n4*.7 + n5*.28 - .26)
        im += highlights[...,None] * np.array([21,31,8],np.float32)
        if kind=='grass_tall':
            im += (n2*.15+n3*.10)[...,None] * np.array([22,25,-4])
        if kind=='meadow':
            flowers=((n4>.68)&(n5>.5)).astype(np.float32)
            im += flowers[...,None]*np.array([60,38,22])
        relief += .06*blade
    elif kind == 'earth':
        grains=np.maximum(0,n5-.5)
        im += grains[...,None]*np.array([14,12,9])
        relief += n3*.17
    elif kind == 'mud':
        wet=np.clip((f+.22)*2.4,0,1)
        im -= wet[...,None]*np.array([21,16,11])
        im += np.maximum(0,n3-.4)[...,None]*np.array([9,12,9])
        relief += f*.22
    elif kind == 'sand':
        ripple=np.sin(2*np.pi*(Y/30 + n1*1.0+n2*.16))
        im += ripple[...,None]*np.array([6.3,6.0,4.5])
        relief += ripple*.13
    elif kind == 'gravel':
        p = np.maximum(0, np.sin(X*.39+n4*2.)*np.cos(Y*.43+n3*2.))
        st = (p>.63).astype(np.float32)
        im += st[...,None]*(10+25*n3[...,None]);relief += st*.27
    elif kind == 'rock':
        veins = np.sin(2*np.pi*(X/218 + Y/340 + n1*.18+n2*.08))
        cracks=np.exp(-((np.abs(veins))/.065)**2)
        im -= cracks[...,None]*np.array([25,25,24])
        im += (np.maximum(0,n3-.15)*15)[...,None]
        relief -= cracks*.23
    elif kind == 'asphalt':
        speckle=np.maximum(0,n5-.60)
        im += speckle[...,None]*np.array([19,18,17])
        im -= np.maximum(0, -.3-n3)[...,None]*6
        relief += speckle*.07
    elif kind == 'concrete':
        # 2x2m slab seams, perfectly periodic across repeats.
        seam=np.minimum.reduce([X%512,511-(X%512),Y%512,511-(Y%512)])
        mortar=np.clip((6-seam)/5,0,1)
        im -= mortar[...,None]*np.array([29,28,26]);relief -= mortar*.2
        im += n3[...,None]*6
    elif kind == 'brick':
        brickH=128;brickW=256
        row=np.floor(Y/brickH)
        yy=Y%brickH
        xx=(X+np.mod(row,2)*128)%brickW
        seam=np.minimum.reduce([xx,brickW-1-xx,yy,brickH-1-yy])
        mortar=np.clip((8-seam)/6,0,1)
        im -= mortar[...,None]*np.array([47,20,16]);relief -= mortar*.43
        brickvariation=periodic_value_noise(n,8,rng)
        im += brickvariation[...,None]*np.array([13,5,4])
    elif kind == 'planks':
        # Four horizontal boards with darker joins and wood rings.
        seam=np.minimum(Y%256,255-Y%256)
        join=np.clip((7-seam)/6,0,1)
        ring=np.sin(2*np.pi*(Y/29.0+n1*.50+n2*.24))*.35 + np.sin(2*np.pi*(Y/9.3+n3*.13))*.10
        im += ring[...,None]*np.array([25,17,8]); im -= join[...,None]*np.array([49,37,26])
        relief += ring*.23-join*.28
        knots = (n2>.66)&(n3>.4)
        im -= knots[...,None]*np.array([12,7,5])
    elif kind == 'bark':
        ridges=np.sin(2*np.pi*(X/32+n1*.22+n2*.1))
        cracks=np.exp(-((np.abs(ridges))/.095)**2)
        im -= cracks[...,None]*np.array([27,21,17]);relief -= cracks*.4
        im += np.maximum(0,ridges)[...,None]*np.array([11,9,6])
    elif kind == 'leaves':
        spots=np.maximum(0, n3*.55+n4*.3+n5*.1)
        im += spots[...,None]*np.array([36,52,16])
        im -= np.maximum(0,-n3-.18)[...,None]*np.array([15,20,11])
        relief += spots*.12
    elif kind == 'water':
        waves=np.sin(2*np.pi*(Y/53+X/160 + n1*.55))*.6+np.sin(2*np.pi*(Y/17-X/105+n2*.25))*.3
        im += waves[...,None]*np.array([10,19,23])
        glimmer=np.maximum(0,waves-.6)**2
        im += glimmer[...,None]*np.array([31,41,42]);relief += waves*.07
    elif kind == 'snow':
        crust=np.maximum(0,n3-.2)
        im -= crust[...,None]*np.array([12,12,7]);relief+=crust*.13
        im += n4[...,None]*3
    albedo=np.uint8(np.clip(im,0,255))
    # Enforce byte-exact edges for any mip level and atlas export.
    albedo[-1]=albedo[0];albedo[:,-1]=albedo[:,0]
    relief[-1]=relief[0];relief[:,-1]=relief[:,0]
    return albedo, relief


def export(root):
    root.mkdir(parents=True,exist_ok=True)
    (root/'runtime').mkdir(exist_ok=True)
    items=[]
    for index,(name,title,rgb,rough,tex_m,kind) in enumerate(MATS):
        albedo, relief=generate_material(name,rgb,kind)
        path=root/name
        path.mkdir(exist_ok=True)
        img=Image.fromarray(albedo, 'RGB')
        for size in LEVELS:
            out=img.resize((size,size),Image.Resampling.LANCZOS) if size!=SIZE else img.copy()
            arr=np.asarray(out).copy()
            # exact seam at every output resolution, not merely original 1024.
            arr[-1]=arr[0];arr[:,-1]=arr[:,0]
            out=Image.fromarray(arr,'RGB')
            out.save(path/f'{name}_{size}.png',optimize=True)
            if size in (128,256,512):
                # Runtime format: 'GZT1' + uint16 width,height + 3 raw RGB bytes/pixel.
                (root/'runtime'/f'{name}_{size}.gzt').write_bytes(b'GZT1'+struct.pack('<HH',size,size)+arr.tobytes())
        # Derive tangent-space normal from cyclic derivatives of the uncolored bump.
        grad_x=(np.roll(relief,-2,axis=1)-np.roll(relief,2,axis=1))*.75
        grad_y=(np.roll(relief,-2,axis=0)-np.roll(relief,2,axis=0))*.75
        inv=np.sqrt(grad_x**2+grad_y**2+1)
        norm=np.uint8(np.clip(np.stack(((1-grad_x/inv)*127.5,(1-grad_y/inv)*127.5,(1/inv+1)*127.5),axis=-1),0,255))
        roughmap=np.uint8(np.clip(rough*255 + relief*20,0,255))
        for res in (256,512):
            nimg=Image.fromarray(norm,'RGB').resize((res,res),Image.Resampling.BILINEAR)
            rimg=Image.fromarray(roughmap,'L').resize((res,res),Image.Resampling.BILINEAR)
            for fname,imgx in [(f'{name}_normal_{res}.png',nimg),(f'{name}_roughness_{res}.png',rimg)]:
                aa=np.asarray(imgx).copy()
                aa[-1]=aa[0];aa[:,-1]=aa[:,0]
                Image.fromarray(aa,imgx.mode).save(path/fname,optimize=True)
        items.append({'id':name,'uk':title,'base_color':list(rgb),'tile_world_m':tex_m,
                      'seamless':True,'levels':list(LEVELS),'normal_levels':[256,512],
                      'roughness_levels':[256,512],'runtime_levels':[128,256,512],
                      'albedo_pattern':f'{name}/{name}_{{size}}.png',
                      'normal_pattern':f'{name}/{name}_normal_{{size}}.png',
                      'roughness_pattern':f'{name}/{name}_roughness_{{size}}.png'})
        print(f'{index+1:02d}/{len(MATS)} {name}',flush=True)
    (root/'manifest.json').write_text(json.dumps({'format':'GZ-TEXTURES-1','version':'0.0.8','generated':'procedural, no downloaded assets',
        'materials':items},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    make_contact_sheet(root,items)


def make_contact_sheet(root,items):
    W,H=1440, ((len(items)+3)//4)*330+125
    contact=Image.new('RGB',(W,H),(24,29,33))
    d=ImageDraw.Draw(contact)
    d.text((27,27),'GRAY ZONE: 1989  |  MATERIAL LIBRARY  v0.0.8',fill=(225,225,216))
    d.text((27,62),'16 tileable materials  /  128  256  512  1024 px  /  albedo + normal + roughness',fill=(162,176,182))
    for i,entry in enumerate(items):
        x=26+(i%4)*355;y=109+(i//4)*330
        sample=Image.open(root/entry['id']/f"{entry['id']}_512.png")
        sample=sample.resize((310,275),Image.Resampling.LANCZOS)
        contact.paste(sample,(x,y))
        d.rectangle((x,y,x+309,y+274),outline=(100,108,110),width=1)
        d.rectangle((x,y+250,x+309,y+274),fill=(18,21,23))
        d.text((x+9,y+254),entry['id'].upper(),fill=(237,238,218))
        d.text((x+5,y+282),f"128 / 256 / 512 / 1024    |    {entry['tile_world_m']} m",fill=(190,197,202))
    contact.save(root/'CONTACT_SHEET.png',optimize=True)

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',default='assets/textures')
    opts=ap.parse_args();export(Path(opts.output))
