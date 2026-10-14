"""Bake an antialiased, proportional Ukrainian font atlas (no runtime dependency)."""
from pathlib import Path
import re, codecs, shutil
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parent.parent
source=(root/'include/gz/Glyphs.hpp').read_text()
glyphs=bytes(re.search(r'kMenuGlyphs\[\] = U"(.*)";',source).group(1),'utf8').decode('unicode_escape').encode('latin1').decode('utf8')
font_path=Path('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf')
font=ImageFont.truetype(str(font_path),32)
w,h=576,((len(glyphs)+15)//16)*44
atlas=Image.new('L',(w,h));draw=ImageDraw.Draw(atlas);advances=[]
for i,ch in enumerate(glyphs):
    x=(i%16)*36;y=(i//16)*44
    draw.text((x+1,y+4),ch,font=font,fill=255,stroke_width=0)
    advances.append(min(36,max(6,round(font.getlength(ch))+1)))
out=root/'assets/ui';(out/'glyphs.pgm').write_bytes(f'P5\n{w} {h}\n255\n'.encode()+atlas.tobytes())
(out/'advances.bin').write_bytes(bytes(advances))
license=Path('/usr/share/doc/fonts-dejavu-core/copyright')
if license.exists():shutil.copy2(license,out/'FONT_LICENSE.txt')
print(f'{len(glyphs)} proportional glyphs, {w} x {h}')
