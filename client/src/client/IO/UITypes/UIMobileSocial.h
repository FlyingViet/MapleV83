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
	// The iPhone's friends and party screen: buddies with who is online and
	// where, and the party with each member's level, job, map and HP.
	class UIMobileSocial : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILESOCIAL;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		enum class Tab { BUDDIES, PARTY };

		UIMobileSocial(Tab tab);
		~UIMobileSocial();

		void draw(float alpha) const override;
		void update() override;
		void deactivate() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		void show(Tab tab);
		Tab get_tab() const { return tab; }

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		MobileUI::Rect tab_rect(int index) const;
		MobileUI::Rect field_rect() const;
		MobileUI::Rect submit_rect() const;
		MobileUI::Rect create_rect() const;
		MobileUI::Rect leave_rect() const;
		MobileUI::Rect row_rect(size_t index) const;
		MobileUI::Rect row_action(size_t index, int action, int count) const;
		int16_t row_height(size_t index) const;
		size_t row_count() const;
		bool in_party() const;
		bool leading() const;
		void submit();
		void unfocus();

		Tab tab;
		Textfield field;
		// The row whose actions are showing, by character id.
		int32_t selected = 0;
	};
}
