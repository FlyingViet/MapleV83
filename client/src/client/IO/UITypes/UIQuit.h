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

#include "../UIElement.h"

#include "../../Character/CharStats.h"
#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"

namespace ms
{
	// "Quit to the login screen?" prompt. v83 has no askReward summary window,
	// so this is a plain Basic.img/Notice3 yes/no notice over a dimmed screen
	// that shows this session's play time and keeps the reconnect logic.
	class UIQuit : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::QUIT;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIQuit(const CharStats& stats);

		void draw(float inter) const override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void close();

		enum Buttons : uint16_t
		{
			NO,
			YES
		};

		// Basic.img/Notice3: 266px wide, 21px top (blue from 10px down),
		// 20px centre tiles, 55px bottom whose grey strip (12..52) holds the buttons
		static constexpr int16_t NOTICE_W = 266;
		static constexpr int16_t NOTICE_TOP_H = 21;
		static constexpr int16_t NOTICE_ROW_H = 20;
		static constexpr int16_t NOTICE_BOTTOM_H = 55;
		static constexpr int16_t NOTICE_STRIP_Y = 12;
		static constexpr int16_t NOTICE_STRIP_H = 40;
		static constexpr int16_t NOTICE_TOP_BLUE_Y = 10;
		static constexpr int16_t NOTICE_BOTTOM_BLUE_H = 11;
		static constexpr int16_t NOTICE_TEXT_W = 230;
		static constexpr int16_t NOTICE_ROWS = 3;      // two text lines plus padding
		// Basic.img/BtYes2 and BtNo2 are 46x18
		static constexpr int16_t BUTTON_W = 46;
		static constexpr int16_t BUTTON_H = 18;
		static constexpr int16_t BUTTON_GAP = 8;
		static constexpr int16_t LINE_GAP = 2;      // between the two message lines

		const CharStats& stats;

		ColorBox background;
		Text message;
		Text playtime;
	};
}
