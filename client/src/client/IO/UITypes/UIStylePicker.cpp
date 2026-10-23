//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIStylePicker.h"

#include "UINpcTalk.h"

#include "../Components/MobileNoticeCard.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/NpcInteractionPackets.h"

#include <nlnx/nx.hpp>

#include <algorithm>
#include <cctype>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t CARD_W = 640;
		constexpr int16_t PAD = 22;
		constexpr int16_t PREVIEW_W = 232;
		constexpr int16_t PREVIEW_H = 262;
		constexpr int16_t ARROW_H = 46;
		constexpr int16_t BUTTON_H = 48;
		constexpr int16_t DOT = 34;
		constexpr int16_t DOT_GAP = 6;
		constexpr size_t MAX_DOTS = 24;

		int16_t view_w()
		{
			return Constants::Constants::get().get_viewwidth();
		}

		int16_t view_h()
		{
			return Constants::Constants::get().get_viewheight();
		}

		// NPC text as plain sentences: names filled in, icons and colour
		// codes dropped.
		std::string plain(const std::string& raw, int32_t npcid)
		{
			std::string text = UINpcTalk::format_text(raw, npcid);
			std::string out;

			for (size_t i = 0; i < text.size(); i++)
			{
				if (text[i] == '#' && i + 2 < text.size() && std::isalpha(static_cast<unsigned char>(text[i + 1])))
				{
					size_t end = text.find('#', i + 2);

					if (end != std::string::npos)
					{
						i = end;

						// An icon followed by its name: keep one space.
						if (!out.empty() && out.back() == ' ' && i + 1 < text.size() && text[i + 1] == ' ')
							i++;

						continue;
					}
				}

				out += text[i];
			}

			return MobileNoticeCard::clean(out);
		}

		std::string upper(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

			return s;
		}
	}

	UIStylePicker::UIStylePicker(int32_t npcid, const std::string& raw, std::vector<int32_t> s) :
		UIElement(Point<int16_t>(0, 0), Point<int16_t>(view_w(), view_h())), styles(std::move(s))
	{
		npc_name = (std::string)nl::nx::string["Npc.img"][std::to_string(npcid)]["name"];
		message = plain(raw, npcid);

		bool skins = !styles.empty() && std::all_of(styles.begin(), styles.end(), [](int32_t id) { return id < 100; });
		bool hair = !styles.empty() && styles.front() >= 30000;

		auto same = [&](auto key)
		{
			return std::all_of(styles.begin(), styles.end(), [&](int32_t id) { return key(id) == key(styles.front()); });
		};

		if (skins)
			kind = Kind::SKIN;
		else if (hair)
			kind = styles.size() > 1 && same([](int32_t id) { return id / 10; }) ? Kind::HAIR_COLOR : Kind::HAIR;
		else
			kind = styles.size() > 1 && same([](int32_t id) { return id % 100 + id / 1000 * 1000; }) ? Kind::LENS : Kind::FACE;

		look = Stage::get().get_player().get_look();
		look.set_stance(Stance::Id::STAND1);

		// Open on the player's own style when it is offered.
		const CharLook& own = Stage::get().get_player().get_look();
		int32_t mine = kind == Kind::SKIN ? own.get_skin_id()
			: kind == Kind::HAIR || kind == Kind::HAIR_COLOR ? own.get_hair_id() : own.get_face_id();
		auto at = std::find(styles.begin(), styles.end(), mine);

		show(at != styles.end() ? static_cast<size_t>(at - styles.begin()) : 0);

		Sound(Sound::Name::MENUUP).play();
	}

	void UIStylePicker::show(size_t index)
	{
		if (styles.empty())
			return;

		current = index % styles.size();
		int32_t id = styles[current];

		switch (kind)
		{
		case Kind::SKIN:
			look.set_body(id);
			break;
		case Kind::HAIR:
		case Kind::HAIR_COLOR:
			look.set_hair(id);
			break;
		default:
			look.set_face(id);
			break;
		}
	}

	std::string UIStylePicker::style_name(size_t index) const
	{
		if (styles.empty())
			return "Nothing to choose";

		int32_t id = styles[index];

		if (kind == Kind::SKIN)
		{
			static const char* tones[] = { "Light", "Tanned", "Dark", "Pale", "Ashen", "Green" };

			return id >= 0 && id < 6 ? tones[id] : "Tone " + std::to_string(id);
		}

		std::string name = (std::string)nl::nx::string["Eqp.img"]["Eqp"][kind == Kind::HAIR || kind == Kind::HAIR_COLOR ? "Hair" : "Face"][std::to_string(id)]["name"];

		return name.empty() ? "Style " + std::to_string(index + 1) : name;
	}

	Rect UIStylePicker::card() const
	{
		int16_t w = std::min<int16_t>(CARD_W, static_cast<int16_t>(dimension.x() - 40));
		int16_t h = static_cast<int16_t>(PAD + PREVIEW_H + 12 + ARROW_H + PAD);

		return rect(static_cast<int16_t>((dimension.x() - w) / 2), static_cast<int16_t>(std::max(12, (dimension.y() - h) / 2 - 8)), w, h);
	}

	Rect UIStylePicker::preview_rect() const
	{
		Rect c = card();

		return rect(static_cast<int16_t>(c.left() + PAD), static_cast<int16_t>(c.top() + PAD), PREVIEW_W, PREVIEW_H);
	}

	Rect UIStylePicker::prev_rect() const
	{
		Rect p = preview_rect();

		return rect(p.left(), static_cast<int16_t>(p.bottom() + 12), ARROW_H + 10, ARROW_H);
	}

	Rect UIStylePicker::next_rect() const
	{
		Rect p = preview_rect();

		return rect(static_cast<int16_t>(p.right() - ARROW_H - 10), static_cast<int16_t>(p.bottom() + 12), ARROW_H + 10, ARROW_H);
	}

	Rect UIStylePicker::cancel_rect() const
	{
		Rect c = card();
		int16_t left = static_cast<int16_t>(preview_rect().right() + 20);
		int16_t w = static_cast<int16_t>((c.right() - PAD - left - 12) / 2);

		return rect(left, static_cast<int16_t>(c.bottom() - PAD - BUTTON_H), w, BUTTON_H);
	}

	Rect UIStylePicker::choose_rect() const
	{
		Rect r = cancel_rect();

		return rect(static_cast<int16_t>(r.right() + 12), r.top(), r.width(), BUTTON_H);
	}

	bool UIStylePicker::shows_dots() const
	{
		return styles.size() > 1 && styles.size() <= MAX_DOTS;
	}

	Rect UIStylePicker::dot_rect(size_t index) const
	{
		Rect c = card();
		int16_t left = static_cast<int16_t>(preview_rect().right() + 20);
		int16_t width = static_cast<int16_t>(c.right() - PAD - left);
		int16_t per_row = static_cast<int16_t>(std::max(1, (width + DOT_GAP) / (DOT + DOT_GAP)));
		int16_t rows = static_cast<int16_t>((styles.size() + per_row - 1) / per_row);
		int16_t bottom = static_cast<int16_t>(cancel_rect().top() - 16);
		int16_t top = static_cast<int16_t>(bottom - rows * DOT - (rows - 1) * DOT_GAP);
		int16_t row = static_cast<int16_t>(index / per_row);
		int16_t col = static_cast<int16_t>(index % per_row);

		return rect(static_cast<int16_t>(left + col * (DOT + DOT_GAP)), static_cast<int16_t>(top + row * (DOT + DOT_GAP)), DOT, DOT);
	}

	int32_t UIStylePicker::hit(Point<int16_t> pos) const
	{
		if (cancel_rect().contains(pos))
			return CANCEL;

		if (!styles.empty())
		{
			if (choose_rect().contains(pos))
				return CHOOSE;

			if (styles.size() > 1)
			{
				if (inset(prev_rect(), -6, -6).contains(pos))
					return PREV;

				if (inset(next_rect(), -6, -6).contains(pos))
					return NEXT;

				// Tapping the character steps forward too.
				if (preview_rect().contains(pos))
					return NEXT;
			}

			if (shows_dots())
				for (size_t i = 0; i < styles.size(); i++)
					if (dot_rect(i).contains(pos))
						return static_cast<int32_t>(FIRST_DOT + i);
		}

		return NONE;
	}

	void UIStylePicker::answer(bool chosen)
	{
		if (answered)
			return;

		answered = true;
		Sound(Sound::Name::BUTTONCLICK).play();

		if (chosen && !styles.empty())
			NpcTalkStylePacket(static_cast<int32_t>(current)).dispatch();
		else
			NpcTalkMorePacket(7, 0).dispatch();

		deactivate();
	}

	void UIStylePicker::draw(float inter) const
	{
		GraphicsGL::get().drawscreenfill(0.0f, 0.0f, 0.0f, 0.55f);

		Rect c = card();
		fill(inset(c, -1, -1), Theme::ACCENT.alpha(0.30f), 23);
		fill(c, Theme::PANEL, 22);

		// The character in the style shown, standing on a floor shadow.
		Rect p = preview_rect();
		fill(p, Theme::CARD, 16);

		int16_t cx = static_cast<int16_t>(p.left() + p.width() / 2);
		int16_t floor = static_cast<int16_t>(p.bottom() - 22);
		fill(rect(static_cast<int16_t>(cx - 60), static_cast<int16_t>(floor - 6), 120, 12), RGBA{ 0.0f, 0.0f, 0.0f, 0.30f }, 6);

		GraphicsGL::get().begin_clip(p.top(), p.bottom());
		look.draw(DrawArgument(Point<int16_t>(cx, floor), 3.0f, 3.0f), inter);
		GraphicsGL::get().end_clip();

		bool many = styles.size() > 1;
		icon_button(prev_rect(), Glyph::LEFT, Style::SECONDARY, many, pressed == PREV);
		icon_button(next_rect(), Glyph::RIGHT, Style::SECONDARY, many, pressed == NEXT);

		std::string count = styles.empty() ? "0 / 0" : std::to_string(current + 1) + " / " + std::to_string(styles.size());
		text_in(count, Font::BODY_BOLD, Theme::MUTED, rect(prev_rect().right(), prev_rect().top(),
			static_cast<int16_t>(next_rect().left() - prev_rect().right()), ARROW_H));

		// What it is, what it's called and what the NPC says.
		int16_t x = static_cast<int16_t>(p.right() + 20);
		int16_t w = static_cast<int16_t>(c.right() - PAD - x);
		int16_t y = p.top();

		static const char* eyebrows[] = { "NEW HAIRSTYLE", "HAIR COLOR", "NEW FACE", "LENS COLOR", "SKIN CARE" };
		std::string eyebrow = eyebrows[static_cast<int>(kind)];

		if (!npc_name.empty())
			eyebrow += "  \xC2\xB7  " + upper(npc_name);

		text(ellipsize(eyebrow, Font::SMALL_BOLD, w), Font::SMALL_BOLD, Theme::ACCENT, Point<int16_t>(x, y));
		y += 20;

		text(ellipsize(style_name(current), Font::HEADLINE, w), Font::HEADLINE, Theme::TEXT, Point<int16_t>(x, y));
		y += 34;

		int16_t bottom = shows_dots() ? static_cast<int16_t>(dot_rect(0).top() - 12) : static_cast<int16_t>(cancel_rect().top() - 12);

		if (bottom > y)
		{
			GraphicsGL::get().begin_clip(y, bottom);
			text(message, Font::BODY, Theme::MUTED, Point<int16_t>(x, y), Text::Alignment::LEFT, w);
			GraphicsGL::get().end_clip();
		}

		if (shows_dots())
		{
			for (size_t i = 0; i < styles.size(); i++)
			{
				Rect d = dot_rect(i);
				bool on = i == current;
				fill(d, on ? Theme::ACCENT : pressed == static_cast<int32_t>(FIRST_DOT + i) ? Theme::CARD_HI : Theme::CARD, 10);
				text_in(std::to_string(i + 1), Font::SMALL_BOLD, on ? RGBA{ 0.13f, 0.08f, 0.03f, 1.0f } : Theme::MUTED, d);
			}
		}

		button(cancel_rect(), "Cancel", Style::SECONDARY, true, pressed == CANCEL);
		button(choose_rect(), kind == Kind::SKIN ? "Choose tone" : "Choose style", Style::PRIMARY, !styles.empty(), pressed == CHOOSE);
	}

	void UIStylePicker::update()
	{
		UIElement::update();
		look.update(Constants::TIMESTEP);

		if (pressed != NONE && !UI::get().is_mouse_held())
			pressed = NONE;
	}

	void UIStylePicker::update_screen(int16_t new_width, int16_t new_height)
	{
		dimension = Point<int16_t>(new_width, new_height);
	}

	bool UIStylePicker::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	Cursor::State UIStylePicker::send_cursor(bool down, Point<int16_t> pos)
	{
		if (down)
		{
			uint32_t serial = UI::get().get_press_serial();

			if (serial != press)
			{
				press = serial;
				pressed = hit(pos);

				// A tap on the dimmed screen leaves the salon.
				if (pressed == NONE && !card().contains(pos))
					answer(false);
			}

			return Cursor::State::CLICKING;
		}

		int32_t target = pressed;
		pressed = NONE;

		if (target == NONE || hit(pos) != target)
			return Cursor::State::IDLE;

		switch (target)
		{
		case CANCEL:
			answer(false);
			break;
		case CHOOSE:
			answer(true);
			break;
		case PREV:
			Sound(Sound::Name::BUTTONCLICK).play();
			show(current + styles.size() - 1);
			break;
		case NEXT:
			Sound(Sound::Name::BUTTONCLICK).play();
			show(current + 1);
			break;
		default:
			Sound(Sound::Name::BUTTONCLICK).play();
			show(static_cast<size_t>(target - FIRST_DOT));
			break;
		}

		return Cursor::State::IDLE;
	}

	void UIStylePicker::send_key(int32_t keycode, bool pressed_key, bool escape)
	{
		if (!pressed_key)
			return;

		if (escape)
			answer(false);
		else if (keycode == KeyAction::Id::RETURN)
			answer(true);
		else if (keycode == KeyAction::Id::LEFT && styles.size() > 1)
			show(current + styles.size() - 1);
		else if (keycode == KeyAction::Id::RIGHT && styles.size() > 1)
			show(current + 1);
	}

	UIElement::Type UIStylePicker::get_type() const
	{
		return TYPE;
	}
}
