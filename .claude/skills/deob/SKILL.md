---
name: deob
description: Reverse-engineer the OSRS client to find or re-derive an offset, function RVA, or struct field for KewlKlient. Use when an offset broke after a game update, or when adding a feature that needs data the client does not expose yet.
---

# Finding things in the game binary

You are helping someone re-derive an offset in a 16 MB stripped x64 binary that gets rebuilt weekly.
This skill is the method that works, and — just as importantly — the ways of working that look
productive and are not.

## The rule that matters most

**Anchor on something the client names itself, then walk to the thing you want.**

The client ships a Lua binding layer that registers its own functions by name. Those names are plain
text in the binary: `worldToScreenCoord`, `npcCoord`, `playerCoord`, `npcName`, `getNpcObj`. Find the
string, find what references it, and you are standing inside the registration code for exactly the
function you want.

Other good anchors, in rough order of how much they are worth:

1. **A name string the client uses for itself** (above). Best, because the client is describing itself.
2. **A distinctive constant.** A function that references both `0x2B0` and `0x3FF` is one function out
   of 218 that mention `0x2B0` alone. Intersecting two weak signals gives one strong one.
3. **Call-graph position.** "The only caller of X", "the function that calls both Y and Z".
4. **Size and shape.** A 184-byte function ending in `shr; and; ret` is an accessor.

## What not to do

**Do not scan for byte patterns.** A byte pattern is a guess about instructions the compiler may
rearrange on the next build. When it breaks it does not fail — it hands you an address that decompiles
into something plausible and wrong, and you lose a day. Blind scanning also cannot find anything hidden
behind a pointer, which is most of what you want.

**Do not trust a stale database.** IDA reuses the `.i64` beside the binary and will not warn you it is
from last month's build. Ghidra keeps its project. After any update, verify the binary hash first.

**Do not read a default as if it were live.** If the project has per-build overrides, the constant in
the header is the *oldest* value, not the running one. Check what actually applies before quoting a
number — this specific mistake has cost real days on projects like this.

## The workflow

```powershell
# 1. First time, or after a game update: analyse the binary (slow once, cached after)
.\tools\ghidra_headless.ps1 -Ghidra "C:\ghidra_11.4" -Script FindOffsets.java

# 2. Read the result
type tools\offsets_found.txt
```

`FindOffsets.java` prints, for each binding name, the functions that reference it. That referencing
function is the **registration**, not the leaf you want — open it and find the function pointer it
registers next to the name. That pointer is your target.

Then put the RVA in `client/offsets.hpp` and **update `BUILD_ID` in the same commit**. Every number in
that file was measured on one build; mixing values from two builds is how you get a crash that looks
like a logic bug.

## Struct fields

Function RVAs move every build. Struct offsets move less, but they do move — an entity table moved by
`0x10` between builds a few weeks apart.

To find a field: find a function that *uses* it, and read what it does with it. If you want "the npc's
name", find the `npcName` binding, decompile the leaf, and read off the chain — it will be something
like `*(*(entity + 0x730) + 0x10)`. That is worth more than any amount of staring at a memory dump,
because it is the client telling you its own layout.

## Confidence, and saying so

Mark anything you have not confirmed against the running game as **NOT VERIFIED**, in the comment next
to it, with what would confirm it. A wrong offset fails *silently* — it reads a plausible number from
the wrong place. An admitted unknown is much cheaper than a confident wrong answer that reads as
evidence.

When two sources disagree, prefer them in this order:

1. What the running client was **observed doing** (a capture, a log line).
2. What the running client **reads right now** (a live memory read).
3. The game **cache** on disk.
4. **Anything written down elsewhere** — a wiki, a forum post, an older copy of this repo. It was
   measured on a different build, so treat those numbers as names and shapes, not as values.

## When you are done

Say plainly which of these you did: derived it from the binary, confirmed it against a running client,
or took it from something already written down. Those are three different levels of certainty and the
next person cannot tell them apart from the code alone.
