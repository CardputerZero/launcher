# 00 - Overview and Reading Path

`launcher` is the application project collection for M5CardputerZero. Its core project is `projects/APPLaunch`. APPLaunch is the main on-device launcher: after boot, it initializes LVGL, displays the home carousel, shows the status bar, launches built-in pages or external applications, and provides features such as settings, terminal, music, recording, camera, and LoRa.

## 1. Documentation Goals

This documentation set answers the following questions:

- What does each directory in this repository do?
- How does the APPLaunch process start, and where is the main loop?
- How is the home carousel UI organized and updated?
- How are built-in pages and external applications registered uniformly with the launcher?
- How are dynamic `.desktop` applications scanned and launched?
- How are resource, font, image, and audio paths resolved?
- How do SDL2 simulation, native on-device builds, and cross compilation work?
- How is the project packaged as a `.deb` and started automatically through systemd?
- How do you add a page, an external application, or a resource?
- How do you troubleshoot black screens, missing resources, or external applications that cannot return?

## 2. Project in One Sentence

APPLaunch can be understood as a small LVGL-based desktop environment:

```text
Linux device / SDL2 simulation
        |
        v
cp0_lvgl platform adaptation layer
        |
        v
LVGL 9.5 UI framework
        |
        v
APPLaunch home, status bar, carousel, application manager
        |
        +--> Built-in page AppPage
        +--> PTY ST terminal application UISTPage
        +--> External independent process cp0_process_exec_blocking()
```

## 3. Recommended Reading Order

If you are new to the project, read these documents in the following order:

1. `01-project-layout-and-module-responsibilities.md`: build an initial understanding of the directory structure.
2. `02-runtime-framework-and-boot-flow.md`: understand the path from `main()` to the home screen.
3. `03-ui-framework-and-home-carousel.md`: understand the home UI and carousel cards.
4. `04-application-model-and-launch-mechanism.md`: understand the application list and launch modes.
5. `08-build-and-compilation-guide.md`: build and run the project.
6. `10-extension-development-guide.md`: add a page or application.

If you only want to complete a specific task:

| Task | Read |
| --- | --- |
| Build and run the SDL2 version locally | `08-build-and-compilation-guide.md` |
| Cross-compile for the device | `08-build-and-compilation-guide.md` |
| Package a `.deb` | `09-packaging-deployment-and-systemd.md` |
| Modify the home card layout | `03-ui-framework-and-home-carousel.md` |
| Add a built-in page | `10-extension-development-guide.md` |
| Add a `.desktop` external application | `04-application-model-and-launch-mechanism.md`, `10-extension-development-guide.md` |
| Troubleshoot a black screen | `11-debugging-and-troubleshooting.md` |
| Find the entry file for a feature | `12-common-modification-entry-points.md` |

## 4. Key Project Paths

| Path | Description |
| --- | --- |
| `projects/APPLaunch` | Main launcher project |
| `projects/APPLaunch/main/src/main.cpp` | APPLaunch entry point and LVGL main loop |
| `projects/APPLaunch/main/ui/launch.cpp` | Application list, launch logic, status bar refresh |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | Home UI, carousel, home key handling |
| `projects/APPLaunch/main/ui/page_app` | Built-in page implementations |
| `projects/APPLaunch/APPLaunch` | Resource tree packaged into the runtime environment |
| `ext_components/cp0_lvgl` | Platform adaptation layer that wraps file, process, input, and system interfaces |
| `scripts/debian_packager.py` | Debian package build script |

## 5. Terminology

- **APPLaunch**: the launcher project or launcher process.
- **Home screen**: the main screen of APPLaunch, with the status bar and application carousel.
- **Built-in page**: a page class compiled into the APPLaunch process, such as `UISetupPage`.
- **Terminal application**: a command run inside APPLaunch through `UISTPage` + PTY, such as `bash`.
- **External application**: an independent executable program. When launched, APPLaunch pauses its own LVGL rendering and waits for the external program to exit.
- **Resource tree**: runtime files such as `APPLaunch/share/images`, `APPLaunch/share/audio`, and `APPLaunch/share/font`.
- **On-device**: the AArch64 Linux environment on M5CardputerZero.
- **SDL2 mode**: running in an SDL2 window on the development machine for simulation.
