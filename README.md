# ⚡ Windows Laptop Optimizer // Cybercore Techwear Edition

> A high-performance, standalone native Windows GUI application (`.exe`) engineered to eliminate background telemetry, unlock full CPU clock responsiveness, purge bloated WebViews, and reclaim gigabytes of NVMe storage — wrapped in a bespoke, brand-free **Cybercore // Dark Techwear** interface with zero external runtime dependencies.

![Windows 11](https://img.shields.io/badge/OS-Windows%2010%20%7C%2011-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![C++](https://img.shields.io/badge/Language-C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Theme](https://img.shields.io/badge/Theme-Cybercore%20Techwear-CCFF00?style=for-the-badge&labelColor=0B0E14)
![Architecture](https://img.shields.io/badge/Arch-x64%20Native%20(Win32%2FGDI%2B)-brightgreen?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)

---

## 🖥️ What Makes It "Out of the Box"?

Unlike traditional batch or PowerShell scripts that require command terminal copy-pasting, **Windows Laptop Optimizer** is a **true standalone native desktop executable (`.exe`)**:

* **Double-Click & Run**: Statically compiled binary. Requires **no** external dependencies (.NET, Python, Node, WebView2, or Electron are **NOT** required).
* **Cybercore // Dark Techwear Theme**:
  * Matte Obsidian Canvas (`#0B0E14`) paired with slate containers (`#10141E`) and subtle techwear borders (`#1E2330`).
  * High-Voltage Acid-Volt (`#CCFF00`) and emerald highlights with interactive focus glow.
  * Real-time hardware telemetry module with dynamic frequency equalizer spectrum visualizer.
  * Monospace tactical stencils (`// SYS.CORE-TUNER`, `// PROTOCOL: ZERO_LATENCY`, `[STATUS: ARMED]`).
* **Bespoke In-Window Execution Modal**:
  * No standard Windows message popups!
  * Displays a custom overclock engine dispatch modal with animated acid-to-emerald gradient progress bar, real-time worker console log stream, and a `[ ✓ DISMISS CONSOLE ]` action.
* **100% Reversible with 1-Click Rollback**:
  * Includes a dedicated **`↺ RESTORE FACTORY DEFAULTS`** button that safely restores all original Windows settings (Balanced power scheme, Game DVR hooks, 400ms menu delay, and re-enables fast startup).
* **Embedded Multi-Resolution Icon**:
  * High-resolution application icon (256x256, 48x48, 32x32, 16x16) compiled directly into the binary via `windres`.

---

## 🚀 Key Performance Modules

| # | Module | What It Does | Technical Mechanism & Impact |
| :-: | :--- | :--- | :--- |
| **01** | ⚡ **Zero-Ramp CPU Frequency Floor on AC** | Sets minimum processor clock floor to 100% on AC power. | Unlocks instantaneous CPU response for compiling, gaming, and app switching without frequency step ramp lag. |
| **02** | 🎮 **Bypass Game DVR Background Video Buffer** | Halts continuous background DirectX video encoding and capture hooks. | Releases GPU video encode shaders and stops background VRAM allocation; eliminates micro-stuttering. |
| **03** | 🏎️ **Instant Desktop Context Menus (20ms)** | Drops Windows artificial `MenuShowDelay` from 400ms down to 20ms. | Right-click menus, dropdowns, and taskbar flyouts snap open instantaneously. |
| **04** | 🗑️ **Purge Windows 11 Widget Daemon Pack** | Uninstalls the `WebExperience` package and kills idle `WidgetService.exe`. | Eliminates 20+ idle background Microsoft Edge WebView2 processes, freeing **~300+ MB RAM**. |
| **05** | 🔍 **Decouple Bing Cloud Integration from Start** | Disables Bing cloud search queries in Windows Start. | Makes app and file search 100% local, instantaneous, and stops typing lag over the network. |
| **06** | 🌐 **Halt Delivery Optimization P2P Seeding** | Configures Delivery Optimization to HTTP-only mode (`DODownloadMode = 0`). | Stops Windows from using your disk I/O, CPU, and WiFi bandwidth to upload update files to other PCs. |
| **07** | 💾 **Clean Kernel Boots & Purge `hiberfil.sys`** | Disables hybrid hibernation via `powercfg -h off` and permanently deletes `hiberfil.sys`. | **Reclaims 6+ GB of SSD storage** immediately and guarantees a true, clean kernel boot on every shutdown. |
| **08** | 🧹 **Purge Stale Temporary Files (>24h)** | Cleans orphaned cache and temp files older than 24 hours from `%TEMP%`. | Recovers drive capacity and reduces disk indexing overhead. |

---

## 📥 How to Run

1. Download `LaptopOptimizer.exe` from the repository root or Releases.
2. Right-click `LaptopOptimizer.exe` and select **Run as administrator** *(required to configure power schemes and delete kernel hibernation storage)*.
3. Toggle the cyber switches on or off to choose your preferred tweaks, or click `TOGGLE ALL`.
4. Click **`⚡ DEPLOY TACTICAL TUNING ➔`** to engage the overclock engine!
5. Inspect the live console logs in the execution modal and click `[ ✓ DISMISS CONSOLE ]` when finished.
6. To undo changes at any time, click **`↺ RESTORE FACTORY DEFAULTS`**.

---

## 🛠️ Building From Source

This project compiles natively with **MinGW-w64 (GCC)** and the GNU resource compiler (`windres`) with zero third-party dependencies:

```bash
# 1. Compile Windows resource icon
windres -i src/resource.rc -o src/resource.res -O coff

# 2. Build standalone GUI executable
g++ -O2 -mwindows -static -Isrc src/main.cpp src/resource.res -o LaptopOptimizer.exe -lgdiplus -lgdi32 -luser32 -ldwmapi -lcomctl32 -lole32 -lshlwapi
```

Or simply run `build.bat`.

---

## 🔒 Safety & Reversibility

* **Zero telemetry**: The app contains zero analytics, trackers, or network calls.
* **Safe defaults**: All modifications target standard Windows configuration keys.
* **Non-destructive**: Does not break Windows Update or core system integrity.
* **100% Reversible**: Every single modification can be restored in 1 click.

---

## 📄 License

Distributed under the **MIT License**. See `LICENSE` for details.
