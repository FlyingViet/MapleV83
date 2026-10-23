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

public class MaxStatCommand extends Command {
    {
        setDescription("Max out all character stats.");
    }

    @Override
    public void execute(Client c, String[] params) {
        // MapleV83: the server's real limits - level 300, and STR/DEX/INT/LUK and
        // max HP/MP at the integer limit (fame is a 16-bit v83 field: 30000).
        Character player = c.getPlayer();
        player.setExp(0);
        player.setLevel(Character.MAX_LEVEL);
        player.resetPlayerRates();
        if (YamlConfig.config.server.USE_ADD_RATES_BY_LEVEL) {
            player.setPlayerRates();
        }
        player.setWorldRates();
        player.updateStrDexIntLuk(YamlConfig.config.server.MAX_AP);
        player.setFame(30000);
        player.updateMaxHpMaxMp(Integer.MAX_VALUE, Integer.MAX_VALUE);
        player.healHpMp();
        player.updateSingleStat(Stat.EXP, 0);
        player.updateSingleStat(Stat.LEVEL, Character.MAX_LEVEL);
        player.updateSingleStat(Stat.FAME, 30000);
        player.yellowMessage("Stats maxed out: level " + Character.MAX_LEVEL + ", every stat and max HP/MP at 2,147,483,647.");
    }
}
