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

#include <unordered_map>

namespace ms
{
	// MapleV83: power stats are uncapped (v83 stopped STR/DEX/INT/LUK/ATT at 999,
	// HP/MP at 30000). Speed and jump keep their v83 limits because movement
	// physics and the server's movement checks are built around them.
	const std::unordered_map<EquipStat::Id, int32_t> EQSTAT_CAPS =
	{
		{ EquipStat::Id::SPEED,	140		},
		{ EquipStat::Id::JUMP,	123		},
	};
}