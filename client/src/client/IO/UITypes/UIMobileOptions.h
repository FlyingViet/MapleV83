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

#include <string>
#include <vector>

namespace ms
{
	// The iPhone's settings: sound, display, warnings and who may reach you.
	// Every change applies and saves at once.
	class UIMobileOptions : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEOPTIONS;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileOptions();

		void draw(float alpha) const override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		bool repeats(int32_t target) const override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		enum class Kind { HEADER, VOLUME, TOGGLE, CHOICE, ACTION };

		struct Row
		{
			Kind kind;
			int id;
			std::string label;
			std::string detail;
			int16_t y;
			int16_t h;
		};

		std::vector<Row> rows() const;
		MobileUI::Rect row_rect(const Row& row) const;
		MobileUI::Rect minus_rect(const Row& row) const;
		MobileUI::Rect plus_rect(const Row& row) const;
		MobileUI::Rect choice_rect(const Row& row, int index, int count) const;
		MobileUI::Rect switch_rect(const Row& row) const;

		bool toggle_value(int id) const;
		void set_toggle(int id, bool value);
		int choice_value(int id) const;
		void set_choice(int id, int index);
		int volume(int id) const;
		void set_volume(int id, int value);
	};
}
