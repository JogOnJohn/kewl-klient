package kewl;

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.awt.image.DataBufferInt;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.swing.SwingUtilities;

import kewl.api.Game;
import kewl.api.Skills;
import kewl.ui.Sidebar;

/**
 * The client.
 *
 * <p>C++ injects itself into the game, starts a JVM, and calls {@link #start()} once and {@link
 * #tick(int)} about thirty times a second. Everything from here down is ordinary Java.</p>
 *
 * <h2>Adding a plugin</h2>
 *
 * <p>Write a class extending {@link Plugin}, add it to {@link #PLUGINS}, rebuild. That is the whole
 * plugin system -- the list below IS the registry. No scanning, no annotations, no manifest, nothing
 * that can silently fail to find your class.</p>
 */
public final class KewlKlient {

    private KewlKlient() {}

    /**
     * Every plugin, in the order they appear in the control panel.
     *
     * <p><b>Add yours here.</b> One line.</p>
     */
    private static final List<Plugin> PLUGINS = new ArrayList<>(List.of(
            new kewl.plugins.PlayerVisuals(),
            new kewl.plugins.NpcVisuals(),
            new kewl.plugins.Woodcutter()
    ));

    // The overlay image, reused between frames. Reallocating eight megabytes thirty times a second
    // would keep the garbage collector permanently busy for no reason.
    private static BufferedImage canvas;
    private static int[] pixels;
    private static int canvasWidth, canvasHeight;

    /** Called once by the native side after the VM starts. */
    public static void start() {
        System.out.println("KewlKlient: " + PLUGINS.size() + " plugins");

        // Anything a plugin wants on by default, it says so here rather than in its constructor, so
        // "what is on when I start" is one list rather than a hunt through every plugin.
        for (Plugin p : PLUGINS) {
            if (p instanceof kewl.plugins.PlayerVisuals || p instanceof kewl.plugins.NpcVisuals) {
                p.setEnabled(true);
            }
        }

        // Swing has to be built on its own thread. The render loop never touches it again -- the panel
        // polls, rather than the loop pushing, precisely so these two threads share nothing.
        SwingUtilities.invokeLater(() -> {
            try {
                Sidebar.open(PLUGINS);
            } catch (Throwable t) {
                System.out.println("KewlKlient: control panel failed to open: " + t);
            }
        });
    }

    /**
     * One frame: read the world, run the plugins, draw, and hand the result back to be shown.
     *
     * @param keys bitmask of function keys pressed since the last frame; bit 0 is F1, bit 7 is F8
     */
    public static void tick(int keys) {
        Skills.newFrame();
        Game.refresh();

        if (keys != 0) {
            for (Plugin p : PLUGINS) {
                int k = p.hotkey();
                if (k >= 0 && k < 8 && (keys & (1 << k)) != 0) p.toggle();
            }
        }

        for (Plugin p : PLUGINS) {
            if (!p.isEnabled()) continue;
            try {
                p.tick();
            } catch (Throwable t) {
                // One broken plugin must not stop the other two, and must never reach the game.
                System.out.println("[" + p.name() + "] tick threw: " + t);
            }
        }

        render();
    }

    /** Draw every enabled plugin's overlay and present it. */
    private static void render() {
        int[] view = Natives.viewport();
        if (view.length != 4) return;
        int w = view[2], h = view[3];
        if (w <= 0 || h <= 0) return;

        if (canvas == null || w != canvasWidth || h != canvasHeight) {
            // TYPE_INT_ARGB_PRE, not TYPE_INT_ARGB: the layered window wants premultiplied alpha, and
            // drawing straight into the right format means nothing has to convert it later.
            canvas = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB_PRE);
            pixels = ((DataBufferInt) canvas.getRaster().getDataBuffer()).getData();
            canvasWidth = w;
            canvasHeight = h;
        }

        Arrays.fill(pixels, 0);                 // fully transparent -- the game shows through

        Graphics2D g = canvas.createGraphics();
        try {
            Sidebar.prettyText(g);
            for (Plugin p : PLUGINS) {
                if (!p.isEnabled()) continue;
                try {
                    p.render(g);
                } catch (Throwable t) {
                    System.out.println("[" + p.name() + "] render threw: " + t);
                }
            }
        } finally {
            g.dispose();
        }

        Natives.present(pixels, w, h);
    }

    /** One line per enabled plugin. The native side shows this if Java is up but nothing has drawn. */
    public static String status() {
        StringBuilder sb = new StringBuilder();
        for (Plugin p : PLUGINS) {
            if (!p.isEnabled()) continue;
            sb.append(p.name());
            String s = p.status();
            if (s != null && !s.isEmpty()) sb.append(": ").append(s);
            sb.append('\n');
        }
        return sb.toString();
    }
}
