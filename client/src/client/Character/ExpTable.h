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

#include <array>
#include <stdint.h>

namespace ms
{
	namespace ExpTable
	{
		constexpr size_t LEVELCAP = 201;
		extern const std::array<int64_t, LEVELCAP> values;

		// MapleV83: the highest level (a level 300 character can be reborn).
		constexpr int32_t MAX_LEVEL = 300;

		// EXP to pass a level: v83's table to 200, then 5.55% more per level up
		// to 2,000,000,000 (EXP is a 32-bit count). 0 at MAX_LEVEL. Matches the
		// server's ExpTable.getExpNeededForLevel.
		int64_t need(int32_t level);
	}
}