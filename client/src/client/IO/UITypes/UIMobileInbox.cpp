//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileInbox.h"

#include "../Components/MobileNoticeCard.h"
#include "../NotificationCenter.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Graphics/GraphicsGL.h"

#include <string>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 380;
		constexpr int16_t PAD = 16;
		constexpr int16_t TOP = 78;
		constexpr int16_t GAP = 10;
		constexpr int16_t ROW_PAD = 14;
		constexpr int16_t BADGE = 30;
		constexpr int16_t BUTTON_H = 38;
		constexpr int16_t FOOTER_H = 44;

		constexpr int32_t T_CLEAR = 10;
		constexpr int32_t T_ROW = 100;	// + index * 2, + 1 for Accept / View

		const std::vector<NotificationCenter::Entry>& entries()
		{
			return NotificationCenter::get().list();
		}

		std::string age_text(int32_t ms)
		{
			int32_t minutes = ms / 60000;

			if (minutes < 1)
				return "now";

			return std::to_string(minutes) + "m ago";
		}
	}

	UIMobileInbox::UIMobileInbox() : UIMobilePanel(Edge::RIGHT, W) {}

	void UIMobileInbox::update()
	{
		UIMobilePanel::update();
		clamp_scroll();
	}

	int16_t UIMobileInbox::column_width() const
	{
		return static_cast<int16_t>(W - PAD * 2 - ROW_PAD * 2 - BADGE - 12);
	}

	bool UIMobileInbox::two_actions(size_t index) const
	{
		return static_cast<bool>(entries()[index].resolver);
	}

	int16_t UIMobileInbox::row_height(size_t index) const
	{
		const auto& e = entries()[index];
		int16_t body = e.body.empty() ? 0 : static_cast<int16_t>(text_height(MobileNoticeCard::clean(e.body), Font::BODY, column_width()) + 4);

		return static_cast<int16_t>(ROW_PAD + 20 + body + 12 + BUTTON_H + ROW_PAD);
	}

	Rectangle<int16_t> UIMobileInbox::scroll_area() const
	{
		Rect f = frame();
		int16_t footer = entries().empty() ? 0 : static_cast<int16_t>(FOOTER_H + 12);

		return rect(static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.top() + TOP), W - PAD * 2,
			static_cast<int16_t>(f.height() - TOP - PAD - footer));
	}

	int16_t UIMobileInbox::scroll_content() const
	{
		int16_t h = 0;

		for (size_t i = 0; i < entries().size(); i++)
			h = static_cast<int16_t>(h + row_height(i) + GAP);

		return h;
	}

	Rect UIMobileInbox::row_rect(size_t index) const
	{
		Rect area = scroll_area();
		int16_t y = static_cast<int16_t>(area.top() - scroll);

		for (size_t i = 0; i < index; i++)
			y = static_cast<int16_t>(y + row_height(i) + GAP);

		return rect(area.left(), y, area.width(), row_height(index));
	}

	Rect UIMobileInbox::action_rect(size_t index, bool accept) const
	{
		Rect r = row_rect(index);
		int16_t left = static_cast<int16_t>(r.left() + ROW_PAD + BADGE + 12);
		int16_t width = column_width();
		int16_t top = static_cast<int16_t>(r.bottom() - ROW_PAD - BUTTON_H);

		if (!two_actions(index))
			return rect(left, top, width, BUTTON_H);

		int16_t half = static_cast<int16_t>((width - 8) / 2);

		return rect(accept ? static_cast<int16_t>(left + half + 8) : left, top, half, BUTTON_H);
	}

	Rect UIMobileInbox::clear_rect() const
	{
		Rect f = frame();

		return rect(static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.bottom() - PAD - FOOTER_H), W - PAD * 2, FOOTER_H);
	}

	int32_t UIMobileInbox::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		if (!entries().empty() && clear_rect().contains(pos))
			return T_CLEAR;

		if (!scroll_area().contains(pos))
			return NO_TARGET;

		for (size_t i = 0; i < entries().size(); i++)
		{
			if (action_rect(i, false).contains(pos))
				return static_cast<int32_t>(T_ROW + i * 2);

			if (two_actions(i) && action_rect(i, true).contains(pos))
				return static_cast<int32_t>(T_ROW + i * 2 + 1);
		}

		return NO_TARGET;
	}

	void UIMobileInbox::activate(int32_t target)
	{
		if (target == T_CLEAR)
		{
			Sound(Sound::Name::BUTTONCLICK).play();

			// Dropped unanswered: whoever asked just never hears back.
			NotificationCenter::get().clear();
			deactivate();

			return;
		}

		if (target < T_ROW)
			return;

		size_t index = static_cast<size_t>((target - T_ROW) / 2);
		bool accept = (target - T_ROW) % 2 == 1;

		if (index >= entries().size())
			return;

		Sound(Sound::Name::BUTTONCLICK).play();

		// The answer may open a window or post another notification.
		NotificationCenter::get().resolve(entries()[index].id, accept);

		if (NotificationCenter::get().empty())
			deactivate();
	}

	void UIMobileInbox::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();
		size_t n = entries().size();

		std::string subtitle = n == 0 ? "You're all caught up" : n == 1 ? "1 waiting for you" : std::to_string(n) + " waiting for you";
		panel(f, "Inbox", subtitle);
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		Rect area = scroll_area();

		if (n == 0)
		{
			text_in("Invites and alerts you haven't answered show up here.", Font::BODY, Theme::MUTED,
				rect(area.left(), static_cast<int16_t>(area.top() + 20), area.width(), 40));

			return;
		}

		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		for (size_t i = 0; i < n; i++)
		{
			Rect r = row_rect(i);

			if (r.bottom() < area.top() || r.top() > area.bottom())
				continue;

			const auto& e = entries()[i];
			MobileNoticeCard::Kind kind = MobileNoticeCard::title_kind(e.title);
			fill(r, Theme::CARD, 14);

			Rect b = rect(static_cast<int16_t>(r.left() + ROW_PAD), static_cast<int16_t>(r.top() + ROW_PAD), BADGE, BADGE);
			MobileNoticeCard::badge(b, kind);

			int16_t x = static_cast<int16_t>(b.right() + 12);
			std::string age = age_text(e.age);
			int16_t age_w = MobileUI::text_width(age, Font::SMALL);

			text(ellipsize(e.title, Font::BODY_BOLD, static_cast<int16_t>(column_width() - age_w - 8)), Font::BODY_BOLD, Theme::TEXT,
				Point<int16_t>(x, static_cast<int16_t>(r.top() + ROW_PAD)));
			text(age, Font::SMALL, Theme::FAINT, Point<int16_t>(static_cast<int16_t>(r.right() - ROW_PAD), static_cast<int16_t>(r.top() + ROW_PAD + 2)),
				Text::Alignment::RIGHT);

			if (!e.body.empty())
				text(MobileNoticeCard::clean(e.body), Font::BODY, Theme::MUTED, Point<int16_t>(x, static_cast<int16_t>(r.top() + ROW_PAD + 22)),
					Text::Alignment::LEFT, column_width());

			int32_t decline = static_cast<int32_t>(T_ROW + i * 2);

			if (!two_actions(i))
			{
				button(action_rect(i, false), "Dismiss", Style::SECONDARY, true, pressed == decline, Font::SMALL_BOLD);
				continue;
			}

			button(action_rect(i, false), e.invite ? "Decline" : "Dismiss", Style::SECONDARY, true, pressed == decline, Font::SMALL_BOLD);
			button(action_rect(i, true), e.invite ? "Accept" : "View", Style::PRIMARY, true, pressed == decline + 1, Font::SMALL_BOLD);
		}

		GraphicsGL::get().end_clip();

		button(clear_rect(), "Clear all", Style::GHOST, true, pressed == T_CLEAR);
	}

	UIElement::Type UIMobileInbox::get_type() const
	{
		return TYPE;
	}
}
