# KewlKlient contributor guide

KewlKlient is a Windows x64 Java/C++ project. The live development checkout is kept inside the JogOnJohn VM; GitHub is the shared remote. Do not create a second working source mirror on the host machine.

## Start here

Read these in order before changing code:

1. `README.md` for the project overview and user-facing behaviour.
2. `docs/README.md` for the team documentation map.
3. `docs/development.md` before building or changing the environment.
4. `docs/reverse-engineering.md` before touching `client/offsets.hpp`.

## Repository map

- `launcher/` - Windows launcher.
- `client/` - C++ DLL, memory access, overlay, JVM bridge, and build-specific offsets.
- `java/` - plugin API, overlay UI, configuration, and built-in plugins.
- `tools/` - repeatable analysis helpers; generated analysis output is ignored.
- `docs/` - maintained team context and operating instructions.

## Non-negotiable safety and quality rules

- Work in the VM checkout. Do not install a host-side source mirror.
- Keep source, build output, Ghidra projects, copied game binaries, databases, and logs separate. Generated material belongs under ignored directories such as `build/` or `tools/` scratch paths.
- Never commit a game executable, an analysis database, credentials, account data, session tokens, or private keys. Do not log them either.
- Treat every native offset as build-specific. Change `BUILD_ID` in the same commit as any offset set.
- A static derivation is not runtime confirmation. Mark unverified work plainly and record how it can be validated.
- Do not build game packets. Use the client's existing menu-action route where the project already does.
- Do not launch, inject into, restart, or otherwise change a running game client unless the task calls for it explicitly.

## Working agreement

1. Begin by checking `git status --short`, branch, and the current game-binary hash.
2. Make one coherent change at a time. Keep generated output out of the diff.
3. Run the narrowest relevant verification; for source changes, also run the distribution build when practical.
4. Record what was derived, observed, or still unverified in the commit and relevant docs.
5. Commit cohesive known-good work and push it to the shared remote.

## Commands

```powershell
git status --short
git log --oneline -5
.\gradlew.bat --no-daemon dist --console=plain
```

See `docs/development.md` for the required toolchain and `docs/reverse-engineering.md` for the offset workflow.
