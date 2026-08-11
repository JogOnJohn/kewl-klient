package kewl;

import java.awt.Graphics2D;

import kewl.config.Config;

/**
 * Something that runs, draws, or both.
 *
 * <p>Extend it, override what you need, add one line to {@link KewlKlient#PLUGINS}. There is no
 * scanning, no annotations and no manifest -- the list is the registry, and you can read it.</p>
 *
 * <pre>{@code
 *   public final class Waver extends Plugin {
 *
 *       public Waver() {
 *           config.number("radius", "Radius", "How far to look", 5, 1, 15);
 *       }
 *
 *       @Override public String name()        { return "Waver"; }
 *       @Override public String description() { return "Says hello to the nearest npc."; }
 *
 *       @Override
 *       public void tick() {
 *           Entity npc = Npcs.nearestWithin(config.number("radius"));
 *           if (npc != null) System.out.println("hello " + npc);
 *       }
 *
 *       @Override
 *       public void render(Graphics2D g) {
 *           g.drawString("waving", 20, 100);
 *       }
 *   }
 * }</pre>
 *
 * <h2>The two methods, and why they are separate</h2>
 *
 * <p>{@link #tick()} decides things and acts. {@link #render(Graphics2D)} draws and must decide nothing.
 * Both run every frame, tick first, on the same thread. Keeping them apart means you can turn the
 * drawing off without changing behaviour, and it stops the classic bug where a bot's logic silently
 * depends on something only the drawing code worked out.</p>
 *
 * <p><b>Neither may block.</b> They run on the overlay thread about thirty times a second. Sleeping in
 * one freezes the overlay. For "do something every few seconds", keep a timestamp field and compare it
 * -- {@code kewl.plugins.Woodcutter} shows the pattern.</p>
 */
public abstract class Plugin {

    /** This plugin's settings. Declare them in your constructor; the control panel draws them for you. */
    public final Config config = new Config();

    private boolean enabled;

    /** The name shown in the control panel. */
    public abstract String name();

    /** One line describing what it does, shown under the name. */
    public String description() { return ""; }

    /**
     * Which function key toggles this, as an index: 0 is F1, 7 is F8. Return -1 for none.
     *
     * <p>Nothing stops two plugins claiming the same key -- they will both toggle, which is
     * occasionally what you want.</p>
     */
    public int hotkey() { return -1; }

    /** Whether it is currently running. */
    public final boolean isEnabled() { return enabled; }

    /** Turn it on or off, firing {@link #onEnable()} / {@link #onDisable()} on a real change. */
    public final void setEnabled(boolean on) {
        if (on == enabled) return;
        enabled = on;
        try {
            if (on) onEnable(); else onDisable();
        } catch (Throwable t) {
            // A plugin misbehaving on a toggle must not take the client with it.
            System.out.println("[" + name() + "] " + (on ? "onEnable" : "onDisable") + " threw: " + t);
        }
    }

    /** Flip it. */
    public final void toggle() { setEnabled(!enabled); }

    /** Called when it is switched on. Reset your state here, not in the constructor. */
    protected void onEnable() {}

    /** Called when it is switched off. */
    protected void onDisable() {}

    /** Called every frame while enabled. Decide and act here. Must not block. */
    public void tick() {}

    /**
     * Called every frame while enabled, after every plugin has ticked. Draw here and nothing else.
     *
     * <p>The graphics context is a normal {@link Graphics2D} over the whole game window, already set up
     * for antialiasing, with (0,0) at the top-left of the game's client area. Anything Java2D can do
     * works: shapes, gradients, alpha, fonts, images.</p>
     */
    public void render(Graphics2D g) {}

    /** A short line for the control panel's status column. Empty for none. */
    public String status() { return ""; }
}
