// jvm.hpp -- start a Java VM inside the game and hand it the few things it needs.
//
// This is the bit that makes KewlKlient worth forking: plugins are Java, not C++. You edit a .java file,
// run one build, restart the client, and your plugin is live. Nobody needs a C++ toolchain to write a
// bot, or to draw an overlay.
//
// The split is deliberately lopsided:
//
//   C++  does the unsafe things -- reading a live process, calling into the game, putting pixels on the
//        screen. That is this file, and it is the entire unsafe surface.
//   Java does everything else   -- plugin logic, state machines, config, and ALL the drawing.
//
// Every native below is a place that can crash the game, so the bar for adding one is: could the Java
// side do this with what it already has? Nine is more than the four we started with, and each of the
// extra five earned its place by removing a whole category of thing C++ would otherwise have to know
// about -- what an NPC is, what a skill is, what a box looks like.
#pragma once
#include <windows.h>
#include <jni.h>
#include <string>
#include <vector>
#include "game.hpp"
#include "overlay.hpp"

namespace kk {

inline JavaVM*   g_vm     = nullptr;
inline jclass    g_api    = nullptr;   // kewl.KewlKlient  -- lifecycle
inline jclass    g_nat    = nullptr;   // kewl.Natives     -- where the natives are registered
inline jmethodID g_tick   = nullptr;
inline jmethodID g_status = nullptr;

/// Set by dllmain: the game's own window, so we can report its size to Java.
inline HWND g_gameWindow = nullptr;

// ---------------------------------------------------------------------------------------------------
// The natives. These are the ONLY things Java can do to the game.
// ---------------------------------------------------------------------------------------------------

/// True once the game has built its client object -- i.e. you are actually in-game.
inline jboolean JNICALL nReady(JNIEnv*, jclass) {
    return clientObj() ? JNI_TRUE : JNI_FALSE;
}

/// Every visible entity, flattened, seven ints each:
///     uid, sceneX, sceneY, isPlayer, typeId, animation, orientation
///
/// One array rather than one object per entity on purpose -- this is called thirty times a second, and
/// allocating a few hundred short-lived objects a frame is exactly the kind of thing that turns into a
/// stutter you then spend an evening profiling. Java unpacks it into records once.
inline jintArray JNICALL nEntities(JNIEnv* env, jclass) {
    std::vector<jint> flat;
    flat.reserve(256 * 7);
    forEachEntity([&](const Entity& e) {
        bool player = isPlayerUid(e.uid);
        flat.push_back(e.uid);
        flat.push_back(e.sceneX);
        flat.push_back(e.sceneY);
        flat.push_back(player ? 1 : 0);
        flat.push_back(player ? combatLevel(e.addr) : npcTypeId(e.addr));
        flat.push_back(e.animation);
        flat.push_back(e.orientation);
    });
    jintArray arr = env->NewIntArray(static_cast<jsize>(flat.size()));
    if (arr && !flat.empty()) env->SetIntArrayRegion(arr, 0, static_cast<jsize>(flat.size()), flat.data());
    return arr;
}

/// int[] {worldX, worldY} of the scene's south-west corner, or empty if the world is not loaded.
inline jintArray JNICALL nSceneBase(JNIEnv* env, jclass) {
    Tile b = sceneBase();
    jint v[2] = { b.x, b.y };
    jintArray arr = env->NewIntArray(b.ok ? 2 : 0);
    if (arr && b.ok) env->SetIntArrayRegion(arr, 0, 2, v);
    return arr;
}

/// You: {uid, sceneX, sceneY, plane, animation, orientation, runEnergy, cycle}. Empty before you spawn.
inline jintArray JNICALL nLocal(JNIEnv* env, jclass) {
    bool found = false;
    Entity me = localPlayer(found);
    if (!found) return env->NewIntArray(0);
    jint v[8] = { me.uid, me.sceneX, me.sceneY, me.plane,
                  me.animation, me.orientation, runEnergy(), cycle() };
    jintArray arr = env->NewIntArray(8);
    if (arr) env->SetIntArrayRegion(arr, 0, 8, v);
    return arr;
}

/// All 25 skills at once: effective[25], base[25], xp[25], in that order. One call rather than 75.
inline jintArray JNICALL nSkills(JNIEnv* env, jclass) {
    if (!clientObj()) return env->NewIntArray(0);
    jint v[SKILL_COUNT * 3];
    for (int i = 0; i < SKILL_COUNT; ++i) {
        v[i]                     = skillEffective(i);
        v[SKILL_COUNT + i]       = skillBase(i);
        v[SKILL_COUNT * 2 + i]   = skillXp(i);
    }
    jintArray arr = env->NewIntArray(SKILL_COUNT * 3);
    if (arr) env->SetIntArrayRegion(arr, 0, SKILL_COUNT * 3, v);
    return arr;
}

/// Project a fine-coordinate point to the screen. Returns the two screen coordinates packed into a
/// long, or Long.MIN_VALUE when the point is not on screen.
///
/// Packed into a long rather than returned in an array because a plugin drawing tile outlines calls
/// this four times per tile, and an allocation per call would dominate the cost of the whole overlay.
inline jlong JNICALL nProject(JNIEnv*, jclass, jint fineX, jint fineHeight, jint fineY) {
    float sx = 0.f, sy = 0.f;
    if (!projectFine(fineX, fineHeight, fineY, sx, sy)) return static_cast<jlong>(0x8000000000000000ULL);
    jlong x = static_cast<jlong>(static_cast<jint>(sx));
    jlong y = static_cast<jlong>(static_cast<jint>(sy));
    return (x << 32) | (y & 0xFFFFFFFFLL);
}

/// Perform a menu action. SCENE coordinates. See game.hpp for why this is the only way we act.
inline void JNICALL nDoAction(JNIEnv*, jclass, jint sx, jint sy, jint opcode, jint targetId) {
    doAction(sx, sy, opcode, targetId);
}

/// Interact with an NPC by uid, looking its tile up for you.
inline void JNICALL nInteractNpc(JNIEnv*, jclass, jint uid, jint opcode) {
    interactNpc(uid, opcode);
}

/// The game's client area on screen: {x, y, width, height}. Java needs the size to make its image and
/// the position to park the control panel beside the game.
inline jintArray JNICALL nViewport(JNIEnv* env, jclass) {
    jintArray arr = env->NewIntArray(4);
    if (!arr || !g_gameWindow || !IsWindow(g_gameWindow)) return arr;
    RECT r{};
    GetClientRect(g_gameWindow, &r);
    POINT tl{ r.left, r.top };
    ClientToScreen(g_gameWindow, &tl);
    jint v[4] = { tl.x, tl.y, r.right - r.left, r.bottom - r.top };
    env->SetIntArrayRegion(arr, 0, 4, v);
    return arr;
}

/// Put a finished frame on the screen. `px` is w*h premultiplied ARGB pixels, top row first.
///
/// GetPrimitiveArrayCritical rather than GetIntArrayElements: the former hands back a pointer to the
/// array's real storage instead of copying eight megabytes we are about to copy again. The window
/// between the two calls must contain nothing that could block or allocate, which is why the only thing
/// in it is the memcpy.
inline void JNICALL nPresent(JNIEnv* env, jclass, jintArray px, jint w, jint h) {
    if (!px || w <= 0 || h <= 0) return;
    if (env->GetArrayLength(px) < w * h) return;          // never trust a length we did not compute

    void* raw = env->GetPrimitiveArrayCritical(px, nullptr);
    if (!raw) return;
    overlay::present(raw, w, h);
    env->ReleasePrimitiveArrayCritical(px, raw, JNI_ABORT);   // ABORT: we did not modify it
}

// ---------------------------------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------------------------------

/// Windows paths are wide; the JNI option string is narrow. Convert properly rather than truncating.
///
/// The obvious `std::string(w.begin(), w.end())` compiles, works on every path you personally test, and
/// then mangles the classpath for anybody whose Windows username is not pure ASCII -- which is a lot of
/// people, and whose symptom is "kewl/Natives not found" with a perfectly correct-looking path in the
/// error. UTF-8 is what the JVM expects here.
inline std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                                nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), n,
                        nullptr, nullptr);
    return out;
}

/// Load jvm.dll. `javaHome` comes from kewlklient.ini so nobody has to guess where your JDK is.
/// `detail` is filled in on failure with the exact path tried and the Win32 error, because "check
/// java= in kewlklient.ini" is useless advice on its own -- it does not say what the client READ, and a
/// path that is subtly mangled (a lost backslash, a stray quote) looks correct at a glance in the file.
/// Print what was attempted and the problem is usually obvious on sight.
inline HMODULE loadJvmDll(const std::wstring& javaHome, std::string& detail) {
    // Tell the loader about the JDK's own bin directory before asking for jvm.dll.
    //
    // jvm.dll does not stand alone -- it pulls in siblings that live in the JDK's bin, one level up
    // from bin\server. A plain LoadLibrary resolves those through the HOST process's search path, and
    // we are inside somebody else's process: if the game has narrowed its default search directories
    // (a normal hardening step), the load fails with ERROR_MOD_NOT_FOUND for a file that is plainly
    // sitting right there. AddDllDirectory is additive and per-process rather than replacing anything,
    // so unlike SetDllDirectory it cannot disturb how the game resolves its own DLLs.
    std::wstring bin = javaHome + L"\\bin";
    AddDllDirectory(bin.c_str());

    // A JDK has it under bin\server, a JRE sometimes under bin\client. Try both, then give up.
    const wchar_t* rel[] = { L"\\bin\\server\\jvm.dll", L"\\bin\\client\\jvm.dll" };
    DWORD lastError = 0;
    for (const wchar_t* r : rel) {
        std::wstring full = javaHome + r;

        // The widened search first; then a plain load, because the flags below need the directory to
        // have been registered and an older or stranger host may not cooperate. Whichever works, works.
        HMODULE m = LoadLibraryExW(full.c_str(), nullptr,
                                   LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                                   LOAD_LIBRARY_SEARCH_USER_DIRS |
                                   LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
        if (!m) m = LoadLibraryW(full.c_str());
        if (m) return m;
        lastError = GetLastError();
    }

    detail = narrow(javaHome + rel[0]);
    detail += "  (error " + std::to_string(lastError);
    if (lastError == 2)        detail += ": no such file -- is java= the JDK folder itself?";
    else if (lastError == 126) detail += ": a dependency of jvm.dll is missing";
    else if (lastError == 193) detail += ": that is a 32-bit JDK, the game is 64-bit";
    detail += ")";
    return nullptr;
}

/// Start the VM, load the classes, wire the natives, call start(). Returns false with a reason you can
/// show the user -- silent failure here is miserable to debug.
inline bool startJvm(const std::wstring& javaHome, const std::wstring& jarPath, std::string& err) {
    std::string detail;
    HMODULE jvmDll = loadJvmDll(javaHome, detail);
    if (!jvmDll) { err = "could not load " + detail; return false; }

    using CreateFn = jint(JNICALL*)(JavaVM**, void**, void*);
    auto create = reinterpret_cast<CreateFn>(GetProcAddress(jvmDll, "JNI_CreateJavaVM"));
    if (!create) { err = "jvm.dll has no JNI_CreateJavaVM"; return false; }

    std::string cp = "-Djava.class.path=" + narrow(jarPath);

    // The control panel is a Swing window, so the VM must not come up headless. Some environments set
    // that by default and the failure is a confusing HeadlessException from inside a plugin.
    std::string headless = "-Djava.awt.headless=false";

    JavaVMOption opt[2]{};
    opt[0].optionString = cp.data();
    opt[1].optionString = headless.data();

    JavaVMInitArgs args{};
    args.version = JNI_VERSION_1_8;
    args.nOptions = 2;
    args.options = opt;
    args.ignoreUnrecognized = JNI_FALSE;

    JNIEnv* env = nullptr;
    if (create(&g_vm, reinterpret_cast<void**>(&env), &args) != JNI_OK || !env) {
        err = "JNI_CreateJavaVM failed";
        return false;
    }

    jclass natLocal = env->FindClass("kewl/Natives");
    if (!natLocal) { err = "kewl/Natives not found -- is kewlklient.jar next to the DLL?"; return false; }
    g_nat = static_cast<jclass>(env->NewGlobalRef(natLocal));

    const JNINativeMethod natives[] = {
        { const_cast<char*>("ready"),       const_cast<char*>("()Z"),     reinterpret_cast<void*>(nReady) },
        { const_cast<char*>("entities"),    const_cast<char*>("()[I"),    reinterpret_cast<void*>(nEntities) },
        { const_cast<char*>("sceneBase"),   const_cast<char*>("()[I"),    reinterpret_cast<void*>(nSceneBase) },
        { const_cast<char*>("local"),       const_cast<char*>("()[I"),    reinterpret_cast<void*>(nLocal) },
        { const_cast<char*>("skills"),      const_cast<char*>("()[I"),    reinterpret_cast<void*>(nSkills) },
        { const_cast<char*>("project"),     const_cast<char*>("(III)J"),  reinterpret_cast<void*>(nProject) },
        { const_cast<char*>("doAction"),    const_cast<char*>("(IIII)V"), reinterpret_cast<void*>(nDoAction) },
        { const_cast<char*>("interactNpc"), const_cast<char*>("(II)V"),   reinterpret_cast<void*>(nInteractNpc) },
        { const_cast<char*>("viewport"),    const_cast<char*>("()[I"),    reinterpret_cast<void*>(nViewport) },
        { const_cast<char*>("present"),     const_cast<char*>("([III)V"), reinterpret_cast<void*>(nPresent) },
    };
    if (env->RegisterNatives(g_nat, natives, 10) != JNI_OK) { err = "RegisterNatives failed"; return false; }

    jclass local = env->FindClass("kewl/KewlKlient");
    if (!local) { err = "kewl/KewlKlient not found"; return false; }
    g_api = static_cast<jclass>(env->NewGlobalRef(local));

    jmethodID start = env->GetStaticMethodID(g_api, "start", "()V");
    g_tick   = env->GetStaticMethodID(g_api, "tick", "(I)V");
    g_status = env->GetStaticMethodID(g_api, "status", "()Ljava/lang/String;");
    if (!start || !g_tick) { err = "kewl.KewlKlient needs static start() and tick(int)"; return false; }

    env->CallStaticVoidMethod(g_api, start);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    return true;
}

/// The JNIEnv for this thread, attaching it the first time. Null if the VM is not up.
inline JNIEnv* env() {
    if (!g_vm) return nullptr;
    JNIEnv* e = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_8) != JNI_OK) {
        if (g_vm->AttachCurrentThread(reinterpret_cast<void**>(&e), nullptr) != JNI_OK) return nullptr;
    }
    return e;
}

/// Run one frame: plugins tick, overlays draw, and Java calls present() before returning. `keys` is the
/// F-key edge mask.
inline void tickJvm(int keys) {
    JNIEnv* e = env();
    if (!e || !g_tick) return;
    e->CallStaticVoidMethod(g_api, g_tick, static_cast<jint>(keys));
    // A plugin throwing must never take the game down. Print it and carry on.
    if (e->ExceptionCheck()) { e->ExceptionDescribe(); e->ExceptionClear(); }
}

/// One line per plugin, for when Java is up but nothing is drawing yet.
inline std::string jvmStatus() {
    JNIEnv* e = env();
    if (!e || !g_status) return {};
    auto js = static_cast<jstring>(e->CallStaticObjectMethod(g_api, g_status));
    if (e->ExceptionCheck()) { e->ExceptionDescribe(); e->ExceptionClear(); return {}; }
    if (!js) return {};
    const char* c = e->GetStringUTFChars(js, nullptr);
    std::string out = c ? c : "";
    if (c) e->ReleaseStringUTFChars(js, c);
    e->DeleteLocalRef(js);
    return out;
}

}  // namespace kk
