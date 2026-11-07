#pragma once
#include <windows.h>
#include <gdiplus.h>

/**
 * @file brutalist_theme.h
 * @brief Neo-Brutalist Technical Studio Theme for Win32 / GDI+
 * 
 * Architecture:
 * - High-contrast monochromatic paper canvas and pitch-black ink borders.
 * - Solid 2.5D hard offset drop shadows (zero blur, pure geometric displacement).
 * - Canary yellow (#FFE600) visual accent badges and indicators.
 * - Reusable by any native C++ desktop window.
 */

namespace BrutalistTheme {
    using namespace Gdiplus;

    // Palette Tokens
    inline Color Canvas()       { return Color(255, 244, 244, 240); } // #F4F4F0
    inline Color Paper()        { return Color(255, 250, 250, 250); } // #FAFAFA
    inline Color Ink()          { return Color(255, 18, 18, 18); }    // #121212
    inline Color CanaryYellow() { return Color(255, 255, 230, 0); }   // #FFE600
    inline Color GrayMuted()    { return Color(255, 110, 110, 110); } // #6E6E6E
    inline Color White()        { return Color(255, 255, 255, 255); }

    /**
     * @brief Render a 2.5D Neo-Brutalist rectangular container with solid hard drop shadow.
     */
    inline void DrawBrutalistCard(Graphics& g, int x, int y, int w, int h, int shadowDist, 
                                  Color fillClr, Color borderClr, Color shadowClr, float borderWidth = 2.0f) {
        // 1. Draw hard solid offset shadow
        if (shadowDist > 0) {
            SolidBrush shadowBrush(shadowClr);
            g.FillRectangle(&shadowBrush, x + shadowDist, y + shadowDist, w, h);
        }

        // 2. Draw card body
        SolidBrush fillBrush(fillClr);
        g.FillRectangle(&fillBrush, x, y, w, h);

        // 3. Draw heavy solid border
        Pen borderPen(borderClr, borderWidth);
        g.DrawRectangle(&borderPen, x, y, w, h);
    }

    /**
     * @brief Draw a 45-degree hazard-striped progress bar.
     */
    inline void DrawHazardBar(Graphics& g, int x, int y, int w, int h, int percent) {
        // Outer box
        DrawBrutalistCard(g, x, y, w, h, 0, Paper(), Ink(), Ink(), 2.0f);

        int fillW = (int)((float)w * ((float)percent / 100.0f));
        if (fillW <= 0) return;

        // Clip to fill area
        Region oldClip;
        g.GetClip(&oldClip);
        Rect clipRect(x + 2, y + 2, fillW - 4, h - 4);
        g.SetClip(clipRect);

        // Fill background yellow
        SolidBrush yBrush(CanaryYellow());
        g.FillRectangle(&yBrush, x + 2, y + 2, fillW - 4, h - 4);

        // Draw diagonal black hazard lines
        Pen stripePen(Ink(), 4.0f);
        int step = 12;
        for (int sx = x - h; sx < x + fillW + h; sx += step) {
            g.DrawLine(&stripePen, sx, y + h, sx + h, y);
        }

        g.SetClip(&oldClip);
    }
}
