#!/usr/bin/env python3
import importlib.util
import sys
from pathlib import Path

repo = Path(__file__).resolve().parents[3]
module_path = repo / "scripts" / "debian_packager.py"
spec = importlib.util.spec_from_file_location("debian_packager", module_path)
packager = importlib.util.module_from_spec(spec)
assert spec.loader
sys.modules[spec.name] = packager
spec.loader.exec_module(packager)

postinst = packager._postinst_text(packager.PackageConfig())
assert "/usr/share/APPLaunch/adb/cardputer-adb" in postinst
assert '"$ADB_HELPER" migrate' in postinst

system_postinst = packager._postinst_text(packager.PackageConfig(service_scope="system"))
assert '"$ADB_HELPER" migrate' in system_postinst

other = packager.PackageConfig(app_name="OtherApp", package_name="other")
assert "cardputer-adb" not in packager._postinst_text(other)
