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

#include "../Components/AreaButton.h"

#include "../../Graphics/Animation.h"

namespace ms
{
	// v83 UIWindow.img/WorldMap is only a 9-piece border (Border/0..7) plus a
	// "WORLD MAP" title glyph; the frame is sized around whichever
	// Map.nx/WorldMap/*.img BaseImg is showing. No search panel exists in v83.
	class UIWorldMap : public UIDragElement<PosMAP>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::WORLDMAP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIWorldMap();

		void draw(float inter) const override;
		void update() override;

		void toggle_active() override;

		void remove_cursor() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursor_pos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		static constexpr uint8_t MAPSPOT_TYPE_MAX = 4u;

		// Border/0 (7x33) top-left, Border/1 (1x33) top tile, Border/2 (7x32)
		// top-right, Border/3 / Border/4 (7x1) left / right tiles, Border/5
		// (7x18) bottom-left, Border/6 (1x18) bottom tile, Border/7 (7x18)
		// bottom-right. The 33px top holds the title bar (y 3..20).
		static constexpr int16_t FRAME_L = 7;
		static constexpr int16_t FRAME_R = 7;
		static constexpr int16_t FRAME_T = 33;
		static constexpr int16_t FRAME_B = 18;
		static constexpr int16_t DRAG_H = 22;
		static constexpr Point<int16_t> TITLE_POS = Point<int16_t>(12, 9);   // 53x5 "WORLD MAP" glyph in the title bar
		static constexpr int16_t CLOSE_W = 12;                               // Basic.img/BtClose
		static constexpr int16_t CLOSE_Y = 6;
		static constexpr int16_t CLOSE_MARGIN = 4;
		// Fallback content size when a world map has no BaseImg at all
		static constexpr Point<int16_t> DEFAULT_CONTENT = Point<int16_t>(640, 470);

		void build_frame();
		void update_world(std::string parent_map);

		enum Buttons
		{
			BT_CLOSE,
			BT_LINK0,
			BT_LINK1,
			BT_LINK2,
			BT_LINK3,
			BT_LINK4,
			BT_LINK5,
			BT_LINK6,
			BT_LINK7,
			BT_LINK8,
			BT_LINK9
		};

		struct MapSpot
		{
			std::string description;
			Texture path;
			std::string title;
			uint8_t type;
			Texture marker;
			bool bolded;
			std::vector<int32_t> map_ids;
		};

		bool show_path_img;

		int32_t mapid;

		std::string parent_map;
		std::string user_map;

		Texture border[8];
		Texture title;
		Texture base_img;
		Texture path_img;

		Animation cur_pos;
		Animation npc_pos[MAPSPOT_TYPE_MAX];

		std::map<uint16_t, Texture> link_images;
		std::map<uint16_t, std::string> link_maps;

		std::vector<std::pair<Point<int16_t>, MapSpot>> map_spots;

		// Where the BaseImg's origin lands, relative to the window: spots,
		// links and markers are all relative to this point.
		Point<int16_t> base_position;
	};
}
