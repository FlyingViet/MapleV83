//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
//
// What the player can do with an item, shared by the classic windows and the
// iPhone panels so both behave the same.
#pragma once

#include "Components/Icon.h"

#include "../Character/Inventory/InventoryType.h"
#include "../Character/Look/EquipSlot.h"

#include <memory>

namespace ms
{
	namespace ItemActions
	{
		// Whether the equip in this Equip-tab slot can be worn now; says why not.
		bool can_wear(int16_t slot);

		// Gender, job, level, stats and fame allow this equip (no notice).
		bool meets_requirements(int32_t item_id);

		// The worn slot an equip goes to: its own slot, or that + CASH_OFFSET
		// for cash (cosmetic) items, which show over the gear. `near` picks
		// among the ring slots.
		int16_t wear_slot(int32_t item_id, int16_t near = 0);

		// Whether the equip belongs in this worn slot (gear or cosmetic layer).
		bool fits_slot(int32_t item_id, int16_t slot);

		// A double click: equip, use, sit, spawn the pet, open the megaphone ...
		void use(InventoryType::Id tab, int16_t slot);

		// Take a worn item off into the first free Equip slot.
		void unequip(EquipSlot::Id slot);

		// Throw an item on the ground, asking how many / confirming first.
		void drop(InventoryType::Id tab, int16_t slot, int32_t item_id, int16_t count, bool untradable, bool cash);

		// Icons that carry an inventory item or a worn item to its next drop.
		std::unique_ptr<Icon> make_item_icon(InventoryType::Id tab, int16_t slot);
		std::unique_ptr<Icon> make_worn_icon(EquipSlot::Id slot);
	}
}
