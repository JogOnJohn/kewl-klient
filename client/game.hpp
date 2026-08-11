// game.hpp -- reading the game, and doing one thing to it.
//
// Everything here is either a guarded memory read or a call into a function the game already has. There
// is no packet building anywhere in KewlKlient, on purpose: we ask the client to perform a menu action
// and it builds and sends the packet itself. That means we never have to track the wire protocol, which
// is the part that changes most often and is hardest to get right.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cmath>
#include "offsets.hpp"

namespace kk {

// ---------------------------------------------------------------------------------------------------
// Guarded reads. Everything we touch is a pointer we derived, in a process we do not own, while the game
// is actively mutating it. A torn or stale read is NORMAL. Never let one crash the client -- return a
// zero and skip that frame instead.
// ---------------------------------------------------------------------------------------------------
inline bool readable(std::uintptr_t p, std::size_t n) {
    if (p < 0x10000) return false;                       // null-ish / first page
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<void*>(p), &mbi, sizeof mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return p + n <= reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
}

template <class T>
T rd(std::uintptr_t addr, T fallback = T{}) {
    return readable(addr, sizeof(T)) ? *reinterpret_cast<T*>(addr) : fallback;
}

inline std::uintptr_t rdp(std::uintptr_t addr) { return rd<std::uintptr_t>(addr); }

// ---------------------------------------------------------------------------------------------------
// The roots
// ---------------------------------------------------------------------------------------------------
inline std::uintptr_t moduleBase() {
    static std::uintptr_t b = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    return b;
}

/// The client object, or 0 if the game has not built it yet (it is null for the first few seconds).
inline std::uintptr_t clientObj() { return rdp(moduleBase() + off::CLIENT_OBJ_PTR); }

/// The scene object, or 0.
inline std::uintptr_t scene() {
    std::uintptr_t c = clientObj();
    return c ? rdp(c + off::SCENE) : 0;
}

struct Tile { int x = 0, y = 0; bool ok = false; };

/// The scene's south-west corner, in world tiles. Entities store SCENE coords; add this to get world.
inline Tile sceneBase() {
    std::uintptr_t s = scene();
    if (!s) return {};
    return { rd<std::int32_t>(s + off::SCENE_BASE_X), rd<std::int32_t>(s + off::SCENE_BASE_Y), true };
}

// ---------------------------------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------------------------------
struct Entity {
    int            uid = 0;
    std::uintptr_t addr = 0;
    int            sceneX = 0, sceneY = 0;
    int            plane = 0;
    int            animation = -1;
    int            orientation = 0;
};

/// What KIND of NPC this is. Meaningless for players -- they have no definition object -- so check
/// isPlayerUid first. Returns -1 when the extra pointer hop lands somewhere unreadable, which happens
/// for an entity being spawned or despawned as we walk the table.
inline int npcTypeId(std::uintptr_t entity) {
    if (!entity) return -1;
    std::uintptr_t def = rdp(entity + off::ENTITY_DEF_PTR);
    if (!def) return -1;
    return rd<std::int32_t>(def, -1);
}

/// A player's combat level. Zero if it is not a player or the read fails.
inline int combatLevel(std::uintptr_t entity) {
    return entity ? rd<std::int32_t>(entity + off::PLAYER_COMBAT_LEVEL) : 0;
}

/// Walk every entity in the scene's hashtable (NPCs and players together) and hand each to `cb`.
/// Bounded at every level so a resize happening under us cannot spin forever.
template <class F>
void forEachEntity(F&& cb) {
    std::uintptr_t s = scene();
    if (!s) return;
    std::uintptr_t buckets = rdp(s + off::ENTITY_BUCKETS);
    std::uint32_t  count   = rd<std::uint32_t>(s + off::ENTITY_COUNT);
    if (!buckets || count == 0 || count > 0x40000) return;      // never a real table this big

    for (std::uint32_t b = 0; b < count; ++b) {
        std::uintptr_t node = rdp(buckets + static_cast<std::uintptr_t>(b) * 8);
        for (int guard = 0; node && guard < 4096; ++guard, node = rdp(node + 0x18)) {
            std::uintptr_t e = rdp(node + 0x10);
            if (!e) continue;
            Entity ent;
            ent.uid    = rd<std::int32_t>(node);
            ent.addr   = e;
            ent.sceneX = rd<std::int32_t>(e + off::ENTITY_SCENE_X);
            ent.sceneY = rd<std::int32_t>(e + off::ENTITY_SCENE_Y);
            if (ent.sceneX <= 0 || ent.sceneY <= 0 || ent.sceneX > 104 || ent.sceneY > 104) continue;
            ent.plane       = rd<std::int32_t>(e + off::ENTITY_PLANE);
            ent.animation   = rd<std::int32_t>(e + off::ENTITY_ANIMATION, -1);
            ent.orientation = rd<std::int32_t>(e + off::ENTITY_ORIENTATION);
            cb(ent);
        }
    }
}

/// True if this uid belongs to a player. The client keeps player handles in a flat array on the client
/// object; anything in the entity table that is not in that array is an NPC.
inline bool isPlayerUid(int uid) {
    std::uintptr_t c = clientObj();
    if (!c) return false;
    int n = rd<std::int32_t>(c + off::PLAYER_COUNT);
    if (n <= 0 || n > 4096) return false;
    for (int i = 0; i < n; ++i) {
        std::uint32_t h = rd<std::uint32_t>(c + off::PLAYER_IDS + static_cast<std::uintptr_t>(i) * 4);
        if (h != 0xFFFFFFFFu && static_cast<int>(h) == uid) return true;
    }
    return false;
}

/// Your own player handle, or -1.
inline int localPlayerUid() {
    std::uintptr_t c = clientObj();
    return c ? rd<std::int32_t>(c + off::LOCAL_PLAYER_IDX, -1) : -1;
}

/// You, as an entity. `found` is false before you are in the world.
inline Entity localPlayer(bool& found) {
    Entity me;
    found = false;
    int uid = localPlayerUid();
    if (uid < 0) return me;
    forEachEntity([&](const Entity& e) {
        if (found || e.uid != uid) return;
        me = e;
        found = true;
    });
    return me;
}

// ---------------------------------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------------------------------
inline constexpr int SKILL_COUNT = 25;

/// One skill's effective (boosted) level, base level, or total xp. `which` is 0..24 -- see Skill.java.
inline int skillEffective(int which) {
    std::uintptr_t c = clientObj();
    if (!c || which < 0 || which >= SKILL_COUNT) return 0;
    return rd<std::int32_t>(c + off::SKILL_EFFECTIVE + static_cast<std::uintptr_t>(which) * 4);
}
inline int skillBase(int which) {
    std::uintptr_t c = clientObj();
    if (!c || which < 0 || which >= SKILL_COUNT) return 0;
    return rd<std::int32_t>(c + off::SKILL_BASE + static_cast<std::uintptr_t>(which) * 4);
}
inline int skillXp(int which) {
    std::uintptr_t c = clientObj();
    if (!c || which < 0 || which >= SKILL_COUNT) return 0;
    return rd<std::int32_t>(c + off::SKILL_XP + static_cast<std::uintptr_t>(which) * 4);
}

/// Run energy, 0..10000 (so 10000 is a full bar).
inline int runEnergy() {
    std::uintptr_t c = clientObj();
    return c ? rd<std::int32_t>(c + off::RUN_ENERGY) : 0;
}

/// The client's frame counter. Useful as a cheap "is the game actually running" check.
inline int cycle() {
    std::uintptr_t c = clientObj();
    return c ? rd<std::int32_t>(c + off::CYCLE) : 0;
}

// ---------------------------------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------------------------------
/// Project a point in FINE coordinates to screen pixels, using the game's own projection -- so it is
/// always exactly right, including while the camera is moving, which is the whole reason we call the
/// game's function instead of reimplementing the maths.
///
/// Fine coordinates are tiles << 7: 128 units per tile, so the centre of scene tile (x, y) is
/// ((x << 7) + 64, ((y << 7) + 64). `fineHeight` is the vertical axis -- 0 is ground level.
///
/// Returns false when the point is behind the camera or otherwise off in the weeds. Do not draw it.
inline bool projectFine(int fineX, int fineHeight, int fineY, float& outX, float& outY) {
    using Fn = float* (__fastcall*)(void*, float*, int*);
    auto fn = reinterpret_cast<Fn>(moduleBase() + off::WORLD_TO_SCREEN);

    float out[2] = { 0.f, 0.f };
    int   fine[3] = { fineX, fineHeight, fineY };
    fn(nullptr, out, fine);

    outX = out[0];
    outY = out[1];
    if (!std::isfinite(outX) || !std::isfinite(outY)) return false;
    return !(outX < -10000.f || outX > 10000.f || outY < -10000.f || outY > 10000.f);
}

/// The centre of a SCENE tile, at ground level.
inline bool project(int sceneX, int sceneY, float& outX, float& outY) {
    return projectFine((sceneX << 7) + 64, 0, (sceneY << 7) + 64, outX, outY);
}

// ---------------------------------------------------------------------------------------------------
// Doing something
// ---------------------------------------------------------------------------------------------------
/// Perform a menu action, exactly as if you had clicked it. The client builds and sends the packet.
///
/// `sceneX`/`sceneY` are SCENE coordinates (0..104), not world ones. `opcode` is a menu action number
/// from offsets.hpp. `targetId` is whatever that action targets -- for scenery it is the object id.
///
/// MUST be called from the game thread. Calling it from our own thread works most of the time and then
/// crashes at the worst moment, so the overlay queues actions and the plugin tick runs them on a timer
/// that is slow enough not to matter. If you make KewlKlient do anything fancier than this, hook a
/// per-frame function and run actions from there.
inline void doAction(int sceneX, int sceneY, int opcode, int targetId) {
    std::uintptr_t c = clientObj();
    if (!c) return;
    using Fn = void(__fastcall*)(void*, int, int, int, int, int, long long, int, int, long long);
    auto fn = reinterpret_cast<Fn>(moduleBase() + off::DO_ACTION);
    fn(reinterpret_cast<void*>(c), sceneX, sceneY, opcode, targetId, 0, 0, 0, 0, 0);
}

/// Walk to a SCENE tile. The game pathfinds and sends the movement itself; we only say where.
inline void walkTo(int sceneX, int sceneY) {
    doAction(sceneX, sceneY, off::OP_WALK, 0);
}

/// Interact with an NPC by uid -- attack it, talk to it, pickpocket it, whatever `opcode` selects.
///
/// The uid IS the target: the client looks the NPC up in the same hashtable we walked to find it, so we
/// do not have to care where it has moved to since. We still pass its tile because that is the shape
/// doAction wants, and we look it up here so callers only need the uid.
inline void interactNpc(int uid, int opcode) {
    bool found = false;
    Entity target;
    forEachEntity([&](const Entity& e) {
        if (found || e.uid != uid) return;
        target = e;
        found = true;
    });
    if (!found) return;
    doAction(target.sceneX, target.sceneY, opcode, uid);
}

}  // namespace kk
