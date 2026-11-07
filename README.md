# ⚡ Windows Laptop Optimizer // Cybercore Techwear Edition

> A high-performance, standalone native Windows desktop application engineered to eliminate background telemetry, unlock full CPU clock responsiveness, purge bloated WebViews, and reclaim gigabytes of NVMe storage — wrapped in a bespoke, brand-free **Cybercore // Dark Techwear** interface with 100% pixel-perfect fidelity.

![Windows 11](https://img.shields.io/badge/OS-Windows%2010%20%7C%2011-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Rust](https://img.shields.io/badge/Backend-Rust%201.95-DEA584?style=for-the-badge&logo=rust&logoColor=white)
![Tauri](https://img.shields.io/badge/GUI-Tauri%20v2%20(WebView2)-FFC131?style=for-the-badge&logo=tauri&logoColor=white)
![Theme](https://img.shields.io/badge/Theme-Cybercore%20Techwear-CCFF00?style=for-the-badge&labelColor=0B0E14)
![Release](https://img.shields.io/badge/Release-v2.4%20Production-00E5FF?style=for-the-badge)

---

## 🖥️ What Makes It "Out of the Box"?

Unlike traditional batch or PowerShell scripts that require terminal copy-pasting, **Windows Laptop Optimizer** is a **true standalone native desktop executable (`.exe`)**:

* **Double-Click & Run**: Self-contained executable powered by Tauri v2 and Windows native WebView2 (pre-installed on Windows 10/11). Zero extra runtime installations needed.
* **100% Pixel-Perfect Concept 2 Fidelity**:
  * Matte Obsidian Canvas (`#0B0E14`) paired with slate containers (`#10141E`) and subtle techwear borders (`#1E2330`).
  * High-Voltage Acid-Volt (`#CCFF00`) and emerald highlights with interactive focus glow.
  * Real-time hardware telemetry module with dynamic 6-bar frequency equalizer spectrum visualizer (`@keyframes spec1..6`).
  * Monospace tactical stencils (`// SYS.CORE-TUNER`, `// PROTOCOL: ZERO_LATENCY`, `[STATUS: ARMED]`).
  * Smooth micro-interactions: card hover drift (`translateX(6px)`), switch thumb animations, and glow scaling.
* **Web Audio API Mechanical Sound Engine**:
  * Procedurally synthesized haptic mechanical switch clicks, relay snaps, and success chords (zero audio asset dependencies).
* **Startup Applications Decoupler**:
  * Integrated modal to audit and toggle background startup programs safely using the Windows `StartupApproved\Run` registry without deleting user keys.
* **Bespoke In-Window Execution Modal**:
  * Displays a custom overclock engine dispatch modal with animated acid-to-emerald gradient progress bar, real-time worker console log stream, and a `[ ✓ DISMISS CONSOLE ]` action.
* **100% Reversible with 1-Click Rollback**:
  * Dedicated **`↺ RESTORE FACTORY DEFAULTS`** button that safely restores all original Windows settings (Balanced power scheme, Game DVR hooks, 400ms menu delay, and re-enables fast startup).
* **Self-Elevation & Security**:
  * Embedded Windows application manifest requesting `requireAdministrator` privileges.
  * Automatic NTFS `:Zone.Identifier` stream unblocking at startup.

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
2. Launch `LaptopOptimizer.exe` (UAC prompt will request administrative privileges for system power and disk cleanup operations).
3. Toggle the cyber switches on or off to choose your preferred tweaks, or click `TOGGLE ALL`.
4. Click **`⚡ DEPLOY TACTICAL TUNING ➔`** to engage the system tuning engine!
5. Inspect the live console logs in the execution modal and click `[ ✓ DISMISS CONSOLE ]` when finished.
6. To undo changes at any time, click **`↺ RESTORE FACTORY DEFAULTS`**.

---

## 🛠️ Building From Source

### Option A: Tauri Architecture (Recommended)

```bash
cd laptop-optimizer-tauri

# 1. Install frontend CLI tools
pnpm install

# 2. Compile optimized release executable
cargo build --release --manifest-path src-tauri/Cargo.toml
```

Output binary: `laptop-optimizer-tauri/src-tauri/target/release/laptop-optimizer-tauri.exe`.

### Option B: Native C++ MinGW Architecture

```bash
# Compile via GCC / MinGW
g++ -O2 -mwindows -static -Isrc src/main.cpp src/resource.res -o LaptopOptimizer.exe -lgdiplus -lgdi32 -luser32 -ldwmapi -lcomctl32 -lole32 -lshlwapi
```

---

## 🔒 Safety & Reversibility

* **Zero telemetry**: The app contains zero analytics, trackers, or network calls.
* **Safe defaults**: All modifications target standard Windows configuration keys.
* **Non-destructive**: Does not break Windows Update or core system integrity.
* **100% Reversible**: Every single modification can be restored in 1 click.

---

## 🛡️ Terms & Guarantee

Engineered and distributed as a free, open utility. Provided "as is" with zero warranty or liability. All optimizations are 100% reversible via the in-app Restore button. See `LICENSE` for legal terms.
