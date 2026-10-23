//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
//
// The phone's chat window: a see-through card on the left of the screen with
// the chat log, the chat target and an input line. The touch controls' chat
// button opens and closes it (MobileUI::toggle_chat).
#include "UIChatBar.h"
#include "UIMiniMap.h"

#include "../MobileUI.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Graphics/GraphicsGL.h"

#include <algorithm>
#include <cstdlib>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 400;
		constexpr int16_t PAD = 10;
		constexpr int16_t HEADER_H = 42;
		constexpr int16_t INPUT_H = 36;
		constexpr int16_t SEND_W = 64;
		constexpr int16_t GAP = 8;
		constexpr int16_t MIN_H = 200;
		constexpr int16_t LINE_GAP = 4;
		constexpr int16_t TEXT_INSET = 4;
		constexpr size_t MAX_LINES = 200;

		constexpr RGBA BACKDROP = { 0.02f, 0.03f, 0.06f, 0.58f };

		enum Target : int32_t
		{
			CLOSE,
			CHAT_TARGET,
			INPUT,
			SEND,
			LOG
		};

		// Legible on the dark see-through card, in v83's colour per kind.
		RGBA line_color(UIChatBar::LineType type)
		{
			switch (type)
			{
				case UIChatBar::RED: return { 1.00f, 0.47f, 0.47f, 1.0f };
				case UIChatBar::BLUE:
				case UIChatBar::MEGAPHONE: return { 0.58f, 0.75f, 1.00f, 1.0f };
				case UIChatBar::YELLOW:
				case UIChatBar::TRIPLE_MEGAPHONE: return { 1.00f, 0.86f, 0.38f, 1.0f };
				case UIChatBar::PINK:
				case UIChatBar::SUPER_MEGAPHONE: return { 1.00f, 0.63f, 0.86f, 1.0f };
				case UIChatBar::LIGHTBLUE: return { 0.58f, 0.88f, 1.00f, 1.0f };
				case UIChatBar::GREEN:
				case UIChatBar::ITEM_MEGAPHONE: return { 0.48f, 0.92f, 0.58f, 1.0f };
				case UIChatBar::ORANGE: return { 1.00f, 0.70f, 0.32f, 1.0f };
				case UIChatBar::LIGHTGREEN: return { 0.74f, 1.00f, 0.62f, 1.0f };
				default: return Theme::TEXT;
			}
		}

		struct TargetInfo
		{
			const char* label;
			RGBA color;
		};

		TargetInfo target_info(UIChatBar::ChatTarget target)
		{
			switch (target)
			{
				case UIChatBar::CHT_BUDDY: return { "To: Buddy", line_color(UIChatBar::ORANGE) };
				case UIChatBar::CHT_GUILD: return { "To: Guild", line_color(UIChatBar::GREEN) };
				case UIChatBar::CHT_ALLIANCE: return { "To: Alliance", line_color(UIChatBar::LIGHTGREEN) };
				case UIChatBar::CHT_PARTY: return { "To: Party", line_color(UIChatBar::PINK) };
				case UIChatBar::CHT_SQUAD: return { "To: Squad", line_color(UIChatBar::LIGHTBLUE) };
				default: return { "To: All", Theme::TEXT };
			}
		}

		constexpr int16_t LOG_TEXT_W = W - PAD * 2 - TEXT_INSET * 2 - 6;
	}

	int16_t UIChatBar::mobile_input_width()
	{
		return W - PAD * 2 - SEND_W - GAP;
	}

	Rectangle<int16_t> UIChatBar::mobile_window() const
	{
		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t top = HUD_BOTTOM;

		// Below the minimap when it shares the left edge.
		if (auto minimap = UI::get().get_element<UIMiniMap>())
		{
			if (minimap->is_active())
			{
				Point<int16_t> at = minimap->get_position();
				Point<int16_t> size = minimap->get_dimension();

				if (at.x() < HUD_LEFT + W && at.y() < vh / 2)
					top = std::max<int16_t>(top, static_cast<int16_t>(at.y() + size.y() + 6));
			}
		}

		// Down to the chat button's row, above the direction pad; the button hides
		// while the window shows.
		Rect button = chat_button();
		int16_t bottom = button.width() > 0 ? button.bottom() : static_cast<int16_t>(vh - 190);

		top = std::min<int16_t>(top, static_cast<int16_t>(bottom - MIN_H));
		top = std::max<int16_t>(top, HUD_TOP);

		return rect(HUD_LEFT, top, W, static_cast<int16_t>(bottom - top));
	}

	Rectangle<int16_t> UIChatBar::mobile_input() const
	{
		Rect f = mobile_window();

		return rect(static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.bottom() - PAD - INPUT_H),
			mobile_input_width(), INPUT_H);
	}

	Rectangle<int16_t> UIChatBar::mobile_send() const
	{
		Rect input = mobile_input();

		return rect(static_cast<int16_t>(input.right() + GAP), input.top(), SEND_W, INPUT_H);
	}

	Rectangle<int16_t> UIChatBar::mobile_log() const
	{
		Rect f = mobile_window();
		int16_t top = static_cast<int16_t>(f.top() + HEADER_H);

		return rect(static_cast<int16_t>(f.left() + PAD), top, W - PAD * 2,
			static_cast<int16_t>(mobile_input().top() - GAP - top));
	}

	Rectangle<int16_t> UIChatBar::mobile_target_chip() const
	{
		Rect f = mobile_window();
		int16_t w = static_cast<int16_t>(text_width(target_info(chattarget).label, Font::SMALL_BOLD) + 28);

		return rect(static_cast<int16_t>(f.left() + PAD + 52), static_cast<int16_t>(f.top() + 9), w, 26);
	}

	Rectangle<int16_t> UIChatBar::mobile_close() const
	{
		Rect f = mobile_window();

		return rect(static_cast<int16_t>(f.right() - PAD - 30), static_cast<int16_t>(f.top() + 7), 30, 30);
	}

	int16_t UIChatBar::mobile_content() const
	{
		int32_t total = 0;

		for (const MobileLine& line : mobile_lines)
			total += line.height + LINE_GAP;

		return static_cast<int16_t>(std::min<int32_t>(total, 30000));
	}

	void UIChatBar::mobile_clamp_scroll()
	{
		int16_t most = std::max<int16_t>(0, static_cast<int16_t>(mobile_content() - mobile_log().height()));
		mobile_scroll = std::clamp<int16_t>(mobile_scroll, 0, most);
	}

	void UIChatBar::mobile_add_line(const std::string& line, LineType type)
	{
		RGBA c = line_color(type);
		Text text(Text::Font::H13M, Text::Alignment::LEFT, Color::Name::WHITE, line, LOG_TEXT_W * 2, true);
		int16_t height = std::max<int16_t>(line_height(Font::BODY), static_cast<int16_t>(text.height() / 2));

		mobile_lines.push_back({ std::move(text), Color(c.r, c.g, c.b, c.a), height });

		// Reading older lines: keep them where they are.
		if (mobile_scroll > 0)
			mobile_scroll = static_cast<int16_t>(mobile_scroll + height + LINE_GAP);

		if (mobile_lines.size() > MAX_LINES)
			mobile_lines.pop_front();

		if (!chatopen && mobile_unread < 999)
			mobile_unread++;

		mobile_clamp_scroll();
	}

	int32_t UIChatBar::mobile_hit(Point<int16_t> pos) const
	{
		if (inset(mobile_close(), -6, -6).contains(pos))
			return CLOSE;

		if (inset(mobile_target_chip(), -4, -6).contains(pos))
			return CHAT_TARGET;

		if (mobile_input().contains(pos))
			return INPUT;

		if (inset(mobile_send(), -2, -4).contains(pos))
			return SEND;

		if (mobile_log().contains(pos))
			return LOG;

		return -1;
	}

	Cursor::State UIChatBar::mobile_cursor(bool clicking, Point<int16_t> pos)
	{
		if (clicking)
		{
			uint32_t serial = UI::get().get_press_serial();

			if (!mobile_tracking || serial != mobile_press)
			{
				mobile_tracking = true;
				mobile_dragging = false;
				mobile_press = serial;
				mobile_start = pos;
				mobile_last = pos;
				mobile_target = mobile_hit(pos);

				return Cursor::State::CLICKING;
			}

			// A vertical move over the log scrolls it.
			if (!mobile_dragging && mobile_log().contains(mobile_start) && std::abs(pos.y() - mobile_start.y()) > 8)
				mobile_dragging = true;

			if (mobile_dragging)
			{
				mobile_scroll = static_cast<int16_t>(mobile_scroll + (pos.y() - mobile_last.y()));
				mobile_clamp_scroll();
			}

			mobile_last = pos;

			return Cursor::State::CLICKING;
		}

		if (mobile_tracking)
		{
			int32_t target = mobile_target;
			bool tap = !mobile_dragging && target >= 0 && mobile_hit(pos) == target;

			mobile_tracking = false;
			mobile_dragging = false;
			mobile_target = -1;

			if (tap)
			{
				switch (target)
				{
					case CLOSE:
						Sound(Sound::Name::MENUDOWN).play();
						set_chat_open(false);
						break;
					case CHAT_TARGET:
						Sound(Sound::Name::BUTTONCLICK).play();
						cycle_chat_target();
						break;
					case INPUT:
						toggle_chatfield(true);
						break;
					case SEND:
						if (submit_chatfield())
							Sound(Sound::Name::BUTTONCLICK).play();

						toggle_chatfield(false);
						break;
				}
			}
		}

		return mobile_window().contains(pos) ? Cursor::State::CANCLICK : Cursor::State::IDLE;
	}

	void UIChatBar::mobile_draw(float) const
	{
		if (!chatopen)
			return;

		Rect f = mobile_window();
		int32_t pressed = mobile_tracking && !mobile_dragging ? mobile_target : -1;

		fill(f, BACKDROP, 14);

		// Header: title, who the line goes to, close.
		text("Chat", Font::TITLE, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(f.left() + PAD + 2), static_cast<int16_t>(f.top() + 12)));

		TargetInfo target = target_info(chattarget);
		Rect chip = mobile_target_chip();
		fill(chip, target.color.alpha(pressed == CHAT_TARGET ? 0.34f : 0.20f), 13);
		text_in(target.label, Font::SMALL_BOLD, target.color, chip, Text::Alignment::LEFT, 10);
		glyph(rect(static_cast<int16_t>(chip.right() - 18), static_cast<int16_t>(chip.top() + 9), 10, 8), Glyph::DOWN, target.color, 8);

		icon_button(mobile_close(), Glyph::CLOSE, Style::GHOST, true, pressed == CLOSE);
		fill(rect(static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.top() + HEADER_H - 5), W - PAD * 2, 1), Theme::LINE);

		// The log, newest at the bottom.
		Rect log = mobile_log();
		GraphicsGL::get().begin_clip(log.top(), log.bottom());

		int16_t y = static_cast<int16_t>(log.bottom() + mobile_scroll);

		for (auto it = mobile_lines.rbegin(); it != mobile_lines.rend(); ++it)
		{
			y = static_cast<int16_t>(y - it->height);

			if (y < log.bottom())
			{
				if (y + it->height < log.top())
					break;

				Point<int16_t> at(static_cast<int16_t>(log.left() + TEXT_INSET), y);
				it->text.draw(DrawArgument(at, at, Point<int16_t>(0, 0), 0.5f, 0.5f, it->color, 0.0f));
			}

			y = static_cast<int16_t>(y - LINE_GAP);
		}

		GraphicsGL::get().end_clip();

		// Where the visible part sits in the whole log.
		int16_t content = mobile_content();

		if (content > log.height())
		{
			int16_t track = log.height();
			int16_t thumb = std::max<int16_t>(24, static_cast<int16_t>(track * log.height() / content));
			int16_t travel = static_cast<int16_t>(track - thumb);
			int16_t most = static_cast<int16_t>(content - log.height());
			int16_t ty = static_cast<int16_t>(log.top() + travel - travel * mobile_scroll / std::max<int16_t>(1, most));

			fill(rect(static_cast<int16_t>(log.right() - 3), ty, 3, thumb), Theme::FAINT.alpha(0.8f), 1);
		}

		// The input line.
		Rect input = mobile_input();
		bool typing = chatfield.get_state() == Textfield::State::FOCUSED;
		fill(input, typing ? RGBA{ 1.0f, 1.0f, 1.0f, 0.16f } : RGBA{ 1.0f, 1.0f, 1.0f, 0.09f }, 10);

		if (typing)
			fill(rect(static_cast<int16_t>(input.left() + 10), static_cast<int16_t>(input.bottom() - 3),
				static_cast<int16_t>(input.width() - 20), 2), Theme::ACCENT, 1);

		if (chatfield.empty() && !typing)
			text_in("Tap to chat", Font::BODY, Theme::MUTED, input, Text::Alignment::LEFT, 12);

		chatfield.draw(input.get_left_top());

		button(mobile_send(), "Send", Style::PRIMARY, !chatfield.empty(), pressed == SEND, Font::SMALL_BOLD);
	}
}
