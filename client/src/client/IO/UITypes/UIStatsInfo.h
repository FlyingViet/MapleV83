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

#include "../UIDragElement.h"

#include "../../Character/CharStats.h"

namespace ms
{
	// v83 character stat window: UIWindow.img/Stat/backgrnd (175x347) with the
	// detail pane (backgrnd2, 177x203) docked at its bottom-right corner.
	class UIStatsInfo : public UIDragElement<PosSTATS>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::STATSINFO;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIStatsInfo(const CharStats& stats);

		void draw(float alpha) const override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;

		UIElement::Type get_type() const override;

		void update_all_stats();
		void update_stat(MapleStat::Id stat);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		enum StatLabel
		{
			// Main window, top to bottom (v83 row order)
			NAME,
			JOB,
			LEVEL,
			GUILD,
			HP,
			MP,
			EXP,
			FAME,
			AP,
			STR,
			DEX,
			INT,
			LUK,
			NUM_NORMAL,
			// Detail pane, top to bottom (v83 row order)
			ATTACK,
			WEAPON_DEF,
			MAGIC,
			MAGIC_DEF,
			ACCURACY,
			AVOIDABILITY,
			HANDS,
			SPEED,
			JUMP,
			// Total
			NUM_LABELS
		};

		void update_ap();
		void update_simple(StatLabel label, MapleStat::Id stat);
		void update_basevstotal(StatLabel label, MapleStat::Id bstat, EquipStat::Id tstat);
		void update_buffed(StatLabel label, EquipStat::Id stat);
		void send_apup(MapleStat::Id stat) const;
		void set_detail(bool enabled);

		enum Buttons
		{
			BT_CLOSE,
			// BT_HP..BT_AUTO must stay contiguous: update_ap() toggles them as a range
			BT_HP,
			BT_MP,
			BT_STR,
			BT_DEX,
			BT_INT,
			BT_LUK,
			BT_AUTO,
			BT_DETAIL,
			BT_RESET
		};

		const CharStats& stats;

		Texture detail_backgrnd;
		bool showdetail;

		bool hasap;

		Text statlabels[StatLabel::NUM_LABELS];
		Point<int16_t> statoffsets[StatLabel::NUM_LABELS];
	};
}
