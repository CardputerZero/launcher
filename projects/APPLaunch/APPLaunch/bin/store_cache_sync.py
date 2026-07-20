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
  - Cache files are written atomically and stale entries are removed after a successful fetch.
  - Logo images are skipped by default if they already exist; use --force to re-download.

Usage:
    python3 store_cache_sync.py [--cache-dir /var/cache/APPLunch/store] [--force]
"""

import argparse
import fcntl
import hashlib
import json
import os
import re
import sys
import tempfile
from collections import Counter
from contextlib import contextmanager
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
MAX_LOGO_SIZE = 10 * 1024 * 1024
MANIFEST_NAME = ".store-cache-manifest.json"
LOCK_NAME = ".store-cache.lock"


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
    """Download and validate a logo, then atomically replace dest_path."""
    if os.path.exists(dest_path) and not force:
        print(f"  [skip]  logo already exists: {dest_path}")
        return True
    try:
        resp = requests.get(url, timeout=TIMEOUT, stream=True)
        resp.raise_for_status()
        content_type = resp.headers.get("Content-Type", "").split(";", 1)[0].strip().lower()
        if not content_type.startswith("image/"):
            raise ValueError(f"unexpected Content-Type: {content_type or 'missing'}")

        content_length = resp.headers.get("Content-Length")
        if content_length is not None:
            try:
                declared_size = int(content_length)
            except ValueError as exc:
                raise ValueError(f"invalid Content-Length: {content_length}") from exc
            if declared_size <= 0 or declared_size > MAX_LOGO_SIZE:
                raise ValueError(f"invalid image size: {declared_size} bytes")

        dest_dir = os.path.dirname(os.path.abspath(dest_path))
        os.makedirs(dest_dir, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".store-logo-", dir=dest_dir) as temp_dir:
            temp_path = os.path.join(temp_dir, os.path.basename(dest_path))
            size = 0
            with open(temp_path, "wb") as f:
                for chunk in resp.iter_content(chunk_size=8192):
                    if not chunk:
                        continue
                    size += len(chunk)
                    if size > MAX_LOGO_SIZE:
                        raise ValueError(f"image exceeds {MAX_LOGO_SIZE} bytes")
                    f.write(chunk)
                f.flush()
                os.fsync(f.fileno())
            if size == 0:
                raise ValueError("empty image response")
            if content_length is not None and size != declared_size:
                raise ValueError(f"incomplete image: expected {declared_size}, got {size} bytes")
            os.replace(temp_path, dest_path)
        print(f"  [logo]  {dest_path}")
        return True
    except Exception as e:
        print(f"  [warn]  logo download failed {url}: {e}")
        return False


def _write_json_atomic(path: str, data: object) -> None:
    """Write JSON through a sibling temporary file and atomically replace path."""
    directory = os.path.dirname(path)
    fd, temp_path = tempfile.mkstemp(prefix=".store-json-", dir=directory)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.replace(temp_path, path)
    except BaseException:
        try:
            os.unlink(temp_path)
        except FileNotFoundError:
            pass
        raise


@contextmanager
def _cache_lock(cache_dir: str):
    lock_path = os.path.join(cache_dir, LOCK_NAME)
    with open(lock_path, "a", encoding="utf-8") as lock_file:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)


def _read_manifest(cache_dir: str) -> tuple:
    path = os.path.join(cache_dir, MANIFEST_NAME)
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        files = data.get("files")
        logo_hashes = data.get("logo_hashes", {})
        if not isinstance(files, list) or not isinstance(logo_hashes, dict):
            raise ValueError("manifest files/logo_hashes have invalid types")
        safe_files = {name for name in files if _is_safe_manifest_name(name)}
        safe_hashes = {
            name: digest for name, digest in logo_hashes.items()
            if _is_safe_manifest_name(name)
            and name in safe_files
            and isinstance(digest, str)
            and re.fullmatch(r"[0-9a-f]{64}", digest)
        }
        # The scan recovers files published by an interrupted run before its manifest commit.
        return safe_files | _find_legacy_cache_files(cache_dir), safe_hashes
    except FileNotFoundError:
        return _find_legacy_cache_files(cache_dir), {}
    except (OSError, ValueError, TypeError, AttributeError) as exc:
        print(f"  [warn]  rebuilding invalid cache manifest: {exc}")
        return _find_legacy_cache_files(cache_dir), {}


def _is_safe_manifest_name(name: object) -> bool:
    return (
        isinstance(name, str)
        and name not in ("", ".", "..")
        and os.path.basename(name) == name
    )


def _file_sha256(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _find_legacy_cache_files(cache_dir: str) -> set:
    """Find cache files created before manifests were introduced."""
    files = set()
    required_keys = {"name", "version", "logo_icon", "class_name", "description", "url"}
    try:
        names = os.listdir(cache_dir)
    except FileNotFoundError:
        return files
    for name in names:
        if not name.endswith(".json") or name == MANIFEST_NAME:
            continue
        path = os.path.join(cache_dir, name)
        try:
            with open(path, encoding="utf-8") as f:
                data = json.load(f)
            if not isinstance(data, dict) or not required_keys.issubset(data):
                continue
            files.add(name)
            logo_name = os.path.basename(data.get("logo_icon", ""))
            if logo_name and os.path.isfile(os.path.join(cache_dir, logo_name)):
                files.add(logo_name)
        except (OSError, ValueError, TypeError):
            continue
    # A crash can happen after publishing a logo but before its JSON file.
    for name in names:
        if re.fullmatch(r".+_logo\.[A-Za-z0-9]{1,10}", name):
            files.add(name)
    return files


def _commit_manifest(
    cache_dir: str,
    previous_files: set,
    current_files: set,
    logo_hashes: dict,
) -> None:
    for name in sorted(previous_files - current_files):
        stale_path = os.path.join(cache_dir, name)
        try:
            if os.path.isfile(stale_path) or os.path.islink(stale_path):
                os.unlink(stale_path)
                print(f"  [clean] {stale_path}")
        except OSError as exc:
            print(f"  [warn]  failed to remove stale cache file {stale_path}: {exc}")
            raise
    _write_json_atomic(
        os.path.join(cache_dir, MANIFEST_NAME),
        {
            "version": 3,
            "files": sorted(current_files),
            "logo_hashes": logo_hashes,
        },
    )


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
    with _cache_lock(cache_dir):
        _sync_cache_locked(cache_dir, force)


def _sync_cache_locked(cache_dir: str, force: bool) -> None:
    """Build a complete cache generation in staging, then publish it."""

    items = fetch_public_list()
    previous_files, previous_logo_hashes = _read_manifest(cache_dir)

    base_names = []
    for item in items:
        name = item.get("firmwareName", "")
        if name:
            base_names.append(
                f"{_safe_filename(name)}_{_safe_filename(item.get('version', ''))}"
            )
    duplicate_names = sorted(name for name, count in Counter(base_names).items() if count > 1)
    if duplicate_names:
        raise ValueError(
            "multiple server entries map to the same cache name: "
            + ", ".join(duplicate_names)
        )

    written = 0
    current_files = set()
    logo_hashes = {}
    publish_logos = []
    publish_json = []
    with tempfile.TemporaryDirectory(prefix=".store-sync-", dir=cache_dir) as staging_dir:
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

            logo_name = ""
            if avatar_url:
                logo_name = f"{base_name}_logo{_logo_ext(avatar_url)}"
                logo_path = os.path.join(cache_dir, logo_name)
                expected_hash = previous_logo_hashes.get(logo_name)
                try:
                    trusted_existing_logo = (
                        bool(expected_hash) and _file_sha256(logo_path) == expected_hash
                    )
                except OSError:
                    trusted_existing_logo = False

                if force or not trusted_existing_logo:
                    staged_logo = os.path.join(staging_dir, logo_name)
                    if download_logo(avatar_url, staged_logo, force=True):
                        logo_hashes[logo_name] = _file_sha256(staged_logo)
                        publish_logos.append(logo_name)
                    elif trusted_existing_logo:
                        logo_hashes[logo_name] = expected_hash
                    else:
                        logo_name = ""
                else:
                    logo_hashes[logo_name] = expected_hash
                if logo_name:
                    current_files.add(logo_name)
            else:
                print(f"  [warn]  {name} has no avatarUrl, logo_icon will be empty")

            cache_data = {
                "name":        name,
                "version":     version,
                "logo_icon":   os.path.join("cache/store", logo_name) if logo_name else "",
                "class_name":  class_name,
                "description": description,
                "url":         url,
            }

            json_name = f"{base_name}.json"
            _write_json_atomic(os.path.join(staging_dir, json_name), cache_data)
            current_files.add(json_name)
            publish_json.append(json_name)
            written += 1

        # Publish logos before JSON so a new JSON never references a missing image.
        for name in publish_logos + publish_json:
            os.replace(os.path.join(staging_dir, name), os.path.join(cache_dir, name))
            print(f"  [{'logo' if name in publish_logos else 'json'}]  {os.path.join(cache_dir, name)}")

    _commit_manifest(cache_dir, previous_files, current_files, logo_hashes)
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
