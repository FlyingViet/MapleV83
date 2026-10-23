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

#include <string>
#include <vector>

namespace ms
{
	class QuestLog;

	// The iPhone's quest journal: quests in progress, ones the character can
	// pick up, and finished ones, with what each needs, who to talk to, where
	// they are and what it pays.
	class UIMobileQuests : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEQUESTS;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileQuests(const QuestLog& questlog);

		void draw(float alpha) const override;
		void update() override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		enum Tab { ACTIVE, AVAILABLE, DONE, NUM_TABS };

		struct Entry
		{
			int16_t id;
			std::string name;
			std::string detail;
			int32_t level;
			bool ready;
		};

		// A requirement with how far along it is.
		struct Need
		{
			std::string label;
			int32_t have;
			int32_t need;
			int32_t icon_item;
		};

		struct Reward
		{
			std::string label;
			int32_t item;
		};

		// The quest open in the detail view.
		struct Detail
		{
			int16_t id = -1;
			Tab tab = ACTIVE;
			std::string name;
			std::string text;
			std::vector<Need> needs;
			std::vector<Reward> rewards;
			int32_t npc = 0;
			std::string npc_name;
			std::string npc_where;
			bool npc_here = false;
			bool ready = false;
		};

		void rebuild();
		void open_detail(int16_t id, Tab tab);
		void refresh_detail();
		int16_t detail_height() const;
		const std::vector<Entry>& shown() const;

		MobileUI::Rect tab_rect(int tab) const;
		MobileUI::Rect row_rect(size_t index) const;
		MobileUI::Rect back_rect() const;
		MobileUI::Rect action_rect(int index, int count) const;
		std::vector<int32_t> actions() const;

		const QuestLog& questlog;
		Tab tab = ACTIVE;
		std::vector<Entry> lists[NUM_TABS];
		Detail detail;
		bool detail_open = false;

		// What the lists were built from, to notice changes.
		size_t seen_started = 0;
		size_t seen_completed = 0;
		std::string seen_progress;
		int16_t seen_level = 0;
		int32_t refresh_ticks = 0;
	};
}
