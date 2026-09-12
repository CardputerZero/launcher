#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

"""
Analyze the CardputerZero render image (1412.jpg) to find:
1. LCD screen region (the dark rectangle with colored content)
2. Individual key positions (dark rounded rectangles in the keyboard area)
3. Device body bounding box (crop out the white background)

Strategy:
- Convert to grayscale, threshold to find the device body (gray body vs white bg)
- Within the body, find the LCD by looking for the large dark rectangle in the upper area
- Find keys by looking for small dark blobs in the lower area
"""
from PIL import Image
import json
import numpy as np

IMG_PATH = "/Users/eggfly/github/CardputerZero/M5CardputerZero-ProjMgmt/1412.jpg"

img = Image.open(IMG_PATH)
w, h = img.size
print(f"Image size: {w} x {h}")

arr = np.array(img)

# Step 1: Find device body bounding box (non-white region)
gray = np.mean(arr, axis=2)
# White background is > 240, device body is < 220
body_mask = gray < 230
rows_any = np.any(body_mask, axis=1)
cols_any = np.any(body_mask, axis=0)
body_top = int(np.argmax(rows_any))
body_bot = int(h - np.argmax(rows_any[::-1]) - 1)
body_left = int(np.argmax(cols_any))
body_right = int(w - np.argmax(cols_any[::-1]) - 1)
print(f"Device body: ({body_left}, {body_top}) - ({body_right}, {body_bot})")
print(f"  Size: {body_right - body_left} x {body_bot - body_top}")

# Step 2: Find LCD region (large dark area in upper portion of body)
# LCD is the biggest contiguous dark region. Look for pixels < 60 brightness
# in the upper 60% of the body
body_h = body_bot - body_top
upper_region = arr[body_top:body_top + int(body_h * 0.55), body_left:body_right]
upper_gray = np.mean(upper_region, axis=2)
lcd_mask = upper_gray < 50

# Find bounding box of the dark LCD region
lcd_rows = np.any(lcd_mask, axis=1)
lcd_cols = np.any(lcd_mask, axis=0)

if np.any(lcd_rows) and np.any(lcd_cols):
    lcd_top = int(np.argmax(lcd_rows)) + body_top
    lcd_bot = int(len(lcd_rows) - np.argmax(lcd_rows[::-1]) - 1) + body_top
    lcd_left = int(np.argmax(lcd_cols)) + body_left
    lcd_right = int(len(lcd_cols) - np.argmax(lcd_cols[::-1]) - 1) + body_left
    print(f"LCD region: ({lcd_left}, {lcd_top}) - ({lcd_right}, {lcd_bot})")
    print(f"  Size: {lcd_right - lcd_left} x {lcd_bot - lcd_top}")
else:
    print("LCD not found!")
    lcd_left = lcd_top = lcd_right = lcd_bot = 0

# Step 3: Find individual key positions
# Keys are in the lower ~50% of the body, dark blobs (< 80 brightness)
key_area_top = body_top + int(body_h * 0.5)
key_area = arr[key_area_top:body_bot, body_left:body_right]
key_gray = np.mean(key_area, axis=2)
key_mask = key_gray < 80

# Find connected components (simple row-based scan)
# Group dark pixels into bounding boxes
from scipy import ndimage

labeled, num_features = ndimage.label(key_mask)
print(f"\nFound {num_features} dark blobs in keyboard area")

keys = []
for i in range(1, num_features + 1):
    ys, xs = np.where(labeled == i)
    if len(ys) < 50:  # skip tiny noise
        continue
    bw = int(xs.max() - xs.min())
    bh = int(ys.max() - ys.min())
    if bw < 10 or bh < 10:  # skip non-key shapes
        continue
    if bw > 200 or bh > 100:  # skip too large
        continue
    cx = int(xs.mean()) + body_left
    cy = int(ys.mean()) + key_area_top
    keys.append({
        "x": int(xs.min()) + body_left,
        "y": int(ys.min()) + key_area_top,
        "w": bw,
        "h": bh,
        "cx": cx,
        "cy": cy
    })

# Sort keys by y then x (row-major)
keys.sort(key=lambda k: (k["y"] // 30, k["x"]))

print(f"Valid keys found: {len(keys)}")

# Group into rows
rows = []
current_row = []
last_y = -100
for k in keys:
    if abs(k["cy"] - last_y) > 20:
        if current_row:
            rows.append(current_row)
        current_row = [k]
    else:
        current_row.append(k)
    last_y = k["cy"]
if current_row:
    rows.append(current_row)

print(f"\nKeyboard rows: {len(rows)}")
for i, row in enumerate(rows):
    print(f"  Row {i}: {len(row)} keys, y≈{row[0]['cy']}")
    for j, k in enumerate(row):
        print(f"    [{j:2d}] ({k['x']:4d}, {k['y']:4d})  {k['w']:3d}x{k['h']:3d}")

# Output as JSON
result = {
    "image": {"width": w, "height": h},
    "body": {"x": body_left, "y": body_top, "w": body_right - body_left, "h": body_bot - body_top},
    "lcd": {"x": lcd_left, "y": lcd_top, "w": lcd_right - lcd_left, "h": lcd_bot - lcd_top},
    "keys": keys,
    "key_rows": [[k for k in row] for row in rows]
}

out_path = "/Users/eggfly/github/CardputerZero/M5CardputerZero-Emulator/assets/device_layout.json"
import os
os.makedirs(os.path.dirname(out_path), exist_ok=True)
with open(out_path, "w") as f:
    json.dump(result, f, indent=2)
print(f"\nLayout saved to {out_path}")
