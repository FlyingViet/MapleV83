//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileSkills.h"

#include "UIMobileConfirm.h"
#include "UIMobileKeyConfig.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Character/SkillId.h"
#include "../../Constants.h"
#include "../../Data/JobData.h"
#include "../../Data/SkillData.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/MessagingPackets.h"
#include "../../Net/Packets/PlayerPackets.h"

#include <algorithm>
#include <unordered_set>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 400;
		constexpr int16_t PAD = 16;
		constexpr int16_t TABS_Y = 76;
		constexpr int16_t SP_Y = 120;
		constexpr int16_t LIST_Y = 182;
		constexpr int16_t ROW_H = 58;
		constexpr int16_t ROW_STEP = 64;
		constexpr int16_t DETAIL_H = 150;

		// Descriptions use "\n" and "\r" pairs; the card flows them as one
		// paragraph.
		std::string flatten(std::string s)
		{
			for (const char* br : { "\\r\\n", "\\n", "\r\n", "\n", "\r" })
			{
				size_t at = 0;
				std::string from = br;

				while ((at = s.find(from, at)) != std::string::npos)
				{
					s.replace(at, from.size(), " ");
					at += 1;
				}
			}

			return s;
		}
	}

	UIMobileSkills::UIMobileSkills(const CharStats& in_stats, const SkillBook& in_skillbook) :
		UIMobilePanel(Edge::LEFT, W), stats(in_stats), skillbook(in_skillbook)
	{
		rebuild();
		select_tab(tab_count() - 1);
	}

	void UIMobileSkills::toggle_active()
	{
		UIMobilePanel::toggle_active();

		if (active)
		{
			rebuild();
			select_tab(tab_count() - 1);
		}
	}

	// ---- data ---------------------------------------------------------------

	int UIMobileSkills::tab_count() const
	{
		return job.get_level() - Job::Level::BEGINNER + 1;
	}

	Job::Level UIMobileSkills::tab_level(int index) const
	{
		return static_cast<Job::Level>(Job::Level::BEGINNER + index);
	}

	bool UIMobileSkills::beginner_tab() const
	{
		return tab_level(tab) == Job::Level::BEGINNER;
	}

	void UIMobileSkills::rebuild()
	{
		uint16_t id = stats.get_stat(MapleStat::Id::JOB);

		if (id != last_job)
		{
			job.change_job(id);
			last_job = id;
		}

		entries.clear();

		Job::Level level = tab_level(tab);
		uint16_t subid = job.get_subjob(level);
		const JobData& data = JobData::get(subid);
		std::unordered_set<int32_t> seen;

		// The job's whole tree from Skill.wz, unlearned ones included, plus
		// anything the server granted under the same prefix (as UISkillBook).
		auto push = [&](int32_t sid)
		{
			bool common = sid / 10000 == 0 && level == Job::Level::BEGINNER;

			if (sid / 10000 != static_cast<int32_t>(subid) && !common)
				return;

			if (SkillData::get(sid).is_invisible() && skillbook.get_level(sid) == 0)
				return;

			if (seen.insert(sid).second)
				entries.push_back({ sid });
		};

		for (int32_t sid : data.get_skills())
			push(sid);

		for (const auto& entry : skillbook.get_entries())
			push(entry.first);

		clamp_scroll();
	}

	void UIMobileSkills::select_tab(int index)
	{
		tab = std::clamp(index, 0, std::max(0, tab_count() - 1));
		scroll = 0;
		selected = 0;
		pending = 0;
		pending_skill = 0;
		pending_skill_levels = 0;
		rebuild();
		last_pool = pool();
	}

	int32_t UIMobileSkills::pool() const
	{
		if (!beginner_tab())
			return stats.get_stat(MapleStat::Id::SP);

		// Beginners get one point a level up to 6 (level 7), shared by
		// Three Snails, Recovery and Nimble Feet.
		uint16_t level = stats.get_stat(MapleStat::Id::LEVEL);
		int32_t total = level >= 7 ? 6 : static_cast<int32_t>(level) - 1;
		int32_t prefix = job.get_subjob(Job::Level::BEGINNER);

		for (const auto& entry : skillbook.get_entries())
		{
			int32_t base = entry.first % 10000;

			if (entry.first / 10000 == prefix && (base == 1000 || base == 1001 || base == 1002))
				total -= entry.second.level;
		}

		return std::max<int32_t>(0, total);
	}

	int32_t UIMobileSkills::available() const
	{
		return std::max(0, pool() - pending);
	}

	int32_t UIMobileSkills::level_of(int32_t skill) const
	{
		return skillbook.get_level(skill) + (skill == pending_skill ? pending_skill_levels : 0);
	}

	int32_t UIMobileSkills::master_of(int32_t skill) const
	{
		int32_t master = skillbook.get_masterlevel(skill);

		return master > 0 ? master : SkillData::get(skill).get_masterlevel();
	}

	bool UIMobileSkills::requirements_met(int32_t skill) const
	{
		return missing_requirement(skill).empty();
	}

	std::string UIMobileSkills::missing_requirement(int32_t skill) const
	{
		std::unordered_map<int32_t, int32_t> required = skillbook.collect_required(skill);

		if (required.empty())
			required = SkillData::get(skill).get_reqskills();

		for (const auto& [id, level] : required)
			if (level_of(id) < level)
				return "Needs " + SkillData::get(id).get_name() + " Lv " + std::to_string(level);

		return "";
	}

	bool UIMobileSkills::can_raise(int32_t skill) const
	{
		if (available() <= 0)
			return false;

		if (skill % 10000 == SkillId::Id::ANGEL_BLESSING)
			return false;

		if (level_of(skill) >= master_of(skill))
			return false;

		return requirements_met(skill);
	}

	void UIMobileSkills::raise(int32_t skill)
	{
		if (!can_raise(skill))
			return;

		if (skill != pending_skill)
		{
			pending_skill = skill;
			pending_skill_levels = 0;
			last_skill_level = skillbook.get_level(skill);
		}

		if (pending == 0)
			Sound(Sound::Name::BUTTONCLICK).play();

		SpendSpPacket(skill).dispatch();
		pending++;
		pending_skill_levels++;
		pending_ms = 0;
	}

	void UIMobileSkills::raise_max(int32_t skill)
	{
		while (can_raise(skill))
			raise(skill);
	}

	void UIMobileSkills::update()
	{
		UIMobilePanel::update();

		if (stats.get_stat(MapleStat::Id::JOB) != last_job)
		{
			rebuild();
			select_tab(tab_count() - 1);
		}

		int32_t p = pool();

		if (p != last_pool)
		{
			pending = p < last_pool ? std::max(0, pending - (last_pool - p)) : 0;
			last_pool = p;
			pending_ms = 0;
		}
		else if (pending > 0)
		{
			pending_ms += Constants::TIMESTEP;

			if (pending_ms > 1500)
			{
				pending = 0;
				pending_skill_levels = 0;
			}
		}

		if (pending_skill)
		{
			int32_t level = skillbook.get_level(pending_skill);

			if (level != last_skill_level)
			{
				pending_skill_levels = level > last_skill_level
					? std::max(0, pending_skill_levels - (level - last_skill_level)) : 0;
				last_skill_level = level;
			}
		}

		// A skill the server just granted joins the list.
		size_t known = 0;

		for (const Entry& e : entries)
			if (skillbook.get_level(e.id) > 0)
				known++;

		size_t learned = 0;
		uint16_t subid = job.get_subjob(tab_level(tab));

		for (const auto& entry : skillbook.get_entries())
			if (entry.second.level > 0 && (entry.first / 10000 == subid || (beginner_tab() && entry.first / 10000 == 0)))
				learned++;

		if (learned > known)
			rebuild();
	}

	// ---- layout -------------------------------------------------------------

	Rect UIMobileSkills::tab_rect(int index) const
	{
		int count = tab_count();
		int16_t gap = 6;
		int16_t w = static_cast<int16_t>((W - PAD * 2 - gap * (count - 1)) / std::max(1, count));

		return rect(static_cast<int16_t>(position.x() + PAD + index * (w + gap)), position.y() + TABS_Y, w, 34);
	}

	Rect UIMobileSkills::reset_rect() const
	{
		return rect(static_cast<int16_t>(position.x() + W - PAD - 12 - 84), position.y() + SP_Y + 9, 84, 36);
	}

	int16_t UIMobileSkills::list_bottom() const
	{
		return static_cast<int16_t>(position.y() + HEIGHT - 12 - (selected ? DETAIL_H + 8 : 0));
	}

	Rectangle<int16_t> UIMobileSkills::scroll_area() const
	{
		return Rectangle<int16_t>(static_cast<int16_t>(position.x() + PAD), static_cast<int16_t>(position.x() + W - PAD),
			static_cast<int16_t>(position.y() + LIST_Y), list_bottom());
	}

	int16_t UIMobileSkills::scroll_content() const
	{
		return static_cast<int16_t>(entries.size() * ROW_STEP - (ROW_STEP - ROW_H));
	}

	Rect UIMobileSkills::row_rect(size_t index) const
	{
		return rect(position.x() + PAD, static_cast<int16_t>(position.y() + LIST_Y + index * ROW_STEP - scroll), W - PAD * 2, ROW_H);
	}

	Rect UIMobileSkills::raise_rect(size_t index) const
	{
		Rect row = row_rect(index);

		return rect(static_cast<int16_t>(row.right() - 8 - 42), static_cast<int16_t>(row.top() + 8), 42, 42);
	}

	Rect UIMobileSkills::detail_rect() const
	{
		return rect(position.x() + PAD, static_cast<int16_t>(position.y() + HEIGHT - 12 - DETAIL_H), W - PAD * 2, DETAIL_H);
	}

	Rect UIMobileSkills::max_rect() const
	{
		Rect d = detail_rect();

		return rect(static_cast<int16_t>(d.left() + 12), static_cast<int16_t>(d.bottom() - 12 - 40), 110, 40);
	}

	Rect UIMobileSkills::bind_rect() const
	{
		Rect d = detail_rect();
		Rect m = max_rect();

		return rect(static_cast<int16_t>(m.right() + 10), m.top(), static_cast<int16_t>(d.right() - 12 - m.right() - 10), 40);
	}

	// ---- input --------------------------------------------------------------

	int32_t UIMobileSkills::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		for (int i = 0; i < tab_count(); i++)
			if (tab_rect(i).contains(pos))
				return TAB + i;

		if (reset_rect().contains(pos))
			return RESET;

		if (selected)
		{
			if (max_rect().contains(pos))
				return MAX;

			if (bind_rect().contains(pos))
				return BIND;

			if (detail_rect().contains(pos))
				return NO_TARGET;
		}

		if (scroll_area().contains(pos))
		{
			for (size_t i = 0; i < entries.size(); i++)
			{
				if (!row_rect(i).contains(pos))
					continue;

				if (inset(raise_rect(i), -10, -8).contains(pos))
					return RAISE + static_cast<int32_t>(i);

				return ROW + static_cast<int32_t>(i);
			}
		}

		return NO_TARGET;
	}

	bool UIMobileSkills::repeats(int32_t target) const
	{
		return target >= RAISE && target < RAISE + static_cast<int32_t>(entries.size());
	}

	void UIMobileSkills::activate(int32_t target)
	{
		if (target >= RAISE)
		{
			size_t i = static_cast<size_t>(target - RAISE);

			if (i < entries.size())
			{
				selected = entries[i].id;
				raise(entries[i].id);
			}

			return;
		}

		if (target >= ROW)
		{
			size_t i = static_cast<size_t>(target - ROW);

			if (i < entries.size())
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				selected = selected == entries[i].id ? 0 : entries[i].id;
				clamp_scroll();
			}

			return;
		}

		if (target >= TAB)
		{
			if (target - TAB != tab)
			{
				Sound(Sound::Name::TAB).play();
				select_tab(target - TAB);
			}

			return;
		}

		switch (target)
		{
			case RESET:
				UI::get().emplace<UIMobileConfirm>("Reset skill points",
					"Every point you put into your job skills comes back to spend again. Free, as often as you like.",
					"Reset", true,
					[]()
					{
						GeneralChatPacket("@resetsp", false).dispatch();
					});
				break;
			case MAX:
				raise_max(selected);
				break;
			case BIND:
				if (selected && skillbook.get_level(selected) > 0 && !SkillData::get(selected).is_passive())
				{
					Sound(Sound::Name::BUTTONCLICK).play();
					UIMobileKeyConfig::assign(KeyType::Id::SKILL, selected);
				}
				break;
		}
	}

	// ---- drawing ------------------------------------------------------------

	void UIMobileSkills::draw(float) const
	{
		Rect f = frame();
		int16_t x = f.left();
		int16_t y = f.top();
		int32_t pressed = pressed_target();

		const std::string& tab_name = JobData::get(job.get_subjob(tab_level(tab))).get_name();
		panel(f, "Skills", tab_name);
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		// One tab per advancement: the job's own name when there is room.
		{
			int count = tab_count();
			static const char* ORDINALS[] = { "Beginner", "1st", "2nd", "3rd", "4th" };

			for (int i = 0; i < count; i++)
			{
				Rect r = tab_rect(i);
				std::string name = JobData::get(job.get_subjob(tab_level(i))).get_name();

				if (text_width(name, Font::SMALL_BOLD) > r.width() - 12)
					name = ORDINALS[std::min(i, 4)];

				button(r, name, i == tab ? Style::SELECTED : Style::GHOST, true, pressed == TAB + i, Font::SMALL_BOLD);
			}
		}

		// Skill points.
		int32_t sp = available();
		{
			Rect card = rect(x + PAD, y + SP_Y, W - PAD * 2, 54);
			fill(card, Theme::CARD, 14);
			text(beginner_tab() ? "BEGINNER SKILL POINTS" : "SKILL POINTS", Font::SMALL_BOLD, Theme::MUTED,
				Point<int16_t>(card.left() + 14, card.top() + 8));
			text(std::to_string(sp), Font::HEADLINE, sp > 0 ? Theme::GOLD : Theme::FAINT,
				Point<int16_t>(card.left() + 14, card.top() + 23));
			button(reset_rect(), "Reset", Style::DANGER, true, pressed == RESET);
		}

		// The list, cut to its area as it scrolls.
		Rectangle<int16_t> area = scroll_area();
		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		if (entries.empty())
			text_in("No skills here yet.", Font::BODY, Theme::FAINT, rect(area.left(), area.top(), area.width(), 60));

		for (size_t i = 0; i < entries.size(); i++)
		{
			Rect row = row_rect(i);

			if (row.bottom() < area.top() || row.top() > area.bottom())
				continue;

			int32_t id = entries[i].id;
			const SkillData& data = SkillData::get(id);
			int32_t level = level_of(id);
			int32_t master = master_of(id);
			bool raisable = can_raise(id);
			bool is_selected = id == selected;
			bool row_pressed = pressed == ROW + static_cast<int32_t>(i);

			fill(row, is_selected || row_pressed ? Theme::CARD_HI : Theme::CARD, 12);

			if (is_selected)
				fill(rect(row.left(), static_cast<int16_t>(row.top() + 12), 4, static_cast<int16_t>(ROW_H - 24)), Theme::ACCENT, 2);

			Rect icon_box = rect(static_cast<int16_t>(row.left() + 10), static_cast<int16_t>(row.top() + 7), 44, 44);
			fill(icon_box, Theme::TRACK, 10);
			icon(data.get_icon(level > 0 || raisable ? SkillData::Icon::NORMAL : SkillData::Icon::DISABLED),
				inset(icon_box, 4, 4), level > 0 || raisable ? 1.0f : 0.55f);

			int16_t tx = static_cast<int16_t>(row.left() + 66);
			int16_t name_w = static_cast<int16_t>(raise_rect(i).left() - tx - 8);
			text(ellipsize(data.get_name(), Font::BODY_BOLD, name_w), Font::BODY_BOLD, level > 0 ? Theme::TEXT : Theme::MUTED,
				Point<int16_t>(tx, static_cast<int16_t>(row.top() + 9)));

			std::string missing = level < master ? missing_requirement(id) : "";
			std::string sub = !missing.empty() ? missing
				: "Lv " + std::to_string(level) + " / " + std::to_string(master) + (data.is_passive() ? "   Passive" : "");
			text(ellipsize(sub, Font::SMALL, name_w), Font::SMALL, !missing.empty() ? Theme::RED.mix(Theme::MUTED, 0.35f) : Theme::MUTED,
				Point<int16_t>(tx, static_cast<int16_t>(row.top() + 29)));

			// Progress to the master level.
			if (master > 0 && missing.empty())
			{
				Rect track = rect(static_cast<int16_t>(tx + 120), static_cast<int16_t>(row.top() + 35), 100, 5);
				bar(track, static_cast<float>(level) / master, level >= master ? Theme::GREEN : Theme::ACCENT);
			}

			if (master > 0 && level < master)
				icon_button(raise_rect(i), Glyph::PLUS, Style::PRIMARY, raisable, pressed == RAISE + static_cast<int32_t>(i));
			else if (master > 0)
				text_in("MAX", Font::SMALL_BOLD, Theme::GREEN, raise_rect(i));
		}

		GraphicsGL::get().end_clip();

		// Scroll hint: a thin thumb on the right edge.
		if (scroll_max() > 0)
		{
			float visible = static_cast<float>(area.height()) / scroll_content();
			int16_t thumb = static_cast<int16_t>(std::max(24.0f, area.height() * visible));
			int16_t ty = static_cast<int16_t>(area.top() + (area.height() - thumb) * (static_cast<float>(scroll) / scroll_max()));
			fill(rect(static_cast<int16_t>(x + W - 8), ty, 3, thumb), Theme::FAINT.alpha(0.7f), 1);
		}

		// The selected skill.
		if (selected)
		{
			const SkillData& data = SkillData::get(selected);
			int32_t level = level_of(selected);
			int32_t master = master_of(selected);
			Rect d = detail_rect();
			fill(d, Theme::CARD, 14);

			text(data.get_name(), Font::TITLE, Theme::TEXT, Point<int16_t>(d.left() + 14, d.top() + 10));
			text("Lv " + std::to_string(level) + " / " + std::to_string(master), Font::SMALL_BOLD, Theme::GOLD,
				Point<int16_t>(d.right() - 14, d.top() + 13), Text::Alignment::RIGHT);

			std::string body = level < master && level + 1 <= master
				? "Next: " + flatten(data.get_level_desc(level + 1))
				: flatten(data.get_level_desc(level));

			if (body.size() <= 6)
				body = flatten(data.get_desc());

			GraphicsGL::get().begin_clip(static_cast<int16_t>(d.top() + 32), static_cast<int16_t>(d.top() + 92));
			text(body, Font::SMALL, Theme::MUTED, Point<int16_t>(d.left() + 14, d.top() + 34), Text::Alignment::LEFT, d.width() - 28);
			GraphicsGL::get().end_clip();

			bool bindable = skillbook.get_level(selected) > 0 && !data.is_passive();
			button(max_rect(), "Max", Style::SECONDARY, can_raise(selected), pressed == MAX);
			button(bind_rect(), bindable ? "Put on a touch button" : (data.is_passive() ? "Passive skill" : "Learn it first"),
				Style::SECONDARY, bindable, pressed == BIND);
		}
	}

	UIElement::Type UIMobileSkills::get_type() const
	{
		return TYPE;
	}
}
