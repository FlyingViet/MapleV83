//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "UIStatusBar.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../Configuration.h"
#include "../../Graphics/Geometry.h"
#include "../NotificationCenter.h"
#include "UIBuddyList.h"
#include "UIChannel.h"
#include "UIEquipInventory.h"
#include "UIEvent.h"
#include "UIMessenger.h"
#include "UIMonsterBattle.h"
#include "UIMonsterLife.h"
#include "UIItemInventory.h"
#include "UIJoypad.h"
#include "UIKeyConfig.h"
#include "UIQuestLog.h"
#include "UINotice.h"
#include "UINotificationList.h"
#include "UIReport.h"

#include "../../Net/Packets/SocialPackets.h"
#include "../../Net/Packets/PlayerPackets.h"
#include "../../Data/SkillData.h"
#include "../../Data/ItemData.h"
#include "../Keyboard.h"
#include "UIOptionMenu.h"
#include "UIQuit.h"
#include "UISkillBook.h"
#include "UIStatsInfo.h"
#include "UIRanking.h"
#include "UIMonsterBook.h"
#include "UIWhisper.h"

#include "../../Audio/Audio.h"
#include "../../Net/OutPacket.h"
#include "../../Net/Session.h"
#include "../../IO/Window.h"

#include "../UI.h"

#include "../Components/MapleButton.h"

#include "../../Character/ExpTable.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"


#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	// ======================================================================
	// v83 layout. Every constant below is measured in pixels relative to
	// the top-left of StatusBar.img/base/backgrnd (800x71); `position` is
	// that top-left on screen (see UIStatusBar::bar_origin).
	//
	// base/backgrnd2 (570x71) paints, on top of the plain strip:
	//   - the white chat input band, rows 7..28, x 0..565
	//   - the dark "LV." panel, rows 36..69, x 0..207 (LV box x 33..75)
	//   - the dark gauge panel, rows 36..69, x 209..562
	// The right-hand 235 px (x 565..800) are the button column.
	// ======================================================================

	// backgrnd2 sits 1 px in from the bar's left edge so its rounded corner
	// does not get clipped at the screen border on an 800-wide view.
	static constexpr int16_t BG2_X = 1;

	// gauge/bar + gauge/graduation (340x31): centred inside the 353 px wide
	// gauge panel (x 209..562 -> 217), rows 38..69 so the label row
	// (bitmap rows 0..12) sits on the panel's top edge.
	static constexpr int16_t GAUGE_X = 217;
	static constexpr int16_t GAUGE_Y = 38;
	// Coloured fill rectangles inside gauge/bar (bitmap-local): the HP bar
	// is red on x 2..106, MP blue on 110..214, EXP yellow on 223..337, all
	// on rows 15..28. gauge/gray (1x16) is stretched over the unfilled
	// remainder, starting one row above the fill so it also hides the
	// bar's own inner highlight; graduation restores the frame on top.
	static constexpr int16_t GAUGE_FILL_Y = 14;
	static constexpr int16_t GAUGE_FILL_H = 16;
	static constexpr int16_t HP_FILL_X = 2;
	static constexpr int16_t HP_FILL_W = 105;
	static constexpr int16_t MP_FILL_X = 110;
	static constexpr int16_t MP_FILL_W = 105;
	static constexpr int16_t EXP_FILL_X = 223;
	static constexpr int16_t EXP_FILL_W = 115;
	// hpFlash / mpFlash (109x18) frame the HP (bitmap x 0..108) and MP
	// (x 108..216) bars, starting on bitmap row 13.
	static constexpr int16_t HP_FLASH_X = 0;
	static constexpr int16_t MP_FLASH_X = 108;
	static constexpr int16_t FLASH_Y = 13;
	// Numbers use the 5x7 StatusBar.img/number font and sit on the label
	// row, right of the baked "HP" (ends x 17), "MP" (x 109..128) and
	// "EXP" (x 221..245) captions; row 3 centres them on the 13 px captions.
	static constexpr int16_t HP_NUM_X = 20;
	static constexpr int16_t MP_NUM_X = 131;
	static constexpr int16_t EXP_NUM_X = 248;
	static constexpr int16_t NUM_Y = 3;

	// Basic.img/LevelNo digits (11x13) centred on the LV box (x 33..75,
	// rows 38..69).
	static constexpr int16_t LEVEL_CX = 54;
	static constexpr int16_t LEVEL_Y = 47;
	static constexpr int16_t LEVEL_DIGIT_W = 11;
	// Character name over job name inside the LV panel, right of the LV box.
	static constexpr int16_t NAME_X = 80;
	static constexpr int16_t NAME_Y = 37;
	static constexpr int16_t JOB_Y = 51;

	// Chat input band: BtWhisper (12x19) at the very left of the band.
	static constexpr int16_t WHISPER_X = 3;
	static constexpr int16_t BAND_Y = 8;

	// Right column, top row (inside the input band, rows 8..27):
	// base/box (42x19) right after the band ends at x 565, then the six
	// 28x20 hot keys with a 2 px gap, ending flush with the big buttons.
	static constexpr int16_t BOX_X = 572;
	static constexpr int16_t BOX_Y = 8;
	// BtClaim (20x19) fills the box's left slot (x 1..18 inside the box);
	// iconMemo (14x11) is centred in the right slot (x 23..40).
	static constexpr int16_t CLAIM_X = 572;
	static constexpr int16_t MEMO_X = 597;
	static constexpr int16_t MEMO_Y = 12;
	static constexpr int16_t MEMO_SLOT_X = 595;
	static constexpr int16_t MEMO_SLOT_W = 18;
	static constexpr int16_t HOTKEY_X0 = 616;
	static constexpr int16_t HOTKEY_Y = 8;
	static constexpr int16_t HOTKEY_STEP = 30;
	// Right column, bottom row: four 54x34 buttons (Shop, MTS, Menu,
	// Shortcut) with a 2 px gap, rows 36..69, ending at x 794.
	static constexpr int16_t BIG_X0 = 572;
	static constexpr int16_t BIG_Y = 36;
	static constexpr int16_t BIG_STEP = 56;
	static constexpr int16_t BIG_W = 54;
	static constexpr int16_t MENU_BTN_X = BIG_X0 + BIG_STEP * 2;

	// Menu popup: text rows above BtMenu, right edge flush with the button,
	// 2 px above the bar.
	static constexpr int16_t MENU_W = 132;
	static constexpr int16_t MENU_ROW_H = 16;
	static constexpr int16_t MENU_PAD = 4;
	static constexpr int16_t MENU_SEP_H = 5;
	static constexpr int16_t MENU_GAP = 2;
	// Index of the first "system" row (Channel / Keys / Options / ...), which
	// gets a separator line above it.
	static constexpr int16_t MENU_SYS_FIRST = 12;

	// === Quickslot layout constants (used by draw() and the slot helpers) ===
	// base/quickSlot (151x80) pops up above the bar, 1 px in from the bar's
	// right edge, so it hangs over the QuickSlot toggle button.
	static constexpr int16_t QS_PANEL_X = UIStatusBar::BAR_WIDTH - 151 - 1;
	static constexpr int16_t QS_PANEL_Y = -80 - 1;
	// Cube grid measured from the bitmap: white cubes at x=9,44,79,114
	// (28 wide, step 35) and y=9,43 (25 tall, step 34).
	static constexpr int16_t QS_CELL_OFFSET_X = 9;
	static constexpr int16_t QS_CELL_OFFSET_Y = 9;
	static constexpr int16_t QS_CELL_W   = 28;
	static constexpr int16_t QS_CELL_H   = 25;
	static constexpr int16_t QS_COL_STEP = 35;
	static constexpr int16_t QS_ROW_STEP = 34;

	// Short display name for a maple/DIK quickslot keycode (drawn on each cube).
	static std::string qs_keyname(uint8_t code)
	{
		switch (code)
		{
		// letters
		case 16: return "Q"; case 17: return "W"; case 18: return "E"; case 19: return "R";
		case 20: return "T"; case 21: return "Y"; case 22: return "U"; case 23: return "I";
		case 24: return "O"; case 25: return "P";
		case 30: return "A"; case 31: return "S"; case 32: return "D"; case 33: return "F";
		case 34: return "G"; case 35: return "H"; case 36: return "J"; case 37: return "K";
		case 38: return "L";
		case 44: return "Z"; case 45: return "X"; case 46: return "C"; case 47: return "V";
		case 48: return "B"; case 49: return "N"; case 50: return "M";
		// number row
		case 2: return "1"; case 3: return "2"; case 4: return "3"; case 5: return "4";
		case 6: return "5"; case 7: return "6"; case 8: return "7"; case 9: return "8";
		case 10: return "9"; case 11: return "0";
		// function keys
		case 59: return "F1"; case 60: return "F2"; case 61: return "F3"; case 62: return "F4";
		case 63: return "F5"; case 64: return "F6"; case 65: return "F7"; case 66: return "F8";
		case 67: return "F9"; case 68: return "F10"; case 87: return "F11"; case 88: return "F12";
		// modifiers / navigation (the default quickslot set)
		case 42: case 54: return "Sh";
		case 29: case 157: return "Ct";
		case 56: return "Al";
		case 82: return "Ins"; case 71: return "Hm"; case 73: return "PU";
		case 83: return "Del"; case 79: return "End"; case 81: return "PD";
		case 57: return "Sp"; case 15: return "Tab"; case 14: return "BS";
		default: return std::to_string(static_cast<int>(code));
		}
	}

	Point<int16_t> UIStatusBar::bar_origin()
	{
		int16_t vwidth = Constants::Constants::get().get_viewwidth();
		int16_t vheight = Constants::Constants::get().get_viewheight();

		int16_t left = vwidth > BAR_WIDTH
			? static_cast<int16_t>((vwidth - BAR_WIDTH) / 2)
			: 0;

		return Point<int16_t>(left, static_cast<int16_t>(vheight - BAR_HEIGHT));
	}

	UIStatusBar::UIStatusBar(const CharStats& st) : stats(st)
	{
		position = bar_origin();
		dimension = Point<int16_t>(BAR_WIDTH, BAR_HEIGHT);

		show_menu = false;
		show_quickslot = false;

		nl::node statusbar = nl::nx::ui["StatusBar.img"];
		nl::node base = statusbar["base"];
		nl::node gauge = statusbar["gauge"];

		has_notification = false;
		notice_pulse_tick = 0;

		// Flash-on-decrease trackers start aligned with the current values so
		// we don't spuriously flash on the first frame.
		prev_hp_pct = gethppercent();
		prev_mp_pct = getmppercent();
		prev_exp_pct = getexppercent();
		hp_shown = prev_hp_pct;
		mp_shown = prev_mp_pct;
		exp_shown = prev_exp_pct;
		hp_flash_ticks = 0;
		mp_flash_ticks = 0;
		exp_flash_ticks = 0;

		// === Background ===
		bar_backgrnd = Texture(base["backgrnd"]);
		bar_backgrnd2 = Texture(base["backgrnd2"]);
		hotkey_box = Texture(base["box"]);
		icon_memo = Texture(base["iconMemo"]);

		// === Gauges ===
		gauge_bar = Texture(gauge["bar"]);
		gauge_graduation = Texture(gauge["graduation"]);
		gauge_gray = Texture(gauge["gray"]);
		hp_flash = Animation(gauge["hpFlash"]);
		mp_flash = Animation(gauge["mpFlash"]);

		nl::node energy_node = nl::nx::ui["UIWindow.img"]["EnergyBar"];

		energy_bar_c = energy_node["c"];
		energy_bar_e = energy_node["e"];
		energy_fill = energy_node["Gage"]["1"]["0"];

		if (energy_node["effect"])
			energy_full_effect = Animation(energy_node["effect"]);

		// === Charsets ===
		// StatusBar.img/number names its glyphs 0..9, Lbracket, Rbracket,
		// percent and slash; Charset keys each by the first character of its
		// name, so the extras are reachable as 'L', 'R', 'p' and 's'.
		nl::node numbers = statusbar["number"];
		statset = Charset(numbers, Charset::Alignment::LEFT);
		num_lbracket = Texture(numbers["Lbracket"]);
		num_rbracket = Texture(numbers["Rbracket"]);
		levelset = Charset(nl::nx::ui["Basic.img"]["LevelNo"], Charset::Alignment::LEFT);

		// === Labels ===
		namelabel = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::WHITE);
		joblabel = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		menu_label = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);
		// Bold white key label drawn in each quickslot cube's corner.
		qs_key_label = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::WHITE);

		nl::node keynames = statusbar["key"];
		for (int i = 0; i < 8; i++)
			qs_key_sprites[i] = keynames[std::to_string(i)];

		// === Main bar buttons ===
		buttons[BT_WHISPER]    = std::make_unique<MapleButton>(statusbar["BtWhisper"], Point<int16_t>(WHISPER_X, BAND_Y));
		buttons[BT_CALLGM]     = std::make_unique<MapleButton>(statusbar["BtClaim"],   Point<int16_t>(CLAIM_X, BOX_Y));
		buttons[BT_CASHSHOP]   = std::make_unique<MapleButton>(statusbar["BtShop"],    Point<int16_t>(BIG_X0,                BIG_Y));
		buttons[BT_TRADE]      = std::make_unique<MapleButton>(statusbar["BtNPT"],     Point<int16_t>(BIG_X0 + BIG_STEP,     BIG_Y));
		buttons[BT_MENU]       = std::make_unique<MapleButton>(statusbar["BtMenu"],    Point<int16_t>(MENU_BTN_X,            BIG_Y));
		buttons[BT_KEYSETTING] = std::make_unique<MapleButton>(statusbar["BtShort"],   Point<int16_t>(BIG_X0 + BIG_STEP * 3, BIG_Y));

		// === Hot-key row ===
		buttons[BT_EQUIPS]    = std::make_unique<MapleButton>(statusbar["EquipKey"], Point<int16_t>(HOTKEY_X0,                   HOTKEY_Y));
		buttons[BT_INVENTORY] = std::make_unique<MapleButton>(statusbar["InvenKey"], Point<int16_t>(HOTKEY_X0 + HOTKEY_STEP,     HOTKEY_Y));
		buttons[BT_STATS]     = std::make_unique<MapleButton>(statusbar["StatKey"],  Point<int16_t>(HOTKEY_X0 + HOTKEY_STEP * 2, HOTKEY_Y));
		buttons[BT_SKILL]     = std::make_unique<MapleButton>(statusbar["SkillKey"], Point<int16_t>(HOTKEY_X0 + HOTKEY_STEP * 3, HOTKEY_Y));
		buttons[BT_KEYSET]    = std::make_unique<MapleButton>(statusbar["KeySet"],   Point<int16_t>(HOTKEY_X0 + HOTKEY_STEP * 4, HOTKEY_Y));

		// Unspent AP / SP reminder: the hot keys ship a 2-frame `ani` blink.
		stat_blink = Animation(statusbar["StatKey"]["ani"]);
		skill_blink = Animation(statusbar["SkillKey"]["ani"]);

		// === Quick slot ===
		quickslot_bg = Texture(base["quickSlot"]);

		// QuickSlot (arrow up) shows the panel, QuickSlotD (arrow down)
		// hides it again; both occupy the last hot-key slot.
		Point<int16_t> qs_btn_pos(HOTKEY_X0 + HOTKEY_STEP * 5, HOTKEY_Y);
		buttons[BT_QS_OPEN]  = std::make_unique<MapleButton>(statusbar["QuickSlot"], qs_btn_pos);
		buttons[BT_QS_CLOSE] = std::make_unique<MapleButton>(statusbar["QuickSlotD"], qs_btn_pos);
		buttons[BT_QS_OPEN]->set_active(true);
		buttons[BT_QS_CLOSE]->set_active(false);

		// === Menu popup rows ===
		// v83's StatusBar.img has no Menu/System sub-panel art, so the popup
		// is a list of text rows. Order: the character windows first, then
		// (after a separator, see MENU_SYS_FIRST) the system entries.
		menu_rows = {
			{ BT_MENU_STAT,          "Stats" },
			{ BT_MENU_SKILL,         "Skills" },
			{ BT_MENU_QUEST,         "Quests" },
			{ BT_MENU_EQUIP,         "Equipment" },
			{ BT_MENU_ITEM,          "Inventory" },
			{ BT_MENU_COMMUNITY,     "Buddy List" },
			{ BT_MENU_MSN,           "Messenger" },
			{ BT_MENU_EVENT,         "Events" },
			{ BT_MENU_RANK,          "Ranking" },
			{ BT_MENU_EPISODBOOK,    "Monster Book" },
			{ BT_MENU_MONSTERBATTLE, "Monster Battle" },
			{ BT_MENU_MONSTERLIFE,   "Monster Life" },
			{ BT_SYS_CHANNEL,        "Change Channel" },
			{ BT_SYS_KEYSETTING,     "Key Settings" },
			{ BT_SYS_OPTION,         "Options" },
			{ BT_SYS_JOYPAD,         "Joypad" },
			{ BT_SYS_GAMEQUIT,       "Quit Game" }
		};
	}

	void UIStatusBar::update_boss_hp(const std::string& name, int8_t percent)
	{
		// 0% (or below) means the boss died / the tag cleared - hide the gauge.
		if (percent <= 0)
		{
			boss_hp_ticks = 0;
			boss_gage.clear();
			return;
		}

		boss_gage.set_mob(0, name, 0);
		boss_hp_percent = std::clamp(static_cast<float>(percent) / 100.0f, 0.0f, 1.0f);
		boss_hp_ticks = 250; // ~4s of fixed-step updates; refreshed on every report
	}

	float UIStatusBar::ease_towards(float current, float target)
	{
		float diff = target - current;

		if (std::fabs(diff) < 0.0005f)
			return target;

		// Exponential ease-out: close ~12% of the remaining gap each tick,
		// floored so small gaps still close in finite time (same feel as
		// Gauge::update, which the v83 gray-cover technique can't use).
		float move = diff * 0.12f;
		const float min_step = 0.0015f;

		if (std::fabs(move) < min_step)
			move = (diff > 0.0f) ? min_step : -min_step;

		if ((diff > 0.0f && move > diff) || (diff < 0.0f && move < diff))
			move = diff;

		return current + move;
	}

	void UIStatusBar::draw_stat_text(const std::string& text, Point<int16_t> pos) const
	{
		// Digits/slash/percent are 7 px tall, the brackets 9 px; drop the
		// glyphs one row so they centre on the brackets.
		int16_t x = pos.x();

		for (char c : text)
		{
			if (c == 'L' || c == 'R')
			{
				const Texture& bracket = (c == 'L') ? num_lbracket : num_rbracket;
				bracket.draw(DrawArgument(Point<int16_t>(x, pos.y())));
				x += bracket.width() + 1;
			}
			else if (c == '.')
			{
				// The v83 number font has no dot glyph - a 1x1 white box on
				// the digit baseline reads the same at this size.
				ColorBox(1, 1, Color::Name::WHITE, 1.0f).draw(DrawArgument(Point<int16_t>(x, pos.y() + 7)));
				x += 2;
			}
			else
			{
				statset.draw(c, DrawArgument(Point<int16_t>(x, pos.y() + 1)));
				x += statset.getw(c) + 1;
			}
		}
	}

	void UIStatusBar::draw(float alpha) const
	{
		int16_t vwidth = Constants::Constants::get().get_viewwidth();

		// Boss HP gauge, centred near the top of the screen while a boss is
		// reporting its HP (auto-hides via boss_hp_ticks in update()).
		if (boss_hp_ticks > 0 && boss_gage.is_active())
		{
			Point<int16_t> boss_pos(
				static_cast<int16_t>((vwidth - boss_gage.width()) / 2), 18);
			boss_gage.draw(boss_pos, boss_hp_percent);
		}

		if (Stage::get().is_energy_active() && energy_bar_c.is_valid())
		{
			constexpr int16_t ENERGY_MAX = 10000;
			constexpr int16_t ENERGY_WIDTH = 100;

			int32_t amount = Stage::get().get_energy();
			int16_t filled = static_cast<int16_t>(
				static_cast<int64_t>(amount) * ENERGY_WIDTH / ENERGY_MAX);

			// Just above the bar, centred.
			Point<int16_t> ep((vwidth - ENERGY_WIDTH) / 2, position.y() - 24);

			for (int16_t x = 0; x < ENERGY_WIDTH; x++)
				energy_bar_c.draw(DrawArgument(ep + Point<int16_t>(x, 0)));

			energy_bar_e.draw(DrawArgument(ep + Point<int16_t>(-energy_bar_e.width(), 0)));
			energy_bar_e.draw(DrawArgument(ep + Point<int16_t>(ENERGY_WIDTH, 0)));

			if (energy_fill.is_valid())
				for (int16_t x = 0; x < filled; x++)
					energy_fill.draw(DrawArgument(ep + Point<int16_t>(x, 2)));

			if (amount >= ENERGY_MAX)
				energy_full_effect.draw(DrawArgument(ep + Point<int16_t>(ENERGY_WIDTH / 2, 0)), alpha);
		}

		// Quickslot panel - drawn FIRST so the status bar renders on top of
		// it; it sits above the bar, over the hot-key column.
		if (show_quickslot)
		{
			Point<int16_t> panel = quickslot_panel_pos();

			if (quickslot_bg.is_valid())
				quickslot_bg.draw(DrawArgument(panel));

			const auto& maplekeys = UI::get().get_keyboard().get_maplekeys();
			const auto& quickslot_keys = UI::get().get_keyboard().get_quickslot_keys();

			for (int16_t i = 0; i < static_cast<int16_t>(quickslot_keys.size()); ++i)
			{
				int32_t keycode = quickslot_keys[i];
				Point<int16_t> tl = quickslot_slot_pos(i);

				auto it = maplekeys.find(keycode);
				bool bound = it != maplekeys.end()
					&& it->second.type != KeyType::Id::NONE && it->second.action != 0;

				if (bound)
				{
					Texture icon = get_quickslot_icon(it->second.type, it->second.action);
					if (icon.is_valid())
					{
						// Fit the (32px) icon into the 28x25 cube and centre it - its
						// origin is already normalised to top-left by get_quickslot_icon.
						Point<int16_t> dims = icon.get_dimensions();
						float scale = 1.0f;
						int16_t maxdim = std::max(dims.x(), dims.y());
						if (maxdim > QS_CELL_H)
							scale = static_cast<float>(QS_CELL_H) / static_cast<float>(maxdim);

						int16_t dw = static_cast<int16_t>(dims.x() * scale);
						int16_t dh = static_cast<int16_t>(dims.y() * scale);
						Point<int16_t> pos(
							static_cast<int16_t>(tl.x() + (QS_CELL_W - dw) / 2),
							static_cast<int16_t>(tl.y() + (QS_CELL_H - dh) / 2));
						icon.draw(DrawArgument(pos, scale, scale, 1.0f));
					}
				}

				// Key label for EVERY cube (even empty ones), so the player always
				// knows which key fires each slot. Drawn from the real binding, so it
				// stays correct after rebinding. The v83 key sprites (28x11,
				// outlined white text) sit on the cube's top edge like the
				// original client; non-default keys get a boxed text label there.
				static const uint8_t defcodes[8] = { 42, 82, 71, 73, 29, 83, 79, 81 };
				bool default_key = i < 8 && (static_cast<uint8_t>(keycode) == defcodes[i]
					|| (i == 0 && keycode == 54) || (i == 4 && keycode == 157));

				if (default_key && qs_key_sprites[i].is_valid())
				{
					qs_key_sprites[i].draw(tl);
				}
				else
				{
					qs_key_label.change_text(qs_keyname(static_cast<uint8_t>(keycode)));

					ColorBox label_bg(qs_key_label.width() + 4, 12, Color::Name::BLACK, 0.65f);
					label_bg.draw(DrawArgument(tl));

					qs_key_label.draw(tl + Point<int16_t>(2, -2));
				}
			}
		}

		// Plain strip stretched across the whole view so the bar reaches both
		// screen edges regardless of the view width; the 800-wide content is
		// laid over it from `position`.
		if (bar_backgrnd.is_valid())
			bar_backgrnd.draw(DrawArgument(Point<int16_t>(0, position.y()), Point<int16_t>(vwidth, BAR_HEIGHT)));

		if (bar_backgrnd2.is_valid())
			bar_backgrnd2.draw(DrawArgument(position + Point<int16_t>(BG2_X, 0)));

		// === Gauges ===
		Point<int16_t> gauge_pos = position + Point<int16_t>(GAUGE_X, GAUGE_Y);

		if (gauge_bar.is_valid())
			gauge_bar.draw(DrawArgument(gauge_pos));

		// Cover the unfilled remainder of each bar with the gray strip.
		auto cover_missing = [&](int16_t fill_x, int16_t fill_w, float shown)
		{
			if (!gauge_gray.is_valid())
				return;

			float pct = std::clamp(shown, 0.0f, 1.0f);
			int16_t filled = static_cast<int16_t>(std::lround(pct * fill_w));
			int16_t missing = static_cast<int16_t>(fill_w - filled);

			if (missing > 0)
				gauge_gray.draw(DrawArgument(
					gauge_pos + Point<int16_t>(fill_x + filled, GAUGE_FILL_Y),
					Point<int16_t>(missing, GAUGE_FILL_H)));
		};

		cover_missing(HP_FILL_X, HP_FILL_W, hp_shown);
		cover_missing(MP_FILL_X, MP_FILL_W, mp_shown);
		cover_missing(EXP_FILL_X, EXP_FILL_W, exp_shown);

		if (gauge_graduation.is_valid())
			gauge_graduation.draw(DrawArgument(gauge_pos));

		// EXP flash-on-drop: no dedicated animation sprite, so pulse the EXP
		// fill's brightness for the duration of the flash.
		if (exp_flash_ticks > 0)
		{
			float pulse = static_cast<float>(exp_flash_ticks) / FLASH_DURATION_TICKS;
			int16_t filled = static_cast<int16_t>(std::lround(std::clamp(exp_shown, 0.0f, 1.0f) * EXP_FILL_W));

			if (filled > 0)
				ColorBox(filled, GAUGE_FILL_H - 2, Color::Name::WHITE, 0.35f * pulse)
					.draw(DrawArgument(gauge_pos + Point<int16_t>(EXP_FILL_X, GAUGE_FILL_Y + 1)));
		}

		// Gauge blink animations (StatusBar.img/gauge/hpFlash, mpFlash). The
		// trigger threshold comes from the HP/MP Warning sliders in
		// System -> Options (0..100 %). The flash_ticks branch also fires the
		// pulse briefly when the gauge just dropped, regardless of the threshold.
		float hp_pct = gethppercent();
		float mp_pct = getmppercent();

		float hp_warn = Setting<HPWarning>::get().load() / 100.0f;
		float mp_warn = Setting<MPWarning>::get().load() / 100.0f;

		if (hp_pct < hp_warn || hp_flash_ticks > 0)
			hp_flash.draw(DrawArgument(gauge_pos + Point<int16_t>(HP_FLASH_X, FLASH_Y)), alpha);

		if (mp_pct < mp_warn || mp_flash_ticks > 0)
			mp_flash.draw(DrawArgument(gauge_pos + Point<int16_t>(MP_FLASH_X, FLASH_Y)), alpha);

		// === Stat numbers ===
		int16_t level = stats.get_stat(MapleStat::Id::LEVEL);
		int32_t hp = stats.get_stat(MapleStat::Id::HP); // uint16_t -> int32; avoid int16 overflow > 32767
		int32_t mp = stats.get_stat(MapleStat::Id::MP);
		int32_t maxhp = stats.get_total(EquipStat::Id::HP);
		int32_t maxmp = stats.get_total(EquipStat::Id::MP);
		int64_t exp = stats.get_exp();

		std::string expstring = std::to_string(100 * getexppercent());
		expstring = expstring.substr(0, expstring.find('.') + 3);

		// "[hp/maxhp]", "[mp/maxmp]" and "exp[12.34%]" in the bitmap font;
		// see draw_stat_text for the L/R/s/p glyph aliases.
		draw_stat_text("L" + std::to_string(hp) + "s" + std::to_string(maxhp) + "R",
			gauge_pos + Point<int16_t>(HP_NUM_X, NUM_Y));
		draw_stat_text("L" + std::to_string(mp) + "s" + std::to_string(maxmp) + "R",
			gauge_pos + Point<int16_t>(MP_NUM_X, NUM_Y));
		draw_stat_text(std::to_string(exp) + "L" + expstring + "pR",
			gauge_pos + Point<int16_t>(EXP_NUM_X, NUM_Y));

		// Level, centred on the LV box (Charset LEFT adds a 1 px gap per digit).
		{
			std::string levelstr = std::to_string(level);
			int16_t ndigits = static_cast<int16_t>(levelstr.size());
			int16_t total_w = static_cast<int16_t>(ndigits * LEVEL_DIGIT_W + (ndigits - 1));
			levelset.draw(levelstr, position + Point<int16_t>(LEVEL_CX - total_w / 2, LEVEL_Y));
		}

		namelabel.draw(position + Point<int16_t>(NAME_X, NAME_Y));
		joblabel.draw(position + Point<int16_t>(NAME_X, JOB_Y));

		// Two-slot tray right of the input band: BtClaim (button) on the
		// left, the memo icon on the right doubling as the notification badge.
		if (hotkey_box.is_valid())
			hotkey_box.draw(DrawArgument(position + Point<int16_t>(BOX_X, BOX_Y)));

		// Draw main-bar buttons
		UIElement::draw_buttons(alpha);

		// Notification badge: dim when the drawer is empty (still clickable),
		// pulsing at full strength while something is pending. Set by
		// `notify()` whenever a quest completes, an invite arrives, an
		// item-effect msg fires, etc., and cleared once the player opens
		// the notification drawer or otherwise resolves it.
		if (icon_memo.is_valid())
		{
			float memo_alpha = 0.35f;

			if (has_notification)
				memo_alpha = 0.7f + 0.3f * static_cast<float>(std::sin(notice_pulse_tick * 0.15));

			icon_memo.draw(DrawArgument(position + Point<int16_t>(MEMO_X, MEMO_Y), memo_alpha));
		}

		// Unspent AP / SP: blink the Stat / Skill hot keys over their buttons.
		if (stats.get_stat(MapleStat::Id::AP) > 0)
			stat_blink.draw(DrawArgument(position + Point<int16_t>(HOTKEY_X0 + HOTKEY_STEP * 2, HOTKEY_Y)), alpha);

		if (stats.get_stat(MapleStat::Id::SP) > 0)
			skill_blink.draw(DrawArgument(position + Point<int16_t>(HOTKEY_X0 + HOTKEY_STEP * 3, HOTKEY_Y)), alpha);

		// Menu popup - drawn LAST so it sits above every other sprite/button
		// in the status bar.
		if (menu_fade > 0.0f)
		{
			Rectangle<int16_t> panel = menu_panel_rect();
			Point<int16_t> tl = panel.get_left_top();
			int16_t panel_h = menu_panel_height();

			ColorBox(MENU_W, panel_h, Color::Name::BLACK, 0.78f * menu_fade).draw(DrawArgument(tl));

			for (int16_t i = 0; i < static_cast<int16_t>(menu_rows.size()); i++)
			{
				Rectangle<int16_t> row = menu_row_rect(i);
				Point<int16_t> rtl = row.get_left_top();

				if (i == MENU_SYS_FIRST)
					ColorBox(MENU_W - MENU_PAD * 2, 1, Color::Name::WHITE, 0.35f * menu_fade)
						.draw(DrawArgument(rtl + Point<int16_t>(MENU_PAD, -(MENU_SEP_H / 2 + 1))));

				if (i == menu_hover)
					ColorBox(MENU_W - MENU_PAD * 2, MENU_ROW_H, Color::Name::WHITE, 0.25f * menu_fade)
						.draw(DrawArgument(rtl + Point<int16_t>(MENU_PAD, 0)));

				menu_label.change_text(menu_rows[i].label);
				menu_label.draw(DrawArgument(rtl + Point<int16_t>(MENU_PAD + 6, 0), menu_fade));
			}
		}
	}

	int16_t UIStatusBar::menu_panel_height() const
	{
		return static_cast<int16_t>(menu_rows.size() * MENU_ROW_H + MENU_PAD * 2 + MENU_SEP_H);
	}

	Rectangle<int16_t> UIStatusBar::menu_panel_rect() const
	{
		int16_t panel_h = menu_panel_height();
		Point<int16_t> tl = position + Point<int16_t>(MENU_BTN_X + BIG_W - MENU_W, -MENU_GAP - panel_h);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(MENU_W, panel_h));
	}

	Rectangle<int16_t> UIStatusBar::menu_row_rect(int16_t row) const
	{
		Point<int16_t> tl = menu_panel_rect().get_left_top();
		int16_t y = static_cast<int16_t>(MENU_PAD + row * MENU_ROW_H + (row >= MENU_SYS_FIRST ? MENU_SEP_H : 0));
		Point<int16_t> rtl = tl + Point<int16_t>(0, y);

		return Rectangle<int16_t>(rtl, rtl + Point<int16_t>(MENU_W, MENU_ROW_H));
	}

	int16_t UIStatusBar::menu_row_at(Point<int16_t> cursorpos) const
	{
		if (!show_menu || menu_fade < 1.0f)
			return -1;

		for (int16_t i = 0; i < static_cast<int16_t>(menu_rows.size()); i++)
			if (menu_row_rect(i).contains(cursorpos))
				return i;

		return -1;
	}

	Rectangle<int16_t> UIStatusBar::notice_rect() const
	{
		Point<int16_t> tl = position + Point<int16_t>(MEMO_SLOT_X, BOX_Y);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(MEMO_SLOT_W, hotkey_box.is_valid() ? hotkey_box.height() : 19));
	}

	void UIStatusBar::update()
	{
		position = bar_origin();
		dimension = Point<int16_t>(BAR_WIDTH, BAR_HEIGHT);

		UIElement::update();

		// Age out the boss HP gauge if the boss stopped reporting (dead / left).
		if (boss_hp_ticks > 0)
		{
			if (--boss_hp_ticks <= 0)
				boss_gage.clear();
		}

		// Age notification entries; auto-decline anything older than
		// NotificationCenter::TTL_TICKS (~2 minutes). When the queue
		// drains we drop the badge but keep the slot clickable.
		NotificationCenter::get().tick();
		if (NotificationCenter::get().empty())
			has_notification = false;

		// Fade the menu popup in/out. Target is 1.0 while shown, 0.0 while
		// hidden - a ~8-tick ramp gives a smooth fade instead of a pop. Rows
		// only take clicks at full opacity (see menu_row_at).
		constexpr float FADE_STEP = 1.0f / 8.0f;
		float menu_target = show_menu ? 1.0f : 0.0f;
		if (menu_fade < menu_target)      menu_fade = std::min(menu_target, menu_fade + FADE_STEP);
		else if (menu_fade > menu_target) menu_fade = std::max(menu_target, menu_fade - FADE_STEP);

		if (!show_menu)
			menu_hover = -1;

		float cur_hp = gethppercent();
		float cur_mp = getmppercent();
		float cur_exp = getexppercent();

		// Trigger flash whenever a gauge drops. Small epsilon avoids noise
		// from floating-point jitter.
		constexpr float eps = 0.0005f;
		if (cur_hp + eps < prev_hp_pct)
			hp_flash_ticks = FLASH_DURATION_TICKS;
		if (cur_mp + eps < prev_mp_pct)
			mp_flash_ticks = FLASH_DURATION_TICKS;
		if (cur_exp + eps < prev_exp_pct)
			exp_flash_ticks = FLASH_DURATION_TICKS;

		prev_hp_pct = cur_hp;
		prev_mp_pct = cur_mp;
		prev_exp_pct = cur_exp;

		if (hp_flash_ticks) hp_flash_ticks--;
		if (mp_flash_ticks) mp_flash_ticks--;
		if (exp_flash_ticks) exp_flash_ticks--;

		hp_shown = ease_towards(hp_shown, cur_hp);
		mp_shown = ease_towards(mp_shown, cur_mp);
		exp_shown = ease_towards(exp_shown, cur_exp);

		namelabel.change_text(stats.get_name());
		joblabel.change_text(stats.get_jobname());

		// Update animations
		hp_flash.update();
		mp_flash.update();
		stat_blink.update();
		skill_blink.update();

		// Pulse counter for the notification badge (only advances when active)
		if (has_notification)
			notice_pulse_tick++;
	}

	Button::State UIStatusBar::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case BT_WHISPER:
			UI::get().emplace<UIWhisper>();
			return Button::State::NORMAL;

		case BT_CALLGM:
			UI::get().emplace<UIReport>();
			return Button::State::NORMAL;

		case BT_STATS:
		case BT_MENU_STAT:
			UI::get().emplace<UIStatsInfo>(
				Stage::get().get_player().get_stats()
			);
			remove_menus();
			return Button::State::NORMAL;

		case BT_INVENTORY:
		case BT_MENU_ITEM:
			UI::get().emplace<UIItemInventory>(
				Stage::get().get_player().get_inventory()
			);
			remove_menus();
			return Button::State::NORMAL;

		case BT_EQUIPS:
		case BT_MENU_EQUIP:
			UI::get().emplace<UIEquipInventory>(
				Stage::get().get_player().get_inventory()
			);
			remove_menus();
			return Button::State::NORMAL;

		case BT_SKILL:
		case BT_MENU_SKILL:
			UI::get().emplace<UISkillBook>(
				Stage::get().get_player().get_stats(),
				Stage::get().get_player().get_skills()
			);
			remove_menus();
			return Button::State::NORMAL;

		case BT_MENU_QUEST:
			UI::get().emplace<UIQuestLog>(
				Stage::get().get_player().get_quests()
			);
			remove_menus();
			return Button::State::NORMAL;

		case BT_CASHSHOP:
			OutPacket(OutPacket::Opcode::ENTER_CASHSHOP).dispatch();
			return Button::State::NORMAL;

		case BT_TRADE:
		{
			// Enter Maple Trading System
			OutPacket(OutPacket::Opcode::ENTER_MTS).dispatch();
			return Button::State::NORMAL;
		}

		case BT_MENU:
			toggle_menu();
			return Button::State::NORMAL;

		// BtShort ("SHORT-CUT"), the KeySet hot key and the menu's Key
		// Settings row all open the key config window.
		case BT_KEYSETTING:
		case BT_KEYSET:
		case BT_SYS_KEYSETTING:
			UI::get().emplace<UIKeyConfig>(
				Stage::get().get_player().get_inventory(),
				Stage::get().get_player().get_skills()
			);
			remove_menus();
			return Button::State::NORMAL;

		case BT_NOTICE:
		{
			if (auto alarm = UI::get().get_element<UIAlarmInvite>())
				if (alarm->is_active())
					alarm->stash();

			// Open the notification drawer anchored above the memo slot.
			// The popup positions itself so its bottom-right corner is
			// the slot's top-left.
			UI::get().emplace<UINotificationList>(get_notice_anchor());
			return Button::State::NORMAL;
		}

		case BT_SYS_CHANNEL:
			UI::get().emplace<UIChannel>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_SYS_GAMEQUIT:
			UI::get().emplace<UIQuit>(stats);
			remove_menus();
			return Button::State::NORMAL;

		case BT_SYS_OPTION:
			UI::get().emplace<UIOptionMenu>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_SYS_JOYPAD:
			UI::get().emplace<UIJoypad>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_QS_OPEN:
		case BT_QS_CLOSE:
			toggle_qs();
			return Button::State::NORMAL;

		case BT_MENU_COMMUNITY:
			UI::get().emplace<UIBuddyList>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_MENU_EVENT:
			UI::get().emplace<UIEvent>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_MENU_RANK:
			UI::get().emplace<UIRanking>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_MENU_EPISODBOOK:
			UI::get().emplace<UIMonsterBook>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_MENU_MSN:
			UI::get().emplace<UIMessenger>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_MENU_MONSTERBATTLE:
			UI::get().emplace<UIMonsterBattle>();
			remove_menus();
			return Button::State::NORMAL;

		case BT_MENU_MONSTERLIFE:
			UI::get().emplace<UIMonsterLife>();
			remove_menus();
			return Button::State::NORMAL;

		default:
			return Button::State::NORMAL;
		}
	}

	bool UIStatusBar::is_in_range(Point<int16_t> cursorpos) const
	{
		int16_t vwidth = Constants::Constants::get().get_viewwidth();

		// The bar itself spans the full view width (the strip is stretched).
		Rectangle<int16_t> bar(
			Point<int16_t>(0, position.y()),
			Point<int16_t>(vwidth, position.y() + BAR_HEIGHT)
		);

		if (bar.contains(cursorpos))
			return true;

		if (show_quickslot && quickslot_bg.is_valid())
		{
			Point<int16_t> tl = quickslot_panel_pos();

			if (Rectangle<int16_t>(tl, tl + quickslot_bg.get_dimensions()).contains(cursorpos))
				return true;
		}

		if (show_menu && menu_panel_rect().contains(cursorpos))
			return true;

		return false;
	}

	UIElement::Type UIStatusBar::get_type() const
	{
		return TYPE;
	}

	void UIStatusBar::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			remove_menus();
	}

	void UIStatusBar::toggle_qs()
	{
		show_quickslot = !show_quickslot;

		if (show_quickslot)
		{
			buttons[BT_QS_OPEN]->set_active(false);
			buttons[BT_QS_CLOSE]->set_active(true);
		}
		else
		{
			buttons[BT_QS_OPEN]->set_active(true);
			buttons[BT_QS_CLOSE]->set_active(false);
		}
	}

	void UIStatusBar::toggle_menu()
	{
		show_menu = !show_menu;
		menu_hover = -1;
	}

	void UIStatusBar::remove_menus()
	{
		if (show_menu)
			toggle_menu();
	}

	bool UIStatusBar::is_menu_active()
	{
		return show_menu;
	}

	float UIStatusBar::getexppercent() const
	{
		int16_t level = stats.get_stat(MapleStat::Id::LEVEL);

		if (level >= ExpTable::LEVELCAP)
			return 0.0f;

		int64_t exp = stats.get_exp();

		return static_cast<float>(
			static_cast<double>(exp) / ExpTable::values[level]
		);
	}

	float UIStatusBar::gethppercent() const
	{
		// get_stat returns uint16_t; store in int32_t so HP > 32767 doesn't
		// overflow to a negative int16_t (which made the bar go invisible).
		int32_t hp = stats.get_stat(MapleStat::Id::HP);
		int32_t maxhp = stats.get_total(EquipStat::Id::HP);

		if (maxhp <= 0)
			return 0.0f;
		return std::clamp(static_cast<float>(hp) / static_cast<float>(maxhp), 0.0f, 1.0f);
	}

	float UIStatusBar::getmppercent() const
	{
		int32_t mp = stats.get_stat(MapleStat::Id::MP);
		int32_t maxmp = stats.get_total(EquipStat::Id::MP);

		if (maxmp <= 0)
			return 0.0f;
		return std::clamp(static_cast<float>(mp) / static_cast<float>(maxmp), 0.0f, 1.0f);
	}

	void UIStatusBar::notify()
	{
		// The memo slot is always clickable (so the user can open an empty
		// drawer at any time). Only the badge brightness/pulse toggles with
		// has_notification.
		has_notification = true;
	}

	void UIStatusBar::clear_notification()
	{
		has_notification = false;
	}

	Point<int16_t> UIStatusBar::get_notice_anchor() const
	{
		return notice_rect().get_left_top();
	}

	// === Quickslot drop / render ===

	Point<int16_t> UIStatusBar::quickslot_panel_pos() const
	{
		return position + Point<int16_t>(QS_PANEL_X, QS_PANEL_Y);
	}

	Point<int16_t> UIStatusBar::quickslot_slot_pos(int16_t slot) const
	{
		int16_t col = slot % 4;
		int16_t row = slot / 4;
		return quickslot_panel_pos() + Point<int16_t>(QS_CELL_OFFSET_X + col * QS_COL_STEP,
		                                               QS_CELL_OFFSET_Y + row * QS_ROW_STEP);
	}

	int16_t UIStatusBar::quickslot_slot_at(Point<int16_t> cursorpos) const
	{
		if (!show_quickslot)
			return -1;

		for (int16_t i = 0; i < 8; i++)
		{
			Point<int16_t> tl = quickslot_slot_pos(i);
			Rectangle<int16_t> rect(tl, tl + Point<int16_t>(QS_CELL_W, QS_CELL_H));
			if (rect.contains(cursorpos))
				return i;
		}
		return -1;
	}

	Texture UIStatusBar::get_quickslot_icon(KeyType::Id type, int32_t action) const
	{
		if (action == 0)
			return Texture();

		auto iter = qs_icon_cache.find(action * 8 + type);
		if (iter != qs_icon_cache.end())
			return iter->second;

		// Match the keyboard UI (UIKeyConfig::get_skill_texture / get_item_texture):
		// skills use NORMAL icon, items use the non-raw icon variant.
		Texture tx;
		if (type == KeyType::Id::SKILL)
			tx = SkillData::get(action).get_icon(SkillData::Icon::NORMAL);
		else if (type == KeyType::Id::ITEM)
			tx = ItemData::get(action).get_icon(false);
		else if (type == KeyType::Id::MACRO)
			tx = nl::nx::ui["UIWindow.img"]["SkillMacro"]["Macroicon"][std::to_string(action)]["icon"];

		// v83 skill/item icons have origin (0, 32) so a raw draw at `pos`
		// renders 32 px ABOVE `pos` (matching StatefulIcon's compensation).
		// Shift so the texture's origin becomes (0, 0) - top-left at `pos`.
		tx.shift(Point<int16_t>(0, 32));

		qs_icon_cache[action * 8 + type] = tx;
		return tx;
	}

	void UIStatusBar::assign_quickslot(int16_t slot, KeyType::Id type, int32_t action)
	{
		if (slot < 0 || slot >= 8)
			return;

		uint8_t key = UI::get().get_keyboard().get_quickslot_keys()[slot];

		// Persist locally so subsequent keypresses work immediately.
		UI::get().get_keyboard().assign(key, static_cast<uint8_t>(type), action);

		// Notify the server so the mapping persists across sessions.
		std::vector<std::tuple<KeyConfig::Key, KeyType::Id, int32_t>> updates;
		updates.emplace_back(static_cast<KeyConfig::Key>(key), type, action);
		ChangeKeyMapPacket(updates).dispatch();
	}

	bool UIStatusBar::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		int16_t slot = quickslot_slot_at(cursorpos);
		if (slot < 0)
			return false; // drop outside slots - let caller fall through

		Icon::IconType itype = const_cast<Icon&>(icon).get_type();
		int32_t action = icon.get_action_id();

		if (itype == Icon::IconType::SKILL)
			assign_quickslot(slot, KeyType::Id::SKILL, action);
		else if (itype == Icon::IconType::ITEM)
			assign_quickslot(slot, KeyType::Id::ITEM, action);
		else if (itype == Icon::IconType::MACRO)
			assign_quickslot(slot, KeyType::Id::MACRO, action);

		return true;
	}

	void UIStatusBar::remove_cursor()
	{
		menu_hover = -1;

		UIElement::remove_cursor();
	}

	Cursor::State UIStatusBar::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Menu popup rows (text rows, not MapleButtons): hover highlight and
		// click dispatch through button_pressed with the row's id.
		if (show_menu)
		{
			int16_t row = menu_row_at(cursorpos);
			menu_hover = row;

			if (row >= 0)
			{
				if (clicked)
				{
					Sound(Sound::Name::BUTTONCLICK).play();
					button_pressed(menu_rows[row].id);
					return Cursor::State::IDLE;
				}

				return Cursor::State::CANCLICK;
			}
		}

		// Notification (memo) slot in the base/box tray.
		if (notice_rect().contains(cursorpos))
		{
			if (clicked)
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				button_pressed(BT_NOTICE);
				return Cursor::State::IDLE;
			}

			return Cursor::State::CANCLICK;
		}

		// If cursor is over a quickslot cube that has a binding, allow the
		// user to pick it up and drag it (either to another cube, or off the
		// quickslot to clear the binding).
		if (show_quickslot)
		{
			int16_t slot = quickslot_slot_at(cursorpos);
			if (slot >= 0)
			{
				uint8_t key = UI::get().get_keyboard().get_quickslot_keys()[slot];
				const auto& maplekeys = UI::get().get_keyboard().get_maplekeys();
				auto it = maplekeys.find(static_cast<int32_t>(key));
				if (it != maplekeys.end())
				{
					const Keyboard::Mapping& m = it->second;
					if ((m.type == KeyType::Id::SKILL || m.type == KeyType::Id::ITEM)
						&& m.action != 0)
					{
						if (clicked)
						{
							// Build a fresh Icon owning its own raw texture.
							// The Icon ctor shifts (0,32), so pass the raw
							// (origin 0,32) texture - not the cache's already-
							// shifted one.
							Texture raw;
							if (m.type == KeyType::Id::SKILL)
								raw = SkillData::get(m.action).get_icon(SkillData::Icon::NORMAL);
							else
								raw = ItemData::get(m.action).get_icon(false);

							auto icon = std::make_unique<Icon>(
								std::make_unique<QuickslotDragType>(key, m.type, m.action),
								raw,
								-1);

							// Clear the binding immediately; if the drop
							// lands on a cube, send_icon reassigns it.
							clear_quickslot(key);

							// Compute cursor offset relative to the icon's
							// top-left so dragdraw follows the grip point.
							Point<int16_t> tl = quickslot_slot_pos(slot);
							Point<int16_t> center = tl + Point<int16_t>(
								(QS_CELL_W - 32) / 2, (QS_CELL_H - 32) / 2);

							icon->start_drag(cursorpos - center);

							Icon* raw_ptr = icon.get();
							qs_drag_icons[key] = std::move(icon);
							UI::get().drag_icon(raw_ptr);

							return Cursor::State::GRABBING;
						}
						else
						{
							return Cursor::State::CANGRAB;
						}
					}
				}
			}
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	void UIStatusBar::clear_quickslot(uint8_t key)
	{
		UI::get().get_keyboard().remove(key);

		std::vector<std::tuple<KeyConfig::Key, KeyType::Id, int32_t>> updates;
		updates.emplace_back(static_cast<KeyConfig::Key>(key), KeyType::Id::NONE, 0);
		ChangeKeyMapPacket(updates).dispatch();
	}

	UIStatusBar::QuickslotDragType::QuickslotDragType(uint8_t k, KeyType::Id t, int32_t a)
		: key(k), type(t), action(a) {}

	void UIStatusBar::QuickslotDragType::drop_on_stage() const
	{
		// Binding was already cleared at drag-start. Nothing to do.
	}

	Icon::IconType UIStatusBar::QuickslotDragType::get_type()
	{
		if (type == KeyType::Id::SKILL)
			return Icon::IconType::SKILL;
		if (type == KeyType::Id::ITEM)
			return Icon::IconType::ITEM;
		return Icon::IconType::NONE;
	}

}
