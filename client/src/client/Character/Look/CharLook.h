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

#include "Body.h"
#include "CharEquips.h"
#include "Face.h"
#include "Hair.h"

#include "../../Net/Login.h"
#include "../../Template/Interpolated.h"
#include "../../Util/Randomizer.h"
#include "../../Util/TimedBool.h"

namespace ms
{
	class CharLook
	{
	public:
		CharLook(const LookEntry& entry);
		CharLook();

		void reset();
		void draw(const DrawArgument& args, float alpha) const;
		void draw(Point<int16_t> pos, bool flip, Stance::Id stance, Expression::Id expression) const;
		bool update(uint16_t timestep);

		void set_hair(int32_t hairid);
		void set_body(int32_t skinid);
		void set_face(int32_t faceid);
		void add_equip(int32_t equipid);
		void remove_equip(EquipSlot::Id slot);
		// The cash weapon drawn over the real one (0: none).
		void set_weapon_skin(int32_t itemid);
		// The Cash Shop's try-on: like add_equip, but a cash weapon shows even
		// over a weapon type it has no art for.
		void try_on(int32_t itemid);

		void attack(bool degenerate);
		void attack(Stance::Id stance);
		void set_stance(Stance::Id stance);
		// Force a stance even if an action (e.g. an attack) is playing, by
		// cancelling that action first. Used for death, which must override
		// whatever the character was doing when HP hit 0.
		void set_stance_forced(Stance::Id stance);
		void set_expression(Expression::Id expression);
		void set_action(const std::string& action);
		void set_direction(bool mirrored);
		void set_alerted(int64_t millis);
		bool get_alerted() const;
		// World position used as the audio anchor for this look's sounds
		// (attack swings), so they attenuate by distance from the listener.
		void set_sound_position(Point<int16_t> position);

		bool is_twohanded(Stance::Id stance) const;
		uint16_t get_attackdelay(size_t no, uint8_t first_frame) const;
		uint8_t get_frame() const;
		Stance::Id get_stance() const;

		const Body* get_body() const;
		const Hair* get_hair() const;
		const Face* get_face() const;
		int32_t get_skin_id() const { return skin_id; }
		int32_t get_hair_id() const { return hair_id; }
		int32_t get_face_id() const { return face_id; }
		const CharEquips& get_equips() const;

		// Initialize drawinfo
		static void init();

		// Shared body skeleton data (per-frame anchors) — used by aura pivots
		// to track the head bob
		static const BodyDrawInfo& get_drawinfo();

	private:
		void updatetwohanded();
		void draw(const DrawArgument& args, Stance::Id interstance, Expression::Id interexp, uint8_t interframe, uint8_t interfcframe) const;
		uint16_t get_delay(Stance::Id stance, uint8_t frame) const;
		uint8_t getnextframe(Stance::Id stance, uint8_t frame) const;
		Stance::Id getattackstance(uint8_t attack, bool degenerate) const;

		Nominal<Stance::Id> stance;
		Nominal<uint8_t> stframe;
		uint16_t stelapsed;

		Nominal<Expression::Id> expression;
		Nominal<uint8_t> expframe;
		uint16_t expelapsed;
		TimedBool expcooldown;

		bool flip;

		Point<int16_t> soundposition;

		const BodyAction* action;
		std::string actionstr;
		uint8_t actframe;

		const Body* body;
		const Hair* hair;
		const Face* face;
		CharEquips equips;

		Randomizer randomizer;
		TimedBool alerted;

		int32_t skin_id = 0;
		int32_t hair_id = 0;
		int32_t face_id = 0;
		static BodyDrawInfo drawinfo;
		static std::unordered_map<int32_t, Hair> hairstyles;
		static std::unordered_map<int32_t, Face> facetypes;
		static std::unordered_map<int32_t, Body> bodytypes;
	};
}