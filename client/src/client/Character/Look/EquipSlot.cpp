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
#include "EquipSlot.h"

namespace ms
{
	namespace EquipSlot
	{
		Id by_id(size_t id)
		{
			if (id >= Id::LENGTH)
				return Id::NONE;

			return static_cast<Id>(id);
		}

		int16_t pet_slot(int32_t item_id)
		{
			switch (item_id / 10000)
			{
				case 180: return Pet::WEAR;
				case 182: return Pet::LABEL;
				case 183: return Pet::QUOTE;
				case 181:
					switch (item_id)
					{
						case 1812000: return Pet::MESO_MAGNET;
						case 1812001: return Pet::ITEM_POUCH;
						case 1812002: return Pet::AUTO_HP;
						case 1812003: return Pet::AUTO_MP;
						case 1812004: return Pet::WING_BOOTS;
						case 1812005: return Pet::BINOCULAR;
						case 1812006: return Pet::MAGIC_SCALES;
						case 1812007: return Pet::ITEM_IGNORE;
						default: return 0;
					}
				default:
					return 0;
			}
		}
	}
}
