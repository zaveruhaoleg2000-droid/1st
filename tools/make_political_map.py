#!/usr/bin/env python3
"""Generates a compact country/republic ID mask aligned to Europe heightmap.
Source: Natural Earth 110m polygons (public domain). 1989 state mergers and the
inner-German border are purposefully APPROXIMATE and not historical GIS data.
"""
from pathlib import Path
import struct, json
import numpy as np
import geopandas as gpd
from rasterio.features import rasterize
from rasterio.transform import from_bounds
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path('/opt/pyvenv/lib/python3.13/site-packages/pyogrio/tests/fixtures/naturalearth_lowres/naturalearth_lowres.shp')
if not SOURCE.exists():
    raise SystemExit('Natural Earth sample dataset not found; use a compatible 110m country shapefile.')
W,H=257,153
west,east,south,north=-20,60,27,76
transform=from_bounds(west,south,east,north,W,H)
# 0 sea, 1 NATO, 2 eastern bloc, 3 USSR, 4 neutral, 5 Yugoslavia,
# 6 Poland, 7 Ukrainian SSR, 8 Belarusian SSR, 9 Baltic SSRs,
# 10 Hungary, 11 GDR, 12 FRG, 13 CSSR, 14 Romania, 15 Bulgaria
palette={
    0:(12,32,46),1:(55,105,136),2:(120,78,86),3:(118,57,58),
    4:(104,115,99),5:(112,102,80),6:(168,120,69),7:(130,91,74),
    8:(124,80,77),9:(107,81,104),10:(153,103,92),11:(144,74,82),
    12:(53,100,130),13:(129,82,104),14:(113,80,91),15:(133,78,82)
}
NATO={'GBR','FRA','ITA','ESP','PRT','GRC','TUR','NOR','DNK','ISL','NLD','BEL','LUX'}
USSR={'RUS','MDA','GEO','ARM','AZE','KAZ','UZB','KGZ','TJK','TKM'}
YUG={'HRV','BIH','SRB','MNE','MKD','SVN'}
def polity(code,name):
    if code in NATO or name in ('France','Norway'):return 1
    if code in USSR:return 3
    if code=='UKR':return 7
    if code=='BLR':return 8
    if code in {'LTU','LVA','EST'}:return 9
    if code=='POL':return 6
    if code=='HUN':return 10
    if code in {'SVK','CZE'}:return 13
    if code=='ROU':return 14
    if code=='BGR':return 15
    if code in YUG or name=='Kosovo':return 5
    if code=='DEU':return 12
    return 4

df=gpd.read_file(SOURCE)
shapes=[(row.geometry,polity(row.iso_a3,row['name'])) for _,row in df.iterrows() if row.geometry is not None]
mask=rasterize(shapes,out_shape=(H,W),transform=transform,fill=0,all_touched=False,dtype=np.uint8)
# Approximation of the FRG/GDR boundary as a polyline in historical mainland Germany.
# Avoid representing the DDR as the modern German Bundeslaender exactly.
from shapely.geometry import LineString,Polygon
from shapely.ops import split
DEU=df[df.iso_a3=='DEU'].geometry.iloc[0]
line=LineString([(10.75,55),(10.8,53.9),(10.63,53.42),(11.1,52.8),(11.18,52.2),(10.62,51.6),(10.2,51.13),(10.5,50.55),(11.3,50.40),(12.3,50.3),(12.5,48.5)])
parts=split(DEU,line)
eastpieces=[p for p in parts.geoms if p.representative_point().x>11.1]
if eastpieces:
    DDR=rasterize([(g,11) for g in eastpieces],out_shape=(H,W),transform=transform,fill=0,dtype=np.uint8)
    mask[DDR==11]=11
# Soviet republics are separately shaded for narrative reasons but still Soviet territory.
raw=ROOT/'assets/politics/political_1989.gzp'
raw.write_bytes(b'GZP1'+struct.pack('<HH',W,H)+mask.tobytes())
colors=np.array([palette[i] for i in range(16)],np.uint8)
img=colors[mask]
# border contrast in visualization (the actual engine renders borders separately)
b=np.zeros_like(mask,dtype=bool)
b[:,1:] |= (mask[:,1:]!=mask[:,:-1])&(mask[:,1:]!=0)&(mask[:,:-1]!=0)
b[1:,:] |= (mask[1:,:]!=mask[:-1,:])&(mask[1:,:]!=0)&(mask[:-1,:]!=0)
img[b]=np.array([228,206,169],np.uint8)
Image.fromarray(img,'RGB').resize((1028,612),Image.Resampling.NEAREST).save(ROOT/'assets/politics/political_mask_preview.png')
info={'source':'Natural Earth 1:110m, public-domain country polygons (modern, modified for game)', 'date':'alternate 1989', 'geographic_accuracy':'approximate, especially East-West German border, Soviet republics, Yugoslavia', 'size':[W,H], 'lon_range':[west,east], 'lat_range':[south,north], 'palette':{str(k):v for k,v in palette.items()}}
(ROOT/'assets/politics/metadata.json').write_text(json.dumps(info,ensure_ascii=False,indent=2),encoding='utf8')
print('Wrote',raw,raw.stat().st_size,'bytes; regions',sorted(set(mask.flatten())))
