package kewl;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * The friendly face of the four natives. Plugins should use this and never call {@link KewlKlient}
 * directly.
 *
 * <p><b>Two coordinate systems, and mixing them up is the single most common bug here.</b></p>
 *
 * <ul>
 *   <li><b>World</b> coordinates are what the wiki and your minimap show — Lumbridge is about 3222, 3218.
 *       Use these in configs and anything a human types.</li>
 *   <li><b>Scene</b> coordinates are 0..104, relative to the corner of the chunk the client currently has
 *       loaded. The game's own click function wants these.</li>
 * </ul>
 *
 * <p>{@link #toScene(int, int)} converts, and every method here that takes a position takes WORLD
 * coordinates, so you should rarely have to think about it.</p>
 */
public final class Game {

    private Game() {}

    /** An NPC or another player, as of the last time you asked. */
    public record Entity(int uid, int sceneX, int sceneY, boolean player) {
        public int worldX() { int[] b = KewlKlient.sceneBase(); return b.length == 2 ? b[0] + sceneX : -1; }
        public int worldY() { int[] b = KewlKlient.sceneBase(); return b.length == 2 ? b[1] + sceneY : -1; }
    }

    /** True once you are in-game. Every method below returns something harmless if this is false. */
    public static boolean ready() { return KewlKlient.ready(); }

    /** Everything visible except you. Never null; empty while loading. */
    public static List<Entity> entities() {
        int[] flat = KewlKlient.entities();
        if (flat == null || flat.length < 4) return Collections.emptyList();
        List<Entity> out = new ArrayList<>(flat.length / 4);
        for (int i = 0; i + 3 < flat.length; i += 4) {
            out.add(new Entity(flat[i], flat[i + 1], flat[i + 2], flat[i + 3] != 0));
        }
        return out;
    }

    /** Just the NPCs. */
    public static List<Entity> npcs() {
        List<Entity> out = new ArrayList<>();
        for (Entity e : entities()) if (!e.player()) out.add(e);
        return out;
    }

    /** Just the other players. */
    public static List<Entity> players() {
        List<Entity> out = new ArrayList<>();
        for (Entity e : entities()) if (e.player()) out.add(e);
        return out;
    }

    /** The loaded scene's south-west corner in world tiles, or null while loading. */
    public static int[] sceneBase() {
        int[] b = KewlKlient.sceneBase();
        return b != null && b.length == 2 ? b : null;
    }

    /** Convert a WORLD tile to a SCENE tile, or null if it is outside the loaded chunk. */
    public static int[] toScene(int worldX, int worldY) {
        int[] b = sceneBase();
        if (b == null) return null;
        int sx = worldX - b[0], sy = worldY - b[1];
        if (sx < 0 || sy < 0 || sx > 104 || sy > 104) return null;
        return new int[] { sx, sy };
    }

    // ---------------------------------------------------------------------------------------------
    // Doing things. All of these end up in the game's own menu-action function, which builds and sends
    // the packet for us -- KewlKlient contains no network code at all.
    // ---------------------------------------------------------------------------------------------

    /** Menu action numbers. Add to these as you find them; see README, "Finding a new action". */
    public static final int OPLOC1 = 3;      // first option on scenery: Chop down / Mine / Open / ...

    /**
     * Click a scenery object at a WORLD tile — a tree, a rock, a door.
     *
     * @return false if the tile is not in the loaded scene, in which case nothing was sent.
     */
    public static boolean interactObject(int objectId, int worldX, int worldY) {
        int[] s = toScene(worldX, worldY);
        if (s == null) return false;
        KewlKlient.doAction(s[0], s[1], OPLOC1, objectId);
        return true;
    }

    /** Nearest entity matching a filter, by rough tile distance from the centre of the scene. */
    public static Entity nearest(List<Entity> from) {
        return from.stream()
                   .min(Comparator.comparingInt(e -> Math.abs(e.sceneX() - 52) + Math.abs(e.sceneY() - 52)))
                   .orElse(null);
    }
}
