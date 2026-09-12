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

#include "../../Graphics/Texture.h"

namespace ms
{
	// v83 UIWindow.img/Channel: a 436px wide window built from a 67px title +
	// world banner (t), 10px body tiles (c) and a 36px button strip (s), with
	// a 5-column grid of 68x20 channel covers laid over the body.
	class UIChannel : public UIDragElement<PosCHANNEL>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CHANNEL;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIChannel();

		void draw(float inter) const override;
		void update() override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		int8_t channel_by_pos(Point<int16_t> cursorpos) const;
		Point<int16_t> cell_pos(uint8_t channel) const;
		void change_channel();

		enum Buttons : uint16_t
		{
			BT_CANCEL,
			BT_CHANGE
		};

		static constexpr int16_t WIDTH = 436;          // Channel/t, c and s are all 436 wide
		static constexpr int16_t TOP_H = 67;           // Channel/t: title bar + blue world banner
		static constexpr int16_t TILE_H = 10;          // Channel/c body tile
		static constexpr int16_t BOTTOM_H = 36;        // Channel/s button strip
		static constexpr int16_t DRAG_HEIGHT = 20;     // title bar only
		static constexpr uint8_t COLS = 5;
		static constexpr int16_t CELL_W = 68;          // Channel/channel0..2 cover size
		static constexpr int16_t CELL_H = 20;
		static constexpr int16_t GRID_X = 16;          // left edge of the first cover
		static constexpr int16_t STRIDE_HORIZ = 84;    // 5 covers + 16px gaps across 436px
		static constexpr int16_t STRIDE_VERT = CELL_H; // rows touch, as in the original grid
		static constexpr Point<int16_t> WORLD_OFFSET = Point<int16_t>(20, 30);   // world badge inside the blue banner (y 24..56)
		static constexpr Point<int16_t> CH_OFFSET = Point<int16_t>(7, 5);        // "CH.n" glyph inside its cover
		static constexpr int16_t BTN_Y_IN_STRIP = 9;   // BtChange / BtCancel (18px) centred in the 36px strip
		static constexpr int16_t BTN_GAP = 8;
		static constexpr int16_t BT_CHANGE_W = 91;     // Channel/BtChange 91x18
		static constexpr int16_t BT_CANCEL_W = 47;     // Channel/BtCancel 47x18

		uint8_t current_channel;
		uint8_t selected_channel;
		uint8_t channel_count;
		uint8_t rows;

		Texture cover_normal;    // Channel/channel0: idle cell
		Texture cover_current;   // Channel/channel1: the channel we are on
		Texture cover_selected;  // Channel/channel2: the highlighted target
		std::vector<Sprite> ch_sprites;
	};
}
