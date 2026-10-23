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
#include "../Messages.h"

#include "../Components/Slider.h"
#include "../Components/Textfield.h"

#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>
#include <unordered_map>

namespace ms
{
	// Chat input line + chat log, rendered from the v83 UI.nx assets. The
	// white input band is part of StatusBar.img/base/backgrnd2 (drawn by
	// UIStatusBar); this element draws the target selector, the text field,
	// the open/close toggle and, when open, the log above the bar.
	class UIChatBar : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CHATBAR;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		enum ChatTarget : uint8_t
		{
			CHT_ALL,
			CHT_BUDDY,
			CHT_GUILD,
			CHT_ALLIANCE,
			CHT_PARTY,
			CHT_SQUAD,
			NUM_TARGETS
		};

		enum LineType
		{
			UNK0,
			WHITE,
			RED,
			BLUE,
			YELLOW,
			PINK,
			LIGHTBLUE,
			GREEN,
			// Channel-chat colors mandated by the v83 protocol -
			// Buddy chat reads orange, Alliance chat reads as a soft
			// light-green band; both are distinct from PINK (party)
			// and GREEN (guild).
			ORANGE,
			LIGHTGREEN,
			// Dedicated megaphone line styles - render a tiny megaphone
			// icon (UIWindow.img/Megaphone/0..3) before the text so these
			// broadcasts are visually distinct from regular chat.
			MEGAPHONE,        // Cheap / Regular  (5070 / 5071) - icon 0
			SUPER_MEGAPHONE,  // Super            (5072)        - icon 1
			ITEM_MEGAPHONE,   // Item             (5076)        - icon 2
			TRIPLE_MEGAPHONE  // Triple           (5077)        - icon 3
		};

		struct PartyMember
		{
			int32_t id = 0;
			std::string name;
			int32_t job_id = 0;
			int32_t level = 0;
			int32_t channel = -2;
			int32_t map_id = 0;
			int32_t hp = 0;
			int32_t max_hp = 0;
		};

		// `position` is accepted for compatibility with the existing
		// emplace call; the bar re-anchors itself to UIStatusBar::bar_origin()
		// so the input line always sits on the status bar art.
		UIChatBar(Point<int16_t> position);

		void draw(float inter) const override;
		void update() override;
		void set_position(Point<int16_t> pos);

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		void send_scroll(double yoffset) override;

		bool is_in_range(Point<int16_t> cursorpos) const override;

		// Windows-style chat-log text selection (drag to highlight, Ctrl+C to copy).
		std::string get_selected_text() const;
		void clear_selection();
		Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		void send_line(const std::string& line, LineType type);
		void send_chatline(const std::string& line, LineType type);
		void display_message(Messages::Type line, UIChatBar::LineType type);
		void focus_chatfield();
		void toggle_chat();
		void toggle_chatfield();
		void toggle_chatfield(bool open);
		bool is_chatopen();
		bool is_chatfieldopen();

		// Lines that arrived while the phone's chat window was closed.
		uint16_t unread() const;

		void set_chat_target(ChatTarget target);
		void cycle_chat_target();

		void set_pending_party_invite(int32_t party_id, const std::string& inviter);
		void clear_pending_party_invite();
		void set_party_state(int32_t party_id, int32_t leader_id, const std::vector<PartyMember>& members);
		void clear_party_state();
		void set_party_leader(int32_t leader_id);
		void update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp);
		int32_t get_party_id() const;
		int32_t get_party_leader_id() const;
		int32_t get_pending_party_invite_id() const;
		const std::string& get_pending_party_inviter() const;
		const std::vector<PartyMember>& get_party_members() const;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void set_chat_open(bool open);
		// Sends what the input line holds; false when it was empty.
		bool submit_chatfield();
		int32_t resolve_party_member_id(const std::string& token) const;
		bool handle_party_command(const std::string& message);
		void send_chat_message(const std::string& message);
		void send_targeted_message(const std::string& target, const std::string& message);
		void send_party_message(const std::string& message);
		// Top of the chat log's text box (screen space).
		int16_t getchattop() const;
		// Top-left of the chat log's text box (screen space).
		Point<int16_t> log_origin() const;
		// Refreshes the "To: ..." caption after the target changes.
		void refresh_target_label();

		enum Buttons : uint16_t
		{
			BT_OPENCHAT,     // Basic.img/BtUP   - expand the log
			BT_CLOSECHAT,    // Basic.img/BtDown - collapse the log
			BT_CHATTARGETS   // Basic.img/BtComboBox - cycle the chat target
		};

		// Width of the log = width of StatusBar.img/base/chat, the 566x5
		// strip that caps the log and doubles as its resize handle.
		static constexpr int16_t CHATLOG_W = 566;
		static constexpr int16_t CHATSTRIP_H = 5;
		// Gap between the log's text box and the top of the status bar.
		static constexpr int16_t CHATYOFFSET = 1;
		static constexpr int16_t CHATROWHEIGHT = 16;
		static constexpr int16_t MAXCHATROWS = 16;
		static constexpr int16_t MINCHATROWS = 1;
		static constexpr time_t MESSAGE_COOLDOWN = 1000;

		Textfield chatfield;
		Texture chat_strip;   // StatusBar.img/base/chat
		Texture target_box;   // StatusBar.img/base/chatTarget
		Text target_label;

		bool chatopen;
		bool chatfieldopen;
		ChatTarget chattarget;
		int32_t party_id;
		int32_t party_leader_id;
		int32_t pending_party_invite_id;
		std::string pending_party_inviter;
		std::vector<PartyMember> party_members;

		EnumMap<Messages::Type, time_t> message_cooldowns;
		std::vector<std::string> lastentered;
		size_t lastpos;

		std::unordered_map<int16_t, Text> rowtexts;
		// Raw line string per row, kept so a selection can be copied to the clipboard.
		std::unordered_map<int16_t, std::string> rowstrings;
		static constexpr int16_t ROW_NONE = -30000;
		int16_t row_at(Point<int16_t> cursorpos) const;
		int16_t col_at(int16_t rowid, int16_t x) const;
		// Selection anchor/focus as (row id, character column). Ordered into
		// start/end by (row, col) reading order for highlight + copy.
		void ordered_sel(int16_t& sr, int16_t& sc, int16_t& er, int16_t& ec) const;
		int16_t sel_arow = 0, sel_acol = 0;
		int16_t sel_frow = 0, sel_fcol = 0;
		bool has_selection = false;
		bool sel_dragging = false;
		// Per-row icon id (0..3 from UIWindow.img/Megaphone) painted before
		// the text on megaphone-broadcast rows. -1 = no icon.
		std::unordered_map<int16_t, int8_t> rowicons;
		// For megaphone rows we split "Name : message" into two Text
		// fragments so the icon can sit between them.
		struct MegaRow
		{
			Text namepart;
			Text msgpart;
			int8_t icon = -1;
		};
		std::unordered_map<int16_t, MegaRow> mega_rows;
		// Cached megaphone icon textures loaded once in the constructor.
		Texture megaphone_icons[4];
		ColorBox chatbox;
		int16_t chatrows;
		int16_t rowpos;
		int16_t rowmax;
		Slider slider;
		bool dragchattop;

		// ---- The phone's chat window (UIChatBarMobile.cpp) ----
		// v83's chat line lives on the status bar, which a phone does not
		// show. There a touch button opens a see-through window on the left
		// instead: the log, the chat target and an input line. `chatopen`
		// says whether it is showing.
		bool mobile = false;

		struct MobileLine
		{
			Text text;
			Color color;
			int16_t height;
		};

		std::deque<MobileLine> mobile_lines;
		// How far the log is scrolled up from its newest line, in pixels.
		int16_t mobile_scroll = 0;
		uint16_t mobile_unread = 0;
		bool mobile_tracking = false;
		bool mobile_dragging = false;
		int32_t mobile_target = -1;
		uint32_t mobile_press = 0;
		Point<int16_t> mobile_start;
		Point<int16_t> mobile_last;

		static int16_t mobile_input_width();
		Rectangle<int16_t> mobile_window() const;
		Rectangle<int16_t> mobile_log() const;
		Rectangle<int16_t> mobile_input() const;
		Rectangle<int16_t> mobile_send() const;
		Rectangle<int16_t> mobile_target_chip() const;
		Rectangle<int16_t> mobile_close() const;
		int16_t mobile_content() const;
		void mobile_clamp_scroll();
		int32_t mobile_hit(Point<int16_t> pos) const;
		void mobile_add_line(const std::string& line, LineType type);
		void mobile_draw(float inter) const;
		Cursor::State mobile_cursor(bool clicking, Point<int16_t> pos);
	};

	// Convenience namespace for the very common
	//   `if (auto cb = UI::get().get_element<UIChatBar>())
	//        cb->send_chatline(text, type);`
	// pattern. Null-safe - silently no-ops if the chat bar isn't
	// present (e.g. on the title screen).
	namespace chat
	{
		using LineType = UIChatBar::LineType;
		void log(const std::string& line, LineType type = LineType::YELLOW);
	}
}
