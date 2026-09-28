"""Read-only quantitative review of references, native captures and checked-in A4 data.
Print JSON; does not regenerate or edit assets. Coordinates are 800x480 logical.
"""
from pathlib import Path
from PIL import Image, ImageStat
import re, json, hashlib

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
REF = ROOT / 'docs/drafts/2026-09-17-pro-max'
def rgb(h): return tuple(bytes.fromhex(h.lstrip('#')))
def hx(c): return '#' + ''.join(f'{int(v):02X}' for v in c)
def q(c):
    r,g,b = c
    r >>= 3; g >>= 2; b >>= 3
    return ((r<<3)|(r>>2), (g<<2)|(g>>4), (b<<3)|(b>>2))
def patch(im, box):
    w,h = im.size
    return im.crop(tuple(round(v * (w/800 if i%2 == 0 else h/480)) for i,v in enumerate(box)))
def stats(im, box):
    s = ImageStat.Stat(patch(im,box))
    return {'median':hx(s.median),'mean':[round(x,2) for x in s.mean], 'stddev':[round(x,2) for x in s.stddev]}
def luminance(c):
    s = [x/255 for x in c]
    l = [x/12.92 if x <= .04045 else ((x+.055)/1.055)**2.4 for x in s]
    return .2126*l[0]+.7152*l[1]+.0722*l[2]
def contrast(a,b):
    x,y=sorted([luminance(a),luminance(b)])
    return round((y+.05)/(x+.05),2)

tokens = dict(re.findall(r'#define UI_COLOR_(\w+)\s+lv_color_hex\(0x([0-9A-Fa-f]{6})\)',(ROOT/'ui_theme.h').read_text()))
out = {'tokens':{k:{'input':'#'+v,'decoded_565':hx(q(rgb(v)))} for k,v in tokens.items()}}
ref=Image.open(REF/'dashboard-quiet-architecture.png').convert('RGB')
native=Image.open(HERE/'home_good.png').convert('RGB')
regions = {'canvas':((345,10,515,80),(345,10,480,50)),
           'surface':((610,435,735,448),(610,435,735,448)),
           'forest_top':((235,115,365,135),(235,120,365,138)),
           'forest_bottom':((55,318,210,326),(55,316,210,324)),
           'celadon':((55,65,110,70),(50,65,100,68))}
out['patches']={k:{'reference_box':a,'native_box':b,'reference':stats(ref,a),'native':stats(native,b)} for k,(a,b) in regions.items()}
out['quantization_only']={}
for p in REF.glob('*quiet-architecture.png'):
    im=Image.open(p).convert('RGB')
    err=[0,0,0]; n=im.width*im.height
    for c in im.getdata():
        d=q(c)
        for i in range(3): err[i]+=abs(c[i]-d[i])
    out['quantization_only'][p.name]={'source_size':im.size,'channel_MAE_8bit':[round(x/n,3) for x in err]}
out['contrast'] = {f'{a}/{b}':contrast(q(rgb(tokens[a])),q(rgb(tokens[b]))) for a,b in [('INK','SURFACE'),('MUTED','SURFACE'),('MUTED','FOREST'),('CELADON','FOREST')]}
out['contrast']['WHITE/SURFACE']=contrast((255,255,255),q(rgb(tokens['SURFACE'])))
code=(ROOT/'ui_icons.c').read_text()
out['icons']={}
for name,body in re.findall(r'uint8_t (ui_icon_\w+)_map\[\] = \{(.*?)\};',code,re.S):
    size=int(name.rsplit('_',1)[1]); data=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',body))
    pts=[]; pixels=[]
    for y in range(size):
        for x in range(size):
            v=data[y*((size+1)//2)+x//2]; a=(v>>4) if x%2==0 else (v&15)
            if a>=8: pts.append((x,y))
            pixels.append(a)
    box=[min(p[0] for p in pts),min(p[1] for p in pts),max(p[0] for p in pts)+1,max(p[1] for p in pts)+1] if pts else [0]*4
    out['icons'][name]={'canvas':size,'bytes':len(data),'ink_box_alpha_ge_8':box,'ink_size':[box[2]-box[0],box[3]-box[1]]}
out['icon_count']=len(out['icons'])
devref=Image.open(REF/'devices-quiet-architecture.png').convert('RGB')
out['reference_icon_ink']={}
for name,box in [('purifier',(46,124,98,187)),('fan',(422,124,476,183)),('light',(420,296,474,359))]:
    crop=patch(devref,box)
    pts=[(x,y) for y in range(crop.height) for x in range(crop.width) if max(crop.getpixel((x,y)))<120]
    out['reference_icon_ink'][name]={'logical_crop':box,'threshold':'all RGB channels <120', 'ink_size_at_800':[round((max(p[i] for p in pts)-min(p[i] for p in pts)+1)/(devref.size[i]/(800 if i==0 else 480)),1) for i in (0,1)]}
out['source_sha256']={str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest().upper() for p in [ROOT/n for n in ['ui.c','ui_theme.h','ui_theme.c','ui_fonts.c','ui_icons.c','tools/generate_assets.py','tools/PlusJakartaSans.ttf','sim_pc/main.c','tools/verify_production.ps1','sim_pc/build/sim_pc.exe']]}
print(json.dumps(out,indent=2))
