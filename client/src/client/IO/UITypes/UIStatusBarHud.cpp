//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
//
// The phone's HUD: v83's status bar reduced to what a phone needs, floating
// see-through in the top-left corner - level and name, HP, MP and EXP, and
// chips for unspent AP/SP and waiting notifications.
#include "UIStatusBar.h"

#include "../MobileUI.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Character/ExpTable.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t NAME_H = 20;
		constexpr int16_t BAR_H = 16;
		constexpr int16_t EXP_H = 8;
		constexpr int16_t ROW_GAP = 4;
		constexpr int16_t CHIP_H = 20;
		constexpr int16_t NAME_MAX_W = 104;

		constexpr RGBA TRACK = { 0.0f, 0.0f, 0.0f, 0.45f };
		constexpr RGBA SHADOW = { 0.0f, 0.0f, 0.0f, 0.70f };
		constexpr RGBA DARK_TEXT = { 0.13f, 0.08f, 0.03f, 1.0f };

		// Text that stays readable over any map: a dark copy one pixel down
		// and right.
		void shadowed_in(const std::string& str, Font font, RGBA color, const Rect& r, Text::Alignment align, int16_t pad)
		{
			Rect s = rect(static_cast<int16_t>(r.left() + 1), static_cast<int16_t>(r.top() + 1), r.width(), r.height());
			text_in(str, font, SHADOW, s, align, pad);
			text_in(str, font, color, r, align, pad);
		}

		void gauge(const Rect& r, float now, float trail, RGBA color, float glow)
		{
			int16_t radius = static_cast<int16_t>(r.height() / 2);
			fill(r, TRACK, radius);

			auto part = [&](float fraction, RGBA c)
			{
				fraction = std::clamp(fraction, 0.0f, 1.0f);

				if (fraction <= 0.0f)
					return;

				int16_t w = static_cast<int16_t>(std::max<float>(r.height(), r.width() * fraction));
				fill(rect(r.left(), r.top(), w, r.height()), c, radius);
			};

			// What was just lost lingers pale while the eased value catches up.
			if (trail > now)
				part(trail, { 1.0f, 1.0f, 1.0f, 0.35f });

			part(now, color.alpha(glow));
		}

		std::string grouped(int64_t value)
		{
			std::string digits = std::to_string(value);
			std::string out;

			for (size_t i = 0; i < digits.size(); i++)
			{
				if (i > 0 && digits[i - 1] != '-' && (digits.size() - i) % 3 == 0)
					out += ',';

				out += digits[i];
			}

			return out;
		}
	}

	Rectangle<int16_t> UIStatusBar::hud_chip(int32_t target) const
	{
		int32_t ap = stats.get_stat(MapleStat::Id::AP);
		int32_t sp = stats.get_stat(MapleStat::Id::SP);

		// Chips follow the name on the top row, left to right.
		std::string level = "Lv " + std::to_string(stats.get_stat(MapleStat::Id::LEVEL));
		int16_t x = static_cast<int16_t>(HUD_LEFT + text_width(level, Font::SMALL_BOLD) + 16 + 6);
		std::string name = ellipsize(stats.get_name(), Font::BODY_BOLD, NAME_MAX_W);
		x = static_cast<int16_t>(x + text_width(name, Font::BODY_BOLD) + 8);

		for (int32_t t = HUD_AP; t <= HUD_NOTICE; t++)
		{
			// Waiting notifications have the touch controls' Inbox chip now.
			bool shown = t == HUD_AP ? ap > 0 : t == HUD_SP ? sp > 0 : false;

			if (!shown)
				continue;

			int16_t w = t == HUD_NOTICE ? CHIP_H
				: static_cast<int16_t>(text_width((t == HUD_AP ? "AP " : "SP ") + std::to_string(t == HUD_AP ? ap : sp), Font::SMALL_BOLD) + 16);
			Rect chip = rect(x, HUD_TOP, w, CHIP_H);

			if (t == target)
				return chip;

			x = static_cast<int16_t>(x + w + 5);
		}

		return {};
	}

	int32_t UIStatusBar::hud_hit(Point<int16_t> pos) const
	{
		for (int32_t t = HUD_AP; t <= HUD_NOTICE; t++)
		{
			Rect chip = hud_chip(t);

			if (chip.width() > 0 && inset(chip, -6, -8).contains(pos))
				return t;
		}

		return HUD_NONE;
	}

	Cursor::State UIStatusBar::hud_cursor(bool clicked, Point<int16_t> pos)
	{
		int32_t target = hud_hit(pos);

		if (target == HUD_NONE)
			return Cursor::State::IDLE;

		if (!clicked)
			return Cursor::State::CANCLICK;

		// Once per touch: a held finger reports the press every frame.
		uint32_t serial = UI::get().get_press_serial();

		if (serial != hud_press)
		{
			hud_press = serial;
			Sound(Sound::Name::BUTTONCLICK).play();

			switch (target)
			{
				case HUD_AP: open_stats(); break;
				case HUD_SP: open_skills(); break;
				case HUD_NOTICE: open_notices(); break;
			}
		}

		return Cursor::State::CLICKING;
	}

	void UIStatusBar::draw_hud(float) const
	{
		int16_t x = HUD_LEFT;
		int16_t y = HUD_TOP;
		int16_t level = static_cast<int16_t>(stats.get_stat(MapleStat::Id::LEVEL));

		// Level and name.
		std::string lv = "Lv " + std::to_string(level);
		Rect pill = rect(x, y, static_cast<int16_t>(text_width(lv, Font::SMALL_BOLD) + 16), NAME_H);
		fill(pill, Theme::ACCENT.alpha(0.92f), static_cast<int16_t>(NAME_H / 2));
		text_in(lv, Font::SMALL_BOLD, DARK_TEXT, pill);

		std::string name = ellipsize(stats.get_name(), Font::BODY_BOLD, NAME_MAX_W);
		shadowed_in(name, Font::BODY_BOLD, Theme::TEXT,
			rect(static_cast<int16_t>(pill.right() + 6), y, static_cast<int16_t>(NAME_MAX_W + 8), NAME_H), Text::Alignment::LEFT, 0);

		// Unspent points and waiting notifications, each opening its place.
		for (int32_t t = HUD_AP; t <= HUD_NOTICE; t++)
		{
			Rect chip = hud_chip(t);

			if (chip.width() <= 0)
				continue;

			if (t == HUD_NOTICE)
			{
				float pulse = 0.65f + 0.35f * static_cast<float>(std::fabs(std::sin(notice_pulse_tick * 0.08)));
				fill(chip, Theme::RED.alpha(pulse), static_cast<int16_t>(CHIP_H / 2));
				text_in("!", Font::BODY_BOLD, Theme::TEXT, chip);
				continue;
			}

			bool is_ap = t == HUD_AP;
			int32_t points = stats.get_stat(is_ap ? MapleStat::Id::AP : MapleStat::Id::SP);
			RGBA color = is_ap ? Theme::GOLD : Theme::BLUE;
			fill(chip, color.alpha(0.92f), static_cast<int16_t>(CHIP_H / 2));
			text_in((is_ap ? "AP " : "SP ") + std::to_string(points), Font::SMALL_BOLD, DARK_TEXT, chip);
		}

		y = static_cast<int16_t>(y + NAME_H + ROW_GAP);

		// HP and MP, numbers on the bars.
		int32_t hp = stats.get_stat(MapleStat::Id::HP);
		int32_t maxhp = stats.get_total(EquipStat::Id::HP);
		int32_t mp = stats.get_stat(MapleStat::Id::MP);
		int32_t maxmp = stats.get_total(EquipStat::Id::MP);
		float hp_now = gethppercent();
		float mp_now = getmppercent();

		// Running low: the HP bar breathes.
		float glow = 1.0f;

		if (maxhp > 0 && hp_now < 0.25f)
			glow = 0.55f + 0.45f * static_cast<float>(std::fabs(std::sin(hud_tick * 0.10)));

		Rect hp_bar = rect(x, y, HUD_WIDTH, BAR_H);
		gauge(hp_bar, hp_now, hp_shown, Theme::HP, glow);
		shadowed_in("HP", Font::SMALL_BOLD, Theme::TEXT, hp_bar, Text::Alignment::LEFT, 8);
		shadowed_in(grouped(hp) + " / " + grouped(maxhp), Font::SMALL_BOLD, Theme::TEXT, hp_bar, Text::Alignment::RIGHT, 8);

		y = static_cast<int16_t>(y + BAR_H + ROW_GAP);

		Rect mp_bar = rect(x, y, HUD_WIDTH, BAR_H);
		gauge(mp_bar, mp_now, mp_shown, Theme::MP, 1.0f);
		shadowed_in("MP", Font::SMALL_BOLD, Theme::TEXT, mp_bar, Text::Alignment::LEFT, 8);
		shadowed_in(grouped(mp) + " / " + grouped(maxmp), Font::SMALL_BOLD, Theme::TEXT, mp_bar, Text::Alignment::RIGHT, 8);

		y = static_cast<int16_t>(y + BAR_H + ROW_GAP + 1);

		// EXP: a thin line with the percentage beside it.
		float exp_now = getexppercent();
		Rect exp_bar = rect(x, y, HUD_WIDTH, EXP_H);
		gauge(exp_bar, std::min(exp_now, exp_shown), 0.0f, Theme::EXP, 1.0f);

		char percent[16];
		std::snprintf(percent, sizeof(percent), "%.2f%%", exp_now * 100.0f);
		bool max_level = ExpTable::need(stats.get_stat(MapleStat::Id::LEVEL)) <= 0;
		shadowed_in(max_level ? std::string("MAX LEVEL") : std::string("EXP ") + percent, Font::SMALL_BOLD, Theme::EXP,
			rect(static_cast<int16_t>(exp_bar.right() + 6), static_cast<int16_t>(y - 6), 90, 20), Text::Alignment::LEFT, 0);
	}
}
