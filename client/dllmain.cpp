// dllmain.cpp -- KewlKlient's native half.
//
// Injected into the game, it:
//   1. starts a Java VM and hands it four native methods (see jvm.hpp),
//   2. puts a transparent always-on-top window over the game and draws boxes on it,
//   3. calls kewl.KewlKlient.tick() about thirty times a second so Java plugins run.
//
// The ESP is drawn here rather than in Java only because drawing is the one thing C++ is genuinely
// better placed to do -- it already has the pointers. Everything you would actually want to CHANGE
// lives in java/.
//
// WHY A SEPARATE WINDOW instead of hooking the game's renderer: hooking needs a detour library, a
// graphics API to get right, and it crashes inside someone else's render loop when you get it wrong. A
// layered window is forty lines you can read in one sitting. It looks worse and it will not appear in
// screenshots or recordings. That trade is deliberate -- this is a client you are meant to learn from.
#include <windows.h>
#include <string>
#include "game.hpp"
#include "jvm.hpp"

namespace {

bool g_espPlayers = true;
bool g_espNpcs    = true;
HWND g_overlay    = nullptr;
HWND g_game       = nullptr;
std::string g_javaError;      // shown on the panel if the VM never came up

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
// Drawing
// ------------------------------------------------------------------------------------------------
void text(HDC dc, int x, int y, COLORREF col, const std::string& s) {
    SetTextColor(dc, RGB(0, 0, 0));                    // cheap shadow so it reads on any background
    TextOutA(dc, x + 1, y + 1, s.c_str(), static_cast<int>(s.size()));
    SetTextColor(dc, col);
    TextOutA(dc, x, y, s.c_str(), static_cast<int>(s.size()));
}

void box(HDC dc, HPEN pen, float cx, float cy) {
    HGDIOBJ old = SelectObject(dc, pen);
    const int w = 14, h = 26;
    int l = static_cast<int>(cx) - w / 2, t = static_cast<int>(cy) - h;
    MoveToEx(dc, l, t, nullptr);
    LineTo(dc, l + w, t);  LineTo(dc, l + w, t + h);  LineTo(dc, l, t + h);  LineTo(dc, l, t);
    SelectObject(dc, old);
}

void render(HDC dc, int width, int height) {
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));        // our transparent colour
    RECT full{ 0, 0, width, height };
    FillRect(dc, &full, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    // -- the panel. This is the entire UI, and it is meant to be boring.
    int y = 8;
    text(dc, 8, y, RGB(255, 255, 255), "KewlKlient");                              y += 16;
    text(dc, 8, y, RGB(170, 170, 170), "F1 players  F2 npcs   F5..F8 -> plugins"); y += 16;
    text(dc, 8, y, g_espPlayers ? RGB(120, 255, 120) : RGB(140, 140, 140),
         std::string("players ") + (g_espPlayers ? "ON" : "off"));                 y += 14;
    text(dc, 8, y, g_espNpcs ? RGB(120, 255, 120) : RGB(140, 140, 140),
         std::string("npcs    ") + (g_espNpcs ? "ON" : "off"));                    y += 16;

    if (!g_javaError.empty()) {
        text(dc, 8, y, RGB(255, 140, 140), "java: " + g_javaError);                y += 14;
    } else {
        // Whatever the Java side wants to say, one line per plugin.
        std::string s = kk::jvmStatus();
        std::size_t start = 0;
        while (start < s.size()) {
            std::size_t nl = s.find('\n', start);
            if (nl == std::string::npos) nl = s.size();
            if (nl > start) text(dc, 8, y, RGB(200, 200, 255), s.substr(start, nl - start));
            y += 14;
            start = nl + 1;
        }
    }
    y += 4;

    if (!kk::clientObj()) {
        text(dc, 8, y, RGB(255, 180, 180), "waiting for the game to load...");
        return;
    }

    // -- the ESP
    HPEN players = CreatePen(PS_SOLID, 1, RGB(90, 200, 255));
    HPEN npcs    = CreatePen(PS_SOLID, 1, RGB(255, 220, 90));
    int me = kk::localPlayerUid();

    kk::forEachEntity([&](const kk::Entity& e) {
        if (e.uid == me) return;                       // do not box yourself
        bool player = kk::isPlayerUid(e.uid);
        if (player ? !g_espPlayers : !g_espNpcs) return;
        float sx = 0.f, sy = 0.f;
        if (!kk::project(e.sceneX, e.sceneY, sx, sy)) return;
        if (sx < 0 || sy < 0 || sx > width || sy > height) return;
        box(dc, player ? players : npcs, sx, sy);
    });

    DeleteObject(players);
    DeleteObject(npcs);
}

// ------------------------------------------------------------------------------------------------
// Keys. Edge-detected, so holding a key toggles once. F1/F2 are ours; F5..F8 go to Java as a bitmask.
// ------------------------------------------------------------------------------------------------
int pollKeys() {
    static bool prev[6] = {};
    const int vks[6] = { VK_F1, VK_F2, VK_F5, VK_F6, VK_F7, VK_F8 };
    int mask = 0;
    for (int i = 0; i < 6; ++i) {
        bool down = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
        bool edge = down && !prev[i];
        prev[i] = down;
        if (!edge) continue;
        if (i == 0) g_espPlayers = !g_espPlayers;
        else if (i == 1) g_espNpcs = !g_espNpcs;
        else mask |= 1 << (i - 2);                     // F5 -> bit 0
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

    // Start Java. If this fails the overlay still runs and shows why, which beats a silent no-op.
    std::wstring javaHome = iniString(ini, L"java", L"");
    std::wstring jar      = dir + L"\\kewlklient.jar";
    if (javaHome.empty()) {
        g_javaError = "java= not set in kewlklient.ini";
    } else if (!kk::startJvm(javaHome, jar, g_javaError)) {
        // g_javaError already says what went wrong.
    }

    for (int i = 0; i < 600 && !g_game; ++i) { g_game = findGameWindow(); Sleep(100); }
    if (!g_game) return 0;

    WNDCLASSEXW wc{ sizeof wc };
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = static_cast<HMODULE>(module);
    wc.lpszClassName = L"KewlKlientOverlay";
    RegisterClassExW(&wc);

    g_overlay = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_overlay) return 0;

    SetLayeredWindowAttributes(g_overlay, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_overlay, SW_SHOWNOACTIVATE);

    HFONT font = CreateFontA(14, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FF_DONTCARE, "Consolas");

    MSG msg{};
    while (IsWindow(g_game)) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);

        RECT r{};
        GetClientRect(g_game, &r);
        POINT tl{ r.left, r.top };
        ClientToScreen(g_game, &tl);
        int w = r.right - r.left, h = r.bottom - r.top;
        SetWindowPos(g_overlay, HWND_TOPMOST, tl.x, tl.y, w, h, SWP_NOACTIVATE);

        kk::tickJvm(pollKeys());

        // Draw off-screen then blit once, or it flickers badly.
        HDC screen = GetDC(g_overlay);
        HDC back = CreateCompatibleDC(screen);
        HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
        HGDIOBJ oldBmp = SelectObject(back, bmp);
        HGDIOBJ oldFnt = SelectObject(back, font);

        render(back, w, h);
        BitBlt(screen, 0, 0, w, h, back, 0, 0, SRCCOPY);

        SelectObject(back, oldFnt);
        SelectObject(back, oldBmp);
        DeleteObject(bmp);
        DeleteDC(back);
        ReleaseDC(g_overlay, screen);

        Sleep(33);                                     // ~30 fps is plenty for boxes
    }

    DeleteObject(font);
    DestroyWindow(g_overlay);
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
