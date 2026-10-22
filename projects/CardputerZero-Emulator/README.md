# Cardputer Zero Emulator on macOS

Install the native dependencies:

```sh
brew install cmake sdl2 sdl2_image freetype pkg-config nlohmann-json
python3 -m pip install --user --break-system-packages parse requests tqdm
```

From the repository root, prepare the SDK and LVGL sources:

```sh
git submodule update --init --depth=1 SDK
(cd projects/APPLaunch && bash ../../.github/scripts/prepare-sdk-source.sh eventpp include/eventpp/callbacklist.h)
git clone --depth 1 --branch v9.5.0 https://github.com/lvgl/lvgl.git projects/CardputerZero-Emulator/lib/lvgl
```

Skip the clone if the LVGL directory already exists. Build and run the headless
integration tests:

```sh
cmake -S projects/CardputerZero-Emulator -B build/emulator -DCMAKE_BUILD_TYPE=Release
cmake --build build/emulator --parallel
ctest --test-dir build/emulator --output-on-failure
```

The tests exercise clickable controls, navigation, reset, and the lock-screen
TAB-hold / wake / TAB / ENTER sequence. They check state and process exit status;
debug logging is not required. Input test instrumentation is excluded when
configured with `-DBUILD_TESTING=OFF`.

Run `build/emulator/cardputer-zero-emu` to open the device skin. Physical keys,
on-screen keys, and side buttons use the native cp0 input runtime. The CLI uses
a Darwin PTY. The display remains at the existing compact size.

For distribution, include `cardputer-zero-emu`, `apps/`, `assets/`, `images/`, and
`APPLaunch/share/`. The shared libraries are in `apps/`; `APPLaunch/share/`
contains fonts, images, and sounds. CI copies the latter directly from
`projects/APPLaunch/APPLaunch/share` and smoke-tests the packaged directory.
