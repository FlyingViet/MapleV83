//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileWorldMap.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/MessagingPackets.h"
#include "../../Util/Misc.h"

#include <nlnx/nx.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <map>
#include <unordered_map>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t PAD = 14;
		constexpr int16_t HEADER = 66;
		constexpr int16_t LIST_W = 380;
		constexpr int16_t FIELD_H = 44;
		constexpr int16_t HEADER_ROW = 28;
		constexpr int16_t LINK_ROW = 48;
		constexpr int16_t SPOT_ROW = 56;
		constexpr int16_t MAP_ROW = 46;
		constexpr int16_t RESULT_ROW = 58;
		constexpr int16_t GAP = 6;
		constexpr size_t MAX_RESULTS = 60;
		const char* const TOP = "WorldMap";

		enum Target : int32_t
		{
			T_BACK = 1,
			T_CLEAR = 2,
			T_HERE = 3,
			T_LINK = 100,		// + link
			T_SPOT = 1000,		// + spot
			T_GO = 3000,		// + map in the picked place
			T_RESULT = 5000,	// + result
			T_RESULT_GO = 7000	// + result
		};

		int16_t panel_width()
		{
			int16_t vw = Constants::Constants::get().get_viewwidth();

			return static_cast<int16_t>(std::clamp<int>(vw - 12, 760, 1240));
		}

		std::string lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		std::string image_name(const nl::node& n)
		{
			std::string name = n.name();
			size_t dot = name.find(".img");

			return dot == std::string::npos ? name : name.substr(0, dot);
		}

		// Whether this client has the map at all (some world map entries name
		// maps the v83 data leaves out).
		bool map_exists(int32_t id)
		{
			static std::unordered_map<int32_t, bool> known;
			auto it = known.find(id);

			if (it != known.end())
				return it->second;

			std::string padded = std::to_string(id);
			padded.insert(0, padded.size() < 9 ? 9 - padded.size() : 0, '0');

			bool exists = static_cast<bool>(nl::nx::map["Map"]["Map" + std::to_string(id / 100000000)][padded + ".img"]);
			known[id] = exists;

			return exists;
		}

		// Region pictures by name: "WorldMap010" -> "Victoria Island".
		const std::map<std::string, std::string>& region_names()
		{
			static std::map<std::string, std::string> names;

			if (names.empty())
			{
				names[TOP] = "Maple World";

				for (nl::node img : nl::nx::map["WorldMap"])
				{
					for (nl::node link : img["MapLink"])
					{
						std::string to = link["link"]["linkMap"];
						std::string tip = link["toolTip"];

						if (!to.empty() && !tip.empty())
							names[to] = tip;
					}
				}
			}

			return names;
		}

		std::string region_name(const std::string& image)
		{
			auto it = region_names().find(image);

			return it != region_names().end() ? it->second : "Maple World";
		}

		// Which region picture shows each map (the most detailed one).
		const std::unordered_map<int32_t, std::string>& regions_by_map()
		{
			static std::unordered_map<int32_t, std::string> index;

			if (index.empty())
			{
				for (nl::node img : nl::nx::map["WorldMap"])
				{
					std::string name = image_name(img);

					if (name == TOP)
						continue;

					for (nl::node list : img["MapList"])
						for (nl::node no : list["mapNo"])
						{
							int32_t id = no;

							if (map_exists(id) && !index.count(id))
								index[id] = name;
						}
				}
			}

			return index;
		}

		struct Named
		{
			int32_t map;
			std::string name;
			std::string street;
			std::string key;
		};

		// Every place, named, for the search; built on the first search.
		const std::vector<Named>& all_places()
		{
			static std::vector<Named> places;

			if (places.empty())
			{
				for (const auto& entry : regions_by_map())
				{
					NxHelper::Map::MapInfo info = NxHelper::Map::get_map_info_by_id(entry.first);
					std::string name = info.name.empty() ? "Map " + std::to_string(entry.first) : info.name;
					places.push_back({ entry.first, name, info.street_name, lower(name + " " + info.street_name) });
				}

				std::sort(places.begin(), places.end(), [](const Named& a, const Named& b) { return a.name < b.name; });
			}

			return places;
		}

		RGBA type_color(int32_t type)
		{
			switch (type)
			{
				case 0: return Theme::ACCENT;		// town
				case 1: return Theme::BLUE;			// field
				case 2: return Theme::PURPLE;		// dungeon
				default: return Theme::GREEN;		// area
			}
		}

		void dot(Point<int16_t> c, int16_t r, RGBA color)
		{
			fill(rect(static_cast<int16_t>(c.x() - r), static_cast<int16_t>(c.y() - r), static_cast<int16_t>(r * 2), static_cast<int16_t>(r * 2)), color, r);
		}
	}

	UIMobileWorldMap::UIMobileWorldMap() : UIMobilePanel(Edge::LEFT, panel_width()), width(panel_width())
	{
		field = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::WHITE, 1.0f,
			Rectangle<int16_t>(Point<int16_t>(14, 13), Point<int16_t>(LIST_W - 60, 31)), 20);

		show_here();
	}

	UIMobileWorldMap::~UIMobileWorldMap()
	{
		unfocus();
	}

	// ---- data -------------------------------------------------------------

	void UIMobileWorldMap::load(const std::string& name)
	{
		nl::node wm = nl::nx::map["WorldMap"][name + ".img"];
		image = wm ? name : TOP;

		if (!wm)
			wm = nl::nx::map["WorldMap"][std::string(TOP) + ".img"];

		base = wm["BaseImg"]["0"];
		parent = wm["info"]["parentMap"].get_string();
		region = region_name(image);
		Point<int16_t> origin = base.get_origin();

		links.clear();

		for (nl::node link : wm["MapLink"])
		{
			nl::node l = link["link"];
			Texture picture = l["linkImg"];
			Point<int16_t> corner = origin - picture.get_origin();
			Point<int16_t> size = picture.get_dimensions();
			std::string to = l["linkMap"];

			if (to.empty())
				continue;

			links.push_back({ to, region_name(to), picture, rect(corner.x(), corner.y(), size.x(), size.y()) });
		}

		spots.clear();

		for (nl::node list : wm["MapList"])
		{
			Spot spot;
			spot.pos = origin + Point<int16_t>(list["spot"]);
			spot.type = list["type"];

			for (nl::node no : list["mapNo"])
			{
				int32_t id = no;

				if (map_exists(id))
					spot.maps.push_back(id);
			}

			if (spot.maps.empty())
				continue;

			NxHelper::Map::MapInfo info = NxHelper::Map::get_map_info_by_id(spot.maps.front());
			std::string title = list["title"];
			spot.title = !title.empty() ? title : !info.name.empty() ? info.name : "Map " + std::to_string(spot.maps.front());
			spot.subtitle = image == TOP ? "" : info.street_name;
			spots.push_back(std::move(spot));
		}

		selected = -1;
		scroll = 0;
	}

	int32_t UIMobileWorldMap::spot_of(int32_t map) const
	{
		for (size_t i = 0; i < spots.size(); i++)
			for (int32_t id : spots[i].maps)
				if (id == map)
					return static_cast<int32_t>(i);

		return -1;
	}

	void UIMobileWorldMap::show_here()
	{
		here = Stage::get().get_mapid();

		auto found = regions_by_map().find(here);
		std::string target = found != regions_by_map().end() ? found->second : "WorldMap0" + std::to_string(here / 10000000);

		load(target);
		select(spot_of(here));
	}

	void UIMobileWorldMap::select(int32_t index)
	{
		selected = index;

		if (index < 0)
			return;

		// Bring the place into view in the list.
		for (const Row& row : rows())
		{
			if (row.kind == Row::SPOT && static_cast<int32_t>(row.index) == index)
			{
				int16_t visible = list().height();

				if (row.y < scroll || row.y + row.h + MAP_ROW > scroll + visible)
					scroll = static_cast<int16_t>(std::max(0, row.y - 40));

				break;
			}
		}

		clamp_scroll();
	}

	void UIMobileWorldMap::search(const std::string& text)
	{
		query = text;
		results.clear();
		scroll = 0;

		std::string needle = lower(text);

		while (!needle.empty() && needle.back() == ' ')
			needle.pop_back();

		if (needle.empty())
			return;

		std::vector<Result> starts;
		std::vector<Result> contains;

		for (const Named& place : all_places())
		{
			size_t at = place.key.find(needle);

			if (at == std::string::npos)
				continue;

			auto region = regions_by_map().find(place.map);
			std::string where = place.street.empty() ? "" : place.street;
			std::string area = region != regions_by_map().end() ? region_name(region->second) : "";

			if (!area.empty() && area != where)
				where += (where.empty() ? "" : "  \xC2\xB7  ") + area;

			Result r{ place.map, place.name, where, region != regions_by_map().end() ? region->second : TOP };
			(at == 0 ? starts : contains).push_back(std::move(r));
		}

		for (Result& r : starts)
			if (results.size() < MAX_RESULTS)
				results.push_back(std::move(r));

		for (Result& r : contains)
			if (results.size() < MAX_RESULTS)
				results.push_back(std::move(r));
	}

	void UIMobileWorldMap::teleport(int32_t map)
	{
		if (map == here)
			return;

		Sound(Sound::Name::PORTAL).play();
		GeneralChatPacket("@mv83warp " + std::to_string(map), false).dispatch();
		unfocus();
		deactivate();
	}

	void UIMobileWorldMap::unfocus()
	{
		if (field.get_state() == Textfield::State::FOCUSED)
		{
			field.set_state(Textfield::State::NORMAL);
			UI::get().remove_textfield();
		}
	}

	void UIMobileWorldMap::deactivate()
	{
		unfocus();
		UIMobilePanel::deactivate();
	}

	void UIMobileWorldMap::update()
	{
		UIMobilePanel::update();

		field.update(field_rect().get_left_top());

		if (field.get_text() != query)
			search(field.get_text());

		here = Stage::get().get_mapid();
	}

	// ---- layout -----------------------------------------------------------

	Rect UIMobileWorldMap::canvas() const
	{
		Rect f = frame();
		int16_t top = static_cast<int16_t>(f.top() + HEADER);

		return rect(static_cast<int16_t>(f.left() + PAD), top, static_cast<int16_t>(width - PAD * 2 - LIST_W - 12),
			static_cast<int16_t>(f.bottom() - PAD - top));
	}

	Rect UIMobileWorldMap::picture() const
	{
		Rect c = canvas();
		Point<int16_t> size = base.get_dimensions();

		if (size.x() <= 0 || size.y() <= 0)
			return c;

		float scale = std::min({ static_cast<float>(c.width() - 16) / size.x(), static_cast<float>(c.height() - 16) / size.y(), 1.8f });
		int16_t w = static_cast<int16_t>(std::lround(size.x() * scale));
		int16_t h = static_cast<int16_t>(std::lround(size.y() * scale));

		return rect(static_cast<int16_t>(c.left() + (c.width() - w) / 2), static_cast<int16_t>(c.top() + (c.height() - h) / 2), w, h);
	}

	Point<int16_t> UIMobileWorldMap::on_screen(Point<int16_t> image_pos) const
	{
		Rect p = picture();
		Point<int16_t> size = base.get_dimensions();
		float scale = size.x() > 0 ? static_cast<float>(p.width()) / size.x() : 1.0f;

		return Point<int16_t>(static_cast<int16_t>(p.left() + std::lround(image_pos.x() * scale)),
			static_cast<int16_t>(p.top() + std::lround(image_pos.y() * scale)));
	}

	Rect UIMobileWorldMap::field_rect() const
	{
		Rect f = frame();

		return rect(static_cast<int16_t>(f.right() - PAD - LIST_W), static_cast<int16_t>(f.top() + HEADER), LIST_W, FIELD_H);
	}

	Rect UIMobileWorldMap::list() const
	{
		Rect f = frame();
		Rect fr = field_rect();
		int16_t top = static_cast<int16_t>(fr.bottom() + 10);

		return rect(fr.left(), top, LIST_W, static_cast<int16_t>(f.bottom() - PAD - top));
	}

	Rect UIMobileWorldMap::back_rect() const
	{
		Rect f = frame();

		return rect(static_cast<int16_t>(f.left() + 12), static_cast<int16_t>(f.top() + 13), 40, 40);
	}

	Rectangle<int16_t> UIMobileWorldMap::scroll_area() const
	{
		return list();
	}

	std::vector<UIMobileWorldMap::Row> UIMobileWorldMap::rows() const
	{
		std::vector<Row> out;
		int16_t y = 0;

		auto add = [&](Row::Kind kind, size_t index, int16_t h, const std::string& text = "")
		{
			out.push_back({ kind, index, y, h, text });
			y = static_cast<int16_t>(y + h + (kind == Row::HEADER || kind == Row::MAP ? 0 : GAP));
		};

		if (!query.empty())
		{
			if (results.empty())
				add(Row::NOTE, 0, 40, "No place matches \"" + query + "\".");

			for (size_t i = 0; i < results.size(); i++)
				add(Row::RESULT, i, RESULT_ROW);

			return out;
		}

		if (!links.empty())
		{
			add(Row::HEADER, 0, HEADER_ROW, "AREAS");

			for (size_t i = 0; i < links.size(); i++)
				add(Row::LINK, i, LINK_ROW);
		}

		add(Row::HEADER, 0, HEADER_ROW, image == TOP ? "REGIONS" : "PLACES");

		// Where the player is first, then towns, then the rest by name.
		std::vector<size_t> order(spots.size());

		for (size_t i = 0; i < order.size(); i++)
			order[i] = i;

		int32_t mine = spot_of(here);
		std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b)
		{
			if ((static_cast<int32_t>(a) == mine) != (static_cast<int32_t>(b) == mine))
				return static_cast<int32_t>(a) == mine;

			if ((spots[a].type == 0) != (spots[b].type == 0))
				return spots[a].type == 0;

			return spots[a].title < spots[b].title;
		});

		for (size_t i : order)
		{
			add(Row::SPOT, i, SPOT_ROW);

			if (static_cast<int32_t>(i) == selected && image != TOP)
			{
				for (size_t m = 0; m < spots[i].maps.size(); m++)
					add(Row::MAP, m, MAP_ROW);

				y = static_cast<int16_t>(y + GAP);
			}
		}

		return out;
	}

	int16_t UIMobileWorldMap::scroll_content() const
	{
		std::vector<Row> list_rows = rows();

		return list_rows.empty() ? 0 : static_cast<int16_t>(list_rows.back().y + list_rows.back().h + 8);
	}

	Rect UIMobileWorldMap::go_rect(const Row& row) const
	{
		Rect l = list();
		int16_t top = static_cast<int16_t>(l.top() + row.y - scroll);

		return rect(static_cast<int16_t>(l.right() - 12 - 86), static_cast<int16_t>(top + (row.h - 34) / 2), 86, 34);
	}

	// ---- input ------------------------------------------------------------

	Cursor::State UIMobileWorldMap::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// The search box takes the keyboard as soon as it is touched.
		Rect fr = field_rect();
		bool on_clear = !query.empty() && cursorpos.x() > fr.right() - 48;

		if (clicked && fr.contains(cursorpos) && !on_clear && field.get_state() != Textfield::State::FOCUSED)
		{
			field.set_state(Textfield::State::FOCUSED);
			return Cursor::State::CLICKING;
		}

		return UIMobilePanel::send_cursor(clicked, cursorpos);
	}

	int32_t UIMobileWorldMap::hit(Point<int16_t> pos) const
	{
		Rect f = frame();

		if (inset(panel_close(f), -6, -6).contains(pos))
			return CLOSE_TARGET;

		if (!parent.empty() && inset(back_rect(), -6, -6).contains(pos))
			return T_BACK;

		Rect here_button = rect(static_cast<int16_t>(panel_close(f).left() - 10 - 132), static_cast<int16_t>(panel_close(f).top()), 132, 40);

		if (spot_of(here) < 0 && here_button.contains(pos))
			return T_HERE;

		Rect fr = field_rect();

		if (!query.empty() && rect(static_cast<int16_t>(fr.right() - 48), fr.top(), 48, fr.height()).contains(pos))
			return T_CLEAR;

		if (canvas().contains(pos))
		{
			// The nearest place within a finger's reach, else an area.
			int32_t best = -1;
			int32_t best_d = 26 * 26;

			for (size_t i = 0; i < spots.size(); i++)
			{
				Point<int16_t> p = on_screen(spots[i].pos);
				int32_t dx = p.x() - pos.x();
				int32_t dy = p.y() - pos.y();

				if (dx * dx + dy * dy < best_d)
				{
					best_d = dx * dx + dy * dy;
					best = static_cast<int32_t>(i);
				}
			}

			if (best >= 0)
				return T_SPOT + best;

			for (size_t i = 0; i < links.size(); i++)
			{
				const Rect& a = links[i].area;
				Point<int16_t> tl = on_screen(Point<int16_t>(a.left(), a.top()));
				Point<int16_t> br = on_screen(Point<int16_t>(a.right(), a.bottom()));

				if (rect(tl.x(), tl.y(), static_cast<int16_t>(br.x() - tl.x()), static_cast<int16_t>(br.y() - tl.y())).contains(pos))
					return T_LINK + static_cast<int32_t>(i);
			}

			return NO_TARGET;
		}

		Rect l = list();

		if (!l.contains(pos))
			return NO_TARGET;

		for (const Row& row : rows())
		{
			Rect r = rect(l.left(), static_cast<int16_t>(l.top() + row.y - scroll), l.width(), row.h);

			if (!r.contains(pos))
				continue;

			switch (row.kind)
			{
				case Row::LINK:
					return T_LINK + static_cast<int32_t>(row.index);
				case Row::SPOT:
					return T_SPOT + static_cast<int32_t>(row.index);
				case Row::MAP:
					return inset(go_rect(row), -4, -4).contains(pos) ? T_GO + static_cast<int32_t>(row.index) : NO_TARGET;
				case Row::RESULT:
					return inset(go_rect(row), -4, -4).contains(pos) ? T_RESULT_GO + static_cast<int32_t>(row.index)
						: T_RESULT + static_cast<int32_t>(row.index);
				default:
					return NO_TARGET;
			}
		}

		return NO_TARGET;
	}

	void UIMobileWorldMap::activate(int32_t target)
	{
		if (target >= T_RESULT_GO)
		{
			size_t i = static_cast<size_t>(target - T_RESULT_GO);

			if (i < results.size())
				teleport(results[i].map);

			return;
		}

		if (target >= T_RESULT)
		{
			size_t i = static_cast<size_t>(target - T_RESULT);

			if (i >= results.size())
				return;

			// Show the place on its region's map.
			Sound(Sound::Name::BUTTONCLICK).play();
			Result r = results[i];
			field.change_text("");
			query.clear();
			results.clear();
			unfocus();
			load(r.image);
			select(spot_of(r.map));
			return;
		}

		if (target >= T_GO)
		{
			size_t m = static_cast<size_t>(target - T_GO);

			if (selected >= 0 && m < spots[selected].maps.size())
				teleport(spots[selected].maps[m]);

			return;
		}

		if (target >= T_SPOT)
		{
			int32_t i = target - T_SPOT;

			if (i < 0 || i >= static_cast<int32_t>(spots.size()))
				return;

			Sound(Sound::Name::BUTTONCLICK).play();

			// On the world picture a region opens its own map.
			if (image == TOP)
			{
				auto found = regions_by_map().find(spots[i].maps.front());

				if (found != regions_by_map().end())
				{
					std::string to = found->second;
					load(to);

					// A region nested in a bigger one opens at the bigger one.
					if (!parent.empty() && parent != TOP)
						load(parent);

					return;
				}
			}

			unfocus();
			select(selected == i ? -1 : i);
			return;
		}

		if (target >= T_LINK)
		{
			size_t i = static_cast<size_t>(target - T_LINK);

			if (i < links.size())
			{
				Sound(Sound::Name::SELECTMAP).play();
				load(links[i].image);
			}

			return;
		}

		switch (target)
		{
			case T_BACK:
				Sound(Sound::Name::SELECTMAP).play();
				load(parent.empty() ? TOP : parent);
				break;
			case T_CLEAR:
				Sound(Sound::Name::BUTTONCLICK).play();
				field.change_text("");
				search("");
				break;
			case T_HERE:
				Sound(Sound::Name::BUTTONCLICK).play();
				show_here();
				break;
		}
	}

	// ---- drawing ----------------------------------------------------------

	void UIMobileWorldMap::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();

		fill(f, Theme::PANEL, 18);

		// Header: back, region, where the player is.
		int16_t tx = static_cast<int16_t>(f.left() + 22);

		if (!parent.empty())
		{
			icon_button(back_rect(), Glyph::LEFT, Style::SECONDARY, true, pressed == T_BACK);
			tx = static_cast<int16_t>(back_rect().right() + 12);
		}

		Rect close = panel_close(f);
		text(region, Font::HEADLINE, Theme::TEXT, Point<int16_t>(tx, static_cast<int16_t>(f.top() + 12)));
		text("You're in " + NxHelper::Map::get_map_name(here), Font::SMALL, Theme::MUTED, Point<int16_t>(tx, static_cast<int16_t>(f.top() + 42)));

		std::string hint = "Tap a place, then Go to teleport for free";
		text(hint, Font::SMALL, Theme::FAINT, Point<int16_t>(static_cast<int16_t>(field_rect().left() - 16), static_cast<int16_t>(f.top() + 24)),
			Text::Alignment::RIGHT);

		if (spot_of(here) < 0)
			button(rect(static_cast<int16_t>(close.left() - 10 - 132), close.top(), 132, 40), "Where am I", Style::SECONDARY, true,
				pressed == T_HERE, Font::SMALL_BOLD);

		icon_button(close, Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		// The picture, with every place on it.
		Rect c = canvas();
		fill(c, RGBA{ 0.0f, 0.0f, 0.0f, 0.28f }, 16);
		Rect pic = picture();
		MobileUI::icon(base, pic);

		if (pressed >= T_LINK && pressed < T_SPOT)
		{
			const Link& link = links[static_cast<size_t>(pressed - T_LINK)];
			Point<int16_t> tl = on_screen(Point<int16_t>(link.area.left(), link.area.top()));
			Point<int16_t> br = on_screen(Point<int16_t>(link.area.right(), link.area.bottom()));
			MobileUI::icon(link.picture, rect(tl.x(), tl.y(), static_cast<int16_t>(br.x() - tl.x()), static_cast<int16_t>(br.y() - tl.y())));
		}

		int32_t mine = spot_of(here);
		int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ms % 1600) / 1600.0f * 6.2832f);

		for (size_t i = 0; i < spots.size(); i++)
		{
			Point<int16_t> p = on_screen(spots[i].pos);
			bool picked = static_cast<int32_t>(i) == selected;
			RGBA color = type_color(spots[i].type);

			if (picked)
				dot(p, 11, Theme::TEXT);

			dot(p, picked ? 8 : 6, color);
		}

		if (mine >= 0)
		{
			Point<int16_t> p = on_screen(spots[mine].pos);
			dot(p, static_cast<int16_t>(12 + pulse * 6), Theme::GOLD.alpha(0.25f + 0.2f * (1.0f - pulse)));
			dot(p, 7, Theme::GOLD);

			Rect chip = rect(static_cast<int16_t>(p.x() - 22), static_cast<int16_t>(p.y() - 36), 44, 20);
			fill(chip, RGBA{ 0.06f, 0.07f, 0.10f, 0.85f }, 10);
			text_in("You", Font::SMALL_BOLD, Theme::GOLD, chip);
		}

		if (selected >= 0 && selected != mine)
		{
			Point<int16_t> p = on_screen(spots[selected].pos);
			const std::string& name = spots[selected].title;
			int16_t cw = static_cast<int16_t>(text_width(name, Font::SMALL_BOLD) + 20);
			int16_t cx = std::clamp<int16_t>(static_cast<int16_t>(p.x() - cw / 2), c.left(), static_cast<int16_t>(c.right() - cw));
			Rect chip = rect(cx, static_cast<int16_t>(p.y() - 38), cw, 24);
			fill(chip, RGBA{ 0.06f, 0.07f, 0.10f, 0.9f }, 12);
			text_in(name, Font::SMALL_BOLD, Theme::TEXT, chip);
		}

		// Search.
		Rect fr = field_rect();
		bool focused = field.get_state() == Textfield::State::FOCUSED;
		fill(fr, focused ? Theme::CARD_HI : Theme::CARD, 12);

		if (focused)
			fill(rect(static_cast<int16_t>(fr.left() + 12), static_cast<int16_t>(fr.bottom() - 3), static_cast<int16_t>(fr.width() - 24), 2), Theme::ACCENT, 1);

		if (field.empty() && !focused)
			text_in("Search every place", Font::BODY, Theme::FAINT, fr, Text::Alignment::LEFT, 14);

		field.draw(fr.get_left_top());

		if (!query.empty())
			glyph(rect(static_cast<int16_t>(fr.right() - 36), static_cast<int16_t>(fr.top() + 12), 20, 20), Glyph::CLOSE, Theme::MUTED, 12);

		// The list.
		Rect l = list();
		GraphicsGL::get().begin_clip(l.top(), l.bottom());

		for (const Row& row : rows())
		{
			Rect r = rect(l.left(), static_cast<int16_t>(l.top() + row.y - scroll), l.width(), row.h);

			if (r.bottom() < l.top() || r.top() > l.bottom())
				continue;

			switch (row.kind)
			{
				case Row::HEADER:
					text(row.text, Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(r.left() + 2), static_cast<int16_t>(r.top() + 6)));
					break;
				case Row::NOTE:
					text(row.text, Font::BODY, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(r.left() + 4), static_cast<int16_t>(r.top() + 8)),
						Text::Alignment::LEFT, static_cast<int16_t>(r.width() - 8));
					break;
				case Row::LINK:
				{
					int32_t id = T_LINK + static_cast<int32_t>(row.index);
					fill(r, pressed == id ? Theme::CARD_HI : Theme::CARD, 14);
					text_in(links[row.index].name, Font::BODY_BOLD, Theme::TEXT, inset(r, 16, 0), Text::Alignment::LEFT, 0);
					glyph(rect(static_cast<int16_t>(r.right() - 30), static_cast<int16_t>(r.top() + (r.height() - 20) / 2), 20, 20), Glyph::RIGHT, Theme::FAINT, 12);
					break;
				}
				case Row::SPOT:
				{
					const Spot& s = spots[row.index];
					int32_t id = T_SPOT + static_cast<int32_t>(row.index);
					bool picked = static_cast<int32_t>(row.index) == selected;
					bool is_mine = static_cast<int32_t>(row.index) == mine;
					fill(r, pressed == id || picked ? Theme::CARD_HI : Theme::CARD, 14);

					dot(Point<int16_t>(static_cast<int16_t>(r.left() + 22), static_cast<int16_t>(r.top() + r.height() / 2)), 7,
						is_mine ? Theme::GOLD : type_color(s.type));

					int16_t sx = static_cast<int16_t>(r.left() + 42);
					int16_t sw = static_cast<int16_t>(r.right() - 36 - sx);
					text(ellipsize(s.title, Font::BODY_BOLD, sw), Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(sx, static_cast<int16_t>(r.top() + 9)));

					std::string sub = is_mine ? "You are here" : s.subtitle;

					if (image != TOP && s.maps.size() > 1)
						sub += (sub.empty() ? "" : "  \xC2\xB7  ") + std::to_string(s.maps.size()) + " maps";

					text(ellipsize(sub, Font::SMALL, sw), Font::SMALL, is_mine ? Theme::GOLD : Theme::MUTED, Point<int16_t>(sx, static_cast<int16_t>(r.top() + 31)));
					glyph(rect(static_cast<int16_t>(r.right() - 30), static_cast<int16_t>(r.top() + (r.height() - 20) / 2), 20, 20),
						image == TOP ? Glyph::RIGHT : picked ? Glyph::UP : Glyph::DOWN, Theme::FAINT, 12);
					break;
				}
				case Row::MAP:
				{
					int32_t map = spots[selected].maps[row.index];
					Rect inner = rect(static_cast<int16_t>(r.left() + 18), r.top(), static_cast<int16_t>(r.width() - 18), r.height());
					fill(rect(static_cast<int16_t>(r.left() + 20), static_cast<int16_t>(r.top() + 4), 2, static_cast<int16_t>(r.height() - 8)), Theme::LINE);

					Rect go = go_rect(row);
					std::string name = NxHelper::Map::get_map_name(map);
					text_in(ellipsize(name, Font::BODY, static_cast<int16_t>(go.left() - 10 - inner.left() - 14)), Font::BODY,
						map == here ? Theme::GOLD : Theme::TEXT, rect(static_cast<int16_t>(inner.left() + 14), inner.top(),
						static_cast<int16_t>(go.left() - inner.left() - 14), inner.height()), Text::Alignment::LEFT, 0);

					if (map == here)
						text_in("Here", Font::SMALL_BOLD, Theme::GOLD, go);
					else
						button(go, "Go", Style::PRIMARY, true, pressed == T_GO + static_cast<int32_t>(row.index), Font::SMALL_BOLD);

					break;
				}
				case Row::RESULT:
				{
					const Result& res = results[row.index];
					int32_t id = T_RESULT + static_cast<int32_t>(row.index);
					fill(r, pressed == id ? Theme::CARD_HI : Theme::CARD, 14);

					Rect go = go_rect(row);
					int16_t sx = static_cast<int16_t>(r.left() + 16);
					int16_t sw = static_cast<int16_t>(go.left() - 10 - sx);
					text(ellipsize(res.name, Font::BODY_BOLD, sw), Font::BODY_BOLD, res.map == here ? Theme::GOLD : Theme::TEXT,
						Point<int16_t>(sx, static_cast<int16_t>(r.top() + 9)));
					text(ellipsize(res.where, Font::SMALL, sw), Font::SMALL, Theme::MUTED, Point<int16_t>(sx, static_cast<int16_t>(r.top() + 32)));

					if (res.map == here)
						text_in("Here", Font::SMALL_BOLD, Theme::GOLD, go);
					else
						button(go, "Go", Style::PRIMARY, true, pressed == T_RESULT_GO + static_cast<int32_t>(row.index), Font::SMALL_BOLD);

					break;
				}
			}
		}

		GraphicsGL::get().end_clip();
	}

	UIElement::Type UIMobileWorldMap::get_type() const
	{
		return TYPE;
	}
}
