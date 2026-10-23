package client.command.commands.gm0;

import client.Character;
import client.Client;
import client.command.Command;
import server.maps.FieldLimit;
import server.maps.MapleMap;
import server.maps.MiniDungeonInfo;
import server.maps.Portal;

/**
 * MapleV83: the iPhone world map's free teleport. "@mv83warp <mapid>" moves the
 * player to that map's spawn point, from anywhere a warp would be allowed.
 */
public class Mv83WarpCommand extends Command {
    {
        setDescription("");
    }

    @Override
    public void execute(Client c, String[] params) {
        Character player = c.getPlayer();
        int mapId;

        try {
            mapId = Integer.parseInt(params[0]);
        } catch (Exception e) {
            player.yellowMessage("That place can't be reached.");
            return;
        }

        if (!player.isAlive()) {
            player.dropMessage(1, "You can't teleport while dead.");
            return;
        }

        if (player.getEventInstance() != null || MiniDungeonInfo.isDungeonMap(player.getMapId())
                || FieldLimit.CANNOTMIGRATE.check(player.getMap().getFieldLimit())
                || c.getCM() != null || player.getTrade() != null || player.getMiniGame() != null
                || player.getPlayerShop() != null || player.getHiredMerchant() != null) {
            player.dropMessage(1, "You can't teleport from here right now.");
            return;
        }

        if (mapId == player.getMapId()) {
            return;
        }

        MapleMap target = null;
        Portal spawn = null;

        try {
            target = c.getChannelServer().getMapFactory().getMap(mapId);
            // Portal 0 is the main spawn (return scrolls and taxis land there, beside the
            // Maple Administrator); fall back to a random spawn when a map has no portal 0.
            spawn = target != null ? target.getPortal(0) : null;
            if (spawn == null && target != null) {
                spawn = target.getRandomPlayerSpawnpoint();
            }
        } catch (Exception e) {
            spawn = null;
        }

        if (target == null || spawn == null) {
            player.dropMessage(1, "That place can't be reached.");
            return;
        }

        player.saveLocationOnWarp();
        player.changeMap(target, spawn);
    }
}
