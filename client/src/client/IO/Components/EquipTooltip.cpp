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
#include "EquipTooltip.h"

#include "TooltipFrame.h"

#include "../../Constants.h"
#include "../../Data/WeaponData.h"
#include "../../Gameplay/Stage.h"

#include <algorithm>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	EquipTooltip::EquipTooltip()
	{
		// v83 keeps only the requirement glyphs under UIWindow.img/ToolTip/Equip.
		// The newer UIToolTip.img frame, icon well, potential-rank icons,
		// ATT-increase summary charsets and job icon strip do not exist: the
		// frame and well are drawn with geometry, the job strip uses the v83
		// per-class name bitmaps, and the potential / ATT-increase art is dropped.
		nl::node Equip = nl::nx::ui["UIWindow.img"]["ToolTip"]["Equip"];
		nl::node EquipCan = Equip["Can"];
		nl::node EquipCannot = Equip["Cannot"];

		line = ColorLine(WIDTH - LINE_X * 2, Color::Name::WHITE, 0.3f, false);
		iconwell = ColorBox(ICON_WELL, ICON_WELL, Color::Name::WHITE, 0.65f);

		requirements.push_back(MapleStat::Id::LEVEL);
		requirements.push_back(MapleStat::Id::STR);
		requirements.push_back(MapleStat::Id::DEX);
		requirements.push_back(MapleStat::Id::INT);
		requirements.push_back(MapleStat::Id::LUK);

		reqstattextures[MapleStat::Id::LEVEL][false] = EquipCannot["reqLEV"];
		reqstattextures[MapleStat::Id::LEVEL][true] = EquipCan["reqLEV"];
		reqstattextures[MapleStat::Id::FAME][false] = EquipCannot["reqPOP"];
		reqstattextures[MapleStat::Id::FAME][true] = EquipCan["reqPOP"];
		reqstattextures[MapleStat::Id::STR][false] = EquipCannot["reqSTR"];
		reqstattextures[MapleStat::Id::STR][true] = EquipCan["reqSTR"];
		reqstattextures[MapleStat::Id::DEX][false] = EquipCannot["reqDEX"];
		reqstattextures[MapleStat::Id::DEX][true] = EquipCan["reqDEX"];
		reqstattextures[MapleStat::Id::INT][false] = EquipCannot["reqINT"];
		reqstattextures[MapleStat::Id::INT][true] = EquipCan["reqINT"];
		reqstattextures[MapleStat::Id::LUK][false] = EquipCannot["reqLUK"];
		reqstattextures[MapleStat::Id::LUK][true] = EquipCan["reqLUK"];

		reqstatpositions[MapleStat::Id::LEVEL] = Point<int16_t>(97, 47);
		reqstatpositions[MapleStat::Id::STR] = Point<int16_t>(97, 62);
		reqstatpositions[MapleStat::Id::LUK] = Point<int16_t>(177, 62);
		reqstatpositions[MapleStat::Id::DEX] = Point<int16_t>(97, 71);
		reqstatpositions[MapleStat::Id::INT] = Point<int16_t>(177, 71);

		reqset[false] = Charset(EquipCannot, Charset::Alignment::LEFT);
		reqset[true] = Charset(EquipCan, Charset::Alignment::LEFT);

		// v83 has no YellowNumber charset; the level uses the same digits
		lvset[false] = Charset(EquipCannot, Charset::Alignment::LEFT);
		lvset[true] = Charset(EquipCan, Charset::Alignment::LEFT);

		// v83 spells the wearable classes out as one bitmap per job under
		// Can/ (wearable) and Cannot/ (not wearable). Index order matches
		// okjobs: 0 beginner, 1 warrior, 2 magician, 3 bowman, 4 thief, 5 pirate.
		static const char* const JOB_NAMES[JOB_COUNT] = { "beginner", "warrior", "magician", "bowman", "thief", "pirate" };

		for (uint8_t j = 0; j < JOB_COUNT; j++)
		{
			jobs[false][j] = EquipCannot[JOB_NAMES[j]];
			jobs[true][j] = EquipCan[JOB_NAMES[j]];
		}

		invpos = 0;
		invpos_preview = 0;
	}

	void EquipTooltip::set_equip(Tooltip::Parent parent, int16_t ivp)
	{
		if (invpos == ivp)
			return;

		invpos = ivp;
		invpos_preview = 0;

		const Player& player = Stage::get().get_player();

		InventoryType::Id invtype;

		switch (parent)
		{
		case Tooltip::Parent::ITEMINVENTORY:
		case Tooltip::Parent::SHOP:
			invtype = InventoryType::Id::EQUIP;
			break;
		case Tooltip::Parent::EQUIPINVENTORY:
			invtype = InventoryType::Id::EQUIPPED;
			break;
		default:
			invtype = InventoryType::Id::NONE;
		}

		const Inventory& inventory = player.get_inventory();
		auto oequip = inventory.get_equip(invtype, invpos);
		const CharStats& stats = player.get_stats();

		if (invtype == InventoryType::Id::EQUIP)
		{
			const int32_t item_id = oequip.get()->get_item_id();
			const EquipData& equipdata = EquipData::get(item_id);
			EquipSlot::Id eqslot = equipdata.get_eqslot();

			if (inventory.has_equipped(eqslot));
			{
				auto eequip = inventory.get_equip(InventoryType::Id::EQUIPPED, eqslot);

				if (eequip)
				{
					const Equip& equip = *eequip;

					int32_t item_id = equip.get_item_id();

					const EquipData& equipdata = EquipData::get(item_id);
					const ItemData& itemdata = equipdata.get_itemdata();

					height_preview = 540;

					itemicon_preview = itemdata.get_icon(false);

					for (auto& ms : requirements)
					{
						canequip_preview[ms] = stats.get_stat(ms) >= equipdata.get_reqstat(ms);
						std::string reqstr = std::to_string(equipdata.get_reqstat(ms));

						if (ms != MapleStat::Id::LEVEL)
							if (reqstr.size() < 3) reqstr.insert(0, 3 - reqstr.size(), '0');

						reqstatstrings_preview[ms] = reqstr;
					}

					int16_t reqjob = equipdata.get_reqstat(MapleStat::Id::JOB);
					okjobs_preview = Job::req_job_classes(reqjob);
					canequip_preview[MapleStat::Id::JOB] = stats.get_job().meets_req_job(reqjob);

					prank_preview = equip.get_potrank();

					switch (prank_preview)
					{
					case Equip::Potential::POT_HIDDEN:
						potflag_preview = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::RED);
						potflag_preview.change_text("(Hidden Potential)");
						break;
					case Equip::Potential::POT_RARE:
						potflag_preview = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
						potflag_preview.change_text("(Rare Item)");
						break;
					case Equip::Potential::POT_EPIC:
						potflag_preview = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
						potflag_preview.change_text("(Epic Item)");
						break;
					case Equip::Potential::POT_UNIQUE:
						potflag_preview = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
						potflag_preview.change_text("(Unique Item)");
						break;
					case Equip::Potential::POT_LEGENDARY:
						potflag_preview = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
						potflag_preview.change_text("(Legendary Item)");
						break;
					default:
						height_preview -= 16;
						break;
					}

					Color::Name namecolor;

					switch (equip.get_quality())
					{
					case EquipQuality::Id::GREY:
						namecolor = Color::Name::LIGHTGREY;
						break;
					case EquipQuality::Id::ORANGE:
						namecolor = Color::Name::ORANGE;
						break;
					case EquipQuality::Id::BLUE:
						namecolor = Color::Name::MEDIUMBLUE;
						break;
					case EquipQuality::Id::VIOLET:
						namecolor = Color::Name::VIOLET;
						break;
					case EquipQuality::Id::GOLD:
						namecolor = Color::Name::YELLOW;
						break;
					default:
						namecolor = Color::Name::WHITE;
						break;
					}

					std::string namestr = itemdata.get_name();
					const int8_t reqGender = itemdata.get_gender();
					const bool female = stats.get_female();

					switch (reqGender)
					{
					case 0: // Male
						namestr += " (M)";
						break;
					case 1: // Female
						namestr += " (F)";
						break;
					case 2: // Unisex
					default:
						break;
					}

					if (equip.get_level() > 0)
					{
						namestr.append(" (+");
						namestr.append(std::to_string(equip.get_level()));
						namestr.append(")");
					}

					name_preview = Text(Text::Font::A12B, Text::Alignment::LEFT, namecolor, namestr, 400);

					std::string desctext = itemdata.get_desc();
					hasdesc_preview = desctext.size() > 0;

					if (hasdesc_preview)
					{
						desc_preview = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, desctext, 250);
						height_preview += desc_preview.height() + 10;
					}

					category_preview = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "Type: " + equipdata.get_type());

					is_weapon_preview = equipdata.is_weapon();

					if (is_weapon_preview)
					{
						const WeaponData& weapon = WeaponData::get(item_id);
						wepspeed_preview = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "Attack Speed: " + weapon.getspeedstring());
					}
					else
					{
						height_preview -= 18;
					}

					hasslots_preview = (equip.get_slots() > 0) || (equip.get_level() > 0);

					if (hasslots_preview)
					{
						slots_preview = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "Remaining Enhancements: " + std::to_string(equip.get_slots()));

						std::string vicious = std::to_string(equip.get_vicious());

						if (equip.get_vicious() > 1)
							vicious.append(" (MAX) ");

						hammers_preview = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "Hammers Applied: " + vicious);
					}
					else
					{
						height_preview -= 36;
					}

					statlabels_preview.clear();

					for (EquipStat::Id es = EquipStat::Id::STR; es <= EquipStat::Id::JUMP; es = static_cast<EquipStat::Id>(es + 1))
					{
						if (equip.get_stat(es) > 0)
						{
							int32_t delta = equip.get_stat(es) - equipdata.get_defstat(es);
							std::string statstr = std::to_string(equip.get_stat(es));

							if (delta != 0)
							{
								statstr.append(" (");
								statstr.append((delta < 0) ? "-" : "+");
								statstr.append(std::to_string(abs(delta)) + ")");
							}

							statlabels_preview[es] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, EquipStat::names[es] + std::string(": ") + statstr);
						}
						else
						{
							height_preview -= 18;
						}
					}

					invpos_preview = 1;
				}
			}
		}

		if (!oequip)
			return;

		const Equip& equip = *oequip;

		int32_t item_id = equip.get_item_id();

		const EquipData& equipdata = EquipData::get(item_id);
		const ItemData& itemdata = equipdata.get_itemdata();

		height = 540;

		itemicon = itemdata.get_icon(false);

		for (auto& ms : requirements)
		{
			canequip[ms] = stats.get_stat(ms) >= equipdata.get_reqstat(ms);
			std::string reqstr = std::to_string(equipdata.get_reqstat(ms));

			if (ms != MapleStat::Id::LEVEL)
				if (reqstr.size() < 3) reqstr.insert(0, 3 - reqstr.size(), '0');

			reqstatstrings[ms] = reqstr;
		}

		int16_t reqjob = equipdata.get_reqstat(MapleStat::Id::JOB);
		okjobs = Job::req_job_classes(reqjob);
		canequip[MapleStat::Id::JOB] = stats.get_job().meets_req_job(reqjob);

		prank = equip.get_potrank();

		switch (prank)
		{
		case Equip::Potential::POT_HIDDEN:
			potflag = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::RED);
			potflag.change_text("(Hidden Potential)");
			break;
		case Equip::Potential::POT_RARE:
			potflag = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
			potflag.change_text("(Rare Item)");
			break;
		case Equip::Potential::POT_EPIC:
			potflag = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
			potflag.change_text("(Epic Item)");
			break;
		case Equip::Potential::POT_UNIQUE:
			potflag = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
			potflag.change_text("(Unique Item)");
			break;
		case Equip::Potential::POT_LEGENDARY:
			potflag = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
			potflag.change_text("(Legendary Item)");
			break;
		default:
			height -= 16;
			break;
		}

		Color::Name namecolor;

		switch (equip.get_quality())
		{
		case EquipQuality::Id::GREY:
			namecolor = Color::Name::LIGHTGREY;
			break;
		case EquipQuality::Id::ORANGE:
			namecolor = Color::Name::ORANGE;
			break;
		case EquipQuality::Id::BLUE:
			namecolor = Color::Name::MEDIUMBLUE;
			break;
		case EquipQuality::Id::VIOLET:
			namecolor = Color::Name::VIOLET;
			break;
		case EquipQuality::Id::GOLD:
			namecolor = Color::Name::YELLOW;
			break;
		default:
			namecolor = Color::Name::WHITE;
			break;
		}

		std::string namestr = itemdata.get_name();
		const int8_t reqGender = itemdata.get_gender();
		const bool female = stats.get_female();

		switch (reqGender)
		{
		case 0: // Male
			namestr += " (M)";
			break;
		case 1: // Female
			namestr += " (F)";
			break;
		case 2: // Unisex
		default:
			break;
		}

		if (equip.get_level() > 0)
		{
			namestr.append(" (+");
			namestr.append(std::to_string(equip.get_level()));
			namestr.append(")");
		}

		name = Text(Text::Font::A12B, Text::Alignment::LEFT, namecolor, namestr, 400);

		std::string desctext = itemdata.get_desc();
		hasdesc = desctext.size() > 0;

		if (hasdesc)
		{
			desc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, desctext, 250);
			height += desc.height() + 10;
		}

		category = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "Type: " + equipdata.get_type());

		is_weapon = equipdata.is_weapon();

		if (is_weapon)
		{
			const WeaponData& weapon = WeaponData::get(item_id);
			wepspeed = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "Attack Speed: " + weapon.getspeedstring());
		}
		else
		{
			height -= 18;
		}

		hasslots = (equip.get_slots() > 0) || (equip.get_level() > 0);

		if (hasslots)
		{
			slots = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "Remaining Enhancements: " + std::to_string(equip.get_slots()));

			std::string vicious = std::to_string(equip.get_vicious());

			if (equip.get_vicious() > 1)
				vicious.append(" (MAX) ");

			hammers = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "Hammers Applied: " + vicious);
		}
		else
		{
			height -= 36;
		}

		statlabels.clear();

		for (EquipStat::Id es = EquipStat::Id::STR; es <= EquipStat::Id::JUMP; es = static_cast<EquipStat::Id>(es + 1))
		{
			if (equip.get_stat(es) > 0)
			{
				int32_t delta = equip.get_stat(es) - equipdata.get_defstat(es);
				std::string statstr = std::to_string(equip.get_stat(es));

				if (delta != 0)
				{
					statstr.append(" (");
					statstr.append((delta < 0) ? "-" : "+");
					statstr.append(std::to_string(abs(delta)) + ")");
				}

				statlabels[es] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, EquipStat::names[es] + std::string(": ") + statstr);
			}
			else
			{
				height -= 18;
			}
		}
	}

	void EquipTooltip::draw(Point<int16_t> pos) const
	{
		if (invpos == 0)
			return;

		draw_preview(pos);

		int16_t max_width = Constants::Constants::get().get_viewwidth();
		int16_t max_height = Constants::Constants::get().get_viewheight();
		int16_t cur_width = pos.x() + WIDTH;
		int16_t cur_height = pos.y() + 36;

		if (invpos_preview == 1)
			cur_width += WIDTH;

		if (invpos_preview == 1)
			cur_height += (height > height_preview) ? height : height_preview;
		else
			cur_height += height;

		int16_t adj_x = cur_width - max_width;
		int16_t adj_y = cur_height - max_height;

		if (adj_x > 0)
			pos.shift_x(adj_x * -1);

		if (adj_y > 0)
			pos.shift_y(adj_y * -1);

		TooltipFrame::draw(pos, WIDTH, height + FRAME_PAD);

		name.draw(pos + Point<int16_t>(17, 7));

		if (prank != Equip::Potential::POT_NONE)
		{
			potflag.draw(pos + Point<int16_t>(130, 20));
			pos.shift_y(16);
		}

		pos.shift_y(44);

		line.draw(pos + Point<int16_t>(LINE_X, 0));

		// Icon well on the left; the 2x icon is anchored by its bottom-left origin
		iconwell.draw(DrawArgument(pos + Point<int16_t>(12, 10)));
		itemicon.draw(DrawArgument(pos + Point<int16_t>(18, 82), 2.0f, 2.0f));

		pos.shift_y(12);

		for (MapleStat::Id ms : requirements)
		{
			Point<int16_t> reqpos = reqstatpositions[ms];
			bool reqok = canequip[ms];
			reqstattextures[ms][reqok].draw(pos + reqpos);

			if (ms != MapleStat::Id::LEVEL)
				reqset[reqok].draw(reqstatstrings[ms], 6, pos + reqpos + Point<int16_t>(54, 0));
			else
				lvset[reqok].draw(reqstatstrings[ms], 6, pos + reqpos + Point<int16_t>(54, 0));
		}

		pos.shift_y(88);

		draw_jobs(pos + Point<int16_t>(JOB_X, 14), okjobs, canequip[MapleStat::Id::JOB]);

		line.draw(pos + Point<int16_t>(LINE_X, 47));

		pos.shift_y(49);

		int16_t stat_x = 13;
		int16_t stat_y = 15;

		category.draw(pos + Point<int16_t>(stat_x, 0));

		pos.shift_y(stat_y);

		if (is_weapon)
		{
			wepspeed.draw(pos + Point<int16_t>(stat_x, 0));
			pos.shift_y(stat_y);
		}

		for (const Text& label : statlabels.values())
		{
			if (label.empty())
				continue;

			label.draw(pos + Point<int16_t>(stat_x, 0));
			pos.shift_y(stat_y);
		}

		if (hasslots)
		{
			slots.draw(pos + Point<int16_t>(stat_x, 0));
			pos.shift_y(stat_y);
			hammers.draw(pos + Point<int16_t>(stat_x, 0));
			pos.shift_y(stat_y);
		}

		if (hasdesc)
		{
			pos.shift_y(13);
			line.draw(pos + Point<int16_t>(LINE_X, 0));
			desc.draw(pos + Point<int16_t>(9, 3));
		}
	}

	void EquipTooltip::draw_preview(Point<int16_t> pos) const
	{
		if (invpos_preview == 0)
			return;

		pos.shift_x(WIDTH);

		int16_t max_width = Constants::Constants::get().get_viewwidth();
		int16_t max_height = Constants::Constants::get().get_viewheight();
		int16_t cur_width = pos.x() + WIDTH;
		int16_t cur_height = pos.y() + height_preview + 36;

		int16_t adj_x = cur_width - max_width;
		int16_t adj_y = cur_height - max_height;

		if (adj_x > 0)
			pos.shift_x(adj_x * -1);

		if (adj_y > 0)
			pos.shift_y(adj_y * -1);

		TooltipFrame::draw(pos, WIDTH, height_preview + FRAME_PAD);

		name_preview.draw(pos + Point<int16_t>(17, 7));

		if (prank_preview != Equip::Potential::POT_NONE)
		{
			potflag_preview.draw(pos + Point<int16_t>(130, 20));
			pos.shift_y(16);
		}

		pos.shift_y(44);

		line.draw(pos + Point<int16_t>(LINE_X, 0));

		iconwell.draw(DrawArgument(pos + Point<int16_t>(12, 10)));
		itemicon_preview.draw(DrawArgument(pos + Point<int16_t>(18, 82), 2.0f, 2.0f));

		pos.shift_y(12);

		for (MapleStat::Id ms : requirements)
		{
			Point<int16_t> reqpos = reqstatpositions[ms];
			bool reqok = canequip_preview[ms];
			reqstattextures[ms][reqok].draw(pos + reqpos);

			if (ms != MapleStat::Id::LEVEL)
				reqset[reqok].draw(reqstatstrings_preview[ms], 6, pos + reqpos + Point<int16_t>(54, 0));
			else
				lvset[reqok].draw(reqstatstrings_preview[ms], 6, pos + reqpos + Point<int16_t>(54, 0));
		}

		pos.shift_y(88);

		draw_jobs(pos + Point<int16_t>(JOB_X, 14), okjobs_preview, canequip_preview[MapleStat::Id::JOB]);

		line.draw(pos + Point<int16_t>(LINE_X, 47));

		pos.shift_y(49);

		int16_t stat_x = 13;
		int16_t stat_y = 15;

		category_preview.draw(pos + Point<int16_t>(stat_x, 0));

		pos.shift_y(stat_y);

		if (is_weapon_preview)
		{
			wepspeed_preview.draw(pos + Point<int16_t>(stat_x, 0));
			pos.shift_y(stat_y);
		}

		for (const Text& label : statlabels_preview.values())
		{
			if (label.empty())
				continue;

			label.draw(pos + Point<int16_t>(stat_x, 0));
			pos.shift_y(stat_y);
		}

		if (hasslots_preview)
		{
			slots_preview.draw(pos + Point<int16_t>(stat_x, 0));
			pos.shift_y(stat_y);
			hammers_preview.draw(pos + Point<int16_t>(stat_x, 0));
			pos.shift_y(stat_y);
		}

		if (hasdesc_preview)
		{
			pos.shift_y(-4);
			line.draw(pos + Point<int16_t>(LINE_X, 0));
			desc_preview.draw(pos + Point<int16_t>(9, 8));
		}
	}

	// Draws the v83 class strip: every job name in a row, from the Can/ set
	// when the item allows that job and the player passes the job check,
	// otherwise from the greyed Cannot/ set.
	void EquipTooltip::draw_jobs(Point<int16_t> position, const std::vector<uint8_t>& allowed, bool jobok) const
	{
		for (uint8_t j = 0; j < JOB_COUNT; j++)
		{
			bool ok = jobok && std::find(allowed.begin(), allowed.end(), j) != allowed.end();
			auto iter = jobs[ok].find(j);

			if (iter == jobs[ok].end())
				continue;

			iter->second.draw(position);
			position.shift_x(iter->second.width() + JOB_GAP);
		}
	}
}