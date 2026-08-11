package kewl;

import java.util.ArrayList;
import java.util.List;

/**
 * The entry point the native side calls into, and the only class that talks to it.
 *
 * <p>The C++ half registers the four {@code native} methods below at startup, then calls {@link #start()}
 * once and {@link #tick(int)} about thirty times a second. Everything else in KewlKlient is ordinary Java
 * and you can change it without touching a compiler flag.</p>
 *
 * <p><b>Why so few natives.</b> Every native method is a place where a mistake crashes the game instead of
 * throwing an exception. Four is enough to write real bots: see what is around you, know where you are,
 * click something, and know whether you are logged in. If you want a fifth, check first that you cannot
 * build it out of these -- most things can be.</p>
 */
public final class KewlKlient {

    private KewlKlient() {}

    // ---------------------------------------------------------------------------------------------
    // The native surface. Implemented in client/jvm.hpp. Do not rename without changing both sides.
    // ---------------------------------------------------------------------------------------------

    /** Flat {uid, sceneX, sceneY, isPlayer} per visible entity. Use {@link Game#entities()} instead. */
    static native int[] entities();

    /** {worldX, worldY} of the loaded scene's south-west corner, or an empty array. */
    static native int[] sceneBase();

    /** Click something, in SCENE coordinates. Use the helpers on {@link Game}. */
    static native void doAction(int sceneX, int sceneY, int opcode, int targetId);

    /** True once you are actually in-game. */
    static native boolean ready();

    // ---------------------------------------------------------------------------------------------
    // Plugins
    // ---------------------------------------------------------------------------------------------

    /** Add yours here. That is the whole registration mechanism -- there is no scanning or config. */
    private static final List<Plugin> PLUGINS = new ArrayList<>(List.of(
            new kewl.plugins.Woodcutter()
    ));

    public static void start() {
        System.out.println("[KewlKlient] up, " + PLUGINS.size() + " plugin(s)");
        for (Plugin p : PLUGINS) {
            try {
                p.start();
            } catch (Throwable t) {
                System.out.println("[KewlKlient] " + p.name() + " failed to start: " + t);
            }
        }
    }

    /**
     * One frame. {@code keys} carries which function keys were pressed since the last tick -- bit 0 is
     * F5, bit 1 F6, and so on up to F8. F1..F4 belong to the overlay itself.
     *
     * <p>A plugin that throws is caught and reported, never propagated: one broken plugin must not take
     * the client, or the game, down with it.</p>
     */
    public static void tick(int keys) {
        if (!ready()) return;
        for (Plugin p : PLUGINS) {
            try {
                p.keys(keys);
                if (p.enabled()) p.tick();
            } catch (Throwable t) {
                System.out.println("[KewlKlient] " + p.name() + " threw: " + t);
            }
        }
    }

    /** One line per plugin for the overlay panel. */
    public static String status() {
        StringBuilder sb = new StringBuilder();
        for (Plugin p : PLUGINS) sb.append(p.name()).append(": ").append(p.status()).append('\n');
        return sb.toString();
    }
}
