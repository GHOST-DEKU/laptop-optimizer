#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <gdiplus.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;
using std::max;

// Structs for ICO file format
#pragma pack(push, 2)
struct ICONDIR {
    WORD idReserved;
    WORD idType;
    WORD idCount;
};

struct ICONDIRENTRY {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
};
#pragma pack(pop)

// Render a single resolution icon image using GDI+ vector graphics
std::vector<BYTE> RenderIconImage(int S) {
    Bitmap bmp(S, S, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    // Color definitions
    Color clrObsidian(255, 11, 14, 20);       // #0B0E14
    Color clrObsidianCenter(255, 20, 26, 38); // Subtle radial center
    Color clrBorderSlate(255, 30, 35, 48);    // #1E2330
    Color clrAcidVolt(255, 204, 255, 0);      // #CCFF00
    Color clrAcidGlow(90, 204, 255, 0);       // Acid bloom
    Color clrEmerald(255, 16, 185, 129);      // #10B981
    Color clrWhite(255, 255, 255, 255);       // Energy core

    REAL sf = (REAL)S;

    // 1. Techwear Chamfered Squircle Background
    REAL pad = sf * 0.04f;
    REAL chw = sf * 0.16f; // Chamfer cut size
    REAL x1 = pad, y1 = pad;
    REAL x2 = sf - pad, y2 = sf - pad;

    GraphicsPath badgePath;
    badgePath.AddLine(x1 + chw, y1, x2 - chw, y1);
    badgePath.AddLine(x2 - chw, y1, x2, y1 + chw);
    badgePath.AddLine(x2, y1 + chw, x2, y2 - chw);
    badgePath.AddLine(x2, y2 - chw, x2 - chw, y2);
    badgePath.AddLine(x2 - chw, y2, x1 + chw, y2);
    badgePath.AddLine(x1 + chw, y2, x1, y2 - chw);
    badgePath.AddLine(x1, y2 - chw, x1, y1 + chw);
    badgePath.AddLine(x1, y1 + chw, x1 + chw, y1);
    badgePath.CloseFigure();

    // Fill badge with deep techwear gradient
    PathGradientBrush pgb(&badgePath);
    pgb.SetCenterPoint(PointF(sf * 0.5f, sf * 0.45f));
    pgb.SetCenterColor(clrObsidianCenter);
    int count = 1;
    Color surroundColor = clrObsidian;
    pgb.SetSurroundColors(&surroundColor, &count);
    g.FillPath(&pgb, &badgePath);

    // Outer Slate Rim
    Pen slatePen(clrBorderSlate, max(1.0f, sf * 0.04f));
    g.DrawPath(&slatePen, &badgePath);

    // Inner Hairline Acid-Volt Rim
    GraphicsPath innerPath;
    REAL ip = pad + max(1.5f, sf * 0.04f);
    REAL ich = chw * 0.85f;
    REAL ix1 = ip, iy1 = ip;
    REAL ix2 = sf - ip, iy2 = sf - ip;
    innerPath.AddLine(ix1 + ich, iy1, ix2 - ich, iy1);
    innerPath.AddLine(ix2 - ich, iy1, ix2, iy1 + ich);
    innerPath.AddLine(ix2, iy1 + ich, ix2, iy2 - ich);
    innerPath.AddLine(ix2, iy2 - ich, ix2 - ich, iy2);
    innerPath.AddLine(ix2 - ich, iy2, ix1 + ich, iy2);
    innerPath.AddLine(ix1 + ich, iy2, ix1, iy2 - ich);
    innerPath.AddLine(ix1, iy2 - ich, ix1, iy1 + ich);
    innerPath.AddLine(ix1, iy1 + ich, ix1 + ich, iy1);
    innerPath.CloseFigure();

    Pen acidHairline(clrAcidVolt, max(1.0f, sf * 0.02f));
    g.DrawPath(&acidHairline, &innerPath);

    // Corner Alignment Techwear Crosshairs (at sizes >= 32)
    if (S >= 32) {
        Pen pinPen(clrAcidVolt, max(1.0f, sf * 0.015f));
        REAL pinDist = sf * 0.14f;
        REAL pinLen = sf * 0.04f;
        // Top-left
        g.DrawLine(&pinPen, pinDist - pinLen, pinDist, pinDist + pinLen, pinDist);
        g.DrawLine(&pinPen, pinDist, pinDist - pinLen, pinDist, pinDist + pinLen);
        // Top-right
        g.DrawLine(&pinPen, sf - pinDist - pinLen, pinDist, sf - pinDist + pinLen, pinDist);
        g.DrawLine(&pinPen, sf - pinDist, pinDist - pinLen, sf - pinDist, pinDist + pinLen);
        // Bottom-left
        g.DrawLine(&pinPen, pinDist - pinLen, sf - pinDist, pinDist + pinLen, sf - pinDist);
        g.DrawLine(&pinPen, pinDist, sf - pinDist - pinLen, pinDist, sf - pinDist + pinLen);
        // Bottom-right
        g.DrawLine(&pinPen, sf - pinDist - pinLen, sf - pinDist, sf - pinDist + pinLen, sf - pinDist);
        g.DrawLine(&pinPen, sf - pinDist, sf - pinDist - pinLen, sf - pinDist, sf - pinDist + pinLen);
    }

    // 2. Overclock Speed Governor Arc (270° Tachometer Ring)
    REAL arcRadius = sf * 0.30f;
    REAL arcBoxX = sf * 0.5f - arcRadius;
    REAL arcBoxY = sf * 0.52f - arcRadius;
    REAL arcBoxW = arcRadius * 2.0f;

    Pen tachPen(Color(160, 16, 185, 129), max(1.5f, sf * 0.035f));
    tachPen.SetDashStyle(DashStyleDash);
    g.DrawArc(&tachPen, arcBoxX, arcBoxY, arcBoxW, arcBoxW, 135.0f, 270.0f);

    // 3. High-Voltage Turbo Lightning Bolt
    // Defining 2-facet precision lightning geometry
    PointF ptTop(sf * 0.56f, sf * 0.16f);
    PointF ptRight(sf * 0.68f, sf * 0.44f);
    PointF ptRightKnee(sf * 0.50f, sf * 0.48f);
    PointF ptBottom(sf * 0.42f, sf * 0.84f);
    PointF ptLeft(sf * 0.30f, sf * 0.56f);
    PointF ptLeftKnee(sf * 0.48f, sf * 0.52f);

    // A. Outer Glow Halo
    GraphicsPath fullBolt;
    fullBolt.AddLine(ptTop, ptRight);
    fullBolt.AddLine(ptRight, ptRightKnee);
    fullBolt.AddLine(ptRightKnee, ptBottom);
    fullBolt.AddLine(ptBottom, ptLeft);
    fullBolt.AddLine(ptLeft, ptLeftKnee);
    fullBolt.AddLine(ptLeftKnee, ptTop);
    fullBolt.CloseFigure();

    Pen glowPen(clrAcidGlow, max(2.0f, sf * 0.08f));
    glowPen.SetLineJoin(LineJoinRound);
    g.DrawPath(&glowPen, &fullBolt);

    // B. Left Facet (High-Voltage Acid-Volt)
    GraphicsPath leftFacet;
    leftFacet.AddLine(ptTop, ptRightKnee);
    leftFacet.AddLine(ptRightKnee, ptBottom);
    leftFacet.AddLine(ptBottom, ptLeft);
    leftFacet.AddLine(ptLeft, ptLeftKnee);
    leftFacet.AddLine(ptLeftKnee, ptTop);
    leftFacet.CloseFigure();

    SolidBrush acidBrush(clrAcidVolt);
    g.FillPath(&acidBrush, &leftFacet);

    // C. Right Facet (Speed Emerald)
    GraphicsPath rightFacet;
    rightFacet.AddLine(ptTop, ptRight);
    rightFacet.AddLine(ptRight, ptRightKnee);
    rightFacet.AddLine(ptRightKnee, ptBottom);
    rightFacet.CloseFigure();

    SolidBrush emeraldBrush(clrEmerald);
    g.FillPath(&emeraldBrush, &rightFacet);

    // D. Center Energy Spine Core (Bright White)
    Pen spinePen(clrWhite, max(1.0f, sf * 0.025f));
    g.DrawLine(&spinePen, ptTop, ptRightKnee);
    g.DrawLine(&spinePen, ptRightKnee, ptBottom);

    // Extract raw 32-bit BGRA pixels from bitmap
    BitmapData bmpData;
    Rect rc(0, 0, S, S);
    bmp.LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bmpData);

    int rowBytes = S * 4;
    int imgSize = rowBytes * S;
    int maskRowBytes = ((S + 31) / 32) * 4;
    int maskSize = maskRowBytes * S;

    // Prepare DIB structure: BITMAPINFOHEADER + bottom-up BGRA pixels + 1-bit AND mask
    BITMAPINFOHEADER bih = { 0 };
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = S;
    bih.biHeight = S * 2; // Required doubled height in ICO DIB
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = imgSize;

    std::vector<BYTE> data;
    data.resize(sizeof(BITMAPINFOHEADER) + imgSize + maskSize, 0);

    memcpy(data.data(), &bih, sizeof(BITMAPINFOHEADER));

    // Copy BGRA pixels in bottom-up order
    BYTE* destPixels = data.data() + sizeof(BITMAPINFOHEADER);
    for (int y = 0; y < S; ++y) {
        int srcY = S - 1 - y; // Bottom-up
        BYTE* srcRow = (BYTE*)bmpData.Scan0 + (srcY * bmpData.Stride);
        memcpy(destPixels + (y * rowBytes), srcRow, rowBytes);
    }

    // Mask is all zeros (transparent alpha in 32bpp)
    bmp.UnlockBits(&bmpData);
    return data;
}

int main() {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    std::vector<int> sizes = { 16, 24, 32, 48, 64, 128, 256 };
    std::vector<std::vector<BYTE>> imagesData;

    for (int s : sizes) {
        imagesData.push_back(RenderIconImage(s));
    }

    // Build ICONDIR
    ICONDIR dir;
    dir.idReserved = 0;
    dir.idType = 1; // 1 = Icon (.ico)
    dir.idCount = (WORD)sizes.size();

    std::vector<ICONDIRENTRY> entries;
    DWORD currentOffset = sizeof(ICONDIR) + (DWORD)(sizes.size() * sizeof(ICONDIRENTRY));

    for (size_t i = 0; i < sizes.size(); ++i) {
        int s = sizes[i];
        DWORD dataLen = (DWORD)imagesData[i].size();

        ICONDIRENTRY entry;
        entry.bWidth = (s >= 256) ? 0 : (BYTE)s;
        entry.bHeight = (s >= 256) ? 0 : (BYTE)s;
        entry.bColorCount = 0;
        entry.bReserved = 0;
        entry.wPlanes = 1;
        entry.wBitCount = 32;
        entry.dwBytesInRes = dataLen;
        entry.dwImageOffset = currentOffset;

        entries.push_back(entry);
        currentOffset += dataLen;
    }

    // Write to app_icon.ico
    std::string outPath = "src/app_icon.ico";
    std::ofstream f(outPath, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "Failed to open output file " << outPath << std::endl;
        return 1;
    }

    f.write((char*)&dir, sizeof(dir));
    for (const auto& e : entries) {
        f.write((char*)&e, sizeof(e));
    }
    for (const auto& d : imagesData) {
        f.write((char*)d.data(), d.size());
    }
    f.close();

    std::cout << "Successfully generated multi-resolution Cybercore Techwear icon at " << outPath << std::endl;
    std::cout << "Layers: 16x16, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256." << std::endl;

    GdiplusShutdown(gdiplusToken);
    return 0;
}
