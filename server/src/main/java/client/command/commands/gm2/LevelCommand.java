/*
    This file is part of the HeavenMS MapleStory Server, commands OdinMS-based
    Copyleft (L) 2016 - 2019 RonanLana

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation version 3 as published by
    the Free Software Foundation. You may not use, modify or distribute
    this program under any other version of the GNU Affero General Public
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
   @Author: Arthur L - Refactored command content into modules
*/
package client.command.commands.gm2;

import client.Character;
import client.Client;
import client.Stat;
import client.command.Command;
import config.YamlConfig;

public class LevelCommand extends Command {
    {
        setDescription("Set your level.");
    }

    @Override
    public void execute(Client c, String[] params) {
        Character player = c.getPlayer();
        if (params.length < 1) {
            player.yellowMessage("Syntax: !level <newlevel>");
            return;
        }

        int target;
        try {
            target = Math.max(1, Math.min(Integer.parseInt(params[0]), player.getMaxClassLevel()));
        } catch (NumberFormatException nfe) {
            player.yellowMessage("Syntax: !level <newlevel> (1-" + player.getMaxClassLevel() + ")");
            return;
        }

        player.setExp(0);
        player.updateSingleStat(Stat.EXP, 0);

        if (target == 1) {
            player.setLevel(1);
            player.updateSingleStat(Stat.LEVEL, 1);
        } else {
            player.setLevel(target - 1);
        }

        player.resetPlayerRates();
        if (YamlConfig.config.server.USE_ADD_RATES_BY_LEVEL) {
            player.setPlayerRates();
        }
        player.setWorldRates();

        if (target > 1) {
            player.levelUp(false);
        }
    }
}
