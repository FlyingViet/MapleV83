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
#include "MobileNoticeCard.h"

#include "../UI.h"

#include "../../Constants.h"
#include "../../Graphics/GraphicsGL.h"

#include <algorithm>
#include <cctype>
#include <chrono>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t DIALOG_W = 480;
		constexpr int16_t DIALOG_PAD = 24;
		constexpr int16_t BADGE = 34;
		constexpr int16_t ROW_H = 50;
		constexpr int16_t GAP = 10;

		constexpr int16_t BANNER_W = 560;
		constexpr int16_t BANNER_PAD = 14;
		constexpr int16_t BANNER_BADGE = 32;
		constexpr int16_t BANNER_TOP = 36;
		constexpr int16_t BANNER_BUTTON_W = 96;
		constexpr int16_t BANNER_BUTTON_H = 40;

		constexpr int16_t STEP_W = 50;
		constexpr int16_t MAX_W = 76;

		int16_t view_w()
		{
			return Constants::Constants::get().get_viewwidth();
		}

		int16_t view_h()
		{
			return Constants::Constants::get().get_viewheight();
		}

		std::string lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			return s;
		}

		bool has(const std::string& haystack, const char* needle)
		{
			return haystack.find(needle) != std::string::npos;
		}
	}

	std::string MobileNoticeCard::clean(const std::string& message)
	{
		std::string out;
		out.reserve(message.size());

		for (size_t i = 0; i < message.size(); i++)
		{
			char c = message[i];

			if (c == '\r')
			{
				if (i + 1 < message.size() && message[i + 1] == '\n')
					continue;

				out += '\n';
				continue;
			}

			// Escaped line breaks ("\\n" in the text itself), as v83's notice
			// layout reads them.
			if (c == '\\' && i + 1 < message.size() && (message[i + 1] == 'n' || message[i + 1] == 'r'))
			{
				if (message[i + 1] == 'n')
					out += '\n';

				i++;
				continue;
			}

			// v83 colour codes: #b blue, #k black, #r red, #e bold, #n normal,
			// #d purple, #g green.
			if (c == '#' && i + 1 < message.size() && std::string("bkrendg").find(message[i + 1]) != std::string::npos)
			{
				i++;
				continue;
			}

			out += c;
		}

		while (!out.empty() && (out.back() == '\n' || out.back() == ' '))
			out.pop_back();

		return out;
	}

	MobileNoticeCard::Kind MobileNoticeCard::invite_kind(const std::string& message, std::string& eyebrow)
	{
		std::string m = lower(message);

		if (has(m, "trade"))
		{
			eyebrow = "TRADE REQUEST";
			return Kind::TRADE;
		}

		if (has(m, "alliance"))
		{
			eyebrow = "ALLIANCE INVITE";
			return Kind::GUILD;
		}

		if (has(m, "guild"))
		{
			eyebrow = "GUILD INVITE";
			return Kind::GUILD;
		}

		if (has(m, "party"))
		{
			eyebrow = "PARTY INVITE";
			return Kind::PARTY;
		}

		if (has(m, "buddy"))
		{
			eyebrow = "BUDDY REQUEST";
			return Kind::BUDDY;
		}

		if (has(m, "messenger"))
		{
			eyebrow = "MESSENGER INVITE";
			return Kind::MESSENGER;
		}

		if (has(m, "family"))
		{
			eyebrow = has(m, "summon") ? "FAMILY SUMMON" : "FAMILY REQUEST";
			return Kind::FAMILY;
		}

		eyebrow = "INVITE";
		return Kind::INFO;
	}

	MobileNoticeCard::Kind MobileNoticeCard::title_kind(const std::string& title)
	{
		std::string t = lower(title);

		if (has(t, "quest"))
			return Kind::QUEST;

		if (has(t, "buddy"))
			return Kind::BUDDY;

		if (has(t, "family"))
			return Kind::FAMILY;

		if (has(t, "party"))
			return Kind::PARTY;

		if (has(t, "guild") || has(t, "alliance"))
			return Kind::GUILD;

		if (has(t, "trade"))
			return Kind::TRADE;

		if (has(t, "messenger"))
			return Kind::MESSENGER;

		return Kind::INFO;
	}

	RGBA MobileNoticeCard::tint(Kind k)
	{
		switch (k)
		{
		case Kind::QUESTION:
		case Kind::TRADE:
		case Kind::QUEST:
			return Theme::GOLD;
		case Kind::AMOUNT:
		case Kind::TEXT:
			return Theme::ACCENT;
		case Kind::WAIT:
			return Theme::MUTED;
		case Kind::DEATH:
			return Theme::RED;
		case Kind::PARTY:
		case Kind::FAMILY:
			return Theme::GREEN;
		case Kind::BUDDY:
			return { 1.0f, 0.55f, 0.75f, 1.0f };
		case Kind::GUILD:
			return Theme::PURPLE;
		default:
			return Theme::BLUE;
		}
	}

	const char* MobileNoticeCard::mark(Kind k)
	{
		switch (k)
		{
		case Kind::QUESTION: return "?";
		case Kind::AMOUNT: return "12";
		case Kind::TEXT: return "Aa";
		case Kind::WAIT: return "...";
		case Kind::DEATH: return "!";
		case Kind::PARTY: return "P";
		case Kind::TRADE: return "$";
		case Kind::BUDDY: return "B";
		case Kind::GUILD: return "G";
		case Kind::MESSENGER: return "M";
		case Kind::FAMILY: return "F";
		case Kind::QUEST: return "Q";
		default: return "i";
		}
	}

	void MobileNoticeCard::badge(const Rect& r, Kind k)
	{
		RGBA c = tint(k);
		fill(r, c.alpha(0.20f), static_cast<int16_t>(r.width() / 2));

		std::string m = mark(k);

		// Waiting: the dots count up.
		if (k == Kind::WAIT)
		{
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
			m = std::string(1 + (ms / 350) % 3, '.');
		}

		text_in(m, Font::TITLE, c, r);
	}

	void MobileNoticeCard::setup(Layout l, Kind k, const std::string& e, const std::string& message)
	{
		layout = l;
		kind = k;
		eyebrow = e;
		paragraphs.clear();
		actions.clear();
		field = false;
		stepper = false;

		std::string text = clean(message);
		size_t start = 0;

		while (true)
		{
			size_t nl = text.find('\n', start);
			paragraphs.push_back(text.substr(start, nl == std::string::npos ? std::string::npos : nl - start));

			if (nl == std::string::npos)
				break;

			start = nl + 1;
		}

		relayout();
	}

	void MobileNoticeCard::add(int32_t id, const std::string& label, Style style)
	{
		actions.push_back({ id, label, style });
		relayout();
	}

	void MobileNoticeCard::add_field(bool with_stepper)
	{
		field = true;
		stepper = with_stepper;
		relayout();
	}

	void MobileNoticeCard::set_progress(float fraction)
	{
		progress = fraction;
	}

	void MobileNoticeCard::set_top(int16_t t)
	{
		top_override = t;
	}

	void MobileNoticeCard::relayout()
	{
		auto measure = [&](Font f)
		{
			int16_t h = 0;

			for (size_t i = 0; i < paragraphs.size(); i++)
			{
				if (i > 0)
					h += 6;

				h += paragraphs[i].empty() ? static_cast<int16_t>(line_height(f) / 2) : text_height(paragraphs[i], f, text_w);
			}

			return h;
		};

		if (layout == Layout::DIALOG)
		{
			width = std::min<int16_t>(DIALOG_W, static_cast<int16_t>(view_w() - 48));
			text_w = static_cast<int16_t>(width - DIALOG_PAD * 2);

			// Short messages read large; long ones drop to body text.
			font = Font::TITLE;
			message_h = measure(font);

			if (message_h > line_height(Font::TITLE) * 5)
			{
				font = Font::BODY;
				message_h = measure(font);
			}

			height = static_cast<int16_t>(DIALOG_PAD + BADGE + 16 + message_h);

			if (field)
				height += 18 + ROW_H;

			if (!actions.empty())
				height += 24 + ROW_H;

			height += DIALOG_PAD;
		}
		else
		{
			width = std::min<int16_t>(BANNER_W, static_cast<int16_t>(std::max(360, view_w() - 480)));

			int16_t buttons_w = static_cast<int16_t>(actions.size() * BANNER_BUTTON_W + (actions.empty() ? 0 : (actions.size() - 1) * 8 + GAP));
			text_w = static_cast<int16_t>(width - BANNER_PAD * 2 - BANNER_BADGE - GAP - buttons_w);
			font = Font::BODY_BOLD;
			message_h = measure(font);

			height = static_cast<int16_t>(std::max<int>(BANNER_PAD + 17 + message_h + BANNER_PAD, BANNER_PAD * 2 + BANNER_BUTTON_H));
		}
	}

	Rect MobileNoticeCard::card() const
	{
		int16_t x = static_cast<int16_t>((view_w() - width) / 2);

		if (layout == Layout::BANNER)
			return rect(x, top_override >= 0 ? top_override : BANNER_TOP, width, height);

		// A field moves the card up, clear of the keyboard.
		int16_t y = field ? 36 : static_cast<int16_t>((view_h() - height) / 2 - 12);

		return rect(x, std::max<int16_t>(12, y), width, height);
	}

	Rect MobileNoticeCard::field_rect() const
	{
		if (!field)
			return {};

		Rect c = card();
		int16_t y = static_cast<int16_t>(c.top() + DIALOG_PAD + BADGE + 16 + message_h + 18);
		int16_t left = static_cast<int16_t>(c.left() + DIALOG_PAD);
		int16_t w = static_cast<int16_t>(width - DIALOG_PAD * 2);

		if (stepper)
		{
			left = static_cast<int16_t>(left + STEP_W + 8);
			w = static_cast<int16_t>(w - (STEP_W + 8) * 2 - MAX_W - 8);
		}

		return rect(left, y, w, ROW_H);
	}

	Rect MobileNoticeCard::step_rect(int32_t id) const
	{
		Rect f = field_rect();

		switch (id)
		{
		case STEP_DOWN:
			return rect(static_cast<int16_t>(f.left() - 8 - STEP_W), f.top(), STEP_W, ROW_H);
		case STEP_UP:
			return rect(static_cast<int16_t>(f.right() + 8), f.top(), STEP_W, ROW_H);
		default:
			return rect(static_cast<int16_t>(f.right() + 8 + STEP_W + 8), f.top(), MAX_W, ROW_H);
		}
	}

	Rect MobileNoticeCard::action_rect(size_t index) const
	{
		Rect c = card();
		size_t n = std::max<size_t>(1, actions.size());

		if (layout == Layout::BANNER)
		{
			int16_t right = static_cast<int16_t>(c.right() - BANNER_PAD);
			int16_t x = static_cast<int16_t>(right - (n - index) * BANNER_BUTTON_W - (n - index - 1) * 8);
			int16_t y = static_cast<int16_t>(c.top() + (c.height() - BANNER_BUTTON_H) / 2);

			return rect(x, y, BANNER_BUTTON_W, BANNER_BUTTON_H);
		}

		int16_t gap = 12;
		int16_t w = static_cast<int16_t>((width - DIALOG_PAD * 2 - gap * (n - 1)) / n);

		return rect(static_cast<int16_t>(c.left() + DIALOG_PAD + index * (w + gap)),
			static_cast<int16_t>(c.bottom() - DIALOG_PAD - ROW_H), w, ROW_H);
	}

	bool MobileNoticeCard::contains(Point<int16_t> pos) const
	{
		return card().contains(pos);
	}

	int32_t MobileNoticeCard::hit(Point<int16_t> pos) const
	{
		for (size_t i = 0; i < actions.size(); i++)
			if (action_rect(i).contains(pos))
				return actions[i].id;

		if (field)
		{
			if (stepper)
				for (int32_t id : { STEP_DOWN, STEP_UP, STEP_MAX })
					if (step_rect(id).contains(pos))
						return id;

			if (field_rect().contains(pos))
				return FIELD;
		}

		return NONE;
	}

	MobileNoticeCard::Touch MobileNoticeCard::touch(bool down, Point<int16_t> pos)
	{
		if (down)
		{
			uint32_t serial = UI::get().get_press_serial();

			// The same press, still held (moves arrive as presses too).
			if (serial == press)
				return {};

			press = serial;
			int32_t id = hit(pos);

			if (id == FIELD)
			{
				pressed = NONE;
				return { Tap::FIELD, FIELD };
			}

			if (id == NONE && !contains(pos))
			{
				pressed = NONE;
				return { Tap::OUTSIDE, NONE };
			}

			pressed = id;
			return {};
		}

		int32_t id = pressed;
		pressed = NONE;

		if (id != NONE && hit(pos) == id)
			return { Tap::ACTION, id };

		return {};
	}

	void MobileNoticeCard::update()
	{
		if (pressed != NONE && !UI::get().is_mouse_held())
			pressed = NONE;
	}

	void MobileNoticeCard::draw(bool field_focused) const
	{
		Rect c = card();
		RGBA accent = tint(kind);

		if (layout == Layout::DIALOG)
		{
			GraphicsGL::get().drawscreenfill(0.0f, 0.0f, 0.0f, 0.55f);

			fill(inset(c, -1, -1), accent.alpha(0.35f), 23);
			fill(c, Theme::PANEL, 22);

			Rect b = rect(static_cast<int16_t>(c.left() + DIALOG_PAD), static_cast<int16_t>(c.top() + DIALOG_PAD), BADGE, BADGE);
			badge(b, kind);
			text_in(eyebrow, Font::SMALL_BOLD, accent, rect(static_cast<int16_t>(b.right() + 12), b.top(), static_cast<int16_t>(width - BADGE - 12 - DIALOG_PAD * 2), BADGE),
				Text::Alignment::LEFT);

			int16_t y = static_cast<int16_t>(b.bottom() + 16);

			for (size_t i = 0; i < paragraphs.size(); i++)
			{
				if (i > 0)
					y += 6;

				const std::string& p = paragraphs[i];

				if (p.empty())
				{
					y += static_cast<int16_t>(line_height(font) / 2);
					continue;
				}

				text(p, font, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(c.left() + DIALOG_PAD), y), Text::Alignment::LEFT, text_w);
				y += text_height(p, font, text_w);
			}

			if (field)
			{
				Rect f = field_rect();
				fill(f, field_focused ? Theme::CARD_HI : Theme::CARD, 14);

				if (field_focused)
					fill(rect(static_cast<int16_t>(f.left() + 14), static_cast<int16_t>(f.bottom() - 4), static_cast<int16_t>(f.width() - 28), 2), Theme::ACCENT, 1);

				if (stepper)
				{
					icon_button(step_rect(STEP_DOWN), Glyph::MINUS, Style::SECONDARY, true, pressed == STEP_DOWN);
					icon_button(step_rect(STEP_UP), Glyph::PLUS, Style::SECONDARY, true, pressed == STEP_UP);
					button(step_rect(STEP_MAX), "Max", Style::SECONDARY, true, pressed == STEP_MAX);
				}
			}
		}
		else
		{
			fill(inset(c, -1, -1), accent.alpha(0.55f), 17);
			fill(c, Theme::PANEL, 16);

			Rect b = rect(static_cast<int16_t>(c.left() + BANNER_PAD), static_cast<int16_t>(c.top() + (c.height() - BANNER_BADGE) / 2), BANNER_BADGE, BANNER_BADGE);
			badge(b, kind);

			int16_t x = static_cast<int16_t>(b.right() + GAP);
			int16_t y = static_cast<int16_t>(c.top() + (c.height() - 17 - message_h) / 2);
			text(eyebrow, Font::SMALL_BOLD, accent, Point<int16_t>(x, y));
			y += 17;

			for (size_t i = 0; i < paragraphs.size(); i++)
			{
				if (i > 0)
					y += 6;

				const std::string& p = paragraphs[i];

				if (p.empty())
				{
					y += static_cast<int16_t>(line_height(font) / 2);
					continue;
				}

				text(p, font, Theme::TEXT, Point<int16_t>(x, y), Text::Alignment::LEFT, text_w);
				y += text_height(p, font, text_w);
			}

			if (progress >= 0.0f)
			{
				int16_t track = static_cast<int16_t>(c.width() - 32);
				int16_t w = static_cast<int16_t>(std::max(0.0f, std::min(1.0f, progress)) * track);
				fill(rect(static_cast<int16_t>(c.left() + 16), static_cast<int16_t>(c.bottom() - 5), track, 3), Theme::TRACK, 1);

				if (w > 0)
					fill(rect(static_cast<int16_t>(c.left() + 16), static_cast<int16_t>(c.bottom() - 5), w, 3), accent, 1);
			}
		}

		for (size_t i = 0; i < actions.size(); i++)
			button(action_rect(i), actions[i].label, actions[i].style, true, pressed == actions[i].id,
				layout == Layout::BANNER ? Font::SMALL_BOLD : Font::BODY_BOLD);
	}
}
