#!/usr/bin/env python3
"""Builds assets/icons/quiet-architecture/masters/*.png and manifest.json
from the authoritative 1619x971 concept PNGs in docs/drafts/2026-09-17-pro-max/.
"""
import os
import sys
import json
import hashlib
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
REF = ROOT / "docs/drafts/2026-09-17-pro-max"
MASTERS_DIR = ROOT / "assets/icons/quiet-architecture/masters"
MANIFEST_FILE = ROOT / "assets/icons/quiet-architecture/manifest.json"

MASTERS_DIR.mkdir(parents=True, exist_ok=True)

SOURCE_METADATA = {
    "devices-quiet-architecture.png": {
        "size": [1619, 971],
        "sha256": "62b5011a2783219a00f482a6c1a6bd6011c49a1f03928de9c60843cd69462ea8"
    },
    "dashboard-quiet-architecture.png": {
        "size": [1619, 971],
        "sha256": "a990b5747a9b4bddbae2151c7fe062463ca4951c86919e95c6f089aae84f4bd4"
    },
    "settings-quiet-architecture.png": {
        "size": [1619, 971],
        "sha256": "4645b359d2aba3c21af33361ad5a873c47faa2599d652e525c1881797a20a0b1"
    },
    "auto-co2-quiet-architecture.png": {
        "size": [1619, 971],
        "sha256": "09f0a227f2dab6f0324fdb7c9eb7bdbde64dc49e2a1916d449e19db2bcbc7fbe"
    },
    "auto-voc-invalid-quiet-architecture.png": {
        "size": [1619, 971],
        "sha256": "756b0c0cca236c5c7d606cb43c32ff8afeb9beb1dba1b91e706719246342c3f5"
    }
}

PRIMARY_ROIS = {
    'purifier': {
        'source': 'devices-quiet-architecture.png',
        'roi': [100, 255, 190, 377],
        'type': 'dark_ink',
        'notes': 'Canonical 1-row 5-perforation purifier tower with curved slit and top oval'
    },
    'fan': {
        'source': 'devices-quiet-architecture.png',
        'roi': [852, 252, 957, 374],
        'type': 'dark_ink',
        'notes': 'Canonical 4-paddle fan on central hollow circular hub with aspect ratio 0.838'
    },
    'drop': {
        'source': 'devices-quiet-architecture.png',
        'roi': [102, 606, 186, 716],
        'type': 'dark_ink',
        'notes': 'Canonical svelte liquid teardrop with smooth contour and circular bulb'
    },
    'light': {
        'source': 'devices-quiet-architecture.png',
        'roi': [855, 601, 944, 723],
        'type': 'dark_ink',
        'notes': 'Canonical architectural task lamp with hollow pedestal base and conical shade'
    },
    'home': {
        'source': 'dashboard-quiet-architecture.png',
        'roi': [55, 34, 120, 99],
        'type': 'dark_ink',
        'notes': 'Pentagonal house silhouette with solid dark-filled rectangular doorway'
    },
    'timer': {
        'source': 'dashboard-quiet-architecture.png',
        'roi': [1115, 50, 1157, 95],
        'type': 'dark_ink',
        'notes': 'Stopwatch with top buttons and vertical hand'
    },
    'thermometer': {
        'source': 'dashboard-quiet-architecture.png',
        'roi': [104, 717, 151, 799],
        'type': 'dark_ink',
        'notes': 'Thermometer capsule with inner mercury line and filled bottom bulb'
    },
    'sliders': {
        'source': 'settings-quiet-architecture.png',
        'roi': [102, 405, 180, 487],
        'type': 'dark_ink',
        'notes': 'Three adjustment track sliders with circular knobs and hollow centers'
    },
    'leaf': {
        'source': 'settings-quiet-architecture.png',
        'roi': [102, 549, 177, 624],
        'type': 'dark_ink',
        'notes': 'Natural organic leaf with base stem and curved central vein'
    },
    'gear': {
        'source': 'settings-quiet-architecture.png',
        'roi': [98, 685, 180, 766],
        'type': 'dark_ink',
        'notes': '8-tooth mechanical gear with central hollow circle'
    },
    'chevron_left': {
        'source': 'settings-quiet-architecture.png',
        'roi': [63, 139, 103, 197],
        'type': 'dark_ink',
        'notes': 'Left-pointing back chevron'
    },
    'chevron_right': {
        'source': 'settings-quiet-architecture.png',
        'roi': [1483, 558, 1520, 610],
        'type': 'dark_ink',
        'notes': 'Right-pointing forward chevron'
    },
    'chevron_down': {
        'source': 'settings-quiet-architecture.png',
        'roi': [1456, 282, 1505, 322],
        'type': 'dark_ink',
        'notes': 'Downward-pointing dropdown chevron'
    },
    'minus': {
        'source': 'auto-co2-quiet-architecture.png',
        'roi': [128, 626, 184, 653],
        'type': 'dark_ink',
        'notes': 'Horizontal minus stepper bar'
    },
    'plus': {
        'source': 'auto-co2-quiet-architecture.png',
        'roi': [671, 611, 729, 669],
        'type': 'dark_ink',
        'notes': 'Centered plus stepper cross'
    },
    'warning': {
        'source': 'auto-voc-invalid-quiet-architecture.png',
        'roi': [48, 746, 111, 807],
        'type': 'warning_red',
        'notes': 'Equilateral rounded warning triangle with centered exclamation mark'
    },
    'air': {
        'source': 'dashboard-quiet-architecture.png',
        'roi': [760, 251, 858, 342],
        'type': 'light_on_dark',
        'notes': 'Three flowing air wind streams on dark hero card'
    },
    'check_circle': {
        'source': 'dashboard-quiet-architecture.png',
        'roi': [399, 331, 486, 415],
        'type': 'check_badge',
        'notes': 'Celadon circular badge with dark checkmark cutout'
    }
}

EXTENSION_ICONS = {
    'chart': 'Documented extension: trend line chart with axes and series curve',
    'devices': 'Documented extension: device card rectangle with content lines',
    'radio': 'Documented extension: 4-bar stepped radio RSSI indicator',
    'plug': 'Documented extension: electrical plug outline',
    'heat': 'Documented extension: heater coil arches',
    'close': 'Documented extension: modal close X',
    'check': 'Documented extension: checkmark with circular outline'
}

def sha256_file(p):
    with open(p, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()

def extract_dark_ink(im):
    w, h = im.size
    pix = im.load()
    # Sample clean background perimeter pixels
    bg_samples = []
    margin = 0.08
    for x in range(w):
        for y in range(int(h * margin)):
            if min(pix[x, y]) > 180: bg_samples.append(pix[x, y])
        for y in range(int(h * (1 - margin)), h):
            if min(pix[x, y]) > 180: bg_samples.append(pix[x, y])
    for y in range(h):
        for x in range(int(w * margin)):
            if min(pix[x, y]) > 180: bg_samples.append(pix[x, y])
        for x in range(int(w * (1 - margin)), w):
            if min(pix[x, y]) > 180: bg_samples.append(pix[x, y])
            
    if not bg_samples:
        bg_samples = [pix[0, 0], pix[w-1, 0], pix[0, h-1], pix[w-1, h-1]]
        
    B = [sum(p[i] for p in bg_samples) / len(bg_samples) for i in range(3)]
    
    # Estimate foreground from lowest 3% luminance
    all_pix = [pix[x, y] for x in range(w) for y in range(h)]
    sorted_by_lum = sorted(all_pix, key=lambda c: c[0]*0.299 + c[1]*0.587 + c[2]*0.114)
    core_count = max(10, int(len(sorted_by_lum) * 0.03))
    F = [sum(p[i] for p in sorted_by_lum[:core_count]) / core_count for i in range(3)]
    
    D = [F[i] - B[i] for i in range(3)]
    len_D_sq = sum(d * d for d in D)
    if len_D_sq < 1e-6: len_D_sq = 1.0
    
    alpha = Image.new("L", (w, h), 0)
    apix = alpha.load()
    for y in range(h):
        for x in range(w):
            c = pix[x, y]
            dot = sum((c[i] - B[i]) * D[i] for i in range(3))
            proj = dot / len_D_sq
            if proj < 0.05:
                val = 0
            elif proj > 0.98:
                val = 255
            else:
                val = int(round(min(1.0, max(0.0, (proj - 0.05) / (0.98 - 0.05))) * 255.0))
            apix[x, y] = val
    return alpha

def extract_light_on_dark(im):
    w, h = im.size
    pix = im.load()
    bg_samples = []
    margin = 0.10
    for x in range(w):
        for y in range(int(h * margin)): bg_samples.append(pix[x, y])
        for y in range(int(h * (1 - margin)), h): bg_samples.append(pix[x, y])
    for y in range(h):
        for x in range(int(w * margin)): bg_samples.append(pix[x, y])
        for x in range(int(w * (1 - margin)), w): bg_samples.append(pix[x, y])
        
    B = [sum(p[i] for p in bg_samples) / len(bg_samples) for i in range(3)]
    all_pix = [pix[x, y] for x in range(w) for y in range(h)]
    sorted_by_lum = sorted(all_pix, key=lambda c: c[0]*0.299 + c[1]*0.587 + c[2]*0.114, reverse=True)
    core_count = max(10, int(len(sorted_by_lum) * 0.03))
    F = [sum(p[i] for p in sorted_by_lum[:core_count]) / core_count for i in range(3)]
    
    D = [F[i] - B[i] for i in range(3)]
    len_D_sq = sum(d * d for d in D)
    if len_D_sq < 1e-6: len_D_sq = 1.0
    
    alpha = Image.new("L", (w, h), 0)
    apix = alpha.load()
    for y in range(h):
        for x in range(w):
            c = pix[x, y]
            dot = sum((c[i] - B[i]) * D[i] for i in range(3))
            proj = dot / len_D_sq
            if proj < 0.08:
                val = 0
            elif proj > 0.95:
                val = 255
            else:
                val = int(round(min(1.0, max(0.0, (proj - 0.08) / (0.95 - 0.08))) * 255.0))
            apix[x, y] = val
    return alpha

def extract_check_badge(im):
    w, h = im.size
    pix = im.load()
    bg_samples = []
    fg_samples = []
    for y in range(h):
        for x in range(w):
            c = pix[x, y]
            lum = c[0]*0.299 + c[1]*0.587 + c[2]*0.114
            if lum < 60: bg_samples.append(c)
            elif lum > 180: fg_samples.append(c)
            
    B = [sum(p[i] for p in bg_samples) / len(bg_samples) for i in range(3)]
    F = [sum(p[i] for p in fg_samples) / len(fg_samples) for i in range(3)]
    D = [F[i] - B[i] for i in range(3)]
    len_D_sq = sum(d * d for d in D)
    if len_D_sq < 1e-6: len_D_sq = 1.0
    
    alpha = Image.new("L", (w, h), 0)
    apix = alpha.load()
    for y in range(h):
        for x in range(w):
            c = pix[x, y]
            dot = sum((c[i] - B[i]) * D[i] for i in range(3))
            proj = dot / len_D_sq
            if proj < 0.15:
                val = 0
            elif proj > 0.90:
                val = 255
            else:
                val = int(round(min(1.0, max(0.0, (proj - 0.15) / (0.90 - 0.15))) * 255.0))
            apix[x, y] = val
    return alpha

def extract_warning(im):
    w, h = im.size
    pix = im.load()
    bg_samples = []
    fg_samples = []
    for y in range(h):
        for x in range(w):
            c = pix[x, y]
            if min(c) > 200:
                bg_samples.append(c)
            elif c[0] > c[1] + 40 and c[0] > c[2] + 40:
                fg_samples.append(c)
                
    B = [sum(p[i] for p in bg_samples) / len(bg_samples) for i in range(3)]
    F = [sum(p[i] for p in fg_samples) / len(fg_samples) for i in range(3)]
    D = [F[i] - B[i] for i in range(3)]
    len_D_sq = sum(d * d for d in D)
    if len_D_sq < 1e-6: len_D_sq = 1.0
    
    alpha = Image.new("L", (w, h), 0)
    apix = alpha.load()
    for y in range(h):
        for x in range(w):
            c = pix[x, y]
            dot = sum((c[i] - B[i]) * D[i] for i in range(3))
            proj = dot / len_D_sq
            if proj < 0.12:
                val = 0
            elif proj > 0.90:
                val = 255
            else:
                val = int(round(min(1.0, max(0.0, (proj - 0.12) / (0.90 - 0.12))) * 255.0))
            apix[x, y] = val
    return alpha

def render_extension_master(key, size=128):
    # Render clean anti-aliased master alpha mask at 128x128 for extensions
    img = Image.new("L", (size * 4, size * 4), 0)
    draw = ImageDraw.Draw(img)
    scale = size * 4 / 24.0
    stroke_w = int(round(1.8 * scale))
    
    if key == 'chart':
        # Axis lines and curve
        p = [(int(3*scale), int(3*scale)), (int(3*scale), int(21*scale)), (int(21*scale), int(21*scale))]
        draw.line(p, fill=255, width=stroke_w)
        curve = [(int(7*scale), int(14*scale)), (int(11*scale), int(9*scale)), (int(15*scale), int(12*scale)), (int(21*scale), int(5*scale))]
        draw.line(curve, fill=255, width=stroke_w)
    elif key == 'devices':
        # Rounded rectangle with screen lines
        draw.rounded_rectangle([int(4*scale), int(3*scale), int(20*scale), int(21*scale)], radius=int(3*scale), outline=255, width=stroke_w)
        for y_c in [7, 11, 15]:
            draw.line([(int(8*scale), int(y_c*scale)), (int(16*scale), int(y_c*scale))], fill=255, width=stroke_w)
    elif key == 'radio':
        # 4 stepped bars
        bars = [(3.5, 16.5, 2.8, 4.0), (8.5, 12.5, 2.8, 8.0), (13.5, 8.5, 2.8, 12.0), (18.5, 4.5, 2.8, 16.0)]
        for bx, by, bw, bh in bars:
            draw.rounded_rectangle([int(bx*scale), int(by*scale), int((bx+bw)*scale), int((by+bh)*scale)], radius=int(1.0*scale), fill=255)
    elif key == 'plug':
        draw.line([(int(9*scale), int(2*scale)), (int(9*scale), int(7*scale))], fill=255, width=stroke_w)
        draw.line([(int(15*scale), int(2*scale)), (int(15*scale), int(7*scale))], fill=255, width=stroke_w)
        draw.rounded_rectangle([int(7*scale), int(7*scale), int(17*scale), int(15*scale)], radius=int(3*scale), outline=255, width=stroke_w)
        draw.line([(int(12*scale), int(16*scale)), (int(12*scale), int(22*scale))], fill=255, width=stroke_w)
    elif key == 'heat':
        draw.arc([int(5*scale), int(2*scale), int(19*scale), int(16*scale)], start=180, end=0, fill=255, width=stroke_w)
        draw.line([(int(5*scale), int(9*scale)), (int(5*scale), int(19*scale))], fill=255, width=stroke_w)
        draw.line([(int(19*scale), int(9*scale)), (int(19*scale), int(19*scale))], fill=255, width=stroke_w)
        draw.arc([int(8*scale), int(6*scale), int(16*scale), int(14*scale)], start=180, end=0, fill=255, width=stroke_w)
        draw.line([(int(8*scale), int(10*scale)), (int(8*scale), int(19*scale))], fill=255, width=stroke_w)
        draw.line([(int(16*scale), int(10*scale)), (int(16*scale), int(19*scale))], fill=255, width=stroke_w)
        draw.line([(int(3*scale), int(19*scale)), (int(21*scale), int(19*scale))], fill=255, width=stroke_w)
    elif key == 'close':
        draw.line([(int(6*scale), int(6*scale)), (int(18*scale), int(18*scale))], fill=255, width=stroke_w)
        draw.line([(int(18*scale), int(6*scale)), (int(6*scale), int(18*scale))], fill=255, width=stroke_w)
    elif key == 'check':
        draw.ellipse([int(3*scale), int(3*scale), int(21*scale), int(21*scale)], outline=255, width=stroke_w)
        draw.line([(int(7*scale), int(12*scale)), (int(10*scale), int(15*scale)), (int(17*scale), int(8*scale))], fill=255, width=stroke_w)
        
    master = img.resize((size, size), Image.Resampling.LANCZOS)
    return master

def get_ink_bbox(alpha_img, threshold=10):
    w, h = alpha_img.size
    pix = alpha_img.load()
    pts = [(x, y) for y in range(h) for x in range(w) if pix[x, y] >= threshold]
    if not pts:
        return [0, 0, w, h]
    return [min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts) + 1, max(p[1] for p in pts) + 1]

def main():
    print("Verifying source PNG hashes...")
    for stem, meta in SOURCE_METADATA.items():
        p = REF / stem
        if not p.exists():
            print(f"ERROR: Missing reference image {p}")
            return 1
        im = Image.open(p)
        if list(im.size) != meta["size"]:
            print(f"ERROR: Image {stem} dimension mismatch: {im.size} vs {meta['size']}")
            return 1
        h = sha256_file(p)
        if h != meta["sha256"]:
            print(f"ERROR: Image {stem} hash mismatch: {h} vs {meta['sha256']}")
            return 1
    print("All 5 source PNGs verified.")

    manifest_masters = {}

    # Extract 18 primary icons from source PNGs
    print("Extracting 18 primary icon masters from source pixels...")
    for key, info in PRIMARY_ROIS.items():
        stem = info["source"]
        roi = info["roi"]
        img_p = REF / stem
        im = Image.open(img_p).convert("RGB").crop(roi)
        
        t = info["type"]
        if t == "dark_ink":
            alpha = extract_dark_ink(im)
        elif t == "light_on_dark":
            alpha = extract_light_on_dark(im)
        elif t == "check_badge":
            alpha = extract_check_badge(im)
        elif t == "warning_red":
            alpha = extract_warning(im)
        else:
            raise ValueError(f"Unknown type {t}")
            
        out_p = MASTERS_DIR / f"{key}_master.png"
        alpha.save(out_p)
        
        ibox = get_ink_bbox(alpha)
        sha = sha256_file(out_p)
        
        manifest_masters[key] = {
            "source_image": stem,
            "source_roi": roi,
            "extraction_method": t,
            "master_file": f"masters/{key}_master.png",
            "master_size": list(alpha.size),
            "master_sha256": sha,
            "ink_bbox": ibox,
            "topology_notes": info["notes"]
        }
        print(f"  [Primary] {key:14s}: master size {alpha.size}, ink bbox {ibox}")

    # Render 7 documented extensions
    print("Generating 7 documented extension masters...")
    for key, notes in EXTENSION_ICONS.items():
        alpha = render_extension_master(key, size=128)
        out_p = MASTERS_DIR / f"{key}_master.png"
        alpha.save(out_p)
        
        ibox = get_ink_bbox(alpha)
        sha = sha256_file(out_p)
        manifest_masters[key] = {
            "source_image": None,
            "source_roi": None,
            "extraction_method": "vector_supersampled_extension",
            "master_file": f"masters/{key}_master.png",
            "master_size": list(alpha.size),
            "master_sha256": sha,
            "ink_bbox": ibox,
            "topology_notes": notes
        }
        print(f"  [Extension] {key:14s}: master size {alpha.size}, ink bbox {ibox}")

    ICON_SPECS = [
        ("home_32", "home", 32, 32),
        ("home_30", "home", 30, 30),
        ("chart_32", "chart", 32, 32),
        ("devices_32", "devices", 32, 32),
        ("home_24", "home", 24, 24),
        ("chart_24", "chart", 24, 24),
        ("devices_24", "devices", 24, 24),
        ("sliders_24", "sliders", 24, 24),
        ("radio_20", "radio", 20, 20),
        ("timer_20", "timer", 20, 20),
        ("air_28", "air", 28, 28),
        ("air_32", "air", 32, 32),
        ("thermometer_24", "thermometer", 24, 24),
        ("thermometer_32", "thermometer", 32, 32),
        ("drop_24", "drop", 24, 24),
        ("drop_32", "drop", 32, 32),
        ("drop_36", "drop", 36, 36),
        ("drop_40", "drop", 40, 40),
        ("drop_42", "drop", 42, 42),
        ("drop_44", "drop", 44, 56),
        ("purifier_24", "purifier", 24, 24),
        ("purifier_32", "purifier", 32, 32),
        ("purifier_36", "purifier", 36, 36),
        ("purifier_40", "purifier", 40, 40),
        ("purifier_42", "purifier", 42, 42),
        ("purifier_44", "purifier", 44, 56),
        ("fan_24", "fan", 24, 24),
        ("fan_32", "fan", 32, 32),
        ("fan_36", "fan", 36, 36),
        ("fan_40", "fan", 40, 40),
        ("fan_42", "fan", 42, 42),
        ("fan_44", "fan", 44, 52),
        ("light_24", "light", 24, 24),
        ("light_32", "light", 32, 32),
        ("light_36", "light", 36, 36),
        ("light_40", "light", 40, 40),
        ("light_42", "light", 42, 42),
        ("light_44", "light", 44, 56),
        ("plug_24", "plug", 24, 24),
        ("plug_32", "plug", 32, 32),
        ("plug_36", "plug", 36, 36),
        ("plug_40", "plug", 40, 40),
        ("plug_42", "plug", 42, 42),
        ("plug_44", "plug", 44, 54),
        ("heat_24", "heat", 24, 24),
        ("heat_32", "heat", 32, 32),
        ("heat_36", "heat", 36, 36),
        ("heat_40", "heat", 40, 40),
        ("heat_42", "heat", 42, 42),
        ("heat_44", "heat", 46, 54),
        ("close_20", "close", 20, 20),
        ("check_20", "check", 20, 20),
        ("check_circle_32", "check_circle", 32, 32),
        ("warning_20", "warning", 20, 20),
        ("warning_24", "warning", 24, 24),
        ("leaf_18", "leaf", 18, 18),
        ("leaf_24", "leaf", 24, 24),
        ("leaf_32", "leaf", 32, 32),
        ("leaf_36", "leaf", 36, 36),
        ("gear_24", "gear", 24, 24),
        ("gear_36", "gear", 36, 36),
        ("sliders_36", "sliders", 36, 36),
        ("chevron_right_20", "chevron_right", 20, 20),
        ("chevron_left_20", "chevron_left", 20, 20),
        ("chevron_left_24", "chevron_left", 24, 24),
        ("chevron_down_18", "chevron_down", 18, 18),
        ("plus_24", "plus", 24, 24),
        ("minus_24", "minus", 24, 24),
    ]

    manifest_icons = []
    for spec in ICON_SPECS:
        asset_name, master_key, w, h = spec[0], spec[1], spec[2], spec[3]
        pad = 1 if max(w, h) <= 24 else 2
        color_role = "UI_COLOR_INK"
        if master_key == "check_circle":
            color_role = "UI_COLOR_CELADON"
        elif master_key == "warning":
            color_role = "UI_COLOR_RED"
        elif master_key == "air":
            color_role = "UI_COLOR_CELADON"

        # Render A4 bitmap and compute hash
        master_entry = manifest_masters[master_key]
        master_img = Image.open(ROOT / "assets/icons/quiet-architecture" / master_entry["master_file"])
        ibox = master_entry["ink_bbox"]

        min_x, min_y, max_x, max_y = ibox
        min_x = max(0, min_x - 1)
        min_y = max(0, min_y - 1)
        max_x = min(master_img.width, max_x + 1)
        max_y = min(master_img.height, max_y + 1)

        ink_crop = master_img.crop((min_x, min_y, max_x, max_y))
        iw, ih = ink_crop.size

        avail_w = cw = w
        avail_h = ch = h
        avail_w -= 2 * pad
        avail_h -= 2 * pad
        s = min(avail_w / iw, avail_h / ih)
        tw = max(1, int(round(iw * s)))
        th = max(1, int(round(ih * s)))

        scaled = ink_crop.resize((tw, th), Image.Resampling.LANCZOS)
        canvas = Image.new("L", (cw, ch), 0)
        paste_x = (cw - tw) // 2
        paste_y = (ch - th) // 2
        canvas.paste(scaled, (paste_x, paste_y))

        stride = (cw + 1) // 2
        out_bytes = bytearray()
        cpix = canvas.load()
        for y in range(ch):
            row = bytearray(stride)
            for x in range(cw):
                val = int(round(cpix[x, y] * 15.0 / 255.0))
                if x % 2 == 0:
                    row[x // 2] |= (val << 4)
                else:
                    row[x // 2] |= val
            out_bytes += row

        data_sha = hashlib.sha256(out_bytes).hexdigest()

        manifest_icons.append({
            "asset_name": f"ui_icon_{asset_name}",
            "descriptor_name": f"ui_icon_{asset_name}",
            "master_key": master_key,
            "canvas_width": w,
            "canvas_height": h,
            "padding": pad,
            "color_role": color_role,
            "cf": "LV_IMG_CF_ALPHA_4BIT",
            "data_size": len(out_bytes),
            "data_sha256": data_sha
        })

    manifest = {
        "schema_version": "2.0",
        "theme": "Quiet Architecture",
        "provenance_authority": "Raster pixels from docs/drafts/2026-09-17-pro-max/",
        "sources": SOURCE_METADATA,
        "masters": manifest_masters,
        "icons": manifest_icons
    }

    with open(MANIFEST_FILE, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print(f"Wrote authoritative manifest: {MANIFEST_FILE}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
