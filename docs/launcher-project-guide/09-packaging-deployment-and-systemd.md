# 09 - Packaging, Deployment, and systemd

This chapter explains how APPLaunch is packaged from the `dist` directory into a Debian `.deb`, how it is deployed to M5CardputerZero, how systemd autostart is configured, and how to verify and troubleshoot deployment issues.

All commands are assumed to start from the repository root by default:

```bash
cd /home/nihao/w2T/github/launcher
```

## 1. Deployment Form Overview

APPLaunch depends on two types of files on the device:

1. Main program: `M5CardputerZero-APPLaunch`.
2. Runtime resource tree: `APPLaunch/`, including application descriptors, fonts, images, audio, scripts, and optional sub-applications.

After formal installation, the target path is:

```text
/usr/share/APPLaunch/
├── applications/
├── bin/
│   ├── M5CardputerZero-APPLaunch
│   ├── M5CardputerZero-AppStore              # packaged if it exists in dist/bin
│   ├── M5CardputerZero-Calculator            # packaged if it exists in dist/bin
│   └── appstore.py                           # packaged if it exists in dist/bin
├── lib/
├── share/
│   ├── font/
│   └── images/
└── cache -> /var/cache/APPLaunch             # created by postinst
```

The systemd service file is installed to:

```text
/lib/systemd/system/APPLaunch.service
```

The service start command is:

```text
/usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

The working directory is:

```text
/usr/share/APPLaunch
```

## 2. Build the Device Target Before Packaging

The `.deb` should use arm64 device artifacts, not Linux SDL2 x86_64 simulation artifacts.

Recommended cross-compilation on a Linux x86_64 development machine:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
file dist/M5CardputerZero-APPLaunch
```

The `file` result should contain:

```text
ARM aarch64
```

If it shows `x86-64`, you are packaging an SDL2 host artifact, which cannot be installed on the device as the formal launcher.

Native device builds can also be used for packaging:

```bash
cd /home/pi/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=config_defaults.mk
scons -j2
file dist/M5CardputerZero-APPLaunch
```

## 3. `llm_pack.py` Packaging Script

The packaging script is located at:

```text
projects/APPLaunch/tools/llm_pack.py
```

Core constants:

| Constant | Value | Description |
| --- | --- | --- |
| `PACKAGE_NAME` | `applaunch` | Debian package name |
| `APP_NAME` | `APPLaunch` | Base application and service name |
| `BIN_NAME` | `M5CardputerZero-APPLaunch` | Main executable name |
| `INSTALL_PPREFIX` | `usr/share` | Parent installation prefix |
| `INSTALL_PREFIX` | `usr/share/APPLaunch` | Application installation root |
| `BIN_PATH` | `usr/share/APPLaunch/bin` | Executable directory |
| `LIB_PATH` | `usr/share/APPLaunch/lib` | Dynamic-library directory |
| `SHARE_PATH` | `usr/share/APPLaunch/share` | Shared-resource directory |
| `APP_PATH` | `usr/share/APPLaunch/applications` | `.desktop` application descriptor directory |
| `SERVICE_PATH` | `lib/systemd/system` | systemd service directory |

Default version information at the script entry point:

```python
version = '0.2.1'
src_folder = '../dist'
revision = 'm5stack1'
```

Generated package filename format:

```text
applaunch_0.2.1-m5stack1_arm64.deb
```

## 4. `.deb` Package Directory Structure

After running the script, a temporary directory is generated under `projects/APPLaunch/tools`:

```text
projects/APPLaunch/tools/debian-APPLaunch/
├── DEBIAN/
│   ├── control
│   ├── postinst
│   └── prerm
├── lib/
│   └── systemd/
│       └── system/
│           └── APPLaunch.service
└── usr/
    └── share/
        └── APPLaunch/
            ├── applications/
            ├── bin/
            │   └── M5CardputerZero-APPLaunch
            ├── lib/
            └── share/
                ├── font/
                └── images/
```

The final `.deb` file is located at:

```text
projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb
```

## 5. Packaging Commands

### 5.1 Install Packaging Tools

Linux development machine:

```bash
sudo apt update
sudo apt install -y dpkg-dev fakeroot
```

Only `dpkg-deb` is required:

```bash
dpkg-deb --version
```

### 5.2 Run Packaging

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch/tools
python3 llm_pack.py
```

On success, output similar to the following appears:

```text
Creating Debian package applaunch 0.2.1 ...
Debian package created: .../applaunch_0.2.1-m5stack1_arm64.deb
applaunch create success!
```

### 5.3 Specify a Custom Version

The current script entry point uses fixed values `0.2.1` and `m5stack1`. To build a temporary custom version without modifying repository files, call the function directly from Python:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch/tools
python3 - <<'PY'
from llm_pack import create_applaunch_deb
print(create_applaunch_deb(version='0.2.1', src_folder='../dist', revision='m5stack1'))
PY
```

If the version number needs to change long-term, make a formal code change to `llm_pack.py` and update the release notes accordingly.

### 5.4 Clean Packaging Artifacts

The script supports:

```bash
python3 llm_pack.py clean
python3 llm_pack.py distclean
```

Differences:

| Command | Behavior |
| --- | --- |
| `clean` | Deletes `*.deb` in the current directory and deletes first-level subdirectories under the current directory |
| `distclean` | Deletes `*.deb` and `m5stack_*` under the current directory |

Note: `clean` deletes first-level directories under `tools`, including temporary directories such as `debian-APPLaunch`. Do not run it from an unintended directory that contains important subdirectories.

## 6. Packaging Script Copy Rules

### 6.1 Main Program Lookup

The script looks for the main program under `src_folder`, which defaults to `../dist`.

Lookup order:

1. `../dist/M5CardputerZero-APPLaunch`
2. `../dist/bin/M5CardputerZero-APPLaunch`

If neither exists, it raises:

```text
FileNotFoundError: Binary M5CardputerZero-APPLaunch not found in ../dist
```

### 6.2 Additional Apps and Backends

The script attempts to include these optional files:

```text
../dist/bin/M5CardputerZero-AppStore
../dist/bin/appstore.py
../dist/bin/M5CardputerZero-Calculator
```

If present, they are copied to:

```text
/usr/share/APPLaunch/bin/
```

Non-`.py` files are set to `0755`.

### 6.3 Resource Tree Copy

The script preferentially copies the resource tree from source:

```text
projects/APPLaunch/APPLaunch
```

The target inside the package is:

```text
usr/share/APPLaunch
```

If the source resource tree does not exist, it tries:

```text
../dist/APPLaunch
```

This means packaging usually does not rely only on `dist/APPLaunch`; it also copies the `APPLaunch/` resource tree from the project source directory.

### 6.4 AppStore Image Additions

If this directory exists:

```text
projects/AppStore/share/images
```

The script copies the following images into `usr/share/APPLaunch/share/images` inside the package:

```text
store_wordmark.png
store_arrow_*.png
```

## 7. Debian Control Scripts

### 7.1 `DEBIAN/control`

The generated control file contains:

```text
Package: applaunch
Version: 0.2.1
Architecture: arm64
Maintainer: dianjixz <dianjixz@m5stack.com>
Original-Maintainer: m5stack <m5stack@m5stack.com>
Section: APPLaunch
Priority: optional
Homepage: https://www.m5stack.com
Packaged-Date: <packaging time>
Description: M5CardputerZero APPLaunch
```

Important points:

- `Architecture` is fixed to `arm64`.
- The script does not automatically declare `Depends`, so dependent libraries must be provided by the base image or declared in a future version.

### 7.2 `DEBIAN/postinst`

The post-install script runs:

```sh
mkdir -p /var/cache/APPLaunch
ln -s /var/cache/APPLaunch /usr/share/APPLaunch/cache
[ -f "/lib/systemd/system/APPLaunch.service" ] && systemctl enable APPLaunch.service
[ -f "/lib/systemd/system/APPLaunch.service" ] && systemctl start APPLaunch.service
exit 0
```

Purpose:

- Creates writable cache directory `/var/cache/APPLaunch`.
- Creates a `cache` symlink under the read-only/system resource directory.
- Enables and starts the systemd service.

Note: if `/usr/share/APPLaunch/cache` already exists, `ln -s` may fail. The current script does not use `ln -sfn`, so check install logs on repeated installation.

### 7.3 `DEBIAN/prerm`

The pre-removal script runs:

```sh
[ -f "/lib/systemd/system/APPLaunch.service" ] && systemctl stop APPLaunch.service
[ -f "/lib/systemd/system/APPLaunch.service" ] && systemctl disable APPLaunch.service
rm -rf /var/cache/APPLaunch
exit 0
```

Purpose:

- Stops the service.
- Disables autostart at boot.
- Deletes the cache directory.

Note: uninstalling deletes `/var/cache/APPLaunch`; any runtime cache or app-store cache stored there is removed as well.

## 8. systemd Service File

The script generates:

```ini
[Unit]
Description=APPLaunch Service

[Service]
ExecStart=/usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
WorkingDirectory=/usr/share/APPLaunch
Restart=always
RestartSec=1
StartLimitInterval=0

[Install]
WantedBy=multi-user.target
```

Field descriptions:

| Field | Description |
| --- | --- |
| `ExecStart` | Starts the APPLaunch main program |
| `WorkingDirectory` | Sets the current directory to `/usr/share/APPLaunch` for convenient relative-path access |
| `Restart=always` | Always restarts after the process exits |
| `RestartSec=1` | Restarts 1 second after exit |
| `StartLimitInterval=0` | Disables the default start-rate limit so systemd does not stop restarting after frequent crashes |
| `WantedBy=multi-user.target` | Starts with the multi-user target after enable |

The current service file does not explicitly set a user, so it runs as root as a systemd system service by default. This usually helps access framebuffer, evdev, GPIO, audio, and camera devices, but it also means the program has high privileges.

## 9. Install on the Device

### 9.1 Copy `.deb` to the Device

Assume the device IP is `192.168.28.177` and the username is `pi`:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch/tools
scp applaunch_0.2.1-m5stack1_arm64.deb pi@192.168.28.177:/home/pi/
```

### 9.2 Install on the Device

```bash
ssh pi@192.168.28.177
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

If the installer reports missing dependencies, fix dependencies first:

```bash
sudo apt-get -f install
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

### 9.3 Overwrite Installation

Install the same package name or a higher version again:

```bash
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

If the service is running, `postinst` attempts to enable/start it. To reduce framebuffer or input-device contention during installation, you can stop the service manually first:

```bash
sudo systemctl stop APPLaunch.service || true
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
sudo systemctl restart APPLaunch.service
```

## 10. Quick Deployment with `scons push`

In addition to `.deb`, the project supports uploading the `dist` directory through `setup.ini`.

Configuration file:

```text
projects/APPLaunch/setup.ini
```

Default content example:

```ini
[ssh]
local_file_path = dist
remote_file_path = /home/pi/dist
remote_host = 192.168.28.177
remote_port = 22
username = pi
password = pi
; before_cmd = 'echo pi |  sudo -S systemctl stop APPLaunch.service'
; after_cmd = 'echo pi |  sudo -S systemctl stop APPLaunch.service; echo pi |  sudo -S cp /home/pi/dist/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin ; echo pi |  sudo -S systemctl start APPLaunch.service'
```

Run:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons push
```

`SDK/tools/scons/push.py` will:

1. Read `setup.ini`.
2. Iterate over all files under `local_file_path`.
3. Calculate local MD5 hashes.
4. Fetch remote file MD5 hashes through SSH.
5. Upload only changed files.
6. Optionally run `before_cmd` and `after_cmd`.

Suitable for:

- Quickly replacing `dist` during development.
- Quickly uploading a single build result.
- Situations where Debian install scripts do not need to be tested.

Not suitable for:

- Verifying the formal installation path.
- Verifying `postinst` and `prerm`.
- Verifying systemd enable/install behavior.
- Generating a distributable installation package.

## 11. Manual Deployment

If you do not want to use `.deb` or `scons push`, you can copy files manually.

Upload from the development machine:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scp dist/M5CardputerZero-APPLaunch pi@192.168.28.177:/home/pi/
scp -r dist/APPLaunch pi@192.168.28.177:/home/pi/APPLaunch-new
```

Install on the device:

```bash
sudo systemctl stop APPLaunch.service || true
sudo mkdir -p /usr/share/APPLaunch/bin
sudo install -m 0755 /home/pi/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
sudo rsync -a --delete /home/pi/APPLaunch-new/ /usr/share/APPLaunch/
sudo mkdir -p /var/cache/APPLaunch
sudo ln -sfn /var/cache/APPLaunch /usr/share/APPLaunch/cache
sudo systemctl daemon-reload
sudo systemctl restart APPLaunch.service
```

If the service file has not been installed, create `/lib/systemd/system/APPLaunch.service` manually, using the content in Section 8 as reference.

## 12. Deployment Verification Commands

### 12.1 Package Status

```bash
dpkg -l | grep applaunch
dpkg -s applaunch
```

List files installed by the package:

```bash
dpkg -L applaunch
```

Inspect `.deb` package contents without installing:

```bash
dpkg-deb -c applaunch_0.2.1-m5stack1_arm64.deb
```

Inspect `.deb` metadata:

```bash
dpkg-deb -I applaunch_0.2.1-m5stack1_arm64.deb
```

### 12.2 Files and Permissions

```bash
ls -l /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
file /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
ls -ld /usr/share/APPLaunch
ls -l /usr/share/APPLaunch/cache
ls -l /var/cache/APPLaunch
find /usr/share/APPLaunch/share/images -maxdepth 1 -type f | head
find /usr/share/APPLaunch/share/font -maxdepth 1 -type f | head
```

Expected:

- The main program has execute permission.
- The main program architecture is `ARM aarch64`.
- `/usr/share/APPLaunch/cache` points to `/var/cache/APPLaunch`.
- Image and font resources exist.

### 12.3 Dynamic Library Dependencies

On the device:

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

Check for missing dependencies:

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
```

If libraries are missing, install the corresponding system packages, or extend the packaging rules to place private libraries under `/usr/share/APPLaunch/lib` and configure the runtime search path.

### 12.4 systemd Status

```bash
systemctl status APPLaunch.service --no-pager
systemctl is-enabled APPLaunch.service
systemctl is-active APPLaunch.service
```

View logs:

```bash
journalctl -u APPLaunch.service -b --no-pager
journalctl -u APPLaunch.service -b -f
```

Restart:

```bash
sudo systemctl restart APPLaunch.service
```

Stop:

```bash
sudo systemctl stop APPLaunch.service
```

Enable boot autostart:

```bash
sudo systemctl enable APPLaunch.service
```

Disable boot autostart:

```bash
sudo systemctl disable APPLaunch.service
```

Reload service files:

```bash
sudo systemctl daemon-reload
```

### 12.5 Manual Foreground Run

Before troubleshooting systemd, run it in the foreground first:

```bash
sudo systemctl stop APPLaunch.service || true
cd /usr/share/APPLaunch
sudo ./bin/M5CardputerZero-APPLaunch
```

This lets you see standard output and crash messages directly. If foreground execution works but systemd does not, check the service file, permissions, and working directory.

### 12.6 framebuffer and Input Devices

Check framebuffer:

```bash
ls -l /dev/fb*
cat /sys/class/graphics/fb0/name 2>/dev/null || true
```

Check input devices:

```bash
ls -l /dev/input/
cat /proc/bus/input/devices
```

Check who currently holds framebuffer or input devices:

```bash
sudo fuser -v /dev/fb0 2>/dev/null || true
sudo fuser -v /dev/input/event* 2>/dev/null || true
```

If another graphics program is running, APPLaunch may not display correctly or read input correctly.

## 13. Uninstall and Rollback

### 13.1 Uninstall

```bash
sudo dpkg -r applaunch
```

This triggers `prerm`: it stops the service, disables it, and deletes `/var/cache/APPLaunch`.

To also clean configuration files:

```bash
sudo dpkg -P applaunch
```

### 13.2 Roll Back by Installing an Older Package

```bash
sudo systemctl stop APPLaunch.service || true
sudo dpkg -i /home/pi/applaunch_old-version-m5stack1_arm64.deb
sudo systemctl restart APPLaunch.service
```

Verify:

```bash
dpkg -s applaunch | grep Version
systemctl status APPLaunch.service --no-pager
```

### 13.3 Temporarily Disable the Launcher

```bash
sudo systemctl disable --now APPLaunch.service
```

Restore:

```bash
sudo systemctl enable --now APPLaunch.service
```

## 14. Common Deployment Errors

### 14.1 Install Error: `package architecture (arm64) does not match system`

Cause: the device system is not arm64, or an arm64 package was installed directly on an x86_64 development machine.

Fix:

```bash
uname -m
dpkg --print-architecture
```

The `.deb` should be installed on the M5CardputerZero device, not on a Linux x86_64 development machine.

### 14.2 Runtime Error: `Exec format error`

Cause: wrong main-program architecture. A common case is packaging a Linux SDL2 x86_64 artifact into an arm64 package.

Check:

```bash
file /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

Correct fix: cross-compile again:

```bash
cd projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
```

Then repackage and reinstall.

### 14.3 Service Keeps Restarting

Check:

```bash
systemctl status APPLaunch.service --no-pager
journalctl -u APPLaunch.service -b --no-pager | tail -n 100
```

Common causes:

- Missing dynamic libraries.
- Resource path does not exist.
- framebuffer or input devices are unavailable.
- The program crashes immediately at startup.
- The installed artifact has the wrong architecture.

Further checks:

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
ls /usr/share/APPLaunch/share/images
ls /dev/fb0
```

### 14.4 `ln: failed to create symbolic link '/usr/share/APPLaunch/cache': File exists`

Cause: during repeated installation, `postinst` uses `ln -s` and the target already exists.

Fix:

```bash
sudo rm -rf /usr/share/APPLaunch/cache
sudo mkdir -p /var/cache/APPLaunch
sudo ln -s /var/cache/APPLaunch /usr/share/APPLaunch/cache
sudo systemctl restart APPLaunch.service
```

To fix this at the root, change the packaging script to use `ln -sfn`, but that is a code change.

### 14.5 `dpkg-deb: error: failed to open package info file .../DEBIAN/control`

Cause: the packaging directory structure is incomplete, or the script failed midway and left an abnormal directory behind.

Fix:

```bash
cd projects/APPLaunch/tools
python3 llm_pack.py clean
python3 llm_pack.py
```

### 14.6 `FileNotFoundError: Binary M5CardputerZero-APPLaunch not found in ../dist`

Cause: the project has not been built, the build directory is not `projects/APPLaunch/dist`, or the packaging script was not run from the `tools` directory.

Fix:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
ls -l dist/M5CardputerZero-APPLaunch
cd tools
python3 llm_pack.py
```

### 14.7 Black Screen After Service Starts

Investigation order:

1. Confirm that the executable can run in the foreground.
2. Confirm that framebuffer exists.
3. Confirm no other process is occupying the display.
4. Confirm that resource paths exist.
5. Check journal logs.

Commands:

```bash
sudo systemctl stop APPLaunch.service || true
cd /usr/share/APPLaunch
sudo ./bin/M5CardputerZero-APPLaunch
ls -l /dev/fb0
sudo fuser -v /dev/fb0 2>/dev/null || true
journalctl -u APPLaunch.service -b --no-pager | tail -n 100
```

### 14.8 External Apps Cannot Start

APPLaunch finds external apps from the resource tree and `.desktop` descriptors. First check:

```bash
find /usr/share/APPLaunch/applications -maxdepth 1 -type f -print
find /usr/share/APPLaunch/bin -maxdepth 1 -type f -print
```

Confirm that external apps have execute permission:

```bash
ls -l /usr/share/APPLaunch/bin
```

If `Exec` in a `.desktop` file points to a missing path, fix the resource tree or repackage.

## 15. Pre-Release Checklist

Before packaging:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
file dist/M5CardputerZero-APPLaunch
```

After packaging:

```bash
cd tools
python3 llm_pack.py
dpkg-deb -I applaunch_0.2.1-m5stack1_arm64.deb
dpkg-deb -c applaunch_0.2.1-m5stack1_arm64.deb | head -n 50
```

After installation:

```bash
dpkg -s applaunch | grep -E 'Package|Version|Architecture'
systemctl status APPLaunch.service --no-pager
systemctl is-enabled APPLaunch.service
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
ls -l /usr/share/APPLaunch/cache
journalctl -u APPLaunch.service -b --no-pager | tail -n 100
```

Functional verification:

- APPLaunch automatically shows the home screen after the device boots.
- Keyboard/button input works.
- The home-screen application carousel can switch items.
- Resource images and fonts display correctly.
- Built-in pages can be entered and exited.
- External apps can exit and return to APPLaunch after launch.
- Optional sub-applications such as AppStore/Calculator, if packaged, can launch normally from the launcher.

## 16. Recommended Deployment Flow

For formal releases, use:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
file dist/M5CardputerZero-APPLaunch
cd tools
python3 llm_pack.py
scp applaunch_0.2.1-m5stack1_arm64.deb pi@192.168.28.177:/home/pi/
ssh pi@192.168.28.177 'sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb && systemctl status APPLaunch.service --no-pager'
```

For fast replacement during development, use:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
scons push
```

Difference: `.deb` verifies the complete installation and systemd lifecycle; `scons push` is faster but cannot replace formal packaging verification.
