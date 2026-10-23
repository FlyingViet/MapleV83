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
#pragma once

#include "../../Template/Enumeration.h"
#include "../../Template/EnumMap.h"

namespace ms
{
	namespace EquipSlot
	{
		enum Id : int16_t
		{
			NONE = 0,
			HAT = 1,
			FACE = 2,
			EYEACC = 3,
			EARACC = 4,
			TOP = 5,
			BOTTOM = 6,
			SHOES = 7,
			GLOVES = 8,
			CAPE = 9,
			SHIELD = 10,
			WEAPON = 11,
			RING1 = 12,
			RING2 = 13,
			RING3 = 15,
			RING4 = 16,
			PENDANT1 = 17,
			TAMEDMOB = 18,
			SADDLE = 19,
			MEDAL = 49,
			BELT = 50,
			POCKET,
			BOOK,
			PENDANT2,
			SHOULDER,
			ANDROID,
			EMBLEM,
			BADGE,
			SUBWEAPON,
			HEART,
			LENGTH
		};

		Id by_id(size_t id);

		// Cash (cosmetic) items are worn at their slot + CASH_OFFSET and drawn
		// over the regular gear in that slot.
		constexpr int16_t CASH_OFFSET = 100;

		constexpr int16_t base_of(int16_t slot)
		{
			return slot > CASH_OFFSET ? static_cast<int16_t>(slot - CASH_OFFSET) : slot;
		}

		constexpr Enumeration<Id> values;

		// Pet gear (all cash) sits in v83's pet slots: the pet's own
		// accessory at 114, its ability items (Item Pouch, Meso Magnet ...)
		// at 121-130.
		namespace Pet
		{
			constexpr int16_t WEAR = 114;
			constexpr int16_t LABEL = 121;
			constexpr int16_t ITEM_POUCH = 122;
			constexpr int16_t MESO_MAGNET = 123;
			constexpr int16_t AUTO_HP = 124;
			constexpr int16_t AUTO_MP = 125;
			constexpr int16_t WING_BOOTS = 126;
			constexpr int16_t BINOCULAR = 127;
			constexpr int16_t MAGIC_SCALES = 128;
			constexpr int16_t ITEM_IGNORE = 129;
			constexpr int16_t QUOTE = 130;
		}

		constexpr bool is_pet(int16_t slot)
		{
			return slot == Pet::WEAR || (slot >= Pet::LABEL && slot <= Pet::QUOTE);
		}

		// Where a piece of pet gear is worn; 0 for anything else.
		int16_t pet_slot(int32_t item_id);
	};
}
