package kewl.plugins;

import kewl.Game;
import kewl.Plugin;

import java.io.File;
import java.io.FileInputStream;
import java.util.Properties;

/**
 * The example plugin, and deliberately the dumbest thing that actually works: it re-clicks one tree on a
 * timer.
 *
 * <p>That really is the whole bot. The game ignores a click on something you are already interacting
 * with, so clicking every few seconds costs nothing and picks you back up after the tree respawns or you
 * get bumped. Toggle it with <b>F5</b>.</p>
 *
 * <p>Configure it in {@code kewlklient.ini}, next to the DLL:</p>
 *
 * <pre>
 * [woodcutter]
 * tree=1278     ; object id — 1278 is a plain tree
 * x=3163        ; WORLD x of the tile the tree stands on
 * y=3441        ; WORLD y
 * </pre>
 *
 * <p><b>Why you have to type the tile in.</b> Finding the nearest tree by yourself means walking the
 * scene's object grid, which needs about six offsets this client does not have yet. That is the single
 * best first contribution to KewlKlient and it is written up in the README under "Good first
 * contributions" — do that and this plugin becomes three lines shorter and a lot smarter.</p>
 */
public final class Woodcutter implements Plugin {

    private static final int F5 = 1;               // bit 0 of the key mask
    private static final long INTERVAL_MS = 4000;

    private boolean enabled;
    private int treeId = 1278;
    private int worldX, worldY;
    private long lastClick;

    @Override public String name() { return "woodcutter"; }
    @Override public boolean enabled() { return enabled; }
    @Override public void setEnabled(boolean on) { enabled = on; }

    @Override
    public void keys(int pressed) {
        if ((pressed & F5) != 0) enabled = !enabled;
    }

    @Override
    public void start() {
        // The ini sits next to the DLL, which is also the working directory the game was started from.
        File ini = new File("kewlklient.ini");
        if (!ini.isFile()) return;                 // no config: stay idle and say so in status()
        Properties p = new Properties();
        try (FileInputStream in = new FileInputStream(ini)) {
            p.load(in);                            // Properties ignores the [woodcutter] header line
        } catch (Exception e) {
            System.out.println("[woodcutter] could not read kewlklient.ini: " + e);
            return;
        }
        treeId = intOr(p, "tree", treeId);
        worldX = intOr(p, "x", 0);
        worldY = intOr(p, "y", 0);
    }

    @Override
    public void tick() {
        if (worldX == 0 && worldY == 0) return;    // nothing configured

        long now = System.currentTimeMillis();
        if (now - lastClick < INTERVAL_MS) return;

        // interactObject returns false when the tile is outside the loaded scene — you walked away, or
        // the world has not finished loading. Do not reset the timer in that case; just try again.
        if (Game.interactObject(treeId, worldX, worldY)) lastClick = now;
    }

    @Override
    public String status() {
        if (worldX == 0 && worldY == 0) return "no tile set — edit kewlklient.ini";
        return (enabled ? "ON " : "off") + "  tree=" + treeId + " @ " + worldX + "," + worldY;
    }

    private static int intOr(Properties p, String key, int fallback) {
        try {
            String v = p.getProperty(key);
            return v == null ? fallback : Integer.parseInt(v.trim().split("\\s*;")[0].trim());
        } catch (RuntimeException e) {
            return fallback;
        }
    }
}
