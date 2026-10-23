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
	// The phone's notification inbox: every invite and alert still waiting,
	// newest last, each with its own Accept / Decline (or View / Dismiss).
	// Banners that time out land here; the touch controls' Inbox chip opens it.
	class UIMobileInbox : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEINBOX;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileInbox();

		void draw(float alpha) const override;
		void update() override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		int16_t row_height(size_t index) const;
		MobileUI::Rect row_rect(size_t index) const;
		MobileUI::Rect action_rect(size_t index, bool accept) const;
		MobileUI::Rect clear_rect() const;
		// Whether entry index has a second (Accept / View) button.
		bool two_actions(size_t index) const;
		int16_t column_width() const;
	};
}
