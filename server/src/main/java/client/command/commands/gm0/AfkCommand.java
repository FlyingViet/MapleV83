package client.command.commands.gm0;

import client.Character;
import client.Client;
import client.command.Command;
import server.life.Monster;
import server.maps.MapObject;
import tools.PacketCreator;

/**
 * MapleV83: the iPhone app sends "@mv83afk on" when it goes to the background
 * and "@mv83afk off" when it comes back. The character stays in the world with
 * an AFK sign, other players take over its monsters, and the server keeps the
 * session for Client.AFK_HOLD_MS while the app can't answer pings.
 */
public class AfkCommand extends Command {
    private static final String SIGN = "AFK";

    {
        setDescription("");
    }

    @Override
    public void execute(Client c, String[] params) {
        Character player = c.getPlayer();
        boolean away = params.length == 0 || !params[0].equalsIgnoreCase("off");

        if (away) {
            if (!c.isAfk()) {
                c.setAfk(true);

                // An existing message on the chalkboard stays.
                if (player.getChalkboard() == null) {
                    player.setChalkboard(SIGN);
                    player.getMap().broadcastMessage(PacketCreator.useChalkboard(player, false));
                }

                // A frozen phone can't move its monsters.
                player.releaseControlledMonsters();
            }
            return;
        }

        c.setAfk(false);

        if (SIGN.equals(player.getChalkboard())) {
            player.setChalkboard(null);
            player.getMap().broadcastMessage(PacketCreator.useChalkboard(player, true));
        }

        // Monsters left without anyone to move them come back to this player.
        for (MapObject mmo : player.getMap().getMonsters()) {
            ((Monster) mmo).aggroUpdateController();
        }

        // Always answer, so the app knows its connection is alive.
        c.sendPacket(PacketCreator.enableActions());
    }
}
