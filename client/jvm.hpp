// jvm.hpp -- start a Java VM inside the game and hand it the four things it needs.
//
// This is the bit that makes KewlKlient worth forking: plugins are Java, not C++. You edit a .java file,
// run one build script, restart the client, and your plugin is live. Nobody needs a C++ toolchain to
// write a bot.
//
// The split is deliberately lopsided:
//
//   C++  does the unsafe things -- reading a live process, calling into the game. Four native methods.
//   Java does everything else   -- plugin logic, state machines, config, whatever you feel like.
//
// If you find yourself adding a fifth native, ask first whether the Java side could do it with the four
// it already has. Every native is a place that can crash the game.
#pragma once
#include <windows.h>
#include <jni.h>
#include <string>
#include <vector>
#include "game.hpp"

namespace kk {

inline JavaVM*   g_vm     = nullptr;
inline jclass    g_api    = nullptr;   // kewl.KewlKlient
inline jmethodID g_tick   = nullptr;
inline jmethodID g_status = nullptr;

// ---------------------------------------------------------------------------------------------------
// The natives. These are the ONLY things Java can do to the game.
// ---------------------------------------------------------------------------------------------------

/// int[] {uid, sceneX, sceneY, isPlayer} per entity, flattened. Empty if the world is not loaded.
inline jintArray JNICALL nEntities(JNIEnv* env, jclass) {
    std::vector<jint> flat;
    int me = localPlayerUid();
    forEachEntity([&](const Entity& e) {
        if (e.uid == me) return;
        flat.push_back(e.uid);
        flat.push_back(e.sceneX);
        flat.push_back(e.sceneY);
        flat.push_back(isPlayerUid(e.uid) ? 1 : 0);
    });
    jintArray arr = env->NewIntArray(static_cast<jsize>(flat.size()));
    if (arr && !flat.empty()) env->SetIntArrayRegion(arr, 0, static_cast<jsize>(flat.size()), flat.data());
    return arr;
}

/// int[] {worldX, worldY} of the scene's south-west corner, or an empty array if not loaded.
inline jintArray JNICALL nSceneBase(JNIEnv* env, jclass) {
    Tile b = sceneBase();
    jint v[2] = { b.x, b.y };
    jintArray arr = env->NewIntArray(b.ok ? 2 : 0);
    if (arr && b.ok) env->SetIntArrayRegion(arr, 0, 2, v);
    return arr;
}

/// Perform a menu action. SCENE coordinates. See game.hpp for why this is the only way we act.
inline void JNICALL nDoAction(JNIEnv*, jclass, jint sx, jint sy, jint opcode, jint targetId) {
    doAction(sx, sy, opcode, targetId);
}

/// True once the game has built its client object -- i.e. you are actually in-game.
inline jboolean JNICALL nReady(JNIEnv*, jclass) {
    return clientObj() ? JNI_TRUE : JNI_FALSE;
}

// ---------------------------------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------------------------------

/// Load jvm.dll. `javaHome` comes from kewlklient.ini so nobody has to guess where your JDK is.
inline HMODULE loadJvmDll(const std::wstring& javaHome) {
    // A JDK has it under bin\server, a JRE sometimes under bin\client. Try both, then give up.
    const wchar_t* rel[] = { L"\\bin\\server\\jvm.dll", L"\\bin\\client\\jvm.dll" };
    for (const wchar_t* r : rel) {
        HMODULE m = LoadLibraryW((javaHome + r).c_str());
        if (m) return m;
    }
    return nullptr;
}

/// Start the VM, load kewl.KewlKlient, wire the natives, call its start(). Returns false with a reason
/// you can show the user -- silent failure here is miserable to debug.
inline bool startJvm(const std::wstring& javaHome, const std::wstring& jarPath, std::string& err) {
    HMODULE jvmDll = loadJvmDll(javaHome);
    if (!jvmDll) { err = "could not load jvm.dll -- check java= in kewlklient.ini"; return false; }

    using CreateFn = jint(JNICALL*)(JavaVM**, void**, void*);
    auto create = reinterpret_cast<CreateFn>(GetProcAddress(jvmDll, "JNI_CreateJavaVM"));
    if (!create) { err = "jvm.dll has no JNI_CreateJavaVM"; return false; }

    std::string cp = "-Djava.class.path=";
    cp.append(jarPath.begin(), jarPath.end());

    JavaVMOption opt[1]{};
    opt[0].optionString = cp.data();

    JavaVMInitArgs args{};
    args.version = JNI_VERSION_1_8;
    args.nOptions = 1;
    args.options = opt;
    args.ignoreUnrecognized = JNI_FALSE;

    JNIEnv* env = nullptr;
    if (create(&g_vm, reinterpret_cast<void**>(&env), &args) != JNI_OK || !env) {
        err = "JNI_CreateJavaVM failed";
        return false;
    }

    jclass local = env->FindClass("kewl/KewlKlient");
    if (!local) { err = "kewl/KewlKlient not found -- is kewlklient.jar next to the DLL?"; return false; }
    g_api = static_cast<jclass>(env->NewGlobalRef(local));

    const JNINativeMethod natives[] = {
        { const_cast<char*>("entities"),  const_cast<char*>("()[I"),   reinterpret_cast<void*>(nEntities) },
        { const_cast<char*>("sceneBase"), const_cast<char*>("()[I"),   reinterpret_cast<void*>(nSceneBase) },
        { const_cast<char*>("doAction"),  const_cast<char*>("(IIII)V"), reinterpret_cast<void*>(nDoAction) },
        { const_cast<char*>("ready"),     const_cast<char*>("()Z"),    reinterpret_cast<void*>(nReady) },
    };
    if (env->RegisterNatives(g_api, natives, 4) != JNI_OK) { err = "RegisterNatives failed"; return false; }

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

/// Run one plugin tick. `keys` is the F5..F8 edge mask (bit 0 = F5).
inline void tickJvm(int keys) {
    JNIEnv* e = env();
    if (!e || !g_tick) return;
    e->CallStaticVoidMethod(g_api, g_tick, static_cast<jint>(keys));
    // A plugin throwing must never take the game down. Print it and carry on.
    if (e->ExceptionCheck()) { e->ExceptionDescribe(); e->ExceptionClear(); }
}

/// One line per plugin, for the overlay panel. Empty if Java is not up yet.
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
