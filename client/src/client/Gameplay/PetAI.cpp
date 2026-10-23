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
#include "PetAI.h"

#include "MapleMap/Drop.h"
#include "MapleMap/MapDrops.h"
#include "MapleMap/MesoDrop.h"

#include "../Character/Player.h"
#include "../Net/Packets/InventoryPackets.h"

#include <cmath>

namespace ms
{
	void PetAI::update(Player& player, MapDrops& drops)
	{
		for (auto it = asked.begin(); it != asked.end();)
		{
			if (--it->second <= 0)
				it = asked.erase(it);
			else
				++it;
		}

		if (--loot_cd > 0)
			return;

		loot_cd = 10;

		Point<int16_t> cpos = player.get_position();
		MapObjects* dobjs = drops.get_drops();

		// Pets fetch what their gear lets them: an Item Pouch for items, a
		// Meso Magnet for mesos (the server refuses anything else).
		const Inventory& inventory = player.get_inventory();
		bool items = inventory.get_item_id(InventoryType::Id::EQUIPPED, EquipSlot::Pet::ITEM_POUCH) != 0
			|| inventory.find_item(InventoryType::Id::EQUIPPED, 1812001) != 0;
		bool mesos = inventory.get_item_id(InventoryType::Id::EQUIPPED, EquipSlot::Pet::MESO_MAGNET) != 0
			|| inventory.find_item(InventoryType::Id::EQUIPPED, 1812000) != 0;

		if (!items && !mesos)
			return;

		for (uint8_t pi = 0; pi < 3; pi++)
		{
			PetLook& pet = player.get_pet(pi);

			if (pet.get_itemid() == 0)
				continue;

			Point<int16_t> ppos = pet.get_position();
			MapObject* nearest = nullptr;
			int32_t bestdist = 200;

			for (auto it = dobjs->begin(); it != dobjs->end(); ++it)
			{
				MapObject* mo = it->second.get();

				if (!mo || !mo->is_active() || asked.count(mo->get_oid()))
					continue;

				if (auto drop = dynamic_cast<Drop*>(mo))
					if (drop->is_leaving() || !drop->is_landed())
						continue;

				bool is_meso = dynamic_cast<MesoDrop*>(mo) != nullptr;

				if (is_meso ? !mesos : !items)
					continue;

				Point<int16_t> dpos = mo->get_position();

				if (std::abs(dpos.x() - ppos.x()) <= 30 && std::abs(dpos.y() - ppos.y()) <= 25)
				{
					PetLootPacket(pet.get_uniqueid(), mo->get_oid()).dispatch();
					asked[mo->get_oid()] = 150;
					pet.clear_loot_target();
					nearest = nullptr;
					loot_cd = 60;
					break;
				}

				int32_t dist = std::abs(dpos.x() - cpos.x()) + std::abs(dpos.y() - cpos.y());

				if (dist < bestdist)
				{
					bestdist = dist;
					nearest = mo;
				}
			}

			if (nearest)
				pet.set_loot_target(nearest->get_position());
			else if (loot_cd != 60)
				pet.clear_loot_target();
		}
	}
}
