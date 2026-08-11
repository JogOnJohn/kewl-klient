package kewl;

/**
 * Every native method in KewlKlient, in one class, so the whole unsafe surface is one screen of code.
 *
 * <p>These are registered from C++ at startup (see {@code client/jvm.hpp}). If you add one here you must
 * add it there too, with a matching JNI signature, or the class will fail to link at the first call with
 * an {@code UnsatisfiedLinkError} naming the method.</p>
 *
 * <p><b>You almost certainly want {@link kewl.api.Game} instead.</b> This class returns flat int arrays
 * with no bounds checks and no meaning attached; the api package turns them into things with names. The
 * only reason to call anything here directly is that you are adding a capability the api does not have
 * yet.</p>
 */
public final class Natives {

    private Natives() {}

    /** True once the game has built its client object -- i.e. you are in-game, not at the login screen. */
    public static native boolean ready();

    /**
     * Every visible entity, seven ints each, flattened:
     * {@code uid, sceneX, sceneY, isPlayer, id, animation, orientation}.
     *
     * <p>{@code id} is the NPC type for NPCs and the combat level for players -- they are different
     * things stored in different places, and packing them into one slot keeps this array rectangular.</p>
     */
    public static native int[] entities();

    /** {@code {worldX, worldY}} of the loaded scene's south-west corner. Empty when nothing is loaded. */
    public static native int[] sceneBase();

    /** You: {@code {uid, sceneX, sceneY, plane, animation, orientation, runEnergy, cycle}}, or empty. */
    public static native int[] local();

    /** All 25 skills: {@code effective[25]}, then {@code base[25]}, then {@code xp[25]}. */
    public static native int[] skills();

    /**
     * Project a point in fine coordinates (tiles &lt;&lt; 7) to screen pixels, using the game's own
     * projection so it stays correct while the camera moves.
     *
     * @return screen x in the high 32 bits, y in the low 32, or {@link Long#MIN_VALUE} if off-screen
     */
    public static native long project(int fineX, int fineHeight, int fineY);

    /** Perform a menu action, in SCENE coordinates. This is the only way KewlKlient acts on the game. */
    public static native void doAction(int sceneX, int sceneY, int opcode, int targetId);

    /** Interact with an NPC by uid; the native side looks its tile up. */
    public static native void interactNpc(int uid, int opcode);

    /** The game's client area on screen: {@code {x, y, width, height}}. */
    public static native int[] viewport();

    /**
     * Put a finished frame on the overlay. {@code px} must be {@code w*h} <b>premultiplied</b> ARGB
     * pixels, top row first -- which is exactly what a {@code BufferedImage.TYPE_INT_ARGB_PRE} holds.
     */
    public static native void present(int[] px, int w, int h);
}
