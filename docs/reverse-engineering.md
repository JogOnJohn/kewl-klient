# Reverse-engineering workflow

This is a learning-oriented, evidence-first workflow for finding a function RVA or struct field after a game-client build changes. It is deliberately slower than guessing and far cheaper than debugging a plausible wrong address.

## Before opening a disassembler

1. Record the SHA-256 of the exact client executable being analyzed.
2. Keep a VM-local analysis copy outside version control.
3. Create or reuse a database only for that exact hash.
4. Check `client/offsets.hpp` for the existing build fingerprint and notes.

Never mix an old analysis database with a newly updated executable. Never commit the executable or database.

## Ghidra route

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\ghidra_headless.ps1 `
  -Ghidra 'C:\Users\vmadmin\tools\ghidra_11.4' `
  -Script FindOffsets.java
```

The first analysis of a client build can take many minutes; later runs reuse the cached project. `tools/offsets_found.txt` is generated scratch output and is ignored by Git.

The script finds *registration functions* for names the client exposes, such as `worldToScreenCoord`, `npcCoord`, and `npcName`. A registration is a starting point, not necessarily the leaf function or field you need. In the Ghidra GUI, follow its references, inspect the function pointer registered next to the name, and then decompile that leaf.

## Establishing confidence

| Status | Meaning |
| --- | --- |
| `DERIVED` | Statically traced from the exact binary hash. |
| `OBSERVED` | Behaviour was seen on the running matching client. |
| `RUNTIME CONFIRMED` | A matching client was checked live and produced the expected result. |
| `NOT VERIFIED` | A lead or reported mapping still needs one of the above checks. |

A safe sequence is: anchor on a client-owned string, identify the registration, trace to the leaf, derive the value, update the build fingerprint and offset in one commit, then validate deliberately against the matching running client.

Do not use byte-pattern scanning as the source of truth. Do not read, record, or commit account/session values while doing research.

## Research-note minimum

For each finding, record the client hash, anchor, registration and leaf locations, the value or chain derived, evidence type, and the next validation step. The [research note template](research/README.md) keeps this compact.
