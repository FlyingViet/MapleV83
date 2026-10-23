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

#include "RegularAttack.h"

#include "../MapleMap/MapChars.h"
#include "../MapleMap/MapMobs.h"
#include "../MapleMap/MapReactors.h"
#include "../Physics/Physics.h"

#include "../../Character/Player.h"
#include "../../Template/TimedQueue.h"

#include <deque>

namespace ms
{
	class Combat
	{
	public:
		Combat(Player& player, MapChars& chars, MapMobs& mobs, MapReactors& reactors, const Physics& physics);

		// Draw bullets, damage numbers etc.
		void draw(double viewx, double viewy, float alpha) const;
		// Poll attacks, damage effects, etc.
		void update();

		// Make the player use a special move
		void use_move(int32_t move_id);
		// use_move for a held key: attack skills only, and quietly - no
		// "not enough MP" message every frame.
		void repeat_move(int32_t move_id);

		// Add an attack to the attack queue
		void push_attack(const AttackResult& attack);
		// Show a standalone damage number over a mob (server DAMAGE_MONSTER
		// packet: DoT, mist, Body Pressure and other attacker-less damage).
		void show_mob_damage(int32_t oid, int64_t damage);
		// Show a buff effect. `speed` and `direction` come from the packet: without
		// them the cast plays at default speed and default facing, so a skill cast
		// facing left renders facing right. Negative direction means "unspecified"
		// and leaves the character's current facing alone.
		void show_buff(int32_t cid, int32_t skillid, int8_t level,
			uint8_t speed = 0, int8_t direction = -1);
		// Teleport variants across the mage jobs and GM. These carry no authored
		// use-effect, so anything rendering them has to substitute the standard
		// BasicEff puff explicitly.
		static bool is_teleport_skill(int32_t skillid);
		// Show a buff effect
		void show_player_buff(int32_t skillid);

		// An arrow key went down: Aran's commands and Combat Step read these.
		void send_direction(KeyAction::Id dir);
		// The attack key went down (held attacks repeat from Stage::update).
		void attack_pressed();

	private:
		struct DamageEffect
		{
			AttackUser user;
			DamageNumber number;
			int64_t damage;
			bool toleft;
			int32_t target_oid;
			int32_t move_id;
		};

		struct BulletEffect
		{
			DamageEffect damageeffect;
			Bullet bullet;
			Point<int16_t> target;
		};

		void apply_attack(const AttackResult& attack);
		void apply_move(const SpecialMove& move);
		std::vector<int32_t> find_closest(MapObjects* objs, Rectangle<int16_t> range, Point<int16_t> origin, uint8_t objcount, bool use_mobs) const;
		void apply_use_movement(const SpecialMove& move);
		void apply_result_movement(const SpecialMove& move, const AttackResult& result);
		void apply_rush(const AttackResult& result);
		// Blink the player a fixed distance in the held-direction (Magician
		// Teleport / Flash Jump), snapping onto the foothold at the destination.
		void apply_teleport(const SpecialMove& move);
		// Mid-air dash: horizontal impulse in the moving/facing direction with a
		// small upward pop, only usable while airborne.
		void apply_flash_jump();
		// Aran: a short dash in the facing direction.
		void apply_combat_step();

		// Aran (or anyone with a polearm and Aran's skills): the attack key runs
		// the swing chain, Triple Swing's finishers and the arrow commands.
		bool aran_inputs() const;
		// What the attack key does now; 0 keeps the regular attack.
		// now: when the attack was pressed (earlier than this for a queued one).
		int32_t aran_attack(uint32_t now);
		// Each monster an Aran hits raises the combo (the server counts).
		void count_aran_hits(const AttackResult& result);
		// The combo a skill needs (Combo Smash 30...), 0 for none.
		static int32_t combo_needed(int32_t skillid);
		static uint16_t hit_delay(const Char& user, const AttackResult& result, size_t i);
		void apply_bullet_effect(const BulletEffect& effect);
		void apply_damage_effect(const DamageEffect& effect);
		void extract_effects(const Char& user, const SpecialMove& move, const AttackResult& result);
		std::vector<DamageNumber> place_numbers(int32_t oid, const std::vector<std::pair<int64_t, bool>>& damagelines);
		const SpecialMove& get_move(int32_t move_id);

		Player& player;
		MapChars& chars;
		MapMobs& mobs;
		MapReactors& reactors;
		const Physics& physics;

		std::unordered_map<int32_t, Skill> skills;
		RegularAttack regularattack;

		TimedQueue<AttackResult> attackresults;
		TimedQueue<BulletEffect> bulleteffects;
		TimedQueue<DamageEffect> damageeffects;

		std::list<BulletEffect> bullets;
		std::list<DamageNumber> damagenumbers;

		// Client-side rate limit on Teleport (ticks). The base skill has no data
		// cooldown, so without this the player could blink every frame.
		int16_t teleport_cooldown = 0;

		// Skill we just cast (use effect already played locally) — the server
		// echoes the cast back as SHOW_ITEM_GAIN_INCHAT/1 and show_player_buff
		// must not replay it, or every sound plays twice
		int32_t local_cast_id = 0;
		int16_t local_cast_timeout = 0;

		struct Arrow
		{
			KeyAction::Id dir;
			uint32_t tick;
		};

		// Recent arrow presses, for Aran's commands and Combat Step.
		std::deque<Arrow> arrows;
		uint32_t ticks = 0;
		// Ticks since the player last stopped attacking.
		uint32_t idle_ticks = 0;
		// The last swing of Aran's chain: 1 regular, 2 Double, 3 Triple; 0 none.
		uint8_t aran_chain = 0;
		// When that swing started.
		uint32_t chain_tick = 0;
		// Ticks since an Aran hit raised the combo; it lapses after 3 s.
		uint32_t combo_ticks = 0;
		// Combat Step: ticks of dash left, and the wait before the next.
		// An attack pressed during a swing (Aran), and when.
		bool queued_attack = false;
		uint32_t queued_tick = 0;
		// While a queued attack goes off: when it was pressed.
		uint32_t pressed_tick = 0;
		uint16_t step_ticks = 0;
		uint16_t step_cooldown = 0;
		bool step_right = true;
	};
}