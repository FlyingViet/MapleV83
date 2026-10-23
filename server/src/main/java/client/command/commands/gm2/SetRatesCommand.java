package client.command.commands.gm2;

import client.Character;
import client.Client;
import client.command.Command;
import client.command.commands.gm0.Mv83InfoCommand;
import net.server.world.RateOverrides;
import net.server.world.World;
import tools.PacketCreator;

/**
 * MapleV83: sets the world's EXP, drop (and boss drop) and meso rates together and
 * keeps them across restarts. The client's Admin panel sends it.
 */
public class SetRatesCommand extends Command {
    {
        setDescription("Set the world's EXP, drop and meso rates: @setrates <exp> <drop> <meso>.");
    }

    private static int rate(String value) {
        return Math.max(1, Math.min(999, Integer.parseInt(value)));
    }

    @Override
    public void execute(Client c, String[] params) {
        Character player = c.getPlayer();

        if (params.length < 3) {
            player.yellowMessage("Syntax: @setrates <exp> <drop> <meso>");
            return;
        }

        int exp, drop, meso;
        try {
            exp = rate(params[0]);
            drop = rate(params[1]);
            meso = rate(params[2]);
        } catch (NumberFormatException e) {
            player.yellowMessage("Syntax: @setrates <exp> <drop> <meso>");
            return;
        }

        World world = c.getWorldServer();
        world.setExpRate(exp);
        world.setDropRate(drop);
        world.setBossDropRate(drop);
        world.setMesoRate(meso);

        try {
            RateOverrides.save(world.getId(), exp, drop, meso);
        } catch (Exception e) {
            player.yellowMessage("Rates changed, but could not be saved for the next restart: " + e.getMessage());
        }

        world.broadcastPacket(PacketCreator.serverNotice(6, "[Rates] EXP " + exp + "x, Drop " + drop + "x, Meso " + meso + "x."));
        player.yellowMessage(Mv83InfoCommand.line(c));
    }
}
