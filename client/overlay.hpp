// overlay.hpp -- the transparent window the client draws on, and the one function that puts pixels in it.
//
// The interesting decision here is that C++ does NOT draw anything. Java hands us a finished image once
// a frame and we put it on the screen. That is the opposite of how most clients do it, and it is the
// reason a plugin can draw its overlay with ordinary Java2D -- shapes, alpha, antialiased text, fonts --
// instead of a handful of primitives someone had to expose one at a time.
//
// The cost is one memcpy of a full-screen image per frame. At 1920x1080 that is 8 MB, which sounds
// alarming and takes well under a millisecond. We were already blitting the same number of pixels when
// the drawing was done in C++.
//
// WHY A SEPARATE WINDOW instead of hooking the game's renderer: hooking needs a detour library, a
// graphics API to get right, and it crashes inside someone else's render loop when you get it wrong. A
// layered window is a page of code you can read in one sitting. The cost is that it will not appear in
// screenshots or recordings, and it can flicker. That trade is deliberate.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace kk::overlay {

inline HWND    g_hwnd = nullptr;
inline HDC     g_memDc = nullptr;
inline HBITMAP g_bitmap = nullptr;
inline void*   g_pixels = nullptr;      // the DIB's own memory, ARGB, top-down
inline int     g_width = 0, g_height = 0;

/// Throw away the current back buffer. Safe to call when there isn't one.
inline void releaseSurface() {
    if (g_memDc)   { DeleteDC(g_memDc);      g_memDc = nullptr; }
    if (g_bitmap)  { DeleteObject(g_bitmap); g_bitmap = nullptr; }
    g_pixels = nullptr;
    g_width = g_height = 0;
}

/// Make sure we have a back buffer of exactly this size. Recreates it when the game window resizes.
inline bool ensureSurface(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (g_memDc && w == g_width && h == g_height) return true;
    releaseSurface();

    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;          // negative = top-down, matching how Java lays out pixels
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    g_memDc  = CreateCompatibleDC(screen);
    g_bitmap = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_pixels, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!g_memDc || !g_bitmap || !g_pixels) { releaseSurface(); return false; }
    SelectObject(g_memDc, g_bitmap);
    g_width = w;
    g_height = h;
    return true;
}

/// Copy `src` (w*h pixels of premultiplied ARGB) into the window and show it.
///
/// PREMULTIPLIED is not optional. UpdateLayeredWindow interprets the colour channels as already scaled
/// by alpha; hand it straight ARGB and every semi-transparent pixel comes out too bright, with pale
/// halos around text. Java has a pixel format for exactly this -- TYPE_INT_ARGB_PRE -- so the Java side
/// draws into one of those and no conversion happens anywhere.
inline void present(const void* src, int w, int h) {
    if (!g_hwnd || !src) return;
    if (!ensureSurface(w, h)) return;

    std::memcpy(g_pixels, src, static_cast<std::size_t>(w) * h * 4);

    POINT         srcPt{ 0, 0 };
    SIZE          size{ w, h };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(g_hwnd, screen, nullptr, &size, g_memDc, &srcPt, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

/// Move and resize the overlay to sit exactly on top of `game`'s client area.
inline void followWindow(HWND game) {
    if (!g_hwnd || !game) return;
    RECT r{};
    GetClientRect(game, &r);
    POINT tl{ r.left, r.top };
    ClientToScreen(game, &tl);
    SetWindowPos(g_hwnd, HWND_TOPMOST, tl.x, tl.y, r.right - r.left, r.bottom - r.top,
                 SWP_NOACTIVATE);
}

}  // namespace kk::overlay
