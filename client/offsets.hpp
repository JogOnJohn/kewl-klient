// offsets.hpp -- every game-specific number KewlKlient depends on, in one file.
//
// READ THIS BEFORE CHANGING ANYTHING HERE.
//
// These are offsets into a program we do not control, and Jagex rebuilds it roughly weekly. Two kinds of
// number live here and they rot at very different speeds:
//
//   FUNCTION RVAs   (DO_ACTION, WORLD_TO_SCREEN)  move on almost every update. Assume they are wrong
//                                                 after any patch until you re-derive them.
//   STRUCT OFFSETS  (ENTITY_SCENE_X, ENTITY_TABLE...) are stabler, but they DO move. ENTITY_TABLE moved
//                                                 by 0x10 between two builds a few weeks apart.
//
// BUILD_ID below is the sanity check. It is the RVA of a function we use as a fingerprint for "which
// build is this". If it does not match, every other number in this file is suspect and the client
// refuses to start rather than reading garbage out of a stranger's address space.
//
// HOW TO RE-DERIVE THESE: see README.md, section "When the game updates". Short version: run the Ghidra
// headless script in tools/, or open the exe in IDA and use the anchors named in the comments below.
// Every one of these was found by anchoring on something the client itself names -- a string, a Lua
// binding, a distinctive constant -- never by scanning for a byte pattern and hoping.
#pragma once
#include <cstdint>

namespace kk::off {

// ---------------------------------------------------------------------------------------------------
// BUILD FINGERPRINT
// ---------------------------------------------------------------------------------------------------
// The "gate" function's RVA. We do not call it; we only use its address as a build id, because it is
// easy to find and it changes every build. If your client does not match, DO NOT just bump this number:
// re-derive the whole file, because everything below was measured on this exact build.
inline constexpr std::uintptr_t BUILD_ID = 0x64990;

// ---------------------------------------------------------------------------------------------------
// THE ROOT POINTER
// ---------------------------------------------------------------------------------------------------
// *(imageBase + CLIENT_OBJ_PTR) is "the client object" -- the god-object almost everything hangs off.
// Find it by: picking any leaf function you already trust (worldToScreen is ideal), and looking at the
// global it dereferences first. It will be a `mov rax, [rip+X]` near the top.
inline constexpr std::uintptr_t CLIENT_OBJ_PTR = 0xE72398;

// ---------------------------------------------------------------------------------------------------
// FUNCTIONS WE CALL (RVAs from the module base)
// ---------------------------------------------------------------------------------------------------
// The client's own "do a menu action" entry point. We call it instead of building network packets by
// hand: it takes the same arguments the real menu does, and the client builds and sends the packet for
// us. This is why KewlKlient does not need to know the wire protocol at all.
//
// Signature (as we use it):
//   void doAction(void* clientObj, int sceneX, int sceneY, int opcode, int targetId,
//                 int a6, long long a7, int itemId, int flags, long long a10)
inline constexpr std::uintptr_t DO_ACTION = 0x868C0;

// The client's world->screen projection leaf. Takes {fineX, fineY, fineZ} and writes {screenX, screenY}.
// "Fine" coordinates are tiles << 7 (i.e. 128 units per tile). It reads the camera out of the client
// object itself, so the first argument is ignored -- pass nullptr.
//
//   float* worldToScreen(void* ignored, float out[2], int fine[3])
inline constexpr std::uintptr_t WORLD_TO_SCREEN = 0x1F2730;

// ---------------------------------------------------------------------------------------------------
// FIELDS ON THE CLIENT OBJECT
// ---------------------------------------------------------------------------------------------------
inline constexpr std::uintptr_t SCENE            = 0xCA90;  // -> the scene/world object (see below)
inline constexpr std::uintptr_t LOCAL_PLAYER_IDX = 0xCC5C;  // your own player handle
inline constexpr std::uintptr_t PLAYER_COUNT     = 0xCCE0;  // how many entries in PLAYER_IDS
inline constexpr std::uintptr_t PLAYER_IDS       = 0xCCE4;  // int[] of player handles, 0xFFFFFFFF = empty

// ---------------------------------------------------------------------------------------------------
// FIELDS ON THE SCENE OBJECT  ( *(clientObj + SCENE) )
// ---------------------------------------------------------------------------------------------------
// The entity hashtable holds NPCs and players together, keyed by uid.
//   buckets = *(scene + ENTITY_BUCKETS)   -- array of linked-list heads, 8 bytes each
//   count   = *(scene + ENTITY_COUNT)     -- number of buckets
//   node:  +0x00 uid (int)   +0x10 entity pointer   +0x18 next node
inline constexpr std::uintptr_t ENTITY_BUCKETS = 0xB8;
inline constexpr std::uintptr_t ENTITY_COUNT   = 0xC0;

// The scene's south-west corner in WORLD tiles. Entities carry SCENE coordinates (0..104), so:
//     worldX = SCENE_BASE_X + entity.sceneX
inline constexpr std::uintptr_t SCENE_BASE_X = 0x48;
inline constexpr std::uintptr_t SCENE_BASE_Y = 0x4C;

// ---------------------------------------------------------------------------------------------------
// FIELDS ON AN ENTITY (a player or an NPC)
// ---------------------------------------------------------------------------------------------------
inline constexpr std::uintptr_t ENTITY_SCENE_X = 0x3F0;
inline constexpr std::uintptr_t ENTITY_SCENE_Y = 0x418;

// ---------------------------------------------------------------------------------------------------
// MENU OPCODES
// ---------------------------------------------------------------------------------------------------
// These are the client's INTERNAL menu action numbers, not network opcodes. OPLOC1 is "the first option
// on a scenery object" -- Chop down, Mine, Open, and so on, depending on what you clicked.
inline constexpr int OPLOC1 = 3;

}  // namespace kk::off
