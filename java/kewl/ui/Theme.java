package kewl.ui;

import java.awt.Color;
import java.awt.Font;

/**
 * The colours and fonts, in one place, so the client looks like one thing rather than nine.
 *
 * <p>Change them here and both the control panel and every overlay follow. That is the entire reason
 * this class exists -- the first version of this client had colours written inline in six files and
 * restyling it meant finding all six.</p>
 */
public final class Theme {

    private Theme() {}

    // -- the panel
    public static final Color BACKGROUND = new Color(28, 28, 32);
    public static final Color SURFACE    = new Color(38, 38, 44);
    public static final Color SURFACE_HI = new Color(50, 50, 58);
    public static final Color BORDER     = new Color(58, 58, 66);
    public static final Color TEXT       = new Color(226, 226, 232);
    public static final Color TEXT_DIM   = new Color(150, 150, 160);
    public static final Color ACCENT     = new Color(120, 190, 255);
    public static final Color ON         = new Color(110, 220, 140);
    public static final Color OFF        = new Color(120, 120, 130);
    public static final Color WARN       = new Color(255, 140, 120);

    // -- the overlay. Semi-transparent so the game stays readable underneath.
    public static final Color HUD_BACK   = new Color(20, 20, 24, 190);
    public static final Color HUD_BORDER = new Color(90, 90, 105, 190);

    public static final Font UI     = new Font("Segoe UI", Font.PLAIN, 12);
    public static final Font UI_BOLD = new Font("Segoe UI", Font.BOLD, 12);
    public static final Font TITLE  = new Font("Segoe UI", Font.BOLD, 15);
    public static final Font MONO   = new Font("Consolas", Font.PLAIN, 12);

    /** The same colour at a different opacity. Handy for "fill faintly, outline solid". */
    public static Color alpha(Color c, int a) {
        return new Color(c.getRed(), c.getGreen(), c.getBlue(), Math.max(0, Math.min(255, a)));
    }
}
