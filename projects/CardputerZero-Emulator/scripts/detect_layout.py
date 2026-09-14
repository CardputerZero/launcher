#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

"""
Detect LCD (transparent area) and key positions (dark blobs) from M5CardputerEmu.png
"""
from PIL import Image
import numpy as np
from scipy import ndimage
import json

IMG = "/Users/eggfly/github/CardputerZero/M5CardputerZero-ProjMgmt/M5CardputerEmu.png"
img = Image.open(IMG).convert("RGBA")
w, h = img.size
arr = np.array(img)
print(f"Image: {w}x{h} RGBA")

# ── 1. Find LCD: transparent region (alpha < 10) ────────────────
alpha = arr[:,:,3]
transparent = alpha < 10
rows_t = np.any(transparent, axis=1)
cols_t = np.any(transparent, axis=0)

lcd_top = int(np.argmax(rows_t))
lcd_bot = int(len(rows_t) - np.argmax(rows_t[::-1]) - 1)
lcd_left = int(np.argmax(cols_t))
lcd_right = int(len(cols_t) - np.argmax(cols_t[::-1]) - 1)

print(f"\n=== LCD (transparent area) ===")
print(f"  ({lcd_left}, {lcd_top}) - ({lcd_right}, {lcd_bot})")
print(f"  Size: {lcd_right-lcd_left} x {lcd_bot-lcd_top}")

# ── 2. Find keys: dark blobs (brightness < 70, alpha > 200) ─────
rgb = arr[:,:,:3].astype(float)
gray = np.mean(rgb, axis=2)
opaque = alpha > 200
dark = (gray < 70) & opaque

# Only look below the LCD area (keyboard region)
key_region_top = lcd_bot + 10
dark_below = np.zeros_like(dark)
dark_below[key_region_top:, :] = dark[key_region_top:, :]

labeled, n = ndimage.label(dark_below)
print(f"\nFound {n} dark blobs below LCD")

keys = []
for i in range(1, n + 1):
    ys, xs = np.where(labeled == i)
    bw = int(xs.max() - xs.min())
    bh = int(ys.max() - ys.min())
    area = len(ys)
    # Filter: real keys are roughly 40-70 wide, 20-40 tall, area > 500
    if bw < 25 or bh < 12 or area < 400:
        continue
    if bw > 120 or bh > 60:
        continue
    keys.append({
        "x": int(xs.min()),
        "y": int(ys.min()),
        "w": bw,
        "h": bh,
        "cx": int(xs.mean()),
        "cy": int(ys.mean()),
        "area": area,
    })

keys.sort(key=lambda k: (k["cy"] // 25, k["cx"]))
print(f"Valid keys: {len(keys)}")

# Group into rows
rows = []
cur_row = []
last_cy = -100
for k in keys:
    if abs(k["cy"] - last_cy) > 20:
        if cur_row:
            rows.append(cur_row)
        cur_row = [k]
    else:
        cur_row.append(k)
    last_cy = k["cy"]
if cur_row:
    rows.append(cur_row)

# Key labels for the 4 main rows
labels = [
    ["1","2","3","4","5","6","7","8","9","0","del"],
    ["tab","Q","W","E","R","T","Y","U","I","O","P"],
    ["Aa","A","S","D","F","G","H","J","K","L","OK"],
    ["fn","ctrl","alt","Z","X","C","V","B","N","M","_"],
]

# SDL keycodes
sdlkeys = [
    ["49","50","51","52","53","54","55","56","57","48","8"],  # SDLK values as strings for readability
    ["9","113","119","101","114","116","121","117","105","111","112"],
    ["304","97","115","100","102","103","104","106","107","108","13"],
    ["283","306","308","122","120","99","118","98","110","109","32"],
]

print(f"\n=== Keyboard Rows: {len(rows)} ===")
key_rows_out = []
for ri, row in enumerate(rows):
    print(f"\nRow {ri}: {len(row)} keys, cy≈{row[0]['cy']}")
    row_out = []
    for ci, k in enumerate(row):
        label = labels[ri][ci] if ri < len(labels) and ci < len(labels[ri]) else "?"
        print(f"  [{ci:2d}] {label:4s}  ({k['x']:4d},{k['y']:4d}) {k['w']:3d}x{k['h']:3d}  area={k['area']}")
        row_out.append({
            "label": label,
            "x": k["x"], "y": k["y"], "w": k["w"], "h": k["h"],
            "cx": k["cx"], "cy": k["cy"],
        })
    key_rows_out.append(row_out)

result = {
    "image": {"w": w, "h": h, "file": "M5CardputerEmu.png"},
    "lcd": {"x": lcd_left, "y": lcd_top, "w": lcd_right - lcd_left, "h": lcd_bot - lcd_top},
    "key_rows": key_rows_out,
}

out = "/Users/eggfly/github/CardputerZero/M5CardputerZero-Emulator/assets/emu_layout.json"
with open(out, "w") as f:
    json.dump(result, f, indent=2)
print(f"\nSaved: {out}")

# Print C array for main.cpp
print("\n=== C code for main.cpp ===")
print(f"// LCD: ({lcd_left},{lcd_top}) {lcd_right-lcd_left}x{lcd_bot-lcd_top}")
print(f"static constexpr int LCD_SX = {lcd_left};")
print(f"static constexpr int LCD_SY = {lcd_top};")
print(f"static constexpr int LCD_SW = {lcd_right-lcd_left};")
print(f"static constexpr int LCD_SH = {lcd_bot-lcd_top};")
print()
print("static KeyRect g_keys[4][11] = {")
sdlk_names = [
    ["SDLK_1","SDLK_2","SDLK_3","SDLK_4","SDLK_5","SDLK_6","SDLK_7","SDLK_8","SDLK_9","SDLK_0","SDLK_BACKSPACE"],
    ["SDLK_TAB","SDLK_q","SDLK_w","SDLK_e","SDLK_r","SDLK_t","SDLK_y","SDLK_u","SDLK_i","SDLK_o","SDLK_p"],
    ["SDLK_LSHIFT","SDLK_a","SDLK_s","SDLK_d","SDLK_f","SDLK_g","SDLK_h","SDLK_j","SDLK_k","SDLK_l","SDLK_RETURN"],
    ["SDLK_F2","SDLK_LCTRL","SDLK_LALT","SDLK_z","SDLK_x","SDLK_c","SDLK_v","SDLK_b","SDLK_n","SDLK_m","SDLK_SPACE"],
]
for ri, row in enumerate(key_rows_out):
    entries = []
    for ci, k in enumerate(row):
        sn = sdlk_names[ri][ci] if ri < len(sdlk_names) and ci < len(sdlk_names[ri]) else "0"
        entries.append(f"{{{k['x']},{k['y']},{k['w']},{k['h']},{sn}}}")
    print(f"    {{{','.join(entries)}}},")
print("};")
