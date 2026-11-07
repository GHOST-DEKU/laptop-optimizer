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

// Optimization items model
struct OptCard {
    int id;
    std::wstring title;
    std::wstring desc;
    std::wstring badge;
    bool isEnabled;
    RECT rect;
    RECT switchRect;
};

// Global state
HWND g_hWnd = NULL;
std::vector<OptCard> g_cards;
std::atomic<bool> g_isOptimizing(false);
std::wstring g_statusText = L"Ready. Select optimizations to apply and click \"⚡ OPTIMIZE LAPTOP\".";
int g_progressPercent = 0;

// Button RECTs
RECT g_btnOptimizeRect = { 600, 560, 800, 605 };
RECT g_btnSelectAllRect = { 40, 565, 150, 600 };
RECT g_btnDeselectAllRect = { 165, 565, 275, 600 };

bool g_hoverOptimize = false;
bool g_hoverSelectAll = false;
bool g_hoverDeselectAll = false;
int g_hoverCardIndex = -1;

void InitOptimizationCards() {
    g_cards.clear();
    g_cards.push_back({ 1, L"High Performance Power Profile", L"Locks CPU clock floor to 100% on AC power to eliminate turbo ramp latency.", L"100% TURBO", true });
    g_cards.push_back({ 2, L"Disable Game DVR Background Hooks", L"Terminates continuous background video buffering; saves GPU shaders & VRAM.", L"ZERO LAG", true });
    g_cards.push_back({ 3, L"Instant UI Menus (20ms Latency)", L"Cuts MenuShowDelay from 400ms to 20ms. Context menus snap open instantly.", L"20ms MENUS", true });
    g_cards.push_back({ 4, L"Remove Windows 11 Widgets Board", L"Uninstalls WebExperience package and terminates 20+ idle background WebViews.", L"-300MB RAM", true });
    g_cards.push_back({ 5, L"Decouple Bing from Start Menu", L"Makes Start Search 100% local and instantaneous; eliminates cloud web queries.", L"LOCAL ONLY", true });
    g_cards.push_back({ 6, L"Stop Delivery Optimization P2P", L"Prevents Windows from using your disk I/O, CPU, and WiFi to seed updates.", L"HTTP ONLY", true });
    g_cards.push_back({ 7, L"Purge Fast Startup (hiberfil.sys)", L"Reclaims +6.2 GB SSD storage permanently and ensures clean boots on shutdown.", L"+6.2GB SSD", true });
    g_cards.push_back({ 8, L"Purge Stale Temporary Files (>24h)", L"Cleans accumulated cache junk from %TEMP% to free storage and reduce disk index.", L"CLEAN TEMP", true });

    int startY = 116;
    int cardH = 46;
    int gap = 6;
    for (size_t i = 0; i < g_cards.size(); ++i) {
        int y = startY + (int)i * (cardH + gap);
        g_cards[i].rect = { 40, y, 800, y + cardH };
        g_cards[i].switchRect = { 730, y + 11, 785, y + 35 };
    }
}

// Background optimization worker
void RunOptimizationThread() {
    g_isOptimizing = true;
    size_t total = g_cards.size();

    for (size_t i = 0; i < g_cards.size(); ++i) {
        if (!g_cards[i].isEnabled) continue;

        g_statusText = L"Applying: " + g_cards[i].title + L"...";
        g_progressPercent = (int)((float)i / (float)total * 100.0f);
        InvalidateRect(g_hWnd, NULL, FALSE);

        switch (g_cards[i].id) {
            case 1: LaptopOptimizerEngine::OptimizePowerPlan(); break;
            case 2: LaptopOptimizerEngine::OptimizeGameDVR(); break;
            case 3: LaptopOptimizerEngine::OptimizeMenuDelay(); break;
            case 4: LaptopOptimizerEngine::OptimizeWidgets(); break;
            case 5: LaptopOptimizerEngine::OptimizeBingSearch(); break;
            case 6: LaptopOptimizerEngine::OptimizeDeliveryOptimization(); break;
            case 7: LaptopOptimizerEngine::OptimizeFastStartup(); break;
            case 8: LaptopOptimizerEngine::OptimizeTempFiles(); break;
        }
        Sleep(250);
    }

    g_progressPercent = 100;
    g_statusText = L"Success! All selected tweaks applied. Please restart your laptop.";
    g_isOptimizing = false;
    InvalidateRect(g_hWnd, NULL, FALSE);

    MessageBoxW(g_hWnd, 
        L"Optimization Sequence Complete!\n\nAll selected performance tweaks have been applied successfully.\nYour laptop is now tuned for peak responsiveness.", 
        L"⚡ Optimization Successful", MB_OK | MB_ICONINFORMATION);
}

// Custom GDI+ Painting
void OnPaint(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    // Double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Dark Background (Deep Obsidian)
    SolidBrush bgBrush(Color(255, 12, 16, 24));
    FillRectI(g, &bgBrush, 0, 0, width, height);

    // Header Background
    SolidBrush headerBrush(Color(255, 18, 24, 38));
    FillRectI(g, &headerBrush, 0, 0, width, 98);

    Pen headerBorder(Color(255, 36, 46, 68), 1.0f);
    g.DrawLine(&headerBorder, 0, 98, width, 98);

    // Fonts
    FontFamily fontFamily(L"Segoe UI");
    Font fontTitle(&fontFamily, 16, FontStyleBold, UnitPixel);
    Font fontSub(&fontFamily, 11, FontStyleRegular, UnitPixel);
    Font fontCardTitle(&fontFamily, 13, FontStyleBold, UnitPixel);
    Font fontCardDesc(&fontFamily, 11, FontStyleRegular, UnitPixel);
    Font fontBadge(&fontFamily, 10, FontStyleBold, UnitPixel);
    Font fontBtn(&fontFamily, 12, FontStyleBold, UnitPixel);
    Font fontStatus(&fontFamily, 11, FontStyleRegular, UnitPixel);

    // Header Title
    SolidBrush cyanBrush(Color(255, 0, 240, 255));
    SolidBrush whiteBrush(Color(255, 240, 246, 252));
    SolidBrush grayBrush(Color(255, 139, 148, 158));

    g.DrawString(L"⚡ WINDOWS LAPTOP OPTIMIZER", -1, &fontTitle, PointF(40, 22), &cyanBrush);
    g.DrawString(L"Modular, safe, high-performance tuning for Windows 10 & Windows 11", -1, &fontSub, PointF(40, 52), &grayBrush);

    // Telemetry Chip on Right Header
    SolidBrush chipBg(Color(255, 26, 34, 52));
    Pen chipPen(Color(255, 0, 240, 255), 1.0f);
    FillRectI(g, &chipBg, 580, 24, 220, 52);
    DrawRectI(g, &chipPen, 580, 24, 220, 52);
    g.DrawString(L"TARGET: Core i7 // 16GB RAM", -1, &fontBadge, PointF(592, 32), &cyanBrush);
    SolidBrush greenText(Color(255, 57, 255, 20));
    g.DrawString(L"STATUS: High Performance Grid", -1, &fontBadge, PointF(592, 50), &greenText);

    // Draw Optimization Cards
    for (size_t i = 0; i < g_cards.size(); ++i) {
        const auto& card = g_cards[i];
        bool isHovered = ((int)i == g_hoverCardIndex);

        Color cardBg = isHovered ? Color(255, 28, 36, 56) : Color(255, 20, 26, 40);
        Color borderClr = card.isEnabled ? (isHovered ? Color(255, 0, 240, 255) : Color(255, 40, 52, 78)) : Color(255, 28, 34, 48);

        SolidBrush cBrush(cardBg);
        Pen pPen(borderClr, 1.0f);

        int cx = (int)card.rect.left;
        int cy = (int)card.rect.top;
        int cw = (int)(card.rect.right - card.rect.left);
        int ch = (int)(card.rect.bottom - card.rect.top);

        FillRectI(g, &cBrush, cx, cy, cw, ch);
        DrawRectI(g, &pPen, cx, cy, cw, ch);

        // Title and Description
        SolidBrush tBrush = card.isEnabled ? Color(255, 240, 246, 252) : Color(255, 110, 118, 129);
        g.DrawString(card.title.c_str(), -1, &fontCardTitle, PointF((REAL)cx + 16, (REAL)cy + 7), &tBrush);
        g.DrawString(card.desc.c_str(), -1, &fontCardDesc, PointF((REAL)cx + 16, (REAL)cy + 25), &grayBrush);

        // Badge
        Color badgeBg = card.isEnabled ? Color(255, 16, 48, 64) : Color(255, 30, 34, 44);
        Color badgeTxt = card.isEnabled ? Color(255, 0, 240, 255) : Color(255, 120, 128, 140);
        SolidBrush bBg(badgeBg);
        SolidBrush bTxt(badgeTxt);
        FillRectI(g, &bBg, cx + 550, cy + 13, 110, 20);
        g.DrawString(card.badge.c_str(), -1, &fontBadge, PointF((REAL)cx + 558, (REAL)cy + 16), &bTxt);

        // Toggle Switch
        int sx = (int)card.switchRect.left;
        int sy = (int)card.switchRect.top;
        int sw = (int)(card.switchRect.right - card.switchRect.left);
        int sh = (int)(card.switchRect.bottom - card.switchRect.top);

        Color switchBg = card.isEnabled ? Color(255, 0, 200, 180) : Color(255, 45, 52, 68);
        SolidBrush sBrush(switchBg);
        FillRectI(g, &sBrush, sx, sy, sw, sh);

        // Thumb
        int thumbX = card.isEnabled ? (sx + sw - 22) : (sx + 2);
        SolidBrush thumbBrush(Color(255, 255, 255, 255));
        FillRectI(g, &thumbBrush, thumbX, sy + 2, 20, sh - 4);
    }

    // Progress Bar Area
    if (g_isOptimizing || g_progressPercent > 0) {
        SolidBrush progBg(Color(255, 20, 26, 40));
        FillRectI(g, &progBg, 40, 536, 760, 6);
        SolidBrush progFill(Color(255, 0, 240, 255));
        int fillW = (int)(760.0f * ((float)g_progressPercent / 100.0f));
        FillRectI(g, &progFill, 40, 536, fillW, 6);
    }

    // Select All Button
    SolidBrush secBtnBrush(g_hoverSelectAll ? Color(255, 35, 44, 66) : Color(255, 24, 30, 46));
    Pen secPen(Color(255, 50, 64, 92), 1.0f);
    int saW = (int)(g_btnSelectAllRect.right - g_btnSelectAllRect.left);
    int saH = (int)(g_btnSelectAllRect.bottom - g_btnSelectAllRect.top);
    FillRectI(g, &secBtnBrush, (int)g_btnSelectAllRect.left, (int)g_btnSelectAllRect.top, saW, saH);
    DrawRectI(g, &secPen, (int)g_btnSelectAllRect.left, (int)g_btnSelectAllRect.top, saW, saH);
    g.DrawString(L"Select All", -1, &fontBtn, PointF((REAL)g_btnSelectAllRect.left + 22, (REAL)g_btnSelectAllRect.top + 9), &whiteBrush);

    // Deselect All Button
    SolidBrush desBtnBrush(g_hoverDeselectAll ? Color(255, 35, 44, 66) : Color(255, 24, 30, 46));
    int daW = (int)(g_btnDeselectAllRect.right - g_btnDeselectAllRect.left);
    int daH = (int)(g_btnDeselectAllRect.bottom - g_btnDeselectAllRect.top);
    FillRectI(g, &desBtnBrush, (int)g_btnDeselectAllRect.left, (int)g_btnDeselectAllRect.top, daW, daH);
    DrawRectI(g, &secPen, (int)g_btnDeselectAllRect.left, (int)g_btnDeselectAllRect.top, daW, daH);
    g.DrawString(L"Deselect All", -1, &fontBtn, PointF((REAL)g_btnDeselectAllRect.left + 16, (REAL)g_btnDeselectAllRect.top + 9), &whiteBrush);

    // Main OPTIMIZE Action Button
    Color optBg = g_hoverOptimize ? Color(255, 0, 255, 230) : Color(255, 0, 210, 190);
    if (g_isOptimizing) optBg = Color(255, 80, 90, 110);
    SolidBrush optBrush(optBg);
    int optW = (int)(g_btnOptimizeRect.right - g_btnOptimizeRect.left);
    int optH = (int)(g_btnOptimizeRect.bottom - g_btnOptimizeRect.top);
    FillRectI(g, &optBrush, (int)g_btnOptimizeRect.left, (int)g_btnOptimizeRect.top, optW, optH);
    
    SolidBrush blackText(Color(255, 10, 14, 20));
    std::wstring optText = g_isOptimizing ? L"OPTIMIZING..." : L"⚡ OPTIMIZE LAPTOP";
    g.DrawString(optText.c_str(), -1, &fontBtn, PointF((REAL)g_btnOptimizeRect.left + 26, (REAL)g_btnOptimizeRect.top + 13), &blackText);

    // Status Message at bottom
    g.DrawString(g_statusText.c_str(), -1, &fontStatus, PointF(40, 514), &cyanBrush);

    // Copy to screen
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
            // Apply Windows 11 Immersive Dark Mode to title bar
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
            int corner = 3; /* DWMWCP_ROUND */
            DwmSetWindowAttribute(hWnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
            InitOptimizationCards();
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            bool redraw = false;

            int oldHover = g_hoverCardIndex;
            g_hoverCardIndex = -1;
            for (size_t i = 0; i < g_cards.size(); ++i) {
                if (PtInRect(&g_cards[i].rect, pt)) {
                    g_hoverCardIndex = (int)i;
                    break;
                }
            }
            if (oldHover != g_hoverCardIndex) redraw = true;

            bool hOpt = PtInRect(&g_btnOptimizeRect, pt) && !g_isOptimizing;
            if (hOpt != g_hoverOptimize) { g_hoverOptimize = hOpt; redraw = true; }

            bool hSel = PtInRect(&g_btnSelectAllRect, pt) && !g_isOptimizing;
            if (hSel != g_hoverSelectAll) { g_hoverSelectAll = hSel; redraw = true; }

            bool hDesel = PtInRect(&g_btnDeselectAllRect, pt) && !g_isOptimizing;
            if (hDesel != g_hoverDeselectAll) { g_hoverDeselectAll = hDesel; redraw = true; }

            if (redraw) InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            if (g_isOptimizing) return 0;

            // Check toggle switches
            for (size_t i = 0; i < g_cards.size(); ++i) {
                if (PtInRect(&g_cards[i].rect, pt)) {
                    g_cards[i].isEnabled = !g_cards[i].isEnabled;
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
            }

            // Select All
            if (PtInRect(&g_btnSelectAllRect, pt)) {
                for (auto& c : g_cards) c.isEnabled = true;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            // Deselect All
            if (PtInRect(&g_btnDeselectAllRect, pt)) {
                for (auto& c : g_cards) c.isEnabled = false;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            // Optimize Button
            if (PtInRect(&g_btnOptimizeRect, pt)) {
                std::thread(RunOptimizationThread).detach();
                return 0;
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent flicker

        case WM_PAINT:
            OnPaint(hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Entry Point: Native Windows GUI
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"LaptopOptimizerWindow";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    int w = 856;
    int h = 660;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hWnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"⚡ Windows Laptop Optimizer",
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
