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

#include "../../Graphics/Geometry.h"

namespace ms
{
	// v83 ships no UIToolTip.img: UIWindow.img/ToolTip only holds the Equip
	// requirement glyphs, so there is no frame / cover / icon-well art for
	// tooltips. The v83 client paints its tooltips as a translucent black panel
	// with a thin light border; this emulates that with geometry so every
	// tooltip shares one look.
	class TooltipFrame
	{
	public:
		static constexpr float FILL_OPACITY = 0.85f;
		static constexpr float BORDER_OPACITY = 0.45f;

		// Draws the panel with its top-left corner at `topleft`.
		static void draw(Point<int16_t> topleft, int16_t width, int16_t height)
		{
			if (width <= 0 || height <= 0)
				return;

			ColorBox fill(width, height, Color::Name::BLACK, FILL_OPACITY);
			fill.draw(DrawArgument(topleft));

			ColorBox horizontal(width, 1, Color::Name::WHITE, BORDER_OPACITY);
			horizontal.draw(DrawArgument(topleft));
			horizontal.draw(DrawArgument(topleft + Point<int16_t>(0, height - 1)));

			ColorBox vertical(1, height, Color::Name::WHITE, BORDER_OPACITY);
			vertical.draw(DrawArgument(topleft));
			vertical.draw(DrawArgument(topleft + Point<int16_t>(width - 1, 0)));
		}
	};
}
