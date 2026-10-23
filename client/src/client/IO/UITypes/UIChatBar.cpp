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
#include "UIChatBar.h"

#include "../Window.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../Components/ChatBalloon.h"
#include "UIStatusBar.h"

#include "../Notifications.h"

#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/InventoryPackets.h"
#include "../../Net/Packets/MessagingPackets.h"
#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Character/Party.h"

#include <algorithm>
#include <cctype>
#include <list>
#include <sstream>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		std::string trim(const std::string& value)
		{
			size_t first = value.find_first_not_of(' ');
			if (first == std::string::npos)
				return "";

			size_t last = value.find_last_not_of(' ');
			return value.substr(first, last - first + 1);
		}

		std::string lowercase(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return value;
		}

		bool parse_int32(const std::string& value, int32_t& out)
		{
			if (value.empty())
				return false;

			size_t start = 0;
			if (value[0] == '+' || value[0] == '-')
				start = 1;

			for (size_t i = start; i < value.size(); i++)
				if (!std::isdigit(static_cast<unsigned char>(value[i])))
					return false;

			try { out = std::stoi(value); }
			catch (...) { return false; }
			return true;
		}

		// ==================================================================
		// v83 layout, in pixels relative to the status bar's top-left
		// (UIStatusBar::bar_origin()). The white chat input band painted by
		// StatusBar.img/base/backgrnd2 occupies rows 7..28, x 0..565; the
		// status bar's own BtWhisper (12x19) takes x 3..14 of it.
		// ==================================================================

		// base/chatTarget (81x20) right after BtWhisper, vertically centred
		// in the 22 px band.
		constexpr int16_t TARGET_X = 17;
		constexpr int16_t TARGET_Y = 8;
		constexpr int16_t TARGET_W = 81;
		constexpr int16_t TARGET_H = 20;
		// Basic.img/BtComboBox (17x16) as the "cycle target" arrow, glued to
		// the box's right edge.
		constexpr int16_t COMBO_X = 100;
		constexpr int16_t COMBO_Y = 10;
		// Text field fills the rest of the band up to the open/close toggle.
		constexpr int16_t FIELD_X = 120;
		constexpr int16_t FIELD_Y = 9;
		constexpr int16_t FIELD_RIGHT = 545;
		constexpr int16_t FIELD_BOTTOM = 27;
		// Basic.img/BtUP / BtDown (12x12) at the band's right end (band art
		// ends at x 565; base/box starts at 572).
		constexpr int16_t TOGGLE_X = 549;
		constexpr int16_t TOGGLE_Y = 12;
		// Extent of the band for hit-testing.
		constexpr int16_t BAND_TOP = 7;
		constexpr int16_t BAND_BOTTOM = 29;

		// Basic.img/VScr4 pieces are 15 px wide; keep the slider inside the
		// log's right edge with a 1 px margin.
		constexpr int16_t SLIDER_W = 16;
		// Slider top sits just under the strip.
		constexpr int16_t SLIDER_Y = 3;
		// Left inset of the log text.
		constexpr int16_t TEXT_X = 4;
		// Log text wraps before the slider column.
		constexpr uint16_t TEXT_MAXWIDTH = 540;
		constexpr uint16_t TEXT_MAXWIDTH_ICON = 524;
	}

	UIChatBar::UIChatBar(Point<int16_t> pos) : UIElement(UIStatusBar::bar_origin(), Point<int16_t>(CHATLOG_W, UIStatusBar::BAR_HEIGHT))
	{
		(void)pos; // re-anchored to the status bar art, see update()

		chatopen = false;
		chatfieldopen = false;
		chattarget = CHT_ALL;
		party_id = -1;
		party_leader_id = -1;
		pending_party_invite_id = -1;
		chatrows = 4;
		rowpos = 0;
		rowmax = -1;
		lastpos = 0;
		dragchattop = false;

		nl::node base = nl::nx::ui["StatusBar.img"]["base"];
		nl::node basic = nl::nx::ui["Basic.img"];

		// v83 has no dedicated chat open/close/target buttons; the generic
		// Basic.img buttons (all with normal/mouseOver/pressed/disabled)
		// stand in for them.
		buttons[BT_OPENCHAT] = std::make_unique<MapleButton>(basic["BtUP"], Point<int16_t>(TOGGLE_X, TOGGLE_Y));
		buttons[BT_CLOSECHAT] = std::make_unique<MapleButton>(basic["BtDown"], Point<int16_t>(TOGGLE_X, TOGGLE_Y));
		buttons[BT_CHATTARGETS] = std::make_unique<MapleButton>(basic["BtComboBox"], Point<int16_t>(COMBO_X, COMBO_Y));

		chat_strip = Texture(base["chat"]);
		target_box = Texture(base["chatTarget"]);
		target_label = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);
		refresh_target_label();

		// Small megaphone icons (UIWindow.img/Megaphone/0..3, 13x12 each)
		// painted before megaphone / super / item / triple chat lines.
		nl::node mega_icons = nl::nx::ui["UIWindow.img"]["Megaphone"];
		for (int i = 0; i < 4; i++)
			megaphone_icons[i] = Texture(mega_icons[std::to_string(i)]);

		chatbox = ColorBox(CHATLOG_W, 1 + chatrows * CHATROWHEIGHT, Color::Name::BLACK, 0.6f);

		// A12M so the typed text sits centred in the 22 px input band.
		chatfield = Textfield(Text::A12M, Text::LEFT, Color::Name::BLACK,
			Rectangle<int16_t>(Point<int16_t>(FIELD_X, FIELD_Y), Point<int16_t>(FIELD_RIGHT, FIELD_BOTTOM)), 0);

		set_chat_open(chatopen);

		chatfield.set_enter_callback(
			[&](std::string msg)
			{
				msg = trim(msg);
				if (msg.empty())
				{
					// Empty Enter minimizes the whole chat window.
					set_chat_open(false);
					return;
				}

				if (!handle_party_command(msg))
					send_chat_message(msg);

				lastentered.push_back(msg);
				lastpos = lastentered.size();
				chatfield.change_text("");
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::UP,
			[&]()
			{
				if (lastpos > 0)
				{
					lastpos--;
					chatfield.change_text(lastentered[lastpos]);
				}
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::DOWN,
			[&]()
			{
				if (lastentered.size() > 0 && lastpos < lastentered.size() - 1)
				{
					lastpos++;
					chatfield.change_text(lastentered[lastpos]);
				}
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				toggle_chatfield(false);
			}
		);

		// Basic.img/VScr4 (the StatusBar2 chat scrollbar does not exist in v83).
		slider = Slider(Slider::Type::NORMAL_CALYPSO, Range<int16_t>(0, CHATROWHEIGHT * chatrows - 14), CHATLOG_W - SLIDER_W, chatrows, 1,
			[&](bool up)
			{
				int16_t next = up ? rowpos - 1 : rowpos + 1;
				if (next >= 0 && next <= rowmax)
					rowpos = next;
			}
		);

		send_line("[Welcome] Welcome to MapleStory!", YELLOW);
	}

	void UIChatBar::set_chat_open(bool open)
	{
		chatopen = open;
		buttons[BT_OPENCHAT]->set_active(!open);
		buttons[BT_CLOSECHAT]->set_active(open);
		buttons[BT_CHATTARGETS]->set_active(open);

		if (!open)
		{
			chatfieldopen = false;
			chatfield.set_state(Textfield::State::DISABLED);
		}
		else
		{
			chatfield.set_state(Textfield::State::NORMAL);
		}
	}

	void UIChatBar::refresh_target_label()
	{
		static const char* const names[NUM_TARGETS] = {
			"To: All", "To: Buddy", "To: Guild", "To: Ally", "To: Party", "To: Squad"
		};

		if (chattarget >= CHT_ALL && chattarget < NUM_TARGETS)
			target_label.change_text(names[chattarget]);
	}

	void UIChatBar::focus_chatfield()
	{
		if (!chatopen)
			set_chat_open(true);

		chatfieldopen = true;
		chatfield.set_state(Textfield::State::FOCUSED);
	}

	Point<int16_t> UIChatBar::log_origin() const
	{
		return Point<int16_t>(position.x(), getchattop());
	}

	void UIChatBar::draw(float inter) const
	{
		// Target selector always shows on the input band (the band art itself
		// is part of the status bar's backgrnd2).
		if (target_box.is_valid())
			target_box.draw(DrawArgument(position + Point<int16_t>(TARGET_X, TARGET_Y)));

		target_label.draw(position + Point<int16_t>(TARGET_X + TARGET_W / 2, TARGET_Y + 3));

		UIElement::draw_buttons(inter);

		if (chatopen)
		{
			Point<int16_t> lo = log_origin();
			int16_t chattop = lo.y();

			// The 566x5 strip caps the log and is the resize handle.
			if (chat_strip.is_valid())
				chat_strip.draw(DrawArgument(lo - Point<int16_t>(0, CHATSTRIP_H)));

			chatbox.draw(DrawArgument(lo));

			int16_t chatbottom = chattop + chatrows * CHATROWHEIGHT;
			Range<int16_t> chatclip(chattop + 2, chatbottom);

			int16_t chatheight = CHATROWHEIGHT * chatrows;
			int16_t yshift = -chatheight;
			int16_t textx = lo.x() + TEXT_X;

			for (int16_t i = 0; i < chatrows; i++)
			{
				int16_t rowid = rowpos - i;

				if (!rowtexts.count(rowid))
					break;

				int16_t lines_here = rowtexts.at(rowid).height() / CHATROWHEIGHT;
				if (lines_here < 1)
					lines_here = 1;
				int16_t textheight = lines_here;

				while (textheight > 0)
				{
					yshift += CHATROWHEIGHT;
					textheight--;
				}

				int16_t msgy = chattop - yshift - 7;

				if (msgy < chattop)
					continue;

				// Megaphone rows: paint a colored strip ALIGNED WITH the
				// actual text row (so it sits behind the glyphs, not
				// above or below), then draw name -> icon -> message.
				auto mega_it = mega_rows.find(rowid);
				if (mega_it != mega_rows.end())
				{
					const MegaRow& mr = mega_it->second;

					Color::Name bg_color = Color::Name::WHITE;
					switch (mr.icon)
					{
					case 0: bg_color = Color::Name::BLUE;   break; // regular
					case 1: bg_color = Color::Name::PINK;   break; // super
					case 2: bg_color = Color::Name::GREEN;  break; // item
					case 3: bg_color = Color::Name::YELLOW; break; // triple
					}

					// A12M glyph top sits at msgy + (linespace - bearing) ~
					// msgy+5; bottom ~ msgy+17. Strip matches that band
					// exactly so it sits BEHIND the glyphs only, and uses
					// a softer alpha so the text stays legible.
					int16_t strip_top = msgy + 5;
					int16_t strip_h   = 13;
					ColorBox row_bg(CHATLOG_W - 6, strip_h, bg_color, 0.18f);
					row_bg.draw(DrawArgument(Point<int16_t>(lo.x() + 2, strip_top)));

					int16_t x = textx;

					if (!mr.namepart.empty())
					{
						mr.namepart.draw(
							DrawArgument(Point<int16_t>(x, msgy)), chatclip);
						x += mr.namepart.width() + 2;
					}

					// Center the 12-tall megaphone icon over the strip.
					if (mr.icon >= 0 && mr.icon < 4
						&& megaphone_icons[mr.icon].is_valid())
					{
						int16_t icon_y = strip_top + (strip_h - 12) / 2;
						megaphone_icons[mr.icon].draw(
							DrawArgument(Point<int16_t>(x, icon_y)));
						x += megaphone_icons[mr.icon].width() + 2;
					}

					mr.msgpart.draw(
						DrawArgument(Point<int16_t>(x, msgy)), chatclip);

					ChatBalloon::draw_inline_images(
						mr.msgpart, Point<int16_t>(x, msgy), chatclip);
				}
				else
				{
					rowtexts.at(rowid).draw(
						DrawArgument(Point<int16_t>(textx, msgy)), chatclip);

					// Text::draw paints glyphs only; inline #v/#i/#q/#s/#f/#e
					// sprites are a separate overlay, which the chat window
					// was never doing -- so emoticons showed in speech
					// balloons but not in the chat log.
					ChatBalloon::draw_inline_images(
						rowtexts.at(rowid), Point<int16_t>(textx, msgy), chatclip);
				}

				if (has_selection)
				{
					int16_t sr, sc, er, ec;
					ordered_sel(sr, sc, er, ec);

					if (rowid >= sr && rowid <= er)
					{
						auto rit = rowstrings.find(rowid);
						int16_t len = rit != rowstrings.end()
							? static_cast<int16_t>(rit->second.size()) : 0;
						int16_t from = (rowid == sr) ? sc : 0;
						int16_t to = (rowid == er) ? ec : len;
						if (to < from) to = from;

						int16_t x0 = textx + static_cast<int16_t>(rowtexts.at(rowid).advance(from));
						int16_t x1 = textx + static_cast<int16_t>(rowtexts.at(rowid).advance(to));
						int16_t w = x1 - x0;

						if (w > 0)
						{
							// Hug the glyph band (A12M top ~ msgy+5, bottom ~ msgy+18)
							// so the highlight sits on the text, and keep it bold.
							ColorBox cover(w, 13, Color::Name::WHITE, 0.55f);
							cover.draw(DrawArgument(Point<int16_t>(x0, msgy + 5)));
						}
					}
				}
			}

			slider.draw(Point<int16_t>(lo.x(), chattop + SLIDER_Y));

			chatfield.draw(position);
		}
		else if (rowtexts.count(rowmax))
		{
			// Collapsed: keep the newest line floating just above the bar so
			// incoming chat is never missed.
			rowtexts.at(rowmax).draw(position + Point<int16_t>(TEXT_X, -20));
		}
	}

	void UIChatBar::update()
	{
		// Follow the status bar art (handles view resizes).
		position = UIStatusBar::bar_origin();

		UIElement::update();
		chatfield.update(position);
	}

	void UIChatBar::set_position(Point<int16_t> pos)
	{
		position = pos;
	}

	void UIChatBar::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (keycode == KeyAction::Id::RETURN)
			{
				toggle_chatfield();
			}
			else if (escape)
				toggle_chatfield(false);
		}
	}

	void UIChatBar::send_scroll(double yoffset)
	{
		if (!chatopen)
			return;

		if (yoffset > 0)
		{
			if (rowpos > 0)
				rowpos--;
		}
		else if (yoffset < 0)
		{
			if (rowpos < rowmax)
				rowpos++;
		}

		slider.setrows(rowpos, chatrows, rowmax);
	}

	UIElement::Type UIChatBar::get_type() const
	{
		return TYPE;
	}

	Button::State UIChatBar::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case BT_OPENCHAT:
			set_chat_open(true);
			break;
		case BT_CLOSECHAT:
			set_chat_open(false);
			break;
		case BT_CHATTARGETS:
			cycle_chat_target();
			return Button::State::NORMAL;
		}

		return Button::State::NORMAL;
	}

	bool UIChatBar::is_in_range(Point<int16_t> cursorpos) const
	{
		// The input band on the status bar.
		Rectangle<int16_t> band(
			position + Point<int16_t>(0, BAND_TOP),
			position + Point<int16_t>(CHATLOG_W, BAND_BOTTOM));

		if (band.contains(cursorpos))
			return true;

		if (chatopen)
		{
			// The log (strip + text box) above the bar.
			Point<int16_t> lo = log_origin();
			Rectangle<int16_t> log(
				lo - Point<int16_t>(0, CHATSTRIP_H),
				Point<int16_t>(lo.x() + CHATLOG_W, position.y()));

			if (log.contains(cursorpos))
				return true;
		}

		return false;
	}

	int16_t UIChatBar::row_at(Point<int16_t> cursorpos) const
	{
		if (!chatopen)
			return ROW_NONE;

		int16_t chattop = getchattop();
		int16_t yshift = -CHATROWHEIGHT * chatrows;

		for (int16_t i = 0; i < chatrows; i++)
		{
			int16_t rowid = rowpos - i;

			auto it = rowtexts.find(rowid);
			if (it == rowtexts.end())
				break;

			int16_t lines = it->second.height() / CHATROWHEIGHT;
			if (lines < 1)
				lines = 1;

			yshift += lines * CHATROWHEIGHT;
			int16_t msgy = chattop - yshift - 7;

			if (msgy < chattop)
				continue;

			if (cursorpos.y() >= msgy && cursorpos.y() < msgy + lines * CHATROWHEIGHT)
				return rowid;
		}

		return ROW_NONE;
	}

	int16_t UIChatBar::col_at(int16_t rowid, int16_t x) const
	{
		auto it = rowtexts.find(rowid);
		auto sit = rowstrings.find(rowid);
		if (it == rowtexts.end() || sit == rowstrings.end())
			return 0;

		int16_t relx = x - (position.x() + TEXT_X);
		if (relx <= 0)
			return 0;

		size_t len = sit->second.size();
		for (size_t i = 0; i < len; i++)
		{
			int16_t a = static_cast<int16_t>(it->second.advance(i));
			int16_t b = static_cast<int16_t>(it->second.advance(i + 1));
			if (relx < (a + b) / 2)
				return static_cast<int16_t>(i);
		}
		return static_cast<int16_t>(len);
	}

	void UIChatBar::ordered_sel(int16_t& sr, int16_t& sc, int16_t& er, int16_t& ec) const
	{
		// reading order: smaller row id is older/higher on screen, then column
		if (sel_arow < sel_frow || (sel_arow == sel_frow && sel_acol <= sel_fcol))
		{
			sr = sel_arow; sc = sel_acol; er = sel_frow; ec = sel_fcol;
		}
		else
		{
			sr = sel_frow; sc = sel_fcol; er = sel_arow; ec = sel_acol;
		}
	}

	std::string UIChatBar::get_selected_text() const
	{
		if (!has_selection)
			return "";

		int16_t sr, sc, er, ec;
		ordered_sel(sr, sc, er, ec);
		std::string out;

		for (int16_t r = sr; r <= er; r++)
		{
			auto it = rowstrings.find(r);
			if (it == rowstrings.end())
				continue;

			const std::string& line = it->second;
			int16_t from = (r == sr) ? sc : 0;
			int16_t to = (r == er) ? ec : static_cast<int16_t>(line.size());
			if (from < 0) from = 0;
			if (to > static_cast<int16_t>(line.size())) to = static_cast<int16_t>(line.size());
			if (to < from) to = from;

			if (!out.empty())
				out += "\n";
			out += line.substr(from, to - from);
		}

		return out;
	}

	void UIChatBar::clear_selection()
	{
		has_selection = false;
		sel_dragging = false;
	}

	Cursor::State UIChatBar::send_cursor(bool clicking, Point<int16_t> cursorpos)
	{
		// Custom button handling: break after a button press fires.
		// UIElement::send_cursor iterates ALL buttons in one loop, so when
		// BT_CLOSECHAT fires and activates BT_OPENCHAT, the loop immediately
		// processes BT_OPENCHAT in the same frame, causing toggle bounce.
		bool button_handled = false;

		for (auto& btit : buttons)
		{
			if (btit.second->is_active() && btit.second->bounds(position).contains(cursorpos))
			{
				if (btit.second->get_state() == Button::State::NORMAL)
				{
					Sound(Sound::Name::BUTTONOVER).play();
					btit.second->set_state(Button::State::MOUSEOVER);
					return Cursor::State::CANCLICK;
				}
				else if (btit.second->get_state() == Button::State::MOUSEOVER)
				{
					if (clicking)
					{
						Sound(Sound::Name::BUTTONCLICK).play();
						btit.second->set_state(button_pressed(btit.first));
						button_handled = true;
						break;  // Stop processing buttons after a press fires
					}
					else
					{
						return Cursor::State::CANCLICK;
					}
				}
			}
			else if (btit.second->get_state() == Button::State::MOUSEOVER)
			{
				btit.second->set_state(Button::State::NORMAL);
			}
		}

		if (button_handled)
			return Cursor::State::IDLE;

		// Clicking the "To: ..." box itself also cycles the target (the
		// combo arrow is small).
		if (chatopen)
		{
			Rectangle<int16_t> target_area(
				position + Point<int16_t>(TARGET_X, TARGET_Y),
				position + Point<int16_t>(TARGET_X + TARGET_W, TARGET_Y + TARGET_H));

			if (target_area.contains(cursorpos))
			{
				if (clicking)
				{
					Sound(Sound::Name::BUTTONCLICK).play();
					cycle_chat_target();
					return Cursor::State::IDLE;
				}

				return Cursor::State::CANCLICK;
			}
		}

		Point<int16_t> lo = log_origin();

		if (slider.isenabled())
		{
			auto cursoroffset = cursorpos - Point<int16_t>(lo.x(), lo.y() + SLIDER_Y);
			Cursor::State sstate = slider.send_cursor(cursoroffset, clicking);
			if (sstate != Cursor::State::IDLE)
				return sstate;
		}

		if (chatopen)
		{
			Cursor::State tstate = chatfield.send_cursor(cursorpos, clicking);

			if (tstate == Cursor::State::CLICKING || tstate == Cursor::State::CANCLICK)
				return tstate;

			// Don't let clicks outside the textfield defocus it -
			// only Enter key should close the chatfield
			if (clicking && chatfieldopen && chatfield.get_state() != Textfield::State::FOCUSED)
				chatfield.set_state(Textfield::State::FOCUSED);
		}
		else if (clicking)
		{
			// Clicking the chat input area when chat is closed opens and focuses it
			Rectangle<int16_t> input_area(
				position + Point<int16_t>(FIELD_X, FIELD_Y),
				position + Point<int16_t>(FIELD_RIGHT, FIELD_BOTTOM)
			);

			if (input_area.contains(cursorpos))
			{
				toggle_chatfield(true);
				return Cursor::State::IDLE;
			}
		}

		// The strip on top of the log is the resize handle.
		auto chattop_rect = Rectangle<int16_t>(lo.x(), lo.x() + CHATLOG_W, lo.y() - CHATSTRIP_H, lo.y() + 1);
		bool contains = chatopen && chattop_rect.contains(cursorpos);

		if (dragchattop)
		{
			if (clicking)
			{
				int16_t ydelta = cursorpos.y() - getchattop();

				while (ydelta > 0 && chatrows > MINCHATROWS)
				{
					chatrows--;
					ydelta -= CHATROWHEIGHT;
				}

				while (ydelta < 0 && chatrows < MAXCHATROWS)
				{
					chatrows++;
					ydelta += CHATROWHEIGHT;
				}

				chatbox = ColorBox(CHATLOG_W, 1 + chatrows * CHATROWHEIGHT, Color::Name::BLACK, 0.6f);
				slider.setrows(rowpos, chatrows, rowmax);
				slider.setvertical(Range<int16_t>(0, CHATROWHEIGHT * chatrows - 14));

				return Cursor::State::CLICKING;
			}
			else
			{
				dragchattop = false;
			}
		}
		else if (contains)
		{
			if (clicking)
			{
				dragchattop = true;
				return Cursor::State::CLICKING;
			}
			else
			{
				return Cursor::State::CANCLICK;
			}
		}

		if (!clicking)
			sel_dragging = false;

		if (chatopen && clicking)
		{
			int16_t ctop = getchattop();
			int16_t cbottom = ctop + chatrows * CHATROWHEIGHT;
			int16_t cleft = lo.x();
			int16_t cright = lo.x() + CHATLOG_W;

			if (!sel_dragging)
			{
				// Begin a selection only when the press lands on an actual line.
				if (cursorpos.x() >= cleft && cursorpos.x() <= cright
					&& cursorpos.y() >= ctop && cursorpos.y() <= cbottom)
				{
					int16_t r = row_at(cursorpos);

					if (r != ROW_NONE)
					{
						sel_arow = r;
						sel_acol = col_at(r, cursorpos.x());
						sel_frow = sel_arow;
						sel_fcol = sel_acol;
						sel_dragging = true;
						has_selection = true;
						return Cursor::State::CLICKING;
					}
				}
			}
			else
			{
				// Mid-drag: keep extending even when the cursor leaves the
				// text - clamp it back into the log so the selection tracks
				// off the end of a line / above / below like a normal drag.
				int16_t cx = cursorpos.x();
				if (cx < cleft) cx = cleft;
				if (cx > cright) cx = cright;
				int16_t cy = cursorpos.y();
				if (cy < ctop) cy = ctop;
				if (cy > cbottom - 1) cy = cbottom - 1;

				int16_t r = row_at(Point<int16_t>(cx, cy));
				if (r != ROW_NONE)
				{
					sel_frow = r;
					sel_fcol = col_at(r, cx);
				}
				return Cursor::State::CLICKING;
			}
		}

		return Cursor::State::IDLE;
	}

	void UIChatBar::send_line(const std::string& line, LineType type)
	{
		rowmax++;
		rowpos = rowmax;

		slider.setrows(rowpos, chatrows, rowmax);

		Color::Name color;
		int8_t icon = -1;

		switch (type)
		{
		case RED:
			color = Color::Name::RED;
			break;
		case BLUE:
			color = Color::Name::BLUE;
			break;
		case YELLOW:
			color = Color::Name::YELLOW;
			break;
		case PINK:
			color = Color::Name::PINK;
			break;
		case LIGHTBLUE:
			color = Color::Name::LIGHTBLUE;
			break;
		case GREEN:
			color = Color::Name::GREEN;
			break;
		case ORANGE:
			color = Color::Name::ORANGE;
			break;
		case LIGHTGREEN:
			color = Color::Name::LIGHTGREEN;
			break;
		// Megaphone lines each pick a distinct icon and text color so
		// viewers can tell the broadcast sub-type at a glance.
		case MEGAPHONE:
			color = Color::Name::BLUE;
			icon = 0;
			break;
		case SUPER_MEGAPHONE:
			color = Color::Name::PINK;
			icon = 1;
			break;
		case ITEM_MEGAPHONE:
			color = Color::Name::GREEN;
			icon = 2;
			break;
		case TRIPLE_MEGAPHONE:
			color = Color::Name::YELLOW;
			icon = 3;
			break;
		default:
			color = Color::Name::WHITE;
			break;
		}

		// Reserve room for the icon + a small gap when the row has one.
		uint16_t text_maxwidth = (icon >= 0) ? TEXT_MAXWIDTH_ICON : TEXT_MAXWIDTH;

		rowtexts.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(rowmax),
			std::forward_as_tuple(Text::Font::A12M, Text::Alignment::LEFT, color, line, text_maxwidth)
		);

		rowstrings[rowmax] = line;

		if (icon >= 0)
		{
			rowicons[rowmax] = icon;

			// Split the broadcast on the name/message separator so the icon
			// can sit between the player name and the colon. Try " : "
			// first (v83 canonical), then fall back to ":" for servers
			// that skip the surrounding spaces.
			std::string name_part;
			std::string msg_part = line;
			size_t sep = line.find(" : ");
			size_t sep_len = 3;
			if (sep == std::string::npos)
			{
				sep = line.find(':');
				sep_len = 1;
			}
			if (sep != std::string::npos)
			{
				name_part = line.substr(0, sep);
				msg_part  = line.substr(sep, sep_len == 3 ? std::string::npos : std::string::npos);
			}

			MegaRow row;
			row.icon = icon;
			row.namepart = Text(Text::Font::A12M, Text::Alignment::LEFT,
				color, name_part, text_maxwidth);
			row.msgpart = Text(Text::Font::A12M, Text::Alignment::LEFT,
				color, msg_part, text_maxwidth);
			mega_rows[rowmax] = std::move(row);
		}
	}

	void UIChatBar::send_chatline(const std::string& line, LineType type)
	{
		send_line(line, type);
	}

	void chat::log(const std::string& line, chat::LineType type)
	{
		if (auto cb = UI::get().get_element<UIChatBar>())
			cb->send_chatline(line, type);
	}

	void UIChatBar::display_message(Messages::Type line, UIChatBar::LineType type)
	{
		if (message_cooldowns[line] > 0)
			return;

		std::string message = Messages::messages[line];
		send_line(message, type);

		message_cooldowns[line] = MESSAGE_COOLDOWN;
	}

	void UIChatBar::toggle_chat()
	{
		set_chat_open(!chatopen);
	}

	void UIChatBar::toggle_chatfield()
	{
		chatfieldopen = !chatfieldopen;
		toggle_chatfield(chatfieldopen);
	}

	void UIChatBar::toggle_chatfield(bool open)
	{
		chatfieldopen = open;

		if (chatfieldopen)
		{
			if (!chatopen)
				set_chat_open(true);

			// set_state(FOCUSED) internally calls UI::get().focus_textfield()
			// so we must NOT call focus_textfield again - the double call
			// would reset the state back to NORMAL.
			chatfield.set_state(Textfield::State::FOCUSED);
		}
		else
		{
			chatfield.set_state(Textfield::State::DISABLED);
		}
	}

	bool UIChatBar::is_chatopen()
	{
		return chatopen;
	}

	bool UIChatBar::is_chatfieldopen()
	{
		return chatfieldopen;
	}

	void UIChatBar::set_chat_target(ChatTarget target)
	{
		if (target >= CHT_ALL && target < NUM_TARGETS)
		{
			chattarget = target;
			refresh_target_label();
		}
	}

	void UIChatBar::cycle_chat_target()
	{
		chattarget = static_cast<ChatTarget>((chattarget + 1) % NUM_TARGETS);
		refresh_target_label();
	}

	void UIChatBar::set_pending_party_invite(int32_t in_party_id, const std::string& inviter)
	{
		pending_party_invite_id = in_party_id;
		pending_party_inviter = inviter;
	}

	void UIChatBar::clear_pending_party_invite()
	{
		pending_party_invite_id = -1;
		pending_party_inviter.clear();
	}

	void UIChatBar::set_party_state(int32_t in_party_id, int32_t leader_id, const std::vector<PartyMember>& members)
	{
		party_id = in_party_id;
		party_leader_id = leader_id;
		party_members = members;

		if (pending_party_invite_id == in_party_id)
			clear_pending_party_invite();
	}

	void UIChatBar::clear_party_state()
	{
		party_id = -1;
		party_leader_id = -1;
		party_members.clear();
	}

	void UIChatBar::set_party_leader(int32_t leader_id)
	{
		party_leader_id = leader_id;
	}

	void UIChatBar::update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp)
	{
		for (PartyMember& member : party_members)
		{
			if (member.id == cid)
			{
				member.hp = hp;
				member.max_hp = max_hp;
				return;
			}
		}
	}

	int32_t UIChatBar::get_party_id() const
	{
		return party_id;
	}

	int32_t UIChatBar::get_party_leader_id() const
	{
		return party_leader_id;
	}

	int32_t UIChatBar::get_pending_party_invite_id() const
	{
		return pending_party_invite_id;
	}

	const std::string& UIChatBar::get_pending_party_inviter() const
	{
		return pending_party_inviter;
	}

	const std::vector<UIChatBar::PartyMember>& UIChatBar::get_party_members() const
	{
		return party_members;
	}

	int32_t UIChatBar::resolve_party_member_id(const std::string& token) const
	{
		int32_t member_id = 0;
		if (parse_int32(token, member_id))
			return member_id;

		std::string lowered = lowercase(token);
		for (const PartyMember& member : party_members)
		{
			if (lowercase(member.name) == lowered)
				return member.id;
		}

		return 0;
	}

	bool UIChatBar::handle_party_command(const std::string& message)
	{
		if (message.empty() || message[0] != '/')
			return false;

		std::istringstream whole(message);
		std::string command;
		whole >> command;
		command = lowercase(command);

		std::string argument_block;
		std::getline(whole, argument_block);
		argument_block = trim(argument_block);

		if (command == "/pet" || command == "/petcmd")
		{
			PetLook& pet = Stage::get().get_player().get_pet(0);

			if (pet.get_itemid() == 0)
			{
				send_line("[Pet] You don't have a pet summoned.", RED);
				return true;
			}

			if (command == "/pet")
			{
				if (argument_block.empty())
				{
					send_line("[Pet] Usage: /pet <message>", YELLOW);
					return true;
				}

				PetChatPacket(pet.get_uniqueid(), 0, argument_block).dispatch();
				return true;
			}

			int32_t cmd_id = 0;
			parse_int32(argument_block, cmd_id);

			if (cmd_id < 0 || cmd_id > 9)
				cmd_id = 0;

			PetCommandPacket(pet.get_uniqueid(), static_cast<int8_t>(cmd_id)).dispatch();
			return true;
		}

		if (command == "/pt")
		{
			if (argument_block.empty())
			{
				send_line("[Party] Usage: /pt <message>", YELLOW);
				return true;
			}

			send_party_message(argument_block);
			return true;
		}

		if (command != "/party" && command != "/p")
			return false;

		if (argument_block.empty())
		{
			send_line("[Party] /party create | leave | invite <name> | join <partyId>", YELLOW);
			send_line("[Party] /party expel <name|id> | leader <name|id> | accept | deny | list", YELLOW);
			return true;
		}

		std::istringstream args(argument_block);
		std::string action;
		args >> action;
		action = lowercase(action);

		std::string argument;
		std::getline(args, argument);
		argument = trim(argument);

		if (action == "create")
		{
			CreatePartyPacket().dispatch();
			send_line("[Party] Sent create request.", YELLOW);
			return true;
		}

		if (action == "leave")
		{
			LeavePartyPacket().dispatch();
			send_line("[Party] Sent leave request.", YELLOW);
			return true;
		}

		if (action == "join")
		{
			int32_t join_party_id = 0;
			if (!parse_int32(argument, join_party_id))
			{
				send_line("[Party] Usage: /party join <partyId>", YELLOW);
				return true;
			}

			JoinPartyPacket(join_party_id).dispatch();
			send_line("[Party] Sent join request.", YELLOW);
			return true;
		}

		if (action == "invite")
		{
			if (argument.empty())
			{
				send_line("[Party] Usage: /party invite <name>", YELLOW);
				return true;
			}

			InviteToPartyPacket(argument).dispatch();
			send_line("[Party] Sent invite to " + argument + ".", YELLOW);
			return true;
		}

		if (action == "expel" || action == "kick")
		{
			int32_t member_id = resolve_party_member_id(argument);
			if (member_id <= 0)
			{
				send_line("[Party] Usage: /party expel <name|id>", YELLOW);
				return true;
			}

			ExpelFromPartyPacket(member_id).dispatch();
			send_line("[Party] Sent expel request.", YELLOW);
			return true;
		}

		if (action == "leader" || action == "lead")
		{
			int32_t member_id = resolve_party_member_id(argument);
			if (member_id <= 0)
			{
				send_line("[Party] Usage: /party leader <name|id>", YELLOW);
				return true;
			}

			ChangePartyLeaderPacket(member_id).dispatch();
			send_line("[Party] Sent leadership transfer request.", YELLOW);
			return true;
		}

		if (action == "accept")
		{
			if (pending_party_invite_id <= 0)
			{
				send_line("[Party] There is no pending invitation.", YELLOW);
				return true;
			}

			JoinPartyPacket(pending_party_invite_id).dispatch();
			send_line("[Party] Sent invitation accept request.", YELLOW);
			clear_pending_party_invite();
			return true;
		}

		if (action == "deny")
		{
			if (pending_party_inviter.empty())
			{
				send_line("[Party] There is no pending invitation.", YELLOW);
				return true;
			}

			DenyPartyInvitePacket(pending_party_inviter).dispatch();
			send_line("[Party] Declined invitation from " + pending_party_inviter + ".", YELLOW);
			clear_pending_party_invite();
			return true;
		}

		if (action == "list")
		{
			if (party_members.empty())
			{
				send_line("[Party] No cached party members.", YELLOW);
				return true;
			}

			send_line("[Party] Current members:", YELLOW);
			for (const PartyMember& member : party_members)
			{
				std::string leader_tag = member.id == party_leader_id ? " (leader)" : "";
				send_line(" - " + member.name + " Lv." + std::to_string(member.level) + leader_tag, WHITE);
			}
			return true;
		}

		send_line("[Party] Unknown command. Use /party for help.", YELLOW);
		return true;
	}

	void UIChatBar::send_targeted_message(const std::string& target, const std::string& message)
	{
		send_line("[To " + target + "] " + message, WHITE);
	}

	void UIChatBar::send_party_message(const std::string& message)
	{
		std::list<int32_t> recipients;
		MultiChatPacket(MultiChatPacket::PARTY, recipients, message).dispatch();
		send_targeted_message("Party", message);
	}

	void UIChatBar::send_chat_message(const std::string& message)
	{

		std::list<int32_t> recipients;

		switch (chattarget)
		{
		case CHT_ALL:
			GeneralChatPacket(message, true).dispatch();
			break;
		case CHT_BUDDY:
			MultiChatPacket(MultiChatPacket::BUDDY, recipients, message).dispatch();
			send_targeted_message("Buddy", message);
			break;
		case CHT_GUILD:
			MultiChatPacket(MultiChatPacket::GUILD, recipients, message).dispatch();
			send_targeted_message("Guild", message);
			break;
		case CHT_ALLIANCE:
			MultiChatPacket(MultiChatPacket::ALLIANCE, recipients, message).dispatch();
			send_targeted_message("Alliance", message);
			break;
		case CHT_SQUAD:
			send_party_message(message);
			break;
		case CHT_PARTY:
		default:
			send_party_message(message);
			break;
		}
	}

	int16_t UIChatBar::getchattop() const
	{
		return position.y() - chatrows * CHATROWHEIGHT - CHATYOFFSET;
	}
}
