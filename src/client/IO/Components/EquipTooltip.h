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
#pragma once

#include "Charset.h"
#include "Tooltip.h"

#include "../../Character/MapleStat.h"
#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"
#include "../../Template/BoolPair.h"

#include "../../Character/Inventory/Equip.h"

namespace ms
{
	class EquipTooltip : public Tooltip
	{
	public:
		EquipTooltip();

		void set_equip(Tooltip::Parent parent, int16_t invpos);
		void draw(Point<int16_t> position) const override;
		void draw_preview(Point<int16_t> position) const;

	private:
		void draw_jobs(Point<int16_t> position, const std::vector<uint8_t>& allowed, bool jobok) const;

		// v83 has no UIToolTip.img frame art: the panel is a TooltipFrame of
		// this width; FRAME_PAD is the header/footer space around `height`.
		static constexpr int16_t WIDTH = 264;
		static constexpr int16_t FRAME_PAD = 26;
		// Separator lines are drawn LINE_X in from either edge
		static constexpr int16_t LINE_X = 8;
		// Light well behind the 2x item icon (replaces ItemIcon/base)
		static constexpr int16_t ICON_WELL = 76;
		// Job-name strip: v83 Can/<job> / Cannot/<job> bitmaps laid out in a row
		static constexpr uint8_t JOB_COUNT = 6;
		static constexpr int16_t JOB_X = 12;
		static constexpr int16_t JOB_GAP = 6;

		int16_t invpos;
		int16_t invpos_preview;
		int16_t height;
		int16_t height_preview;
		bool hasdesc;
		bool hasdesc_preview;
		bool hasslots;
		bool hasslots_preview;
		bool is_weapon;
		bool is_weapon_preview;
		EnumMap<MapleStat::Id, std::string> reqstatstrings;
		EnumMap<MapleStat::Id, std::string> reqstatstrings_preview;
		Texture itemicon;
		Texture itemicon_preview;

		Text name;
		Text name_preview;
		Text desc;
		Text desc_preview;
		Text potflag;
		Text potflag_preview;
		Text category;
		Text category_preview;
		Text wepspeed;
		Text wepspeed_preview;
		Text slots;
		Text slots_preview;
		Text hammers;
		Text hammers_preview;
		EnumMap<EquipStat::Id, Text> statlabels;
		EnumMap<EquipStat::Id, Text> statlabels_preview;

		ColorLine line;
		ColorBox iconwell;

		Equip::Potential prank;
		Equip::Potential prank_preview;

		std::vector<MapleStat::Id> requirements;
		EnumMap<MapleStat::Id, BoolPair<Texture>> reqstattextures;
		EnumMap<MapleStat::Id, bool> canequip;
		EnumMap<MapleStat::Id, bool> canequip_preview;
		EnumMap<MapleStat::Id, Point<int16_t>> reqstatpositions;
		BoolPair<Charset> reqset;
		BoolPair<Charset> lvset;

		BoolPair<std::map<uint8_t, Texture>> jobs;
		std::vector<uint8_t> okjobs;
		std::vector<uint8_t> okjobs_preview;
	};
}