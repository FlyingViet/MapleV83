package client.command.commands.gm0;

import client.Client;
import client.command.Command;

/**
 * MapleV83: takes back the SP spent on job skills and returns it to place again.
 * The iPhone/Mac clients send it from the RESET button in the skill window.
 */
public class ResetSpCommand extends Command {
    {
        setDescription("Return your skill points so you can place them again.");
    }

    @Override
    public void execute(Client c, String[] params) {
        int refunded = c.getPlayer().refundSkillPoints();
        c.getPlayer().dropMessage(5, refunded > 0
                ? "Your skill points were reset: " + refunded + " SP to place."
                : "There are no job skill points to reset.");
    }
}
