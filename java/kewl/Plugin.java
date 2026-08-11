package kewl;

/**
 * A plugin. Implement this, add one line to {@code KewlKlient.PLUGINS}, rebuild, restart. That is all
 * there is to it.
 *
 * <p>{@link #tick()} runs about thirty times a second while your plugin is enabled, so do not sleep in
 * it and do not do anything slow. Keep your state in fields and act a little on each call. If you want
 * something to happen every few seconds, remember a timestamp and compare against
 * {@code System.currentTimeMillis()} — the {@code Woodcutter} example does exactly that.</p>
 */
public interface Plugin {

    /** Short name for the overlay panel and log lines. */
    String name();

    /** Whether {@link #tick()} should be called. */
    boolean enabled();

    /** Turn on or off. */
    void setEnabled(boolean on);

    /** Called once when the client starts. Load config here. Default: nothing. */
    default void start() {}

    /** Called ~30x a second while enabled. Keep it fast and non-blocking. */
    void tick();

    /** One short line for the overlay, e.g. "ON  tree=1278". */
    default String status() { return enabled() ? "ON" : "off"; }

    /**
     * Function keys pressed since the last tick: bit 0 is F5, bit 1 F6, bit 2 F7, bit 3 F8. Called even
     * when the plugin is disabled, so you can bind a key that turns yourself on.
     *
     * <p>F1 through F4 are taken by the overlay and never reach here.</p>
     */
    default void keys(int pressed) {}
}
