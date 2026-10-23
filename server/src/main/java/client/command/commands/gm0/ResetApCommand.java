package client.command.commands.gm0;

import client.Client;
import client.command.Command;

/**
 * MapleV83: puts STR/DEX/INT/LUK back to the job minimum and returns the rest as AP.
 * The iPhone/Mac clients send it from the RESET button in the stat window.
 */
public class ResetApCommand extends Command {
    {
        setDescription("Return your ability points so you can place them again.");
    }

    @Override
    public void execute(Client c, String[] params) {
        int refunded = c.getPlayer().refundAbilityPoints();
        c.getPlayer().dropMessage(5, refunded > 0
                ? "Your ability points were reset: " + refunded + " AP to place."
                : "Your stats are already at the minimum for your job.");
    }
}
