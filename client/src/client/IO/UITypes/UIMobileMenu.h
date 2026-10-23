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

namespace ms
{
	// The iPhone's menu: large tiles for every window, in place of v83's
	// list of 16-pixel text rows above the status bar's Menu button.
	class UIMobileMenu : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEMENU;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileMenu();

		void draw(float alpha) const override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;

	private:
		MobileUI::Rect tile_rect(size_t index) const;
		MobileUI::Rect wallet_rect() const;
		MobileUI::Rect cash_shop_rect() const;
	};
}
