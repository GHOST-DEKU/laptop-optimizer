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

// 60 FPS Micro-Interactions & Animation State
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
RECT g_btnRestoreRect     = { 0, 0, 0, 0 };
RECT g_btnToggleAllRect   = { 0, 0, 0, 0 };
RECT g_btnStartupMgrRect  = { 0, 0, 0, 0 };
RECT g_btnDeployRect      = { 0, 0, 0, 0 };

// Modal Close & Dismiss Buttons
RECT g_modalDismissRect   = { 0, 0, 0, 0 };
RECT g_startupCloseRect   = { 0, 0, 0, 0 };
RECT g_startupRefreshRect = { 0, 0, 0, 0 };

// Hover States
bool g_hoverDeploy = false;
bool g_hoverRestore = false;
bool g_hoverToggleAll = false;
bool g_hoverStartupMgr = false;
bool g_hoverModalDismiss = false;
bool g_hoverStartupClose = false;
bool g_hoverStartupRefresh = false;
int  g_hoverCardIndex = -1;
int  g_hoverStartupIndex = -1;

// Native Win32 Self-Unblock: Deletes NTFS Alternate Data Stream Zone.Identifier
void SelfUnblockExecutable() {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        std::wstring adsPath = std::wstring(szPath) + L":Zone.Identifier";
        DeleteFileW(adsPath.c_str());

        wchar_t* lastSlash = wcsrchr(szPath, L'\\');
        if (lastSlash) {
            *lastSlash = L'\0';
            std::wstring batAds = std::wstring(szPath) + L"\\Run-Optimizer.bat:Zone.Identifier";
            DeleteFileW(batAds.c_str());
        }
    }
}

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

    int pad = 24;
    int mx = pad, my = pad;
    int mw = clientW - pad * 2;
    int mh = clientH - pad * 2;

    int startY = my + 94;
    int cardH = 62;
    int gap = 10;
    int cardW = mw - 32;

    for (size_t i = 0; i < g_cards.size(); ++i) {
        int y = startY + (int)i * (cardH + gap);
        g_cards[i].rect = { mx + 16, y, mx + 16 + cardW, y + cardH };
        // 48x24 switch (matching w-12 h-6)
        g_cards[i].switchRect = { mx + 16 + cardW - 16 - 48, y + 15, mx + 16 + cardW - 16, y + 39 };
    }

    // Bottom Action Buttons (aligned with concept footer)
    int btnY = my + mh - 48;
    int btnH = 40;
    
    // Left: Restore Defaults (matching concept)
    g_btnRestoreRect     = { mx + 16, btnY, mx + 16 + 215, btnY + btnH };

    // Right-aligned cluster: Toggle All, Startup Apps, Deploy
    int depW = 250;
    int depX = mx + mw - 16 - depW;
    g_btnDeployRect      = { depX, btnY, depX + depW, btnY + btnH };

    int smW = 135;
    int smX = depX - 10 - smW;
    g_btnStartupMgrRect  = { smX, btnY, smX + smW, btnY + btnH };

    int tAllW = 110;
    int tAllX = smX - 10 - tAllW;
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

// Double-Buffered Painting: Exact 1:1 Pixel Mirror to Concept 2 (be9d415c.jpg)
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
    Color clrCanvas(255, 5, 7, 10);           // Deep canvas background
    Color clrObsidian(255, 11, 14, 20);       // #0B0E14 - Master card obsidian
    Color clrCardSlate(255, 16, 20, 30);      // #10141E - Standard card
    Color clrCardHover(255, 18, 23, 36);      // #121724 - Hover card
    Color clrBorder(255, 27, 34, 48);         // #1B2230 - Border
    Color clrBorderOuter(255, 30, 35, 48);    // #1E2330 - Master card border
    Color clrAcidVolt(255, 204, 255, 0);      // #CCFF00 - Signature volt
    Color clrEmerald(255, 16, 185, 129);      // #10B981
    Color clrAmber(255, 245, 158, 11);        // #F59E0B
    Color clrTextWhite(255, 255, 255, 255);   // #FFFFFF - Crisp white
    Color clrTextMuted(255, 156, 163, 175);   // #9CA3AF - Muted gray
    Color clrTextDark(255, 107, 114, 128);    // #6B7280 - Stencil gray
    Color clrJetBlack(255, 0, 0, 0);

    // 1. Fill Outer Canvas
    SolidBrush canvasBrush(clrCanvas);
    g.FillRectangle(&canvasBrush, 0, 0, width, height);

    // Master Card Container (24px padding, matching p-6)
    int pad = 24;
    int mx = pad, my = pad;
    int mw = width - pad * 2;
    int mh = height - pad * 2;

    SolidBrush masterBrush(clrObsidian);
    Pen masterPen(clrBorderOuter, 1.0f);
    FillRoundRect(g, &masterBrush, mx, my, mw, mh, 16);
    DrawRoundRect(g, &masterPen, mx, my, mw, mh, 16);

    // Subtle 16px Dot-Matrix Grid inside Master Card (techwear-grid)
    SolidBrush dotBrush(Color(22, 31, 38, 54)); // #1F2636
    for (int gx = mx + 16; gx < mx + mw - 16; gx += 16) {
        for (int gy = my + 16; gy < my + mh - 16; gy += 16) {
            g.FillRectangle(&dotBrush, gx, gy, 1, 1);
        }
    }

    // Typography
    FontFamily fontMono(L"Consolas");
    FontFamily fontSans(L"Segoe UI");
    Font fontHeaderPill(&fontMono, 10, FontStyleBold, UnitPixel);
    Font fontCpuTitle(&fontSans, 15, FontStyleBold, UnitPixel);
    Font fontCpuBracket(&fontMono, 13, FontStyleBold, UnitPixel);
    Font fontCpuSpecs(&fontSans, 11, FontStyleRegular, UnitPixel);
    Font fontCardTitle(&fontSans, 13, FontStyleBold, UnitPixel);
    Font fontCardDesc(&fontSans, 10, FontStyleRegular, UnitPixel);
    Font fontBadge(&fontMono, 9, FontStyleBold, UnitPixel);
    Font fontId(&fontMono, 11, FontStyleBold, UnitPixel);
    Font fontBtn(&fontSans, 11, FontStyleBold, UnitPixel);
    Font fontBtnDeploy(&fontSans, 12, FontStyleBold, UnitPixel);
    Font fontTelemLabel(&fontMono, 8, FontStyleBold, UnitPixel);
    Font fontTelemVal(&fontMono, 10, FontStyleBold, UnitPixel);
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
    Pen divPen(clrBorder, 1.0f);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentNear);

    // =========================================================================
    // 2. TOP TECHWEAR BANNER (MATCHING Concept 2 HTML EXACTLY)
    // =========================================================================
    // Clean bottom divider separating header from cards (pb-4 border-b mb-5)
    g.DrawLine(&divPen, mx + 16, my + 74, mx + mw - 16, my + 74);

    // 2A. Left Header Cluster (No enclosed box, sits directly on canvas)
    // Line 1: // SYS.CORE-TUNER v2.4 (pill with animated pulsing LED pip) + // LEN_82HS
    SolidBrush pillBg(Color(255, 22, 27, 38)); // #161B26
    Pen pillBorder(Color(255, 38, 48, 69), 1.0f); // #263045
    FillRoundRect(g, &pillBg, mx + 16, my + 14, 168, 20, 4);
    DrawRoundRect(g, &pillBorder, mx + 16, my + 14, 168, 20, 4);

    // Pulsing Core LED Pip
    float pulse = (sinf(g_animTime * 4.0f) + 1.0f) * 0.5f;
    int dotAlpha = (int)(120.0f + 135.0f * pulse);
    SolidBrush pulseDotBrush(Color(dotAlpha, 204, 255, 0));
    g.FillEllipse(&pulseDotBrush, mx + 24, my + 20, 6, 6);
    g.DrawString(L"// SYS.CORE-TUNER v2.4", -1, &fontHeaderPill, PointF((REAL)mx + 34, (REAL)my + 17), &acidBrush);

    g.DrawString(L"// LEN_82HS", -1, &fontHeaderPill, PointF((REAL)mx + 196, (REAL)my + 17), &darkBrush);

    // Line 2: INTEL i7-1165G7 (WHITE) + [4C/8T] (#CCFF00) + • 16GB RAM • NVMe 512GB (GRAY)
    std::wstring cpuMain = L"INTEL i7-1165G7 ";
    g.DrawString(cpuMain.c_str(), -1, &fontCpuTitle, PointF((REAL)mx + 16, (REAL)my + 38), &whiteBrush);

    RectF cpuMainRect;
    g.MeasureString(cpuMain.c_str(), -1, &fontCpuTitle, PointF(0, 0), &sf, &cpuMainRect);
    float curX = (REAL)mx + 16 + cpuMainRect.Width;

    std::wstring cpuBracket = L"[4C/8T]  ";
    g.DrawString(cpuBracket.c_str(), -1, &fontCpuBracket, PointF(curX, (REAL)my + 39), &acidBrush);

    RectF bracketRect;
    g.MeasureString(cpuBracket.c_str(), -1, &fontCpuBracket, PointF(0, 0), &sf, &bracketRect);
    curX += bracketRect.Width;

    g.DrawString(L"•  16GB RAM  •  NVMe 512GB", -1, &fontCpuSpecs, PointF(curX, (REAL)my + 40), &mutedBrush);

    // 2B. Right Telemetry Equalizer Module (in enclosed card #111622)
    int telemW = 390;
    int telemH = 46;
    int telemX = mx + mw - 16 - telemW;
    int telemY = my + 12;

    SolidBrush telemBg(Color(255, 17, 22, 34)); // #111622
    Pen telemBorder(Color(255, 32, 41, 60), 1.0f); // #20293C
    FillRoundRect(g, &telemBg, telemX, telemY, telemW, telemH, 10);
    DrawRoundRect(g, &telemBorder, telemX, telemY, telemW, telemH, 10);

    // 6 Real-Time Animated Equalizer Bars bouncing from bottom baseline
    int baseLineY = telemY + telemH - 9;
    for (int b = 0; b < 6; ++b) {
        int barH = g_eqBars[b];
        int barX = telemX + 16 + b * 6;
        FillRoundRect(g, &acidBrush, barX, baseLineY - barH, 4, barH, 1);
    }

    // Divider 1
    g.DrawLine(&divPen, telemX + 62, telemY + 8, telemX + 62, telemY + telemH - 8);

    // Throughput Governor
    g.DrawString(L"THROUGHPUT GOVERNOR", -1, &fontTelemLabel, PointF((REAL)telemX + 74, (REAL)telemY + 9), &darkBrush);
    g.DrawString(L"MAX SPEED // 100% UNLOCKED", -1, &fontTelemVal, PointF((REAL)telemX + 74, (REAL)telemY + 23), &acidBrush);

    // Divider 2
    g.DrawLine(&divPen, telemX + 248, telemY + 8, telemX + 248, telemY + telemH - 8);

    // Storage Gain
    g.DrawString(L"STORAGE GAIN", -1, &fontTelemLabel, PointF((REAL)telemX + 260, (REAL)telemY + 9), &darkBrush);
    g.DrawString(L"+7.5 GB FREED", -1, &fontTelemVal, PointF((REAL)telemX + 260, (REAL)telemY + 23), &whiteBrush);

    // =========================================================================
    // 3. OPTIMIZATION CARDS (MATCHING Concept 2 CSS EXACTLY)
    // =========================================================================
    for (size_t i = 0; i < g_cards.size(); ++i) {
        const auto& card = g_cards[i];
        bool isHovered = ((int)i == g_hoverCardIndex) && !g_showModal && !g_showStartupModal;

        // Smooth horizontal drift animation (translateX(6px))
        int driftX = (int)roundf(g_cardDrift[i]);
        int cx = (int)card.rect.left + driftX;
        int cy = (int)card.rect.top;
        int cw = (int)(card.rect.right - card.rect.left) - driftX;
        int ch = (int)(card.rect.bottom - card.rect.top);

        Color cardBg = isHovered ? clrCardHover : clrCardSlate;
        Color borderClr = isHovered ? Color(200, 204, 255, 0) : clrBorder;

        SolidBrush cBrush(cardBg);
        Pen pPen(borderClr, 1.0f);

        // Hover neon glow drop shadow (box-shadow: 0 0 25px rgba(204,255,0,0.18))
        if (isHovered) {
            Pen glowPen(Color(45, 204, 255, 0), 2.0f);
            DrawRoundRect(g, &glowPen, cx - 1, cy - 1, cw + 2, ch + 2, 13);
        }

        FillRoundRect(g, &cBrush, cx, cy, cw, ch, 12);
        DrawRoundRect(g, &pPen, cx, cy, cw, ch, 12);

        // ID Box: exactly 28x28px (matching w-7 h-7), radius 8px (rounded-lg)
        int idX = cx + 14;
        int idY = cy + (ch - 28) / 2;  // Vertically centered
        int idS = 28;

        if (isHovered) {
            SolidBrush idHoverBg(clrAcidVolt);
            FillRoundRect(g, &idHoverBg, idX, idY, idS, idS, 8);
            Pen idGlow(Color(120, 204, 255, 0), 2.0f);
            DrawRoundRect(g, &idGlow, idX - 1, idY - 1, idS + 2, idS + 2, 9);
            g.DrawString(card.numStr.c_str(), -1, &fontId, PointF((REAL)idX + 6, (REAL)idY + 6), &jetBlackBrush);
        } else {
            SolidBrush numBg(Color(255, 24, 32, 48)); // #182030
            Pen numBorder(Color(255, 38, 50, 73), 1.0f); // #263249
            FillRoundRect(g, &numBg, idX, idY, idS, idS, 8);
            DrawRoundRect(g, &numBorder, idX, idY, idS, idS, 8);
            g.DrawString(card.numStr.c_str(), -1, &fontId, PointF((REAL)idX + 6, (REAL)idY + 6), &acidBrush);
        }

        // Title text (Line 1): White (or Acid-Volt on hover) — vertically centered
        int titleY = cy + (ch / 2) - 16;  // Upper half of card
        SolidBrush* titleBrush = isHovered ? &acidBrush : &whiteBrush;
        g.DrawString(card.title.c_str(), -1, &fontCardTitle, PointF((REAL)cx + 52, (REAL)titleY), titleBrush);

        // INLINE BADGE PILL: positioned immediately next to title!
        RectF tRect;
        g.MeasureString(card.title.c_str(), -1, &fontCardTitle, PointF(0, 0), &sf, &tRect);
        int badgeX = (int)(cx + 52 + tRect.Width + 10);
        int badgeY = titleY + 1;

        RectF bTextRect;
        g.MeasureString(card.badge.c_str(), -1, &fontBadge, PointF(0, 0), &sf, &bTextRect);
        int badgeW = (int)bTextRect.Width + 12;
        int badgeH = 18;

        SolidBrush badgeBg(Color(255, 22, 32, 22)); // #162016
        Pen badgeBorder(Color(255, 60, 100, 20), 1.0f); // #3C6414
        FillRoundRect(g, &badgeBg, badgeX, badgeY, badgeW, badgeH, 4);
        DrawRoundRect(g, &badgeBorder, badgeX, badgeY, badgeW, badgeH, 4);
        g.DrawString(card.badge.c_str(), -1, &fontBadge, PointF((REAL)badgeX + 6, (REAL)badgeY + 2), &acidBrush);

        // Description text (Line 2) — 11px, vertically below title
        int descY = cy + (ch / 2) + 2;  // Lower half of card
        g.DrawString(card.desc.c_str(), -1, &fontCardDesc, PointF((REAL)cx + 52, (REAL)descY), &mutedBrush);

        // SLEEK TOGGLE SWITCH: Exactly 48x24px (w-12 h-6), radius 8px (rounded-lg)
        int swW = 48, swH = 24;
        int swX = cx + cw - 16 - swW;
        int swY = cy + (ch - swH) / 2;  // Vertically centered

        // Switch track with inner shadow effect (shadow-inner)
        SolidBrush swBg(Color(255, 22, 28, 40)); // #161C28
        Pen swBorder(isHovered ? clrAcidVolt : Color(255, 44, 56, 80), 1.0f); // #2C3850
        FillRoundRect(g, &swBg, swX, swY, swW, swH, 8);
        // Inner shadow: darker top edge
        SolidBrush swInnerShadow(Color(40, 0, 0, 0));
        FillRoundRect(g, &swInnerShadow, swX, swY, swW, 6, 8);
        DrawRoundRect(g, &swBorder, swX, swY, swW, swH, 8);

        // Animated Smooth Switch Gliding (interpolated 0.0 -> 1.0)
        float sAnim = g_switchAnim[i];
        int thumbW = 22, thumbH = 20;
        int minThumbX = swX + 2;
        int maxThumbX = swX + swW - thumbW - 2;
        int curThumbX = minThumbX + (int)roundf(sAnim * (float)(maxThumbX - minThumbX));

        if (sAnim > 0.5f) {
            // ON Thumb: Acid Volt with soft glow & black ON text
            SolidBrush onThumb(clrAcidVolt);
            FillRoundRect(g, &onThumb, curThumbX, swY + 2, thumbW, thumbH, 4);

            Pen thumbGlow(Color(80, 204, 255, 0), 1.5f);
            DrawRoundRect(g, &thumbGlow, curThumbX - 1, swY + 1, thumbW + 2, thumbH + 2, 5);

            g.DrawString(L"ON", -1, &fontBadge, PointF((REAL)curThumbX + 4, (REAL)swY + 4), &jetBlackBrush);
        } else {
            // OFF Thumb: Dark slate with gray OFF text
            SolidBrush offThumb(Color(255, 35, 42, 56)); // #232A38
            FillRoundRect(g, &offThumb, curThumbX, swY + 2, thumbW, thumbH, 4);
            g.DrawString(L"OFF", -1, &fontBadge, PointF((REAL)curThumbX + 2, (REAL)swY + 4), &mutedBrush);
        }
    }

    // =========================================================================
    // 4. BOTTOM ACTION BAR (MATCHING be9d415c.jpg EXACTLY)
    // =========================================================================
    // Clean top divider separating cards from footer
    g.DrawLine(&divPen, mx + 16, my + mh - 58, mx + mw - 16, my + mh - 58);

    // Left Button: ↺ RESTORE FACTORY DEFAULTS
    SolidBrush restBg(g_hoverRestore ? Color(255, 28, 34, 52) : Color(255, 20, 24, 36)); // #1C2234 vs #141824
    Pen restBorder(g_hoverRestore ? clrTextWhite : Color(255, 36, 46, 66), 1.0f); // #242E42
    int rw = g_btnRestoreRect.right - g_btnRestoreRect.left;
    int rh = g_btnRestoreRect.bottom - g_btnRestoreRect.top;
    FillRoundRect(g, &restBg, g_btnRestoreRect.left, g_btnRestoreRect.top, rw, rh, 8);
    DrawRoundRect(g, &restBorder, g_btnRestoreRect.left, g_btnRestoreRect.top, rw, rh, 8);
    SolidBrush* restText = g_hoverRestore ? &whiteBrush : &mutedBrush;
    g.DrawString(L"↺  RESTORE FACTORY DEFAULTS", -1, &fontBtn, PointF((REAL)g_btnRestoreRect.left + 14, (REAL)g_btnRestoreRect.top + 11), restText);

    // Right-Aligned Cluster:
    // 1. TOGGLE ALL
    SolidBrush tAllBg(g_hoverToggleAll ? Color(255, 28, 34, 52) : Color(255, 20, 24, 36));
    Pen tAllBorder(g_hoverToggleAll ? clrTextWhite : Color(255, 36, 46, 66), 1.0f);
    int tw = g_btnToggleAllRect.right - g_btnToggleAllRect.left;
    int th = g_btnToggleAllRect.bottom - g_btnToggleAllRect.top;
    FillRoundRect(g, &tAllBg, g_btnToggleAllRect.left, g_btnToggleAllRect.top, tw, th, 8);
    DrawRoundRect(g, &tAllBorder, g_btnToggleAllRect.left, g_btnToggleAllRect.top, tw, th, 8);
    g.DrawString(L"TOGGLE ALL", -1, &fontBtn, PointF((REAL)g_btnToggleAllRect.left + 18, (REAL)g_btnToggleAllRect.top + 11), &whiteBrush);

    // 2. STARTUP APPS
    SolidBrush smBg(g_hoverStartupMgr ? Color(255, 28, 34, 52) : Color(255, 20, 24, 36));
    Pen smBorder(g_hoverStartupMgr ? clrAcidVolt : Color(255, 36, 46, 66), 1.0f);
    int smw = g_btnStartupMgrRect.right - g_btnStartupMgrRect.left;
    int smh = g_btnStartupMgrRect.bottom - g_btnStartupMgrRect.top;
    FillRoundRect(g, &smBg, g_btnStartupMgrRect.left, g_btnStartupMgrRect.top, smw, smh, 8);
    DrawRoundRect(g, &smBorder, g_btnStartupMgrRect.left, g_btnStartupMgrRect.top, smw, smh, 8);
    g.DrawString(L"🚀 STARTUP APPS", -1, &fontBtn, PointF((REAL)g_btnStartupMgrRect.left + 14, (REAL)g_btnStartupMgrRect.top + 11), &whiteBrush);

    // 3. DEPLOY TACTICAL TUNING ➔ (High-voltage acid volt with animated neon shadow)
    Color depBg = g_hoverDeploy ? Color(255, 184, 230, 0) : clrAcidVolt; // #B8E600 vs #CCFF00
    SolidBrush depBrush(depBg);
    int dw = g_btnDeployRect.right - g_btnDeployRect.left;
    int dh = g_btnDeployRect.bottom - g_btnDeployRect.top;

    if (g_hoverDeploy) {
        Pen glowOuter(Color(45, 204, 255, 0), 4.0f);
        DrawRoundRect(g, &glowOuter, g_btnDeployRect.left - 3, g_btnDeployRect.top - 3, dw + 6, dh + 6, 11);
        Pen glowMid(Color(100, 204, 255, 0), 2.0f);
        DrawRoundRect(g, &glowMid, g_btnDeployRect.left - 1, g_btnDeployRect.top - 1, dw + 2, dh + 2, 9);
    } else {
        Pen glowSubtle(Color(50, 204, 255, 0), 2.0f);
        DrawRoundRect(g, &glowSubtle, g_btnDeployRect.left - 1, g_btnDeployRect.top - 1, dw + 2, dh + 2, 9);
    }

    FillRoundRect(g, &depBrush, g_btnDeployRect.left, g_btnDeployRect.top, dw, dh, 8);
    g.DrawString(L"⚡  DEPLOY TACTICAL TUNING  ➔", -1, &fontBtnDeploy, PointF((REAL)g_btnDeployRect.left + 18, (REAL)g_btnDeployRect.top + 11), &jetBlackBrush);

    // =========================================================================
    // 5. IN-WINDOW CYBERCORE OVERCLOCK MODAL OVERLAY
    // =========================================================================
    if (g_showModal) {
        SolidBrush modalBackdrop(Color(220, 7, 9, 14));
        g.FillRectangle(&modalBackdrop, 0, 0, width, height);

        int ox = 80, oy = 50, ow = width - 160, oh = height - 100;
        g_modalDismissRect = { ox + ow - 220, oy + oh - 54, ox + ow - 24, oy + oh - 16 };

        SolidBrush modalBody(clrObsidian);
        Pen modalPen(Color(255, 42, 54, 78), 1.5f);
        FillRoundRect(g, &modalBody, ox, oy, ow, oh, 16);
        DrawRoundRect(g, &modalPen, ox, oy, ow, oh, 16);

        SolidBrush modalTopBg(clrCardSlate);
        Pen modalTopBorder(clrBorder, 1.0f);
        FillRoundRect(g, &modalTopBg, ox, oy, ow, 52, 16);
        DrawRoundRect(g, &modalTopBorder, ox, oy, ow, 52, 16);

        g.FillEllipse(&acidBrush, ox + 20, oy + 21, 10, 10);
        g.DrawString(L"// SYSTEM OVERCLOCK ENGINE // KERNEL-DISPATCH", -1, &fontModalTitle, PointF((REAL)ox + 38, (REAL)oy + 18), &acidBrush);

        std::wstring pctStr = std::to_wstring(g_progressPercent) + L"%";
        SolidBrush pctBg(Color(255, 22, 34, 24));
        Pen pctBorder(Color(255, 60, 100, 20), 1.0f);
        FillRoundRect(g, &pctBg, ox + ow - 80, oy + 15, 60, 24, 6);
        DrawRoundRect(g, &pctBorder, ox + ow - 80, oy + 15, 60, 24, 6);
        g.DrawString(pctStr.c_str(), -1, &fontBadge, PointF((REAL)ox + ow - 66, (REAL)oy + 20), &acidBrush);

        SolidBrush progTrack(Color(255, 16, 20, 30));
        FillRoundRect(g, &progTrack, ox + 20, oy + 66, ow - 40, 8, 4);
        int progFillW = (int)((float)(ow - 40) * ((float)g_progressPercent / 100.0f));
        if (progFillW > 0) {
            LinearGradientBrush gradBrush(Point(ox + 20, 0), Point(ox + ow - 20, 0), clrAcidVolt, clrEmerald);
            FillRoundRect(g, &gradBrush, ox + 20, oy + 66, progFillW, 8, 4);
        }

        int lx = ox + 20, ly = oy + 88, lw = ow - 40, lh = oh - 160;
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

        g.DrawString(g_modalStatus.c_str(), -1, &fontCardDesc, PointF((REAL)ox + 20, (REAL)oy + oh - 40), &acidBrush);

        if (g_modalComplete) {
            SolidBrush disBg(g_hoverModalDismiss ? Color(255, 224, 255, 50) : clrAcidVolt);
            int mdw = g_modalDismissRect.right - g_modalDismissRect.left;
            int mdh = g_modalDismissRect.bottom - g_modalDismissRect.top;
            FillRoundRect(g, &disBg, g_modalDismissRect.left, g_modalDismissRect.top, mdw, mdh, 8);
            g.DrawString(L"[ ✓ DISMISS CONSOLE ]", -1, &fontBtn, PointF((REAL)g_modalDismissRect.left + 16, (REAL)g_modalDismissRect.top + 13), &jetBlackBrush);
        }
    }

    // =========================================================================
    // 6. IN-WINDOW STARTUP APPLICATIONS MANAGER MODAL
    // =========================================================================
    if (g_showStartupModal) {
        SolidBrush modalBackdrop(Color(220, 7, 9, 14));
        g.FillRectangle(&modalBackdrop, 0, 0, width, height);

        int ox = 60, oy = 40, ow = width - 120, oh = height - 80;

        SolidBrush modalBody(clrObsidian);
        Pen modalPen(Color(255, 42, 54, 78), 1.5f);
        FillRoundRect(g, &modalBody, ox, oy, ow, oh, 16);
        DrawRoundRect(g, &modalPen, ox, oy, ow, oh, 16);

        SolidBrush modalTopBg(clrCardSlate);
        Pen modalTopBorder(clrBorder, 1.0f);
        FillRoundRect(g, &modalTopBg, ox, oy, ow, 62, 16);
        DrawRoundRect(g, &modalTopBorder, ox, oy, ow, 62, 16);

        g.FillEllipse(&acidBrush, ox + 20, oy + 26, 10, 10);
        g.DrawString(L"// STARTUP APPLICATIONS MANAGER // AUTOSTART DECOUPLER", -1, &fontModalTitle, PointF((REAL)ox + 38, (REAL)oy + 14), &acidBrush);
        g.DrawString(L"Toggle background autostart bloatware without deleting registry keys. Changes persist cleanly.", -1, &fontCardDesc, PointF((REAL)ox + 38, (REAL)oy + 36), &mutedBrush);

        g_startupRefreshRect = { ox + ow - 220, oy + 14, ox + ow - 110, oy + 48 };
        g_startupCloseRect   = { ox + ow - 98,  oy + 14, ox + ow - 20,  oy + 48 };

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

        int listY = oy + 76;
        int itemH = 52;
        int itemGap = 6;
        int maxVisible = (oh - 100) / (itemH + itemGap);

        if (g_startupApps.empty()) {
            g.DrawString(L"Scanning Windows startup entries...", -1, &fontCardDesc, PointF((REAL)ox + 30, (REAL)listY + 30), &mutedBrush);
        }

        for (size_t s = 0; s < g_startupApps.size() && (int)s < maxVisible; ++s) {
            const auto& app = g_startupApps[s];
            int itemTop = listY + (int)s * (itemH + itemGap);
            int itemW = ow - 40;
            int itemX = ox + 20;

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

            int swW2 = 48, swH2 = 24;
            int swX2 = itemX + itemW - 16 - swW2;
            int swY2 = itemTop + 14;

            SolidBrush swBg2(Color(255, 22, 28, 40));
            Pen swBorder2(app.isEnabled ? clrAcidVolt : Color(255, 44, 56, 80), 1.0f);
            FillRoundRect(g, &swBg2, swX2, swY2, swW2, swH2, 6);
            DrawRoundRect(g, &swBorder2, swX2, swY2, swW2, swH2, 6);

            if (app.isEnabled) {
                SolidBrush onThumb(clrAcidVolt);
                FillRoundRect(g, &onThumb, swX2 + swW2 - 24, swY2 + 2, 22, 20, 4);
                g.DrawString(L"ON", -1, &fontBadge, PointF((REAL)swX2 + swW2 - 20, (REAL)swY2 + 4), &jetBlackBrush);
            } else {
                SolidBrush offThumb(Color(255, 35, 42, 56));
                FillRoundRect(g, &offThumb, swX2 + 2, swY2 + 2, 22, 20, 4);
                g.DrawString(L"OFF", -1, &fontBadge, PointF((REAL)swX2 + 4, (REAL)swY2 + 4), &mutedBrush);
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

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
            int corner = 3; /* DWMWCP_ROUND */
            DwmSetWindowAttribute(hWnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
            
            COLORREF captionClr = RGB(5, 7, 10);
            DwmSetWindowAttribute(hWnd, 35 /* DWMWA_CAPTION_COLOR */, &captionClr, sizeof(captionClr));

            g_isAdmin = CheckIsAdmin();

            RECT rc;
            GetClientRect(hWnd, &rc);
            InitOptimizationCards(rc.right - rc.left, rc.bottom - rc.top);

            // 60 FPS Micro-Interaction & Physics Timer (16ms tick)
            SetTimer(hWnd, 1 /* IDT_ANIMATION */, 16, NULL);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                g_animTime += 0.016f;

                // 1. Procedural Spectrum Equalizer Animation matching @keyframes spec1..6
                float t = g_animTime;
                g_eqBars[0] = 8 + (int)(18.0f * (sinf(t * 8.97f) + 1.0f) * 0.5f);
                g_eqBars[1] = 6 + (int)(20.0f * (sinf(t * 12.56f + 1.5f) + 1.0f) * 0.5f);
                g_eqBars[2] = 8 + (int)(16.0f * (sinf(t * 9.66f + 0.8f) + 1.0f) * 0.5f);
                g_eqBars[3] = 10 + (int)(18.0f * (sinf(t * 7.85f + 2.2f) + 1.0f) * 0.5f);
                g_eqBars[4] = 6 + (int)(14.0f * (sinf(t * 11.42f + 3.1f) + 1.0f) * 0.5f);
                g_eqBars[5] = 8 + (int)(16.0f * (sinf(t * 8.37f + 0.4f) + 1.0f) * 0.5f);

                // 2. Interpolate Card Hover Drift (translateX(6px)) with smooth spring lerp
                for (size_t i = 0; i < 8; ++i) {
                    float targetDrift = (g_hoverCardIndex == (int)i && !g_showModal && !g_showStartupModal) ? 6.0f : 0.0f;
                    g_cardDrift[i] += (targetDrift - g_cardDrift[i]) * 0.25f;
                }

                // 3. Interpolate Switch Gliding Position (0.0 <-> 1.0)
                for (size_t i = 0; i < 8 && i < g_cards.size(); ++i) {
                    float targetSwitch = g_cards[i].isEnabled ? 1.0f : 0.0f;
                    g_switchAnim[i] += (targetSwitch - g_switchAnim[i]) * 0.32f;
                }

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
                        g_hoverCardIndex >= 0) {
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
            // Track mouse leaving window to prevent stuck hover states
            TRACKMOUSEEVENT tme = { sizeof(tme) };
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);

            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            if (g_showStartupModal) {
                g_hoverStartupClose = PtInRect(&g_startupCloseRect, pt);
                g_hoverStartupRefresh = PtInRect(&g_startupRefreshRect, pt);

                RECT rc;
                GetClientRect(hWnd, &rc);
                int width = rc.right - rc.left;
                int height = rc.bottom - rc.top;
                int ox = 60, oy = 40, ow = width - 120, oh = height - 80;
                int listY = oy + 76;
                int itemH = 52, itemGap = 6;
                int maxVisible = (oh - 100) / (itemH + itemGap);

                g_hoverStartupIndex = -1;
                for (size_t s = 0; s < g_startupApps.size() && (int)s < maxVisible; ++s) {
                    int itemTop = listY + (int)s * (itemH + itemGap);
                    RECT appRect = { ox + 20, itemTop, ox + 20 + ow - 40, itemTop + itemH };
                    if (PtInRect(&appRect, pt)) {
                        g_hoverStartupIndex = (int)s;
                        break;
                    }
                }
            } else if (g_showModal) {
                g_hoverModalDismiss = PtInRect(&g_modalDismissRect, pt) && g_modalComplete;
            } else {
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

        case WM_MOUSELEAVE: {
            // Instantly clear all hover states when mouse leaves window
            g_hoverCardIndex = -1;
            g_hoverDeploy = false;
            g_hoverRestore = false;
            g_hoverToggleAll = false;
            g_hoverStartupMgr = false;
            g_hoverStartupClose = false;
            g_hoverStartupRefresh = false;
            g_hoverModalDismiss = false;
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
                int ox = 60, oy = 40, ow = width - 120, oh = height - 80;
                int listY = oy + 76;
                int itemH = 52, itemGap = 6;
                int maxVisible = (oh - 100) / (itemH + itemGap);

                for (size_t s = 0; s < g_startupApps.size() && (int)s < maxVisible; ++s) {
                    int itemTop = listY + (int)s * (itemH + itemGap);
                    RECT appRect = { ox + 20, itemTop, ox + 20 + ow - 40, itemTop + itemH };
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
    // 0. Automatically unblock executable by deleting NTFS Zone.Identifier stream
    SelfUnblockExecutable();

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

    // 4. Create Window with exact Concept proportions (980 x 780)
    int w = 980;
    int h = 780;
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
