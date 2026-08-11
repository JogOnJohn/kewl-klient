package kewl.config;

import java.awt.Color;

/**
 * One knob on a plugin's config panel.
 *
 * <p>A setting knows its own type, so the control panel can build the right widget for it without every
 * plugin having to write any Swing. Declare them in your plugin's constructor and read them in
 * {@code tick()}; the panel wires itself up.</p>
 *
 * <p>Not generic on purpose. A {@code Setting<T>} reads better in isolation and turns the panel into a
 * pile of unchecked casts, because the panel handles a heterogeneous list and has to switch on the type
 * anyway.</p>
 */
public final class Setting {

    /** What kind of value this holds, and therefore which control the panel draws. */
    public enum Kind { BOOL, INT, TEXT, COLOR }

    private final String key, label, description;
    private final Kind kind;
    private final int min, max;
    private Object value;

    Setting(String key, String label, String description, Kind kind, Object value, int min, int max) {
        this.key = key;
        this.label = label;
        this.description = description;
        this.kind = kind;
        this.value = value;
        this.min = min;
        this.max = max;
    }

    /** The name you look it up by. */
    public String key() { return key; }

    /** The name shown on the panel. */
    public String label() { return label; }

    /** The tooltip. Empty for none. */
    public String description() { return description; }

    public Kind kind() { return kind; }

    /** Lower bound, for a number. */
    public int min() { return min; }

    /** Upper bound, for a number. */
    public int max() { return max; }

    public boolean asBool() { return value instanceof Boolean b && b; }

    public int asInt() { return value instanceof Integer i ? i : 0; }

    public String asText() { return value == null ? "" : String.valueOf(value); }

    public Color asColor() { return value instanceof Color c ? c : Color.WHITE; }

    /** Set it. Out-of-range numbers are clamped rather than rejected, so a slider cannot wedge. */
    public void set(Object v) {
        if (kind == Kind.INT && v instanceof Integer i) {
            value = Math.max(min, Math.min(max, i));
        } else {
            value = v;
        }
    }
}
