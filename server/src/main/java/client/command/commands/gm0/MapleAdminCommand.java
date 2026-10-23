package client.command.commands.gm0;

import client.Character;
import client.Client;
import client.command.Command;
import scripting.npc.NPCScriptManager;

/**
 * MapleV83: opens the Maple Administrator (job advancement, starter pack, AP reset)
 * from anywhere. Used by the mobile Menu's "Maple Admin" tile.
 */
public class MapleAdminCommand extends Command {
    private static final int MAPLE_ADMINISTRATOR = 9010000;

    {
        setDescription("Talk to the Maple Administrator: job advancement, starter pack, AP reset.");
    }

    @Override
    public void execute(Client c, String[] params) {
        Character player = c.getPlayer();

        if (c.getCM() != null || player.getTrade() != null || player.getMiniGame() != null
                || player.getPlayerShop() != null || player.getHiredMerchant() != null || player.getShop() != null) {
            player.yellowMessage("Finish what you are doing before talking to the Maple Administrator.");
            return;
        }

        NPCScriptManager.getInstance().start(c, MAPLE_ADMINISTRATOR, player);
    }
}
