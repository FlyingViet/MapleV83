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

#include "../../Graphics/Animation.h"

namespace ms
{
	class Portal
	{
	public:
		enum Type
		{
			SPAWN,
			INVISIBLE,
			REGULAR,
			TOUCH,
			TYPE4,
			TYPE5,
			WARP,
			SCRIPTED,
			SCRIPTED_INVISIBLE,
			SCRIPTED_TOUCH,
			HIDDEN,
			SCRIPTED_HIDDEN,
			SPRING1,
			SPRING2,
			TYPE14
		};

		static Type typebyid(int32_t id)
		{
			return static_cast<Type>(id);
		}

		struct WarpInfo
		{
			int32_t mapid;
			std::string toname;
			std::string name;
			bool intramap;
			bool valid;
			// Entered through a server portal script (Free Market entrances, ships...):
			// these have no target map and use CHANGE_MAP_SPECIAL.
			bool scripted;

			WarpInfo(int32_t m, bool i, std::string tn, std::string n, bool s = false) : mapid(m), intramap(i), toname(tn), name(n), scripted(s)
			{
				valid = mapid < 999999999;
			}

			WarpInfo() : WarpInfo(999999999, false, {}, {}) {}

			// Something happens when the player enters it.
			bool usable() const
			{
				return valid || scripted;
			}
		};

		Portal(const Animation* animation, Type type, std::string name, bool intramap, Point<int16_t> position, int32_t tomap, std::string toname, bool scripted = false);
		Portal();

		void update(Point<int16_t> playerpos);
		void draw(Point<int16_t> viewpos, float alpha) const;

		std::string get_name() const;
		Type get_type() const;
		Point<int16_t> get_position() const;
		Rectangle<int16_t> bounds() const;

		WarpInfo getwarpinfo() const;

	private:
		const Animation* animation;
		Type type;
		std::string name;
		Point<int16_t> position;
		WarpInfo warpinfo;
		bool touched;
	};
}