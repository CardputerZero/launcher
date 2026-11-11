# Cardputer Zero Emulator on macOS

Requires CMake 3.20 or newer and SDL2 2.0.18 or newer. Install the build dependencies:

```sh
brew install cmake sdl2 sdl2_image freetype pkg-config nlohmann-json
python3 -m pip install --user --break-system-packages parse requests tqdm
```

From the repository root, initialize the pinned SDK and LVGL submodules:

```sh
git submodule update --init --depth=1 SDK projects/CardputerZero-Emulator/lib/lvgl
(cd projects/APPLaunch && bash ../../.github/scripts/prepare-sdk-source.sh eventpp include/eventpp/callbacklist.h)
```

Build and run the headless tests:

```sh
cmake -S projects/CardputerZero-Emulator -B build/emulator -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/emulator --parallel
ctest --test-dir build/emulator --output-on-failure --no-tests=error
```

On macOS and Linux, `emulator-runtime` queues a mouse click and a physical-key
SDL event through `SDL_PushEvent` and verifies both reach the app's
`LV_EVENT_KEYBOARD` path. Navigation and reset are smoke checks: the emulator
performs them and exits cleanly; the test does not assert the selected app or
rendered UI state. `emulator-saved-volume-exit` starts with a saved volume and
checks clean shutdown of the restore worker. Each test uses its own configuration
under the build directory instead of the developer's home configuration.

Run `build/emulator/cardputer-zero-emu` to open the compact device skin. The native
cp0 input runtime handles physical/on-screen keys and side buttons; the CLI uses
a Darwin PTY. Lock-screen binding is handled separately in PR #236.

The emulator is not a hardware simulator. OS information, battery, Wi-Fi, and
audio controls still use placeholder or Linux-oriented implementations; macOS
behavior for those services is limited. These changes have not been tested on a
physical CardputerZero.

For distribution, configure a separate Release build with `-DBUILD_TESTING=OFF`;
this excludes the input test driver. Package its `cardputer-zero-emu`, `apps/`,
`assets/`, `images/`, and staged `APPLaunch/share/` directories. The latter contains
fonts, images, and sounds. The executable still expects Homebrew SDL2,
SDL2_image, and FreeType libraries on the target Mac; the archive does not bundle
those system dependencies.
