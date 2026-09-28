"""Read-only icon provenance/contour measurements. Prints JSON; edits no image/asset.
Run from any directory. Coordinates are ORIGINAL raster pixels, exclusive right/bottom.
"""
from pathlib import Path
from PIL import Image
import re, json, hashlib, base64

ROOT = Path(__file__).resolve().parents[3]
REF = ROOT / 'docs/drafts/2026-09-17-pro-max'
ROIS = [
    ('purifier','devices',(100,255,190,377),'purifier_44'),
    ('fan','devices',(852,252,957,374),'fan_44'),
    ('drop','devices',(102,606,186,716),'drop_44'),
    ('light','devices',(855,601,944,723),'light_44'),
    ('home','dashboard',(55,34,120,99),'home_32'),
    ('timer','dashboard',(1115,50,1157,95),'timer_20'),
    ('thermometer','dashboard',(104,717,151,799),'thermometer_32'),
    ('sliders','settings',(102,405,180,487),'sliders_24'),
    ('leaf','settings',(102,549,177,624),'leaf_24'),
    ('gear','settings',(98,685,180,766),'gear_24'),
    ('chevron_left','settings',(63,139,103,197),'chevron_left_24'),
    ('chevron_right','settings',(1483,558,1520,610),'chevron_right_20'),
    ('chevron_down','settings',(1456,282,1505,322),'chevron_down_18'),
    ('minus','auto-co2',(128,626,184,653),'minus_24'),
    ('plus','auto-co2',(671,611,729,669),'plus_24'),
    ('warning','auto-voc-invalid',(48,746,111,807),'warning_24'),
]
def box(points):
    return [min(p[0] for p in points),min(p[1] for p in points),max(p[0] for p in points)+1,max(p[1] for p in points)+1]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
code=(ROOT/'ui_icons.c').read_text(encoding='utf-8')
out={'method':'Dark reference bbox: max(R,G,B)<120; current A4 bbox: alpha>=8/15. Approximate edge alignment, not identical thresholds.', 'sources':{}, 'icons':[]}
for name,source,roi,asset in ROIS:
    p=REF/(source+'-quiet-architecture.png'); im=Image.open(p).convert('RGB')
    x1,y1,x2,y2=roi
    def is_ink(c):
        return c[1]<120 and c[0]>c[1]+20 if name=='warning' else max(c)<120
    pts=[(x,y) for y in range(y1,y2) for x in range(x1,x2) if is_ink(im.getpixel((x,y)))]
    if not pts: raise RuntimeError('Empty ROI: '+name)
    refbox=box(pts)
    desc=re.search(r'const lv_img_dsc_t ui_icon_'+asset+r'\s*=\s*\{(.*?)\};',code,re.S).group(1)
    w=int(re.search(r'\.header.w\s*=\s*(\d+)',desc).group(1)); h=int(re.search(r'\.header.h\s*=\s*(\d+)',desc).group(1))
    body=re.search(r'uint8_t ui_icon_'+asset+r'_map\[\]\s*=\s*\{(.*?)\};',code,re.S).group(1)
    data=bytes(int(v,16) for v in re.findall(r'0x([0-9a-fA-F]{2})',body))
    if len(data)!=((w+1)//2)*h: raise RuntimeError('Invalid A4 length: '+asset)
    mask=[]
    for y in range(h):
        row=[]
        for x in range(w):
            v=data[y*((w+1)//2)+x//2]; row.append((v>>4) if x%2==0 else(v&15))
        mask.append(row)
    currentbox=box([(x,y) for y in range(h) for x in range(w) if mask[y][x]>=8])
    entry={'icon':name,'source':p.name,'roi':roi,'reference_dark_bbox':refbox,'reference_selector':'G<120 and R>G+20' if name=='warning' else 'max(R,G,B)<120','asset':'ui_icon_'+asset,
           'current_canvas':[w,h],'current_ink_bbox':currentbox,'a4_base64':base64.b64encode(data).decode()}
    # Horizontal side-wall sample through 45% of purifier ink height, normalized by height.
    if name=='purifier':
        def runs(vals):
            r=[]; start=None
            for i,v in enumerate(vals+[False]):
                if v and start is None:start=i
                elif not v and start is not None:r.append(i-start); start=None
            return r
        a,b,c,d=refbox; ry=b+round((d-b)*.45)
        ref_runs=runs([max(im.getpixel((x,ry)))<120 for x in range(a,c)])
        a,b,c,d=currentbox; cy=b+round((d-b)*.45)
        native_runs=runs([mask[cy][x]>=8 for x in range(a,c)])
        entry['sidewall_probe']={'reference_y':ry,'reference_runs_px':ref_runs,'reference_height':refbox[3]-refbox[1],
          'current_y':cy,'current_runs_px':native_runs,'current_height':currentbox[3]-currentbox[1]}
    out['icons'].append(entry)
    out['sources'][p.name]={'size':im.size,'sha256':sha(p)}
for name,source,roi in [('air','dashboard',(760,251,858,342)),('check_circle','dashboard',(399,331,486,415))]:
    p=REF/(source+'-quiet-architecture.png')
    out['icons'].append({'icon':name,'source':p.name,'roi':roi,'note':'Light-on-dark source; do not use dark-pixel segmentation. Use foreground/background matting or contour trace.'})
out['implementation_sha256']={n:sha(ROOT/n) for n in ['ui_icons.c','tools/generate_assets.py','tools/create_icon_manifest.py','assets/icons/quiet-architecture/manifest.json']}
print(json.dumps(out,indent=2))
