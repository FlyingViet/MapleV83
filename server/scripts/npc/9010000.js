/*
	NPC Name:     Maple Administrator (9010000)
	Map(s):       Mushroom Town (10000, plife), and the v83 towns that already have him: Henesys, Lith Harbor,
	              Nautilus, Ereve, Rien, Orbis, El Nath, Ludibrium, Leafre, Mu Lung, Ariant...
	Description:  MapleV83 helper. Job advancement without the instructor quests (normal level
	              requirements; GMs skip them), a one-time starter pack, a trip to Henesys, an
	              ability point reset, and rebirth at level 300 (back to a level 1 Beginner with
	              every stat and skill kept; a reborn Beginner picks a new 1st job at any level).
*/

var HENESYS = 100000000;
var CLAIM_AREA = 30083;   // area_info key marking the claimed starter pack (per character)
var STARTER_MESOS = 100000;
var STARTER_ITEMS = [
    [2000000, 100],   // Red Potion
    [2000001, 50],    // Orange Potion
    [2000003, 100],   // Blue Potion
    [2000002, 30],    // White Potion
    [2030000, 10]     // Return Scroll - Nearest Town
];

// Weapons the explorer instructors hand out at 1st job.
var FIRST_JOB_ITEMS = {
    100: [[1302077, 1]],
    200: [[1372043, 1]],
    300: [[1452051, 1], [2060000, 1000]],
    400: [[1472061, 1], [1332063, 1], [2070015, 500]],
    500: [[1492000, 1], [1482000, 1], [2330000, 1000]]
};

var status = 0;
var choice = -1;
var options = [];   // job advancement candidates [jobId, requiredLevel]
var target = -1;

var MENU_ADVANCE = 4, MENU_PACK = 0, MENU_HENESYS = 1, MENU_AP = 2, MENU_LEAVE = 3, MENU_REBIRTH = 5;
var MAX_LEVEL = 300;

function claimed() {
    return cm.getPlayer().getAreaInfo(CLAIM_AREA) != null;
}

function packText() {
    var text = "";
    for (var i = 0; i < STARTER_ITEMS.length; i++) {
        text += "\r\n- #t" + STARTER_ITEMS[i][0] + "# x" + STARTER_ITEMS[i][1];
    }
    return text + "\r\n#b" + STARTER_MESOS + " mesos#k";
}

// Every job the character can move to next, with its usual level requirement.
function nextJobs(job) {
    if (job == 0) {
        return [[100, 10], [200, 8], [300, 10], [400, 10], [500, 10]];
    }
    if (job == 1000) {   // Noblesse -> Cygnus Knights
        return [[1100, 10], [1200, 10], [1300, 10], [1400, 10], [1500, 10]];
    }
    if (job == 2000) {   // Aran
        return [[2100, 10]];
    }
    if (job >= 100 && job < 600 && job % 100 == 0) {   // explorer 1st -> 2nd
        var branches = { 100: [110, 120, 130], 200: [210, 220, 230], 300: [310, 320], 400: [410, 420], 500: [510, 520] };
        var list = branches[job] || [];
        var out = [];
        for (var i = 0; i < list.length; i++) {
            out.push([list[i], 30]);
        }
        return out;
    }
    if (job >= 1100 && job < 1600 && job % 100 == 0) {   // Cygnus 1st -> 2nd
        return [[job + 10, 30]];
    }
    if (job >= 1100 && job < 1600 && job % 100 == 10) {  // Cygnus 2nd -> 3rd (v83's last)
        return [[job + 1, 70]];
    }
    if (job == 2100) {
        return [[2110, 30]];
    }
    if (job == 2110) {
        return [[2111, 70]];
    }
    if (job == 2111) {
        return [[2112, 120]];
    }
    if (job >= 100 && job < 600 && job % 10 == 0) {      // explorer 2nd -> 3rd
        return [[job + 1, 70]];
    }
    if (job >= 100 && job < 600 && job % 10 == 1) {      // explorer 3rd -> 4th
        return [[job + 1, 120]];
    }
    return [];
}

// A reborn Beginner chooses a new 1st job straight away; later jobs keep their levels.
function canAdvance(entry) {
    if (cm.getPlayer().getReborns() > 0 && cm.getJobId() == 0) {
        return true;
    }
    return cm.getPlayer().isGM() || cm.getLevel() >= entry[1];
}

function ordinal(n) {
    var mod100 = n % 100;
    if (mod100 >= 11 && mod100 <= 13) {
        return n + "th";
    }
    return n + ["th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"][n % 10];
}

// After a rebirth: the 1st jobs to choose from, as a list.
function chooseNewJob(intro) {
    options = nextJobs(0);
    choice = MENU_ADVANCE;
    status = 1;

    var list = intro + "Which job will you take this time?#b";
    for (var i = 0; i < options.length; i++) {
        list += "\r\n#L" + i + "#" + cm.getJobName(options[i][0]) + "#l";
    }
    cm.sendSimple(list);
}

function giveFirstJobItems(job) {
    var items = FIRST_JOB_ITEMS[job];
    if (!items) {
        return "";
    }
    for (var i = 0; i < items.length; i++) {
        if (!cm.canHold(items[i][0], items[i][1])) {
            return "\r\n\r\n(Your inventory was too full for the starter weapon.)";
        }
    }
    var text = "\r\n\r\nHere's something to start with:";
    for (var i = 0; i < items.length; i++) {
        cm.gainItem(items[i][0], items[i][1]);
        text += "\r\n#i" + items[i][0] + "# #t" + items[i][0] + "#" + (items[i][1] > 1 ? " x" + items[i][1] : "");
    }
    return text;
}

function advance() {
    var before = cm.getJobId();
    cm.changeJobById(target);
    var name = cm.getJobName(target);
    var text = "You are now " + article(name) + " #b" + name + "#k! Your new skills are already maxed. Open #bSkills#k to set them up.";
    if (before == 0) {
        text += giveFirstJobItems(target);
    }
    cm.sendOk(text);
    cm.dispose();
}

function article(name) {
    return "AEIOUaeiou".indexOf(name.charAt(0)) >= 0 ? "an" : "a";
}

function confirmAdvance() {
    var name = cm.getJobName(target);
    cm.sendYesNo("Become " + article(name) + " #b" + name + "#k?");
}

function start() {
    options = nextJobs(cm.getJobId());

    var menu = "Hi #b#h ##k! What can I do for you?#b";
    if (cm.getLevel() >= MAX_LEVEL) {
        menu += "\r\n#L" + MENU_REBIRTH + "#Be reborn (back to level 1, keep everything)#l";
    }
    if (options.length > 0) {
        menu += "\r\n#L" + MENU_ADVANCE + "#" + (cm.getPlayer().getReborns() > 0 && cm.getJobId() == 0 ? "Choose my new job" : "Advance my job") + "#l";
    }
    if (!claimed()) {
        menu += "\r\n#L" + MENU_PACK + "#Give me the starter pack and take me to Henesys#l";
    } else if (cm.getMapId() != HENESYS) {
        menu += "\r\n#L" + MENU_HENESYS + "#Take me to Henesys#l";
    }
    menu += "\r\n#L" + MENU_AP + "#Reset my ability points (AP)#l";
    menu += "\r\n#L" + MENU_LEAVE + "#Never mind#l";
    cm.sendSimple(menu);
}

function action(mode, type, selection) {
    if (mode != 1) {
        cm.dispose();
        return;
    }

    status++;

    if (choice == MENU_REBIRTH) {
        if (status == 2) {
            var reborns = cm.getPlayer().rebirth();
            if (reborns < 0) {
                cm.sendOk("You need to be #blevel " + MAX_LEVEL + "#k to be reborn.");
                cm.dispose();
                return;
            }
            chooseNewJob("You've been reborn for the #b" + ordinal(reborns) + "#k time! You're a level 1 #bBeginner#k again, with all your stats and skills.\r\n\r\n");
        } else {
            cm.dispose();
        }
        return;
    }

    if (choice == MENU_ADVANCE) {
        if (status == 2) {
            if (options.length > 1) {
                if (selection < 0 || selection >= options.length || !canAdvance(options[selection])) {
                    cm.dispose();
                    return;
                }
                target = options[selection][0];
                confirmAdvance();
            } else {
                advance();
            }
        } else if (status == 3) {
            advance();
        } else {
            cm.dispose();
        }
        return;
    }

    if (status == 1) {
        choice = selection;

        if (choice == MENU_ADVANCE) {
            var ready = [];
            for (var i = 0; i < options.length; i++) {
                if (canAdvance(options[i])) {
                    ready.push(options[i]);
                }
            }

            if (ready.length == 0) {
                cm.sendOk("You need to reach #blevel " + options[0][1] + "#k for your next job advancement. Come back then!");
                cm.dispose();
            } else if (options.length == 1) {
                target = options[0][0];
                confirmAdvance();
            } else {
                var list = "Which path will you take?#b";
                for (var i = 0; i < options.length; i++) {
                    if (canAdvance(options[i])) {
                        list += "\r\n#L" + i + "#" + cm.getJobName(options[i][0]) + "#l";
                    } else {
                        list += "\r\n#k" + cm.getJobName(options[i][0]) + " (level " + options[i][1] + ")#b";
                    }
                }
                cm.sendSimple(list);
            }
        } else if (choice == MENU_PACK) {
            cm.sendYesNo("Here's your starter pack:" + packText() + "\r\n\r\nThen I'll send you straight to #bHenesys#k. Ready?");
        } else if (choice == MENU_HENESYS) {
            cm.sendYesNo("Off to #bHenesys#k?");
        } else if (choice == MENU_REBIRTH) {
            var next = cm.getPlayer().getReborns() + 1;
            cm.sendYesNo("Ready for your #b" + ordinal(next) + " rebirth#k?\r\n\r\nYou'll go back to #blevel 1#k as a #bBeginner#k with no EXP, then choose a new job right away. You keep your #bstats, HP, MP, skills, items and mesos#k.");
        } else if (choice == MENU_AP) {
            cm.sendYesNo("I'll take back every STR, DEX, INT and LUK point above the minimum for your job (4 each for a beginner), and give them to you as #bAP#k to place where you like. Go ahead?");
        } else {
            cm.dispose();
        }
    } else if (status == 2) {
        if (choice == MENU_PACK) {
            if (claimed()) {
                cm.dispose();
                return;
            }

            for (var i = 0; i < STARTER_ITEMS.length; i++) {
                if (!cm.canHold(STARTER_ITEMS[i][0], STARTER_ITEMS[i][1])) {
                    cm.sendOk("Please make some room in your #bUse#k inventory first.");
                    cm.dispose();
                    return;
                }
            }

            for (var i = 0; i < STARTER_ITEMS.length; i++) {
                cm.gainItem(STARTER_ITEMS[i][0], STARTER_ITEMS[i][1]);
            }
            cm.gainMeso(STARTER_MESOS);
            cm.getPlayer().updateAreaInfo(CLAIM_AREA, "claimed=1");
            cm.warp(HENESYS, 0);
            cm.dispose();
        } else if (choice == MENU_HENESYS) {
            cm.warp(HENESYS, 0);
            cm.dispose();
        } else if (choice == MENU_AP) {
            var refunded = cm.getPlayer().refundAbilityPoints();
            if (refunded > 0) {
                cm.sendOk("Done! You got #b" + refunded + " AP#k back. Open #bStats#k to place them.");
            } else {
                cm.sendOk("Your stats are already at the minimum, so there's nothing to give back.");
            }
            cm.dispose();
        } else {
            cm.dispose();
        }
    } else {
        cm.dispose();
    }
}
