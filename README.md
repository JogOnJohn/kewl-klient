# KewlKlient

A small, ugly, readable OSRS client. Player and NPC boxes, one example bot, and about 900 lines of code
you can hold in your head.

It exists to be **learned from and hacked on**, not to be pretty. The UI is deliberately plain. There is
no auto-updater, no account manager, no plugin store. What there is:

- **Plugins are Java.** You write a class, add one line, rebuild. No C++ toolchain needed to write a bot.
- **No network code at all.** We never build a packet. We call the game's own "do this menu action"
  function and let it build and send the packet. That is the single biggest reason this codebase is
  small, and the reason it survives most game updates.
- **Four native methods.** That is the entire unsafe surface. Everything else is ordinary Java.

```
       launcher                       injected into the game
    ┌──────────────┐                ┌────────────────────────────────┐
    │  one button  │ ── inject ──>  │  kewlklient.dll                │
    └──────────────┘                │    reads memory, draws boxes   │
                                    │    starts a JVM ───────────────┼──> kewlklient.jar
                                    │    4 natives ──────────────────┼──>   your plugins
                                    └────────────────────────────────┘
```

---

## Quick start

**Windows only.** The whole thing is Win32 — there is no Linux or Mac build and there is not going to be
one.

You need three things installed. The first two you probably have; the third is the one people miss:

| | why | get it |
|---|---|---|
| **JDK 17+** | the plugins are Java | IntelliJ ships one, or [adoptium.net](https://adoptium.net) |
| **CMake** | the injected part is C++ | [cmake.org/download](https://cmake.org/download/) — tick *Add CMake to the system PATH* |
| **A C++ compiler** | same | [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/) → *Desktop development with C++*. The full IDE is not needed. |

You do **not** need Gradle — the wrapper in this repo fetches it.

### From IntelliJ

1. **File → Open** and pick the folder you cloned. It imports as a Gradle project.
2. Pick the **KewlKlient** run configuration (it is checked into the repo) and press **Run**.
3. Start OSRS and **log in**.
4. In the little window that appeared, press **LAUNCH OSRS CLIENT NOW**.

That is it — no config file to edit. The build points `kewlklient.ini` at whichever JDK IntelliJ is
using, and puts everything in `build\dist\`.

### From a terminal

```bat
gradlew run
```

Same thing. `gradlew dist` builds without launching, and `build.bat` is a wrapper around it for people
who prefer a double-click.

In game:

| key | does |
|-----|------|
| F1 | player boxes on/off |
| F2 | NPC boxes on/off |
| F5 | the woodcutter plugin on/off |
| F6–F8 | free, for your plugins |

If nothing appears, see [Troubleshooting](#troubleshooting).

---

## Writing a plugin

This is the whole thing:

```java
package kewl.plugins;

import kewl.Game;
import kewl.Plugin;

public final class Waver implements Plugin {
    private boolean on;

    @Override public String name()  { return "waver"; }
    @Override public boolean enabled() { return on; }
    @Override public void setEnabled(boolean v) { on = v; }
    @Override public void keys(int pressed) { if ((pressed & 2) != 0) on = !on; }   // bit 1 = F6

    @Override
    public void tick() {
        Game.Entity npc = Game.nearest(Game.npcs());
        if (npc != null) System.out.println("nearest npc at " + npc.worldX() + "," + npc.worldY());
    }
}
```

Add it to the list in `java/kewl/KewlKlient.java`:

```java
private static final List<Plugin> PLUGINS = new ArrayList<>(List.of(
        new kewl.plugins.Woodcutter(),
        new kewl.plugins.Waver()          // <- yours
));
```

Rebuild, restart the game, press F6. That is the entire plugin system — there is no scanning, no
manifest, no reflection.

**`tick()` runs ~30 times a second.** Never sleep in it. If you want something every few seconds, keep a
`long lastX` field and compare `System.currentTimeMillis()`, like `Woodcutter` does.

### What a plugin can see and do

Everything is on `kewl.Game`:

```java
Game.ready()                              // are we in-game yet
Game.entities()                           // every visible NPC and player, except you
Game.npcs()  Game.players()               // filtered
Game.nearest(list)                        // closest by tile distance
Game.sceneBase()                          // {worldX, worldY} of the loaded chunk's corner
Game.toScene(worldX, worldY)              // world -> scene, or null if not loaded
Game.interactObject(id, worldX, worldY)   // click a tree/rock/door
```

**Two coordinate systems, and mixing them up is the most common bug here.** World coordinates are what
your minimap shows (Lumbridge ≈ 3222, 3218). Scene coordinates are 0–104 within the loaded chunk, which
is what the game's click function wants. Everything on `Game` takes **world** coordinates and converts
internally, so you rarely have to care.

---

## How it works

### The launcher
`launcher/main.cpp` — finds `osclient.exe`, writes the DLL path into it, and calls `LoadLibraryA` on a
remote thread. The oldest, most boring injection there is, in about thirty lines.

### The native half
- `client/offsets.hpp` — every game-specific number, in one file, each with a note on how to re-find it.
- `client/game.hpp` — guarded memory reads, entity enumeration, the world→screen projection, and
  `doAction`.
- `client/jvm.hpp` — starts the JVM and registers the four natives.
- `client/dllmain.cpp` — the overlay window and the 30 fps loop.

The overlay is a **transparent always-on-top window**, not a renderer hook. Hooking would need a detour
library, a graphics API to get right, and it crashes inside someone else's render loop when you get it
wrong. A layered window is forty lines you can read in one sitting. The cost: it looks worse, and it
will not show up in screenshots or recordings. That trade is on purpose.

### Why there is no packet code
Bot clients usually reimplement the game's network protocol — a big table of opcodes and byte layouts
that changes every update and fails silently when it drifts.

KewlKlient does not. `doAction` is the client's own menu-action entry point: the same function that runs
when you right-click a tree and choose "Chop down". We call it with the same arguments and the client
builds and sends the packet itself. Consequences:

- We never need to know the wire format.
- Anti-cheat sees a normally-constructed packet, because it *is* one.
- One function to re-find after an update instead of a hundred opcodes.

---

## When the game updates

Jagex rebuilds the client roughly weekly. Two kinds of number in `offsets.hpp` rot at different speeds:

- **Function RVAs** (`DO_ACTION`, `WORLD_TO_SCREEN`) — assume these are wrong after any patch.
- **Struct offsets** (`ENTITY_SCENE_X`, `SCENE`…) — stabler, but they do move. One of them shifted by
  `0x10` between builds a few weeks apart.

`BUILD_ID` is the fingerprint. If it does not match, **do not just bump it** — every other number was
measured on that build.

### Re-deriving offsets

```powershell
.\tools\ghidra_headless.ps1 -Ghidra "C:\ghidra_11.4" -Script FindOffsets.java
type tools\offsets_found.txt
```

First run analyses 16 MB and takes 10–20 minutes; it is cached after that.

**The method: anchor on a name the client uses for itself, then walk to what you want.** The client ships
a Lua binding layer that registers its own functions by name, and those names are plain text in the
binary — `worldToScreenCoord`, `npcCoord`, `playerCoord`. Find the string, find its references, and you
are inside the registration code for the function you are after. `FindOffsets.java` does the finding;
you open the referencing function and read off the pointer it registers.

**Do not scan for byte patterns.** A byte pattern is a guess about instructions the compiler may
rearrange. When it breaks it does not error — it gives you an address that decompiles into something
plausible and wrong.

Prefer IDA if you own it (better decompiler on this binary): `.\tools\ida_headless.ps1`. It works on a
copy and deletes the stale database when the exe changes, because IDA will happily answer from last
month's build without telling you.

There is also a **Claude skill** in `.claude/skills/deob/` that carries this whole method — if you use
Claude Code, ask it to find an offset and it will follow it.

### Finding a new action

`Game.OPLOC1 = 3` is "first option on a scenery object". Other actions have other numbers. To find one,
attach a debugger to `DO_ACTION`, do the action by hand in game, and read the opcode argument. That is
how every number in this repo was found — none of them were guessed.

---

## Good first contributions

Genuinely useful, roughly easiest first:

1. **Find the nearest tree automatically.** The woodcutter currently needs a hard-coded tile because
   scanning scenery means walking the scene's object grid — about six more offsets. Land this and every
   gathering bot gets easier. *(the highest-value one on this list)*
2. **Draw NPC names.** The name lives behind a pointer on the entity; the `npcName` binding shows the
   chain.
3. **Inventory reading.** Needed by nearly every bot for "am I full yet".
4. **A `walkTo(x, y)` helper.** The walk action is another `doAction` opcode.
5. **Make the panel less ugly.** Low stakes, all GDI, a fine first PR.

Please keep the two rules this codebase is built on: **no packet building**, and **every offset gets a
comment saying how it was found**.

---

## Troubleshooting

**Nothing happens when I press the button.**
Is the game actually running and logged in? The launcher looks for `osclient.exe` by name. If it says the
game refused the DLL, you built 32-bit — the game is x64 and rejects a 32-bit DLL silently.

**The overlay says "java: could not load jvm.dll".**
`java=` in `kewlklient.ini` is wrong. It needs a folder with `bin\server\jvm.dll` under it. A JDK always
has that; some JREs do not.

**The build says "CMake is not installed (or not on your PATH)".**
Install it from the table above. If you just installed it, **restart IntelliJ** — it caches the `PATH`
it was started with, so a new install is invisible to it until then.

**The build says "No CMAKE_CXX_COMPILER could be found".**
No C++ compiler. Install the Build Tools from the table above.

**The overlay says "kewl/KewlKlient not found".**
`kewlklient.jar` must sit next to `kewlklient.dll`. `gradlew dist` puts all four files in `build\dist\`
and now checks they are actually there before claiming success — it used to be possible for the build
to pass while producing no DLL at all.

**Boxes are in the wrong place, or the client crashes on inject.**
The game updated and the offsets moved. See [When the game updates](#when-the-game-updates).

**Boxes flicker.**
Expected — it is a layered window, not a renderer hook. See "The native half".

---

## Licence

**GPL-3.0.** See [`LICENSE`](LICENSE). In plain terms:

- **Anyone can use it**, for anything, including commercially.
- **It has to stay open.** If you distribute a modified version — or anything built on it — you have to
  ship the source under the GPL too. You cannot take this closed.
- **Credit stays with it.** Keep the copyright notices and say what you changed.

If you fork it, a link back here is appreciated on top of what the licence requires.

Copyright (C) 2026 StoneShorts and the KewlKlient contributors.

---

This is a personal-use tool for your own account. Automating a game breaks its rules and can get that
account banned. That is your call to make, and yours to live with.
