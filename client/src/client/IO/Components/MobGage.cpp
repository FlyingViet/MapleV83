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
#include "MobGage.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	MobGage::MobGage() : current_mob(0), hp_percent(0.0f), active(false)
	{
		// v83 UIWindow.img/MobGage: backgrnd is the 35x37 portrait frame,
		// backgrnd2 / backgrnd3 / backgrnd4 are the bar's left cap, 1px middle
		// and right cap, Gage/<layer>/0 is the 1px-wide fill strip and
		// Gage/<layer>/1 its leading edge. Mob/<mobid> holds boss portraits.
		nl::node src = nl::nx::ui["UIWindow.img"]["MobGage"];

		portrait_frame = src["backgrnd"];
		bar_left = src["backgrnd2"];
		bar_mid = src["backgrnd3"];
		bar_right = src["backgrnd4"];
		fill = src["Gage"]["1"]["0"];
		fill_edge = src["Gage"]["1"]["1"];
		mob_icons = src["Mob"];

		name_label = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);
	}

	void MobGage::draw(Point<int16_t> position, float percent) const
	{
		if (!active)
			return;

		portrait_frame.draw(DrawArgument(position));

		if (portrait.is_valid())
			portrait.draw(DrawArgument(position + Point<int16_t>(PORTRAIT_X, PORTRAIT_Y)));

		Point<int16_t> bar_pos = position + Point<int16_t>(portrait_frame.width() + BAR_GAP, BAR_Y);

		bar_left.draw(DrawArgument(bar_pos));
		bar_pos.shift_x(bar_left.width());

		bar_mid.draw(DrawArgument(bar_pos, Point<int16_t>(BAR_WIDTH, 0)));

		if (percent > 0.0f)
		{
			if (percent > 1.0f)
				percent = 1.0f;

			int16_t filled = static_cast<int16_t>(BAR_WIDTH * percent);

			if (filled > 0)
			{
				fill.draw(DrawArgument(bar_pos + Point<int16_t>(0, FILL_Y), Point<int16_t>(filled, 0)));
				fill_edge.draw(DrawArgument(bar_pos + Point<int16_t>(filled - 1, FILL_EDGE_Y)));
			}
		}

		bar_right.draw(DrawArgument(bar_pos + Point<int16_t>(BAR_WIDTH, 0)));

		name_label.draw(DrawArgument(bar_pos + Point<int16_t>(BAR_WIDTH / 2, NAME_Y)));
	}

	void MobGage::set_mob(int32_t mobid, const std::string& name, int8_t boss_type)
	{
		current_mob = mobid;
		active = true;

		name_label.change_text(name);

		// Only bosses with a portrait under Mob/<id> get one; the frame is
		// drawn empty otherwise (the status bar reports id 0 for name-only bars)
		portrait = Texture();

		if (mobid > 0)
		{
			nl::node icon = mob_icons[std::to_string(mobid)];

			if (icon)
				portrait = Texture(icon);
		}
	}

	bool MobGage::is_active() const
	{
		return active;
	}

	void MobGage::clear()
	{
		active = false;
		current_mob = 0;
	}

	int16_t MobGage::width() const
	{
		return portrait_frame.width() + BAR_GAP + bar_left.width() + BAR_WIDTH + bar_right.width();
	}
}
