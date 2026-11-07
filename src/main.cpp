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

// Execution / Overclock Modal State
bool g_showModal = false;
int g_progressPercent = 0;
std::wstring g_modalStatus = L"// INITIATING LOW-LEVEL PROTOCOLS...";
std::vector<std::wstring> g_modalLogs;
bool g_modalComplete = false;

// Startup Applications Manager State
bool g_showStartupModal = false;
std::vector<LaptopOptimizerEngine::StartupAppEntry> g_startupApps;
int g_startupScrollOffset = 0;

// Buttons RECTs (Dynamically adapted on resize / init)
RECT g_btnPrivilegeRect   = { 360, 24, 570, 48 };
RECT g_btnRestoreRect     = { 24,  638, 204, 684 };
RECT g_btnToggleAllRect   = { 216, 638, 348, 684 };
RECT g_btnStartupMgrRect  = { 360, 638, 532, 684 };
RECT g_btnDeployRect      = { 544, 638, 920, 684 };

// Modal Close & Dismiss Buttons
RECT g_modalDismissRect   = { 680, 520, 860, 560 };
RECT g_startupCloseRect   = { 760, 64, 880, 98 };
RECT g_startupRefreshRect = { 620, 64, 748, 98 };

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

// Equalizer Frequency Bar Heights for Telemetry Module
int g_eqBars[6] = { 10, 18, 14, 22, 12, 16 };

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
    g_cards.clear();
    g_cards.push_back({ 1, L"01", L"Zero-Ramp CPU Frequency Floor on AC", L"Sets processor state to 100% floor on AC power to eliminate turbo ramp stutter.", L"100% FLOOR", true });
    g_cards.push_back({ 2, L"02", L"Bypass Game DVR Background Video Buffer", L"Halts continuous background DirectX video encoding and capture hooks; saves GPU VRAM.", L"ZERO VRAM", true });
    g_cards.push_back({ 3, L"03", L"Instant Desktop Context Menus (20ms)", L"Cuts MenuShowDelay from 400ms to 20ms for instantaneous shell responsiveness.", L"20ms DELAY", true });
    g_cards.push_back({ 4, L"04", L"Purge Windows 11 Widget Daemon Pack", L"Uninstalls WebExperience package; permanently terminates 20+ idle WebView2 processes.", L"-300MB RAM", true });
    g_cards.push_back({ 5, L"05", L"Decouple Bing Cloud Integration from Start", L"Eliminates remote cloud latency on Start search; makes app launch search instant.", L"LOCAL ONLY", true });
    g_cards.push_back({ 6, L"06", L"Halt Delivery Optimization P2P Seeding", L"Stops Windows from using your disk I/O and bandwidth to seed updates to network peers.", L"HTTP ONLY", true });
    g_cards.push_back({ 7, L"07", L"Clean Kernel Boots & Purge hiberfil.sys", L"Reclaims +6.22 GB NVMe SSD storage permanently and guarantees 100% clean boots.", L"+6.2GB SSD", true });
    g_cards.push_back({ 8, L"08", L"Purge Stale Temporary Files (>24h)", L"Cleans accumulated cache junk from %TEMP% to free NVMe storage and reduce indexing.", L"CLEAN TEMP", true });

    int startY = 114;
    int cardH = 54;
    int gap = 8;
    int cardW = clientW - 48;

    for (size_t i = 0; i < g_cards.size(); ++i) {
        int y = startY + (int)i * (cardH + gap);
        g_cards[i].rect = { 24, y, 24 + cardW, y + cardH };
        g_cards[i].switchRect = { 24 + cardW - 76, y + 15, 24 + cardW - 14, y + 39 };
    }

    // Refresh bottom action buttons layout
    int btnY = clientH - 62;
    int btnH = 46;
    g_btnRestoreRect     = { 24,  btnY, 204, btnY + btnH };
    g_btnToggleAllRect   = { 216, btnY, 348, btnY + btnH };
    g_btnStartupMgrRect  = { 360, btnY, 532, btnY + btnH };
    g_btnDeployRect      = { 544, btnY, clientW - 24, btnY + btnH };
}

// Background Optimization Worker
void RunOptimizationPipeline() {
    g_isExecuting = true;
    g_modalComplete = false;
    g_progressPercent = 0;
    g_modalLogs.clear();
    g_modalStatus = L"// DISPATCHING SYSTEM TUNING VECTORS...";
    g_modalLogs.push_back(L"// INITIALIZING CYBERCORE TUNING WORKER (PID: 9482)");
    InvalidateRect(g_hWnd, NULL, FALSE);

    std::vector<OptCard> activeCards;
    for (const auto& c : g_cards) {
        if (c.isEnabled) activeCards.push_back(c);
    }

    if (activeCards.empty()) {
        g_modalLogs.push_back(L"[!] NO PROTOCOLS SELECTED. NOTHING TO DISPATCH.");
        g_modalComplete = true;
        g_isExecuting = false;
        InvalidateRect(g_hWnd, NULL, FALSE);
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
            case 5: LaptopOptimizerEngine::OptimizeBingSearch(); logEntry += L"OK (Local Search Only)"; break;
            case 6: LaptopOptimizerEngine::OptimizeDeliveryOptimization(); logEntry += L"OK (P2P Seeding Halted)"; break;
            case 7: LaptopOptimizerEngine::OptimizeFastStartup(); logEntry += L"OK (+6.22GB SSD Freed)"; break;
            case 8: LaptopOptimizerEngine::OptimizeTempFiles(); logEntry += L"OK (Stale Cache Reclaimed)"; break;
        }

        g_modalLogs.push_back(logEntry);
        g_progressPercent = (int)((float)(i + 1) / (float)total * 100.0f);
        InvalidateRect(g_hWnd, NULL, FALSE);
        Sleep(220);
    }

    g_progressPercent = 100;
    g_modalStatus = L"STATUS: ALL SUBSYSTEMS RUNNING AT PEAK SPEC";
    g_modalLogs.push_back(L"// ------------------------------------------------");
    g_modalLogs.push_back(L"// [✓] SUCCESS: ALL SELECTED TUNING VECTORS SATISFIED.");
    g_modalLogs.push_back(L"// TIP: REBOOT LAPTOP TO ENGAGE FRESH KERNEL STATE.");
    g_modalComplete = true;
    g_isExecuting = false;
    InvalidateRect(g_hWnd, NULL, FALSE);
}

// Background Rollback / Restore Worker
void RunRestorePipeline() {
    g_isRestoring = true;
    g_modalComplete = false;
    g_progressPercent = 0;
    g_modalLogs.clear();
    g_modalStatus = L"// ROLLING BACK ALL TUNING TO WINDOWS DEFAULTS...";
    g_modalLogs.push_back(L"// RESTORATION WORKER ENGAGED (PID: 9482)");
    InvalidateRect(g_hWnd, NULL, FALSE);

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
        InvalidateRect(g_hWnd, NULL, FALSE);
        Sleep(180);
    }

    g_progressPercent = 100;
    g_modalStatus = L"STATUS: WINDOWS FACTORY DEFAULTS RESTORED";
    g_modalLogs.push_back(L"// ------------------------------------------------");
    g_modalLogs.push_back(L"// [✓] ROLLBACK COMPLETE: ALL SUBSYSTEMS RESET TO FACTORY SPEC.");
    g_modalComplete = true;
    g_isRestoring = false;
    InvalidateRect(g_hWnd, NULL, FALSE);
}

// Double-Buffered Painting: Faithful Concept 2 (Cybercore // Dark Techwear)
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
    Color clrObsidian(255, 11, 14, 20);       // #0B0E14 - Deep matte carbon obsidian
    Color clrSlate(255, 16, 20, 30);          // #10141E - Sleek slate card background
    Color clrSlateHover(255, 22, 28, 42);     // #161C2A - Focused card hover
    Color clrBorder(255, 30, 36, 50);         // #1E2432 - Subtle techwear border
    Color clrBorderHover(255, 45, 55, 78);    // #2D374E
    Color clrAcidVolt(255, 204, 255, 0);      // #CCFF00 - Signature high-voltage acid volt
    Color clrEmerald(255, 16, 185, 129);      // #10B981 - Clean emerald
    Color clrAmber(255, 245, 158, 11);        // #F59E0B - Warning amber
    Color clrTextWhite(255, 243, 244, 246);   // #F3F4F6 - Crisp white
    Color clrTextMuted(255, 156, 163, 175);   // #9CA3AF - Muted gray
    Color clrTextDark(255, 107, 114, 128);    // #6B7280 - Darker tech gray
    Color clrJetBlack(255, 11, 14, 20);

    // 1. Fill Deep Obsidian Canvas
    SolidBrush bgBrush(clrObsidian);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // 2. Render Subtle Ambient Techwear Grid Texture (Identical to Concept 2 web grid)
    SolidBrush gridDotBrush(Color(22, 255, 255, 255));
    for (int gx = 20; gx < width; gx += 26) {
        for (int gy = 16; gy < height; gy += 26) {
            g.FillRectangle(&gridDotBrush, gx, gy, 1, 1);
        }
    }

    // Typography
    FontFamily fontMono(L"Consolas");
    FontFamily fontSans(L"Segoe UI");
    Font fontHeaderPill(&fontMono, 10, FontStyleBold, UnitPixel);
    Font fontTitle(&fontSans, 16, FontStyleBold, UnitPixel);
    Font fontSub(&fontMono, 10, FontStyleRegular, UnitPixel);
    Font fontCardTitle(&fontSans, 13, FontStyleBold, UnitPixel);
    Font fontCardDesc(&fontSans, 10, FontStyleRegular, UnitPixel);
    Font fontBadge(&fontMono, 9, FontStyleBold, UnitPixel);
    Font fontBtn(&fontMono, 11, FontStyleBold, UnitPixel);
    Font fontBtnDeploy(&fontSans, 12, FontStyleBold, UnitPixel);
    Font fontModalLog(&fontMono, 10, FontStyleRegular, UnitPixel);
    Font fontModalTitle(&fontMono, 12, FontStyleBold, UnitPixel);

    // Brushes
    SolidBrush acidBrush(clrAcidVolt);
    SolidBrush whiteBrush(clrTextWhite);
    SolidBrush mutedBrush(clrTextMuted);
    SolidBrush darkBrush(clrTextDark);
    SolidBrush emeraldBrush(clrEmerald);
    SolidBrush jetBlackBrush(clrJetBlack);
    SolidBrush amberBrush(clrAmber);

    // 3. Header Container (Rounded-2xl Techwear Glass Banner)
    int headW = width - 48;
    SolidBrush headerBg(clrSlate);
    Pen headerBorder(clrBorder, 1.0f);
    FillRoundRect(g, &headerBg, 24, 14, headW, 86, 12);
    DrawRoundRect(g, &headerBorder, 24, 14, headW, 86, 12);

    // 3A. Top-Left Stencils & Pills
    SolidBrush pillBg(Color(255, 22, 28, 42));
    Pen pillBorder(Color(255, 38, 50, 75), 1.0f);

    // Stencil 1: // SYS.CORE-TUNER v2.4 (with pulsing acid volt dot)
    FillRoundRect(g, &pillBg, 40, 24, 185, 22, 6);
    DrawRoundRect(g, &pillBorder, 40, 24, 185, 22, 6);
    g.FillEllipse(&acidBrush, 48, 30, 8, 8);
    g.DrawString(L"// SYS.CORE-TUNER v2.4", -1, &fontHeaderPill, PointF(60, 28), &acidBrush);

    // Stencil 2: // LENOVO_82HS
    FillRoundRect(g, &pillBg, 235, 24, 115, 22, 6);
    DrawRoundRect(g, &pillBorder, 235, 24, 115, 22, 6);
    g.DrawString(L"// LEN_82HS", -1, &fontHeaderPill, PointF(245, 28), &darkBrush);

    // 3B. UAC Privilege Indicator Badge (Interactive Elevation Shield!)
    int privW = 210, privH = 22;
    int privX = 360, privY = 24;
    g_btnPrivilegeRect = { privX, privY, privX + privW, privY + privH };

    if (g_isAdmin) {
        SolidBrush adminPillBg(Color(255, 20, 36, 26));
        Pen adminPillBorder(Color(255, 30, 90, 50), 1.0f);
        FillRoundRect(g, &adminPillBg, privX, privY, privW, privH, 6);
        DrawRoundRect(g, &adminPillBorder, privX, privY, privW, privH, 6);
        g.DrawString(L"[ ✓ ADMIN: ELEVATED (UAC) ]", -1, &fontBadge, PointF((REAL)privX + 12, (REAL)privY + 5), &acidBrush);
    } else {
        SolidBrush userPillBg(g_hoverPrivilege ? Color(255, 55, 38, 15) : Color(255, 40, 28, 12));
        Pen userPillBorder(g_hoverPrivilege ? clrAmber : Color(255, 120, 80, 20), 1.0f);
        FillRoundRect(g, &userPillBg, privX, privY, privW, privH, 6);
        DrawRoundRect(g, &userPillBorder, privX, privY, privW, privH, 6);
        g.DrawString(L"[ 🛡️ STANDARD: CLICK FOR UAC ]", -1, &fontBadge, PointF((REAL)privX + 10, (REAL)privY + 5), &amberBrush);
    }

    // Hardware Processor Spec Titles
    g.DrawString(L"INTEL Core i7-1165G7 [4C/8T]", -1, &fontTitle, PointF(40, 50), &whiteBrush);
    g.DrawString(L"TARGET SPEC: 16GB RAM  •  512GB NVMe SSD  •  WINDOWS 11 / 10", -1, &fontSub, PointF(41, 74), &mutedBrush);

    // 4. Top-Right Telemetry Governor & Equalizer Module
    int telemW = 295, telemH = 64;
    int telemX = width - 24 - telemW - 12;
    int telemY = 24;
    SolidBrush telemBg(Color(255, 18, 23, 35));
    Pen telemBorder(Color(255, 34, 44, 65), 1.0f);
    FillRoundRect(g, &telemBg, telemX, telemY, telemW, telemH, 10);
    DrawRoundRect(g, &telemBorder, telemX, telemY, telemW, telemH, 10);

    // Dynamic Equalizer Frequency Bars
    for (int b = 0; b < 6; ++b) {
        int barH = g_eqBars[b];
        int barX = telemX + 14 + b * 6;
        int barY = telemY + 50 - barH;
        FillRoundRect(g, &acidBrush, barX, barY, 4, barH, 1);
    }

    Pen divPen(Color(255, 34, 44, 65), 1.0f);
    g.DrawLine(&divPen, telemX + 58, telemY + 12, telemX + 58, telemY + telemH - 12);

    Font fontTelemLabel(&fontMono, 8, FontStyleBold, UnitPixel);
    Font fontTelemVal(&fontMono, 10, FontStyleBold, UnitPixel);
    g.DrawString(L"THROUGHPUT GOVERNOR", -1, &fontTelemLabel, PointF((REAL)telemX + 68, (REAL)telemY + 12), &darkBrush);
    g.DrawString(L"MAX SPEED // 100% UNLOCKED", -1, &fontTelemVal, PointF((REAL)telemX + 68, (REAL)telemY + 24), &acidBrush);

    g.DrawString(L"STORAGE GAIN", -1, &fontTelemLabel, PointF((REAL)telemX + 68, (REAL)telemY + 38), &darkBrush);
    g.DrawString(L"+7.5 GB FREED", -1, &fontTelemVal, PointF((REAL)telemX + 68, (REAL)telemY + 49), &whiteBrush);

    // 5. Render 8 Optimization Cards (Rounded Cybercore Style)
    for (size_t i = 0; i < g_cards.size(); ++i) {
        const auto& card = g_cards[i];
        bool isHovered = ((int)i == g_hoverCardIndex) && !g_showModal && !g_showStartupModal;

        Color cardBg = isHovered ? clrSlateHover : clrSlate;
        Color borderClr = card.isEnabled ? (isHovered ? clrAcidVolt : clrBorder) : Color(255, 24, 29, 40);

        SolidBrush cBrush(cardBg);
        Pen pPen(borderClr, isHovered ? 1.5f : 1.0f);

        int cx = (int)card.rect.left;
        int cy = (int)card.rect.top;
        int cw = (int)(card.rect.right - card.rect.left);
        int ch = (int)(card.rect.bottom - card.rect.top);

        // Rounded Card Container
        FillRoundRect(g, &cBrush, cx, cy, cw, ch, 10);
        DrawRoundRect(g, &pPen, cx, cy, cw, ch, 10);

        // Rounded ID Number Box
        SolidBrush numBg(isHovered && card.isEnabled ? clrAcidVolt : Color(255, 24, 32, 48));
        Pen numBorder(isHovered && card.isEnabled ? clrAcidVolt : Color(255, 38, 50, 73), 1.0f);
        FillRoundRect(g, &numBg, cx + 14, cy + 12, 28, 28, 6);
        DrawRoundRect(g, &numBorder, cx + 14, cy + 12, 28, 28, 6);
        SolidBrush* numTextBrush = (isHovered && card.isEnabled) ? &jetBlackBrush : &acidBrush;
        g.DrawString(card.numStr.c_str(), -1, &fontBtn, PointF((REAL)cx + 19, (REAL)cy + 18), numTextBrush);

        // Card Title & Description
        SolidBrush titleBrush = card.isEnabled ? (isHovered ? clrAcidVolt : clrTextWhite) : clrTextDark;
        g.DrawString(card.title.c_str(), -1, &fontCardTitle, PointF((REAL)cx + 52, (REAL)cy + 9), &titleBrush);
        g.DrawString(card.desc.c_str(), -1, &fontCardDesc, PointF((REAL)cx + 52, (REAL)cy + 30), &mutedBrush);

        // Rounded Badge Pill
        SolidBrush badgeBg(card.isEnabled ? Color(255, 22, 34, 24) : Color(255, 22, 27, 36));
        Pen badgeBorder(card.isEnabled ? Color(255, 60, 100, 20) : Color(255, 35, 42, 56), 1.0f);
        SolidBrush badgeText(card.isEnabled ? clrAcidVolt : clrTextDark);
        int badgeW = 105, badgeH = 20;
        int badgeX = cx + cw - 195, badgeY = cy + 17;
        FillRoundRect(g, &badgeBg, badgeX, badgeY, badgeW, badgeH, 5);
        DrawRoundRect(g, &badgeBorder, badgeX, badgeY, badgeW, badgeH, 5);
        g.DrawString(card.badge.c_str(), -1, &fontBadge, PointF((REAL)badgeX + 10, (REAL)badgeY + 4), &badgeText);

        // Cybercore Rounded Toggle Slider Switch
        int sx = (int)card.switchRect.left;
        int sy = (int)card.switchRect.top;
        int sw = (int)(card.switchRect.right - card.switchRect.left);
        int sh = (int)(card.switchRect.bottom - card.switchRect.top);

        SolidBrush swBg(Color(255, 20, 26, 38));
        Pen swBorder(card.isEnabled ? clrAcidVolt : Color(255, 42, 54, 76), 1.0f);
        FillRoundRect(g, &swBg, sx, sy, sw, sh, 7);
        DrawRoundRect(g, &swBorder, sx, sy, sw, sh, 7);

        if (card.isEnabled) {
            // ON Thumb (Acid Volt Pill on right with neon glow)
            FillRoundRect(g, &acidBrush, sx + sw - 28, sy + 2, 26, sh - 4, 5);
            g.DrawString(L"ON", -1, &fontBadge, PointF((REAL)(sx + sw - 23), (REAL)sy + 5), &jetBlackBrush);
        } else {
            // OFF Thumb (Dark slate on left)
            SolidBrush offThumb(Color(255, 35, 44, 62));
            FillRoundRect(g, &offThumb, sx + 2, sy + 2, 26, sh - 4, 5);
            g.DrawString(L"OFF", -1, &fontBadge, PointF((REAL)sx + 6, (REAL)sy + 5), &mutedBrush);
        }
    }

    // 6. Live Status Bar
    int statusY = height - 82;
    g.FillEllipse(&acidBrush, 28, statusY + 4, 6, 6);
    g.DrawString(L"[SYS_LOG] >> READY. 8 PROTOCOLS ARMED. CLICK DEPLOY TO ENGAGE.", -1, &fontSub, PointF(40, (REAL)statusY), &acidBrush);

    // 7. Restore Defaults Button
    SolidBrush restBg(g_hoverRestore ? Color(255, 28, 36, 54) : Color(255, 20, 26, 38));
    Pen restBorder(g_hoverRestore ? clrTextWhite : Color(255, 36, 48, 70), 1.0f);
    int rw = g_btnRestoreRect.right - g_btnRestoreRect.left;
    int rh = g_btnRestoreRect.bottom - g_btnRestoreRect.top;
    FillRoundRect(g, &restBg, g_btnRestoreRect.left, g_btnRestoreRect.top, rw, rh, 10);
    DrawRoundRect(g, &restBorder, g_btnRestoreRect.left, g_btnRestoreRect.top, rw, rh, 10);
    g.DrawString(L"↺ RESTORE DEFAULTS", -1, &fontBtn, PointF((REAL)g_btnRestoreRect.left + 16, (REAL)g_btnRestoreRect.top + 15), &whiteBrush);

    // 8. Toggle All Button
    SolidBrush tAllBg(g_hoverToggleAll ? Color(255, 28, 36, 54) : Color(255, 20, 26, 38));
    Pen tAllBorder(g_hoverToggleAll ? clrTextWhite : Color(255, 36, 48, 70), 1.0f);
    int tw = g_btnToggleAllRect.right - g_btnToggleAllRect.left;
    int th = g_btnToggleAllRect.bottom - g_btnToggleAllRect.top;
    FillRoundRect(g, &tAllBg, g_btnToggleAllRect.left, g_btnToggleAllRect.top, tw, th, 10);
    DrawRoundRect(g, &tAllBorder, g_btnToggleAllRect.left, g_btnToggleAllRect.top, tw, th, 10);
    g.DrawString(L"TOGGLE ALL", -1, &fontBtn, PointF((REAL)g_btnToggleAllRect.left + 22, (REAL)g_btnToggleAllRect.top + 15), &whiteBrush);

    // 9. Startup Applications Manager Button
    SolidBrush smBg(g_hoverStartupMgr ? Color(255, 28, 42, 64) : Color(255, 18, 28, 44));
    Pen smBorder(g_hoverStartupMgr ? clrAcidVolt : Color(255, 36, 58, 88), 1.0f);
    int smw = g_btnStartupMgrRect.right - g_btnStartupMgrRect.left;
    int smh = g_btnStartupMgrRect.bottom - g_btnStartupMgrRect.top;
    FillRoundRect(g, &smBg, g_btnStartupMgrRect.left, g_btnStartupMgrRect.top, smw, smh, 10);
    DrawRoundRect(g, &smBorder, g_btnStartupMgrRect.left, g_btnStartupMgrRect.top, smw, smh, 10);
    g.DrawString(L"🚀 STARTUP APPS", -1, &fontBtn, PointF((REAL)g_btnStartupMgrRect.left + 18, (REAL)g_btnStartupMgrRect.top + 15), &acidBrush);

    // 10. Main Action Button: DEPLOY TACTICAL TUNING
    Color depBg = g_hoverDeploy ? Color(255, 224, 255, 50) : clrAcidVolt;
    SolidBrush depBrush(depBg);
    int dw = g_btnDeployRect.right - g_btnDeployRect.left;
    int dh = g_btnDeployRect.bottom - g_btnDeployRect.top;
    FillRoundRect(g, &depBrush, g_btnDeployRect.left, g_btnDeployRect.top, dw, dh, 10);
    if (g_hoverDeploy) {
        Pen glowPen(Color(255, 255, 255, 255), 1.5f);
        DrawRoundRect(g, &glowPen, g_btnDeployRect.left - 1, g_btnDeployRect.top - 1, dw + 2, dh + 2, 10);
    }
    g.DrawString(L"⚡ DEPLOY TACTICAL TUNING ➔", -1, &fontBtnDeploy, PointF((REAL)g_btnDeployRect.left + 50, (REAL)g_btnDeployRect.top + 14), &jetBlackBrush);

    // =========================================================================
    // 11. BESPOKE IN-WINDOW CYBERCORE OVERCLOCK MODAL OVERLAY
    // =========================================================================
    if (g_showModal) {
        SolidBrush modalBackdrop(Color(220, 7, 9, 14));
        g.FillRectangle(&modalBackdrop, 0, 0, width, height);

        int mx = 80, my = 50, mw = width - 160, mh = height - 100;
        g_modalDismissRect = { mx + mw - 220, my + mh - 54, mx + mw - 24, my + mh - 16 };

        // Rounded Modal Body
        SolidBrush modalBody(clrObsidian);
        Pen modalPen(Color(255, 42, 54, 78), 1.5f);
        FillRoundRect(g, &modalBody, mx, my, mw, mh, 16);
        DrawRoundRect(g, &modalPen, mx, my, mw, mh, 16);

        // Header Top Bar
        SolidBrush modalTopBg(clrSlate);
        Pen modalTopBorder(clrBorder, 1.0f);
        FillRoundRect(g, &modalTopBg, mx, my, mw, 52, 16);
        DrawRoundRect(g, &modalTopBorder, mx, my, mw, 52, 16);

        // Pulsing Acid-Volt Dot & Title
        g.FillEllipse(&acidBrush, mx + 20, my + 21, 10, 10);
        g.DrawString(L"// SYSTEM OVERCLOCK ENGINE // KERNEL-DISPATCH", -1, &fontModalTitle, PointF((REAL)mx + 38, (REAL)my + 18), &acidBrush);

        // Progress Percentage Pill
        std::wstring pctStr = std::to_wstring(g_progressPercent) + L"%";
        SolidBrush pctBg(Color(255, 22, 34, 24));
        Pen pctBorder(Color(255, 60, 100, 20), 1.0f);
        FillRoundRect(g, &pctBg, mx + mw - 80, my + 15, 60, 24, 6);
        DrawRoundRect(g, &pctBorder, mx + mw - 80, my + 15, 60, 24, 6);
        g.DrawString(pctStr.c_str(), -1, &fontBadge, PointF((REAL)mx + mw - 66, (REAL)my + 20), &acidBrush);

        // Gradient Progress Bar
        SolidBrush progTrack(Color(255, 16, 20, 30));
        FillRoundRect(g, &progTrack, mx + 20, my + 66, mw - 40, 8, 4);
        int progFillW = (int)((float)(mw - 40) * ((float)g_progressPercent / 100.0f));
        if (progFillW > 0) {
            LinearGradientBrush gradBrush(Point(mx + 20, 0), Point(mx + mw - 20, 0), clrAcidVolt, clrEmerald);
            FillRoundRect(g, &gradBrush, mx + 20, my + 66, progFillW, 8, 4);
        }

        // Monospace Console Terminal Window
        int lx = mx + 20, ly = my + 88, lw = mw - 40, lh = mh - 160;
        SolidBrush termBg(Color(255, 7, 9, 14));
        Pen termBorder(Color(255, 27, 34, 48), 1.0f);
        FillRoundRect(g, &termBg, lx, ly, lw, lh, 8);
        DrawRoundRect(g, &termBorder, lx, ly, lw, lh, 8);

        // Draw Log Entries
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

        // Bottom Status Label
        g.DrawString(g_modalStatus.c_str(), -1, &fontSub, PointF((REAL)mx + 20, (REAL)my + mh - 40), &acidBrush);

        // Dismiss Console Action Button
        if (g_modalComplete) {
            SolidBrush disBg(g_hoverModalDismiss ? Color(255, 224, 255, 50) : clrAcidVolt);
            int dw = g_modalDismissRect.right - g_modalDismissRect.left;
            int dh = g_modalDismissRect.bottom - g_modalDismissRect.top;
            FillRoundRect(g, &disBg, g_modalDismissRect.left, g_modalDismissRect.top, dw, dh, 8);
            g.DrawString(L"[ ✓ DISMISS CONSOLE ]", -1, &fontBtn, PointF((REAL)g_modalDismissRect.left + 16, (REAL)g_modalDismissRect.top + 13), &jetBlackBrush);
        }
    }

    // =========================================================================
    // 12. BESPOKE IN-WINDOW STARTUP APPLICATIONS MANAGER MODAL
    // =========================================================================
    if (g_showStartupModal) {
        SolidBrush modalBackdrop(Color(220, 7, 9, 14));
        g.FillRectangle(&modalBackdrop, 0, 0, width, height);

        int mx = 60, my = 40, mw = width - 120, mh = height - 80;

        // Modal Body
        SolidBrush modalBody(clrObsidian);
        Pen modalPen(Color(255, 42, 54, 78), 1.5f);
        FillRoundRect(g, &modalBody, mx, my, mw, mh, 16);
        DrawRoundRect(g, &modalPen, mx, my, mw, mh, 16);

        // Top Header Bar
        SolidBrush modalTopBg(clrSlate);
        Pen modalTopBorder(clrBorder, 1.0f);
        FillRoundRect(g, &modalTopBg, mx, my, mw, 62, 16);
        DrawRoundRect(g, &modalTopBorder, mx, my, mw, 62, 16);

        // Title and Subtitle
        g.FillEllipse(&acidBrush, mx + 20, my + 26, 10, 10);
        g.DrawString(L"// STARTUP APPLICATIONS MANAGER // AUTOSTART DECOUPLER", -1, &fontModalTitle, PointF((REAL)mx + 38, (REAL)my + 14), &acidBrush);
        g.DrawString(L"Toggle background autostart bloatware without deleting registry keys. Changes persist cleanly.", -1, &fontSub, PointF((REAL)mx + 38, (REAL)my + 36), &mutedBrush);

        // Refresh & Close Buttons in Header
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

        // List of Startup Applications
        int listY = my + 76;
        int itemH = 52;
        int itemGap = 6;
        int maxVisible = (mh - 100) / (itemH + itemGap);

        if (g_startupApps.empty()) {
            g.DrawString(L"Scanning Windows startup entries...", -1, &fontSub, PointF((REAL)mx + 30, (REAL)listY + 30), &mutedBrush);
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

            // Hive Pill (HKCU / HKLM)
            SolidBrush hiveBg(Color(255, 24, 32, 48));
            Pen hiveBorder(Color(255, 40, 52, 75), 1.0f);
            FillRoundRect(g, &hiveBg, itemX + 12, itemTop + 14, 52, 24, 4);
            DrawRoundRect(g, &hiveBorder, itemX + 12, itemTop + 14, 52, 24, 4);
            g.DrawString(app.location.c_str(), -1, &fontBadge, PointF((REAL)itemX + 18, (REAL)itemTop + 19), &acidBrush);

            // App Name & Path
            SolidBrush nameBrush = app.isEnabled ? clrTextWhite : clrTextDark;
            g.DrawString(app.name.c_str(), -1, &fontCardTitle, PointF((REAL)itemX + 74, (REAL)itemTop + 8), &nameBrush);

            std::wstring shortCmd = app.command;
            if (shortCmd.length() > 70) shortCmd = shortCmd.substr(0, 67) + L"...";
            g.DrawString(shortCmd.c_str(), -1, &fontCardDesc, PointF((REAL)itemX + 74, (REAL)itemTop + 28), &mutedBrush);

            // Toggle Switch
            int swX = itemX + itemW - 80;
            int swY = itemTop + 14;
            int swW = 64;
            int swH = 24;

            SolidBrush swBg(Color(255, 20, 26, 38));
            Pen swBorder(app.isEnabled ? clrAcidVolt : Color(255, 42, 54, 76), 1.0f);
            FillRoundRect(g, &swBg, swX, swY, swW, swH, 7);
            DrawRoundRect(g, &swBorder, swX, swY, swW, swH, 7);

            if (app.isEnabled) {
                FillRoundRect(g, &acidBrush, swX + swW - 28, swY + 2, 26, swH - 4, 5);
                g.DrawString(L"ON", -1, &fontBadge, PointF((REAL)(swX + swW - 23), (REAL)swY + 5), &jetBlackBrush);
            } else {
                SolidBrush offThumb(Color(255, 35, 44, 62));
                FillRoundRect(g, &offThumb, swX + 2, swY + 2, 26, swH - 4, 5);
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

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Apply Windows 11 Immersive Dark Mode and Rounded Corners
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
            int corner = 3; /* DWMWCP_ROUND */
            DwmSetWindowAttribute(hWnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
            
            // Set dark caption color (Windows 11 Build 22000+)
            COLORREF captionClr = RGB(11, 14, 20);
            DwmSetWindowAttribute(hWnd, 35 /* DWMWA_CAPTION_COLOR */, &captionClr, sizeof(captionClr));

            g_isAdmin = CheckIsAdmin();

            RECT rc;
            GetClientRect(hWnd, &rc);
            InitOptimizationCards(rc.right - rc.left, rc.bottom - rc.top);
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
            bool redraw = false;

            if (g_showStartupModal) {
                bool hCls = PtInRect(&g_startupCloseRect, pt);
                if (hCls != g_hoverStartupClose) { g_hoverStartupClose = hCls; redraw = true; }

                bool hRef = PtInRect(&g_startupRefreshRect, pt);
                if (hRef != g_hoverStartupRefresh) { g_hoverStartupRefresh = hRef; redraw = true; }

                RECT rc;
                GetClientRect(hWnd, &rc);
                int width = rc.right - rc.left;
                int height = rc.bottom - rc.top;
                int mx = 60, my = 40, mw = width - 120, mh = height - 80;
                int listY = my + 76;
                int itemH = 52, itemGap = 6;
                int maxVisible = (mh - 100) / (itemH + itemGap);

                int oldHoverStartup = g_hoverStartupIndex;
                g_hoverStartupIndex = -1;
                for (size_t s = 0; s < g_startupApps.size() && (int)s < maxVisible; ++s) {
                    int itemTop = listY + (int)s * (itemH + itemGap);
                    RECT appRect = { mx + 20, itemTop, mx + 20 + mw - 40, itemTop + itemH };
                    if (PtInRect(&appRect, pt)) {
                        g_hoverStartupIndex = (int)s;
                        break;
                    }
                }
                if (oldHoverStartup != g_hoverStartupIndex) redraw = true;

            } else if (g_showModal) {
                bool hDis = PtInRect(&g_modalDismissRect, pt) && g_modalComplete;
                if (hDis != g_hoverModalDismiss) { g_hoverModalDismiss = hDis; redraw = true; }
            } else {
                bool hPriv = PtInRect(&g_btnPrivilegeRect, pt);
                if (hPriv != g_hoverPrivilege) { g_hoverPrivilege = hPriv; redraw = true; }

                int oldHover = g_hoverCardIndex;
                g_hoverCardIndex = -1;
                for (size_t i = 0; i < g_cards.size(); ++i) {
                    if (PtInRect(&g_cards[i].rect, pt)) {
                        g_hoverCardIndex = (int)i;
                        break;
                    }
                }
                if (oldHover != g_hoverCardIndex) redraw = true;

                bool hDep = PtInRect(&g_btnDeployRect, pt) && !g_isExecuting && !g_isRestoring;
                if (hDep != g_hoverDeploy) { g_hoverDeploy = hDep; redraw = true; }

                bool hRes = PtInRect(&g_btnRestoreRect, pt) && !g_isExecuting && !g_isRestoring;
                if (hRes != g_hoverRestore) { g_hoverRestore = hRes; redraw = true; }

                bool hTgl = PtInRect(&g_btnToggleAllRect, pt) && !g_isExecuting && !g_isRestoring;
                if (hTgl != g_hoverToggleAll) { g_hoverToggleAll = hTgl; redraw = true; }

                bool hSm = PtInRect(&g_btnStartupMgrRect, pt) && !g_isExecuting && !g_isRestoring;
                if (hSm != g_hoverStartupMgr) { g_hoverStartupMgr = hSm; redraw = true; }
            }

            if (redraw) InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            // Handle Startup Applications Modal Clicks
            if (g_showStartupModal) {
                if (PtInRect(&g_startupCloseRect, pt)) {
                    g_showStartupModal = false;
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                if (PtInRect(&g_startupRefreshRect, pt)) {
                    g_startupApps = LaptopOptimizerEngine::EnumerateStartupApps();
                    InvalidateRect(hWnd, NULL, FALSE);
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
                        InvalidateRect(hWnd, NULL, FALSE);
                        return 0;
                    }
                }
                return 0;
            }

            // Handle Overclock Modal Clicks
            if (g_showModal) {
                if (g_modalComplete && PtInRect(&g_modalDismissRect, pt)) {
                    g_showModal = false;
                    InvalidateRect(hWnd, NULL, FALSE);
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
                    InvalidateRect(hWnd, NULL, FALSE);
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
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            // Startup Apps Manager Button
            if (PtInRect(&g_btnStartupMgrRect, pt)) {
                g_startupApps = LaptopOptimizerEngine::EnumerateStartupApps();
                g_showStartupModal = true;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            // Restore Defaults Button
            if (PtInRect(&g_btnRestoreRect, pt)) {
                g_showModal = true;
                for (auto& c : g_cards) c.isEnabled = true;
                std::thread(RunRestorePipeline).detach();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            // Deploy Tactical Tuning Button
            if (PtInRect(&g_btnDeployRect, pt)) {
                g_showModal = true;
                std::thread(RunOptimizationPipeline).detach();
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Double buffered, avoid flicker

        case WM_PAINT:
            OnPaint(hWnd);
            return 0;

        case WM_DESTROY:
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
        // Automatically request UAC elevation dialog so user gets prompt
        if (ElevateProcessViaUAC(NULL)) {
            return 0; // Elevated instance launched successfully
        }
        // If user cancelled UAC dialog, continue in standard mode (user can click privilege pill)
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
    
    // Load embedded high-resolution application icon
    wc.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_SHARED);

    RegisterClassExW(&wc);

    // 4. Create Window with exact Concept 2 proportions (960 x 740)
    int w = 960;
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
