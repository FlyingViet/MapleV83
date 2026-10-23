//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
//
// The phone's minimap: v83's framed window is too big for a phone, so it is a
// see-through card under the HUD in three sizes - the map's name, a small map,
// or a larger map whose NPCs and portals can be tapped for their names.
#include "UIMiniMap.h"
#include "UIChatBar.h"
#include "UIWorldMap.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Character/Char.h"
#include "../../Configuration.h"
#include "../../Constants.h"
#include "../../Gameplay/MapleMap/Mob.h"
#include "../../Gameplay/MapleMap/Npc.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Util/Misc.h"

#include <algorithm>
#include <cmath>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t PILL_H = 26;
		constexpr int16_t PILL_MAX_W = 260;
		constexpr int16_t SMALL_HEADER = 22;
		constexpr int16_t SMALL_CANVAS_H = 84;
		constexpr int16_t LARGE_W = 520;
		// Room for the name and the three buttons.
		constexpr int16_t LARGE_MIN_W = 340;
		constexpr int16_t LARGE_HEADER = 48;
		constexpr int16_t PAD = 8;
		constexpr float MAX_ZOOM = 3.0f;
		constexpr int32_t LABEL_TICKS = 240;

		constexpr RGBA BACKDROP = { 0.02f, 0.03f, 0.06f, 0.50f };
		constexpr RGBA SHADOW = { 0.0f, 0.0f, 0.0f, 0.75f };
		constexpr RGBA PORTAL = { 0.72f, 0.56f, 1.00f, 1.0f };
		constexpr RGBA OTHERS = { 0.45f, 0.78f, 1.00f, 1.0f };

		void dot(Point<int16_t> p, int16_t r, RGBA c)
		{
			fill(rect(static_cast<int16_t>(p.x() - r - 1), static_cast<int16_t>(p.y() - r - 1),
				static_cast<int16_t>(r * 2 + 2), static_cast<int16_t>(r * 2 + 2)), SHADOW, static_cast<int16_t>(r + 1));
			fill(rect(static_cast<int16_t>(p.x() - r), static_cast<int16_t>(p.y() - r),
				static_cast<int16_t>(r * 2), static_cast<int16_t>(r * 2)), c, r);
		}

		void shadowed(const std::string& str, Font font, RGBA color, const Rect& r, Text::Alignment align, int16_t pad)
		{
			text_in(str, font, SHADOW, rect(static_cast<int16_t>(r.left() + 1), static_cast<int16_t>(r.top() + 1), r.width(), r.height()), align, pad);
			text_in(str, font, color, r, align, pad);
		}
	}

	bool UIMiniMap::is_expanded() const
	{
		return mobile && type == Type::MAX;
	}

	void UIMiniMap::shrink()
	{
		if (is_expanded())
			mobile_set_type(Type::NORMAL);
	}

	void UIMiniMap::mobile_set_type(int8_t next)
	{
		if (!has_map)
			next = Type::MIN;

		type = next;
		user_type = next;
		picked_ticks = 0;
		// Written now: iOS ends apps without running the exit save.
		Setting<MiniMapType>::get().save(static_cast<uint8_t>(next));
		Configuration::get().save();

		// The large map and the chat window both want the left side.
		if (type == Type::MAX)
			if (auto chatbar = UI::get().get_element<UIChatBar>())
				if (chatbar->is_chatopen())
					chatbar->toggle_chat();

		Rectangle<int16_t> f = mobile_frame();
		position = f.get_left_top();
		dimension = Point<int16_t>(f.width(), f.height());
	}

	Rect UIMiniMap::mobile_canvas() const
	{
		if (!has_map || type == Type::MIN)
			return {};

		Point<int16_t> map = map_sprite.get_dimensions();

		if (map.x() <= 0 || map.y() <= 0)
			return {};

		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t left = HUD_LEFT + PAD;
		int16_t top;
		int16_t room_w;
		int16_t room_h;

		if (type == Type::NORMAL)
		{
			top = static_cast<int16_t>(HUD_BOTTOM + SMALL_HEADER);
			room_w = HUD_WIDTH - PAD * 2;
			room_h = SMALL_CANVAS_H;
		}
		else
		{
			int16_t vw = Constants::Constants::get().get_viewwidth();
			int16_t width = std::min<int16_t>(LARGE_W, static_cast<int16_t>(vw / 2));
			// Down to just above the chat button, clear of the stick.
			Rect chat = chat_button();
			int16_t bottom = chat.width() > 0 ? static_cast<int16_t>(chat.top() - 10) : static_cast<int16_t>(vh - 240);

			top = static_cast<int16_t>(HUD_BOTTOM + LARGE_HEADER);
			room_w = static_cast<int16_t>(width - PAD * 2);
			room_h = std::max<int16_t>(60, static_cast<int16_t>(bottom - PAD - top));
		}

		float zoom = std::min({ static_cast<float>(room_w) / map.x(), static_cast<float>(room_h) / map.y(), MAX_ZOOM });
		int16_t w = static_cast<int16_t>(map.x() * zoom);
		int16_t h = static_cast<int16_t>(map.y() * zoom);

		// The large card narrows to a tall map; the map sits centred in it.
		if (type == Type::MAX)
			room_w = static_cast<int16_t>(std::max<int16_t>(LARGE_MIN_W, static_cast<int16_t>(w + PAD * 2)) - PAD * 2);

		return rect(static_cast<int16_t>(left + (room_w - w) / 2), top, w, h);
	}

	Rect UIMiniMap::mobile_frame() const
	{
		if (!has_map || type == Type::MIN)
		{
			std::string name = ellipsize(mobile_name, Font::SMALL_BOLD, PILL_MAX_W - 30);

			return rect(HUD_LEFT, HUD_BOTTOM, static_cast<int16_t>(text_width(name, Font::SMALL_BOLD) + 30), PILL_H);
		}

		Rect canvas = mobile_canvas();

		if (type == Type::NORMAL)
			return rect(HUD_LEFT, HUD_BOTTOM, HUD_WIDTH, static_cast<int16_t>(canvas.bottom() + PAD - HUD_BOTTOM));

		int16_t width = std::max<int16_t>(LARGE_MIN_W, static_cast<int16_t>(canvas.width() + PAD * 2));

		return rect(HUD_LEFT, HUD_BOTTOM, width, static_cast<int16_t>(canvas.bottom() + PAD - HUD_BOTTOM));
	}

	Rect UIMiniMap::mobile_button(int32_t target) const
	{
		Rect f = mobile_frame();
		int16_t y = static_cast<int16_t>(f.top() + 10);
		int16_t right = static_cast<int16_t>(f.right() - PAD);

		switch (target)
		{
			case MobileTargets::SHRINK: return rect(static_cast<int16_t>(right - 30), y, 30, 30);
			case MobileTargets::HIDE: return rect(static_cast<int16_t>(right - 30 - 6 - 56), y, 56, 30);
			case MobileTargets::WORLD: return rect(static_cast<int16_t>(right - 30 - 6 - 56 - 6 - 92), y, 92, 30);
			default: return {};
		}
	}

	Point<int16_t> UIMiniMap::mobile_point(Point<int16_t> world_pos) const
	{
		Rect canvas = mobile_canvas();
		Point<int16_t> map = map_sprite.get_dimensions();

		if (map.x() <= 0 || scale <= 0)
			return {};

		float zoom = static_cast<float>(canvas.width()) / map.x();
		float x = (world_pos.x() + center_offset.x()) / static_cast<float>(scale);
		float y = (world_pos.y() + center_offset.y()) / static_cast<float>(scale);

		return Point<int16_t>(static_cast<int16_t>(canvas.left() + x * zoom), static_cast<int16_t>(canvas.top() + y * zoom));
	}

	int32_t UIMiniMap::mobile_hit(Point<int16_t> pos) const
	{
		Rect f = mobile_frame();

		if (!f.contains(pos))
			return MobileTargets::NONE;

		if (type != Type::MAX)
			return has_map ? MobileTargets::GROW : MobileTargets::NONE;

		for (int32_t t : { MobileTargets::SHRINK, MobileTargets::HIDE, MobileTargets::WORLD })
			if (inset(mobile_button(t), -3, -6).contains(pos))
				return t;

		if (inset(mobile_canvas(), -10, -10).contains(pos))
			return MobileTargets::MAP;

		return MobileTargets::NONE;
	}

	// Names whatever was tapped on the large map: an NPC, a portal's
	// destination or another player.
	void UIMiniMap::mobile_pick(Point<int16_t> pos)
	{
		int32_t best = 16 * 16;
		std::string label;
		Point<int16_t> at;

		auto consider = [&](Point<int16_t> world, const std::string& name)
		{
			Point<int16_t> p = mobile_point(world);
			int32_t dx = p.x() - pos.x();
			int32_t dy = p.y() - pos.y();
			int32_t d = dx * dx + dy * dy;

			if (d < best && !name.empty())
			{
				best = d;
				label = name;
				at = world;
			}
		};

		MapObjects* npcs = Stage::get().get_npcs().get_npcs();

		for (auto it = npcs->begin(); it != npcs->end(); ++it)
		{
			auto* npc = static_cast<Npc*>(it->second.get());

			if (npc && npc->is_active())
			{
				std::string func = npc->get_func();
				consider(npc->get_position(), func.empty() ? npc->get_name() : npc->get_name() + " \xC2\xB7 " + func);
			}
		}

		for (const auto& portal : mobile_portals)
			if (portal.second > 0 && portal.second != 999999999)
				consider(portal.first, "To " + NxHelper::Map::get_map_name(portal.second));

		MapObjects* chars = Stage::get().get_chars().get_chars();

		for (auto it = chars->begin(); it != chars->end(); ++it)
		{
			auto* chr = static_cast<Char*>(it->second.get());

			if (chr && chr->is_active())
				consider(chr->get_position(), chr->get_name());
		}

		picked_label = label;
		picked_world = at;
		picked_ticks = label.empty() ? 0 : LABEL_TICKS;
	}

	Cursor::State UIMiniMap::mobile_cursor(bool clicked, Point<int16_t> pos)
	{
		if (clicked)
		{
			uint32_t serial = UI::get().get_press_serial();

			if (!mobile_tracking || serial != mobile_press)
			{
				mobile_tracking = true;
				mobile_press = serial;
				mobile_target = mobile_hit(pos);
			}

			return Cursor::State::CLICKING;
		}

		if (mobile_tracking)
		{
			int32_t target = mobile_target;
			bool tap = target != MobileTargets::NONE && mobile_hit(pos) == target;

			mobile_tracking = false;
			mobile_target = MobileTargets::NONE;

			if (tap)
			{
				switch (target)
				{
					case MobileTargets::GROW:
						Sound(Sound::Name::BUTTONCLICK).play();
						mobile_set_type(static_cast<int8_t>(type + 1));
						break;
					case MobileTargets::SHRINK:
						Sound(Sound::Name::BUTTONCLICK).play();
						mobile_set_type(Type::NORMAL);
						break;
					case MobileTargets::HIDE:
						Sound(Sound::Name::BUTTONCLICK).play();
						mobile_set_type(Type::MIN);
						break;
					case MobileTargets::WORLD:
						Sound(Sound::Name::BUTTONCLICK).play();
						MobileUI::open_world_map();
						break;
					case MobileTargets::MAP:
						mobile_pick(pos);
						break;
				}
			}
		}

		return mobile_frame().contains(pos) ? Cursor::State::CANCLICK : Cursor::State::IDLE;
	}

	void UIMiniMap::mobile_draw(float alpha) const
	{
		Rect f = mobile_frame();
		int32_t pressed = mobile_tracking ? mobile_target : MobileTargets::NONE;

		// Just the name: tap for the map.
		if (!has_map || type == Type::MIN)
		{
			fill(f, BACKDROP.alpha(pressed == MobileTargets::GROW ? 1.4f : 1.0f), static_cast<int16_t>(PILL_H / 2));
			dot(Point<int16_t>(static_cast<int16_t>(f.left() + 13), static_cast<int16_t>(f.top() + PILL_H / 2)), 4, Theme::ACCENT);
			shadowed(ellipsize(mobile_name, Font::SMALL_BOLD, PILL_MAX_W - 30), Font::SMALL_BOLD, Theme::TEXT,
				rect(static_cast<int16_t>(f.left() + 22), f.top(), static_cast<int16_t>(f.width() - 22), f.height()), Text::Alignment::LEFT, 0);
			return;
		}

		bool large = type == Type::MAX;
		fill(f, BACKDROP.alpha(pressed == MobileTargets::GROW ? 1.4f : 1.0f), 12);

		if (large)
		{
			int16_t text_w = static_cast<int16_t>(mobile_button(MobileTargets::WORLD).left() - f.left() - PAD * 2);
			shadowed(ellipsize(mobile_name, Font::BODY_BOLD, text_w), Font::BODY_BOLD, Theme::TEXT,
				rect(static_cast<int16_t>(f.left() + PAD + 2), static_cast<int16_t>(f.top() + 6), text_w, 20), Text::Alignment::LEFT, 0);
			shadowed(ellipsize(mobile_street, Font::SMALL, text_w), Font::SMALL, Theme::MUTED,
				rect(static_cast<int16_t>(f.left() + PAD + 2), static_cast<int16_t>(f.top() + 25), text_w, 16), Text::Alignment::LEFT, 0);

			button(mobile_button(MobileTargets::WORLD), "World Map", Style::SECONDARY, true, pressed == MobileTargets::WORLD, Font::SMALL_BOLD);
			button(mobile_button(MobileTargets::HIDE), "Hide", Style::SECONDARY, true, pressed == MobileTargets::HIDE, Font::SMALL_BOLD);
			icon_button(mobile_button(MobileTargets::SHRINK), Glyph::MINUS, Style::SECONDARY, true, pressed == MobileTargets::SHRINK);
		}
		else
		{
			shadowed(ellipsize(mobile_name, Font::SMALL_BOLD, HUD_WIDTH - PAD * 2), Font::SMALL_BOLD, Theme::TEXT,
				rect(static_cast<int16_t>(f.left() + PAD), f.top(), static_cast<int16_t>(f.width() - PAD * 2), SMALL_HEADER), Text::Alignment::LEFT, 0);
		}

		// The map itself, a little see-through.
		Rect canvas = mobile_canvas();

		if (!canvas_hd.empty())
			GraphicsGL::get().drawraw(canvas_hd_id, canvas_hd_w, canvas_hd_h, canvas_hd.data(), canvas,
				Range<int16_t>(0, 0), Color(1.0f, 1.0f, 1.0f, 0.88f), 0.0f);

		int16_t r = large ? 3 : 2;

		for (const auto& portal : mobile_portals)
			dot(mobile_point(portal.first), r, PORTAL);

		MapObjects* npcs = Stage::get().get_npcs().get_npcs();

		for (auto it = npcs->begin(); it != npcs->end(); ++it)
		{
			auto* npc = static_cast<Npc*>(it->second.get());

			if (!npc || !npc->is_active())
				continue;

			Point<int16_t> p = mobile_point(npc->get_position());
			dot(p, r, Theme::GREEN);

			// Quest "Find NPC": a gold ring that breathes.
			if (quest_npc_id > 0 && npc->get_npcid() == quest_npc_id)
			{
				float pulse = 0.5f + 0.5f * static_cast<float>(std::fabs(std::sin(mobile_tick * 0.12)));
				fill(rect(static_cast<int16_t>(p.x() - r - 4), static_cast<int16_t>(p.y() - r - 4), static_cast<int16_t>(r * 2 + 8), static_cast<int16_t>(r * 2 + 8)),
					Theme::GOLD.alpha(0.45f * pulse), static_cast<int16_t>(r + 4));
			}
		}

		MapObjects* mobs = Stage::get().get_mobs().get_mobs();

		for (auto it = mobs->begin(); it != mobs->end(); ++it)
		{
			auto* mob = static_cast<Mob*>(it->second.get());

			if (mob && mob->is_active())
				dot(mobile_point(mob->get_position()), static_cast<int16_t>(large ? 2 : 1), Theme::RED);
		}

		MapObjects* chars = Stage::get().get_chars().get_chars();

		for (auto it = chars->begin(); it != chars->end(); ++it)
		{
			auto* chr = static_cast<Char*>(it->second.get());

			if (chr && chr->is_active())
				dot(mobile_point(chr->get_position()), r, OTHERS);
		}

		dot(mobile_point(Stage::get().get_player().get_position()), static_cast<int16_t>(r + 1), Theme::GOLD);

		// What the last tap named.
		if (large && picked_ticks > 0 && !picked_label.empty())
		{
			Point<int16_t> p = mobile_point(picked_world);
			int16_t w = static_cast<int16_t>(std::min<int16_t>(static_cast<int16_t>(text_width(picked_label, Font::SMALL_BOLD) + 16), static_cast<int16_t>(f.width() - PAD * 2)));
			int16_t x = std::clamp<int16_t>(static_cast<int16_t>(p.x() - w / 2), static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.right() - PAD - w));
			int16_t y = std::max<int16_t>(static_cast<int16_t>(canvas.top()), static_cast<int16_t>(p.y() - 34));
			float fade = std::min(1.0f, picked_ticks / 30.0f);
			Rect chip = rect(x, y, w, 22);

			fill(chip, RGBA{ 0.05f, 0.06f, 0.09f, 0.92f * fade }, 11);
			text_in(ellipsize(picked_label, Font::SMALL_BOLD, static_cast<int16_t>(w - 16)), Font::SMALL_BOLD, Theme::TEXT.alpha(fade), chip);
		}

		(void)alpha;
	}
}
