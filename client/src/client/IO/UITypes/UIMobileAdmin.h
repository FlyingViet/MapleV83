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
#include "../Components/Textfield.h"

namespace ms
{
	// Admin tools for GM characters. Rates: the world's EXP, drop and meso
	// rates (the MapleV83 server's @setrates, for everyone, kept across
	// restarts). Commands: GM commands as buttons, and any command typed in.
	class UIMobileAdmin : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEADMIN;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileAdmin();
		~UIMobileAdmin();

		void draw(float alpha) const override;
		void update() override;
		void deactivate() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		bool repeats(int32_t target) const override;

	private:
		MobileUI::Rect row_rect(int index) const;
		MobileUI::Rect minus_rect(int index) const;
		MobileUI::Rect plus_rect(int index) const;
		MobileUI::Rect preset_rect(int index) const;
		MobileUI::Rect apply_rect() const;
		MobileUI::Rect reset_rect() const;

		int server_rate(int index) const;
		bool changed() const;

		MobileUI::Rect tab_rect(int index) const;
		MobileUI::Rect quick_rect(int index) const;
		MobileUI::Rect field_rect() const;
		MobileUI::Rect value_rect(int index) const;
		void send(const std::string& command);
		void run_value(int index);
		void unfocus();
		void draw_rates() const;
		void draw_commands() const;

		bool commands = false;
		Textfield field;
		std::string last_sent;
		int32_t sent_ticks = 0;

		int pending[3] = { 1, 1, 1 };
		// The pending rates follow the server's until the admin edits them.
		bool edited = false;
		bool applying = false;
	};
}
