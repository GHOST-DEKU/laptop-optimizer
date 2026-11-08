# Laptop Optimizer

A Windows desktop app that applies a fixed set of eight performance and cleanup tweaks (power plan, Game DVR, menu delay, Widgets, hibernation file, Bing search in Start, Delivery Optimization, temp files) and lets you switch startup programs on or off. It is built with Tauri v2 and a Rust backend, with a dark "Cybercore" interface.

**Live preview:** <https://ghost-deku.github.io/laptop-optimizer/> shows the real app UI running in demo mode. It cannot change anything on your computer.

> [!WARNING]
> The app changes Windows settings, uninstalls a Windows component, and deletes files. It does **not** create a restore point or save your previous values, and not every change can be undone. Read [What the tweaks do](#what-the-tweaks-do) before you deploy.

## Contents

- [Screenshots](#screenshots)
- [Features](#features)
- [What the tweaks do](#what-the-tweaks-do)
- [Installation](#installation)
- [Usage](#usage)
- [Supported platforms](#supported-platforms)
- [Development](#development)
- [Project structure](#project-structure)
- [Known limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgements](#acknowledgements)

## Screenshots

These images are the app's own interface (`laptop-optimizer-tauri/src/index.html`) rendered in a browser in demo mode, so the hardware details shown (Lenovo 82HS, i7-1165G7) are sample data, not read from the machine that took the screenshot.

| Main window | After deploying |
| :-- | :-- |
| ![Main window with eight tweak cards](docs/screenshots/main-window.png) | ![Console drawer showing deployment log](docs/screenshots/dispatch-console.png) |

![Startup applications dialog](docs/screenshots/startup-apps.png)

## Features

- **Eight tweaks** in one window, each with its own switch. All eight start switched on.
- **Deploy** applies every switched-on tweak and prints a result line for each one in a console drawer. A safety cover over the Deploy button must be opened first.
- **Restore Factory Defaults** puts the settings back to Windows defaults (see the limits in [What the tweaks do](#what-the-tweaks-do)).
- **Startup applications manager** lists programs in the `Run` registry keys and turns them on or off without deleting the keys.
- **Hardware summary** in the header: processor name, thread count, RAM and system drive size, and system model, read from the registry and Win32 calls.
- **Sound effects** generated in the browser engine (Web Audio API), with a mute toggle in the header. No audio files are used.
- **Runs as administrator.** On launch the app re-starts itself with a UAC prompt if it is not already elevated.

## What the tweaks do

This table is taken from the Rust backend in `laptop-optimizer-tauri/src-tauri/src/optimizer.rs`. Numbers match the card numbers in the app.

| # | Tweak | What the code does | What Restore does |
| :-: | :-- | :-- | :-- |
| 01 | CPU frequency floor | Activates the Windows High Performance power scheme (falls back to a second scheme GUID if needed) and turns off power throttling (`powercfg /powerthrottling disable` and `PowerThrottlingOff=1` under `HKLM`). It does not set a processor-state value itself. | Turns throttling back on and activates the Balanced scheme. A duplicated scheme is not deleted. |
| 02 | Game DVR | Sets `GameDVR_Enabled`, `AppCaptureEnabled` and `HistoricalCaptureEnabled` to 0 and writes two full-screen behaviour values under `HKCU`. | Sets the three enable values back to 1. The two behaviour values stay as written. |
| 03 | Menu delay | Sets `MenuShowDelay` to `20` in `HKCU\Control Panel\Desktop`. | Sets it to `400`, even if you had a different value before. |
| 04 | Widgets | Hides the taskbar Widgets button (`TaskbarDa=0`), ends `WidgetService.exe`, and removes the Windows Web Experience package with `winget`, or with PowerShell `Remove-AppxPackage` if `winget` fails. | Shows the Widgets button again. **The removed package is not reinstalled.** |
| 05 | Hibernation file | Runs `powercfg -h off`, which also turns off Fast Startup and deletes `hiberfil.sys`. | Runs `powercfg -h on`. |
| 06 | Bing in Start | Sets `BingSearchEnabled=0` under `HKCU` and ends `SearchHost.exe`. | Sets it to 1. |
| 07 | Delivery Optimization | Sets `DODownloadMode=0` under `HKCU\Software\Microsoft\Windows\CurrentVersion\DeliveryOptimization`. | Sets it to 1. |
| 08 | Temp files | Deletes **every** file and folder it can inside your `%TEMP%` folder. Items in use are skipped. The card says "older than 24h", but the code does not check age. | Nothing. Deleted files cannot be restored. |

The labels in the app such as "−380ms", "−300MB" and "+7.5 GB FREED" are fixed text. The app does not measure them, and it does not check whether Windows honours each setting on your version.

## Installation

**Option 1: portable executable.** Download [`LaptopOptimizer.exe`](https://github.com/GHOST-DEKU/laptop-optimizer/raw/main/LaptopOptimizer.exe) from the repository root. It is a Tauri build (about 9 MB) committed to the repository. No checksum or code signature is published, so Windows SmartScreen may warn about it. If you prefer not to trust a committed binary, build it yourself (see [Development](#development)).

You can also run `Run-Optimizer.bat`, which unblocks the executable and starts it with administrator rights. It must sit in the same folder as `LaptopOptimizer.exe`.

**Option 2: installers.** The workflow in `.github/workflows/release.yml` builds an MSI and an NSIS installer and attaches them to a GitHub release when a tag starting with `v` is pushed. No such tag exists yet, so there are no installer downloads at the moment.

The app needs Microsoft Edge WebView2. It is included in Windows 10 (version 1803 and later) and Windows 11.

## Usage

1. Start the app and accept the UAC prompt.
2. Click a card to switch a tweak on or off, or use **TOGGLE ALL**. Remember that every tweak starts switched on.
3. Click the cover over the Deploy button (**SILO: SEALED**). It changes to **WARHEAD: ARMED**.
4. Click **LAUNCH NUCLEAR DISPATCH**. The console drawer opens and logs each step. Failed steps are marked `[✗]` with the error.
5. To open or close the console by hand, click its header bar.
6. **STARTUP APPS** opens the startup manager. Click a row to switch that program on or off for the next sign-in.
7. **RESTORE FACTORY DEFAULTS** asks for confirmation, then applies the Restore column above.

## Supported platforms

| Platform | Status |
| :-- | :-- |
| Windows 10 / 11, 64-bit | Target platform. The release workflow builds `x86_64-pc-windows-msvc`. There are no automated tests, so no test matrix exists. |
| Other Windows builds (32-bit, ARM) | Not built by the workflow. Not verified. |
| Linux, macOS | Not supported. The Rust backend uses Windows-only APIs (`winreg`, `std::os::windows`) and will not compile there. |

## Development

### Prerequisites (Windows)

From the [Tauri v2 prerequisites](https://v2.tauri.app/start/prerequisites):

- Microsoft C++ Build Tools with the "Desktop development with C++" workload
- Microsoft Edge WebView2 (already present on Windows 10 1803+ and Windows 11)
- Rust, installed with the MSVC toolchain (`rustup default stable-msvc`)
- Node.js (the release workflow uses version 20)
- pnpm 12.18.0, the version pinned in `package.json` (`corepack enable` will provide it)
- VBSCRIPT enabled, only if you build the MSI installer

### Run and build

```bash
cd laptop-optimizer-tauri
pnpm install
pnpm tauri dev      # run the app in development
pnpm tauri build    # production build: MSI and NSIS installers
```

`pnpm dev` and `pnpm build` are shortcuts for the same two commands. Build output is written under `laptop-optimizer-tauri/src-tauri/target/release/`, with installers in the `bundle` folder. Use `tauri build` rather than a plain `cargo build --release`, because the Tauri CLI is what prepares a production build.

`pnpm install` was run against the committed lockfile and installs `@tauri-apps/cli` 2.12.4. The `tauri dev` and `tauri build` commands could not be run on the Linux machine used to write this document, so they are taken from `package.json`, `tauri.conf.json` and the release workflow rather than tested here.

### Try the interface without Tauri

Open `laptop-optimizer-tauri/src/index.html` in any browser. Outside Tauri the page uses built-in sample data and makes no system changes. The website preview embeds this same file, so a change to it shows up on the site as well.

### Older C++ version

`src/` holds an earlier native Win32/GDI+ version of the app (`main.cpp`, `optimizer.h`). It is not the build that the release workflow produces, and its button labels and layout differ from the Tauri app. `build.bat` compiles it with MinGW-w64 into `LaptopOptimizer.exe`, which would replace the Tauri executable in the repository root. `src/resource.rc` contains absolute paths from the author's machine, so the `windres` step in `build.bat` only works if those paths exist. The C++ build was not compiled while preparing this document.

## Project structure

```text
.
├── laptop-optimizer-tauri/       Current application
│   ├── package.json              pnpm scripts (dev, build) and the Tauri CLI
│   ├── src/index.html            Whole interface: HTML, CSS and JavaScript in one file
│   └── src-tauri/
│       ├── src/main.rs           Entry point, relaunches as administrator
│       ├── src/lib.rs            Tauri commands exposed to the interface
│       ├── src/optimizer.rs      Registry, powercfg and file operations
│       ├── tauri.conf.json       Window size, bundle targets, icons
│       └── capabilities/         Tauri permission set
├── index.html                    Project website (GitHub Pages), embeds the app UI
├── src/                          Older C++ implementation
├── LaptopOptimizer.exe           Committed portable build (Tauri)
├── Run-Optimizer.bat             Unblock-and-run helper
├── build.bat                     Builds the C++ version
├── docs/screenshots/             Images used in this README
└── .github/workflows/release.yml Release build on `v*` tags
```

The interface talks to Rust through seven commands: `get_system_info`, `get_live_telemetry`, `apply_tweak`, `restore_tweak`, `restore_all`, `get_startup_apps` and `toggle_startup_app`.

## Known limitations

- **Restore returns Windows defaults, not your previous values.** For example, a custom menu delay becomes 400.
- **Some changes are permanent:** deleted temp files and the removed Web Experience package.
- **The startup manager only covers the `Run` keys** under `HKCU` and `HKLM`. It does not cover the Startup folder, `RunOnce`, 32-bit registry views or scheduled tasks.
- **Sample startup entries:** if no `Run` entries are found, the app shows four sample programs (OneDrive, Grammarly, UniGetUI, RiotClient). Switching one writes a real `StartupApproved` value under that name.
- **Live telemetry is not shown.** The backend can report CPU speed, RAM use and free disk space, but the current interface has no elements that display them.
- **The "NVMe" label is fixed text** in the header, and the drive size is rounded to a common size (256, 512 or 1024 GB).
- **Own file unblocking:** at startup the app deletes the `Zone.Identifier` stream of its own executable, which removes the "downloaded from the internet" mark.
- **Network use:** the app's own code contains no analytics or network calls. The `winget` command in tweak 04 may contact Microsoft's package source, and the installers can download WebView2 if it is missing.
- **Version numbers differ:** the interface shows V2.4, `tauri.conf.json` says 2.5.0, and `Cargo.toml` and `package.json` say 1.0.0.

## Troubleshooting

| Problem | What to try |
| :-- | :-- |
| SmartScreen warns about the file | The executable is unsigned. Choose "More info", then "Run anyway", or build from source. |
| Some deploy steps show `[✗]` | The app probably is not elevated, for example after declining the UAC prompt. Close it and start it again, accepting the prompt. |
| Window stays blank or the app will not start | Install the [WebView2 Evergreen runtime](https://developer.microsoft.com/microsoft-edge/webview2/). |
| Widgets button or panel is gone after Restore | Restore does not reinstall the package. Reinstall "Windows Web Experience Pack" from the Microsoft Store. |
| `pnpm install` complains about the package manager version | Run `corepack enable` and use pnpm 12.18.0. |
| MSI build fails | Check that VBSCRIPT is enabled (Settings, Apps, Optional features, More Windows features). |
| Build fails on a missing linker | Install the "Desktop development with C++" workload and use the MSVC Rust toolchain. |

## Contributing

Issues and pull requests are welcome at <https://github.com/GHOST-DEKU/laptop-optimizer>. The repository has no automated tests or contribution guide yet, so please:

- keep each pull request focused on one change,
- describe how you tested it, since the Tauri app only runs on Windows,
- check interface changes in a browser using `laptop-optimizer-tauri/src/index.html`, and
- avoid committing build output or secrets.

## License

Released under the [MIT License](LICENSE).

## Acknowledgements

Built with [Tauri](https://tauri.app), [serde](https://serde.rs), the [`winreg`](https://crates.io/crates/winreg) crate and `tauri-plugin-log`.
