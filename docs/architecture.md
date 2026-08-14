# Architecture

KewlKlient has a deliberately small split: Java owns features and drawing; native C++ owns the narrow boundary to the game client.

```text
KewlKlient.exe
    |
    | starts the approved launch/injection flow
    v
kewlklient.dll  <---->  running osclient.exe
    |
    | starts a JVM and exposes a small native API
    v
kewlklient.jar
    |
    +-- kewl.api       named game-facing Java API
    +-- kewl.plugins   built-in and team plugins
    +-- kewl.ui        Java2D HUD and Swing control panel
    +-- kewl.config    plugin settings and controls
```

## Native layer

`client/dllmain.cpp` coordinates the DLL lifetime and frame loop. `offsets.hpp` holds build-specific values; `game.hpp` performs guarded reads and invokes existing menu actions; `jvm.hpp` starts the JVM and registers Java native methods; `overlay.hpp` displays Java-rendered frames.

The C++ surface is the risky part. Keep it small, explain any new native method, and keep game-build findings adjacent to `offsets.hpp`.

## Java layer

`java/kewl/KewlKlient.java` assembles the plugin list. Plugins extend `Plugin`; `tick()` decides or acts and `render(Graphics2D)` draws. The API packages give plugins names such as `Game`, `Entity`, and `Actions` instead of raw pointers.

A feature should usually be implemented in Java. Add C++ only when the Java API genuinely cannot express the needed, reviewed capability.

## Build output

Gradle compiles the Java JAR and drives CMake for the x64 launcher and DLL. The four runtime files live together in `build/dist/`:

- `KewlKlient.exe`
- `kewlklient.dll`
- `kewlklient.jar`
- `kewlklient.ini`

Everything under `build/` is generated and intentionally ignored by Git.
