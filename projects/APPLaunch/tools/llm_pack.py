#!/bin/env python3
import os
import sys
import shutil
import subprocess
import glob
from datetime import datetime

'''
{package_name}_{version}-{revision}_{architecture}.deb
applaunch_0.1-m5stack1_arm64.deb

debian-APPLaunch directory structure:
  DEBIAN/
    control
    postinst
    prerm
  lib/
    systemd/
      system/
        APPLaunch.service
  usr/
    share/
      APPLaunch/
        applications/
          vim.desktop.temple
        bin/
          M5CardputerZero-APPLaunch
        lib/
          lvgl.so
        share/
          font/
            *.ttf
          images/
            *.png
'''

PACKAGE_NAME   = 'applaunch'
APP_NAME       = 'APPLaunch'
BIN_NAME       = 'M5CardputerZero-APPLaunch'
INSTALL_PPREFIX = 'usr/share'
INSTALL_PREFIX = f'{INSTALL_PPREFIX}/APPLaunch'
BIN_PATH       = f'{INSTALL_PREFIX}/bin'
LIB_PATH       = f'{INSTALL_PREFIX}/lib'
SHARE_PATH     = f'{INSTALL_PREFIX}/share'
APP_PATH       = f'{INSTALL_PREFIX}/applications'
SERVICE_PATH   = 'lib/systemd/system'


def create_applaunch_deb(version='0.1', src_folder='../dist', revision='m5stack1'):
    """
    Build the APPLaunch Debian package from src_folder.

    Expected files inside src_folder:
      bin/M5CardputerZero-APPLaunch   (or directly M5CardputerZero-APPLaunch)
      lib/lvgl.so
      share/font/*.ttf
      share/images/*.png
      applications/vim.desktop.temple  (optional)
    """
    tools_dir  = os.path.dirname(os.path.abspath(__file__))
    deb_folder = os.path.join(tools_dir, f'debian-{APP_NAME}')
    deb_file   = os.path.join(tools_dir, f'{PACKAGE_NAME}_{version}-{revision}_arm64.deb')

    # ------------------------------------------------------------------ cleanup
    if os.path.exists(deb_folder):
        shutil.rmtree(deb_folder)

    print(f'Creating Debian package {PACKAGE_NAME} {version} ...')

    # --------------------------------------------------------- create directories
    for d in [
        os.path.join(deb_folder, 'DEBIAN'),
        os.path.join(deb_folder, BIN_PATH),
        os.path.join(deb_folder, LIB_PATH),
        os.path.join(deb_folder, SHARE_PATH, 'font'),
        os.path.join(deb_folder, SHARE_PATH, 'images'),
        os.path.join(deb_folder, APP_PATH),
        os.path.join(deb_folder, SERVICE_PATH),
    ]:
        os.makedirs(d, exist_ok=True)

    # ------------------------------------------------------- copy binary
    # Search in src_folder directly, or inside src_folder/bin/
    bin_src = os.path.join(src_folder, BIN_NAME)
    if not os.path.exists(bin_src):
        bin_src = os.path.join(src_folder, 'bin', BIN_NAME)
    if not os.path.exists(bin_src):
        raise FileNotFoundError(f'Binary {BIN_NAME} not found in {src_folder}')
    shutil.copy2(bin_src, os.path.join(deb_folder, BIN_PATH, BIN_NAME))

    # ------------------------------------------------------- copy bundled app binaries + backends
    for extra in ['M5CardputerZero-AppStore', 'appstore.py', 'M5CardputerZero-Calculator']:
        extra_src = os.path.join(src_folder, 'bin', extra)
        if os.path.exists(extra_src):
            dest = os.path.join(deb_folder, BIN_PATH, extra)
            shutil.copy2(extra_src, dest)
            if not extra.endswith('.py'):
                os.chmod(dest, 0o755)
            print(f'  Included: {extra}')

    # ------------------------------------------------------- APPLaunch/
    source_app_tree = os.path.abspath(os.path.join(tools_dir, '..', 'APPLaunch'))
    app_src = source_app_tree if os.path.exists(source_app_tree) else os.path.join(src_folder, "APPLaunch")
    app_dst = os.path.join(deb_folder, INSTALL_PREFIX)
    print(app_src, app_dst)
    shutil.copytree(app_src, app_dst, dirs_exist_ok=True)

    appstore_images = os.path.abspath(os.path.join(tools_dir, '..', '..', 'AppStore', 'share', 'images'))
    if os.path.isdir(appstore_images):
        images_dst = os.path.join(app_dst, 'share', 'images')
        os.makedirs(images_dst, exist_ok=True)
        for pattern in ('store_wordmark.png', 'store_arrow_*.png'):
            for image_src in glob.glob(os.path.join(appstore_images, pattern)):
                shutil.copy2(image_src, os.path.join(images_dst, os.path.basename(image_src)))


    # ------------------------------------------------------- DEBIAN/control
    with open(os.path.join(deb_folder, 'DEBIAN', 'control'), 'w') as f:
        f.write(f'Package: {PACKAGE_NAME}\n')
        f.write(f'Version: {version}\n')
        f.write(f'Architecture: arm64\n')
        f.write(f'Maintainer: dianjixz <dianjixz@m5stack.com>\n')
        f.write(f'Original-Maintainer: m5stack <m5stack@m5stack.com>\n')
        f.write(f'Section: {APP_NAME}\n')
        f.write(f'Priority: optional\n')
        f.write(f'Homepage: https://www.m5stack.com\n')
        f.write(f'Packaged-Date: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}\n')
        f.write(f'Description: M5CardputerZero {APP_NAME}\n')

    # ------------------------------------------------------- DEBIAN/postinst
    with open(os.path.join(deb_folder, 'DEBIAN', 'postinst'), 'w') as f:
        f.write('#!/bin/sh\n')
        f.write(f'mkdir -p /var/cache/APPLaunch\n')
        f.write(f'ln -s /var/cache/APPLaunch /usr/share/APPLaunch/cache\n')
        f.write(f'[ -f "/lib/systemd/system/{APP_NAME}.service" ] && systemctl enable {APP_NAME}.service\n')
        f.write(f'[ -f "/lib/systemd/system/{APP_NAME}.service" ] && systemctl start {APP_NAME}.service\n')
        f.write('exit 0\n')

    # ------------------------------------------------------- DEBIAN/prerm
    with open(os.path.join(deb_folder, 'DEBIAN', 'prerm'), 'w') as f:
        f.write('#!/bin/sh\n')
        f.write(f'[ -f "/lib/systemd/system/{APP_NAME}.service" ] && systemctl stop {APP_NAME}.service\n')
        f.write(f'[ -f "/lib/systemd/system/{APP_NAME}.service" ] && systemctl disable {APP_NAME}.service\n')
        f.write(f'rm -rf /var/cache/APPLaunch\n')
        f.write('exit 0\n')

    # ------------------------------------------------------- lib/systemd/system/APPLaunch.service
    service_file = os.path.join(deb_folder, SERVICE_PATH, f'{APP_NAME}.service')
    with open(service_file, 'w') as f:
        f.write('[Unit]\n')
        f.write(f'Description={APP_NAME} Service\n')
        f.write('\n')
        f.write('[Service]\n')
        f.write(f'ExecStart=/{BIN_PATH}/{BIN_NAME}\n')
        f.write(f'WorkingDirectory=/{INSTALL_PREFIX}\n')
        f.write('Restart=always\n')
        f.write('RestartSec=1\n')
        f.write('StartLimitInterval=0\n')
        f.write('\n')
        f.write('[Install]\n')
        f.write('WantedBy=multi-user.target\n')
        f.write('\n')

    # ------------------------------------------------------- fix permissions
    os.chmod(os.path.join(deb_folder, 'DEBIAN', 'postinst'), 0o755)
    os.chmod(os.path.join(deb_folder, 'DEBIAN', 'prerm'),    0o755)
    os.chmod(os.path.join(deb_folder, BIN_PATH, BIN_NAME),   0o755)

    # ------------------------------------------------------- build .deb
    subprocess.run(['dpkg-deb', '-b', deb_folder, deb_file], check=True)
    print(f'Debian package created: {deb_file}')

    # shutil.rmtree(deb_folder)
    return f'{PACKAGE_NAME} create success!'


if __name__ == '__main__':

    if 'clean' in sys.argv:
        os.system('rm -f ./*.deb')
        os.system('find . -maxdepth 1 -type d ! -name "." -exec rm -rf {} +')
        sys.exit(0)

    if 'distclean' in sys.argv:
        os.system('rm -rf ./*.deb m5stack_*')
        sys.exit(0)

    version    = '0.2.1'
    src_folder = '../dist'
    revision   = 'm5stack1'

    result = create_applaunch_deb(version, src_folder, revision)
    print(result)
