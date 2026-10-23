package client.command.commands.gm0;

import client.Character;
import client.Client;
import client.command.Command;
import server.Shop;
import server.ShopFactory;

/**
 * MapleV83: the iPhone's store, open anywhere - Power Elixirs, pet food, throwing
 * stars and recharging. It is an ordinary shop (shops.shopid 9990083) for the
 * non-existent NPC 0, which the client shows as its own panel.
 */
public class StoreCommand extends Command {
    public static final int SHOP_ID = 9990083;

    {
        setDescription("Open the store: elixirs, pet food, throwing stars and recharging.");
    }

    @Override
    public void execute(Client c, String[] params) {
        Character player = c.getPlayer();

        if (c.getCM() != null || player.getTrade() != null || player.getMiniGame() != null
                || player.getPlayerShop() != null || player.getHiredMerchant() != null) {
            player.yellowMessage("Finish what you are doing before opening the store.");
            return;
        }

        Shop shop = ShopFactory.getInstance().getShop(SHOP_ID);
        if (shop == null) {
            player.yellowMessage("The store is not set up on this server.");
            return;
        }

        shop.sendShop(c);
    }
}
