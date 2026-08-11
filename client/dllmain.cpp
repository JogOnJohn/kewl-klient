// dllmain.cpp -- KewlKlient's native half.
//
// Injected into the game, it does four things and then gets out of the way:
//   1. starts a Java VM and registers the natives (jvm.hpp),
//   2. finds the game window,
//   3. puts a transparent always-on-top window over it (overlay.hpp),
//   4. calls kewl.KewlKlient.tick() about thirty times a second.
//
// Notice what is NOT here any more: drawing. Java renders the whole overlay into an image and hands it
// back through present(). Everything you would actually want to CHANGE lives in java/.
//
// The one exception is the error path at the bottom. If Java never came up, Java cannot tell you why --
// so a few lines of GDI put the reason on screen. That is worth the duplication; a client that fails
// silently is a client nobody can fix.
#include <windows.h>
#include <string>
#include "game.hpp"
#include "overlay.hpp"
#include "jvm.hpp"

namespace {

HWND        g_game = nullptr;
std::string g_javaError;      // shown natively if the VM never started

// ------------------------------------------------------------------------------------------------
// Find the game's window: the biggest visible top-level window this process owns.
// ------------------------------------------------------------------------------------------------
BOOL CALLBACK pickWindow(HWND h, LPARAM lp) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r{};
    GetClientRect(h, &r);
    long area = (r.right - r.left) * (r.bottom - r.top);
    auto* best = reinterpret_cast<std::pair<HWND, long>*>(lp);
    if (area > best->second) *best = { h, area };
    return TRUE;
}

HWND findGameWindow() {
    std::pair<HWND, long> best{ nullptr, 0 };
    EnumWindows(pickWindow, reinterpret_cast<LPARAM>(&best));
    return best.first;
}

// ------------------------------------------------------------------------------------------------
// The only drawing C++ still does: telling you why Java is not drawing.
// ------------------------------------------------------------------------------------------------
void renderJavaError(int w, int h) {
    if (!kk::overlay::ensureSurface(w, h)) return;

    // Start from fully transparent, then paint an opaque panel. Every pixel we touch needs alpha 255
    // or UpdateLayeredWindow will treat it as invisible.
    std::memset(kk::overlay::g_pixels, 0, static_cast<std::size_t>(w) * h * 4);

    HDC dc = kk::overlay::g_memDc;
    RECT panel{ 10, 10, 560, 84 };
    HBRUSH bg = CreateSolidBrush(RGB(24, 24, 28));
    FillRect(dc, &panel, bg);
    DeleteObject(bg);

    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontA(15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FF_DONTCARE, "Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);

    SetTextColor(dc, RGB(255, 120, 120));
    TextOutA(dc, 22, 20, "KewlKlient: Java did not start", 30);
    SetTextColor(dc, RGB(210, 210, 215));
    TextOutA(dc, 22, 42, g_javaError.c_str(), static_cast<int>(g_javaError.size()));
    SetTextColor(dc, RGB(140, 140, 150));
    TextOutA(dc, 22, 60, "check java= in kewlklient.ini", 29);

    SelectObject(dc, oldFont);
    DeleteObject(font);

    // GDI text leaves the alpha byte at zero, which would make everything we just drew invisible.
    // Force the panel opaque.
    auto* px = static_cast<std::uint32_t*>(kk::overlay::g_pixels);
    for (int y = panel.top; y < panel.bottom && y < h; ++y) {
        for (int x = panel.left; x < panel.right && x < w; ++x) {
            px[static_cast<std::size_t>(y) * w + x] |= 0xFF000000u;
        }
    }

    POINT         srcPt{ 0, 0 };
    SIZE          size{ w, h };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(kk::overlay::g_hwnd, screen, nullptr, &size, dc, &srcPt, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

// ------------------------------------------------------------------------------------------------
// Keys. Edge-detected, so holding a key toggles once. All of them go to Java as a bitmask -- the
// native side has no opinion about what F1 means any more, because the plugins decide that.
// ------------------------------------------------------------------------------------------------
int pollKeys() {
    static bool prev[8] = {};
    const int vks[8] = { VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8 };
    int mask = 0;
    for (int i = 0; i < 8; ++i) {
        bool down = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
        if (down && !prev[i]) mask |= 1 << i;      // F1 -> bit 0
        prev[i] = down;
    }
    return mask;
}

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

std::wstring iniString(const std::wstring& ini, const wchar_t* key, const wchar_t* fallback) {
    wchar_t buf[MAX_PATH]{};
    GetPrivateProfileStringW(L"kewlklient", key, fallback, buf, MAX_PATH, ini.c_str());
    return buf;
}

DWORD WINAPI run(LPVOID module) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(static_cast<HMODULE>(module), path, MAX_PATH);
    std::wstring dir(path);
    dir.resize(dir.find_last_of(L'\\'));
    std::wstring ini = dir + L"\\kewlklient.ini";

    // Find the game window BEFORE starting Java: the Java side asks for the viewport as soon as it
    // starts, and a null window there would have it build a zero-sized image.
    for (int i = 0; i < 600 && !g_game; ++i) { g_game = findGameWindow(); Sleep(100); }
    if (!g_game) return 0;
    kk::g_gameWindow = g_game;

    std::wstring javaHome = iniString(ini, L"java", L"");
    std::wstring jar      = dir + L"\\kewlklient.jar";
    if (javaHome.empty()) {
        g_javaError = "java= is not set in kewlklient.ini";
    } else if (!kk::startJvm(javaHome, jar, g_javaError)) {
        // g_javaError already says what went wrong.
    }

    WNDCLASSEXW wc{ sizeof wc };
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = static_cast<HMODULE>(module);
    wc.lpszClassName = L"KewlKlientOverlay";
    RegisterClassExW(&wc);

    // WS_EX_TRANSPARENT is what makes clicks fall through to the game underneath. Without it the
    // overlay eats every click and the game becomes unplayable, which is a memorable ten minutes.
    kk::overlay::g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!kk::overlay::g_hwnd) return 0;
    ShowWindow(kk::overlay::g_hwnd, SW_SHOWNOACTIVATE);

    MSG msg{};
    while (IsWindow(g_game)) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);

        kk::overlay::followWindow(g_game);

        if (g_javaError.empty()) {
            // Java draws and presents. We do nothing but ask.
            kk::tickJvm(pollKeys());
        } else {
            RECT r{};
            GetClientRect(g_game, &r);
            renderJavaError(r.right - r.left, r.bottom - r.top);
        }

        Sleep(33);                                     // ~30 fps is plenty for an overlay
    }

    kk::overlay::releaseSurface();
    DestroyWindow(kk::overlay::g_hwnd);
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        CreateThread(nullptr, 0, run, module, 0, nullptr);
    }
    return TRUE;
}
