//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileAdmin.h"

#include "../../Character/ExpTable.h"

#include "../ServerInfo.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/MessagingPackets.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 380;
		constexpr int16_t PAD = 16;
		constexpr int16_t TABS_Y = 72;
		constexpr int16_t TOP = 122;
		constexpr int16_t ROW_H = 84;
		constexpr int16_t GAP = 10;

		// The values a stepper walks through.
		constexpr int STEPS[] = { 1, 2, 3, 4, 5, 6, 8, 10, 15, 20, 25, 30, 40, 50, 75, 100, 150, 200, 300, 500, 999 };
		constexpr int NUM_STEPS = sizeof(STEPS) / sizeof(STEPS[0]);

		// One tap sets all three.
		constexpr int PRESETS[] = { 1, 5, 10, 50, 100 };
		constexpr int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

		const char* const NAMES[3] = { "EXP", "DROP", "MESO" };
		constexpr RGBA COLORS[3] = { Theme::EXP, Theme::GREEN, Theme::GOLD };

		enum Target : int32_t
		{
			MINUS = 0,		// + row
			PLUS = 10,		// + row
			PRESET = 20,	// + preset
			APPLY = 40,
			RESET = 41,
			TAB = 50,		// + tab
			QUICK = 100,	// + quick command
			VALUE = 200		// + value command
		};

		// One tap, no typing.
		struct Quick { const char* label; const char* command; };
		const Quick QUICKS[] = {
			{ "Heal", "!heal" },
			{ "Max stats", "!maxstat" },
			{ "Max skills", "!maxskill" },
			{ "GM buffs", "!buffme" },
			{ "All buffs", "!empowerme" },
			{ "Recharge", "!recharge" },
			{ "Hide", "!hide" },
			{ "Unhide", "!unhide" },
			{ "Loot all", "!itemvac" },
			{ "Kill mobs", "!killall" },
			{ "Clear drops", "!cleardrops" },
			{ "Level +1", "" }
		};
		constexpr int NUM_QUICKS = sizeof(QUICKS) / sizeof(QUICKS[0]);
		constexpr int QUICK_COLS = 3;

		// Use what is typed in the box.
		const char* const VALUES[] = { "Item", "Mesos", "NX", "Job", "Level", "Mob", "Search", "Run" };
		constexpr int NUM_VALUES = sizeof(VALUES) / sizeof(VALUES[0]);
		const char* const HINTS[] = {
			"Item ID, then amount (2000005 100)",
			"Mesos to add (1000000)",
			"NX to add (10000)",
			"Job ID (412 Night Lord, 112 Hero)",
			"Level to set (70)",
			"Monster ID, then count (100100 5)",
			"Name to look up (item, mob, map or npc first)",
			"Any command (!warp 100000000)"
		};

		int step(int value, int direction)
		{
			if (direction > 0)
			{
				for (int i = 0; i < NUM_STEPS; i++)
					if (STEPS[i] > value)
						return STEPS[i];

				return STEPS[NUM_STEPS - 1];
			}

			for (int i = NUM_STEPS - 1; i >= 0; i--)
				if (STEPS[i] < value)
					return STEPS[i];

			return STEPS[0];
		}
	}

	UIMobileAdmin::UIMobileAdmin() : UIMobilePanel(Edge::LEFT, W)
	{
		ServerInfo::request();

		field = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::WHITE, 1.0f,
			Rectangle<int16_t>(Point<int16_t>(14, 13), Point<int16_t>(W - PAD * 2 - 20, 31)), 60);
		field.set_enter_callback([&](std::string) { run_value(NUM_VALUES - 1); });
	}

	UIMobileAdmin::~UIMobileAdmin()
	{
		unfocus();
	}

	void UIMobileAdmin::unfocus()
	{
		if (field.get_state() == Textfield::State::FOCUSED)
		{
			field.set_state(Textfield::State::NORMAL);
			UI::get().remove_textfield();
		}
	}

	void UIMobileAdmin::deactivate()
	{
		unfocus();
		UIMobilePanel::deactivate();
	}

	Cursor::State UIMobileAdmin::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// The box takes the keyboard as soon as it is touched.
		if (commands && clicked && field_rect().contains(cursorpos) && field.get_state() != Textfield::State::FOCUSED)
		{
			field.set_state(Textfield::State::FOCUSED);
			return Cursor::State::CLICKING;
		}

		return UIMobilePanel::send_cursor(clicked, cursorpos);
	}

	void UIMobileAdmin::send(const std::string& command)
	{
		Sound(Sound::Name::BUTTONCLICK).play();
		GeneralChatPacket(command, false).dispatch();
		last_sent = command;
		sent_ticks = 300;
	}

	void UIMobileAdmin::run_value(int index)
	{
		std::string value = field.get_text();

		while (!value.empty() && value.back() == ' ')
			value.pop_back();

		while (!value.empty() && value.front() == ' ')
			value.erase(value.begin());

		if (value.empty())
		{
			field.set_state(Textfield::State::FOCUSED);
			return;
		}

		bool two = value.find(' ') != std::string::npos;
		std::string command;

		switch (index)
		{
			case 0: command = "!item " + value + (two ? "" : " 1"); break;
			case 1: command = "!givems " + value; break;
			case 2: command = "!givenx " + value; break;
			case 3: command = "!job " + value; break;
			case 4: command = "!level " + value; break;
			case 5: command = "!spawn " + value; break;
			case 6:
			{
				std::string first = value.substr(0, value.find(' '));

				for (char& c : first)
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

				bool typed = first == "item" || first == "mob" || first == "map" || first == "npc" || first == "quest" || first == "skill";
				command = "!search " + (typed ? value : "item " + value);
				break;
			}
			default:
				command = value.front() == '!' || value.front() == '@' ? value : "!" + value;
				break;
		}

		unfocus();
		send(command);
	}

	Rect UIMobileAdmin::tab_rect(int index) const
	{
		int16_t w = static_cast<int16_t>((W - PAD * 2 - 6) / 2);

		return rect(static_cast<int16_t>(position.x() + PAD + index * (w + 6)), static_cast<int16_t>(position.y() + TABS_Y), w, 36);
	}

	Rect UIMobileAdmin::quick_rect(int index) const
	{
		int16_t gap = 8;
		int16_t w = static_cast<int16_t>((W - PAD * 2 - gap * (QUICK_COLS - 1)) / QUICK_COLS);
		int16_t col = static_cast<int16_t>(index % QUICK_COLS);
		int16_t row = static_cast<int16_t>(index / QUICK_COLS);

		return rect(static_cast<int16_t>(position.x() + PAD + col * (w + gap)), static_cast<int16_t>(position.y() + TOP + 22 + row * 46), w, 40);
	}

	Rect UIMobileAdmin::field_rect() const
	{
		int16_t top = static_cast<int16_t>(quick_rect(NUM_QUICKS - 1).bottom() + 40);

		return rect(static_cast<int16_t>(position.x() + PAD), top, W - PAD * 2, 44);
	}

	Rect UIMobileAdmin::value_rect(int index) const
	{
		int16_t gap = 6;
		int16_t w = static_cast<int16_t>((W - PAD * 2 - gap * 3) / 4);
		int16_t col = static_cast<int16_t>(index % 4);
		int16_t row = static_cast<int16_t>(index / 4);

		return rect(static_cast<int16_t>(position.x() + PAD + col * (w + gap)), static_cast<int16_t>(field_rect().bottom() + 10 + row * 44), w, 38);
	}

	int UIMobileAdmin::server_rate(int index) const
	{
		const ServerInfo::Info& info = ServerInfo::get();

		return index == 0 ? info.exp : index == 1 ? info.drop : info.meso;
	}

	bool UIMobileAdmin::changed() const
	{
		for (int i = 0; i < 3; i++)
			if (pending[i] != server_rate(i))
				return true;

		return false;
	}

	void UIMobileAdmin::update()
	{
		UIMobilePanel::update();

		field.update(field_rect().get_left_top());

		if (sent_ticks > 0)
			sent_ticks--;

		// Until edited, show what the server runs; after applying, the
		// server's answer brings both back in line.
		if (!edited || (applying && !changed()))
		{
			for (int i = 0; i < 3; i++)
				pending[i] = server_rate(i);

			edited = false;
			applying = false;
		}
	}

	Rect UIMobileAdmin::row_rect(int index) const
	{
		return rect(static_cast<int16_t>(position.x() + PAD), static_cast<int16_t>(position.y() + TOP + index * (ROW_H + GAP)),
			W - PAD * 2, ROW_H);
	}

	Rect UIMobileAdmin::minus_rect(int index) const
	{
		Rect r = row_rect(index);

		return rect(static_cast<int16_t>(r.right() - 14 - 44 - 10 - 44), static_cast<int16_t>(r.top() + (ROW_H - 44) / 2), 44, 44);
	}

	Rect UIMobileAdmin::plus_rect(int index) const
	{
		Rect r = row_rect(index);

		return rect(static_cast<int16_t>(r.right() - 14 - 44), static_cast<int16_t>(r.top() + (ROW_H - 44) / 2), 44, 44);
	}

	Rect UIMobileAdmin::preset_rect(int index) const
	{
		Rect last = row_rect(2);
		int16_t gap = 8;
		int16_t w = static_cast<int16_t>((W - PAD * 2 - gap * (NUM_PRESETS - 1)) / NUM_PRESETS);

		return rect(static_cast<int16_t>(last.left() + index * (w + gap)), static_cast<int16_t>(last.bottom() + 42), w, 34);
	}

	Rect UIMobileAdmin::reset_rect() const
	{
		Rect f = frame();

		return rect(static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.bottom() - PAD - 46), 110, 46);
	}

	Rect UIMobileAdmin::apply_rect() const
	{
		Rect f = frame();
		Rect reset = reset_rect();

		return rect(static_cast<int16_t>(reset.right() + 10), reset.top(), static_cast<int16_t>(f.right() - PAD - reset.right() - 10), 46);
	}

	int32_t UIMobileAdmin::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		for (int t = 0; t < 2; t++)
			if (inset(tab_rect(t), -2, -4).contains(pos))
				return TAB + t;

		if (commands)
		{
			for (int i = 0; i < NUM_QUICKS; i++)
				if (inset(quick_rect(i), -2, -3).contains(pos))
					return QUICK + i;

			for (int i = 0; i < NUM_VALUES; i++)
				if (inset(value_rect(i), -2, -3).contains(pos))
					return VALUE + i;

			return NO_TARGET;
		}

		for (int i = 0; i < 3; i++)
		{
			if (inset(minus_rect(i), -4, -4).contains(pos))
				return MINUS + i;

			if (inset(plus_rect(i), -4, -4).contains(pos))
				return PLUS + i;
		}

		for (int i = 0; i < NUM_PRESETS; i++)
			if (inset(preset_rect(i), -2, -4).contains(pos))
				return PRESET + i;

		if (apply_rect().contains(pos))
			return APPLY;

		if (reset_rect().contains(pos))
			return RESET;

		return NO_TARGET;
	}

	bool UIMobileAdmin::repeats(int32_t target) const
	{
		return target >= MINUS && target < PRESET;
	}

	void UIMobileAdmin::activate(int32_t target)
	{
		if (target == TAB || target == TAB + 1)
		{
			Sound(Sound::Name::TAB).play();
			commands = target == TAB + 1;

			if (!commands)
				unfocus();

			return;
		}

		if (target >= VALUE && target < VALUE + NUM_VALUES)
		{
			run_value(target - VALUE);
			return;
		}

		if (target >= QUICK && target < QUICK + NUM_QUICKS)
		{
			const Quick& q = QUICKS[target - QUICK];

			if (q.command[0] != 0)
			{
				send(q.command);
			}
			else
			{
				int level = Stage::get().get_player().get_stats().get_stat(MapleStat::Id::LEVEL);
				send("!level " + std::to_string(std::min(level + 1, ExpTable::MAX_LEVEL)));
			}

			return;
		}

		if (target == APPLY)
		{
			if (!changed())
				return;

			Sound(Sound::Name::BUTTONCLICK).play();
			GeneralChatPacket("@setrates " + std::to_string(pending[0]) + " " + std::to_string(pending[1]) + " "
				+ std::to_string(pending[2]), false).dispatch();
			applying = true;
			return;
		}

		if (target == RESET)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			edited = false;
			return;
		}

		if (target >= PRESET && target < PRESET + NUM_PRESETS)
		{
			for (int i = 0; i < 3; i++)
				pending[i] = PRESETS[target - PRESET];
		}
		else if (target >= PLUS && target < PLUS + 3)
		{
			pending[target - PLUS] = step(pending[target - PLUS], 1);
		}
		else if (target >= MINUS && target < MINUS + 3)
		{
			pending[target - MINUS] = step(pending[target - MINUS], -1);
		}
		else
		{
			return;
		}

		Sound(Sound::Name::BUTTONCLICK).play();
		edited = true;
		applying = false;
	}

	void UIMobileAdmin::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();

		panel(f, "Admin", commands ? "GM commands for your character and map" : "Server rates for everyone");
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		for (int t = 0; t < 2; t++)
			button(tab_rect(t), t == 0 ? "Rates" : "Commands", (t == 1) == commands ? Style::SELECTED : Style::SECONDARY, true,
				pressed == TAB + t, Font::SMALL_BOLD);

		if (commands)
			draw_commands();
		else
			draw_rates();
	}

	void UIMobileAdmin::draw_commands() const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();

		text("QUICK", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(f.left() + PAD + 2), static_cast<int16_t>(f.top() + TOP)));

		for (int i = 0; i < NUM_QUICKS; i++)
		{
			bool danger = std::string(QUICKS[i].command) == "!killall" || std::string(QUICKS[i].command) == "!cleardrops";
			button(quick_rect(i), QUICKS[i].label, danger ? Style::DANGER : Style::SECONDARY, true, pressed == QUICK + i, Font::SMALL_BOLD);
		}

		Rect fr = field_rect();
		text("WITH A VALUE", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(fr.left() + 2), static_cast<int16_t>(fr.top() - 22)));

		bool focused = field.get_state() == Textfield::State::FOCUSED;
		fill(fr, focused ? Theme::CARD_HI : Theme::CARD, 12);

		if (focused)
			fill(rect(static_cast<int16_t>(fr.left() + 12), static_cast<int16_t>(fr.bottom() - 3), static_cast<int16_t>(fr.width() - 24), 2), Theme::ACCENT, 1);

		if (field.empty() && !focused)
			text_in("ID, amount or a command", Font::BODY, Theme::FAINT, fr, Text::Alignment::LEFT, 14);

		field.draw(fr.get_left_top());

		for (int i = 0; i < NUM_VALUES; i++)
			button(value_rect(i), VALUES[i], i == NUM_VALUES - 1 ? Style::PRIMARY : Style::SECONDARY, true, pressed == VALUE + i, Font::SMALL_BOLD);

		// What a pressed button will do, else what was just sent.
		int16_t ny = static_cast<int16_t>(value_rect(NUM_VALUES - 1).bottom() + 14);
		std::string note;
		RGBA tint = Theme::MUTED;

		if (pressed >= VALUE && pressed < VALUE + NUM_VALUES)
			note = HINTS[pressed - VALUE];
		else if (sent_ticks > 0)
			note = "Sent  " + last_sent, tint = Theme::GREEN;
		else
			note = "Results and errors show in chat.";

		text(ellipsize(note, Font::SMALL, W - PAD * 2), Font::SMALL, tint, Point<int16_t>(static_cast<int16_t>(f.left() + PAD + 2), ny));
	}

	void UIMobileAdmin::draw_rates() const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();
		const ServerInfo::Info& info = ServerInfo::get();

		for (int i = 0; i < 3; i++)
		{
			Rect r = row_rect(i);
			fill(r, Theme::CARD, 16);
			fill(rect(r.left(), static_cast<int16_t>(r.top() + 16), 4, static_cast<int16_t>(ROW_H - 32)), COLORS[i], 2);

			text(std::string(NAMES[i]) + " RATE", Font::SMALL_BOLD, Theme::MUTED,
				Point<int16_t>(static_cast<int16_t>(r.left() + 18), static_cast<int16_t>(r.top() + 14)));
			text(std::to_string(pending[i]) + "x", Font::HEADLINE, pending[i] != server_rate(i) ? Theme::ACCENT : Theme::TEXT,
				Point<int16_t>(static_cast<int16_t>(r.left() + 18), static_cast<int16_t>(r.top() + 32)));

			if (info.known)
				text("Now " + std::to_string(server_rate(i)) + "x", Font::SMALL, Theme::MUTED,
					Point<int16_t>(static_cast<int16_t>(r.left() + 108), static_cast<int16_t>(r.top() + 42)));

			icon_button(minus_rect(i), Glyph::MINUS, Style::SECONDARY, pending[i] > STEPS[0], pressed == MINUS + i);
			icon_button(plus_rect(i), Glyph::PLUS, Style::PRIMARY, pending[i] < STEPS[NUM_STEPS - 1], pressed == PLUS + i);
		}

		Rect last = row_rect(2);
		text("SET ALL", Font::SMALL_BOLD, Theme::MUTED,
			Point<int16_t>(static_cast<int16_t>(last.left() + 2), static_cast<int16_t>(last.bottom() + 18)));

		for (int i = 0; i < NUM_PRESETS; i++)
		{
			bool all = pending[0] == PRESETS[i] && pending[1] == PRESETS[i] && pending[2] == PRESETS[i];
			button(preset_rect(i), std::to_string(PRESETS[i]) + "x", all ? Style::SELECTED : Style::SECONDARY, true,
				pressed == PRESET + i, Font::SMALL_BOLD);
		}

		std::string note = !info.known ? "Asking the server for its rates..."
			: applying ? "Applying..."
			: "Applies to everyone at once and stays after a restart.";
		text(note, Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(f.left() + PAD + 2),
			static_cast<int16_t>(reset_rect().top() - 26)));

		button(reset_rect(), "Undo", Style::SECONDARY, edited, pressed == RESET);
		button(apply_rect(), "Apply", Style::PRIMARY, changed() && !applying, pressed == APPLY);
	}

	UIElement::Type UIMobileAdmin::get_type() const
	{
		return TYPE;
	}
}
