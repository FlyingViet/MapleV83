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
#pragma once

#include "../UIDragElement.h"

#include "../Components/MobileNoticeCard.h"
#include "../Components/Textfield.h"

namespace ms
{
	// The v83 notice art on a computer. On a phone a notice is a
	// MobileNoticeCard over the dimmed screen instead: large text, full-width
	// buttons, and a tap outside the card answers like Escape.
	class UINoticeFrame : public UIDragElement<PosNOTICE>
	{
	public:
		void update() override;
		void update_screen(int16_t new_width, int16_t new_height) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		bool captures_hold() const override { return mobile; }

	protected:
		UINoticeFrame(Point<int16_t> dragarea) : UIDragElement<PosNOTICE>(dragarea) {}

		// Switches to the phone's card; call from the constructor.
		void use_card(MobileNoticeCard::Kind kind, const std::string& eyebrow, const std::string& message);

		// A card button was tapped; by default its id is the v83 button's.
		// It may close or replace this notice.
		virtual void card_action(int32_t id);
		virtual void card_field() {}
		// A tap on the dimmed screen: Escape, for notices that take one.
		virtual void card_outside();

		bool mobile = false;
		MobileNoticeCard card;
	};

	class UINotice : public UINoticeFrame
	{
	public:
		static constexpr Type TYPE = UIElement::Type::NOTICE;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

	protected:
		enum NoticeType : uint8_t
		{
			YESNO,
			ENTERNUMBER,
			OK
		};

		UINotice(std::string message, NoticeType type, Text::Alignment alignment);
		UINotice(std::string message, NoticeType type);

		void draw(bool textfield) const;

		// Kept for callers of the old Notice6 layout: false = the button row's
		// y, true = the text field's y (same as button_row_y / textfield_y).
		int16_t box2offset(bool textfield) const;
		// Top of the button row inside the bottom piece's grey strip
		int16_t button_row_y() const;
		// Top of the white text field baked into Basic.img/Notice4/s
		int16_t textfield_y() const;

		// v83 Basic.img/Notice3 (plain) and Notice4 (with text field) are
		// 266px wide t/c/s stacks: t is 21px with the blue panel starting 10px
		// down, c is a 20px blue tile, s closes the panel and carries a grey
		// button strip (Notice3: rows 12..51; Notice4: text field rows 5..19,
		// strip rows 35..71).
		static constexpr int16_t BODY_PAD = 6;          // blue space above/below the text
		static constexpr int16_t BODY_MIN_ROWS = 2;     // never fewer centre tiles than this
		static constexpr int16_t TOP_BLUE_Y = 10;       // blue panel starts here inside t
		static constexpr int16_t BOTTOM_BLUE_H = 11;    // blue rows at the top of Notice3/s
		static constexpr int16_t FIELD_BLUE_H = 4;      // blue rows above the field in Notice4/s
		static constexpr int16_t TEXT_WIDTH = 230;      // wrap width for left/centre text
		static constexpr int16_t TEXT_LEFT_X = 16;      // left-aligned text x
		static constexpr int16_t TEXT_CENTER_X = 133;   // centred text x (half of 266)
		static constexpr int16_t STRIP_Y = 12;          // grey strip top inside Notice3/s
		static constexpr int16_t STRIP_H = 40;
		static constexpr int16_t FIELD_STRIP_Y = 35;    // grey strip top inside Notice4/s
		static constexpr int16_t FIELD_STRIP_H = 37;
		static constexpr int16_t FIELD_Y = 5;           // white field top inside Notice4/s
		static constexpr int16_t FIELD_H = 15;
		static constexpr int16_t FIELD_LEFT = 22;       // white field x range (19..246) with a little inset
		static constexpr int16_t FIELD_RIGHT = 244;
		static constexpr int16_t BUTTON_H = 18;         // Basic.img/BtOK2 and BtCancel2 are 47x18
		static constexpr int16_t BUTTON_W = 47;
		static constexpr int16_t BTN_YES_X = 156;       // right-aligned OK / Cancel pair
		static constexpr int16_t BTN_NO_X = 210;
		static constexpr int16_t BTN_OK_X = 110;        // a lone OK button, centred

	private:
		Texture top;
		Texture center;
		Texture bottom;
		Text question;
		int16_t height;
		int16_t body_rows;
		NoticeType type;
		Text::Alignment alignment;
	};

	class UIYesNo : public UINotice
	{
	public:
		UIYesNo(std::string message, std::function<void(bool yes)> yesnohandler,
			Text::Alignment alignment,
			int16_t button_x_offset = 0,
			int16_t button_y_offset = 0);
		UIYesNo(std::string message, std::function<void(bool yes)> yesnohandler);

		void draw(float alpha) const override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		enum Buttons : int16_t
		{
			YES, NO
		};

		std::function<void(bool yes)> yesnohandler;
	};

	class UIEnterNumber : public UINotice
	{
	public:
		// The offsets nudge the field / buttons away from their Notice4 slots;
		// 0 puts them exactly on the baked-in art.
		UIEnterNumber(std::string message, std::function<void(int32_t number)> numhandler, int32_t max, int32_t quantity,
			int16_t field_y_offset = 0,
			int16_t button_x_offset = 0,
			int16_t button_y_offset = 0);

		~UIEnterNumber();

		void draw(float alpha) const override;
		void update() override;
		void deactivate() override;

		Cursor::State send_cursor(bool pressed, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;
		void card_action(int32_t id) override;
		void card_field() override;

	private:
		void handlestring(std::string numstr);
		void unfocus();

		enum Buttons : int16_t
		{
			OK, CANCEL
		};

		std::function<void(int32_t number)> numhandler;
		Textfield numfield;
		int32_t max;
	};

	class UIEnterText : public UINotice
	{
	public:
		UIEnterText(std::string message, std::function<void(const std::string& text)> texthandler, int32_t maxlength = 15,
			int16_t field_y_offset = 0, int16_t button_x_offset = 0, int16_t button_y_offset = 0);

		~UIEnterText();

		void draw(float alpha) const override;
		void update() override;
		void deactivate() override;

		Cursor::State send_cursor(bool pressed, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;
		void card_field() override;

	private:
		void unfocus();

		enum Buttons : int16_t
		{
			OK, CANCEL
		};

		std::function<void(const std::string& text)> texthandler;
		Textfield textfield;
	};

	// Wide 286x146 modal used specifically for the "You have died"
	// popup. Sources the backdrop from UIWindow.img/Notice/0 so the
	// death dialog reads as a proper standalone panel instead of the
	// small generic Basic.img/Notice3 frame used by UIOk.
	class UIDeathNotice : public UINoticeFrame
	{
	public:
		static constexpr Type TYPE = UIElement::Type::NOTICE;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIDeathNotice(std::string message, std::function<void(bool ok)> okhandler);

		void draw(float alpha) const override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;
		// Only the button revives.
		void card_outside() override {}

	private:
		enum Buttons : int16_t { OK };

		std::function<void(bool ok)> okhandler;
		Texture backdrop;
		Text question;
	};

	// Small v83 "invite banner" that appears above the notice button.
	// Sourced from UIWindow.img/FadeYesNo — a single flat backdrop with
	// a type-icon and tiny OK/Cancel buttons. 12 backdrop/icon variants
	// (backgrnd..backgrnd11 / icon0..icon11) correspond to the
	// different invite types in v83.
	class UIAlarmInvite : public UIDragElement<PosNOTICE>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::NOTICE;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		// variant  = which backgrnd/icon pair to use (0..11).
		UIAlarmInvite(std::string message,
			std::function<void(bool yes)> handler,
			int variant = 0);

		// Asks the player. A phone shows a banner across the top instead
		// (Notifications::invite), so the game stays playable and a dialog
		// opening meanwhile can't replace the question.
		static void show(std::string message, std::function<void(bool yes)> handler, int variant = 0);

		void draw(float alpha) const override;
		void update() override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		// Move this popup into the notification drawer (bell) and close it.
		void stash();

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		enum Buttons : int16_t { ACCEPT, DECLINE };

		std::function<void(bool yes)> handler;
		Texture backdrop;
		Texture icon;
		Text label;
		// An invite needs long enough to actually read and answer; the
		// 5s toast timing was fine for informational popups but far too
		// short for anything with Accept/Decline on it.
		static constexpr int32_t HOLD_MS = 30000;

		std::string message_text;
		int32_t age_ms = 0;
		bool stashed = false;
	};

	// A button-less notice window used for "Waiting for partner..."
	// and similar blocking-but-passive status prompts. The owner has
	// to remove it explicitly via UI::remove(UIElement::Type::NOTICE).
	class UIWaitNotice : public UINotice
	{
	public:
		UIWaitNotice(std::string message);

		void draw(float alpha) const override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t) override { return Button::State::NORMAL; }
	};

	class UIOk : public UINotice
	{
	public:
		UIOk(std::string message, std::function<void(bool ok)> okhandler);

		void draw(float alpha) const override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		enum Buttons : int16_t
		{
			OK
		};

		std::function<void(bool ok)> okhandler;
	};
}