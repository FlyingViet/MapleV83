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

#include "../../Graphics/Texture.h"
#include "../../Graphics/Text.h"

#include <nlnx/node.hpp>

namespace ms
{
	// Boss HP gauge overlay built from the v83 UIWindow.img/MobGage pieces:
	// a portrait frame on the left and a capped bar filled by a 1px strip.
	class MobGage
	{
	public:
		MobGage();

		void draw(Point<int16_t> position, float hp_percent) const;
		void set_mob(int32_t mobid, const std::string& name, int8_t boss_type);
		bool is_active() const;
		void clear();
		// Total width of the gauge (portrait frame + bar), for centering on screen.
		int16_t width() const;

	private:
		// Bar interior width in pixels (backgrnd3 is tiled this wide)
		static constexpr int16_t BAR_WIDTH = 300;
		// Gap between the portrait frame and the bar's left cap
		static constexpr int16_t BAR_GAP = 4;
		// Bar top relative to the gauge top: the 20px bar centred on the 37px frame
		static constexpr int16_t BAR_Y = 8;
		// The 10px fill strip sits inside the bar's 12px interior (rows 4..15)
		static constexpr int16_t FILL_Y = 5;
		// The 12px leading-edge strip spans the whole interior
		static constexpr int16_t FILL_EDGE_Y = 4;
		// 25x25 mob portrait inside the 35x37 frame
		static constexpr int16_t PORTRAIT_X = 5;
		static constexpr int16_t PORTRAIT_Y = 6;
		// Boss name, centred on the bar
		static constexpr int16_t NAME_Y = 2;

		Texture portrait_frame;
		Texture portrait;
		Texture bar_left;
		Texture bar_mid;
		Texture bar_right;
		Texture fill;
		Texture fill_edge;
		nl::node mob_icons;
		Text name_label;
		int32_t current_mob;
		float hp_percent;
		bool active;
	};
}
