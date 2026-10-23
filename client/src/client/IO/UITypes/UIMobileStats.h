//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../MobileUI.h"

#include "../../Character/CharStats.h"

namespace ms
{
	// The iPhone's character sheet: HP/MP/EXP, ability points with large
	// "+" buttons (tap for one, hold to keep adding), auto-assign, the free
	// reset, and the combat numbers. Points go to the server as they are
	// placed; a reset gives them all back.
	class UIMobileStats : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILESTATS;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileStats(const CharStats& stats);

		void draw(float alpha) const override;
		void update() override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		bool repeats(int32_t target) const override;

	private:
		enum Target : int32_t
		{
			AUTO = 1,
			RESET,
			ADD_STR,
			ADD_DEX,
			ADD_INT,
			ADD_LUK,
			ADD_HP,
			ADD_MP
		};

		MobileUI::Rect auto_rect() const;
		MobileUI::Rect reset_rect() const;
		MobileUI::Rect stat_row(int index) const;
		MobileUI::Rect add_rect(int32_t target) const;

		int32_t available_ap() const;
		void spend(MapleStat::Id stat);
		void auto_assign();

		const CharStats& stats;

		int32_t last_ap = 0;
		int32_t pending = 0;
		int32_t pending_ms = 0;
	};
}
