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

#include <functional>
#include <string>

namespace ms
{
	// A yes/no question in the mobile style: a card over a dimmed screen.
	class UIMobileConfirm : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILECONFIRM;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIMobileConfirm(std::string title, std::string message, std::string confirm, bool danger,
			std::function<void()> on_confirm);

		void draw(float alpha) const override;
		void update() override;

		bool is_in_range(Point<int16_t> cursorpos) const override;
		bool captures_hold() const override { return true; }
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	private:
		enum Target { NONE, CANCEL, CONFIRM };

		MobileUI::Rect card() const;
		MobileUI::Rect cancel_rect() const;
		MobileUI::Rect confirm_rect() const;
		Target hit(Point<int16_t> pos) const;
		void finish(bool yes);

		std::string title;
		std::string message;
		std::string confirm;
		bool danger;
		std::function<void()> on_confirm;

		Target pressed = NONE;
		uint32_t press = 0;
		int16_t height = 0;
	};
}
