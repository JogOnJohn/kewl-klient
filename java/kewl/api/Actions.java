package kewl.api;

import java.awt.Point;

import kewl.Natives;

/**
 * Doing things.
 *
 * <p>Every method here goes through the game's own menu-action function -- the same one that runs when
 * you right-click something and pick an option. We never build a network packet. The client builds and
 * sends it, which is why this client does not contain a protocol table and does not break every time
 * the protocol changes.</p>
 *
 * <h2>Two things worth knowing before you use this</h2>
 *
 * <p><b>Actions take world coordinates.</b> The game wants scene coordinates and the conversion happens
 * here, once. If a tile is not in the loaded chunk these methods do nothing and return false, which is
 * the honest answer -- you cannot click a tile the client has not loaded.</p>
 *
 * <p><b>Do not spam them.</b> These run on the overlay's thread, not the game's. That is fine at the
 * pace a human clicks and it is asking for trouble in a tight loop. Every plugin in this repo rate
 * limits itself; yours should too.</p>
 */
public final class Actions {

    private Actions() {}

    // The client's own menu action numbers. These came from hooking the game's action function and
    // clicking things by hand -- see client/offsets.hpp, which explains how to find more.
    private static final int OPLOC1  = 3;
    private static final int OPNPC1  = 9;
    private static final int OP_WALK = 31;

    /**
     * Walk to a world tile. The game pathfinds; we only say where.
     *
     * @return false when that tile is not in the loaded scene
     */
    public static boolean walkTo(int worldX, int worldY) {
        Point s = Game.toScene(worldX, worldY);
        if (s == null) return false;
        Natives.doAction(s.x, s.y, OP_WALK, 0);
        return true;
    }

    /** Walk to a world tile. */
    public static boolean walkTo(WorldPoint p) {
        return p != null && walkTo(p.x(), p.y());
    }

    /**
     * Click a piece of scenery -- a tree, a rock, a door -- taking its first option.
     *
     * <p>"First option" is Chop down on a tree and Mine on a rock, so this one method covers most
     * gathering. You need the object's id and the tile it stands on; the README's contribution list has
     * finding those automatically as its top item.</p>
     *
     * @return false when that tile is not in the loaded scene
     */
    public static boolean object(int objectId, int worldX, int worldY) {
        Point s = Game.toScene(worldX, worldY);
        if (s == null) return false;
        Natives.doAction(s.x, s.y, OPLOC1, objectId);
        return true;
    }

    /** Click a piece of scenery at a world point. */
    public static boolean object(int objectId, WorldPoint p) {
        return p != null && object(objectId, p.x(), p.y());
    }

    /**
     * Take an NPC's first option -- Attack on anything hostile, but Talk-to on a shopkeeper, so a bot
     * that assumes this always attacks will cheerfully strike up a conversation with a cow.
     */
    public static void npc(Entity npc) {
        if (npc != null) Natives.interactNpc(npc.uid(), OPNPC1);
    }

    /**
     * Take one of an NPC's five options, numbered 1..5 as they appear in the right-click menu.
     *
     * @param option 1..5; anything else is ignored rather than sent, because an out-of-range opcode
     *               lands on some unrelated action rather than failing
     */
    public static void npc(Entity npc, int option) {
        if (npc == null || option < 1 || option > 5) return;
        Natives.interactNpc(npc.uid(), OPNPC1 + option - 1);
    }
}
