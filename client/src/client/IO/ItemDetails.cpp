//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "ItemDetails.h"

#include "../Character/Inventory/Equip.h"
#include "../Character/Job.h"
#include "../Data/EquipData.h"
#include "../Data/ItemData.h"
#include "../Data/WeaponData.h"
#include "../Gameplay/Stage.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t LABEL_H = 22;
		constexpr int16_t CHIP_H = 26;
		constexpr int16_t STAT_H = 22;
		constexpr int16_t FACT_H = 20;
		constexpr int16_t SECTION_GAP = 10;

		const char* stat_name(EquipStat::Id id)
		{
			switch (id)
			{
				case EquipStat::Id::STR: return "STR";
				case EquipStat::Id::DEX: return "DEX";
				case EquipStat::Id::INT: return "INT";
				case EquipStat::Id::LUK: return "LUK";
				case EquipStat::Id::HP: return "Max HP";
				case EquipStat::Id::MP: return "Max MP";
				case EquipStat::Id::WATK: return "Weapon ATT";
				case EquipStat::Id::MAGIC: return "Magic ATT";
				case EquipStat::Id::WDEF: return "Weapon DEF";
				case EquipStat::Id::MDEF: return "Magic DEF";
				case EquipStat::Id::ACC: return "Accuracy";
				case EquipStat::Id::AVOID: return "Avoid";
				case EquipStat::Id::HANDS: return "Hands";
				case EquipStat::Id::SPEED: return "Speed";
				case EquipStat::Id::JUMP: return "Jump";
				default: return "";
			}
		}

		const char* class_name(uint8_t index)
		{
			static const char* const NAMES[6] = { "Beginner", "Warrior", "Magician", "Bowman", "Thief", "Pirate" };

			return index < 6 ? NAMES[index] : "";
		}

		// Item descriptions carry colour codes (#c ... #); the words stay.
		std::string plain(const std::string& src)
		{
			std::string out;

			for (size_t i = 0; i < src.size(); i++)
			{
				if (src[i] == '\r')
					continue;

				if (src[i] == '\n')
				{
					out += "\\n";
					continue;
				}

				if (src[i] == '#' && i + 1 < src.size() && std::isalpha(static_cast<unsigned char>(src[i + 1])))
				{
					i++;
					continue;
				}

				if (src[i] == '#')
					continue;

				out += src[i];
			}

			return out;
		}

		// "ONE-HANDED SWORD" -> "One-handed Sword".
		std::string title_case(const std::string& src)
		{
			std::string out = src;
			bool start = true;

			for (char& c : out)
			{
				unsigned char u = static_cast<unsigned char>(c);
				c = static_cast<char>(start ? std::toupper(u) : std::tolower(u));
				start = c == ' ';
			}

			return out;
		}

		std::string signed_value(int32_t value)
		{
			return (value >= 0 ? "+" : "") + std::to_string(value);
		}
	}

	ItemDetails::ItemDetails(const Equip& equip, const Equip* worn)
	{
		describe_equip(equip.get_item_id(), &equip, worn);
	}

	ItemDetails::ItemDetails(int32_t item_id)
	{
		if (item_id / 1000000 == 1)
		{
			const Inventory& inventory = Stage::get().get_player().get_inventory();
			auto worn = inventory.get_equip(InventoryType::Id::EQUIPPED, EquipData::get(item_id).get_eqslot());

			describe_equip(item_id, nullptr, worn ? worn.get() : nullptr);
			return;
		}

		description = plain(ItemData::get(item_id).get_desc());
	}

	void ItemDetails::describe_equip(int32_t item_id, const Equip* equip, const Equip* worn)
	{
		const EquipData& data = EquipData::get(item_id);
		const ItemData& item = data.get_itemdata();
		const CharStats& me = Stage::get().get_player().get_stats();

		// What it takes to wear it.
		const MapleStat::Id needs[] = { MapleStat::Id::LEVEL, MapleStat::Id::STR, MapleStat::Id::DEX, MapleStat::Id::INT, MapleStat::Id::LUK };

		for (MapleStat::Id stat : needs)
		{
			int16_t need = data.get_reqstat(stat);

			if (need <= 0)
				continue;

			std::string label = stat == MapleStat::Id::LEVEL ? "Lv " + std::to_string(need)
				: std::string(stat == MapleStat::Id::STR ? "STR " : stat == MapleStat::Id::DEX ? "DEX " : stat == MapleStat::Id::INT ? "INT " : "LUK ")
					+ std::to_string(need);
			requirements.push_back({ label, me.get_stat(stat) >= need });
		}

		int16_t reqjob = data.get_reqstat(MapleStat::Id::JOB);

		if (reqjob != 0)
		{
			std::string classes;

			for (uint8_t c : Job::req_job_classes(reqjob))
				classes += (classes.empty() ? "" : " / ") + std::string(class_name(c));

			requirements.push_back({ classes, me.get_job().meets_req_job(reqjob) });
		}

		int8_t gender = item.get_gender();

		if (gender == 0 || gender == 1)
			requirements.push_back({ gender == 0 ? "Male" : "Female", me.get_female() == (gender == 1) });

		// What it gives, and how that compares with what is worn.
		for (EquipStat::Id es = EquipStat::Id::STR; es <= EquipStat::Id::JUMP; es = static_cast<EquipStat::Id>(es + 1))
		{
			int32_t value = equip ? equip->get_stat(es) : data.get_defstat(es);
			int32_t other = worn ? worn->get_stat(es) : 0;

			if (value <= 0 && other <= 0)
				continue;

			stats.push_back({ stat_name(es), value, value - other, worn != nullptr });
		}

		// The rest, one line each.
		std::string type = title_case(data.get_type());

		if (data.is_weapon())
			type += "  \xC2\xB7  " + WeaponData::get(item_id).getspeedstring() + " speed";

		facts.push_back(type);

		if (equip)
		{
			std::string upgrades = std::to_string(equip->get_slots()) + " upgrade slots left";

			if (equip->get_level() > 0)
				upgrades += "  \xC2\xB7  +" + std::to_string(equip->get_level()) + " from scrolls";

			if (equip->get_vicious() > 0)
				upgrades += "  \xC2\xB7  " + std::to_string(equip->get_vicious()) + " hammer";

			facts.push_back(upgrades);
		}

		if (worn && equip != worn)
			facts.push_back("Compared with your " + ItemData::get(worn->get_item_id()).get_name());

		if (item.is_untradable())
			facts.push_back("Can't be traded");

		description = plain(item.get_desc());
	}

	bool ItemDetails::empty() const
	{
		return requirements.empty() && stats.empty() && facts.empty() && description.empty();
	}

	int16_t ItemDetails::height(int16_t width) const
	{
		return layout(rect(0, 0, width, 0), false);
	}

	void ItemDetails::draw(const Rect& area) const
	{
		layout(area, true);
	}

	int16_t ItemDetails::layout(const Rect& area, bool paint) const
	{
		int16_t x = area.left();
		int16_t w = area.width();
		int16_t y = area.top();
		bool first = true;

		auto section = [&](const char* title)
		{
			if (!first)
				y = static_cast<int16_t>(y + SECTION_GAP);

			first = false;

			if (paint)
				text(title, Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(x, y));

			y = static_cast<int16_t>(y + LABEL_H);
		};

		if (!requirements.empty())
		{
			section("REQUIRES");

			int16_t cx = x;

			for (const Requirement& r : requirements)
			{
				int16_t cw = static_cast<int16_t>(text_width(r.label, Font::SMALL_BOLD) + 18);

				if (cx > x && cx + cw > x + w)
				{
					cx = x;
					y = static_cast<int16_t>(y + CHIP_H + 6);
				}

				if (paint)
				{
					RGBA tint = r.met ? Theme::GREEN : Theme::RED;
					Rect chip = rect(cx, y, cw, CHIP_H);
					fill(chip, tint.alpha(0.16f), 13);
					text_in(r.label, Font::SMALL_BOLD, tint.mix(Theme::TEXT, 0.25f), chip);
				}

				cx = static_cast<int16_t>(cx + cw + 6);
			}

			y = static_cast<int16_t>(y + CHIP_H);
		}

		if (!stats.empty())
		{
			section("STATS");

			// Two columns when they fit, else one.
			size_t cols = w >= 300 ? 2 : 1;
			int16_t col_w = static_cast<int16_t>((w - 12 * (cols - 1)) / cols);

			for (size_t i = 0; i < stats.size(); i++)
			{
				const Stat& s = stats[i];
				int16_t sx = static_cast<int16_t>(x + (i % cols) * (col_w + 12));
				int16_t sy = static_cast<int16_t>(y + (i / cols) * STAT_H);

				if (!paint)
					continue;

				text(s.label, Font::SMALL, Theme::MUTED, Point<int16_t>(sx, sy));

				int16_t right = static_cast<int16_t>(sx + col_w);

				if (s.compared && s.delta != 0)
				{
					std::string arrow = "(" + signed_value(s.delta) + ")";
					text(arrow, Font::SMALL_BOLD, s.delta > 0 ? Theme::GREEN : Theme::RED, Point<int16_t>(right, sy), Text::Alignment::RIGHT);
					right = static_cast<int16_t>(right - text_width(arrow, Font::SMALL_BOLD) - 6);
				}

				text(signed_value(s.value), Font::SMALL_BOLD, s.value > 0 ? Theme::TEXT : Theme::FAINT, Point<int16_t>(right, sy), Text::Alignment::RIGHT);
			}

			y = static_cast<int16_t>(y + ((stats.size() + cols - 1) / cols) * STAT_H);
		}

		if (!facts.empty())
		{
			if (!first)
				y = static_cast<int16_t>(y + SECTION_GAP);

			first = false;

			for (const std::string& f : facts)
			{
				if (paint)
					text(ellipsize(f, Font::SMALL, w), Font::SMALL, Theme::MUTED, Point<int16_t>(x, y));

				y = static_cast<int16_t>(y + FACT_H);
			}
		}

		if (!description.empty())
		{
			if (!first)
				y = static_cast<int16_t>(y + SECTION_GAP);

			if (paint)
				text(description, Font::SMALL, Theme::TEXT.alpha(0.85f), Point<int16_t>(x, y), Text::Alignment::LEFT, w);

			y = static_cast<int16_t>(y + text_height(description, Font::SMALL, w));
		}

		return static_cast<int16_t>(y - area.top());
	}
}
