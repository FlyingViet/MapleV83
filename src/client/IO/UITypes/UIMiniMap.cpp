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
#include "UIMiniMap.h"

#include "UIWorldMap.h"

#include "../UI.h"

#include "../Components/MapleButton.h"

#include "../../Gameplay/MapleMap/Npc.h"
#include "../../Gameplay/MapleMap/Mob.h"
#include "../../Gameplay/MapleMap/Drop.h"
#include "../../Character/Char.h"
#include "../../Character/Player.h"
#include "../../Character/Look/CharLook.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Graphics/DrawArgument.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

#include <algorithm>
#include <cmath>

namespace ms
{
	UIMiniMap::UIMiniMap(const CharStats& stats) : UIDragElement<PosMINIMAP>(Point<int16_t>(128, 20)), stats(stats)
	{
		scroll_locked = false;
		has_map = false;
		drag_resize = false;
		drag_resize_side = false;
		zoom_scale = 1.5f;
		drag_start_y = 0;
		drag_start_x = 0;
		drag_start_zoom = 1.0f;
		quest_npc_id = 0;
		window_width = MIN_WINDOW_W;
		combined_text_width = 0;
		bt_min_width = 0;
		bt_max_width = 0;
		bt_map_width = 0;
		map_draw_origin_x = FRAME_SIDE_W + CANVAS_PAD;
		map_draw_origin_y = NORMAL_TOP_H + CANVAS_PAD;

		type = Setting<MiniMapType>::get().load();
		user_type = type;

		// v83: frame pieces and the world-map button live in UIWindow.img/MiniMap,
		// the min/max toggles are the generic Basic.img window buttons.
		MiniMap = nl::nx::ui["UIWindow.img"]["MiniMap"];
		nl::node basic = nl::nx::ui["Basic.img"];

		buttons[Buttons::BT_MIN] = std::make_unique<MapleButton>(basic["BtMin"], Point<int16_t>(0, BTN_Y));
		buttons[Buttons::BT_MAX] = std::make_unique<MapleButton>(basic["BtMax"], Point<int16_t>(0, BTN_Y));
		buttons[Buttons::BT_MAP] = std::make_unique<MapleButton>(MiniMap["BtMap"], Point<int16_t>(0, BTN_Y));

		// Title box is white, the MAX header band is blue-grey: dark text on both
		region_text = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::BLACK);
		town_text = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::BLACK);
		combined_text = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::BLACK);

		// v83 has no UI-side DefaultHelper set; markers always come from Map.wz
		marker = nl::nx::map["MapHelper.img"]["minimap"];

		player_marker = Animation(marker["user"]);
		portal_marker_anim = Animation(marker["portal"]);
		npc_marker_anim = Animation(marker["npc"]);
		quest_npc_marker = Animation(marker["startnpc"]);
	}

	void UIMiniMap::draw(float alpha) const
	{
		if (type == Type::MIN)
		{
			for (auto sprite : min_sprites)
				sprite.draw(position, alpha);

			combined_text.draw(position + Point<int16_t>(MIN_STRIP_W_W, 2));
		}
		else if (type == Type::NORMAL)
		{
			for (auto sprite : normal_sprites)
				sprite.draw(position, alpha);

			combined_text.draw(position + TITLE_TEXT_POS);

			draw_canvas_hd(0);

			if (has_map)
			{
				Point<int16_t> scroll_off = get_scroll_offset(normal_dimensions, 0);

				for (auto sprite : static_marker_info)
					portal_marker_anim.draw(position + sprite.second + scroll_off, alpha);

				draw_movable_markers(position + scroll_off, alpha);
			}
		}
		else
		{
			for (auto sprite : max_sprites)
				sprite.draw(position, alpha);

			combined_text.draw(position + TITLE_TEXT_POS);

			draw_canvas_hd(MAX_ADJ);

			region_text.draw(position + Point<int16_t>(MAX_TEXT_X, MAX_REGION_Y));
			town_text.draw(position + Point<int16_t>(MAX_TEXT_X, MAX_TOWN_Y));

			if (has_map)
			{
				Point<int16_t> scroll_off = get_scroll_offset(max_dimensions, MAX_ADJ);

				for (auto sprite : static_marker_info)
					portal_marker_anim.draw(position + sprite.second + Point<int16_t>(0, MAX_ADJ) + scroll_off, alpha);

				draw_movable_markers(position + Point<int16_t>(0, MAX_ADJ) + scroll_off, alpha);
			}
		}

		UIElement::draw(alpha);
	}

	void UIMiniMap::update()
	{
		int32_t mid = Stage::get().get_mapid();

		if (mid != mapid)
		{
			mapid = mid;
			Map = NxHelper::Map::get_map_node_name(mapid);
			quest_npc_id = 0;

			nl::node town = Map["info"]["town"];
			nl::node miniMap = Map["miniMap"];

			if (!miniMap)
			{
				has_map = false;
				type = Type::MIN;
			}
			else
			{
				has_map = true;

				if (town && town.get_bool())
					type = Type::MAX;
				else
					type = user_type;
			}

			scale = std::pow(2, (int)miniMap["mag"]);
			center_offset = Point<int16_t>(miniMap["centerX"], miniMap["centerY"]);

			update_text();
			update_buttons();
			update_canvas();
			update_static_markers();
			toggle_buttons();
		}

		if (type == Type::MIN)
		{
			for (auto sprite : min_sprites)
				sprite.update();
		}
		else if (type == Type::NORMAL)
		{
			for (auto sprite : normal_sprites)
				sprite.update();
		}
		else
		{
			for (auto sprite : max_sprites)
				sprite.update();
		}

		if (quest_npc_id > 0)
			quest_npc_marker.update();

		UIElement::update();
	}

	void UIMiniMap::remove_cursor()
	{
		UIDragElement::remove_cursor();

		UI::get().clear_tooltip(Tooltip::Parent::MINIMAP);
	}

	Cursor::State UIMiniMap::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Handle drag-to-resize on edges (only in NORMAL/MAX mode)
		if (type != Type::MIN && has_map)
		{
			Point<int16_t> cursor_relative = cursorpos - position;
			int16_t bottom_y = dimension.y();
			int16_t right_x = dimension.x();

			// 8px resize zones on bottom, left, and right edges
			bool on_bottom = cursor_relative.y() >= bottom_y - 8 && cursor_relative.y() <= bottom_y + 4
				&& cursor_relative.x() >= 0 && cursor_relative.x() <= right_x;
			bool on_right = cursor_relative.x() >= right_x - 8 && cursor_relative.x() <= right_x + 4
				&& cursor_relative.y() >= NORMAL_TOP_H && cursor_relative.y() <= bottom_y;
			bool on_left = cursor_relative.x() >= -4 && cursor_relative.x() <= 8
				&& cursor_relative.y() >= NORMAL_TOP_H && cursor_relative.y() <= bottom_y;

			if (drag_resize)
			{
				if (clicked)
				{
					float new_zoom;

					if (drag_resize_side)
					{
						int16_t delta_x = cursorpos.x() - drag_start_x;
						new_zoom = drag_start_zoom + static_cast<float>(delta_x) / 100.0f;
					}
					else
					{
						int16_t delta_y = cursorpos.y() - drag_start_y;
						new_zoom = drag_start_zoom + static_cast<float>(delta_y) / 100.0f;
					}

					new_zoom = std::max(0.5f, std::min(4.0f, new_zoom));

					if (new_zoom != zoom_scale)
					{
						zoom_scale = new_zoom;
						update_canvas();
						update_static_markers();
						toggle_buttons();
					}

					return Cursor::State::CLICKING;
				}
				else
				{
					drag_resize = false;
					drag_resize_side = false;
				}
			}
			else if (on_bottom && clicked)
			{
				drag_resize = true;
				drag_resize_side = false;
				drag_start_y = cursorpos.y();
				drag_start_zoom = zoom_scale;
				return Cursor::State::CLICKING;
			}
			else if ((on_right || on_left) && clicked)
			{
				drag_resize = true;
				drag_resize_side = true;
				drag_start_x = cursorpos.x();
				drag_start_zoom = zoom_scale;
				return Cursor::State::CLICKING;
			}
			else if (on_bottom)
			{
				return Cursor::State::VSCROLL;
			}
			else if (on_right || on_left)
			{
				return Cursor::State::HSCROLL;
			}
		}

		Cursor::State dstate = UIDragElement::send_cursor(clicked, cursorpos);

		if (dragged)
			return dstate;

		Point<int16_t> cursor_relative = cursorpos - position;

		bool found = false;
		auto npcs = Stage::get().get_npcs().get_npcs();

		for (auto npc = npcs->begin(); npc != npcs->end(); npc++)
		{
			Point<int16_t> npc_pos = scale_map_pos(npc->second->get_position()) + Point<int16_t>(map_draw_origin_x, map_draw_origin_y);
			Rectangle<int16_t> marker_spot = Rectangle<int16_t>(npc_pos - Point<int16_t>(4, 8), npc_pos);

			if (type == Type::MAX)
				marker_spot.shift(Point<int16_t>(0, MAX_ADJ));

			if (marker_spot.contains(cursor_relative))
			{
				found = true;

				auto n = static_cast<Npc*>(npc->second.get());
				std::string name = n->get_name();
				std::string func = n->get_func();

				UI::get().show_map(Tooltip::Parent::MINIMAP, name, func, {}, false);
				break;
			}
		}

		if (!found)
		{
			for (auto sprite : static_marker_info)
			{
				Rectangle<int16_t> marker_spot = Rectangle<int16_t>(sprite.second, sprite.second + 8);

				if (type == Type::MAX)
					marker_spot.shift(Point<int16_t>(0, MAX_ADJ));

				if (marker_spot.contains(cursor_relative))
				{
					nl::node portal_tm = Map["portal"][sprite.first]["tm"];
					std::string portal_cat = NxHelper::Map::get_map_category(portal_tm);
					nl::node portal_name = nl::nx::string["Map.img"][portal_cat][portal_tm]["mapName"];

					if (portal_name)
					{
						found = true;

						UI::get().show_map(Tooltip::Parent::MINIMAP, portal_name, "", portal_tm, false);
						break;
					}
				}
			}
		}

		return Cursor::State::IDLE;
	}

	void UIMiniMap::send_scroll(double yoffset)
	{
		// Nothing scrollable in the v83 minimap (the NPC list is gone)
	}

	void UIMiniMap::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (has_map)
		{
			if (type < Type::MAX)
				type++;
			else
				type = Type::MIN;

			user_type = type;

			toggle_buttons();
		}
	}

	Button::State UIMiniMap::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case BT_MIN:
			type -= 1;
			// Remember the chosen size so it survives a map change / respawn
			// (update_map resets `type` to `user_type`). Without this, only
			// the keyboard toggle persisted and the map shrank on every warp.
			user_type = type;
			toggle_buttons();
			return type == Type::MIN ? Button::State::DISABLED : Button::State::NORMAL;
		case BT_MAX:
			type += 1;
			user_type = type;
			toggle_buttons();
			return type == Type::MAX ? Button::State::DISABLED : Button::State::NORMAL;
		case BT_MAP:
			UI::get().emplace<UIWorldMap>();
			break;
		}

		return Button::State::NORMAL;
	}

	UIElement::Type UIMiniMap::get_type() const
	{
		return TYPE;
	}

	void UIMiniMap::update_buttons()
	{
		// Add one pixel for a space to the right of each button
		bt_min_width = buttons[Buttons::BT_MIN]->width() + 1;
		bt_max_width = buttons[Buttons::BT_MAX]->width() + 1;
		bt_map_width = buttons[Buttons::BT_MAP]->width() + 1;

		combined_text_width = combined_text.width();
	}

	void UIMiniMap::toggle_buttons()
	{
		buttons[Buttons::BT_MAP]->set_active(true);
		buttons[Buttons::BT_MAX]->set_active(true);
		buttons[Buttons::BT_MIN]->set_active(true);

		if (type == Type::MIN)
		{
			buttons[Buttons::BT_MIN]->set_state(Button::State::DISABLED);

			if (has_map)
				buttons[Buttons::BT_MAX]->set_state(Button::State::NORMAL);
			else
				buttons[Buttons::BT_MAX]->set_state(Button::State::DISABLED);

			// Min strip: [8 px cap][name][4 px gap][min][max][map][4 px cap]
			int16_t bt_x = MIN_STRIP_W_W + combined_text_width + 4;

			buttons[Buttons::BT_MIN]->set_position(Point<int16_t>(bt_x, BTN_MIN_Y));
			bt_x += bt_min_width;
			buttons[Buttons::BT_MAX]->set_position(Point<int16_t>(bt_x, BTN_MIN_Y));
			bt_x += bt_max_width;
			buttons[Buttons::BT_MAP]->set_position(Point<int16_t>(bt_x, BTN_MIN_Y));

			update_dimensions();

			dragarea = dimension;
		}
		else
		{
			buttons[Buttons::BT_MIN]->set_state(Button::State::NORMAL);

			if (type == Type::MAX)
				buttons[Buttons::BT_MAX]->set_state(Button::State::DISABLED);
			else
				buttons[Buttons::BT_MAX]->set_state(Button::State::NORMAL);

			// Right-aligned inside the title box: 6 px frame + 4 px inset
			int16_t bt_x = window_width - FRAME_SIDE_W - 4 - (bt_map_width - 1);

			buttons[Buttons::BT_MAP]->set_position(Point<int16_t>(bt_x, BTN_Y));
			bt_x -= bt_max_width;
			buttons[Buttons::BT_MAX]->set_position(Point<int16_t>(bt_x, BTN_Y));
			bt_x -= bt_min_width;
			buttons[Buttons::BT_MIN]->set_position(Point<int16_t>(bt_x, BTN_Y));

			update_dimensions();

			// Only the title bar drags the window; the canvas edges resize it
			dragarea = Point<int16_t>(dimension.x(), NORMAL_TOP_H);
		}
	}

	void UIMiniMap::update_text()
	{
		NxHelper::Map::MapInfo map_info = NxHelper::Map::get_map_info_by_id(mapid);
		combined_text.change_text(map_info.full_name);
		region_text.change_text(map_info.name);
		town_text.change_text(map_info.street_name);
	}

	void UIMiniMap::update_canvas()
	{
		min_sprites.clear();
		normal_sprites.clear();
		max_sprites.clear();

		nl::node Min = MiniMap["Min"];
		nl::node Normal = MiniMap["MinMap"];
		nl::node Max = MiniMap["MaxMap"];

		map_sprite = Texture(Map["miniMap"]["canvas"]);
		Point<int16_t> map_dimensions = map_sprite.get_dimensions();

		nl::bitmap canvas_bm = Map["miniMap"]["canvas"];
		size_t want_id = 0x4D4D4150ULL << 32 ^ canvas_bm.id();

		if (has_map && canvas_bm.id() && canvas_hd_id != want_id)
		{
			const uint8_t* src = reinterpret_cast<const uint8_t*>(canvas_bm.data());
			int sw = canvas_bm.width(), sh = canvas_bm.height();

			if (src && sw > 0 && sh > 0)
			{
				const int F = 3;
				canvas_hd_w = static_cast<int16_t>(sw * F);
				canvas_hd_h = static_cast<int16_t>(sh * F);
				canvas_hd.assign(static_cast<size_t>(canvas_hd_w) * canvas_hd_h * 4, 0);

				// Catmull-Rom (bicubic) upscale keeps edges much crisper than
				// bilinear, then a light unsharp pass restores line contrast.
				auto cr_w = [](float t)
				{
					float a = t < 0 ? -t : t;
					if (a <= 1.0f)
						return 1.5f * a * a * a - 2.5f * a * a + 1.0f;
					if (a < 2.0f)
						return -0.5f * a * a * a + 2.5f * a * a - 4.0f * a + 2.0f;
					return 0.0f;
				};
				auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

				for (int y = 0; y < canvas_hd_h; y++)
				{
					float fy = (y + 0.5f) / F - 0.5f;
					int yb = static_cast<int>(std::floor(fy));
					float ty = fy - yb;

					for (int x = 0; x < canvas_hd_w; x++)
					{
						float fx = (x + 0.5f) / F - 0.5f;
						int xb = static_cast<int>(std::floor(fx));
						float tx = fx - xb;

						uint8_t* d = &canvas_hd[(static_cast<size_t>(y) * canvas_hd_w + x) * 4];

						for (int ch = 0; ch < 4; ch++)
						{
							float v = 0.0f;
							for (int j = -1; j <= 2; j++)
							{
								int sy = clampi(yb + j, 0, sh - 1);
								float wyj = cr_w(ty - j);
								if (wyj == 0.0f)
									continue;
								for (int i = -1; i <= 2; i++)
								{
									int sx = clampi(xb + i, 0, sw - 1);
									v += src[(static_cast<size_t>(sy) * sw + sx) * 4 + ch]
										* wyj * cr_w(tx - i);
								}
							}
							d[ch] = static_cast<uint8_t>(clampi(static_cast<int>(v + 0.5f), 0, 255));
						}
					}
				}

				// unsharp mask: out = hd + 0.5 * (hd - blur3x3)
				std::vector<uint8_t> soft = canvas_hd;
				for (int y = 1; y < canvas_hd_h - 1; y++)
				{
					for (int x = 1; x < canvas_hd_w - 1; x++)
					{
						uint8_t* d = &canvas_hd[(static_cast<size_t>(y) * canvas_hd_w + x) * 4];
						for (int ch = 0; ch < 3; ch++)
						{
							int blur = 0;
							for (int j = -1; j <= 1; j++)
								for (int i = -1; i <= 1; i++)
									blur += soft[(static_cast<size_t>(y + j) * canvas_hd_w + (x + i)) * 4 + ch];
							blur /= 9;
							int sharp = d[ch] + (d[ch] - blur) / 2;
							d[ch] = static_cast<uint8_t>(clampi(sharp, 0, 255));
						}
					}
				}

				canvas_hd_id = want_id;
			}
		}

		// Apply zoom scale to get the effective display size of the map
		int16_t zoomed_w = static_cast<int16_t>(map_dimensions.x() * zoom_scale);
		int16_t zoomed_h = static_cast<int16_t>(map_dimensions.y() * zoom_scale);

		// Frame width: whichever is widest of the canvas (+ side padding),
		// the title row (name + 3 buttons) and the MAX header texts.
		int16_t title_w = TITLE_TEXT_POS.x() + combined_text_width + 4
			+ bt_min_width + bt_max_width + bt_map_width + FRAME_SIDE_W + 4;
		int16_t mark_text_w = MAX_TEXT_X + std::max(region_text.width(), town_text.width()) + FRAME_SIDE_W + 4;
		int16_t canvas_w = zoomed_w + 2 * (FRAME_SIDE_W + CANVAS_PAD);

		window_width = std::max({ (int)MIN_WINDOW_W, (int)title_w, (int)mark_text_w, (int)canvas_w });

		// Height of the stretched side pieces (the map area between header and foot)
		int16_t middle_h = std::max<int16_t>(zoomed_h + 2 * CANVAS_PAD, 20);

		map_draw_origin_x = std::max<int16_t>(FRAME_SIDE_W + CANVAS_PAD, window_width / 2 - zoomed_w / 2);
		map_draw_origin_y = NORMAL_TOP_H + CANVAS_PAD;
		canvas_pos = Point<int16_t>(map_draw_origin_x, map_draw_origin_y);

		int16_t inner_w = window_width - 2 * FRAME_SIDE_W;
		int16_t right_x = window_width - FRAME_SIDE_W;

		// Min strip queue: [w 8px][c stretched][e 4px]
		int16_t min_w = MIN_STRIP_W_W + combined_text_width + 4 + bt_min_width + bt_max_width + bt_map_width + MIN_STRIP_E_W;
		min_dimensions = Point<int16_t>(min_w, MIN_STRIP_H);

		min_sprites.emplace_back(Min["c"], DrawArgument(Point<int16_t>(MIN_STRIP_W_W, 0), Point<int16_t>(min_w - MIN_STRIP_W_W - MIN_STRIP_E_W, 0)));
		min_sprites.emplace_back(Min["w"], DrawArgument(Point<int16_t>(0, 0)));
		min_sprites.emplace_back(Min["e"], DrawArgument(Point<int16_t>(min_w - MIN_STRIP_E_W, 0)));

		// Normal frame (MinMap 9-slice): 29 px header, stretched middle, 14 px foot
		auto build_frame = [&](std::vector<Sprite>& out, nl::node frame, int16_t top_h)
		{
			int16_t foot_y = top_h + middle_h;

			out.emplace_back(frame["c"], DrawArgument(Point<int16_t>(FRAME_SIDE_W, top_h), Point<int16_t>(inner_w, middle_h)));
			out.emplace_back(frame["w"], DrawArgument(Point<int16_t>(0, top_h), Point<int16_t>(0, middle_h)));
			out.emplace_back(frame["e"], DrawArgument(Point<int16_t>(right_x, top_h), Point<int16_t>(0, middle_h)));
			out.emplace_back(frame["n"], DrawArgument(Point<int16_t>(FRAME_SIDE_W, 0), Point<int16_t>(inner_w, 0)));
			out.emplace_back(frame["nw"], DrawArgument(Point<int16_t>(0, 0)));
			out.emplace_back(frame["ne"], DrawArgument(Point<int16_t>(right_x, 0)));
			out.emplace_back(frame["s"], DrawArgument(Point<int16_t>(FRAME_SIDE_W, foot_y), Point<int16_t>(inner_w, 0)));
			out.emplace_back(frame["sw"], DrawArgument(Point<int16_t>(0, foot_y)));
			out.emplace_back(frame["se"], DrawArgument(Point<int16_t>(right_x, foot_y)));
		};

		build_frame(normal_sprites, Normal, NORMAL_TOP_H);
		normal_dimensions = Point<int16_t>(window_width, NORMAL_TOP_H + middle_h + NORMAL_BOTTOM_H);

		// Max frame (MaxMap 9-slice): 72 px header with the map mark, 15 px foot
		build_frame(max_sprites, Max, MAX_TOP_H);

		nl::node mark = nl::nx::map["MapHelper.img"]["mark"][Map["info"]["mapMark"]];

		if (mark)
			max_sprites.emplace_back(mark, DrawArgument(MARK_POS));

		max_dimensions = Point<int16_t>(window_width, MAX_TOP_H + middle_h + MAX_BOTTOM_H);
	}

	void UIMiniMap::draw_canvas_hd(int16_t yadj) const
	{
		if (!has_map || canvas_hd.empty())
			return;

		int16_t dw = static_cast<int16_t>(canvas_hd_w / 3.0f * zoom_scale);
		int16_t dh = static_cast<int16_t>(canvas_hd_h / 3.0f * zoom_scale);
		Point<int16_t> tl = position + canvas_pos + Point<int16_t>(0, yadj);

		GraphicsGL::get().drawraw(
			canvas_hd_id, canvas_hd_w, canvas_hd_h, canvas_hd.data(),
			Rectangle<int16_t>(tl, tl + Point<int16_t>(dw, dh)),
			Range<int16_t>(0, 0), Color(1.0f, 1.0f, 1.0f, 1.0f), 0.0f);
	}

	Point<int16_t> UIMiniMap::scale_map_pos(Point<int16_t> world_pos) const
	{
		Point<int16_t> map_pos = (world_pos + center_offset) / scale;
		return Point<int16_t>(
			static_cast<int16_t>(map_pos.x() * zoom_scale),
			static_cast<int16_t>(map_pos.y() * zoom_scale)
		);
	}

	void UIMiniMap::draw_movable_markers(Point<int16_t> init_pos, float alpha) const
	{
		if (!has_map)
			return;

		Point<int16_t> origin(map_draw_origin_x, map_draw_origin_y);

		// Draw shrunken copies of the real sprites on the minimap instead of
		// plain marker dots. Size is tied to the minimap ZOOM (so sprites
		// grow/shrink as the minimap is resized) at a fixed readable base,
		// rather than to the per-map magnification — that varies so wildly
		// between maps that a proportional size looked huge on some and
		// invisible on others. The player is drawn a bit larger to stand out.
		float base = (zoom_scale > 1.0f) ? zoom_scale : 1.0f;

		const float MM_MOB_SCALE  = base * 0.12f;
		const float MM_NPC_SCALE  = base * 0.14f;
		const float MM_DROP_SCALE = base * 0.14f;
		const float MM_CHAR_SCALE = base * 0.16f;

		auto mm_point = [&](Point<int16_t> world_pos)
		{
			return scale_map_pos(world_pos) + origin + init_pos;
		};

		auto draw_look = [&](CharLook& look, Point<int16_t> p, float sc)
		{
			look.draw(DrawArgument(p, p, Point<int16_t>(0, 0),
				sc, sc, 1.0f, 0.0f), alpha);
		};

		// These collections are homogeneous (only mobs / drops / npcs /
		// chars), so a static_cast avoids the per-entity dynamic_cast cost.

		/// Monsters
		MapObjects* mobs = Stage::get().get_mobs().get_mobs();
		for (auto it = mobs->begin(); it != mobs->end(); ++it)
		{
			auto* mob = static_cast<Mob*>(it->second.get());
			if (mob && mob->is_active())
				mob->draw_minimap(mm_point(mob->get_position()), MM_MOB_SCALE, alpha);
		}

		/// Drops
		MapObjects* drops = Stage::get().get_drops().get_drops();
		for (auto it = drops->begin(); it != drops->end(); ++it)
		{
			auto* drop = static_cast<Drop*>(it->second.get());
			if (drop && drop->is_active())
				drop->draw_minimap(mm_point(drop->get_position()), MM_DROP_SCALE, alpha);
		}

		/// NPCs
		MapObjects* npcs = Stage::get().get_npcs().get_npcs();
		for (auto it = npcs->begin(); it != npcs->end(); ++it)
		{
			auto* npc = static_cast<Npc*>(it->second.get());
			if (npc && npc->is_active())
			{
				Point<int16_t> p = mm_point(npc->get_position());
				npc->draw_minimap(p, MM_NPC_SCALE, alpha);

				// Quest "Find NPC" highlight: the 4x13 startnpc flag from
				// MapHelper, drawn so its foot sits on the NPC's spot.
				if (quest_npc_id > 0 && npc->get_npcid() == quest_npc_id)
				{
					Point<int16_t> flag_dim = quest_npc_marker.get_dimensions();
					quest_npc_marker.draw(DrawArgument(p - Point<int16_t>(flag_dim.x() / 2, flag_dim.y())), alpha);
				}
			}
		}

		/// Other characters
		MapObjects* chars = Stage::get().get_chars().get_chars();
		for (auto it = chars->begin(); it != chars->end(); ++it)
		{
			auto* chr = static_cast<Char*>(it->second.get());
			if (chr && chr->is_active())
				draw_look(chr->get_look(), mm_point(chr->get_position()), MM_CHAR_SCALE);
		}

		/// Player
		Player& player = Stage::get().get_player();
		draw_look(player.get_look(), mm_point(player.get_position()), MM_CHAR_SCALE);
	}

	Point<int16_t> UIMiniMap::get_scroll_offset(Point<int16_t> view_dims, int16_t y_adj) const
	{
		if (scroll_locked || !has_map)
			return Point<int16_t>(0, 0);

		// Calculate where the player would be drawn on the minimap
		Point<int16_t> player_pos = Stage::get().get_player().get_position();
		Point<int16_t> player_on_map = scale_map_pos(player_pos) + Point<int16_t>(map_draw_origin_x, map_draw_origin_y);

		// The visible area center (approximate center of the minimap view)
		int16_t view_center_x = view_dims.x() / 2;
		int16_t view_center_y = (view_dims.y() + y_adj) / 2;

		// Offset to center the player in the view
		Point<int16_t> offset = Point<int16_t>(view_center_x - player_on_map.x(), view_center_y - player_on_map.y());

		// Clamp so the map doesn't scroll past its edges
		Point<int16_t> map_dims = map_sprite.get_dimensions();
		int16_t zoomed_mx = static_cast<int16_t>(map_dims.x() * zoom_scale);
		int16_t zoomed_my = static_cast<int16_t>(map_dims.y() * zoom_scale);
		int16_t max_offset_x = 0;
		int16_t min_offset_x = std::min(0, (int)view_dims.x() - zoomed_mx - map_draw_origin_x * 2);
		int16_t max_offset_y = 0;
		int16_t min_offset_y = std::min(0, (int)(view_dims.y() - y_adj) - zoomed_my - map_draw_origin_y);

		offset = Point<int16_t>(
			static_cast<int16_t>(std::max((int)min_offset_x, std::min((int)max_offset_x, (int)offset.x()))),
			static_cast<int16_t>(std::max((int)min_offset_y, std::min((int)max_offset_y, (int)offset.y())))
		);

		return offset;
	}

	void UIMiniMap::update_static_markers()
	{
		static_marker_info.clear();

		if (!has_map)
			return;

		/// Portals
		nl::node portals = Map["portal"];
		Point<int16_t> marker_offset = portal_marker_anim.get_dimensions() / Point<int16_t>(2, 0);

		for (nl::node portal = portals.begin(); portal != portals.end(); ++portal)
		{
			int portal_type = portal["pt"];

			if (portal_type == 2)
			{
				Point<int16_t> marker_pos = scale_map_pos(Point<int16_t>(portal["x"], portal["y"])) - marker_offset + Point<int16_t>(map_draw_origin_x, map_draw_origin_y);
				static_marker_info.emplace_back(portal.name(), marker_pos);
			}
		}
	}

	void UIMiniMap::update_dimensions()
	{
		if (type == Type::MIN)
			dimension = min_dimensions;
		else if (type == Type::MAX)
			dimension = max_dimensions;
		else
			dimension = normal_dimensions;
	}

	void UIMiniMap::set_quest_npc(int32_t npcid)
	{
		if (npcid <= 0)
			return;

		// v83 has no NPC list panel; the NPC is flagged on the map itself.
		// Only flag NPCs that are actually on this map so a stale id from
		// another map never draws a marker at a bogus spot.
		auto npcs = Stage::get().get_npcs().get_npcs();

		for (auto it = npcs->begin(); it != npcs->end(); ++it)
		{
			auto* npc = static_cast<Npc*>(it->second.get());

			if (npc && npc->get_npcid() == npcid)
			{
				quest_npc_id = npcid;

				// Pop the window open so the highlight is visible
				if (has_map && type == Type::MIN)
				{
					type = user_type > Type::MIN ? user_type : Type::NORMAL;
					toggle_buttons();
				}

				return;
			}
		}
	}
}
