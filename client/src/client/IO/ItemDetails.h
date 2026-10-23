//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "MobileUI.h"

#include <string>
#include <vector>

namespace ms
{
	class Equip;

	// An item explained for the phone: what a piece of equipment needs, what
	// it gives (against what is worn in its slot) and an item's description.
	class ItemDetails
	{
	public:
		// A piece carried or worn; worn is what sits in its slot now, to
		// compare against (null for none, or for the worn piece itself).
		ItemDetails(const Equip& equip, const Equip* worn);
		// An item by id only (shop stock): equipment shows its base stats.
		explicit ItemDetails(int32_t item_id);

		int16_t height(int16_t width) const;
		// Lays out from the top-left of area, area.width() wide.
		void draw(const MobileUI::Rect& area) const;

		bool empty() const;

	private:
		struct Requirement
		{
			std::string label;
			bool met;
		};

		struct Stat
		{
			std::string label;
			int32_t value;
			int32_t delta;
			bool compared;
		};

		void describe_equip(int32_t item_id, const Equip* equip, const Equip* worn);
		int16_t layout(const MobileUI::Rect& area, bool paint) const;

		std::vector<Requirement> requirements;
		std::vector<Stat> stats;
		std::vector<std::string> facts;
		std::string description;
	};
}
