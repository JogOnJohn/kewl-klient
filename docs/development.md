# Development on the VM

The JogOnJohn VM is the active development environment. Its repository checkout is the only working source copy; GitHub is the team remote. Keep the host free of a duplicate checkout and use GitHub to share source and docs.

## Directory layout

```text
C:\Users\vmadmin\IdeaProjects\kewl-klient\         source and versioned docs
C:\Users\vmadmin\IdeaProjects\kewl-klient\build\   generated build and analysis cache
C:\Users\vmadmin\tools\                              VM-local tools and analysis copies
```

Do not put a copy of `osclient.exe`, Ghidra databases, or other large generated files in the repository. The `.gitignore` rules are a safety net, not permission to leave unclear scratch files around.

## Required toolchain

- Windows x64
- JDK 17 or newer for the Gradle/Java build
- CMake
- Visual Studio Build Tools with the Desktop development with C++ workload
- JDK 21 for the installed Ghidra release, when using Ghidra

The JDK used to build should be explicit in a terminal session when more than one JDK is installed:

```powershell
$env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-17.0.19.10-hotspot'
$env:Path = "$env:JAVA_HOME\bin;$env:Path"
.\gradlew.bat --no-daemon dist --console=plain
```

Use the installed JDK path that actually exists on the VM if it has changed; do not edit machine-wide environment variables just to build the project.

## First-session checklist

```powershell
Set-Location C:\Users\vmadmin\IdeaProjects\kewl-klient
git status --short
git remote -v
git log --oneline -5
java -version
cmake --version
.\gradlew.bat --no-daemon dist --console=plain
```

A successful `dist` build must produce the four files described in [Architecture](architecture.md#build-output). Building is safe; launching or interacting with the game client is a separate, explicit step.

## Before committing

1. Review `git diff --check` and `git diff`.
2. Run the narrowest relevant test or build.
3. Confirm `git status --short` contains only intended source/docs changes.
4. Use a focused, imperative commit subject, for example `docs: add VM onboarding guide`.
5. Push the commit from the authenticated VM checkout.

Never commit generated artifacts, copied game binaries, session/account data, or local credentials.
