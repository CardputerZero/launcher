import tempfile
import unittest
from pathlib import Path

from build_support.config_default_file import GENERATED_CONFIG_FILES
from build_support.config_default_file import sync_config_default, sync_config_tmp


class ConfigDefaultFileTest(unittest.TestCase):
    def test_selection_change_invalidates_generated_config_once(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            config_dir = Path(temporary_directory)
            sdl_config = config_dir / "sdl.mk"
            cross_config = config_dir / "cross.mk"
            sdl_config.write_text("MODE=sdl\n", encoding="utf-8")
            cross_config.write_text("MODE=cross\n", encoding="utf-8")
            self.assertEqual(sync_config_default(config_dir, sdl_config), ())
            stamp = config_dir / "config_default_file"
            stamp_mtime = stamp.stat().st_mtime_ns

            for filename in GENERATED_CONFIG_FILES:
                (config_dir / filename).write_text("generated\n", encoding="utf-8")

            self.assertEqual(sync_config_default(config_dir, sdl_config), ())
            self.assertEqual(stamp.stat().st_mtime_ns, stamp_mtime)
            for filename in GENERATED_CONFIG_FILES:
                self.assertTrue((config_dir / filename).exists())

            self.assertEqual(
                sync_config_default(config_dir, cross_config),
                GENERATED_CONFIG_FILES,
            )
            self.assertEqual(
                stamp.read_text(encoding="utf-8"),
                str(cross_config.resolve()) + "\nMODE=cross\n",
            )
            for filename in GENERATED_CONFIG_FILES:
                self.assertFalse((config_dir / filename).exists())

    def test_content_change_invalidates_same_selection(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            config_dir = Path(temporary_directory)
            selected_config = config_dir / "defaults.mk"
            selected_config.write_text("VALUE=1\n", encoding="utf-8")
            self.assertEqual(sync_config_default(config_dir, selected_config), ())

            for filename in GENERATED_CONFIG_FILES:
                (config_dir / filename).write_text("generated\n", encoding="utf-8")
            selected_config.write_text("VALUE=2\n", encoding="utf-8")

            self.assertEqual(
                sync_config_default(config_dir, selected_config),
                GENERATED_CONFIG_FILES,
            )
            self.assertIn(
                "VALUE=2\n",
                (config_dir / "config_default_file").read_text(encoding="utf-8"),
            )

    def test_cross_config_tmp_is_content_stable_and_removed_for_sdl(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            config_dir = Path(temporary_directory)
            content = 'CONFIG_TOOLCHAIN_SYSROOT="/sdk"\n'
            self.assertTrue(sync_config_tmp(config_dir, content))
            config_tmp = config_dir / "config_tmp.mk"
            config_tmp_mtime = config_tmp.stat().st_mtime_ns
            self.assertFalse(sync_config_tmp(config_dir, content))
            self.assertEqual(config_tmp.stat().st_mtime_ns, config_tmp_mtime)
            self.assertTrue(sync_config_tmp(config_dir, None))
            self.assertFalse(config_tmp.exists())
            self.assertFalse(sync_config_tmp(config_dir, None))


if __name__ == "__main__":
    unittest.main()
