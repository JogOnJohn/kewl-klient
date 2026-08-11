// launcher/main.cpp -- one window, one button.
//
// Start the game yourself, log in, then press the button. It finds the game process and injects
// kewlklient.dll into it with LoadLibrary on a remote thread -- the oldest and most boring injection
// there is, in about thirty lines, so you can read all of it.
#include <windows.h>
#include <tlhelp32.h>
#include <string>

namespace {

constexpr wchar_t kTarget[] = L"osclient.exe";

HWND g_status = nullptr;

void say(const std::wstring& s) { if (g_status) SetWindowTextW(g_status, s.c_str()); }

/// PID of the first process named kTarget, or 0.
DWORD findGame() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof pe };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, kTarget) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

/// True if the target process already has a module with this name loaded.
///
/// This matters more than it looks. LoadLibrary on an ALREADY-LOADED module does not reload it -- it
/// bumps a reference count and hands back the existing handle. So injecting a second time into a
/// running game silently keeps the OLD code, and the button reports success while nothing whatsoever
/// has changed. After a rebuild you must restart the game, and the only kind thing to do is say so
/// rather than let somebody test the same stale build four times.
bool alreadyLoaded(DWORD pid, const wchar_t* moduleName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{ sizeof me };
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, moduleName) == 0) { found = true; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

/// Write the DLL path into the target and make it call LoadLibraryA on it.
bool inject(DWORD pid, const std::string& dll, std::wstring& err) {
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                              PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) { err = L"OpenProcess failed -- try running this as administrator"; return false; }

    SIZE_T len = dll.size() + 1;
    void* remote = VirtualAllocEx(proc, nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { err = L"VirtualAllocEx failed"; CloseHandle(proc); return false; }

    if (!WriteProcessMemory(proc, remote, dll.c_str(), len, nullptr)) {
        err = L"WriteProcessMemory failed";
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    // kernel32 sits at the same address in every process on a given boot, so our LoadLibraryA is theirs.
    auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA"));

    HANDLE th = CreateRemoteThread(proc, nullptr, 0, loadLib, remote, 0, nullptr);
    if (!th) {
        err = L"CreateRemoteThread failed";
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    WaitForSingleObject(th, 10000);
    DWORD loaded = 0;
    GetExitCodeThread(th, &loaded);                 // 0 means LoadLibrary returned null
    CloseHandle(th);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);

    if (!loaded) { err = L"the game refused the DLL -- is it the same 64-bit build?"; return false; }
    return true;
}

void onLaunch() {
    DWORD pid = findGame();
    if (!pid) { say(L"osclient.exe is not running -- start the game and log in first."); return; }

    if (alreadyLoaded(pid, L"kewlklient.dll")) {
        say(L"Already injected into this game.\r\n"
            L"If you just rebuilt, CLOSE OSRS and start it again -- Windows will not\r\n"
            L"reload a DLL that is already in the process.");
        return;
    }

    char exe[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dll(exe);
    dll.resize(dll.find_last_of('\\') + 1);
    dll += "kewlklient.dll";

    if (GetFileAttributesA(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        say(L"kewlklient.dll is not next to this exe.");
        return;
    }

    std::wstring err;
    say(inject(pid, dll, err) ? L"injected -- F1/F2 for ESP, F5 for the woodcutter." : err);
}

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        CreateWindowW(L"BUTTON", L"LAUNCH OSRS CLIENT NOW",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      20, 20, 320, 56, h, reinterpret_cast<HMENU>(1), nullptr, nullptr);
        g_status = CreateWindowW(L"STATIC", L"Start the game, log in, then press the button.",
                                 WS_CHILD | WS_VISIBLE, 20, 88, 320, 64, h, nullptr, nullptr, nullptr);
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == 1) onLaunch();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

}  // namespace

// WinMain, not wWinMain, on purpose: mingw needs -municode for the wide entry point and silently fails
// to link without it. We take no command-line arguments, so the narrow entry costs us nothing and the
// project builds under both MSVC and mingw with no extra flags.
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"KewlKlientLauncher";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND h = CreateWindowW(wc.lpszClassName, L"KewlKlient",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 380, 180,
                           nullptr, nullptr, inst, nullptr);
    ShowWindow(h, show);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

