#!/usr/bin/env python3

import importlib.util
import json
import os
import tempfile
import unittest
from unittest import mock


SCRIPT = os.path.join(
    os.path.dirname(__file__), "..", "APPLaunch", "bin", "store_cache_sync.py"
)
SPEC = importlib.util.spec_from_file_location("store_cache_sync", SCRIPT)
store_cache_sync = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(store_cache_sync)


class FakeResponse:
    def __init__(self, chunks, content_type="image/png", content_length=None):
        self.chunks = chunks
        self.headers = {"Content-Type": content_type}
        if content_length is not None:
            self.headers["Content-Length"] = str(content_length)

    def raise_for_status(self):
        pass

    def iter_content(self, chunk_size):
        del chunk_size
        yield from self.chunks


class StoreCacheSyncTests(unittest.TestCase):
    def test_first_sync_with_logo_creates_hash_manifest(self):
        items = [{"firmwareName": "App", "version": "1.0", "avatarUrl": "https://example/logo.png"}]
        with tempfile.TemporaryDirectory() as cache_dir, \
                mock.patch.object(store_cache_sync, "fetch_public_list", return_value=items), \
                mock.patch.object(store_cache_sync.requests, "get", return_value=FakeResponse([b"image-data"])):
            store_cache_sync.sync_cache(cache_dir)

            with open(os.path.join(cache_dir, store_cache_sync.MANIFEST_NAME), encoding="utf-8") as f:
                manifest = json.load(f)
            self.assertEqual(manifest["version"], 3)
            self.assertIn("App_1.0_logo.png", manifest["logo_hashes"])

    def test_interrupted_download_preserves_existing_logo(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            logo_path = os.path.join(cache_dir, "app_logo.png")
            with open(logo_path, "wb") as f:
                f.write(b"old-image")

            def interrupted_chunks():
                yield b"partial"
                raise OSError("connection lost")

            response = FakeResponse(interrupted_chunks())
            with mock.patch.object(store_cache_sync.requests, "get", return_value=response):
                self.assertFalse(store_cache_sync.download_logo("https://example/logo.png", logo_path, True))

            with open(logo_path, "rb") as f:
                self.assertEqual(f.read(), b"old-image")
            self.assertFalse(any(name.startswith(".store-logo-") for name in os.listdir(cache_dir)))

    def test_download_rejects_non_image_and_invalid_sizes(self):
        cases = [
            FakeResponse([b"html"], content_type="text/html", content_length=4),
            FakeResponse([], content_length=0),
            FakeResponse([b"short"], content_length=10),
            FakeResponse([b"x"], content_length=store_cache_sync.MAX_LOGO_SIZE + 1),
        ]
        with tempfile.TemporaryDirectory() as cache_dir:
            for index, response in enumerate(cases):
                path = os.path.join(cache_dir, f"logo-{index}.png")
                with mock.patch.object(store_cache_sync.requests, "get", return_value=response):
                    self.assertFalse(store_cache_sync.download_logo("https://example/logo.png", path))
                self.assertFalse(os.path.exists(path))

    def test_sync_cleans_stale_manifest_files_and_keeps_unmanaged_files(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            stale_json = "removed_1.0.json"
            stale_logo = "removed_1.0_logo.png"
            unmanaged = "manual.json"
            for name in (stale_json, stale_logo, unmanaged):
                with open(os.path.join(cache_dir, name), "w", encoding="utf-8") as f:
                    f.write("old")
            with open(os.path.join(cache_dir, store_cache_sync.MANIFEST_NAME), "w", encoding="utf-8") as f:
                json.dump({"version": 3, "files": [stale_json, stale_logo], "logo_hashes": {}}, f)

            items = [{
                "firmwareName": "No Avatar",
                "version": "2.0",
                "class": "tool",
                "remark": "description",
                "url": "https://example/app.bin",
            }]
            with mock.patch.object(store_cache_sync, "fetch_public_list", return_value=items):
                store_cache_sync.sync_cache(cache_dir)

            current_json = "No_Avatar_2.0.json"
            self.assertFalse(os.path.exists(os.path.join(cache_dir, stale_json)))
            self.assertFalse(os.path.exists(os.path.join(cache_dir, stale_logo)))
            self.assertTrue(os.path.exists(os.path.join(cache_dir, unmanaged)))
            with open(os.path.join(cache_dir, current_json), encoding="utf-8") as f:
                self.assertEqual(json.load(f)["logo_icon"], "")
            with open(os.path.join(cache_dir, store_cache_sync.MANIFEST_NAME), encoding="utf-8") as f:
                manifest = json.load(f)
                self.assertEqual(manifest["files"], [current_json])
                self.assertEqual(manifest["logo_hashes"], {})

    def test_first_manifest_cleans_recognized_legacy_cache(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            legacy_json = "removed_1.0.json"
            legacy_logo = "removed_1.0_logo.png"
            legacy_data = {
                "name": "removed",
                "version": "1.0",
                "logo_icon": f"cache/store/{legacy_logo}",
                "class_name": "tool",
                "description": "old",
                "url": "https://example/old.bin",
            }
            with open(os.path.join(cache_dir, legacy_json), "w", encoding="utf-8") as f:
                json.dump(legacy_data, f)
            with open(os.path.join(cache_dir, legacy_logo), "wb") as f:
                f.write(b"old logo")
            with open(os.path.join(cache_dir, "unrelated.json"), "w", encoding="utf-8") as f:
                json.dump({"unrelated": True}, f)

            with mock.patch.object(store_cache_sync, "fetch_public_list", return_value=[]):
                store_cache_sync.sync_cache(cache_dir)

            self.assertFalse(os.path.exists(os.path.join(cache_dir, legacy_json)))
            self.assertFalse(os.path.exists(os.path.join(cache_dir, legacy_logo)))
            self.assertTrue(os.path.exists(os.path.join(cache_dir, "unrelated.json")))

    def test_invalid_manifest_is_rebuilt_and_stale_files_are_cleaned(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            stale_json = "removed_1.0.json"
            stale_data = {
                "name": "removed", "version": "1.0", "logo_icon": "",
                "class_name": "tool", "description": "old", "url": "old.bin",
            }
            with open(os.path.join(cache_dir, stale_json), "w", encoding="utf-8") as f:
                json.dump(stale_data, f)
            with open(os.path.join(cache_dir, store_cache_sync.MANIFEST_NAME), "w", encoding="utf-8") as f:
                json.dump({"version": 3, "files": None}, f)

            with mock.patch.object(store_cache_sync, "fetch_public_list", return_value=[]):
                store_cache_sync.sync_cache(cache_dir)

            self.assertFalse(os.path.exists(os.path.join(cache_dir, stale_json)))

    def test_uncommitted_store_file_is_recovered_and_cleaned(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            orphan_json = "orphan_1.0.json"
            orphan_data = {
                "name": "orphan", "version": "1.0", "logo_icon": "",
                "class_name": "tool", "description": "old", "url": "old.bin",
            }
            with open(os.path.join(cache_dir, orphan_json), "w", encoding="utf-8") as f:
                json.dump(orphan_data, f)
            with open(os.path.join(cache_dir, store_cache_sync.MANIFEST_NAME), "w", encoding="utf-8") as f:
                json.dump({"version": 3, "files": [], "logo_hashes": {}}, f)

            with mock.patch.object(store_cache_sync, "fetch_public_list", return_value=[]):
                store_cache_sync.sync_cache(cache_dir)

            self.assertFalse(os.path.exists(os.path.join(cache_dir, orphan_json)))

    def test_uncommitted_logo_is_recovered_and_cleaned(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            orphan_logo = "orphan_1.0_logo.png"
            with open(os.path.join(cache_dir, orphan_logo), "wb") as f:
                f.write(b"orphan")
            with open(os.path.join(cache_dir, store_cache_sync.MANIFEST_NAME), "w", encoding="utf-8") as f:
                json.dump({"version": 3, "files": [], "logo_hashes": {}}, f)

            with mock.patch.object(store_cache_sync, "fetch_public_list", return_value=[]):
                store_cache_sync.sync_cache(cache_dir)

            self.assertFalse(os.path.exists(os.path.join(cache_dir, orphan_logo)))

    def test_colliding_safe_names_abort_instead_of_overwriting(self):
        items = [
            {"firmwareName": "a/b", "version": "1"},
            {"firmwareName": "a?b", "version": "1"},
        ]
        with tempfile.TemporaryDirectory() as cache_dir, \
                mock.patch.object(store_cache_sync, "fetch_public_list", return_value=items):
            with self.assertRaisesRegex(ValueError, "same cache name"):
                store_cache_sync.sync_cache(cache_dir)

    def test_staging_failure_does_not_publish_partial_generation(self):
        items = [
            {"firmwareName": "First", "version": "1"},
            {"firmwareName": "Second", "version": "1"},
        ]
        with tempfile.TemporaryDirectory() as cache_dir:
            first_path = os.path.join(cache_dir, "First_1.json")
            with open(first_path, "w", encoding="utf-8") as f:
                f.write("old-generation")
            real_write = store_cache_sync._write_json_atomic
            writes = 0

            def fail_second_write(path, data):
                nonlocal writes
                writes += 1
                if writes == 2:
                    raise OSError("disk full")
                real_write(path, data)

            with mock.patch.object(store_cache_sync, "fetch_public_list", return_value=items), \
                    mock.patch.object(store_cache_sync, "_write_json_atomic", side_effect=fail_second_write):
                with self.assertRaisesRegex(OSError, "disk full"):
                    store_cache_sync.sync_cache(cache_dir)

            with open(first_path, encoding="utf-8") as f:
                self.assertEqual(f.read(), "old-generation")
            self.assertFalse(os.path.exists(os.path.join(cache_dir, "Second_1.json")))

    def test_legacy_manifest_does_not_trust_existing_logo(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            logo_name = "App_1.0_logo.png"
            with open(os.path.join(cache_dir, logo_name), "wb") as f:
                f.write(b"partial")
            with open(os.path.join(cache_dir, store_cache_sync.MANIFEST_NAME), "w", encoding="utf-8") as f:
                json.dump({"version": 1, "files": [logo_name]}, f)
            items = [{"firmwareName": "App", "version": "1.0", "avatarUrl": "https://example/logo.png"}]

            with mock.patch.object(store_cache_sync, "fetch_public_list", return_value=items), \
                    mock.patch.object(store_cache_sync.requests, "get", return_value=FakeResponse([b"complete"])) as get:
                store_cache_sync.sync_cache(cache_dir)

            get.assert_called_once()
            with open(os.path.join(cache_dir, logo_name), "rb") as f:
                self.assertEqual(f.read(), b"complete")


if __name__ == "__main__":
    unittest.main()
