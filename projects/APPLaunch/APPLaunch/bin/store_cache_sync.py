#!/usr/bin/env python3
"""
APPLaunch Store Cache Sync
Fetches the public firmware list from FirmwareManagementV3 /public/list endpoint,
downloads logo images locally, and writes each app's information to
/var/cache/APPLunch/store/<name>_<version>.json cache files.

Cached JSON corresponds to the C++ struct store_app_info:
  {
    "name":        string,   // firmwareName
    "version":     string,   // version
    "logo_icon":   string,   // absolute local path to the logo
    "class_name":  string,   // class
    "description": string,   // remark
    "url":         string    // url
  }

Notes:
  - Each run fetches the latest list from the server and overwrites the JSON cache.
  - Logo images are skipped by default if they already exist; use --force to re-download.
  - If the number of cached entries is less than the server entries, it means the server
    had fewer entries during the last run; re-run this script to sync the latest data.

Usage:
    python3 store_cache_sync.py [--cache-dir /var/cache/APPLunch/store] [--force]
"""

import argparse
import json
import os
import re
import sys
from urllib.parse import urlparse

try:
    import requests
except ImportError:
    print("Missing dependency 'requests', please run: pip3 install requests")
    sys.exit(1)

# ─────────────────────────── Constants ───────────────────────────────────────
BASE_URL    = "https://ota.m5stack.com/ota/api/v3/firmware-management"
PUBLIC_LIST = f"{BASE_URL}/public/list"
DEFAULT_CACHE_DIR = "/var/cache/APPLunch/store"
TIMEOUT = 30


# ─────────────────────────── Helper Functions ────────────────────────────────

def _safe_filename(s: str) -> str:
    """Convert a string to a safe filename (keeps alphanumerics, dots, hyphens, underscores)."""
    return re.sub(r'[^\w.\-]', '_', s)


def _logo_ext(url: str) -> str:
    """Extract image extension from URL, defaults to .png."""
    path = urlparse(url).path
    _, ext = os.path.splitext(path)
    return ext.lower() if ext else ".png"


def download_logo(url: str, dest_path: str, force: bool = False) -> bool:
    """Download logo to dest_path; skip if already exists (force=True to re-download). Returns True on success."""
    if os.path.exists(dest_path) and not force:
        print(f"  [skip]  logo already exists: {dest_path}")
        return True
    try:
        resp = requests.get(url, timeout=TIMEOUT, stream=True)
        resp.raise_for_status()
        with open(dest_path, "wb") as f:
            for chunk in resp.iter_content(chunk_size=8192):
                f.write(chunk)
        print(f"  [logo]  {dest_path}")
        return True
    except Exception as e:
        print(f"  [warn]  logo download failed {url}: {e}")
        return False


def fetch_public_list() -> list:
    """Call /public/list endpoint and return all FirmwarePublicItemVO entries (with sku field attached)."""
    print(f"→ Requesting {PUBLIC_LIST}")
    resp = requests.get(PUBLIC_LIST, timeout=TIMEOUT)
    resp.raise_for_status()
    body = resp.json()
    if body.get("code") != 200:
        print(f"✗ API returned error: {body.get('msg', '')}")
        sys.exit(1)

    items_flat = []
    data = body.get("data") or []
    for group in data:
        sku   = group.get("sku", "")
        items = group.get("items") or []
        for item in items:
            item["_sku"] = sku   # attach sku for logging
            items_flat.append(item)

    print(f"  Fetched {len(items_flat)} app entries in total")
    return items_flat


def sync_cache(cache_dir: str, force: bool = False) -> None:
    """Main logic: fetch list → download logos → write JSON cache. force=True re-downloads existing logos."""
    os.makedirs(cache_dir, exist_ok=True)

    items = fetch_public_list()

    written = 0
    for item in items:
        name        = item.get("firmwareName", "")
        version     = item.get("version", "")
        avatar_url  = item.get("avatarUrl", "")
        class_name  = item.get("class", "")
        description = item.get("remark", "")
        url         = item.get("url", "")

        if not name:
            print(f"  [skip]  entry missing firmwareName, skipped: {item}")
            continue

        safe_name    = _safe_filename(name)
        safe_version = _safe_filename(version)
        base_name    = f"{safe_name}_{safe_version}"

        # ── Download logo ──────────────────────────────────────────────────
        logo_icon = ""
        if avatar_url:
            ext        = _logo_ext(avatar_url)
            logo_path  = os.path.join(cache_dir, f"{base_name}_logo{ext}")
            if download_logo(avatar_url, logo_path, force=force):
                logo_icon = logo_path
        else:
            print(f"  [warn]  {name} has no avatarUrl, logo_icon will be empty")

        # ── Write JSON cache ───────────────────────────────────────────────
        cache_data = {
            "name":        name,
            "version":     version,
            "logo_icon":   os.path.join('cache/store',f"{os.path.basename(logo_icon)}"),
            "class_name":  class_name,
            "description": description,
            "url":         url,
        }

        json_path = os.path.join(cache_dir, f"{base_name}.json")
        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(cache_data, f, ensure_ascii=False, indent=2)

        print(f"  [json]  {json_path}")
        written += 1

    print(f"\n✓ Done: wrote {written} cache files to {cache_dir}")


# ─────────────────────────── CLI Entry Point ─────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="APPLaunch Store Cache Sync — sync cache from /public/list to local",
    )
    parser.add_argument(
        "--cache-dir",
        default=DEFAULT_CACHE_DIR,
        help=f"Cache directory (default: {DEFAULT_CACHE_DIR})",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Force re-download of existing logo images",
    )
    args = parser.parse_args()
    sync_cache(args.cache_dir, force=args.force)


if __name__ == "__main__":
    main()
