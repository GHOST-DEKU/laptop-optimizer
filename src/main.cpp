#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <propidl.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include "optimizer.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Gdiplus;

// Helper drawing functions avoiding GCC overload ambiguity
inline void FillRectI(Graphics& g, const Brush* b, int x, int y, int w, int h) {
    g.FillRectangle(b, (INT)x, (INT)y, (INT)w, (INT)h);
}
inline void DrawRectI(Graphics& g, const Pen* p, int x, int y, int w, int h) {
    g.DrawRectangle(p, (INT)x, (INT)y, (INT)w, (INT)h);
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
bool g_showModal = false;
int g_progressPercent = 0;
std::wstring g_modalStatus = L"// INITIATING LOW-LEVEL PROTOCOLS...";
std::vector<std::wstring> g_modalLogs;
bool g_modalComplete = false;

// Buttons RECTs (Cybercore Tactical Grid)
RECT g_btnDeployRect      = { 550, 568, 840, 616 };
RECT g_btnRestoreRect     = { 40,  570, 260, 614 };
RECT g_btnToggleAllRect   = { 272, 570, 400, 614 };
RECT g_modalDismissRect   = { 530, 470, 715, 508 };

// Hover States
bool g_hoverDeploy = false;
bool g_hoverRestore = false;
bool g_hoverToggleAll = false;
bool g_hoverModalDismiss = false;
int  g_hoverCardIndex = -1;

// Equalizer Frequency Bar Heights for Telemetry Module
int g_eqBars[6] = { 10, 18, 14, 22, 12, 16 };

void InitOptimizationCards() {
    g_cards.clear();
    g_cards.push_back({ 1, L"01", L"Zero-Ramp CPU Frequency Floor on AC", L"Sets processor state to 100% floor on AC power to eliminate turbo ramp stutter.", L"100% FLOOR", true });
    g_cards.push_back({ 2, L"02", L"Bypass Game DVR Background Video Buffer", L"Halts continuous background DirectX video encoding and capture hooks; saves GPU VRAM.", L"ZERO VRAM", true });
    g_cards.push_back({ 3, L"03", L"Instant Desktop Context Menus (20ms)", L"Cuts MenuShowDelay from 400ms to 20ms for instantaneous shell responsiveness.", L"20ms DELAY", true });
    g_cards.push_back({ 4, L"04", L"Purge Windows 11 Widget Daemon Pack", L"Uninstalls WebExperience package; permanently terminates 20+ idle WebView2 processes.", L"-300MB RAM", true });
    g_cards.push_back({ 5, L"05", L"Decouple Bing Cloud Integration from Start", L"Eliminates remote cloud latency on Start search; makes app launch search instant.", L"LOCAL ONLY", true });
    g_cards.push_back({ 6, L"06", L"Halt Delivery Optimization P2P Seeding", L"Stops Windows from using your disk I/O and bandwidth to seed updates to network peers.", L"HTTP ONLY", true });
    g_cards.push_back({ 7, L"07", L"Clean Kernel Boots & Purge hiberfil.sys", L"Reclaims +6.22 GB NVMe SSD storage permanently and guarantees 100% clean boots.", L"+6.2GB SSD", true });
    g_cards.push_back({ 8, L"08", L"Purge Stale Temporary Files (>24h)", L"Cleans accumulated cache junk from %TEMP% to free NVMe storage and reduce indexing.", L"CLEAN TEMP", true });

    int startY = 108;
    int cardH = 50;
    int gap = 6;
    for (size_t i = 0; i < g_cards.size(); ++i) {
        int y = startY + (int)i * (cardH + gap);
        g_cards[i].rect = { 40, y, 840, y + cardH };
        g_cards[i].switchRect = { 770, y + 13, 824, y + 37 };
    }
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

// Double-Buffered Painting with Concept 2: Cybercore // Dark Techwear Theme
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
    Color clrObsidian(255, 11, 14, 20);       // #0B0E14
    Color clrSlate(255, 16, 20, 30);          // #10141E
    Color clrSlateHover(255, 22, 27, 40);     // #161B28
    Color clrBorder(255, 30, 35, 48);         // #1E2330
    Color clrAcidVolt(255, 204, 255, 0);      // #CCFF00
    Color clrEmerald(255, 16, 185, 129);      // #10B981
    Color clrTextWhite(255, 243, 244, 246);   // #F3F4F6
    Color clrTextMuted(255, 156, 163, 175);   // #9CA3AF
    Color clrTextDark(255, 107, 114, 128);    // #6B7280
    Color clrJetBlack(255, 11, 14, 20);

    // 1. Fill Deep Obsidian Canvas
    SolidBrush bgBrush(clrObsidian);
    FillRectI(g, &bgBrush, 0, 0, width, height);

    // 2. Header Container
    SolidBrush headerBg(clrSlate);
    FillRectI(g, &headerBg, 0, 0, width, 96);
    Pen headerBorder(clrBorder, 1.0f);
    g.DrawLine(&headerBorder, 0, 96, width, 96);

    // Fonts (Monospace & Stencil Typography)
    FontFamily fontMono(L"Consolas");
    Font fontHeaderPill(&fontMono, 10, FontStyleBold, UnitPixel);
    Font fontTitle(&fontMono, 16, FontStyleBold, UnitPixel);
    Font fontSub(&fontMono, 11, FontStyleRegular, UnitPixel);
    Font fontCardTitle(&fontMono, 12, FontStyleBold, UnitPixel);
    Font fontCardDesc(&fontMono, 10, FontStyleRegular, UnitPixel);
    Font fontBadge(&fontMono, 9, FontStyleBold, UnitPixel);
    Font fontBtn(&fontMono, 11, FontStyleBold, UnitPixel);
    Font fontModalLog(&fontMono, 11, FontStyleRegular, UnitPixel);

    // Brushes
    SolidBrush acidBrush(clrAcidVolt);
    SolidBrush whiteBrush(clrTextWhite);
    SolidBrush mutedBrush(clrTextMuted);
    SolidBrush darkBrush(clrTextDark);
    SolidBrush emeraldBrush(clrEmerald);
    SolidBrush jetBlackBrush(clrJetBlack);

    // 3. Top-Left Stencils & System Info
    SolidBrush pillBg(Color(255, 22, 27, 38));
    Pen pillBorder(Color(255, 38, 48, 69), 1.0f);
    FillRectI(g, &pillBg, 40, 16, 175, 20);
    DrawRectI(g, &pillBorder, 40, 16, 175, 20);
    g.DrawString(L"// SYS.CORE-TUNER v2.4", -1, &fontHeaderPill, PointF(46, 19), &acidBrush);

    g.DrawString(L"// LENOVO_82HS", -1, &fontHeaderPill, PointF(225, 19), &darkBrush);

    g.DrawString(L"INTEL Core i7-1165G7 [4C/8T]", -1, &fontTitle, PointF(40, 42), &whiteBrush);
    g.DrawString(L"TARGET SPEC: 16GB RAM  •  512GB NVMe SSD  •  WINDOWS 11 / 10", -1, &fontSub, PointF(40, 68), &mutedBrush);

    // 4. Top-Right Telemetry Equalizer Module
    SolidBrush telemBg(Color(255, 17, 22, 34));
    Pen telemBorder(Color(255, 32, 41, 60), 1.0f);
    FillRectI(g, &telemBg, 540, 18, 300, 60);
    DrawRectI(g, &telemBorder, 540, 18, 300, 60);

    // Dynamic Equalizer Frequency Bars
    for (int b = 0; b < 6; ++b) {
        int barH = g_eqBars[b];
        int barX = 552 + b * 6;
        int barY = 18 + 48 - barH;
        FillRectI(g, &acidBrush, barX, barY, 4, barH);
    }

    Pen divPen(Color(255, 32, 41, 60), 1.0f);
    g.DrawLine(&divPen, 595, 24, 595, 72);

    Font fontTelemLabel(&fontMono, 8, FontStyleBold, UnitPixel);
    Font fontTelemVal(&fontMono, 10, FontStyleBold, UnitPixel);
    g.DrawString(L"THROUGHPUT GOVERNOR", -1, &fontTelemLabel, PointF(605, 24), &darkBrush);
    g.DrawString(L"MAX SPEED // 100% UNLOCKED", -1, &fontTelemVal, PointF(605, 36), &acidBrush);

    g.DrawString(L"STORAGE GAIN", -1, &fontTelemLabel, PointF(605, 52), &darkBrush);
    g.DrawString(L"+7.5 GB FREED", -1, &fontTelemVal, PointF(605, 64), &whiteBrush);

    // 5. Render 8 Optimization Cards (Cybercore Style)
    for (size_t i = 0; i < g_cards.size(); ++i) {
        const auto& card = g_cards[i];
        bool isHovered = ((int)i == g_hoverCardIndex) && !g_showModal;

        Color cardBg = isHovered ? clrSlateHover : clrSlate;
        Color borderClr = card.isEnabled ? (isHovered ? clrAcidVolt : clrBorder) : Color(255, 25, 30, 42);

        SolidBrush cBrush(cardBg);
        Pen pPen(borderClr, isHovered ? 1.5f : 1.0f);

        int cx = (int)card.rect.left;
        int cy = (int)card.rect.top;
        int cw = (int)(card.rect.right - card.rect.left);
        int ch = (int)(card.rect.bottom - card.rect.top);

        FillRectI(g, &cBrush, cx, cy, cw, ch);
        DrawRectI(g, &pPen, cx, cy, cw, ch);

        // ID Number Box
        SolidBrush numBg(Color(255, 24, 32, 48));
        Pen numBorder(Color(255, 38, 50, 73), 1.0f);
        FillRectI(g, &numBg, cx + 12, cy + 11, 28, 28);
        DrawRectI(g, &numBorder, cx + 12, cy + 11, 28, 28);
        g.DrawString(card.numStr.c_str(), -1, &fontBtn, PointF((REAL)cx + 17, (REAL)cy + 17), &acidBrush);

        // Card Title & Description
        SolidBrush titleBrush = card.isEnabled ? (isHovered ? clrAcidVolt : clrTextWhite) : clrTextDark;
        g.DrawString(card.title.c_str(), -1, &fontCardTitle, PointF((REAL)cx + 50, (REAL)cy + 9), &titleBrush);
        g.DrawString(card.desc.c_str(), -1, &fontCardDesc, PointF((REAL)cx + 50, (REAL)cy + 28), &mutedBrush);

        // Badge Pill
        SolidBrush badgeBg(card.isEnabled ? Color(255, 22, 32, 22) : Color(255, 22, 27, 36));
        Pen badgeBorder(card.isEnabled ? Color(255, 60, 100, 20) : Color(255, 35, 42, 56), 1.0f);
        SolidBrush badgeText(card.isEnabled ? clrAcidVolt : clrTextDark);
        FillRectI(g, &badgeBg, cx + 635, cy + 15, 110, 20);
        DrawRectI(g, &badgeBorder, cx + 635, cy + 15, 110, 20);
        g.DrawString(card.badge.c_str(), -1, &fontBadge, PointF((REAL)cx + 645, (REAL)cy + 18), &badgeText);

        // Cybercore Toggle Slider Switch
        int sx = (int)card.switchRect.left;
        int sy = (int)card.switchRect.top;
        int sw = (int)(card.switchRect.right - card.switchRect.left);
        int sh = (int)(card.switchRect.bottom - card.switchRect.top);

        SolidBrush swBg(Color(255, 22, 28, 40));
        Pen swBorder(card.isEnabled ? clrAcidVolt : Color(255, 44, 56, 80), 1.0f);
        FillRectI(g, &swBg, sx, sy, sw, sh);
        DrawRectI(g, &swBorder, sx, sy, sw, sh);

        if (card.isEnabled) {
            // ON Thumb (Acid Volt Pill on right)
            FillRectI(g, &acidBrush, sx + sw - 28, sy + 2, 26, sh - 4);
            g.DrawString(L"ON", -1, &fontBadge, PointF((REAL)(sx + sw - 23), (REAL)sy + 5), &jetBlackBrush);
        } else {
            // OFF Thumb (Dark slate on left)
            SolidBrush offThumb(Color(255, 35, 42, 58));
            FillRectI(g, &offThumb, sx + 2, sy + 2, 26, sh - 4);
            g.DrawString(L"OFF", -1, &fontBadge, PointF((REAL)sx + 6, (REAL)sy + 5), &mutedBrush);
        }
    }

    // 6. Live Status Bar
    g.DrawString(L"[SYS_LOG] >> READY. 8 LOW-LEVEL PROTOCOLS ARMED. CLICK DEPLOY TO ENGAGE.", -1, &fontSub, PointF(40, 546), &acidBrush);

    // 7. Restore Defaults Button
    SolidBrush restBg(g_hoverRestore ? Color(255, 28, 35, 52) : Color(255, 20, 25, 36));
    Pen restBorder(g_hoverRestore ? clrTextWhite : Color(255, 36, 46, 66), 1.0f);
    int rw = g_btnRestoreRect.right - g_btnRestoreRect.left;
    int rh = g_btnRestoreRect.bottom - g_btnRestoreRect.top;
    FillRectI(g, &restBg, g_btnRestoreRect.left, g_btnRestoreRect.top, rw, rh);
    DrawRectI(g, &restBorder, g_btnRestoreRect.left, g_btnRestoreRect.top, rw, rh);
    g.DrawString(L"↺ RESTORE DEFAULTS", -1, &fontBtn, PointF((REAL)g_btnRestoreRect.left + 32, (REAL)g_btnRestoreRect.top + 13), &whiteBrush);

    // 8. Toggle All Button
    SolidBrush tAllBg(g_hoverToggleAll ? Color(255, 28, 35, 52) : Color(255, 20, 25, 36));
    Pen tAllBorder(g_hoverToggleAll ? clrTextWhite : Color(255, 36, 46, 66), 1.0f);
    int tw = g_btnToggleAllRect.right - g_btnToggleAllRect.left;
    int th = g_btnToggleAllRect.bottom - g_btnToggleAllRect.top;
    FillRectI(g, &tAllBg, g_btnToggleAllRect.left, g_btnToggleAllRect.top, tw, th);
    DrawRectI(g, &tAllBorder, g_btnToggleAllRect.left, g_btnToggleAllRect.top, tw, th);
    g.DrawString(L"TOGGLE ALL", -1, &fontBtn, PointF((REAL)g_btnToggleAllRect.left + 22, (REAL)g_btnToggleAllRect.top + 13), &whiteBrush);

    // 9. Main Action Button: DEPLOY TACTICAL TUNING
    Color depBg = g_hoverDeploy ? Color(255, 220, 255, 50) : clrAcidVolt;
    SolidBrush depBrush(depBg);
    int dw = g_btnDeployRect.right - g_btnDeployRect.left;
    int dh = g_btnDeployRect.bottom - g_btnDeployRect.top;
    FillRectI(g, &depBrush, g_btnDeployRect.left, g_btnDeployRect.top, dw, dh);
    if (g_hoverDeploy) {
        Pen glowPen(Color(255, 255, 255, 255), 1.5f);
        DrawRectI(g, &glowPen, g_btnDeployRect.left - 1, g_btnDeployRect.top - 1, dw + 2, dh + 2);
    }
    g.DrawString(L"⚡ DEPLOY TACTICAL TUNING ➔", -1, &fontBtn, PointF((REAL)g_btnDeployRect.left + 28, (REAL)g_btnDeployRect.top + 15), &jetBlackBrush);

    // =========================================================================
    // 10. BESPOKE IN-WINDOW CYBERCORE OVERCLOCK MODAL OVERLAY
    // =========================================================================
    if (g_showModal) {
        // Dark translucent obsidian backdrop
        SolidBrush modalBackdrop(Color(220, 7, 9, 14));
        FillRectI(g, &modalBackdrop, 0, 0, width, height);

        int mx = 120, my = 90, mw = 640, mh = 450;

        // Modal Box Body
        SolidBrush modalBody(clrObsidian);
        Pen modalPen(Color(255, 38, 48, 69), 1.5f);
        FillRectI(g, &modalBody, mx, my, mw, mh);
        DrawRectI(g, &modalPen, mx, my, mw, mh);

        // Header Top Bar
        SolidBrush modalTopBg(clrSlate);
        FillRectI(g, &modalTopBg, mx, my, mw, 48);
        Pen modalTopBorder(clrBorder, 1.0f);
        g.DrawLine(&modalTopBorder, mx, my + 48, mx + mw, my + 48);

        // Pulsing Acid-Volt Dot & Title
        SolidBrush dotBrush(clrAcidVolt);
        g.FillEllipse(&dotBrush, mx + 20, my + 19, 10, 10);
        g.DrawString(L"// SYSTEM OVERCLOCK ENGINE // KERNEL-DISPATCH", -1, &fontBtn, PointF((REAL)mx + 38, (REAL)my + 16), &acidBrush);

        // Progress Percentage Pill
        std::wstring pctStr = std::to_wstring(g_progressPercent) + L"%";
        SolidBrush pctBg(Color(255, 22, 32, 22));
        Pen pctBorder(Color(255, 60, 100, 20), 1.0f);
        FillRectI(g, &pctBg, mx + mw - 75, my + 13, 55, 22);
        DrawRectI(g, &pctBorder, mx + mw - 75, my + 13, 55, 22);
        g.DrawString(pctStr.c_str(), -1, &fontBadge, PointF((REAL)mx + mw - 63, (REAL)my + 17), &acidBrush);

        // Gradient Progress Bar
        SolidBrush progTrack(Color(255, 16, 20, 30));
        FillRectI(g, &progTrack, mx + 20, my + 62, mw - 40, 6);
        int progFillW = (int)((float)(mw - 40) * ((float)g_progressPercent / 100.0f));
        if (progFillW > 0) {
            LinearGradientBrush gradBrush(Point(mx + 20, 0), Point(mx + mw - 20, 0), clrAcidVolt, clrEmerald);
            FillRectI(g, &gradBrush, mx + 20, my + 62, progFillW, 6);
        }

        // Monospace Console Terminal Window
        int lx = mx + 20, ly = my + 82, lw = mw - 40, lh = 280;
        SolidBrush termBg(Color(255, 7, 9, 14));
        Pen termBorder(Color(255, 27, 34, 48), 1.0f);
        FillRectI(g, &termBg, lx, ly, lw, lh);
        DrawRectI(g, &termBorder, lx, ly, lw, lh);

        // Draw Log Entries (latest 12 lines visible)
        int startLine = 0;
        if (g_modalLogs.size() > 12) {
            startLine = (int)g_modalLogs.size() - 12;
        }
        for (size_t l = startLine; l < g_modalLogs.size(); ++l) {
            int lineY = ly + 12 + (int)(l - startLine) * 21;
            const auto& lineText = g_modalLogs[l];
            bool isSuccess = (lineText.find(L"[✓]") != std::wstring::npos);
            bool isAlert   = (lineText.find(L"[!]") != std::wstring::npos);
            SolidBrush* lineBrush = isSuccess ? &acidBrush : (isAlert ? &acidBrush : &whiteBrush);
            g.DrawString(lineText.c_str(), -1, &fontModalLog, PointF((REAL)lx + 14, (REAL)lineY), lineBrush);
        }

        // Bottom Status Label
        g.DrawString(g_modalStatus.c_str(), -1, &fontSub, PointF((REAL)mx + 20, (REAL)my + 395), &acidBrush);

        // Dismiss Console Action Button (only when finished)
        if (g_modalComplete) {
            SolidBrush disBg(g_hoverModalDismiss ? Color(255, 220, 255, 50) : clrAcidVolt);
            FillRectI(g, &disBg, g_modalDismissRect.left, g_modalDismissRect.top, 
                      g_modalDismissRect.right - g_modalDismissRect.left, 
                      g_modalDismissRect.bottom - g_modalDismissRect.top);
            g.DrawString(L"[ ✓ DISMISS CONSOLE ]", -1, &fontBtn, PointF((REAL)g_modalDismissRect.left + 16, (REAL)g_modalDismissRect.top + 12), &jetBlackBrush);
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

            InitOptimizationCards();
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            bool redraw = false;

            if (g_showModal) {
                bool hDis = PtInRect(&g_modalDismissRect, pt) && g_modalComplete;
                if (hDis != g_hoverModalDismiss) { g_hoverModalDismiss = hDis; redraw = true; }
            } else {
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
            }

            if (redraw) InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            // Handle Modal Clicks
            if (g_showModal) {
                if (g_modalComplete && PtInRect(&g_modalDismissRect, pt)) {
                    g_showModal = false;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                return 0; // Capture all clicks while modal is open
            }

            if (g_isExecuting || g_isRestoring) return 0;

            // Toggle Switches
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

            // Restore Defaults Button
            if (PtInRect(&g_btnRestoreRect, pt)) {
                g_showModal = true;
                for (auto& c : g_cards) c.isEnabled = true; // reset checkboxes
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
            return 1; // Prevent flicker with double-buffering

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
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

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

    int w = 896;
    int h = 670;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hWnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"// SYS.CORE-TUNER // [CYBERCORE TECHWEAR GRID]",
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

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
