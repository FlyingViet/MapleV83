package client.command.commands.gm0;

import client.Character;
import client.Client;
import client.command.Command;
import net.server.world.World;

/**
 * MapleV83: what the iPhone/Mac client needs to know about this character and
 * world, as one machine-readable line it hides from the chat log:
 * "[MV83] gm=6 exp=10 drop=10 meso=10". It asks on entering the game.
 */
public class Mv83InfoCommand extends Command {
    {
        setDescription("Client info line (used by the MapleV83 client).");
    }

    public static String line(Client c) {
        Character player = c.getPlayer();
        World world = c.getWorldServer();

        return "[MV83] gm=" + player.gmLevel() + " exp=" + world.getExpRate() + " drop=" + world.getDropRate()
                + " meso=" + world.getMesoRate();
    }

    @Override
    public void execute(Client c, String[] params) {
        c.getPlayer().yellowMessage(line(c));
    }
}
