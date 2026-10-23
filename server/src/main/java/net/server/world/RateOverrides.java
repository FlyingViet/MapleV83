package net.server.world;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Properties;

/**
 * MapleV83: world rates an admin sets in game (@setrates) outlive a restart. They
 * are kept beside config.yaml in rate-overrides.properties and take precedence
 * over its exp_rate / drop_rate / boss_drop_rate / meso_rate.
 */
public final class RateOverrides {
    private static final Path FILE = Path.of("rate-overrides.properties");

    private RateOverrides() {
    }

    private static Properties load() {
        Properties props = new Properties();

        if (Files.exists(FILE)) {
            try (InputStream in = Files.newInputStream(FILE)) {
                props.load(in);
            } catch (IOException ignored) {
            }
        }

        return props;
    }

    public static synchronized int get(int world, String key, int fallback) {
        String value = load().getProperty("world" + world + "." + key);

        try {
            return value != null ? Math.max(1, Integer.parseInt(value.trim())) : fallback;
        } catch (NumberFormatException e) {
            return fallback;
        }
    }

    public static synchronized void save(int world, int exp, int drop, int meso) throws IOException {
        Properties props = load();
        props.setProperty("world" + world + ".exp", Integer.toString(exp));
        props.setProperty("world" + world + ".drop", Integer.toString(drop));
        props.setProperty("world" + world + ".bossdrop", Integer.toString(drop));
        props.setProperty("world" + world + ".meso", Integer.toString(meso));

        try (OutputStream out = Files.newOutputStream(FILE)) {
            props.store(out, "MapleV83 world rates set in game with @setrates");
        }
    }
}
