# ⚡ Windows Laptop Optimizer

> A high-performance, standalone Windows GUI utility (`.exe`) designed to eliminate background telemetry, unlock full CPU responsiveness, purge bloated WebViews, and reclaim gigabytes of storage — with zero external dependencies and an interactive out-of-the-box dark UI.

![Windows 11](https://img.shields.io/badge/OS-Windows%2010%20%7C%2011-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![C++](https://img.shields.io/badge/Language-C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Architecture](https://img.shields.io/badge/Arch-x64%20Native-brightgreen?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)

---

## 🖥️ What Makes It "Out of the Box"?

Unlike ordinary scripts that require terminal pasting or command prompts, **Windows Laptop Optimizer** is a **true graphical desktop application (`.exe`)**:
* **Double-Click & Run**: No terminal console, no PowerShell execution policy restrictions, and no dependencies (.NET, Python, or Node are **NOT** required).
* **Interactive Control**: Toggle switches allow you to choose **exactly** which optimizations to apply and which to skip.
* **Modern Windows 11 Aesthetic**: Built with deep obsidian dark-mode theming, smooth anti-aliased GDI+ rendering, and Windows 11 rounded corners.
* **Multi-Threaded**: Non-blocking background worker with a live progress bar and status feedback.

---

## 🚀 Key Performance Modules

| Module | What It Does | Immediate Benefit |
| :--- | :--- | :--- |
| ⚡ **High Performance Power Profile** | Locks minimum CPU clock floor to 100% on AC power via `powercfg`. | Eliminates frequency step-up ramp lag; unlocks instant turbo boost for compiling, gaming, and app switching. |
| 🎮 **Disable Game DVR Background Hooks** | Disables continuous background DirectX video capture and full-screen latency hooks. | Stops background GPU encoding; frees VRAM and eliminates micro-stutters. |
| 🏎️ **Instant Context Menus (20ms)** | Drops Windows artificial `MenuShowDelay` from 400ms down to 20ms. | Right-click menus, dropdowns, and taskbar flyouts snap open instantly. |
| 🗑️ **Purge Windows 11 Widgets Board** | Uninstalls the `WebExperience` package and kills idle `WidgetService.exe`. | Eliminates 20+ idle background Microsoft Edge WebView2 processes, freeing **~300+ MB RAM**. |
| 🔍 **Decouple Bing from Start Menu** | Disables Bing cloud search integration in Windows Start. | Makes app and file search 100% local, instantaneous, and stops typing lag over the network. |
| 🌐 **Stop Delivery Optimization P2P** | Configures Delivery Optimization to HTTP-only mode (`DODownloadMode = 0`). | Stops Windows from using your disk I/O, CPU, and WiFi bandwidth to upload update files to other PCs. |
| 💾 **Purge Fast Startup (`hiberfil.sys`)** | Disables kernel hibernation via `powercfg -h off` and permanently deletes `hiberfil.sys`. | **Reclaims 6+ GB of SSD storage** immediately and guarantees a true, clean kernel boot on every shutdown. |
| 🧹 **Purge Stale Temporary Files** | Cleans orphaned cache and temp files older than 24 hours from `%TEMP%`. | Recovers drive capacity and reduces disk indexing overhead. |

---

## 📥 How to Run

1. Go to the [Releases](https://github.com/) page and download `LaptopOptimizer.exe`.
2. Right-click `LaptopOptimizer.exe` and select **Run as administrator** *(required to apply power schemes and delete hibernation files)*.
3. Toggle the switches on or off to choose your preferred tweaks.
4. Click **`⚡ OPTIMIZE LAPTOP`**!
5. Restart your laptop once complete to enjoy peak responsiveness.

---

## 🛠️ Building From Source

This project compiles natively with **MinGW-w64 (GCC)** with zero third-party dependencies:

```bash
# Clone the repository
git clone <your-repo-url>
cd laptop-optimizer

# Build standalone GUI executable
g++ -O2 -mwindows src/main.cpp -lgdiplus -lgdi32 -luser32 -ldwmapi -lcomctl32 -static -o LaptopOptimizer.exe
```

Or simply double-click `build.bat`.

---

## 🔒 Safety & Reversibility

* **Zero telemetry**: The app contains no analytics or network calls.
* **Safe defaults**: All modifications target standard user/system preference keys.
* **Non-destructive**: Does not break Windows Update or core system integrity.

---

## 📄 License

Distributed under the **MIT License**. See `LICENSE` for details.
