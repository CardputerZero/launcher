# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

"""main.cpp's bash_init copies the sample files into ~/Downloads once per user.

The command is taken from the source and run the way system() runs it (/bin/sh -c), against
a temporary HOME and a temporary template.zip, so nothing on the host is touched.
"""

import os
import re
import shutil
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path

MAIN_CPP = Path(__file__).resolve().parents[1] / "main" / "src" / "main.cpp"
SAMPLES = {"hello.md": "# hello\n", "quine.py": "print('q')\n", "epoch.txt": "1970\n"}
MARKER = Path(".local/state/APPLaunch/samples-installed")


def bash_init_command():
    source = MAIN_CPP.read_text(encoding="utf-8")
    match = re.search(r"const char \*bash_init\s*=\s*((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*;", source)
    if match is None:
        raise AssertionError("bash_init not found in main.cpp")
    pieces = re.findall(r"\"((?:[^\"\\]|\\.)*)\"", match.group(1))
    return "".join(pieces).encode("utf-8").decode("unicode_escape")


@unittest.skipUnless(shutil.which("unzip"), "unzip is not installed")
class SampleFilesInstallOnceTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.home = root / "home"
        self.home.mkdir()
        self.downloads = self.home / "Downloads"
        self.template = root / "template.zip"
        with zipfile.ZipFile(self.template, "w") as archive:
            for name, text in SAMPLES.items():
                archive.writestr(name, text)
        command = bash_init_command()
        self.assertIn("/var/template.zip", command)
        self.command = command.replace("/var/template.zip", str(self.template))

    def boot(self):
        env = {"HOME": str(self.home), "PATH": os.environ.get("PATH", "/usr/bin:/bin")}
        result = subprocess.run(["/bin/sh", "-c", self.command], env=env, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)

    def present(self):
        return sorted(p.name for p in self.downloads.iterdir()) if self.downloads.exists() else []

    def test_first_boot_installs_the_samples(self):
        self.boot()
        self.assertEqual(self.present(), sorted(SAMPLES))
        self.assertTrue((self.home / MARKER).exists())

    def test_deleted_samples_stay_deleted(self):
        self.boot()
        for name in SAMPLES:
            (self.downloads / name).unlink()
        self.boot()
        self.assertEqual(self.present(), [])

    def test_deleting_only_hello_md_brings_nothing_back(self):
        self.boot()
        (self.downloads / "hello.md").unlink()
        (self.downloads / "quine.py").unlink()
        self.boot()
        self.assertEqual(self.present(), ["epoch.txt"])

    def test_existing_home_with_samples_is_marked_not_reextracted(self):
        # A device set up before the marker existed: samples present, one of them edited.
        self.downloads.mkdir()
        (self.downloads / "hello.md").write_text("mine\n", encoding="utf-8")
        self.boot()
        self.assertEqual(self.present(), ["hello.md"])
        self.assertEqual((self.downloads / "hello.md").read_text(encoding="utf-8"), "mine\n")
        self.assertTrue((self.home / MARKER).exists())

    def test_missing_template_is_retried_at_the_next_boot(self):
        self.template.rename(self.template.with_suffix(".away"))
        self.boot()
        self.assertEqual(self.present(), [])
        self.assertFalse((self.home / MARKER).exists())
        self.template.with_suffix(".away").rename(self.template)
        self.boot()
        self.assertEqual(self.present(), sorted(SAMPLES))

    def test_extraction_never_overwrites_a_file(self):
        self.downloads.mkdir()
        (self.downloads / "quine.py").write_text("mine\n", encoding="utf-8")
        self.boot()
        self.assertEqual((self.downloads / "quine.py").read_text(encoding="utf-8"), "mine\n")
        self.assertEqual(self.present(), sorted(SAMPLES))


if __name__ == "__main__":
    unittest.main()
