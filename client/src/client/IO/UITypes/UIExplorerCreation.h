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

#include "UICreationBase.h"

#include "../../Graphics/Animation.h"

#include <array>
#include <string>

namespace ms
{
	// v83 Explorer creation (Login.img/NewChar): NAME OF CHARACTER sign with a
	// check button first, then the CHARACTER SETTINGS sign with nine avatarSel
	// rows, the STR/DEX/INT/LUK table and the dice, then OK to create.
	//
	// Base flags: named = name accepted by the server (look sign showing),
	// charSet = OK pressed on the look sign (creation packet sent). gender is
	// always true: the v83 sign has no gender board, gender is row 8.
	class UIExplorerCreation : public UICreationBase
	{
	public:
		UIExplorerCreation();

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_naming_result(bool nameused) override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		// Rows the base enum has no arrows for (hair colour, gender) and the
		// name-check button continue after the last base id.
		enum LegacyButtons : uint16_t
		{
			BT_CHARC_CHECK = Buttons::BT_CHARC_HAIRC7 + 1,
			BT_CHARC_HAIRCL,
			BT_CHARC_HAIRCR,
			BT_CHARC_GENDERL,
			BT_CHARC_GENDERR
		};

		void randomize_look();
		void refresh_names();
		void apply_hair();
		void roll_stats();
		void show_name_sign();
		void show_look_sign();
		void set_look_buttons(bool active);
		static const std::string& hair_color_name(uint8_t color);

		// Background is stretched to the view (lay: x and y scaled separately);
		// sign and stat art keep their aspect: alay()/aargs() map a design point
		// at uniform scale away from a design anchor that itself sits at
		// lay(anchor). slay()/sign_args() use the sign anchor, flay() the
		// character-feet anchor.
		Point<int16_t> lay(int16_t x, int16_t y) const;
		Point<int16_t> alay(Point<int16_t> anchor, int16_t x, int16_t y) const;
		DrawArgument aargs(const Texture& t, Point<int16_t> anchor, int16_t x, int16_t y) const;
		Point<int16_t> slay(int16_t x, int16_t y) const;
		DrawArgument sign_args(const Texture& t, int16_t x, int16_t y) const;
		Point<int16_t> flay(int16_t x, int16_t y) const;
		float ui_scale_x;
		float ui_scale_y;

		std::vector<Sprite> sprites_lookboard;
		std::vector<std::pair<Texture, Point<int16_t>>> scenery;
		Texture sky;
		Texture cloud;
		float cloudfx;
		Texture nameboard;

		// Rows the base has no label for.
		Text haircname;
		Text botname;
		Text gendername;

		// Display-only stat roll: the v83 CreateChar packet carries no stats.
		Texture stattable;
		std::array<uint8_t, 4> stats;
		std::array<Text, 4> stattexts;
		Texture dice_idle;
		Animation dice_roll;
		Rectangle<int16_t> dice_bounds;
		bool rolling;
		uint8_t roll_cycles;
	};
}
