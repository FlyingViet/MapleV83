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

#include "../MobileUI.h"
#include "../NotificationCenter.h"
#include "../Notifications.h"
#include "UIStatusBar.h"

#include "../UI.h"

#include "../Components/MapleButton.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

#include <cctype>
#include <cstdlib>

namespace ms
{
	using Kind = MobileNoticeCard::Kind;
	using MobileUI::Style;

	namespace
	{
		// The number in a field, or -1 when it isn't one (empty, letters,
		// more digits than an int holds).
		int64_t parse_amount(const std::string& str)
		{
			if (str.empty() || str.size() > 10 || str.find_first_not_of("0123456789") != std::string::npos)
				return -1;

			return std::strtoll(str.c_str(), nullptr, 10);
		}

		// The phone's text fields: white 18 px text, the row's full width so a
		// tap anywhere on it keeps the keyboard up.
		Textfield card_textfield(const MobileUI::Rect& field, size_t limit)
		{
			return Textfield(Text::Font::A18M, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::WHITE, 1.0f,
				Rectangle<int16_t>(Point<int16_t>(16, 14), Point<int16_t>(static_cast<int16_t>(field.width() - 16), field.height())), limit);
		}

		// "PARTY INVITE" -> "Party Invite".
		std::string title_case(const std::string& upper)
		{
			std::string out = upper;
			bool start = true;

			for (char& c : out)
			{
				c = start ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				start = c == ' ';
			}

			return out;
		}
	}

	void UINoticeFrame::use_card(Kind kind, const std::string& eyebrow, const std::string& message)
	{
		mobile = true;
		card.setup(MobileNoticeCard::Layout::DIALOG, kind, eyebrow, message);

		position = Point<int16_t>(0, 0);
		dimension = Point<int16_t>(Constants::Constants::get().get_viewwidth(), Constants::Constants::get().get_viewheight());
	}

	void UINoticeFrame::update()
	{
		UIElement::update();

		if (mobile)
			card.update();
	}

	void UINoticeFrame::update_screen(int16_t new_width, int16_t new_height)
	{
		if (mobile)
			dimension = Point<int16_t>(new_width, new_height);
		else
			UIDragElement::update_screen(new_width, new_height);
	}

	bool UINoticeFrame::is_in_range(Point<int16_t> cursorpos) const
	{
		// The dimmed screen is part of the notice.
		return mobile || UIElement::is_in_range(cursorpos);
	}

	Cursor::State UINoticeFrame::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (!mobile)
			return UIDragElement::send_cursor(clicked, cursorpos);

		MobileNoticeCard::Touch touch = card.touch(clicked, cursorpos);
		Cursor::State state = clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		// Each of these can close or replace this notice: nothing after them
		// may use a member.
		switch (touch.tap)
		{
		case MobileNoticeCard::Tap::ACTION:
			Sound(Sound::Name::BUTTONCLICK).play();
			card_action(touch.id);
			break;
		case MobileNoticeCard::Tap::FIELD:
			card_field();
			break;
		case MobileNoticeCard::Tap::OUTSIDE:
			card_outside();
			break;
		default:
			break;
		}

		return state;
	}

	void UINoticeFrame::card_action(int32_t id)
	{
		button_pressed(static_cast<uint16_t>(id));
	}

	void UINoticeFrame::card_outside()
	{
		send_key(KeyAction::Id::ESCAPE, true, true);
	}

	UINotice::UINotice(std::string message, NoticeType t, Text::Alignment a) : UINoticeFrame(Point<int16_t>(250, 20)), type(t), alignment(a)
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

		if (MobileUI::enabled())
		{
			use_card(Kind::QUESTION, "CONFIRM", message);
			card.add(Buttons::NO, "No", Style::SECONDARY);
			card.add(Buttons::YES, "Yes", Style::PRIMARY);
		}
	}

	UIYesNo::UIYesNo(std::string message, std::function<void(bool yes)> yesnohandler) : UIYesNo(message, yesnohandler, Text::Alignment::CENTER) {}

	void UIYesNo::draw(float alpha) const
	{
		if (mobile)
			return card.draw();

		UINotice::draw(false);
		UIElement::draw(alpha);
	}

	void UIYesNo::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed)
			return;

		// The handler may open the next notice, which replaces this one.
		auto handler = yesnohandler;

		if (keycode == KeyAction::Id::RETURN)
		{
			deactivate();
			handler(true);
		}
		else if (escape)
		{
			deactivate();
			handler(false);
		}
	}

	UIElement::Type UIYesNo::get_type() const
	{
		return TYPE;
	}

	Button::State UIYesNo::button_pressed(uint16_t buttonid)
	{
		auto handler = yesnohandler;
		deactivate();

		switch (buttonid)
		{
		case Buttons::YES:
			handler(true);
			break;
		case Buttons::NO:
			handler(false);
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

		if (MobileUI::enabled())
		{
			use_card(Kind::AMOUNT, "HOW MANY?  (UP TO " + std::to_string(max) + ")", message);
			card.add_field(true);
			card.add(Buttons::CANCEL, "Cancel", Style::SECONDARY);
			card.add(Buttons::OK, "OK", Style::PRIMARY);

			numfield = card_textfield(card.field_rect(), 10);
		}
		else
		{
			// Black text on the white field baked into Notice4/s
			numfield = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, Rectangle<int16_t>(FIELD_LEFT, FIELD_RIGHT, field_top, field_top + FIELD_H), 10);
		}

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

		// A phone sets the amount with - / + / Max; the keyboard waits for a
		// tap on the number.
		if (!mobile)
			numfield.set_state(Textfield::State::FOCUSED);
	}

	UIEnterNumber::~UIEnterNumber()
	{
		unfocus();
	}

	void UIEnterNumber::unfocus()
	{
		if (numfield.get_state() == Textfield::State::FOCUSED)
			UI::get().remove_textfield();
	}

	void UIEnterNumber::deactivate()
	{
		unfocus();
		UINotice::deactivate();
	}

	void UIEnterNumber::draw(float alpha) const
	{
		if (mobile)
		{
			card.draw(numfield.get_state() == Textfield::State::FOCUSED);
			numfield.draw(card.field_rect().get_left_top());

			return;
		}

		UINotice::draw(true);
		UIElement::draw(alpha);

		numfield.draw(position);
	}

	void UIEnterNumber::update()
	{
		UINoticeFrame::update();

		numfield.update(mobile ? card.field_rect().get_left_top() : position);
	}

	void UIEnterNumber::card_field()
	{
		numfield.set_state(Textfield::State::FOCUSED);
	}

	void UIEnterNumber::card_action(int32_t id)
	{
		int64_t amount = parse_amount(numfield.get_text());

		switch (id)
		{
		case MobileNoticeCard::STEP_DOWN:
			amount = std::min<int64_t>(amount - 1, max);
			break;
		case MobileNoticeCard::STEP_UP:
			amount = amount + 1;
			break;
		case MobileNoticeCard::STEP_MAX:
			amount = max;
			break;
		default:
			return UINotice::card_action(id);
		}

		amount = std::max<int64_t>(1, std::min<int64_t>(amount, max));
		numfield.change_text(std::to_string(amount));
	}

	Cursor::State UIEnterNumber::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (mobile)
			return UINoticeFrame::send_cursor(clicked, cursorpos);

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
		// A phone keeps the card up when the amount is out of range.
		if (mobile && keycode == KeyAction::Id::RETURN)
		{
			if (pressed)
				handlestring(numfield.get_text());

			return;
		}

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
		// A phone corrects the field rather than replacing the card with an
		// error: the player sees the nearest amount allowed and taps OK again.
		if (mobile)
		{
			int64_t amount = parse_amount(numstr);

			if (amount < 1 || amount > max)
			{
				numfield.change_text(std::to_string(amount < 1 ? 1 : max));

				return;
			}

			auto handler = numhandler;
			deactivate();
			handler(static_cast<int32_t>(amount));

			return;
		}

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

		if (MobileUI::enabled())
		{
			use_card(Kind::TEXT, "TYPE YOUR ANSWER", message);
			card.add_field(false);
			card.add(Buttons::CANCEL, "Cancel", Style::SECONDARY);
			card.add(Buttons::OK, "OK", Style::PRIMARY);

			textfield = card_textfield(card.field_rect(), maxlength);
		}
		else
		{
			textfield = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, Rectangle<int16_t>(FIELD_LEFT, FIELD_RIGHT, field_top, field_top + FIELD_H), maxlength);
		}

		textfield.set_enter_callback(
			[&](std::string str)
			{
				if (!str.empty())
				{
					auto handler = texthandler;
					deactivate();
					handler(str);
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

	UIEnterText::~UIEnterText()
	{
		unfocus();
	}

	void UIEnterText::unfocus()
	{
		if (textfield.get_state() == Textfield::State::FOCUSED)
			UI::get().remove_textfield();
	}

	void UIEnterText::deactivate()
	{
		unfocus();
		UINotice::deactivate();
	}

	void UIEnterText::draw(float alpha) const
	{
		if (mobile)
		{
			bool focused = textfield.get_state() == Textfield::State::FOCUSED;
			MobileUI::Rect field = card.field_rect();
			card.draw(focused);

			if (textfield.empty() && !focused)
				MobileUI::text_in("Tap to type", MobileUI::Font::BODY, MobileUI::Theme::FAINT, field, Text::Alignment::LEFT, 16);

			textfield.draw(field.get_left_top());

			return;
		}

		UINotice::draw(true);
		UIElement::draw(alpha);

		textfield.draw(position);
	}

	void UIEnterText::update()
	{
		UINoticeFrame::update();

		textfield.update(mobile ? card.field_rect().get_left_top() : position);
	}

	void UIEnterText::card_field()
	{
		textfield.set_state(Textfield::State::FOCUSED);
	}

	Cursor::State UIEnterText::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (mobile)
			return UINoticeFrame::send_cursor(clicked, cursorpos);

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
				auto handler = texthandler;
				deactivate();
				handler(str);
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
		if (MobileUI::enabled())
			use_card(Kind::WAIT, "PLEASE WAIT", message);
	}

	void UIWaitNotice::draw(float alpha) const
	{
		if (mobile)
			return card.draw();

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

		if (MobileUI::enabled())
		{
			use_card(Kind::INFO, "NOTICE", message);
			card.add(Buttons::OK, "OK", Style::PRIMARY);
		}
	}

	void UIOk::draw(float alpha) const
	{
		if (mobile)
			return card.draw();

		UINotice::draw(false);
		UIElement::draw(alpha);
	}

	void UIOk::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			// The handler may open the next notice, which replaces this one.
			auto handler = okhandler;

			if (keycode == KeyAction::Id::RETURN)
			{
				deactivate();
				handler(true);
			}
			else if (escape)
			{
				deactivate();
				handler(false);
			}
		}
	}

	UIElement::Type UIOk::get_type() const
	{
		return TYPE;
	}

	Button::State UIOk::button_pressed(uint16_t buttonid)
	{
		auto handler = okhandler;
		deactivate();

		switch (buttonid)
		{
		case Buttons::OK:
			handler(true);
			break;
		}

		return Button::State::NORMAL;
	}

	// ---------- UIAlarmInvite ----------

	void UIAlarmInvite::show(std::string message, std::function<void(bool yes)> handler, int variant)
	{
		if (MobileUI::enabled())
		{
			std::string eyebrow;
			MobileNoticeCard::invite_kind(message, eyebrow);
			Notifications::invite(title_case(eyebrow), MobileNoticeCard::clean(message), std::move(handler));

			return;
		}

		UI::get().emplace<UIAlarmInvite>(std::move(message), std::move(handler), variant);
	}

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

		std::string eyebrow;
		MobileNoticeCard::invite_kind(message_text, eyebrow);
		NotificationCenter::get().push(title_case(eyebrow), MobileNoticeCard::clean(message_text), handler, true);

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
		: UINoticeFrame(Point<int16_t>(0, 0))
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

		if (MobileUI::enabled())
		{
			use_card(Kind::DEATH, "YOU DIED", "You have died.\nYou'll go back to the nearest town.");
			card.add(Buttons::OK, "Return to town", Style::PRIMARY);
		}

		Sound(Sound::Name::DLGNOTICE).play();
	}

	void UIDeathNotice::draw(float alpha) const
	{
		if (mobile)
			return card.draw();

		// The UIWindow.img/Notice tombstone sprite already has its message
		// baked in, so we don't draw our own text over it.
		backdrop.draw(DrawArgument(position));
		UIElement::draw(alpha);
	}

	void UIDeathNotice::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && (keycode == KeyAction::Id::RETURN || escape))
		{
			auto handler = okhandler;
			deactivate();
			handler(true);
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
			auto handler = okhandler;
			deactivate();
			handler(true);
		}
		return Button::State::NORMAL;
	}
}