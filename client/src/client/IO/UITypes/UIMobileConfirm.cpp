//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileConfirm.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Graphics/GraphicsGL.h"

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t CARD_W = 440;
		constexpr int16_t PAD = 24;
		constexpr int16_t BUTTON_H = 46;
	}

	UIMobileConfirm::UIMobileConfirm(std::string t, std::string m, std::string c, bool d, std::function<void()> f) :
		UIElement(Point<int16_t>(0, 0), Point<int16_t>(Constants::Constants::get().get_viewwidth(), Constants::Constants::get().get_viewheight())),
		title(std::move(t)), message(std::move(m)), confirm(std::move(c)), danger(d), on_confirm(std::move(f))
	{
		height = static_cast<int16_t>(PAD + 24 + 12 + text_height(message, Font::BODY, CARD_W - PAD * 2) + 22 + BUTTON_H + PAD);
		Sound(Sound::Name::MENUUP).play();
	}

	Rect UIMobileConfirm::card() const
	{
		int16_t x = static_cast<int16_t>((dimension.x() - CARD_W) / 2);
		int16_t y = static_cast<int16_t>((dimension.y() - height) / 2 - 20);

		return rect(x, y, CARD_W, height);
	}

	Rect UIMobileConfirm::cancel_rect() const
	{
		Rect c = card();
		int16_t w = static_cast<int16_t>((CARD_W - PAD * 2 - 12) / 2);

		return rect(static_cast<int16_t>(c.left() + PAD), static_cast<int16_t>(c.bottom() - PAD - BUTTON_H), w, BUTTON_H);
	}

	Rect UIMobileConfirm::confirm_rect() const
	{
		Rect c = cancel_rect();

		return rect(static_cast<int16_t>(c.right() + 12), c.top(), c.width(), BUTTON_H);
	}

	void UIMobileConfirm::draw(float) const
	{
		GraphicsGL::get().drawscreenfill(0.0f, 0.0f, 0.0f, 0.55f);

		Rect c = card();
		fill(c, Theme::PANEL, 20);

		text(title, Font::HEADLINE, Theme::TEXT, Point<int16_t>(c.left() + PAD, c.top() + PAD - 2));
		text(message, Font::BODY, Theme::MUTED, Point<int16_t>(c.left() + PAD, c.top() + PAD + 36),
			Text::Alignment::LEFT, CARD_W - PAD * 2);

		button(cancel_rect(), "Cancel", Style::SECONDARY, true, pressed == CANCEL);
		button(confirm_rect(), confirm, danger ? Style::DANGER : Style::PRIMARY, true, pressed == CONFIRM);
	}

	void UIMobileConfirm::update()
	{
		if (pressed != NONE && !UI::get().is_mouse_held())
			pressed = NONE;
	}

	bool UIMobileConfirm::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	UIMobileConfirm::Target UIMobileConfirm::hit(Point<int16_t> pos) const
	{
		if (cancel_rect().contains(pos))
			return CANCEL;

		if (confirm_rect().contains(pos))
			return CONFIRM;

		return NONE;
	}

	Cursor::State UIMobileConfirm::send_cursor(bool down, Point<int16_t> pos)
	{
		if (down)
		{
			uint32_t serial = UI::get().get_press_serial();

			if (serial != press)
			{
				press = serial;
				pressed = hit(pos);

				// A tap outside the card is a "no".
				if (pressed == NONE && !card().contains(pos))
					finish(false);
			}

			return Cursor::State::CLICKING;
		}

		Target t = pressed;
		pressed = NONE;

		if (t != NONE && hit(pos) == t)
			finish(t == CONFIRM);

		return Cursor::State::IDLE;
	}

	void UIMobileConfirm::send_key(int32_t keycode, bool pressed_key, bool escape)
	{
		if (!pressed_key)
			return;

		if (escape)
			finish(false);
		else if (keycode == KeyAction::Id::RETURN)
			finish(true);
	}

	void UIMobileConfirm::finish(bool yes)
	{
		Sound(Sound::Name::BUTTONCLICK).play();
		deactivate();

		if (yes && on_confirm)
			on_confirm();
	}

	UIElement::Type UIMobileConfirm::get_type() const
	{
		return TYPE;
	}
}
