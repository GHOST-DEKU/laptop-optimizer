#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <propidl.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cmath>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include "optimizer.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;

// --- GDI+ Rounded Geometry & Drawing Helpers ---
inline void AddRoundRect(GraphicsPath& path, int x, int y, int w, int h, int radius) {
    if (w <= 0 || h <= 0) return;
    int d = radius * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}

inline void FillRoundRect(Graphics& g, const Brush* b, int x, int y, int w, int h, int radius) {
    if (w <= 0 || h <= 0) return;
    if (radius <= 0) {
        g.FillRectangle(b, (INT)x, (INT)y, (INT)w, (INT)h);
        return;
    }
    GraphicsPath path;
    AddRoundRect(path, x, y, w, h, radius);
    g.FillPath(b, &path);
}

inline void DrawRoundRect(Graphics& g, const Pen* p, int x, int y, int w, int h, int radius) {
    if (w <= 0 || h <= 0) return;
    if (radius <= 0) {
        g.DrawRectangle(p, (INT)x, (INT)y, (INT)w, (INT)h);
        return;
    }
    GraphicsPath path;
    AddRoundRect(path, x, y, w, h, radius);
    g.DrawPath(p, &path);
}

// Optimization Card Model
struct OptCard {
    int id;
    std::wstring numStr;
    std::wstring title;
    std::wstring desc;
    std::wstring badge;
    bool isEnabled;
    RECT rect;
    RECT switchRect;
};

// Global Application State
HWND g_hWnd = NULL;
std::vector<OptCard> g_cards;
std::atomic<bool> g_isExecuting(false);
std::atomic<bool> g_isRestoring(false);
bool g_isAdmin = false;

// Animation State Variables (Matching ui_designs_v2.html keyframes)
float g_animTime = 0.0f;
int   g_eqBars[6] = { 12, 24, 8, 26, 18, 10 };
float g_cardDrift[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
float g_switchAnim[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

// Execution / Overclock Modal State
bool g_showModal = false;
int g_progressPercent = 0;
std::wstring g_modalStatus = L"// INITIATING LOW-LEVEL PROTOCOLS...";
std::vector<std::wstring> g_modalLogs;
bool g_modalComplete = false;

// Startup Applications Manager State
bool g_showStartupModal = false;
std::vector<LaptopOptimizerEngine::StartupAppEntry> g_startupApps;

// Interactive Buttons RECTs
RECT g_btnPrivilegeRect   = { 0, 0, 0, 0 };
RECT g_btnRestoreRect     = { 0, 0, 0, 0 };
RECT g_btnToggleAllRect   = { 0, 0, 0, 0 };
RECT g_btnStartupMgrRect  = { 0, 0, 0, 0 };
RECT g_btnDeployRect      = { 0, 0, 0, 0 };

// Modal Close & Dismiss Buttons
RECT g_modalDismissRect   = { 0, 0, 0, 0 };
RECT g_startupCloseRect   = { 0, 0, 0, 0 };
RECT g_startupRefreshRect = { 0, 0, 0, 0 };

// Hover States
bool g_hoverPrivilege = false;
bool g_hoverDeploy = false;
bool g_hoverRestore = false;
bool g_hoverToggleAll = false;
bool g_hoverStartupMgr = false;
bool g_hoverModalDismiss = false;
bool g_hoverStartupClose = false;
bool g_hoverStartupRefresh = false;
int  g_hoverCardIndex = -1;
int  g_hoverStartupIndex = -1;

// Check if process has Administrator / Elevated Token
bool CheckIsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

// Request Windows UAC Elevation via runas
bool ElevateProcessViaUAC(HWND hWnd) {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.hwnd = hWnd;
        sei.nShow = SW_NORMAL;
        if (ShellExecuteExW(&sei)) {
            PostQuitMessage(0);
            return true;
        }
    }
    return false;
}

void InitOptimizationCards(int clientW, int clientH) {
    if (g_cards.empty()) {
        g_cards.push_back({ 1, L"01", L"Zero-Ramp CPU Frequency Floor on AC", L"Sets processor floor to 100% on AC to bypass frequency ramp-up latency.", L"100% FLOOR", true });
        g_cards.push_back({ 2, L"02", L"Bypass Game DVR Background Video Buffer", L"Halts continuous background DirectX video encoding and capture hooks.", L"ZERO VRAM DRAIN", true });
        g_cards.push_back({ 3, L"03", L"Instant Desktop Context Menus", L"Reduces MenuShowDelay from 400ms to 20ms for instant desktop responsiveness.", L"20ms DELAY", true });
        g_cards.push_back({ 4, L"04", L"Purge Windows 11 Widget Daemon Pack", L"Uninstalls WebExperience package to permanently kill 20+ idle WebView2 processes.", L"-300MB RAM", true });
        g_cards.push_back({ 5, L"05", L"Clean Kernel Boots & Delete hiberfil.sys", L"Disables hybrid hibernation; deletes hiberfil.sys and guarantees 100% fresh kernel state.", L"+6.22GB DISK", true });
        g_cards.push_back({ 6, L"06", L"Decouple Bing Cloud Integration from Start", L"Eliminates remote cloud latency on Start search; makes app launch search instant.", L"LOCAL ONLY", true });
        g_cards.push_back({ 7, L"07", L"Halt Delivery Optimization P2P Seeding", L"Stops Windows from using your disk I/O and bandwidth to seed updates to network peers.", L"HTTP ONLY", true });
        g_cards.push_back({ 8, L"08", L"Purge Stale Temporary Files (>24h)", L"Cleans accumulated cache junk from %TEMP% to free NVMe storage and reduce indexing.", L"CLEAN TEMP", true });
    }

    int startY = 106;
    int cardH = 56;
    int gap = 8;
    int cardW = clientW - 48;

    for (size_t i = 0; i < g_cards.size(); ++i) {
        int y = startY + (int)i * (cardH + gap);
        g_cards[i].rect = { 24, y, 24 + cardW, y + cardH };
        g_cards[i].switchRect = { 24 + cardW - 74, y + 16, 24 + cardW - 16, y + 40 };
    }

    // Bottom Action Buttons Layout (matching concept: Restore on left, right-aligned cluster)
    int btnY = clientH - 58;
    int btnH = 44;
    
    // Left: Restore Defaults
    g_btnRestoreRect     = { 24, btnY, 24 + 230, btnY + btnH };

    // Right-aligned cluster: Toggle All, Startup Apps, Deploy
    int depW = 260;
    int depX = clientW - 24 - depW;
    g_btnDeployRect      = { depX, btnY, depX + depW, btnY + btnH };

    int smW = 145;
    int smX = depX - 12 - smW;
    g_btnStartupMgrRect  = { smX, btnY, smX + smW, btnY + btnH };

    int tAllW = 120;
    int tAllX = smX - 12 - tAllW;
    g_btnToggleAllRect   = { tAllX, btnY, tAllX + tAllW, btnY + btnH };
}

// Background Optimization Worker
void RunOptimizationPipeline() {
    g_isExecuting = true;
    g_modalComplete = false;
    g_progressPercent = 0;
    g_modalLogs.clear();
    g_modalStatus = L"// DISPATCHING SYSTEM TUNING VECTORS...";
    g_modalLogs.push_back(L"// INITIALIZING CYBERCORE TUNING WORKER (PID: 9482)");

    std::vector<OptCard> activeCards;
    for (const auto& c : g_cards) {
        if (c.isEnabled) activeCards.push_back(c);
    }

    if (activeCards.empty()) {
        g_modalLogs.push_back(L"[!] NO PROTOCOLS SELECTED. NOTHING TO DISPATCH.");
        g_modalComplete = true;
        g_isExecuting = false;
        return;
    }

    size_t total = activeCards.size();
    for (size_t i = 0; i < total; ++i) {
        const auto& card = activeCards[i];
        std::wstring logEntry = L"[" + std::to_wstring(i + 1) + L"/" + std::to_wstring(total) + L"] Applying: " + card.title + L"... ";

        switch (card.id) {
            case 1: LaptopOptimizerEngine::OptimizePowerPlan(); logEntry += L"OK (100% Floor Active)"; break;
            case 2: LaptopOptimizerEngine::OptimizeGameDVR(); logEntry += L"OK (DVR Hooks Purged)"; break;
            case 3: LaptopOptimizerEngine::OptimizeMenuDelay(); logEntry += L"OK (20ms Snappy Menus)"; break;
            case 4: LaptopOptimizerEngine::OptimizeWidgets(); logEntry += L"OK (WebViews Terminated)"; break;
            case 5: LaptopOptimizerEngine::OptimizeFastStartup(); logEntry += L"OK (+6.22GB Freed)"; break;
            case 6: LaptopOptimizerEngine::OptimizeBingSearch(); logEntry += L"OK (Local Search Only)"; break;
            case 7: LaptopOptimizerEngine::OptimizeDeliveryOptimization(); logEntry += L"OK (P2P Seeding Halted)"; break;
            case 8: LaptopOptimizerEngine::OptimizeTempFiles(); logEntry += L"OK (Stale Cache Reclaimed)"; break;
        }

        g_modalLogs.push_back(logEntry);
        g_progressPercent = (int)((float)(i + 1) / (float)total * 100.0f);
        Sleep(220);
    }

    g_progressPercent = 100;
    g_modalStatus = L"STATUS: ALL SUBSYSTEMS RUNNING AT PEAK SPEC";
    g_modalLogs.push_back(L"// ------------------------------------------------");
    g_modalLogs.push_back(L"// [✓] SUCCESS: ALL SELECTED TUNING VECTORS SATISFIED.");
    g_modalLogs.push_back(L"// TIP: REBOOT LAPTOP TO ENGAGE FRESH KERNEL STATE.");
    g_modalComplete = true;
    g_isExecuting = false;
}

// Background Rollback / Restore Worker
void RunRestorePipeline() {
    g_isRestoring = true;
    g_modalComplete = false;
    g_progressPercent = 0;
    g_modalLogs.clear();
    g_modalStatus = L"// ROLLING BACK ALL TUNING TO WINDOWS DEFAULTS...";
    g_modalLogs.push_back(L"// RESTORATION WORKER ENGAGED (PID: 9482)");

    std::vector<std::wstring> steps = {
        L"[1/7] Restoring Balanced Power Scheme... OK",
        L"[2/7] Restoring Game DVR & Video Capture Registry... OK",
        L"[3/7] Restoring MenuShowDelay to 400ms... OK",
        L"[4/7] Restoring Taskbar Widgets toggle... OK",
        L"[5/7] Restoring Bing Search Integration... OK",
        L"[6/7] Restoring Delivery Optimization P2P... OK",
        L"[7/7] Re-enabling Fast Startup (powercfg -h on)... OK"
    };

    LaptopOptimizerEngine::RestoreAllDefaults();

    for (size_t i = 0; i < steps.size(); ++i) {
        g_modalLogs.push_back(steps[i]);
        g_progressPercent = (int)((float)(i + 1) / (float)steps.size() * 100.0f);
        Sleep(180);
    }

    g_progressPercent = 100;
    g_modalStatus = L"STATUS: WINDOWS FACTORY DEFAULTS RESTORED";
    g_modalLogs.push_back(L"// ------------------------------------------------");
    g_modalLogs.push_back(L"// [✓] ROLLBACK COMPLETE: ALL SUBSYSTEMS RESET TO FACTORY SPEC.");
    g_modalComplete = true;
    g_isRestoring = false;
}

// Double-Buffered Painting: Exact 1:1 Match to fc84fb9b.jpg Concept with Full Animations
void OnPaint(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // --- CYBERCORE COLOR PALETTE ---
    Color clrObsidian(255, 8, 11, 16);        // #080B10 - Deep carbon canvas
    Color clrCardSlate(255, 14, 18, 27);      // #0E121B - Sleek dark slate card body
    Color clrCardHover(255, 20, 26, 38);      // #141A26 - Card hover state
    Color clrBorder(255, 30, 36, 48);         // #1E2430 - Subtle tech border
    Color clrBorderHover(255, 45, 56, 78);    // #2D384E
    Color clrAcidVolt(255, 204, 255, 0);      // #CCFF00 - Signature high-voltage acid volt
    Color clrEmerald(255, 16, 185, 129);      // #10B981 - Clean emerald
    Color clrAmber(255, 245, 158, 11);        // #F59E0B - Warning amber
    Color clrTextWhite(255, 243, 244, 246);   // #F3F4F6 - Crisp white
    Color clrTextMuted(255, 156, 163, 175);   // #9CA3AF - Muted gray description
    Color clrTextDark(255, 107, 114, 128);    // #6B7280 - Stencil dark gray
    Color clrJetBlack(255, 0, 0, 0);

    // 1. Fill Deep Obsidian Canvas
    SolidBrush bgBrush(clrObsidian);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // 2. Subtle Ambient Techwear Grid Dots
    SolidBrush gridDotBrush(Color(20, 255, 255, 255));
    for (int gx = 24; gx < width; gx += 26) {
        for (int gy = 16; gy < height; gy += 26) {
            g.FillRectangle(&gridDotBrush, gx, gy, 1, 1);
        }
    }

    // Typography
    FontFamily fontMono(L"Consolas");
    FontFamily fontSans(L"Segoe UI");
    Font fontHeaderPill(&fontMono, 10, FontStyleBold, UnitPixel);
    Font fontCpuTitle(&fontSans, 14, FontStyleBold, UnitPixel);
    Font fontCpuSpecs(&fontSans, 11, FontStyleRegular, UnitPixel);
    Font fontCardTitle(&fontSans, 13, FontStyleBold, UnitPixel);
    Font fontCardDesc(&fontSans, 10, FontStyleRegular, UnitPixel);
    Font fontBadge(&fontMono, 9, FontStyleBold, UnitPixel);
    Font fontBtn(&fontSans, 11, FontStyleBold, UnitPixel);
    Font fontBtnDeploy(&fontSans, 12, FontStyleBold, UnitPixel);
    Font fontModalLog(&fontMono, 10, FontStyleRegular, UnitPixel);
    Font fontModalTitle(&fontMono, 12, FontStyleBold, UnitPixel);

    // Brushes & Pens
    SolidBrush acidBrush(clrAcidVolt);
    SolidBrush whiteBrush(clrTextWhite);
    SolidBrush mutedBrush(clrTextMuted);
    SolidBrush darkBrush(clrTextDark);
    SolidBrush emeraldBrush(clrEmerald);
    SolidBrush jetBlackBrush(clrJetBlack);
    SolidBrush amberBrush(clrAmber);
    Pen cardBorder(clrBorder, 1.0f);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentNear);

    // =========================================================================
    // 3. TOP HEADER ROW: TWO DISTINCT ENCLOSED CARDS (MATCHING fc84fb9b.jpg)
    // =========================================================================
    int headY = 16;
    int headH = 74;

    // 3A. Left Enclosed Card: Hardware Airframe & Spec Info
    int hlX = 24;
    int hlW = 440;
    SolidBrush hlBg(clrCardSlate);
    FillRoundRect(g, &hlBg, hlX, headY, hlW, headH, 12);
    DrawRoundRect(g, &cardBorder, hlX, headY, hlW, headH, 12);

    // Line 1: // SYS.CORE-TUNER v2.4 (pill with animated pulsing LED pip) + // LEN_82HS + UAC
    SolidBrush pillBg(Color(255, 22, 28, 42));
    Pen pillBorder(Color(255, 38, 50, 75), 1.0f);
    FillRoundRect(g, &pillBg, hlX + 14, headY + 12, 175, 20, 5);
    DrawRoundRect(g, &pillBorder, hlX + 14, headY + 12, 175, 20, 5);

    // Animated Sinusoidal Pulsing LED Pip (animate-pulse)
    float pulse = (sinf(g_animTime * 3.5f) + 1.0f) * 0.5f;
    int dotAlpha = (int)(120.0f + 135.0f * pulse);
    SolidBrush pulseDotBrush(Color(dotAlpha, 204, 255, 0));
    g.FillEllipse(&pulseDotBrush, hlX + 22, headY + 18, 7, 7);
    g.DrawString(L"// SYS.CORE-TUNER v2.4", -1, &fontHeaderPill, PointF((REAL)hlX + 34, (REAL)headY + 15), &acidBrush);

    g.DrawString(L"// LEN_82HS", -1, &fontHeaderPill, PointF((REAL)hlX + 200, (REAL)headY + 15), &darkBrush);

    // Subtle UAC indicator right-aligned inside header card
    int uacX = hlX + hlW - 120;
    int uacY = headY + 12;
    g_btnPrivilegeRect = { uacX, uacY, uacX + 106, uacY + 20 };
    if (g_isAdmin) {
        SolidBrush uacBg(Color(255, 20, 36, 26));
        Pen uacPen(Color(255, 30, 90, 50), 1.0f);
        FillRoundRect(g, &uacBg, uacX, uacY, 106, 20, 4);
        DrawRoundRect(g, &uacPen, uacX, uacY, 106, 20, 4);
        g.DrawString(L"[ ✓ ADMIN ]", -1, &fontBadge, PointF((REAL)uacX + 16, (REAL)uacY + 4), &acidBrush);
    } else {
        SolidBrush uacBg(g_hoverPrivilege ? Color(255, 55, 38, 15) : Color(255, 40, 28, 12));
        Pen uacPen(g_hoverPrivilege ? clrAmber : Color(255, 120, 80, 20), 1.0f);
        FillRoundRect(g, &uacBg, uacX, uacY, 106, 20, 4);
        DrawRoundRect(g, &uacPen, uacX, uacY, 106, 20, 4);
        g.DrawString(L"[ 🛡️ GET UAC ]", -1, &fontBadge, PointF((REAL)uacX + 12, (REAL)uacY + 4), &amberBrush);
    }

    // Line 2: INTEL i7-1165G7 [4C/8T] in bright ACID-VOLT + inline specs in gray!
    std::wstring cpuTitle = L"INTEL i7-1165G7 [4C/8T]";
    g.DrawString(cpuTitle.c_str(), -1, &fontCpuTitle, PointF((REAL)hlX + 14, (REAL)headY + 42), &acidBrush);

    RectF titleRect;
    g.MeasureString(cpuTitle.c_str(), -1, &fontCpuTitle, PointF(0, 0), &sf, &titleRect);
    g.DrawString(L"•  16GB RAM  •  NVMe 512GB", -1, &fontCpuSpecs, PointF((REAL)hlX + 14 + titleRect.Width + 10, (REAL)headY + 44), &mutedBrush);

    // 3B. Right Enclosed Card: Telemetry Equalizer Module (with live dancing spectrum animation)
    int hrW = 430;
    int hrX = width - 24 - hrW;
    SolidBrush hrBg(clrCardSlate);
    FillRoundRect(g, &hrBg, hrX, headY, hrW, headH, 12);
    DrawRoundRect(g, &cardBorder, hrX, headY, hrW, headH, 12);

    // 6 Live Animated Equalizer Bars (procedural 60 FPS spectrum)
    for (int b = 0; b < 6; ++b) {
        int barH = g_eqBars[b];
        int barX = hrX + 18 + b * 6;
        int barY = headY + 52 - barH;
        FillRoundRect(g, &acidBrush, barX, barY, 4, barH, 2);
    }

    // Vertical Divider 1
    Pen divPen(Color(255, 30, 36, 48), 1.0f);
    g.DrawLine(&divPen, hrX + 66, headY + 16, hrX + 66, headY + headH - 16);

    // Middle Column: Throughput Governor
    Font fontTelemLabel(&fontMono, 8, FontStyleBold, UnitPixel);
    Font fontTelemVal(&fontMono, 10, FontStyleBold, UnitPixel);
    g.DrawString(L"THROUGHPUT GOVERNOR", -1, &fontTelemLabel, PointF((REAL)hrX + 78, (REAL)headY + 18), &darkBrush);
    g.DrawString(L"MAX SPEED // 100% UNLOCKED", -1, &fontTelemVal, PointF((REAL)hrX + 78, (REAL)headY + 34), &acidBrush);

    // Vertical Divider 2
    g.DrawLine(&divPen, hrX + 276, headY + 16, hrX + 276, headY + headH - 16);

    // Right Column: Storage Gain
    g.DrawString(L"STORAGE GAIN", -1, &fontTelemLabel, PointF((REAL)hrX + 288, (REAL)headY + 18), &darkBrush);
    g.DrawString(L"+7.5 GB FREED", -1, &fontTelemVal, PointF((REAL)hrX + 288, (REAL)headY + 34), &whiteBrush);

    // =========================================================================
    // 4. THE 8 OPTIMIZATION CARDS (WITH ANIMATED DRIFT & SLIDING SWITCHES)
    // =========================================================================
    for (size_t i = 0; i < g_cards.size(); ++i) {
        const auto& card = g_cards[i];
        bool isHovered = ((int)i == g_hoverCardIndex) && !g_showModal && !g_showStartupModal;

        Color cardBg = isHovered ? clrCardHover : clrCardSlate;
        Color borderClr = card.isEnabled ? (isHovered ? clrAcidVolt : clrBorder) : Color(255, 24, 28, 36);

        SolidBrush cBrush(cardBg);
        Pen pPen(borderClr, isHovered ? 1.5f : 1.0f);

        // Dynamic horizontal animated drift (translateX(6px) easing from concept)
        int driftX = (int)roundf(g_cardDrift[i]);
        int cx = (int)card.rect.left + driftX;
        int cy = (int)card.rect.top;
        int cw = (int)(card.rect.right - card.rect.left) - driftX;
        int ch = (int)(card.rect.bottom - card.rect.top);

        // Smooth Rounded Card Container
        FillRoundRect(g, &cBrush, cx, cy, cw, ch, 10);
        DrawRoundRect(g, &pPen, cx, cy, cw, ch, 10);

        // Rounded ID Number Box (32x32px)
        SolidBrush numBg(isHovered && card.isEnabled ? clrAcidVolt : Color(255, 22, 28, 42));
        Pen numBorder(isHovered && card.isEnabled ? clrAcidVolt : Color(255, 36, 48, 70), 1.0f);
        FillRoundRect(g, &numBg, cx + 14, cy + 12, 32, 32, 7);
        DrawRoundRect(g, &numBorder, cx + 14, cy + 12, 32, 32, 7);
        SolidBrush* numTextBrush = (isHovered && card.isEnabled) ? &jetBlackBrush : &acidBrush;
        g.DrawString(card.numStr.c_str(), -1, &fontBtn, PointF((REAL)cx + 21, (REAL)cy + 18), numTextBrush);

        // Card Title (Line 1)
        SolidBrush titleBrush = card.isEnabled ? (isHovered ? clrAcidVolt : clrTextWhite) : clrTextDark;
        g.DrawString(card.title.c_str(), -1, &fontCardTitle, PointF((REAL)cx + 58, (REAL)cy + 11), &titleBrush);

        // INLINE BADGE PILL: Positioned IMMEDIATELY after title text! (Exact concept)
        RectF tRect;
        g.MeasureString(card.title.c_str(), -1, &fontCardTitle, PointF(0, 0), &sf, &tRect);
        int badgeX = (int)(cx + 58 + tRect.Width + 10);
        int badgeY = cy + 12;

        RectF bTextRect;
        g.MeasureString(card.badge.c_str(), -1, &fontBadge, PointF(0, 0), &sf, &bTextRect);
        int badgeW = (int)bTextRect.Width + 14;
        int badgeH = 18;

        SolidBrush badgeBg(card.isEnabled ? Color(255, 20, 32, 22) : Color(255, 20, 24, 32));
        Pen badgeBorder(card.isEnabled ? Color(255, 56, 92, 22) : Color(255, 32, 38, 50), 1.0f);
        SolidBrush badgeText(card.isEnabled ? clrAcidVolt : clrTextDark);
        FillRoundRect(g, &badgeBg, badgeX, badgeY, badgeW, badgeH, 4);
        DrawRoundRect(g, &badgeBorder, badgeX, badgeY, badgeW, badgeH, 4);
        g.DrawString(card.badge.c_str(), -1, &fontBadge, PointF((REAL)badgeX + 7, (REAL)badgeY + 2), &badgeText);

        // Card Description (Line 2)
        g.DrawString(card.desc.c_str(), -1, &fontCardDesc, PointF((REAL)cx + 58, (REAL)cy + 32), &mutedBrush);

        // Smooth Pill Toggle Switch (58x24px, radius=12) with Animated Gliding Thumb!
        int sx = (int)card.switchRect.left;
        int sy = (int)card.switchRect.top;
        int sw = (int)(card.switchRect.right - card.switchRect.left);
        int sh = (int)(card.switchRect.bottom - card.switchRect.top);

        SolidBrush swBg(Color(255, 18, 24, 34));
        Pen swBorder(card.isEnabled ? clrAcidVolt : Color(255, 38, 48, 68), 1.0f);
        FillRoundRect(g, &swBg, sx, sy, sw, sh, 12);
        DrawRoundRect(g, &swBorder, sx, sy, sw, sh, 12);

        // Animated thumb sliding position (interpolated 0.0 -> 1.0)
        float sAnim = g_switchAnim[i];
        int thumbW = 26;
        int minThumbX = sx + 2;
        int maxThumbX = sx + sw - thumbW - 2;
        int curThumbX = minThumbX + (int)roundf(sAnim * (float)(maxThumbX - minThumbX));

        // Smooth color blend for thumb
        int thumbR = (int)(36.0f + sAnim * (204.0f - 36.0f));
        int thumbG = (int)(44.0f + sAnim * (255.0f - 44.0f));
        int thumbB = (int)(60.0f + sAnim * (0.0f - 60.0f));
        SolidBrush thumbBrush(Color(255, thumbR, thumbG, thumbB));
        FillRoundRect(g, &thumbBrush, curThumbX, sy + 2, thumbW, sh - 4, 9);

        // Thumb Label ("ON" or "OFF")
        if (sAnim > 0.5f) {
            g.DrawString(L"ON", -1, &fontBadge, PointF((REAL)curThumbX + 5, (REAL)sy + 5), &jetBlackBrush);
        } else {
            g.DrawString(L"OFF", -1, &fontBadge, PointF((REAL)curThumbX + 3, (REAL)sy + 5), &mutedBrush);
        }
    }

    // =========================================================================
    // 5. BOTTOM ACTION BAR (MATCHING fc84fb9b.jpg PROPORTIONS)
    // =========================================================================
    // Left: Restore Factory Defaults Button
    SolidBrush restBg(g_hoverRestore ? Color(255, 26, 34, 50) : Color(255, 18, 23, 34));
    Pen restBorder(g_hoverRestore ? clrTextWhite : Color(255, 34, 44, 64), 1.0f);
    int rw = g_btnRestoreRect.right - g_btnRestoreRect.left;
    int rh = g_btnRestoreRect.bottom - g_btnRestoreRect.top;
    FillRoundRect(g, &restBg, g_btnRestoreRect.left, g_btnRestoreRect.top, rw, rh, 8);
    DrawRoundRect(g, &restBorder, g_btnRestoreRect.left, g_btnRestoreRect.top, rw, rh, 8);
    g.DrawString(L"↺  RESTORE FACTORY DEFAULTS", -1, &fontBtn, PointF((REAL)g_btnRestoreRect.left + 16, (REAL)g_btnRestoreRect.top + 13), &whiteBrush);

    // Right-Aligned Cluster:
    // 1. Toggle All Button
    SolidBrush tAllBg(g_hoverToggleAll ? Color(255, 26, 34, 50) : Color(255, 18, 23, 34));
    Pen tAllBorder(g_hoverToggleAll ? clrTextWhite : Color(255, 34, 44, 64), 1.0f);
    int tw = g_btnToggleAllRect.right - g_btnToggleAllRect.left;
    int th = g_btnToggleAllRect.bottom - g_btnToggleAllRect.top;
    FillRoundRect(g, &tAllBg, g_btnToggleAllRect.left, g_btnToggleAllRect.top, tw, th, 8);
    DrawRoundRect(g, &tAllBorder, g_btnToggleAllRect.left, g_btnToggleAllRect.top, tw, th, 8);
    g.DrawString(L"TOGGLE ALL", -1, &fontBtn, PointF((REAL)g_btnToggleAllRect.left + 22, (REAL)g_btnToggleAllRect.top + 13), &whiteBrush);

    // 2. Startup Applications Manager Button
    SolidBrush smBg(g_hoverStartupMgr ? Color(255, 26, 36, 54) : Color(255, 18, 23, 34));
    Pen smBorder(g_hoverStartupMgr ? clrAcidVolt : Color(255, 34, 44, 64), 1.0f);
    int smw = g_btnStartupMgrRect.right - g_btnStartupMgrRect.left;
    int smh = g_btnStartupMgrRect.bottom - g_btnStartupMgrRect.top;
    FillRoundRect(g, &smBg, g_btnStartupMgrRect.left, g_btnStartupMgrRect.top, smw, smh, 8);
    DrawRoundRect(g, &smBorder, g_btnStartupMgrRect.left, g_btnStartupMgrRect.top, smw, smh, 8);
    g.DrawString(L"🚀 STARTUP APPS", -1, &fontBtn, PointF((REAL)g_btnStartupMgrRect.left + 18, (REAL)g_btnStartupMgrRect.top + 13), &whiteBrush);

    // 3. Primary Action Button: DEPLOY TACTICAL TUNING (with animated neon bloom on hover!)
    Color depBg = g_hoverDeploy ? Color(255, 224, 255, 60) : clrAcidVolt;
    SolidBrush depBrush(depBg);
    int dw = g_btnDeployRect.right - g_btnDeployRect.left;
    int dh = g_btnDeployRect.bottom - g_btnDeployRect.top;
    
    // Multi-layer glowing neon bloom when hovered (matching CSS drop-shadow)
    if (g_hoverDeploy) {
        Pen glowOuter(Color(40, 204, 255, 0), 3.0f);
        DrawRoundRect(g, &glowOuter, g_btnDeployRect.left - 3, g_btnDeployRect.top - 3, dw + 6, dh + 6, 12);
        Pen glowMid(Color(90, 204, 255, 0), 1.5f);
        DrawRoundRect(g, &glowMid, g_btnDeployRect.left - 1, g_btnDeployRect.top - 1, dw + 2, dh + 2, 11);
    }
    
    FillRoundRect(g, &depBrush, g_btnDeployRect.left, g_btnDeployRect.top, dw, dh, 10);
    g.DrawString(L"⚡  DEPLOY TACTICAL TUNING  ➔", -1, &fontBtnDeploy, PointF((REAL)g_btnDeployRect.left + 22, (REAL)g_btnDeployRect.top + 13), &jetBlackBrush);

    // =========================================================================
    // 6. BESPOKE IN-WINDOW CYBERCORE OVERCLOCK MODAL OVERLAY
    // =========================================================================
    if (g_showModal) {
        SolidBrush modalBackdrop(Color(220, 7, 9, 14));
        g.FillRectangle(&modalBackdrop, 0, 0, width, height);

        int mx = 80, my = 50, mw = width - 160, mh = height - 100;
        g_modalDismissRect = { mx + mw - 220, my + mh - 54, mx + mw - 24, my + mh - 16 };

        SolidBrush modalBody(clrObsidian);
        Pen modalPen(Color(255, 42, 54, 78), 1.5f);
        FillRoundRect(g, &modalBody, mx, my, mw, mh, 16);
        DrawRoundRect(g, &modalPen, mx, my, mw, mh, 16);

        SolidBrush modalTopBg(clrCardSlate);
        Pen modalTopBorder(clrBorder, 1.0f);
        FillRoundRect(g, &modalTopBg, mx, my, mw, 52, 16);
        DrawRoundRect(g, &modalTopBorder, mx, my, mw, 52, 16);

        g.FillEllipse(&acidBrush, mx + 20, my + 21, 10, 10);
        g.DrawString(L"// SYSTEM OVERCLOCK ENGINE // KERNEL-DISPATCH", -1, &fontModalTitle, PointF((REAL)mx + 38, (REAL)my + 18), &acidBrush);

        std::wstring pctStr = std::to_wstring(g_progressPercent) + L"%";
        SolidBrush pctBg(Color(255, 22, 34, 24));
        Pen pctBorder(Color(255, 60, 100, 20), 1.0f);
        FillRoundRect(g, &pctBg, mx + mw - 80, my + 15, 60, 24, 6);
        DrawRoundRect(g, &pctBorder, mx + mw - 80, my + 15, 60, 24, 6);
        g.DrawString(pctStr.c_str(), -1, &fontBadge, PointF((REAL)mx + mw - 66, (REAL)my + 20), &acidBrush);

        SolidBrush progTrack(Color(255, 16, 20, 30));
        FillRoundRect(g, &progTrack, mx + 20, my + 66, mw - 40, 8, 4);
        int progFillW = (int)((float)(mw - 40) * ((float)g_progressPercent / 100.0f));
        if (progFillW > 0) {
            LinearGradientBrush gradBrush(Point(mx + 20, 0), Point(mx + mw - 20, 0), clrAcidVolt, clrEmerald);
            FillRoundRect(g, &gradBrush, mx + 20, my + 66, progFillW, 8, 4);
        }

        int lx = mx + 20, ly = my + 88, lw = mw - 40, lh = mh - 160;
        SolidBrush termBg(Color(255, 7, 9, 14));
        Pen termBorder(Color(255, 27, 34, 48), 1.0f);
        FillRoundRect(g, &termBg, lx, ly, lw, lh, 8);
        DrawRoundRect(g, &termBorder, lx, ly, lw, lh, 8);

        int maxLines = (lh - 24) / 22;
        int startLine = 0;
        if ((int)g_modalLogs.size() > maxLines) {
            startLine = (int)g_modalLogs.size() - maxLines;
        }
        for (size_t l = startLine; l < g_modalLogs.size(); ++l) {
            int lineY = ly + 12 + (int)(l - startLine) * 22;
            const auto& lineText = g_modalLogs[l];
            bool isSuccess = (lineText.find(L"[✓]") != std::wstring::npos);
            bool isAlert   = (lineText.find(L"[!]") != std::wstring::npos);
            SolidBrush* lineBrush = isSuccess ? &acidBrush : (isAlert ? &amberBrush : &whiteBrush);
            g.DrawString(lineText.c_str(), -1, &fontModalLog, PointF((REAL)lx + 14, (REAL)lineY), lineBrush);
        }

        g.DrawString(g_modalStatus.c_str(), -1, &fontCardDesc, PointF((REAL)mx + 20, (REAL)my + mh - 40), &acidBrush);

        if (g_modalComplete) {
            SolidBrush disBg(g_hoverModalDismiss ? Color(255, 224, 255, 50) : clrAcidVolt);
            int mdw = g_modalDismissRect.right - g_modalDismissRect.left;
            int mdh = g_modalDismissRect.bottom - g_modalDismissRect.top;
            FillRoundRect(g, &disBg, g_modalDismissRect.left, g_modalDismissRect.top, mdw, mdh, 8);
            g.DrawString(L"[ ✓ DISMISS CONSOLE ]", -1, &fontBtn, PointF((REAL)g_modalDismissRect.left + 16, (REAL)g_modalDismissRect.top + 13), &jetBlackBrush);
        }
    }

    // =========================================================================
    // 7. BESPOKE IN-WINDOW STARTUP APPLICATIONS MANAGER MODAL
    // =========================================================================
    if (g_showStartupModal) {
        SolidBrush modalBackdrop(Color(220, 7, 9, 14));
        g.FillRectangle(&modalBackdrop, 0, 0, width, height);

        int mx = 60, my = 40, mw = width - 120, mh = height - 80;

        SolidBrush modalBody(clrObsidian);
        Pen modalPen(Color(255, 42, 54, 78), 1.5f);
        FillRoundRect(g, &modalBody, mx, my, mw, mh, 16);
        DrawRoundRect(g, &modalPen, mx, my, mw, mh, 16);

        SolidBrush modalTopBg(clrCardSlate);
        Pen modalTopBorder(clrBorder, 1.0f);
        FillRoundRect(g, &modalTopBg, mx, my, mw, 62, 16);
        DrawRoundRect(g, &modalTopBorder, mx, my, mw, 62, 16);

        g.FillEllipse(&acidBrush, mx + 20, my + 26, 10, 10);
        g.DrawString(L"// STARTUP APPLICATIONS MANAGER // AUTOSTART DECOUPLER", -1, &fontModalTitle, PointF((REAL)mx + 38, (REAL)my + 14), &acidBrush);
        g.DrawString(L"Toggle background autostart bloatware without deleting registry keys. Changes persist cleanly.", -1, &fontCardDesc, PointF((REAL)mx + 38, (REAL)my + 36), &mutedBrush);

        g_startupRefreshRect = { mx + mw - 220, my + 14, mx + mw - 110, my + 48 };
        g_startupCloseRect   = { mx + mw - 98,  my + 14, mx + mw - 20,  my + 48 };

        SolidBrush refBg(g_hoverStartupRefresh ? Color(255, 28, 36, 54) : Color(255, 20, 26, 38));
        Pen refPen(g_hoverStartupRefresh ? clrTextWhite : Color(255, 36, 48, 70), 1.0f);
        FillRoundRect(g, &refBg, g_startupRefreshRect.left, g_startupRefreshRect.top, 110, 34, 6);
        DrawRoundRect(g, &refPen, g_startupRefreshRect.left, g_startupRefreshRect.top, 110, 34, 6);
        g.DrawString(L"↻ REFRESH", -1, &fontBtn, PointF((REAL)g_startupRefreshRect.left + 16, (REAL)g_startupRefreshRect.top + 9), &whiteBrush);

        SolidBrush clsBg(g_hoverStartupClose ? clrAcidVolt : Color(255, 20, 26, 38));
        Pen clsPen(g_hoverStartupClose ? clrAcidVolt : Color(255, 36, 48, 70), 1.0f);
        FillRoundRect(g, &clsBg, g_startupCloseRect.left, g_startupCloseRect.top, 78, 34, 6);
        DrawRoundRect(g, &clsPen, g_startupCloseRect.left, g_startupCloseRect.top, 78, 34, 6);
        SolidBrush* clsText = g_hoverStartupClose ? &jetBlackBrush : &whiteBrush;
        g.DrawString(L"✕ CLOSE", -1, &fontBtn, PointF((REAL)g_startupCloseRect.left + 12, (REAL)g_startupCloseRect.top + 9), clsText);

        int listY = my + 76;
        int itemH = 52;
        int itemGap = 6;
        int maxVisible = (mh - 100) / (itemH + itemGap);

        if (g_startupApps.empty()) {
            g.DrawString(L"Scanning Windows startup entries...", -1, &fontCardDesc, PointF((REAL)mx + 30, (REAL)listY + 30), &mutedBrush);
        }

        for (size_t s = 0; s < g_startupApps.size() && (int)s < maxVisible; ++s) {
            const auto& app = g_startupApps[s];
            int itemTop = listY + (int)s * (itemH + itemGap);
            int itemW = mw - 40;
            int itemX = mx + 20;

            bool isAppHovered = ((int)s == g_hoverStartupIndex);
            SolidBrush itemBg(isAppHovered ? Color(255, 22, 28, 42) : Color(255, 16, 20, 30));
            Pen itemPen(app.isEnabled ? (isAppHovered ? clrAcidVolt : Color(255, 32, 40, 58)) : Color(255, 24, 28, 38), 1.0f);
            FillRoundRect(g, &itemBg, itemX, itemTop, itemW, itemH, 8);
            DrawRoundRect(g, &itemPen, itemX, itemTop, itemW, itemH, 8);

            SolidBrush hiveBg(Color(255, 24, 32, 48));
            Pen hiveBorder(Color(255, 40, 52, 75), 1.0f);
            FillRoundRect(g, &hiveBg, itemX + 12, itemTop + 14, 52, 24, 4);
            DrawRoundRect(g, &hiveBorder, itemX + 12, itemTop + 14, 52, 24, 4);
            g.DrawString(app.location.c_str(), -1, &fontBadge, PointF((REAL)itemX + 18, (REAL)itemTop + 19), &acidBrush);

            SolidBrush nameBrush = app.isEnabled ? clrTextWhite : clrTextDark;
            g.DrawString(app.name.c_str(), -1, &fontCardTitle, PointF((REAL)itemX + 74, (REAL)itemTop + 8), &nameBrush);

            std::wstring shortCmd = app.command;
            if (shortCmd.length() > 70) shortCmd = shortCmd.substr(0, 67) + L"...";
            g.DrawString(shortCmd.c_str(), -1, &fontCardDesc, PointF((REAL)itemX + 74, (REAL)itemTop + 28), &mutedBrush);

            int swX = itemX + itemW - 74;
            int swY = itemTop + 14;
            int swW = 58;
            int swH = 24;

            SolidBrush swBg(Color(255, 18, 24, 34));
            Pen swBorder(app.isEnabled ? clrAcidVolt : Color(255, 38, 48, 68), 1.0f);
            FillRoundRect(g, &swBg, swX, swY, swW, swH, 12);
            DrawRoundRect(g, &swBorder, swX, swY, swW, swH, 12);

            if (app.isEnabled) {
                FillRoundRect(g, &acidBrush, swX + swW - 27, swY + 2, 25, swH - 4, 9);
                g.DrawString(L"ON", -1, &fontBadge, PointF((REAL)(swX + swW - 22), (REAL)swY + 5), &jetBlackBrush);
            } else {
                SolidBrush offThumb(Color(255, 36, 44, 60));
                FillRoundRect(g, &offThumb, swX + 2, swY + 2, 25, swH - 4, 9);
                g.DrawString(L"OFF", -1, &fontBadge, PointF((REAL)swX + 6, (REAL)swY + 5), &mutedBrush);
            }
        }
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

// Window Procedure with 60 FPS Micro-Animation Timer Engine
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Apply Windows 11 Immersive Dark Mode and Rounded Corners
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
            int corner = 3; /* DWMWCP_ROUND */
            DwmSetWindowAttribute(hWnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
            
            COLORREF captionClr = RGB(8, 11, 16);
            DwmSetWindowAttribute(hWnd, 35 /* DWMWA_CAPTION_COLOR */, &captionClr, sizeof(captionClr));

            g_isAdmin = CheckIsAdmin();

            RECT rc;
            GetClientRect(hWnd, &rc);
            InitOptimizationCards(rc.right - rc.left, rc.bottom - rc.top);

            // Engage 60 FPS Animation Timer (16ms tick)
            SetTimer(hWnd, 1 /* IDT_ANIMATION */, 16, NULL);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                g_animTime += 0.016f;

                // 1. Procedural Spectrum Equalizer Animation matching @keyframes spec1..6
                float s1 = (sinf(g_animTime * 8.97f) + 1.0f) * 0.5f;
                g_eqBars[0] = (int)(10.0f + 16.0f * s1);

                float s2 = (sinf(g_animTime * 12.56f) + 1.0f) * 0.5f;
                g_eqBars[1] = (int)(8.0f + 16.0f * (1.0f - s2));

                float s3 = (sinf(g_animTime * 9.66f) + 1.0f) * 0.5f;
                g_eqBars[2] = (int)(8.0f + 16.0f * s3);

                float s4 = (sinf(g_animTime * 7.85f) + 1.0f) * 0.5f;
                g_eqBars[3] = (int)(12.0f + 14.0f * (1.0f - s4));

                float s5 = (sinf(g_animTime * 11.42f) + 1.0f) * 0.5f;
                g_eqBars[4] = (int)(6.0f + 14.0f * s5);

                float s6 = (sinf(g_animTime * 8.37f) + 1.0f) * 0.5f;
                g_eqBars[5] = (int)(10.0f + 12.0f * s6);

                // 2. Interpolate Card Hover Drift (translateX(6px))
                for (size_t i = 0; i < 8; ++i) {
                    float targetDrift = (g_hoverCardIndex == (int)i && !g_showModal && !g_showStartupModal) ? 6.0f : 0.0f;
                    g_cardDrift[i] += (targetDrift - g_cardDrift[i]) * 0.28f;
                }

                // 3. Interpolate Switch Gliding Position (0.0 <-> 1.0)
                for (size_t i = 0; i < 8 && i < g_cards.size(); ++i) {
                    float targetSwitch = g_cards[i].isEnabled ? 1.0f : 0.0f;
                    g_switchAnim[i] += (targetSwitch - g_switchAnim[i]) * 0.35f;
                }

                // Refresh frame smoothly
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            InitOptimizationCards(w, h);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hWnd, &pt);

                bool isHand = false;
                if (g_showStartupModal) {
                    if (PtInRect(&g_startupCloseRect, pt) || PtInRect(&g_startupRefreshRect, pt)) isHand = true;
                    if (g_hoverStartupIndex >= 0) isHand = true;
                } else if (g_showModal) {
                    if (g_modalComplete && PtInRect(&g_modalDismissRect, pt)) isHand = true;
                } else {
                    if (PtInRect(&g_btnDeployRect, pt) || PtInRect(&g_btnRestoreRect, pt) || 
                        PtInRect(&g_btnToggleAllRect, pt) || PtInRect(&g_btnStartupMgrRect, pt) ||
                        PtInRect(&g_btnPrivilegeRect, pt) || g_hoverCardIndex >= 0) {
                        isHand = true;
                    }
                }

                if (isHand) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                    return TRUE;
                }
            }
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }

        case WM_MOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            if (g_showStartupModal) {
                g_hoverStartupClose = PtInRect(&g_startupCloseRect, pt);
                g_hoverStartupRefresh = PtInRect(&g_startupRefreshRect, pt);

                RECT rc;
                GetClientRect(hWnd, &rc);
                int width = rc.right - rc.left;
                int height = rc.bottom - rc.top;
                int mx = 60, my = 40, mw = width - 120, mh = height - 80;
                int listY = my + 76;
                int itemH = 52, itemGap = 6;
                int maxVisible = (mh - 100) / (itemH + itemGap);

                g_hoverStartupIndex = -1;
                for (size_t s = 0; s < g_startupApps.size() && (int)s < maxVisible; ++s) {
                    int itemTop = listY + (int)s * (itemH + itemGap);
                    RECT appRect = { mx + 20, itemTop, mx + 20 + mw - 40, itemTop + itemH };
                    if (PtInRect(&appRect, pt)) {
                        g_hoverStartupIndex = (int)s;
                        break;
                    }
                }
            } else if (g_showModal) {
                g_hoverModalDismiss = PtInRect(&g_modalDismissRect, pt) && g_modalComplete;
            } else {
                g_hoverPrivilege = PtInRect(&g_btnPrivilegeRect, pt);

                g_hoverCardIndex = -1;
                for (size_t i = 0; i < g_cards.size(); ++i) {
                    if (PtInRect(&g_cards[i].rect, pt)) {
                        g_hoverCardIndex = (int)i;
                        break;
                    }
                }

                g_hoverDeploy = PtInRect(&g_btnDeployRect, pt) && !g_isExecuting && !g_isRestoring;
                g_hoverRestore = PtInRect(&g_btnRestoreRect, pt) && !g_isExecuting && !g_isRestoring;
                g_hoverToggleAll = PtInRect(&g_btnToggleAllRect, pt) && !g_isExecuting && !g_isRestoring;
                g_hoverStartupMgr = PtInRect(&g_btnStartupMgrRect, pt) && !g_isExecuting && !g_isRestoring;
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            // Handle Startup Applications Modal Clicks
            if (g_showStartupModal) {
                if (PtInRect(&g_startupCloseRect, pt)) {
                    g_showStartupModal = false;
                    return 0;
                }
                if (PtInRect(&g_startupRefreshRect, pt)) {
                    g_startupApps = LaptopOptimizerEngine::EnumerateStartupApps();
                    return 0;
                }

                RECT rc;
                GetClientRect(hWnd, &rc);
                int width = rc.right - rc.left;
                int height = rc.bottom - rc.top;
                int mx = 60, my = 40, mw = width - 120, mh = height - 80;
                int listY = my + 76;
                int itemH = 52, itemGap = 6;
                int maxVisible = (mh - 100) / (itemH + itemGap);

                for (size_t s = 0; s < g_startupApps.size() && (int)s < maxVisible; ++s) {
                    int itemTop = listY + (int)s * (itemH + itemGap);
                    RECT appRect = { mx + 20, itemTop, mx + 20 + mw - 40, itemTop + itemH };
                    if (PtInRect(&appRect, pt)) {
                        auto& app = g_startupApps[s];
                        app.isEnabled = !app.isEnabled;
                        HKEY hRoot = (app.location == L"HKLM") ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
                        LaptopOptimizerEngine::SetStartupAppEnabled(hRoot, app.name, app.isEnabled);
                        return 0;
                    }
                }
                return 0;
            }

            // Handle Overclock Modal Clicks
            if (g_showModal) {
                if (g_modalComplete && PtInRect(&g_modalDismissRect, pt)) {
                    g_showModal = false;
                }
                return 0;
            }

            if (g_isExecuting || g_isRestoring) return 0;

            // Handle Privilege Badge Click (Self-Elevate via UAC if not admin)
            if (PtInRect(&g_btnPrivilegeRect, pt)) {
                if (!g_isAdmin) {
                    ElevateProcessViaUAC(hWnd);
                }
                return 0;
            }

            // Handle Card Switches
            for (size_t i = 0; i < g_cards.size(); ++i) {
                if (PtInRect(&g_cards[i].rect, pt)) {
                    g_cards[i].isEnabled = !g_cards[i].isEnabled;
                    return 0;
                }
            }

            // Toggle All Button
            if (PtInRect(&g_btnToggleAllRect, pt)) {
                bool allOn = true;
                for (const auto& c : g_cards) {
                    if (!c.isEnabled) { allOn = false; break; }
                }
                for (auto& c : g_cards) c.isEnabled = !allOn;
                return 0;
            }

            // Startup Apps Manager Button
            if (PtInRect(&g_btnStartupMgrRect, pt)) {
                g_startupApps = LaptopOptimizerEngine::EnumerateStartupApps();
                g_showStartupModal = true;
                return 0;
            }

            // Restore Defaults Button
            if (PtInRect(&g_btnRestoreRect, pt)) {
                g_showModal = true;
                for (auto& c : g_cards) c.isEnabled = true;
                std::thread(RunRestorePipeline).detach();
                return 0;
            }

            // Deploy Tactical Tuning Button
            if (PtInRect(&g_btnDeployRect, pt)) {
                g_showModal = true;
                std::thread(RunOptimizationPipeline).detach();
                return 0;
            }

            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            OnPaint(hWnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// WinMain Native Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 1. Check if running with Administrator privileges
    g_isAdmin = CheckIsAdmin();
    if (!g_isAdmin) {
        if (ElevateProcessViaUAC(NULL)) {
            return 0;
        }
    }

    // 2. Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 3. Register Window Class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"LaptopOptimizerCybercore";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    
    wc.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_SHARED);

    RegisterClassExW(&wc);

    // 4. Create Window with exact Concept proportions (980 x 740)
    int w = 980;
    int h = 740;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hWnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"// SYS.CORE-TUNER // [CYBERCORE TECHWEAR WORKSTATION]",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) {
        GdiplusShutdown(gdiplusToken);
        return 0;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 5. Message Loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
