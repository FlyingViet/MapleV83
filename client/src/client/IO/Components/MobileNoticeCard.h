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

#include "../MobileUI.h"

#include <string>
#include <vector>

namespace ms
{
	// The phone's look for notices. Dialogs (OK, yes/no, an amount, text,
	// waiting, death) are a card in the middle of a dimmed screen; invites and
	// alerts are a banner across the top that leaves the game playable. This
	// lays out, draws and hit-tests the card; the element that owns it keeps
	// its own behaviour and gets the tapped action's id back.
	class MobileNoticeCard
	{
	public:
		enum class Layout { DIALOG, BANNER };

		enum class Kind
		{
			INFO, QUESTION, AMOUNT, TEXT, WAIT, DEATH,
			PARTY, TRADE, BUDDY, GUILD, MESSENGER, FAMILY, QUEST
		};

		// What a touch did.
		enum class Tap { NOTHING, OUTSIDE, FIELD, ACTION };

		struct Touch
		{
			Tap tap = Tap::NOTHING;
			int32_t id = -1;
		};

		static constexpr int32_t NONE = -1;
		static constexpr int32_t FIELD = -2;
		// The amount stepper around a number field.
		static constexpr int32_t STEP_DOWN = -10;
		static constexpr int32_t STEP_UP = -11;
		static constexpr int32_t STEP_MAX = -12;

		void setup(Layout layout, Kind kind, const std::string& eyebrow, const std::string& message);

		// Buttons along the bottom (dialog) or the right (banner), left to
		// right: put the main action last.
		void add(int32_t id, const std::string& label, MobileUI::Style style);

		// A row for a text field above the buttons; with a stepper the field
		// sits between - and +, with Max at the end.
		void add_field(bool stepper);

		// The banner's time-left bar; below zero hides it.
		void set_progress(float fraction);

		// Moves a banner down, under another one already showing.
		void set_top(int16_t top);

		MobileUI::Rect card() const;
		MobileUI::Rect field_rect() const;

		bool contains(Point<int16_t> pos) const;
		int32_t hit(Point<int16_t> pos) const;

		// Press and release, as UIElement::send_cursor sees them. An action
		// fires on release over the same button; the field and a press
		// outside the card report on the press.
		Touch touch(bool down, Point<int16_t> pos);

		// Call from update(): forgets a press whose finger lifted elsewhere.
		void update();

		// Dialogs dim the whole screen behind the card.
		void draw(bool field_focused = false) const;

		Layout get_layout() const { return layout; }
		Kind get_kind() const { return kind; }

		// Server text without "\r" or the #b/#k colour codes the v83 notice
		// box understood.
		static std::string clean(const std::string& message);

		// What an invite is, from its wording ("... invited you to their
		// party."), and the heading that goes with it.
		static Kind invite_kind(const std::string& message, std::string& eyebrow);
		// The same for a toast or inbox entry's title ("Quest Ready").
		static Kind title_kind(const std::string& title);

		static MobileUI::RGBA tint(Kind kind);
		static const char* mark(Kind kind);
		static void badge(const MobileUI::Rect& r, Kind kind);

	private:
		struct Action
		{
			int32_t id;
			std::string label;
			MobileUI::Style style;
		};

		void relayout();
		MobileUI::Rect action_rect(size_t index) const;
		MobileUI::Rect step_rect(int32_t id) const;

		Layout layout = Layout::DIALOG;
		Kind kind = Kind::INFO;
		std::string eyebrow;
		std::vector<std::string> paragraphs;
		std::vector<Action> actions;
		MobileUI::Font font = MobileUI::Font::TITLE;
		bool field = false;
		bool stepper = false;
		float progress = -1.0f;
		int16_t top_override = -1;

		int16_t width = 0;
		int16_t height = 0;
		int16_t message_h = 0;
		int16_t text_w = 0;

		int32_t pressed = NONE;
		uint32_t press = 0;
	};
}
