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

#include "../UIElement.h"

#include "../Components/Slider.h"
#include "../Components/Textfield.h"

#include "../../Graphics/Animation.h"
#include "../../Graphics/Text.h"

#include <set>
#include <vector>

namespace ms
{
	class UINpcTalk : public UIElement
	{
	public:
		// msgType layout per the actual server (Cosmic / HeavenMS,
		// confirmed by reading the wire bytes the server emits):
		//  0  sendOk(text)              — text + [OK]
		//  1  sendYesNo(text)           — text + [Yes] [No]
		//  2  sendGetText(...)          — text input field
		//  3  sendGetNumber(...)        — number input slider
		//  4  sendSimple(text)          — text with #L0#..#l selection links
		//  5  sendNext(text)            — text + [Next]
		//  6  sendNextPrev(text)        — text + [Back] [Next]
		// 12  sendAcceptDecline(text)   — text + [Accept] [Decline]
		enum TalkType : int8_t
		{
			NONE = -1,
			SENDOK = 0,
			SENDYESNO = 1,
			SENDGETTEXT = 2,
			SENDGETNUMBER = 3,
			SENDSIMPLE = 4,
			SENDNEXT = 5,
			SENDNEXTPREV = 6,
			SENDACCEPTDECLINE = 12,
			LENGTH = 13
		};

		static constexpr Type TYPE = UIElement::Type::NPCTALK;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UINpcTalk();

		// NPC text with its # macros filled in (names) or kept (icons).
		static std::string format_text(const std::string& tx, const int32_t& npcid);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		void send_scroll(double yoffset) override;

		UIElement::Type get_type() const override;

		void change_text(int32_t npcid, int8_t msgtype, int16_t style, int8_t speaker, const std::string& text);

		// Extra payload setters — call after change_text based on msgtype.
		void set_number_bounds(int32_t def, int32_t lo, int32_t hi);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		bool is_valid_type(int8_t value);
		std::string parse_simple_selections(const std::string& tx);

		static constexpr int16_t MAX_HEIGHT = 248;
		// Pixels the body text scrolls per slider row (one text line). The old code
		// stepped 400px/row — far bigger than the ~230px viewport — so scrolling
		// skipped straight past the middle of the text.
		static constexpr int16_t SCROLL_STEP = 16;

		// v83 UIWindow.img/UtilDlgEx geometry. The window is t (529x28) over
		// `height` px of the tiled c (529x20) over s (529x58). The white text
		// panel spans x 152..515 from y 23 (inside t) to PANEL_BOTTOM_INSET px
		// into s; the NPC portrait column is x 13..147; s carries the blue
		// button band 28..52 px into it.
		static constexpr int16_t TEXT_X = 166;               // body text left edge, 14px into the panel
		static constexpr int16_t TEXT_Y = 48;                // body text top for the non-scrolling layout
		static constexpr int16_t TEXT_WIDTH = 320;           // wrap width of the body text
		static constexpr int16_t LINE_HEIGHT = 16;           // A12M row height
		static constexpr int16_t SCROLL_TEXT_Y = 30;         // scrolling layout: text starts just under the header piece
		static constexpr int16_t SCROLL_CLIP_TOP = 28;       // visible band starts where the c tiles start
		static constexpr int16_t SCROLL_CLIP_BOTTOM_PAD = 8; // ...and ends this far below the last tile (5px above the panel border)
		static constexpr int16_t PANEL_BOTTOM_INSET = 13;    // the panel continues this far into s
		static constexpr int16_t SLIDER_X = 501;             // 15px VScr4 bar just inside the panel's right edge (516)
		static constexpr int16_t SLIDER_BUTTON_H = 13;       // VScr4 prev / next height
		static constexpr int16_t BUTTON_BAND_Y = 31;         // 18px buttons centred in the blue band of s (rows 28..52)
		static constexpr int16_t BTN_CLOSE_X = 9;            // 85px End Chat on the band's left
		static constexpr int16_t BTN_RIGHT_X = 471;          // right-aligned 46px button (OK / Next / No)
		static constexpr int16_t BTN_LEFT_X = 419;           // the 46px button left of it (Prev / Yes), 6px gap
		static constexpr int16_t BTN_QNO_X = 460;            // right-aligned 57px Decline
		static constexpr int16_t BTN_QYES_X = 397;           // 57px Accept left of it, 6px gap
		static constexpr int16_t SELECTION_PAD_X = 36;       // bullet column / hover band left of the text
		static constexpr int16_t HIGHLIGHT_W = 360;          // hover band width
		static constexpr int16_t INPUT_FIELD_H = 18;         // text / number entry field
		static constexpr int16_t INPUT_FIELD_RIGHT = 460;
		static constexpr int16_t INPUT_FIELD_BOTTOM_PAD = 5; // field bottom clears the panel border by this

		enum Buttons
		{
			CLOSE,
			NEXT,
			NO,
			OK,
			PREV,
			QNO,
			QYES,
			YES,
			QGIVEUP
		};

		// Kind of quest icon to draw next to a SENDSIMPLE option row.
		// Set during parse_simple_selections based on markers in the
		// option text (Start/Complete/etc.) sent by the server.
		enum class QuestMark : int8_t
		{
			NONE = -1,
			AVAILABLE = 0, // quest the player can start
			IN_PROGRESS = 1,
			COMPLETE  = 2  // quest the player can turn in
		};

		struct Selection
		{
			int32_t index;
			std::string text;
			int16_t line;
			std::string prefix; // rendered text before this option (for measuring visual-line Y)
			QuestMark mark = QuestMark::NONE;
		};

		Texture top;
		Texture fill;
		Texture bottom;
		Texture nametag;
		Texture speaker;
		Texture dot_normal;
		Texture dot_hovered;
		Texture list_normal;
		Texture list_hovered;

		// Quest-state icons drawn next to SENDSIMPLE option rows when the
		// server tags the option as (Start) / (Complete) / (In progress).
		// Loaded from UI.nx/UIWindow.img/QuestIcon/{0,1,2}.
		Animation mark_available;
		Animation mark_in_progress;
		Animation mark_complete;

		Text text;
		Text name;

		int16_t height;
		int16_t offset;
		int16_t unitrows;
		int16_t rowmax;
		int16_t min_height;

		bool show_slider;
		bool draw_text;
		Slider slider;
		TalkType type;
		std::string formatted_text;
		size_t formatted_text_pos;
		uint16_t timestep;

		std::vector<Selection> selections;
		int16_t selection_top;
		int32_t hovered_selection;

		// Hover pulse animation — ticks each frame so the highlight on
		// the currently-hovered selection row breathes instead of being
		// a flat color block.
		uint32_t hover_pulse_tick = 0;

		Textfield input_field;

		// msgType 12 — sendGetNumber bounds
		int32_t num_default = 0;
		int32_t num_min = 0;
		int32_t num_max = 0;

		// NPC id for the active dialog. Used by the quest-progress
		// banner so we can look up which in-progress quest this NPC
		// is start/end for and render the requirements footer.
		int32_t current_npcid = 0;

		// Quest-progress banner state. When this NPC has one or more
		// in-progress quests the dialog draws a clickable list_normal
		// sprite per quest below the main text (5 px between each).
		// Click a banner to flip just that quest's body from text to
		// requirements; click again to flip back. Multiple banners
		// can be expanded simultaneously.
		struct QuestBannerHit
		{
			int16_t qid;
			Rectangle<int16_t> rect;
		};
		mutable std::vector<QuestBannerHit> quest_banner_hits;
		int16_t quest_banner_hovered_qid = -1;
		std::set<int16_t> expanded_quest_ids;

		std::function<void(bool)> onmoved;
	};
}