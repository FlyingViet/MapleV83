//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileStats.h"

#include "UIMobileConfirm.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Character/ExpTable.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/MessagingPackets.h"
#include "../../Net/Packets/PlayerPackets.h"

#include <algorithm>
#include <cstdio>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 400;
		constexpr int16_t PAD = 16;
		constexpr int16_t BARS_Y = 76;
		constexpr int16_t BAR_STEP = 34;
		constexpr int16_t AP_Y = 182;
		constexpr int16_t ROWS_Y = 248;
		constexpr int16_t ROW_STEP = 52;
		constexpr int16_t ROW_H = 46;
		constexpr int16_t VITALS_Y = ROWS_Y + ROW_STEP * 4;
		constexpr int16_t DETAILS_Y = VITALS_Y + 52;

		struct StatRow
		{
			const char* name;
			MapleStat::Id stat;
			EquipStat::Id total;
		};

		constexpr StatRow STAT_ROWS[4] = {
			{ "STR", MapleStat::Id::STR, EquipStat::Id::STR },
			{ "DEX", MapleStat::Id::DEX, EquipStat::Id::DEX },
			{ "INT", MapleStat::Id::INT, EquipStat::Id::INT },
			{ "LUK", MapleStat::Id::LUK, EquipStat::Id::LUK }
		};

		MapleStat::Id maple_stat(EquipStat::Id id)
		{
			switch (id)
			{
				case EquipStat::Id::DEX: return MapleStat::Id::DEX;
				case EquipStat::Id::INT: return MapleStat::Id::INT;
				case EquipStat::Id::LUK: return MapleStat::Id::LUK;
				default: return MapleStat::Id::STR;
			}
		}

		std::string with_commas(int64_t value)
		{
			std::string digits = std::to_string(value < 0 ? -value : value);
			std::string out;

			for (size_t i = 0; i < digits.size(); i++)
			{
				if (i > 0 && (digits.size() - i) % 3 == 0)
					out += ',';

				out += digits[i];
			}

			return value < 0 ? "-" + out : out;
		}
	}

	UIMobileStats::UIMobileStats(const CharStats& in_stats) : UIMobilePanel(Edge::LEFT, W), stats(in_stats)
	{
		last_ap = stats.get_stat(MapleStat::Id::AP);
	}

	// ---- layout -------------------------------------------------------------

	Rect UIMobileStats::auto_rect() const
	{
		return rect(static_cast<int16_t>(position.x() + W - PAD - 12 - 84 - 8 - 84), position.y() + AP_Y + 10, 84, 36);
	}

	Rect UIMobileStats::reset_rect() const
	{
		return rect(static_cast<int16_t>(position.x() + W - PAD - 12 - 84), position.y() + AP_Y + 10, 84, 36);
	}

	Rect UIMobileStats::stat_row(int index) const
	{
		return rect(position.x() + PAD, static_cast<int16_t>(position.y() + ROWS_Y + ROW_STEP * index), W - PAD * 2, ROW_H);
	}

	Rect UIMobileStats::add_rect(int32_t target) const
	{
		if (target >= ADD_STR && target <= ADD_LUK)
		{
			Rect row = stat_row(target - ADD_STR);

			return rect(static_cast<int16_t>(row.right() - 6 - 40), static_cast<int16_t>(row.top() + 3), 40, 40);
		}

		int16_t half = static_cast<int16_t>((W - PAD * 2 - 8) / 2);
		int16_t x = static_cast<int16_t>(position.x() + PAD + (target == ADD_MP ? half + 8 : 0));

		return rect(static_cast<int16_t>(x + half - 6 - 34), static_cast<int16_t>(position.y() + VITALS_Y + 4), 34, 34);
	}

	// ---- state --------------------------------------------------------------

	int32_t UIMobileStats::available_ap() const
	{
		return std::max(0, stats.get_stat(MapleStat::Id::AP) - pending);
	}

	void UIMobileStats::update()
	{
		UIMobilePanel::update();

		int32_t ap = stats.get_stat(MapleStat::Id::AP);

		// Points sent but not yet confirmed by the server keep a held "+"
		// from overshooting.
		if (ap != last_ap)
		{
			pending = ap < last_ap ? std::max(0, pending - (last_ap - ap)) : 0;
			last_ap = ap;
			pending_ms = 0;
		}
		else if (pending > 0)
		{
			pending_ms += Constants::TIMESTEP;

			if (pending_ms > 1500)
				pending = 0;
		}
	}

	void UIMobileStats::spend(MapleStat::Id stat)
	{
		if (available_ap() <= 0)
			return;

		if (pending == 0)
			Sound(Sound::Name::BUTTONCLICK).play();

		SpendApPacket(stat).dispatch();
		pending++;
		pending_ms = 0;
	}

	void UIMobileStats::auto_assign()
	{
		int32_t ap = available_ap();

		if (ap <= 0)
			return;

		const Player& player = Stage::get().get_player();
		EquipStat::Id primary = stats.get_job().get_primary(player.get_weapontype());
		MapleStat::Id stat = maple_stat(primary);
		std::string name = EquipStat::names[primary];

		UI::get().emplace<UIMobileConfirm>("Auto-assign",
			"Put all " + with_commas(ap) + " ability points into " + name + ", your main stat?",
			"Assign", false,
			[ap, stat]()
			{
				// One request for all of them (the server stops at the stat limit).
				AutoAssignApPacket(stat, ap).dispatch();
			});
	}

	// ---- input --------------------------------------------------------------

	int32_t UIMobileStats::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		if (auto_rect().contains(pos))
			return AUTO;

		if (reset_rect().contains(pos))
			return RESET;

		// The whole row is the "+": generous for thumbs.
		for (int i = 0; i < 4; i++)
			if (stat_row(i).contains(pos) && pos.x() > stat_row(i).right() - 120)
				return ADD_STR + i;

		for (int32_t t : { ADD_HP, ADD_MP })
			if (inset(add_rect(t), -8, -6).contains(pos))
				return t;

		return NO_TARGET;
	}

	bool UIMobileStats::repeats(int32_t target) const
	{
		return target >= ADD_STR && target <= ADD_MP;
	}

	void UIMobileStats::activate(int32_t target)
	{
		switch (target)
		{
			case AUTO:
				auto_assign();
				break;
			case RESET:
				UI::get().emplace<UIMobileConfirm>("Reset ability points",
					"Every point you placed in STR, DEX, INT and LUK comes back to place again. Free, as often as you like.",
					"Reset", true,
					[]()
					{
						GeneralChatPacket("@resetap", false).dispatch();
					});
				break;
			case ADD_STR: spend(MapleStat::Id::STR); break;
			case ADD_DEX: spend(MapleStat::Id::DEX); break;
			case ADD_INT: spend(MapleStat::Id::INT); break;
			case ADD_LUK: spend(MapleStat::Id::LUK); break;
			case ADD_HP: spend(MapleStat::Id::HP); break;
			case ADD_MP: spend(MapleStat::Id::MP); break;
		}
	}

	// ---- drawing ------------------------------------------------------------

	void UIMobileStats::draw(float) const
	{
		Rect f = frame();
		int16_t x = f.left();
		int16_t y = f.top();
		int32_t pressed = pressed_target();

		uint16_t level = stats.get_stat(MapleStat::Id::LEVEL);
		panel(f, stats.get_name(), "Lv " + std::to_string(level) + "  \xC2\xB7  " + stats.get_jobname()
			+ "  \xC2\xB7  Fame " + std::to_string(static_cast<int16_t>(stats.get_stat(MapleStat::Id::FAME))));
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		// HP / MP / EXP.
		{
			int32_t hp = stats.get_stat(MapleStat::Id::HP);
			int32_t maxhp = std::max(1, stats.get_total(EquipStat::Id::HP));
			int32_t mp = stats.get_stat(MapleStat::Id::MP);
			int32_t maxmp = std::max(1, stats.get_total(EquipStat::Id::MP));
			int64_t exp = stats.get_exp();
			int64_t need = ExpTable::need(level);
			float expf = need > 0 ? static_cast<float>(static_cast<double>(exp) / need) : 1.0f;

			char pct[32];
			std::snprintf(pct, sizeof(pct), "%.2f%%", expf * 100.0f);
			std::string exp_text = need > 0 ? with_commas(exp) + "  (" + pct + ")" : "MAX LEVEL";

			struct Vital { const char* label; std::string value; float fraction; RGBA color; };
			Vital vitals[3] = {
				{ "HP", with_commas(hp) + " / " + with_commas(maxhp), static_cast<float>(hp) / maxhp, Theme::HP },
				{ "MP", with_commas(mp) + " / " + with_commas(maxmp), static_cast<float>(mp) / maxmp, Theme::MP },
				{ "EXP", exp_text, expf, Theme::EXP }
			};

			for (int i = 0; i < 3; i++)
			{
				int16_t vy = static_cast<int16_t>(y + BARS_Y + BAR_STEP * i);
				text(vitals[i].label, Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(x + PAD + 2, vy));
				text(vitals[i].value, Font::SMALL_BOLD, Theme::TEXT, Point<int16_t>(x + W - PAD - 2, vy), Text::Alignment::RIGHT);
				bar(rect(x + PAD, static_cast<int16_t>(vy + 17), W - PAD * 2, 8), vitals[i].fraction, vitals[i].color);
			}
		}

		// Ability points.
		int32_t ap = available_ap();
		bool has_ap = ap > 0;
		{
			Rect card = rect(x + PAD, y + AP_Y, W - PAD * 2, 56);
			fill(card, Theme::CARD, 14);
			text("ABILITY POINTS", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(card.left() + 14, card.top() + 9));
			text(with_commas(ap), Font::HEADLINE, has_ap ? Theme::GOLD : Theme::FAINT, Point<int16_t>(card.left() + 14, card.top() + 24));

			button(auto_rect(), "Auto", Style::SECONDARY, has_ap, pressed == AUTO);
			button(reset_rect(), "Reset", Style::DANGER, true, pressed == RESET);
		}

		// STR / DEX / INT / LUK.
		const Player& player = Stage::get().get_player();
		EquipStat::Id primary = stats.get_job().get_primary(player.get_weapontype());
		EquipStat::Id secondary = stats.get_job().get_secondary(player.get_weapontype());

		for (int i = 0; i < 4; i++)
		{
			const StatRow& s = STAT_ROWS[i];
			Rect row = stat_row(i);
			bool row_pressed = pressed == ADD_STR + i;
			fill(row, row_pressed && has_ap ? Theme::CARD_HI : Theme::CARD, 12);

			text_in(s.name, Font::TITLE, Theme::TEXT, row, Text::Alignment::LEFT, 16);

			int16_t name_w = text_width(s.name, Font::TITLE);
			const char* tag = s.total == primary ? "MAIN" : s.total == secondary ? "SUB" : nullptr;

			if (tag)
			{
				int16_t tw = static_cast<int16_t>(text_width(tag, Font::SMALL_BOLD) + 14);
				Rect pill = rect(static_cast<int16_t>(row.left() + 16 + name_w + 10), static_cast<int16_t>(centre(row).y() - 9), tw, 18);
				RGBA c = s.total == primary ? Theme::ACCENT : Theme::BLUE;
				fill(pill, c.alpha(0.20f), 9);
				text_in(tag, Font::SMALL_BOLD, c, pill);
			}

			int32_t base = stats.get_stat(s.stat);
			int32_t total = stats.get_total(s.total);
			Rect add = add_rect(ADD_STR + i);
			int16_t vx = static_cast<int16_t>(add.left() - 14);

			text_in(with_commas(total), Font::HEADLINE, Theme::TEXT, rect(row.left(), row.top(), vx - row.left(), row.height()),
				Text::Alignment::RIGHT, 0);

			if (total != base)
			{
				int16_t value_w = text_width(with_commas(total), Font::HEADLINE);
				int32_t bonus = total - base;
				std::string detail = with_commas(base) + (bonus > 0 ? " + " : " - ") + with_commas(bonus > 0 ? bonus : -bonus);
				text_in(detail, Font::SMALL_BOLD, bonus > 0 ? Theme::GREEN : Theme::RED,
					rect(row.left(), row.top(), static_cast<int16_t>(vx - value_w - 10 - row.left()), row.height()),
					Text::Alignment::RIGHT, 0);
			}

			icon_button(add, Glyph::PLUS, Style::PRIMARY, has_ap, row_pressed);
		}

		// Max HP / MP.
		{
			int16_t half = static_cast<int16_t>((W - PAD * 2 - 8) / 2);

			for (int i = 0; i < 2; i++)
			{
				int32_t t = i == 0 ? ADD_HP : ADD_MP;
				Rect card = rect(static_cast<int16_t>(x + PAD + i * (half + 8)), y + VITALS_Y, half, 42);
				fill(card, Theme::CARD, 12);

				int32_t value = stats.get_total(i == 0 ? EquipStat::Id::HP : EquipStat::Id::MP);
				text(i == 0 ? "MAX HP" : "MAX MP", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(card.left() + 12, card.top() + 5));
				text(with_commas(value), Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(card.left() + 12, card.top() + 20));
				icon_button(add_rect(t), Glyph::PLUS, Style::SECONDARY, has_ap, pressed == t);
			}
		}

		// Combat numbers.
		{
			std::string damage = with_commas(stats.get_mindamage()) + " ~ " + with_commas(stats.get_maxdamage());

			struct Cell { const char* label; std::string value; int16_t col; };
			std::string move = std::to_string(stats.get_total(EquipStat::Id::SPEED)) + "% / "
				+ std::to_string(stats.get_total(EquipStat::Id::JUMP)) + "%";

			// ATTACK takes two columns: damage ranges get long.
			Cell cells[7] = {
				{ "ATTACK", damage, 0 },
				{ "ACCURACY", with_commas(stats.get_total(EquipStat::Id::ACC)), 2 },
				{ "AVOID", with_commas(stats.get_total(EquipStat::Id::AVOID)), 3 },
				{ "W. DEF", with_commas(stats.get_total(EquipStat::Id::WDEF)), 4 },
				{ "MAGIC", with_commas(stats.get_total(EquipStat::Id::MAGIC)), 5 },
				{ "M. DEF", with_commas(stats.get_total(EquipStat::Id::MDEF)), 6 },
				{ "SPEED / JUMP", move, 7 }
			};

			int16_t col = static_cast<int16_t>((W - PAD * 2) / 4);

			for (const Cell& c : cells)
			{
				int16_t cx = static_cast<int16_t>(x + PAD + 4 + col * (c.col % 4));
				int16_t cy = static_cast<int16_t>(y + DETAILS_Y + (c.col / 4) * 34);

				text(c.label, Font::SMALL_BOLD, Theme::FAINT, Point<int16_t>(cx, cy));
				text(c.value, Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(cx, static_cast<int16_t>(cy + 14)));
			}
		}
	}

	UIElement::Type UIMobileStats::get_type() const
	{
		return TYPE;
	}
}
