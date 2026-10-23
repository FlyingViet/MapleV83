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
	// The iPhone's channel switcher: every channel of this world with how
	// busy it is; tap one to move there.
	class UIMobileChannel : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILECHANNEL;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileChannel();

		void draw(float alpha) const override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		MobileUI::Rect channel_rect(uint8_t channel) const;
		uint8_t count() const;
	};
}
