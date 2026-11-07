# UI Templates Directory

This folder contains reusable standalone UI design templates preserved from design explorations.

---

## 1. Neo-Brutalist Technical Studio (`concept3_neobrutalist_template.html`)

### **Design Archetype Overview**
- **Aesthetic**: Architectural Engineering / Neo-Brutalist High-Contrast Technical Studio.
- **Color Palette**:
  - Paper White: `#FAFAFA`
  - Canvas Cream: `#F4F4F0`
  - Pitch Black Ink: `#121212`
  - Canary Yellow Accent: `#FFE600`
- **Border & Shadows**: Heavy 2px / 4px solid black borders paired with **2.5D solid hard drop shadows** (`shadow-[4px_4px_0px_#000]`). Zero Gaussian blur.
- **Typography**: Monospace typewriter grid (`JetBrains Mono` / `Space Mono`).
- **Interactive Elements**:
  - 4-Badge Diagnostic Cluster with hard 2.5D pop-out and instant color inversion on hover.
  - Interactive parameter cards with custom checkbox spring states.
  - Bespoke Execution Console with hazard-striped animated progress bar (`repeating-linear-gradient(45deg, #000, #ffe600)`).
  - Procedural mechanical relay sound effects synthesized via the Web Audio API (zero audio files needed).

### **How to Use / Adapt for Future Tools**
1. Open `concept3_neobrutalist_template.html` in any browser to preview.
2. Edit the `TEMPLATE_CONFIG` JavaScript object at the bottom:
   - Change `title`, `systemId`, and `subtitle`.
   - Update `metrics` for your application's telemetry.
   - Modify the `options` array with your custom actions, descriptions, and badges.
3. Replace `executePipeline()` with your backend API, WebSocket, or native IPC calls.

---

## 2. C++ Win32 Brutalist Theme Header (`../src/themes/brutalist_theme.h`)
For native C++ Windows desktop applications utilizing Win32 and GDI+, include `brutalist_theme.h`:
```cpp
#include "themes/brutalist_theme.h"

// Example usage in WM_PAINT:
BrutalistTheme::DrawBrutalistCard(graphics, x, y, width, height, 4, 
    BrutalistTheme::Paper(), BrutalistTheme::Ink(), BrutalistTheme::Ink(), 2.0f);

BrutalistTheme::DrawHazardBar(graphics, x, y, width, 18, 75);
```
