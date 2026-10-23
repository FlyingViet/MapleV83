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
#include "UIStatsInfo.h"

#include "UINotice.h"

#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../Components/TextButton.h"

#include "../../Character/ExpTable.h"
#include "../../Gameplay/Stage.h"

#include "../../Net/Packets/MessagingPackets.h"
#include "../../Net/Packets/PlayerPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// ---- UIWindow.img/Stat/backgrnd (175x347), measured from the v83 bitmap ----
		constexpr int16_t WINDOW_W = 175;
		constexpr int16_t WINDOW_H = 347;

		// Title bar text box y 5..18; the 12x12 Basic.img/BtClose sits at its right end.
		constexpr Point<int16_t> CLOSE_POS = Point<int16_t>(158, 6);

		// The pink label boxes are 13 px tall on an 18 px pitch (STR..LUK: green,
		// 15 px) and the value fields span x 57..166. Text starts 4 px into the
		// field and 1 px below the label box top. The JOB field is double height
		// (y 50..75) so the job name has room.
		constexpr int16_t VALUE_X = 61;
		constexpr int16_t NAME_Y = 35;    // box y 34..46
		constexpr int16_t JOB_Y = 53;     // box y 52..64
		constexpr int16_t LEVEL_Y = 81;   // box y 80..91
		constexpr int16_t GUILD_Y = 99;   // box y 98..109
		constexpr int16_t HP_Y = 117;     // box y 116..127
		constexpr int16_t MP_Y = 135;     // box y 134..146
		constexpr int16_t EXP_Y = 153;    // box y 152..163
		constexpr int16_t FAME_Y = 171;   // box y 170..182
		constexpr int16_t STR_Y = 246;    // box y 245..259
		constexpr int16_t DEX_Y = 264;    // box y 263..277
		constexpr int16_t INT_Y = 282;    // box y 281..295
		constexpr int16_t LUK_Y = 300;    // box y 299..313

		// "ABILITY POINT" section y 195..233 with its white counter box at
		// x 63..87, y 216..228; BtAuto (73x35) fills the space right of the box.
		constexpr Point<int16_t> AP_POS = Point<int16_t>(85, 217);       // right-aligned number
		constexpr Point<int16_t> AUTO_POS = Point<int16_t>(94, 198);

		// The 12x12 BtApUp sits inside the right end of the value field (x 152..164),
		// aligned with the top of the row's label box.
		constexpr int16_t APUP_X = 152;
		constexpr int16_t HP_APUP_Y = 116;
		constexpr int16_t MP_APUP_Y = 134;
		constexpr int16_t STR_APUP_Y = 247;
		constexpr int16_t DEX_APUP_Y = 265;
		constexpr int16_t INT_APUP_Y = 283;
		constexpr int16_t LUK_APUP_Y = 301;

		// Bottom band y 323..341: BtDetail (47x18) on the right, MapleV83's AP
		// RESET on the left.
		constexpr Point<int16_t> DETAIL_BT_POS = Point<int16_t>(119, 324);
		constexpr Point<int16_t> RESET_POS = Point<int16_t>(9, 324);
		constexpr Point<int16_t> RESET_DIM = Point<int16_t>(56, 17);

		// ---- UIWindow.img/Stat/backgrnd2 (177x203): the detail pane ----
		// Docked at the bottom-right, overlapping the 3 px window border and
		// bottom-aligned (347 - 203 = 144; the ilia810 v92 layout uses 143 and
		// that is what the bottom bands line up with).
		constexpr Point<int16_t> DETAIL_OFFSET = Point<int16_t>(172, 143);
		constexpr int16_t DETAIL_W = 177;
		// Blue label boxes are 16 px tall on an 18 px pitch starting at y 7;
		// value fields span x 75..166.
		constexpr int16_t DETAIL_VALUE_X = 79;
		constexpr int16_t DETAIL_ROW0_Y = 9;
		constexpr int16_t DETAIL_ROW_PITCH = 18;
	}

	UIStatsInfo::UIStatsInfo(const CharStats& st) : UIDragElement<PosSTATS>(Point<int16_t>(WINDOW_W, 20)), stats(st), showdetail(false), hasap(false)
	{
		nl::node Stat = nl::nx::ui["UIWindow.img"]["Stat"];
		nl::node close = nl::nx::ui["Basic.img"]["BtClose"];

		sprites.emplace_back(Stat["backgrnd"]);
		detail_backgrnd = Stat["backgrnd2"];

		// v83 has one BtApUp bitmap shared by all six "+" buttons
		nl::node apup = Stat["BtApUp"];

		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close, CLOSE_POS);
		buttons[Buttons::BT_HP] = std::make_unique<MapleButton>(apup, Point<int16_t>(APUP_X, HP_APUP_Y));
		buttons[Buttons::BT_MP] = std::make_unique<MapleButton>(apup, Point<int16_t>(APUP_X, MP_APUP_Y));
		buttons[Buttons::BT_STR] = std::make_unique<MapleButton>(apup, Point<int16_t>(APUP_X, STR_APUP_Y));
		buttons[Buttons::BT_DEX] = std::make_unique<MapleButton>(apup, Point<int16_t>(APUP_X, DEX_APUP_Y));
		buttons[Buttons::BT_INT] = std::make_unique<MapleButton>(apup, Point<int16_t>(APUP_X, INT_APUP_Y));
		buttons[Buttons::BT_LUK] = std::make_unique<MapleButton>(apup, Point<int16_t>(APUP_X, LUK_APUP_Y));
		buttons[Buttons::BT_AUTO] = std::make_unique<MapleButton>(Stat["BtAuto"], AUTO_POS);
		buttons[Buttons::BT_DETAIL] = std::make_unique<MapleButton>(Stat["BtDetail"], DETAIL_BT_POS);
		buttons[Buttons::BT_RESET] = std::make_unique<TextButton>(RESET_POS, RESET_DIM, "RESET AP");

		update_ap();

		for (size_t i = 0; i < StatLabel::NUM_LABELS; i++)
			statlabels[i] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);

		statlabels[StatLabel::AP] = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::EMPEROR);

		statoffsets[StatLabel::NAME] = Point<int16_t>(VALUE_X, NAME_Y);
		statoffsets[StatLabel::JOB] = Point<int16_t>(VALUE_X, JOB_Y);
		statoffsets[StatLabel::LEVEL] = Point<int16_t>(VALUE_X, LEVEL_Y);
		statoffsets[StatLabel::GUILD] = Point<int16_t>(VALUE_X, GUILD_Y);
		statoffsets[StatLabel::HP] = Point<int16_t>(VALUE_X, HP_Y);
		statoffsets[StatLabel::MP] = Point<int16_t>(VALUE_X, MP_Y);
		statoffsets[StatLabel::EXP] = Point<int16_t>(VALUE_X, EXP_Y);
		statoffsets[StatLabel::FAME] = Point<int16_t>(VALUE_X, FAME_Y);
		statoffsets[StatLabel::AP] = AP_POS;
		statoffsets[StatLabel::STR] = Point<int16_t>(VALUE_X, STR_Y);
		statoffsets[StatLabel::DEX] = Point<int16_t>(VALUE_X, DEX_Y);
		statoffsets[StatLabel::INT] = Point<int16_t>(VALUE_X, INT_Y);
		statoffsets[StatLabel::LUK] = Point<int16_t>(VALUE_X, LUK_Y);

		// Detail pane rows, relative to the pane's top-left corner
		for (size_t i = StatLabel::ATTACK; i <= StatLabel::JUMP; i++)
			statoffsets[i] = Point<int16_t>(DETAIL_VALUE_X, DETAIL_ROW0_Y + static_cast<int16_t>(i - StatLabel::ATTACK) * DETAIL_ROW_PITCH);

		update_all_stats();
		update_stat(MapleStat::Id::JOB);
		update_stat(MapleStat::Id::FAME);

		dimension = Point<int16_t>(WINDOW_W, WINDOW_H);
		set_detail(false);
	}

	void UIStatsInfo::draw(float alpha) const
	{
		UIElement::draw_sprites(alpha);

		if (showdetail)
			detail_backgrnd.draw(position + DETAIL_OFFSET);

		size_t last = showdetail ? StatLabel::NUM_LABELS : StatLabel::NUM_NORMAL;

		for (size_t i = 0; i < last; i++)
		{
			Point<int16_t> labelpos = position + statoffsets[i];

			if (i >= StatLabel::NUM_NORMAL)
				labelpos += DETAIL_OFFSET;

			statlabels[i].draw(labelpos);
		}

		UIElement::draw_buttons(alpha);
	}

	void UIStatsInfo::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	bool UIStatsInfo::is_in_range(Point<int16_t> cursorpos) const
	{
		// With the detail pane open the hit area extends to the pane's right edge
		Point<int16_t> pos_adj = showdetail
			? Point<int16_t>(DETAIL_OFFSET.x() + DETAIL_W - WINDOW_W, 0)
			: Point<int16_t>(0, 0);

		auto bounds = Rectangle<int16_t>(position, position + dimension + pos_adj);
		return bounds.contains(cursorpos);
	}

	UIElement::Type UIStatsInfo::get_type() const
	{
		return TYPE;
	}

	void UIStatsInfo::update_all_stats()
	{
		update_simple(StatLabel::AP, MapleStat::Id::AP);

		if (hasap ^ (stats.get_stat(MapleStat::Id::AP) > 0))
			update_ap();

		statlabels[StatLabel::NAME].change_text(stats.get_name());
		statlabels[StatLabel::GUILD].change_text("-");
		update_simple(StatLabel::LEVEL, MapleStat::Id::LEVEL);
		statlabels[StatLabel::HP].change_text(std::to_string(stats.get_stat(MapleStat::Id::HP)) + " / " + std::to_string(stats.get_total(EquipStat::Id::HP)));
		statlabels[StatLabel::MP].change_text(std::to_string(stats.get_stat(MapleStat::Id::MP)) + " / " + std::to_string(stats.get_total(EquipStat::Id::MP)));

		// EXP: absolute value plus the percentage into the current level
		{
			int64_t exp = stats.get_exp();
			uint16_t level = stats.get_stat(MapleStat::Id::LEVEL);
			std::string exptext = std::to_string(exp);

			if (ExpTable::need(level) > 0)
			{
				int64_t percent = exp * 100 / ExpTable::need(level);
				exptext += " (" + std::to_string(percent) + "%)";
			}

			statlabels[StatLabel::EXP].change_text(exptext);
		}

		update_basevstotal(StatLabel::STR, MapleStat::Id::STR, EquipStat::Id::STR);
		update_basevstotal(StatLabel::DEX, MapleStat::Id::DEX, EquipStat::Id::DEX);
		update_basevstotal(StatLabel::INT, MapleStat::Id::INT, EquipStat::Id::INT);
		update_basevstotal(StatLabel::LUK, MapleStat::Id::LUK, EquipStat::Id::LUK);

		// Detail pane: the nine v83 rows, buffed totals in red/blue like the main stats
		update_buffed(StatLabel::ATTACK, EquipStat::Id::WATK);
		update_buffed(StatLabel::WEAPON_DEF, EquipStat::Id::WDEF);
		update_buffed(StatLabel::MAGIC, EquipStat::Id::MAGIC);
		update_buffed(StatLabel::MAGIC_DEF, EquipStat::Id::MDEF);
		update_buffed(StatLabel::ACCURACY, EquipStat::Id::ACC);
		update_buffed(StatLabel::AVOIDABILITY, EquipStat::Id::AVOID);
		update_buffed(StatLabel::HANDS, EquipStat::Id::HANDS);

		statlabels[StatLabel::SPEED].change_text(std::to_string(stats.get_total(EquipStat::Id::SPEED)) + "%");
		statlabels[StatLabel::JUMP].change_text(std::to_string(stats.get_total(EquipStat::Id::JUMP)) + "%");
	}

	void UIStatsInfo::update_stat(MapleStat::Id stat)
	{
		switch (stat)
		{
			case MapleStat::Id::JOB:
				statlabels[StatLabel::JOB].change_text(stats.get_jobname());
				break;
			case MapleStat::Id::FAME:
				update_simple(StatLabel::FAME, MapleStat::Id::FAME);
				break;
		}
	}

	Button::State UIStatsInfo::button_pressed(uint16_t id)
	{
		const Player& player = Stage::get().get_player();

		switch (id)
		{
			case Buttons::BT_CLOSE:
			{
				deactivate();
				break;
			}
			case Buttons::BT_HP:
			{
				send_apup(MapleStat::Id::HP);
				break;
			}
			case Buttons::BT_MP:
			{
				send_apup(MapleStat::Id::MP);
				break;
			}
			case Buttons::BT_STR:
			{
				send_apup(MapleStat::Id::STR);
				break;
			}
			case Buttons::BT_DEX:
			{
				send_apup(MapleStat::Id::DEX);
				break;
			}
			case Buttons::BT_INT:
			{
				send_apup(MapleStat::Id::INT);
				break;
			}
			case Buttons::BT_LUK:
			{
				send_apup(MapleStat::Id::LUK);
				break;
			}
			case Buttons::BT_AUTO:
			{
				int32_t nowap = stats.get_stat(MapleStat::Id::AP);

				if (nowap <= 0)
					break;

				EquipStat::Id id = player.get_stats().get_job().get_primary(player.get_weapontype());
				MapleStat::Id stat = id == EquipStat::Id::DEX ? MapleStat::Id::DEX
					: id == EquipStat::Id::INT ? MapleStat::Id::INT
					: id == EquipStat::Id::LUK ? MapleStat::Id::LUK : MapleStat::Id::STR;

				std::string message = "Put all " + std::to_string(nowap) + " AP into " + EquipStat::names[id] + "?";

				// One request for all of them (the server stops at the stat limit).
				std::function<void(bool)> yesnohandler = [stat, nowap](bool yes)
				{
					if (yes)
						AutoAssignApPacket(stat, nowap).dispatch();
				};

				UI::get().emplace<UIYesNo>(message, yesnohandler, Text::Alignment::LEFT);
				break;
			}
			case Buttons::BT_DETAIL:
			{
				// v83 has a single "DETAIL" button that opens and closes the pane
				set_detail(!showdetail);
				break;
			}
			case Buttons::BT_RESET:
			{
				// Cosmic's @resetap (MapleV83): stats back to the job minimum,
				// the rest as AP to place again.
				UI::get().emplace<UIYesNo>("Put all your ability points back so you can place them again?",
					[](bool yes)
					{
						if (yes)
							GeneralChatPacket("@resetap", false).dispatch();
					});
				break;
			}
			default:
			{
				break;
			}
		}

		return Button::State::NORMAL;
	}

	void UIStatsInfo::send_apup(MapleStat::Id stat) const
	{
		SpendApPacket(stat).dispatch();
		UI::get().disable();
	}

	void UIStatsInfo::set_detail(bool enabled)
	{
		showdetail = enabled;
	}

	void UIStatsInfo::update_ap()
	{
		bool nowap = stats.get_stat(MapleStat::Id::AP) > 0;
		Button::State newstate = nowap ? Button::State::NORMAL : Button::State::DISABLED;

		// Like the v83 client, the "+" buttons only show while there is AP to
		// spend; an inactive button also cannot swallow clicks.
		for (int i = Buttons::BT_HP; i <= Buttons::BT_AUTO; i++)
		{
			buttons[i]->set_state(newstate);
			buttons[i]->set_active(nowap);
		}

		hasap = nowap;
	}

	void UIStatsInfo::update_simple(StatLabel label, MapleStat::Id stat)
	{
		statlabels[label].change_text(std::to_string(stats.get_stat(stat)));
	}

	void UIStatsInfo::update_basevstotal(StatLabel label, MapleStat::Id bstat, EquipStat::Id tstat)
	{
		int32_t base = stats.get_stat(bstat);
		int32_t total = stats.get_total(tstat);
		int32_t delta = total - base;

		std::string stattext = std::to_string(total);

		if (delta)
		{
			stattext += " (" + std::to_string(base);

			if (delta > 0)
				stattext += "+" + std::to_string(delta);
			else if (delta < 0)
				stattext += "-" + std::to_string(-delta);

			stattext += ")";
		}

		statlabels[label].change_text(stattext);
	}

	void UIStatsInfo::update_buffed(StatLabel label, EquipStat::Id stat)
	{
		int32_t total = stats.get_total(stat);
		int32_t delta = stats.get_buffdelta(stat);

		std::string stattext = std::to_string(total);

		if (delta)
		{
			stattext += " (" + std::to_string(total - delta);

			if (delta > 0)
			{
				stattext += "+" + std::to_string(delta);

				statlabels[label].change_color(Color::Name::RED);
			}
			else if (delta < 0)
			{
				stattext += "-" + std::to_string(-delta);

				statlabels[label].change_color(Color::Name::BLUE);
			}

			stattext += ")";
		}
		else
		{
			// Back to the plain colour once a buff has worn off
			statlabels[label].change_color(Color::Name::EMPEROR);
		}

		statlabels[label].change_text(stattext);
	}
}
