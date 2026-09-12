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

#include "../Components/Slider.h"

#include "../../Gameplay/Stage.h"

namespace ms
{
	// v83 minimap (UIWindow.img/MiniMap). Three window states:
	//   MIN    - "Min" strip (w/c/e, 20 px tall): map name + buttons only
	//   NORMAL - "MinMap" 9-slice frame: 29 px title bar, map canvas, 14 px foot
	//   MAX    - "MaxMap" 9-slice frame: 72 px header (title bar + map mark,
	//            region and street names), map canvas, 15 px foot
	// The min/max toggles come from Basic.img (BtMin/BtMax); the world map
	// button is MiniMap/BtMap. v83 has no NPC list, no simple mode and no
	// big/small variants, so those OpenStory features are gone.
	class UIMiniMap : public UIDragElement<PosMINIMAP>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MINIMAP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMiniMap(const CharStats& stats);

		void draw(float alpha) const override;
		void update() override;

		void remove_cursor() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> pos) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		// Highlight an NPC on the minimap by its data ID (quest "Find NPC").
		void set_quest_npc(int32_t npcid);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		/// v83 frame geometry (all from UIWindow.img/MiniMap bitmap sizes)
		// Min strip: w 8x20, c 1x20, e 4x20
		static constexpr int16_t MIN_STRIP_H = 20;
		static constexpr int16_t MIN_STRIP_W_W = 8;
		static constexpr int16_t MIN_STRIP_E_W = 4;
		// MinMap/MaxMap 9-slice: corner/side pieces are 6 px wide
		static constexpr int16_t FRAME_SIDE_W = 6;
		// MinMap: n/nw/ne are 29 px tall, s/sw/se 14 px
		static constexpr int16_t NORMAL_TOP_H = 29;
		static constexpr int16_t NORMAL_BOTTOM_H = 14;
		// MaxMap: n/nw/ne are 72 px tall, s/sw/se 15 px
		static constexpr int16_t MAX_TOP_H = 72;
		static constexpr int16_t MAX_BOTTOM_H = 15;
		// Extra header height of the MAX frame over the NORMAL frame; every
		// canvas-relative position is shifted down by this much in MAX state.
		static constexpr int16_t MAX_ADJ = MAX_TOP_H - NORMAL_TOP_H;
		// Buttons (12 px tall) sit inside the white title box (y 3..22)
		static constexpr int16_t BTN_Y = 6;
		static constexpr int16_t BTN_MIN_Y = 4;
		// Map name text inside the title box
		static constexpr Point<int16_t> TITLE_TEXT_POS = Point<int16_t>(8, 4);
		// Gap between the frame's inner edge and the map canvas
		static constexpr int16_t CANVAS_PAD = 4;
		// MAX header: map mark (38x38) and region/street text in the blue band
		static constexpr Point<int16_t> MARK_POS = Point<int16_t>(8, 27);
		static constexpr int16_t MAX_TEXT_X = 50;
		static constexpr int16_t MAX_REGION_Y = 26;
		static constexpr int16_t MAX_TOWN_Y = 42;
		// Smallest frame width we bother drawing
		static constexpr int16_t MIN_WINDOW_W = 150;

		void update_buttons();
		void toggle_buttons();
		void update_text();
		void update_canvas();
		Point<int16_t> scale_map_pos(Point<int16_t> world_pos) const;
		void draw_canvas_hd(int16_t yadj) const;
		void draw_movable_markers(Point<int16_t> init_pos, float alpha) const;
		Point<int16_t> get_scroll_offset(Point<int16_t> view_dims, int16_t y_adj) const;
		void update_static_markers();
		void update_dimensions();

		enum Buttons
		{
			BT_MIN,
			BT_MAX,
			BT_MAP
		};

		enum Type
		{
			MIN,
			NORMAL,
			MAX
		};

		/// Constants
		int32_t mapid = -1;
		int8_t type;
		int8_t user_type;
		bool scroll_locked;
		bool has_map;
		int16_t scale;
		nl::node Map;
		nl::node MiniMap;
		nl::node marker;
		Texture map_sprite;
		Animation player_marker;
		int16_t combined_text_width;
		int16_t window_width;
		int16_t bt_min_width;
		int16_t bt_max_width;
		int16_t bt_map_width;
		// CPU-upscaled minimap canvas (bicubic x3) drawn via drawraw so
		// zooming samples a pre-smoothed image instead of stretching pixels.
		std::vector<uint8_t> canvas_hd;
		int16_t canvas_hd_w = 0;
		int16_t canvas_hd_h = 0;
		size_t canvas_hd_id = 0;
		Point<int16_t> canvas_pos;

		std::vector<Sprite> min_sprites;
		std::vector<Sprite> normal_sprites;
		std::vector<Sprite> max_sprites;
		std::vector<std::pair<std::string, Point<int16_t>>> static_marker_info;
		int16_t map_draw_origin_x, map_draw_origin_y;
		Point<int16_t> center_offset;
		Point<int16_t> min_dimensions;
		Point<int16_t> normal_dimensions;
		Point<int16_t> max_dimensions;
		Text combined_text;
		Text region_text;
		Text town_text;

		// Quest NPC highlight (MapHelper.img/minimap/startnpc over the NPC)
		int32_t quest_npc_id;
		Animation quest_npc_marker;

		// Pre-allocated draw objects
		mutable Animation portal_marker_anim;
		mutable Animation npc_marker_anim;

		// Drag-to-resize state
		bool drag_resize;
		bool drag_resize_side;
		float zoom_scale;
		int16_t drag_start_y;
		int16_t drag_start_x;
		float drag_start_zoom;

		const CharStats& stats;
	};
}
