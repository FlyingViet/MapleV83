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
#include "UIWorldMap.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Gameplay/Stage.h"
#include "../../Util/Misc.h"

#include "../../IO/Components/MapleButton.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIWorldMap::UIWorldMap() : UIDragElement<PosMAP>(Point<int16_t>(DEFAULT_CONTENT.x() + FRAME_L + FRAME_R, DRAG_H)),
		show_path_img(false), mapid(-1)
	{
		nl::node WorldMap = nl::nx::ui["UIWindow.img"]["WorldMap"];
		nl::node Border = WorldMap["Border"];
		nl::node MapHelper = nl::nx::map["MapHelper.img"]["worldMap"];

		for (size_t i = 0; i < 8; i++)
			border[i] = Border[std::to_string(i)];

		title = WorldMap["title"];

		cur_pos = MapHelper["curPos"];

		for (size_t i = 0; i < MAPSPOT_TYPE_MAX; i++)
			npc_pos[i] = MapHelper["npcPos" + std::to_string(i)];

		// v83 Basic.img only has BtClose (12x12) and BtClose2 (14x14);
		// build_frame() moves it to the title bar's right end
		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(nl::nx::ui["Basic.img"]["BtClose"]);

		build_frame();
	}

	void UIWorldMap::build_frame()
	{
		Point<int16_t> content = base_img.is_valid() ? base_img.get_dimensions() : DEFAULT_CONTENT;

		dimension = Point<int16_t>(FRAME_L + content.x() + FRAME_R, FRAME_T + content.y() + FRAME_B);
		dragarea = Point<int16_t>(dimension.x(), DRAG_H);

		// The map's top-left sits in the frame's content area; its origin
		// (centre in newer data, top-left in some v83 images) lands here
		base_position = Point<int16_t>(FRAME_L, FRAME_T) + base_img.get_origin();

		buttons[Buttons::BT_CLOSE]->set_position(Point<int16_t>(dimension.x() - FRAME_R - CLOSE_W - CLOSE_MARGIN, CLOSE_Y));
	}

	void UIWorldMap::draw(float alpha) const
	{
		int16_t w = dimension.x();
		int16_t h = dimension.y();
		int16_t inner_w = w - FRAME_L - FRAME_R;
		int16_t inner_h = h - FRAME_T - FRAME_B;

		// Nine-piece border: corners as-is, the 1px edge tiles stretched
		border[0].draw(DrawArgument(position));
		border[1].draw(DrawArgument(position + Point<int16_t>(FRAME_L, 0), Point<int16_t>(inner_w, FRAME_T)));
		border[2].draw(DrawArgument(position + Point<int16_t>(w - FRAME_R, 0)));
		border[3].draw(DrawArgument(position + Point<int16_t>(0, FRAME_T), Point<int16_t>(FRAME_L, inner_h)));
		border[4].draw(DrawArgument(position + Point<int16_t>(w - FRAME_R, FRAME_T), Point<int16_t>(FRAME_R, inner_h)));
		border[5].draw(DrawArgument(position + Point<int16_t>(0, h - FRAME_B)));
		border[6].draw(DrawArgument(position + Point<int16_t>(FRAME_L, h - FRAME_B), Point<int16_t>(inner_w, FRAME_B)));
		border[7].draw(DrawArgument(position + Point<int16_t>(w - FRAME_R, h - FRAME_B)));

		title.draw(DrawArgument(position + TITLE_POS));

		base_img.draw(DrawArgument(position + base_position));

		if (link_images.size() > 0)
		{
			for (auto& iter : buttons)
			{
				if (const auto button = iter.second.get())
				{
					if (iter.first >= Buttons::BT_LINK0 && button->get_state() == Button::State::MOUSEOVER)
					{
						if (link_images.find(iter.first) != link_images.end())
						{
							link_images.at(iter.first).draw(position + base_position);
							break;
						}
					}
				}
			}
		}

		if (show_path_img)
			path_img.draw(position + base_position);

		for (auto& spot : map_spots)
			spot.second.marker.draw(spot.first + position + base_position);

		for (auto& spot : map_spots)
		{
			bool found = false;

			for (auto map_id : spot.second.map_ids)
			{
				if (map_id == mapid)
				{
					found = true;

					if (spot.second.type < MAPSPOT_TYPE_MAX)
						npc_pos[spot.second.type].draw(spot.first + position + base_position, alpha);

					cur_pos.draw(spot.first + position + base_position, alpha);
					break;
				}
			}

			if (found)
				break;
		}

		UIElement::draw_buttons(alpha);
	}

	void UIWorldMap::update()
	{
		int32_t mid = Stage::get().get_mapid();

		if (mid != mapid)
		{
			mapid = mid;
			auto prefix = mapid / 10000000;
			auto parent_map = "WorldMap0" + std::to_string(prefix);
			user_map = parent_map;

			update_world(parent_map);
		}

		for (size_t i = 0; i < MAPSPOT_TYPE_MAX; i++)
			npc_pos[i].update(1);

		cur_pos.update();

		UIElement::update();
	}

	void UIWorldMap::toggle_active()
	{
		UIElement::toggle_active();

		if (!active)
			update_world(user_map);
	}

	void UIWorldMap::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			if (parent_map == "")
			{
				toggle_active();

				update_world(user_map);
			}
			else
			{
				Sound(Sound::Name::SELECTMAP).play();

				update_world(parent_map);
			}
		}
	}

	UIElement::Type UIWorldMap::get_type() const
	{
		return TYPE;
	}

	Button::State UIWorldMap::button_pressed(uint16_t buttonid)
	{
		if (buttonid == Buttons::BT_CLOSE)
		{
			deactivate();

			return Button::State::NORMAL;
		}

		if (buttonid >= Buttons::BT_LINK0)
		{
			update_world(link_maps[buttonid]);

			return Button::State::IDENTITY;
		}

		return Button::State::NORMAL;
	}

	void UIWorldMap::remove_cursor()
	{
		UIDragElement::remove_cursor();

		UI::get().clear_tooltip(Tooltip::Parent::WORLDMAP);

		show_path_img = false;
	}

	Cursor::State UIWorldMap::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		show_path_img = false;

		for (auto& path : map_spots)
		{
			Point<int16_t> p = path.first + position + base_position - 10;
			Point<int16_t> d = p + path.second.marker.get_dimensions();
			Rectangle<int16_t> abs_bounds = Rectangle<int16_t>(p, d);

			if (abs_bounds.contains(cursorpos))
			{
				path_img = path.second.path;
				show_path_img = path_img.is_valid();

				UI::get().show_map(Tooltip::Parent::WORLDMAP, path.second.title, path.second.description, path.second.map_ids[0], path.second.bolded);
				break;
			}
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIWorldMap::update_world(std::string map)
	{
		nl::node WorldMap = nl::nx::map["WorldMap"][map + ".img"];

		if (!WorldMap)
			WorldMap = nl::nx::map["WorldMap"]["WorldMap.img"];

		base_img = WorldMap["BaseImg"][0];
		parent_map = (std::string)WorldMap["info"]["parentMap"];

		// Resize the border around the new map before placing anything on it
		build_frame();

		link_images.clear();
		link_maps.clear();

		for (auto& iter : buttons)
			if (const auto button = iter.second.get())
				if (iter.first >= Buttons::BT_LINK0)
					button->set_active(false);

		size_t i = Buttons::BT_LINK0;

		for (nl::node link : WorldMap["MapLink"])
		{
			nl::node l = link["link"];
			Texture link_image = l["linkImg"];

			link_images[i] = link_image;
			link_maps[i] = (std::string)l["linkMap"];

			buttons[i] = std::make_unique<AreaButton>(base_position - link_image.get_origin(), link_image.get_dimensions());
			buttons[i]->set_active(true);

			i++;
		}

		nl::node mapImage = nl::nx::map["MapHelper.img"]["worldMap"]["mapImage"];

		map_spots.clear();

		for (nl::node list : WorldMap["MapList"])
		{
			nl::node desc = list["desc"];
			nl::node mapNo = list["mapNo"];
			nl::node path = list["path"];
			nl::node spot = list["spot"];
			nl::node title = list["title"];
			nl::node type = list["type"];
			nl::node marker = mapImage[type];

			std::vector<int32_t> map_ids;

			for (nl::node map_no : mapNo)
				map_ids.push_back(map_no);

			if (!desc && !title)
			{
				NxHelper::Map::MapInfo map_info = NxHelper::Map::get_map_info_by_id(mapNo[0]);

				map_spots.emplace_back(std::make_pair<Point<int16_t>, MapSpot>(spot, { map_info.description, path, map_info.full_name, type, marker, true, map_ids }));
			}
			else
			{
				map_spots.emplace_back(std::make_pair<Point<int16_t>, MapSpot>(spot, { desc, path, title, type, marker, false, map_ids }));
			}
		}
	}
}
