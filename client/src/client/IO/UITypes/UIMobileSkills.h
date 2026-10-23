//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../MobileUI.h"

#include "../../Character/CharStats.h"
#include "../../Character/Job.h"
#include "../../Character/SkillBook.h"

#include <vector>

namespace ms
{
	// The iPhone's skill book: one tab per job advancement, the SP left to
	// spend, a list with a "+" per skill (tap for a level, hold to keep
	// going), and a card for the selected skill with Max and "put on a touch
	// button". Levels go to the server as they are placed; the free reset
	// gives the points back.
	class UIMobileSkills : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILESKILLS;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileSkills(const CharStats& stats, const SkillBook& skillbook);

		void draw(float alpha) const override;
		void update() override;
		void toggle_active() override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		bool repeats(int32_t target) const override;

		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		enum Target : int32_t
		{
			RESET = 1,
			MAX,
			BIND,
			TAB = 100,
			ROW = 1000,
			RAISE = 3000
		};

		struct Entry
		{
			int32_t id;
		};

		void rebuild();
		void select_tab(int index);

		Job::Level tab_level(int index) const;
		int tab_count() const;
		bool beginner_tab() const;

		int32_t pool() const;
		int32_t available() const;
		int32_t level_of(int32_t skill) const;
		int32_t master_of(int32_t skill) const;
		bool requirements_met(int32_t skill) const;
		std::string missing_requirement(int32_t skill) const;
		bool can_raise(int32_t skill) const;
		void raise(int32_t skill);
		void raise_max(int32_t skill);

		MobileUI::Rect tab_rect(int index) const;
		MobileUI::Rect reset_rect() const;
		MobileUI::Rect row_rect(size_t index) const;
		MobileUI::Rect raise_rect(size_t index) const;
		MobileUI::Rect detail_rect() const;
		MobileUI::Rect max_rect() const;
		MobileUI::Rect bind_rect() const;
		int16_t list_bottom() const;

		const CharStats& stats;
		const SkillBook& skillbook;

		Job job;
		uint16_t last_job = 0xFFFF;
		int tab = 0;
		std::vector<Entry> entries;
		int32_t selected = 0;

		// Levels sent but not yet confirmed, so a held "+" stops in time.
		int32_t pending = 0;
		int32_t last_pool = 0;
		int32_t pending_ms = 0;
		int32_t pending_skill = 0;
		int32_t pending_skill_levels = 0;
		int32_t last_skill_level = 0;
	};
}
