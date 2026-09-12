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
#include "UINotice.h"

#include "../NotificationCenter.h"
#include "UIStatusBar.h"

#include "../UI.h"

#include "../Components/MapleButton.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UINotice::UINotice(std::string message, NoticeType t, Text::Alignment a) : UIDragElement<PosNOTICE>(Point<int16_t>(250, 20)), type(t), alignment(a)
	{
		// v83 Basic.img has no Notice6. Notice3 is the plain blue notice and
		// Notice4 the variant whose bottom piece carries a white text field,
		// so between them they replace Notice6's c_box / box / box2 stack.
		nl::node basic = nl::nx::ui["Basic.img"];
		nl::node src = (type == NoticeType::ENTERNUMBER) ? basic["Notice4"] : basic["Notice3"];

		top = src["t"];
		center = src["c"];
		bottom = src["s"];

		if (type == NoticeType::YESNO)
		{
			question = Text(Text::Font::A11M, alignment, Color::Name::WHITE, message, TEXT_WIDTH);
		}
		else if (type == NoticeType::ENTERNUMBER)
		{
			question = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, message, TEXT_WIDTH);
		}
		else if (type == NoticeType::OK)
		{
			question = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, message, TEXT_WIDTH);
		}

		height = question.height();

		// Enough 20px centre tiles for the text plus its padding
		int16_t tile = center.height() > 0 ? center.height() : 20;
		body_rows = (height + BODY_PAD * 2 + tile - 1) / tile;

		if (body_rows < BODY_MIN_ROWS)
			body_rows = BODY_MIN_ROWS;

		dimension = Point<int16_t>(top.width(), top.height() + body_rows * tile + bottom.height());

		// Settings store the top-left corner already (that's what
		// UIDragElement saves). Re-centring on every open caused the
		// popup to drift left and up each time it was reopened.
		dragarea = Point<int16_t>(dimension.x(), 20);

		if (type != NoticeType::ENTERNUMBER)
			Sound(Sound::Name::DLGNOTICE).play();
	}

	UINotice::UINotice(std::string message, NoticeType t) : UINotice(message, t, Text::Alignment::CENTER) {}

	void UINotice::draw(bool textfield) const
	{
		// The piece set was chosen by type in the constructor, so the flag only
		// decides where the text is centred: above the field or in the whole panel.
		Point<int16_t> start = position;
		int16_t body = body_rows * center.height();

		top.draw(start);
		start.shift_y(top.height());

		center.draw(DrawArgument(start, Point<int16_t>(0, body)));
		start.shift_y(body);

		bottom.draw(start);

		// Centre the text vertically in the blue area
		int16_t blue_h = (top.height() - TOP_BLUE_Y) + body + (textfield ? FIELD_BLUE_H : BOTTOM_BLUE_H);
		int16_t text_y = TOP_BLUE_Y + (blue_h - height) / 2;

		if (text_y < TOP_BLUE_Y + 2)
			text_y = TOP_BLUE_Y + 2;

		bool left = (type == NoticeType::ENTERNUMBER) || (type == NoticeType::YESNO && alignment == Text::Alignment::LEFT);
		int16_t text_x = left ? TEXT_LEFT_X : TEXT_CENTER_X;

		question.draw(position + Point<int16_t>(text_x, text_y));
	}

	int16_t UINotice::box2offset(bool textfield) const
	{
		return textfield ? textfield_y() : button_row_y();
	}

	int16_t UINotice::button_row_y() const
	{
		int16_t bottom_y = top.height() + body_rows * center.height();

		if (type == NoticeType::ENTERNUMBER)
			return bottom_y + FIELD_STRIP_Y + (FIELD_STRIP_H - BUTTON_H) / 2;

		return bottom_y + STRIP_Y + (STRIP_H - BUTTON_H) / 2;
	}

	int16_t UINotice::textfield_y() const
	{
		return top.height() + body_rows * center.height() + FIELD_Y;
	}

	UIYesNo::UIYesNo(std::string message, std::function<void(bool yes)> yh, Text::Alignment alignment,
		int16_t button_x_offset, int16_t button_y_offset) : UINotice(message, NoticeType::YESNO, alignment)
	{
		yesnohandler = yh;

		int16_t belowtext = button_row_y() + button_y_offset;

		// v83 has no BtOK4 / BtCancel4; BtOK2 / BtCancel2 are the 47x18 pair
		nl::node src = nl::nx::ui["Basic.img"];

		buttons[Buttons::YES] = std::make_unique<MapleButton>(src["BtOK2"], Point<int16_t>(BTN_YES_X + button_x_offset, belowtext));
		buttons[Buttons::NO] = std::make_unique<MapleButton>(src["BtCancel2"], Point<int16_t>(BTN_NO_X + button_x_offset, belowtext));
	}

	UIYesNo::UIYesNo(std::string message, std::function<void(bool yes)> yesnohandler) : UIYesNo(message, yesnohandler, Text::Alignment::CENTER) {}

	void UIYesNo::draw(float alpha) const
	{
		UINotice::draw(false);
		UIElement::draw(alpha);
	}

	void UIYesNo::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed)
			return;

		if (keycode == KeyAction::Id::RETURN)
		{
			yesnohandler(true);
			deactivate();
		}
		else if (escape)
		{
			yesnohandler(false);
			deactivate();
		}
	}

	UIElement::Type UIYesNo::get_type() const
	{
		return TYPE;
	}

	Button::State UIYesNo::button_pressed(uint16_t buttonid)
	{
		deactivate();

		switch (buttonid)
		{
		case Buttons::YES:
			yesnohandler(true);
			break;
		case Buttons::NO:
			yesnohandler(false);
			break;
		}

		return Button::State::PRESSED;
	}

	UIEnterNumber::UIEnterNumber(std::string message, std::function<void(int32_t)> nh, int32_t m, int32_t quantity,
		int16_t field_y_offset, int16_t button_x_offset, int16_t button_y_offset) : UINotice(message, NoticeType::ENTERNUMBER)
	{
		numhandler = nh;
		max = m;

		int16_t field_top = textfield_y() + field_y_offset;
		int16_t pos_y = button_row_y() + button_y_offset;

		nl::node src = nl::nx::ui["Basic.img"];

		buttons[Buttons::OK] = std::make_unique<MapleButton>(src["BtOK2"], BTN_YES_X + button_x_offset, pos_y);
		buttons[Buttons::CANCEL] = std::make_unique<MapleButton>(src["BtCancel2"], BTN_NO_X + button_x_offset, pos_y);

		// Black text on the white field baked into Notice4/s
		numfield = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, Rectangle<int16_t>(FIELD_LEFT, FIELD_RIGHT, field_top, field_top + FIELD_H), 10);
		numfield.change_text(std::to_string(quantity));

		numfield.set_enter_callback(
			[&](std::string numstr)
			{
				handlestring(numstr);
			}
		);

		numfield.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				deactivate();
			}
		);

		numfield.set_state(Textfield::State::FOCUSED);
	}

	void UIEnterNumber::draw(float alpha) const
	{
		UINotice::draw(true);
		UIElement::draw(alpha);

		numfield.draw(position);
	}

	void UIEnterNumber::update()
	{
		UIElement::update();

		numfield.update(position);
	}

	Cursor::State UIEnterNumber::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (numfield.get_state() == Textfield::State::NORMAL)
		{
			Cursor::State nstate = numfield.send_cursor(cursorpos, clicked);

			if (nstate != Cursor::State::IDLE)
				return nstate;
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIEnterNumber::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (keycode == KeyAction::Id::RETURN)
		{
			handlestring(numfield.get_text());
			deactivate();
		}
		else if (escape)
		{
			deactivate();
		}
	}

	UIElement::Type UIEnterNumber::get_type() const
	{
		return TYPE;
	}

	Button::State UIEnterNumber::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::OK:
			handlestring(numfield.get_text());
			break;
		case Buttons::CANCEL:
			deactivate();
			break;
		}

		return Button::State::NORMAL;
	}

	void UIEnterNumber::handlestring(std::string numstr)
	{
		int num = -1;
		bool has_only_digits = (numstr.find_first_not_of("0123456789") == std::string::npos);

		// okhandler must NOT capture `this` — UIOk and UIEnterNumber
		// share Type::NOTICE, so `emplace<UIOk>` destroys this very
		// window. Any reference to `numfield` / `buttons` in the
		// callback would fire against a freed object → crash on click.
		auto okhandler = [](bool) {};

		if (!has_only_digits)
		{
			UI::get().emplace<UIOk>("Only numbers are allowed.", okhandler);
			return;
		}
		else
		{
			num = std::stoi(numstr);
		}

		if (num < 1)
		{
			UI::get().emplace<UIOk>("You may only enter a number equal to or higher than 1.", okhandler);
			return;
		}
		else if (num > max)
		{
			UI::get().emplace<UIOk>("You may only enter a number equal to or lower than " + std::to_string(max) + ".", okhandler);
			return;
		}
		else
		{
			numhandler(num);
			deactivate();
		}

		buttons[Buttons::OK]->set_state(Button::State::NORMAL);
	}

	UIEnterText::UIEnterText(std::string message, std::function<void(const std::string&)> th, int32_t maxlength,
		int16_t field_y_offset, int16_t button_x_offset, int16_t button_y_offset) : UINotice(message, NoticeType::ENTERNUMBER)
	{
		texthandler = th;

		int16_t field_top = textfield_y() + field_y_offset;
		int16_t pos_y = button_row_y() + button_y_offset;

		nl::node src = nl::nx::ui["Basic.img"];

		buttons[Buttons::OK]     = std::make_unique<MapleButton>(src["BtOK2"],     BTN_YES_X + button_x_offset, pos_y);
		buttons[Buttons::CANCEL] = std::make_unique<MapleButton>(src["BtCancel2"], BTN_NO_X + button_x_offset, pos_y);

		textfield = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, Rectangle<int16_t>(FIELD_LEFT, FIELD_RIGHT, field_top, field_top + FIELD_H), maxlength);

		textfield.set_enter_callback(
			[&](std::string str)
			{
				if (!str.empty())
				{
					texthandler(str);
					deactivate();
				}
			}
		);

		textfield.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				deactivate();
			}
		);

		textfield.set_state(Textfield::State::FOCUSED);
	}

	void UIEnterText::draw(float alpha) const
	{
		UINotice::draw(true);
		UIElement::draw(alpha);

		textfield.draw(position);
	}

	void UIEnterText::update()
	{
		UIElement::update();

		textfield.update(position);
	}

	Cursor::State UIEnterText::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (textfield.get_state() == Textfield::State::NORMAL)
		{
			Cursor::State nstate = textfield.send_cursor(cursorpos, clicked);

			if (nstate != Cursor::State::IDLE)
				return nstate;
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIEnterText::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	UIElement::Type UIEnterText::get_type() const
	{
		return TYPE;
	}

	Button::State UIEnterText::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::OK:
		{
			std::string str = textfield.get_text();
			if (!str.empty())
			{
				texthandler(str);
				deactivate();
			}
			break;
		}
		case Buttons::CANCEL:
			deactivate();
			break;
		}

		return Button::State::NORMAL;
	}

	// ---------- UIWaitNotice ----------

	UIWaitNotice::UIWaitNotice(std::string message) : UINotice(message, NoticeType::OK)
	{
		// No buttons — purely informational. The trade/quest handler
		// will UI::remove() us when the pending state resolves.
	}

	void UIWaitNotice::draw(float alpha) const
	{
		UINotice::draw(false);
		UIElement::draw(alpha);
	}

	void UIWaitNotice::send_key(int32_t, bool, bool) { }
	UIElement::Type UIWaitNotice::get_type() const { return TYPE; }

	UIOk::UIOk(std::string message, std::function<void(bool ok)> oh) : UINotice(message, NoticeType::OK)
	{
		okhandler = oh;

		nl::node src = nl::nx::ui["Basic.img"];

		buttons[Buttons::OK] = std::make_unique<MapleButton>(src["BtOK2"], BTN_OK_X, button_row_y());
	}

	void UIOk::draw(float alpha) const
	{
		UINotice::draw(false);
		UIElement::draw(alpha);
	}

	void UIOk::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (keycode == KeyAction::Id::RETURN)
			{
				okhandler(true);
				deactivate();
			}
			else if (escape)
			{
				okhandler(false);
				deactivate();
			}
		}
	}

	UIElement::Type UIOk::get_type() const
	{
		return TYPE;
	}

	Button::State UIOk::button_pressed(uint16_t buttonid)
	{
		deactivate();

		switch (buttonid)
		{
		case Buttons::OK:
			okhandler(true);
			break;
		}

		return Button::State::NORMAL;
	}

	// ---------- UIAlarmInvite ----------

	UIAlarmInvite::UIAlarmInvite(std::string message, std::function<void(bool yes)> h, int variant)
		: UIDragElement<PosNOTICE>(Point<int16_t>(0, 0))
	{
		handler = h;

		nl::node root = nl::nx::ui["UIWindow.img"]["FadeYesNo"];

		// Variant 0 = "backgrnd"; 1..11 = "backgrnd1".."backgrnd11".
		std::string bg_name = (variant == 0) ? "backgrnd"
			: "backgrnd" + std::to_string(variant);
		backdrop = Texture(root[bg_name]);
		if (!backdrop.is_valid())
			backdrop = Texture(root["backgrnd"]);

		icon = Texture(root["icon" + std::to_string(variant)]);
		if (!icon.is_valid())
			icon = Texture(root["icon0"]);

		Point<int16_t> dims = backdrop.get_dimensions();
		dimension = dims;
		dragarea  = Point<int16_t>(dims.x(), 20);

		// Anchor above the notice button, clamped to the viewport.
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t notice_x = 512 + 56;
		int16_t anchor_x = notice_x - dims.x() / 2 - 75;
		if (anchor_x < 4) anchor_x = 4;
		if (anchor_x + dims.x() > vw - 4) anchor_x = vw - dims.x() - 4;
		int16_t anchor_y = vh - dims.y() - 60;           // 10px down
		position = Point<int16_t>(anchor_x, anchor_y);

		// Text next to the icon (icon on the left, text to its right).
		int16_t icon_w = icon.is_valid() ? icon.get_dimensions().x() : 18;
		int16_t text_x = 6 + icon_w + 6;
		int16_t text_area = dims.x() - text_x - 6;
		message_text = message;
		label = Text(Text::Font::A11M, Text::Alignment::LEFT,
			Color::Name::WHITE, message, text_area);

		// OK above Cancel, stacked on the right edge.
		buttons[Buttons::ACCEPT]  = std::make_unique<MapleButton>(
			root["BtOK"],     Point<int16_t>(dims.x() - 16, dims.y() / 2 - 19));
		buttons[Buttons::DECLINE] = std::make_unique<MapleButton>(
			root["BtCancel"], Point<int16_t>(dims.x() - 16, dims.y() / 2 - 3));

		Sound(Sound::Name::DLGNOTICE).play();
	}

	void UIAlarmInvite::draw(float alpha) const
	{
		// Fade in over the first 300ms, fade out over the last 600ms
		// before the popup stashes itself into the drawer.
		float a = 1.0f;

		if (age_ms < 300)
			a = age_ms / 300.0f;
		else if (age_ms > HOLD_MS - 600)
			a = (HOLD_MS - age_ms) / 600.0f;

		if (a < 0.0f) a = 0.0f;
		if (a > 1.0f) a = 1.0f;

		backdrop.draw(DrawArgument(position, a));
		if (icon.is_valid())
			icon.draw(DrawArgument(position + Point<int16_t>(6, 6), a));

		int16_t icon_w = icon.is_valid() ? icon.get_dimensions().x() : 18;
		label.draw(DrawArgument(position + Point<int16_t>(6 + icon_w + 6, 6), a));

		UIElement::draw(alpha);
	}

	void UIAlarmInvite::update()
	{
		UIElement::update();

		age_ms += Constants::TIMESTEP;

		if (age_ms >= HOLD_MS && !stashed)
			stash();
	}

	void UIAlarmInvite::stash()
	{
		if (stashed)
			return;

		stashed = true;
		NotificationCenter::get().push("Invite", message_text, handler);

		if (auto statusbar = UI::get().get_element<UIStatusBar>())
			statusbar->notify();

		deactivate();
	}

	void UIAlarmInvite::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			handler(false);
			deactivate();
		}
	}

	UIElement::Type UIAlarmInvite::get_type() const { return TYPE; }

	Button::State UIAlarmInvite::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::ACCEPT:
			handler(true);
			deactivate();
			break;
		case Buttons::DECLINE:
			handler(false);
			deactivate();
			break;
		}
		return Button::State::NORMAL;
	}

	// ---------- UIDeathNotice ----------

	UIDeathNotice::UIDeathNotice(std::string message, std::function<void(bool ok)> oh)
		: UIDragElement<PosNOTICE>(Point<int16_t>(0, 0))
	{
		okhandler = oh;

		nl::node notice_root = nl::nx::ui["UIWindow.img"]["Notice"];
		backdrop = Texture(notice_root["0"]);
		dimension = backdrop.get_dimensions();
		dragarea = Point<int16_t>(dimension.x(), 20);

		question = Text(Text::Font::A12M, Text::Alignment::CENTER,
			Color::Name::WHITE, message, dimension.x() - 30);

		// OK button — reuse the 47x18 Basic.img/BtOK2 sprite. Place it
		// centered near the bottom of the 286x146 frame.
		nl::node basic = nl::nx::ui["Basic.img"];
		buttons[Buttons::OK] = std::make_unique<MapleButton>(
			basic["BtOK2"],
			Point<int16_t>(dimension.x() / 2 - 23, dimension.y() - 30));

		Sound(Sound::Name::DLGNOTICE).play();
	}

	void UIDeathNotice::draw(float alpha) const
	{
		// The UIWindow.img/Notice tombstone sprite already has its message
		// baked in, so we don't draw our own text over it.
		backdrop.draw(DrawArgument(position));
		UIElement::draw(alpha);
	}

	void UIDeathNotice::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && (keycode == KeyAction::Id::RETURN || escape))
		{
			okhandler(true);
			deactivate();
		}
	}

	UIElement::Type UIDeathNotice::get_type() const
	{
		return TYPE;
	}

	Button::State UIDeathNotice::button_pressed(uint16_t buttonid)
	{
		if (buttonid == Buttons::OK)
		{
			okhandler(true);
			deactivate();
		}
		return Button::State::NORMAL;
	}
}