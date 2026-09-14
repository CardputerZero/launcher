#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

"""
Crop device image and generate final layout for the emulator.
"""
from PIL import Image
import json

IMG_PATH = "/Users/eggfly/github/CardputerZero/M5CardputerZero-ProjMgmt/1412.jpg"
OUT_IMG = "/Users/eggfly/github/CardputerZero/M5CardputerZero-Emulator/assets/device_skin.png"
OUT_JSON = "/Users/eggfly/github/CardputerZero/M5CardputerZero-Emulator/assets/skin_layout.json"

img = Image.open(IMG_PATH)

# From analysis: device body roughly (230, 260) to (1000, 780) contains the actual device
# Let's crop with some margin. The white bg starts at edges.
# Body bounds from analysis: left=4, but the actual device rendering starts further in.
# Let's use a tighter crop based on the gray body region.
import numpy as np
arr = np.array(img)
gray = np.mean(arr, axis=2)

# Find rows/cols where gray < 220 (not white background)
body_mask = gray < 225
rows_any = np.any(body_mask, axis=1)
cols_any = np.any(body_mask, axis=0)

# Find first/last non-white
top = int(np.argmax(rows_any))
bot = int(len(rows_any) - np.argmax(rows_any[::-1]) - 1)
left = int(np.argmax(cols_any))
right = int(len(cols_any) - np.argmax(cols_any[::-1]) - 1)

# Add small margin
margin = 5
top = max(0, top - margin)
bot = min(img.height, bot + margin)
left = max(0, left - margin)
right = min(img.width, right + margin)

print(f"Crop: ({left},{top}) - ({right},{bot}) = {right-left}x{bot-top}")

cropped = img.crop((left, top, right, bot))
cw, ch = cropped.size
print(f"Cropped size: {cw} x {ch}")

# Save as PNG (lossless, for SDL texture)
cropped.save(OUT_IMG, "PNG")
print(f"Saved: {OUT_IMG}")

# Now find LCD and key positions relative to cropped image
# LCD: from analysis was (277,329)-(996,586) in original
lcd_x = 277 - left
lcd_y = 329 - top
lcd_w = 996 - 277
lcd_h = 586 - 329
print(f"LCD (in cropped): ({lcd_x},{lcd_y}) {lcd_w}x{lcd_h}")

# Key rows (the 11-key rows from analysis):
# Row 1 (numbers):  y≈574, keys at x=286..977, each ~41x24
# Row 13 (QWERTY):  y≈631, same x pattern
# Row 15 (ASDF):    y≈687, same
# Row 17 (ZXCV):    y≈743, same
# First key x=286, step=65, width=41, height=24

key_rows_orig = [
    # (y, [(x, w) ...])  — from the 11-key rows
    (563, [(286,41),(351,41),(417,40),(482,40),(547,40),(612,40),(676,41),(742,40),(807,40),(872,41),(937,41)]),
    (619, [(286,41),(351,41),(416,41),(482,40),(546,41),(612,40),(676,41),(742,40),(807,40),(872,41),(937,41)]),
    (676, [(286,41),(351,41),(416,41),(482,40),(547,40),(612,40),(676,41),(742,40),(807,40),(872,41),(937,41)]),
    (732, [(286,41),(351,41),(416,41),(482,40),(547,40),(612,40),(676,41),(742,40),(807,40),(872,41),(937,41)]),
]

key_labels = [
    ["1","2","3","4","5","6","7","8","9","0","del"],
    ["tab","Q","W","E","R","T","Y","U","I","O","P"],
    ["Aa","A","S","D","F","G","H","J","K","L","OK"],
    ["fn","ctrl","alt","Z","X","C","V","B","N","M","_"],
]

key_h = 24

key_rows = []
for ri, (row_y, keys) in enumerate(key_rows_orig):
    row = []
    for ci, (kx, kw) in enumerate(keys):
        row.append({
            "label": key_labels[ri][ci],
            "x": kx - left,
            "y": row_y - top,
            "w": kw,
            "h": key_h,
        })
    key_rows.append(row)

layout = {
    "skin_image": "device_skin.png",
    "skin_size": {"w": cw, "h": ch},
    "lcd": {"x": lcd_x, "y": lcd_y, "w": lcd_w, "h": lcd_h},
    "lcd_logical": {"w": 320, "h": 170},
    "key_rows": key_rows,
}

with open(OUT_JSON, "w") as f:
    json.dump(layout, f, indent=2)
print(f"Layout: {OUT_JSON}")

# Print summary
print(f"\n=== Emulator Layout ===")
print(f"Skin image: {cw}x{ch}")
print(f"LCD viewport: ({lcd_x},{lcd_y}) {lcd_w}x{lcd_h}")
print(f"LCD maps to LVGL 320x170")
print(f"Scale factor: {lcd_w/320:.2f}x horizontal, {lcd_h/170:.2f}x vertical")
print(f"Key rows: 4 x 11 keys")
for ri, row in enumerate(key_rows):
    labels = " ".join(k["label"] for k in row)
    print(f"  Row {ri}: y={row[0]['y']}  [{labels}]")
