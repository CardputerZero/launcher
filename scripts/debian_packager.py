#!/usr/bin/env python3
"""Build project Debian packages on Linux, macOS, or Windows.

APPLaunch remains the default target, while CLI options allow other projects in
this repository to reuse the same cross-platform package builder.
"""

from __future__ import annotations

import argparse
import io
import os
import platform
import shutil
import subprocess
import sys
import tarfile
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path, PurePosixPath
from typing import Iterable, Sequence


DEFAULT_PROJECT = "APPLaunch"
PACKAGE_NAME = "applaunch"
APP_NAME = "APPLaunch"
BIN_NAME = "M5CardputerZero-APPLaunch"
DEFAULT_VERSION = "0.2.1"
DEFAULT_REVISION = "m5stack1"
DEFAULT_ARCHITECTURE = "arm64"

INSTALL_PARENT = PurePosixPath("usr/share")
SERVICE_PATH = PurePosixPath("usr/lib/systemd/user")

OPTIONAL_BINARIES = (
    "M5CardputerZero-AppStore",
    "appstore.py",
    "M5CardputerZero-Calculator",
)
APPSTORE_IMAGE_PATTERNS = ("store_wordmark.png", "store_arrow_*.png")


class PackError(RuntimeError):
    """Raised when the package cannot be assembled."""


@dataclass(frozen=True)
class PackageConfig:
    version: str = DEFAULT_VERSION
    revision: str = DEFAULT_REVISION
    architecture: str = DEFAULT_ARCHITECTURE
    package_name: str = PACKAGE_NAME
    app_name: str = APP_NAME
    bin_name: str = BIN_NAME
    maintainer: str = "dianjixz <dianjixz@m5stack.com>"
    original_maintainer: str = "m5stack <m5stack@m5stack.com>"
    section: str = APP_NAME
    priority: str = "optional"
    homepage: str = "https://www.m5stack.com"
    description: str = "M5CardputerZero APPLaunch"

    @property
    def install_prefix(self) -> PurePosixPath:
        return INSTALL_PARENT / self.app_name

    @property
    def bin_path(self) -> PurePosixPath:
        return self.install_prefix / "bin"

    @property
    def service_path(self) -> PurePosixPath:
        return SERVICE_PATH

    @property
    def file_name(self) -> str:
        return f"{self.package_name}_{self.version}-{self.revision}_{self.architecture}.deb"


@dataclass(frozen=True)
class Paths:
    repo_root: Path
    tool_dir: Path
    project_dir: Path
    src_dir: Path
    output_dir: Path
    work_dir: Path
    package_root: Path
    package_file: Path


def _posix_path(path: PurePosixPath | str) -> str:
    return str(path).replace("\\", "/")


def _resolve_path(path: str | os.PathLike[str], base: Path) -> Path:
    candidate = Path(path).expanduser()
    if not candidate.is_absolute():
        candidate = base / candidate
    return candidate.resolve()


def _safe_remove(path: Path) -> None:
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def _chmod(path: Path, mode: int) -> None:
    try:
        path.chmod(mode)
    except PermissionError:
        if platform.system() != "Windows":
            raise


def _mkdir(root: Path, relative: PurePosixPath | str) -> Path:
    target = root / Path(*PurePosixPath(relative).parts)
    target.mkdir(parents=True, exist_ok=True)
    return target


def _copy_file(src: Path, dst: Path, mode: int | None = None) -> None:
    if not src.is_file():
        raise PackError(f"required file not found: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    if mode is not None:
        _chmod(dst, mode)


def _find_binary(src_dir: Path, bin_name: str) -> Path:
    for candidate in (src_dir / bin_name, src_dir / "bin" / bin_name):
        if candidate.is_file():
            return candidate
    raise PackError(f"binary {bin_name} not found in {src_dir} or {src_dir / 'bin'}")


def _default_app_tree(project_dir: Path, src_dir: Path, app_name: str) -> Path:
    for candidate in (project_dir / app_name, src_dir / app_name):
        if candidate.is_dir():
            return candidate
    raise PackError(
        f"{app_name} resource tree not found. Expected one of: "
        f"{project_dir / app_name}, {src_dir / app_name}"
    )


def _copy_tree(src: Path, dst: Path) -> None:
    if not src.is_dir():
        raise PackError(f"required directory not found: {src}")
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst, symlinks=True)


def _copy_optional_binaries(src_dir: Path, package_root: Path, config: PackageConfig) -> list[str]:
    copied: list[str] = []
    for name in OPTIONAL_BINARIES:
        candidates = (src_dir / "bin" / name, src_dir / name)
        source = next((path for path in candidates if path.is_file()), None)
        if source is None:
            continue
        mode = 0o644 if name.endswith(".py") else 0o755
        _copy_file(source, package_root / Path(*config.bin_path.parts) / name, mode=mode)
        copied.append(name)
    return copied


def _copy_appstore_images(project_dir: Path, app_dst: Path) -> list[str]:
    images_src = project_dir.parent / "AppStore" / "share" / "images"
    if not images_src.is_dir():
        return []
    images_dst = app_dst / "share" / "images"
    images_dst.mkdir(parents=True, exist_ok=True)
    copied: list[str] = []
    for pattern in APPSTORE_IMAGE_PATTERNS:
        for image in sorted(images_src.glob(pattern)):
            if image.is_file():
                shutil.copy2(image, images_dst / image.name)
                copied.append(image.name)
    return copied


def _control_text(config: PackageConfig) -> str:
    fields = {
        "Package": config.package_name,
        "Version": config.version,
        "Architecture": config.architecture,
        "Maintainer": config.maintainer,
        "Original-Maintainer": config.original_maintainer,
        "Section": config.section,
        "Priority": config.priority,
        "Homepage": config.homepage,
        "Packaged-Date": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "Description": config.description,
    }
    return "".join(f"{key}: {value}\n" for key, value in fields.items())


def _postinst_text(config: PackageConfig) -> str:
    service_file = f"/{_posix_path(config.service_path / f'{config.app_name}.service')}"
    return f"""#!/bin/sh
set -e
mkdir -p /var/cache/{config.app_name}
ln -sfn /var/cache/{config.app_name} /usr/share/{config.app_name}/cache
APP_UID=1000
APP_USER="$(getent passwd "$APP_UID" | cut -d: -f1)"
if command -v systemctl >/dev/null 2>&1 && [ -n "$APP_USER" ] && [ -f "{service_file}" ]; then
    if command -v loginctl >/dev/null 2>&1; then
        loginctl enable-linger "$APP_USER" || true
    fi
    systemctl daemon-reload || true
    systemctl start "user@$APP_UID.service" || true
    runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" systemctl --user daemon-reload || true
    runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" systemctl --user enable {config.app_name}.service || true
    runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" systemctl --user restart {config.app_name}.service || \
        runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" systemctl --user start {config.app_name}.service || true
else
    echo "{config.app_name}: UID 1000 user not found or service file missing; skip user service enable/start" >&2
fi
exit 0
"""


def _prerm_text(config: PackageConfig) -> str:
    service_file = f"/{_posix_path(config.service_path / f'{config.app_name}.service')}"
    return f"""#!/bin/sh
set -e
APP_UID=1000
APP_USER="$(getent passwd "$APP_UID" | cut -d: -f1)"
if command -v systemctl >/dev/null 2>&1 && [ -n "$APP_USER" ] && [ -f "{service_file}" ]; then
    runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" systemctl --user stop {config.app_name}.service || true
    runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" systemctl --user disable {config.app_name}.service || true
fi
rm -rf /var/cache/{config.app_name}
exit 0
"""


def _service_text(config: PackageConfig) -> str:
    return f"""[Unit]
Description={config.app_name} Service
After=pipewire-pulse.service
Wants=pipewire-pulse.service

[Service]
ExecStart=/{_posix_path(config.bin_path / config.bin_name)}
WorkingDirectory=/{_posix_path(config.install_prefix)}
Restart=always
RestartSec=1
StartLimitInterval=0

[Install]
WantedBy=default.target
"""


def _write_text(path: Path, text: str, mode: int = 0o644) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    _chmod(path, mode)


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def _default_project_dir(repo_root: Path, project: str) -> Path:
    candidate = repo_root / "projects" / project
    if candidate.is_dir():
        return candidate
    return _resolve_path(project, repo_root)


def _prepare_paths(
    src_folder: str | os.PathLike[str],
    output_dir: str | os.PathLike[str] | None,
    work_dir: str | os.PathLike[str] | None,
    config: PackageConfig,
    project: str = DEFAULT_PROJECT,
    project_dir: str | os.PathLike[str] | None = None,
) -> Paths:
    repo_root = _repo_root()
    tool_dir = Path(__file__).resolve().parent
    resolved_project_dir = (
        _resolve_path(project_dir, repo_root)
        if project_dir
        else _default_project_dir(repo_root, project)
    )
    src_dir = _resolve_path(src_folder, resolved_project_dir)
    out_dir = _resolve_path(output_dir or resolved_project_dir / "tools", repo_root)
    staging_parent = _resolve_path(work_dir or out_dir, repo_root)
    package_root = staging_parent / f"debian-{config.app_name}"
    return Paths(
        repo_root=repo_root,
        tool_dir=tool_dir,
        project_dir=resolved_project_dir,
        src_dir=src_dir,
        output_dir=out_dir,
        work_dir=staging_parent,
        package_root=package_root,
        package_file=out_dir / config.file_name,
    )


def prepare_package_tree(
    config: PackageConfig,
    paths: Paths,
    app_tree: str | os.PathLike[str] | None = None,
) -> None:
    if paths.package_root.exists():
        shutil.rmtree(paths.package_root)

    for directory in (
        PurePosixPath("DEBIAN"),
        config.bin_path,
        config.service_path,
    ):
        _mkdir(paths.package_root, directory)

    app_src = _resolve_path(app_tree, paths.project_dir) if app_tree else _default_app_tree(paths.project_dir, paths.src_dir, config.app_name)
    app_dst = paths.package_root / Path(*config.install_prefix.parts)
    _copy_tree(app_src, app_dst)

    binary = _find_binary(paths.src_dir, config.bin_name)
    _copy_file(binary, paths.package_root / Path(*config.bin_path.parts) / config.bin_name, mode=0o755)

    copied_bins = _copy_optional_binaries(paths.src_dir, paths.package_root, config)
    copied_images = (
        _copy_appstore_images(paths.project_dir, app_dst)
        if config.app_name == APP_NAME
        else []
    )

    _write_text(paths.package_root / "DEBIAN" / "control", _control_text(config))
    _write_text(paths.package_root / "DEBIAN" / "postinst", _postinst_text(config), mode=0o755)
    _write_text(paths.package_root / "DEBIAN" / "prerm", _prerm_text(config), mode=0o755)
    _write_text(
        paths.package_root / Path(*config.service_path.parts) / f"{config.app_name}.service",
        _service_text(config),
    )

    print(f"Staged package tree: {paths.package_root}")
    print(f"  binary: {binary}")
    print(f"  app tree: {app_src}")
    if copied_bins:
        print(f"  optional binaries: {', '.join(copied_bins)}")
    if copied_images:
        print(f"  AppStore images: {', '.join(copied_images)}")


def _tar_filter(tar_info: tarfile.TarInfo) -> tarfile.TarInfo:
    tar_info.uid = 0
    tar_info.gid = 0
    tar_info.uname = "root"
    tar_info.gname = "root"
    if tar_info.isdir():
        tar_info.mode = 0o755
    elif tar_info.mode & 0o111:
        tar_info.mode = 0o755
    else:
        tar_info.mode = 0o644
    return tar_info


def _tar_tree(root: Path, names: Iterable[str]) -> bytes:
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz", format=tarfile.GNU_FORMAT) as tar:
        for name in names:
            source = root / name
            if not source.exists():
                continue
            tar.add(source, arcname=name, recursive=True, filter=_tar_filter)
    return buffer.getvalue()


def _data_members(package_root: Path) -> list[str]:
    return sorted(
        entry.name for entry in package_root.iterdir() if entry.name != "DEBIAN"
    )


def _ar_member_header(name: str, size: int, mode: int = 0o100644) -> bytes:
    if len(name) > 15:
        raise PackError(f"ar member name too long: {name}")
    header = (
        f"{name + '/':<16}"
        f"{int(datetime.now().timestamp()):<12}"
        f"{0:<6}"
        f"{0:<6}"
        f"{format(mode, 'o'):<8}"
        f"{size:<10}`\n"
    )
    return header.encode("ascii")


def _write_ar_member(handle, name: str, data: bytes) -> None:
    handle.write(_ar_member_header(name, len(data)))
    handle.write(data)
    if len(data) % 2:
        handle.write(b"\n")


def build_deb_with_python(package_root: Path, deb_file: Path) -> None:
    control_tar = _tar_tree(package_root / "DEBIAN", ("control", "postinst", "prerm"))
    data_tar = _tar_tree(package_root, _data_members(package_root))

    deb_file.parent.mkdir(parents=True, exist_ok=True)
    with deb_file.open("wb") as handle:
        handle.write(b"!<arch>\n")
        _write_ar_member(handle, "debian-binary", b"2.0\n")
        _write_ar_member(handle, "control.tar.gz", control_tar)
        _write_ar_member(handle, "data.tar.gz", data_tar)


def build_deb_with_dpkg(package_root: Path, deb_file: Path) -> None:
    deb_file.parent.mkdir(parents=True, exist_ok=True)
    command = ["dpkg-deb", "--root-owner-group", "-b", str(package_root), str(deb_file)]
    subprocess.run(command, check=True)


def build_deb(package_root: Path, deb_file: Path, builder: str = "auto") -> str:
    selected = builder
    if builder == "auto":
        selected = "dpkg-deb" if shutil.which("dpkg-deb") else "python"

    if selected == "dpkg-deb":
        build_deb_with_dpkg(package_root, deb_file)
    elif selected == "python":
        build_deb_with_python(package_root, deb_file)
    else:
        raise PackError(f"unsupported builder: {builder}")
    return selected


def create_deb_package(
    version: str = DEFAULT_VERSION,
    src_folder: str | os.PathLike[str] = "dist",
    revision: str = DEFAULT_REVISION,
    architecture: str = DEFAULT_ARCHITECTURE,
    output_dir: str | os.PathLike[str] | None = None,
    work_dir: str | os.PathLike[str] | None = None,
    builder: str = "auto",
    app_tree: str | os.PathLike[str] | None = None,
    keep_staging: bool = True,
    project: str = DEFAULT_PROJECT,
    project_dir: str | os.PathLike[str] | None = None,
    package_name: str = PACKAGE_NAME,
    app_name: str = APP_NAME,
    bin_name: str = BIN_NAME,
) -> str:
    """Build a project as a Debian package and return the output path."""
    config = PackageConfig(
        version=version,
        revision=revision,
        architecture=architecture,
        package_name=package_name,
        app_name=app_name,
        bin_name=bin_name,
        section=app_name,
        description=f"M5CardputerZero {app_name}",
    )
    paths = _prepare_paths(src_folder, output_dir, work_dir, config, project, project_dir)

    print(f"Creating Debian package {config.file_name} ...")
    prepare_package_tree(config, paths, app_tree=app_tree)
    selected_builder = build_deb(paths.package_root, paths.package_file, builder=builder)

    if not keep_staging:
        shutil.rmtree(paths.package_root)

    print(f"Debian package created: {paths.package_file}")
    print(f"Builder: {selected_builder}")
    return str(paths.package_file)


def create_applaunch_deb(**kwargs) -> str:
    """Compatibility wrapper for older callers that imported this helper."""
    return create_deb_package(**kwargs)


def clean_outputs(output_dir: Path, app_name: str, distclean: bool = False) -> None:
    patterns = ["*.deb", f"debian-{app_name}"]
    if distclean:
        patterns.append("m5stack_*")
    for pattern in patterns:
        for path in output_dir.glob(pattern):
            _safe_remove(path)
            print(f"removed: {path}")


def resolve_output_dir(
    project: str,
    project_dir: str | os.PathLike[str] | None,
    output_dir: str | os.PathLike[str] | None,
) -> Path:
    repo_root = _repo_root()
    resolved_project_dir = (
        _resolve_path(project_dir, repo_root)
        if project_dir
        else _default_project_dir(repo_root, project)
    )
    return _resolve_path(output_dir or resolved_project_dir / "tools", repo_root)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Package repository projects into Debian .deb files on Linux, macOS, or Windows."
    )
    subparsers = parser.add_subparsers(dest="command")

    build = subparsers.add_parser("build", help="build the Debian package")
    build.add_argument("--project", default=DEFAULT_PROJECT, help="project name under projects/ or a project path")
    build.add_argument("--project-dir", default=None, help="explicit project directory; overrides --project")
    build.add_argument("--package-name", default=PACKAGE_NAME, help="Debian package name")
    build.add_argument("--app-name", default=APP_NAME, help="installed application name under /usr/share")
    build.add_argument("--bin-name", default=BIN_NAME, help="main executable name")
    build.add_argument("--version", default=DEFAULT_VERSION, help="package version")
    build.add_argument("--revision", default=DEFAULT_REVISION, help="Debian package revision")
    build.add_argument("--architecture", default=DEFAULT_ARCHITECTURE, help="Debian architecture")
    build.add_argument("--src", "--src-folder", dest="src", default="dist", help="dist directory containing the built binary; relative paths are resolved from the project directory")
    build.add_argument("--app-tree", default=None, help="resource tree to install as /usr/share/<app-name>")
    build.add_argument("--output-dir", default=None, help="directory for the generated .deb")
    build.add_argument("--work-dir", default=None, help="directory for the staging tree")
    build.add_argument(
        "--builder",
        choices=("auto", "python", "dpkg-deb"),
        default="auto",
        help="package writer to use; auto prefers dpkg-deb when available",
    )
    build.add_argument("--remove-staging", action="store_true", help="delete staging tree after build")

    def add_clean_args(subparser: argparse.ArgumentParser) -> None:
        subparser.add_argument("--project", default=DEFAULT_PROJECT, help="project name under projects/ or a project path")
        subparser.add_argument("--project-dir", default=None, help="explicit project directory; overrides --project")
        subparser.add_argument("--app-name", default=APP_NAME, help="installed application name under /usr/share")
        subparser.add_argument("--output-dir", default=None, help="directory containing generated package artifacts")

    clean = subparsers.add_parser("clean", help="remove generated .deb files and staging tree")
    add_clean_args(clean)
    distclean = subparsers.add_parser("distclean", help="clean plus legacy m5stack_* outputs")
    add_clean_args(distclean)

    # Backward compatibility: bare execution still builds the APPLaunch package.
    normalized = list(argv)
    if not normalized:
        normalized = ["build"]
    elif normalized[0].startswith("-") and normalized[0] not in ("-h", "--help"):
        normalized.insert(0, "build")
    return parser.parse_args(normalized)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if args.command == "clean":
            clean_outputs(
                resolve_output_dir(args.project, args.project_dir, args.output_dir),
                args.app_name,
            )
            return 0
        if args.command == "distclean":
            clean_outputs(
                resolve_output_dir(args.project, args.project_dir, args.output_dir),
                args.app_name,
                distclean=True,
            )
            return 0

        create_deb_package(
            version=args.version,
            src_folder=args.src,
            revision=args.revision,
            architecture=args.architecture,
            output_dir=args.output_dir,
            work_dir=args.work_dir,
            builder=args.builder,
            app_tree=args.app_tree,
            keep_staging=not args.remove_staging,
            project=args.project,
            project_dir=args.project_dir,
            package_name=args.package_name,
            app_name=args.app_name,
            bin_name=args.bin_name,
        )
        return 0
    except (OSError, subprocess.CalledProcessError, PackError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
