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

#include "../Components/Icon.h"

#include "../../Character/Inventory/Inventory.h"

#include <chrono>
#include <memory>

namespace ms
{
	// The iPhone's bag: one tab per inventory, a grid of finger-sized slots,
	// mesos and a one-tap sort. Tapping an item opens its menu; Move carries
	// it to the next tap (another slot, a worn item, a trade, the ground).
	class UIMobileInventory : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEINVENTORY;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileInventory(const Inventory& inventory);

		void draw(float alpha) const override;
		void update() override;

		void doubleclick(Point<int16_t> cursorpos) override;
		bool send_icon(const Icon& icon, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		InventoryType::Id get_tab() const { return tab; }
		void change_tab(InventoryType::Id tab);
		// Item menu "Move": attach the item to the next tap.
		bool pick_up(int16_t slot);

		// The server merged stacks / finished sorting.
		void on_gathered();
		void on_sorted();

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		static constexpr int32_t TAB = 10;
		static constexpr int32_t SORT = 20;
		static constexpr int32_t SLOT_TARGET = 1000;

		MobileUI::Rect tab_rect(int index) const;
		MobileUI::Rect cell_rect(int16_t slot) const;
		MobileUI::Rect footer_rect() const;
		MobileUI::Rect sort_rect() const;
		int16_t slot_at(Point<int16_t> pos) const;
		int16_t used_slots() const;
		void open_menu(int16_t slot);

		const Inventory& inventory;
		InventoryType::Id tab = InventoryType::Id::EQUIP;

		std::unique_ptr<Icon> carry;
		uint32_t double_press = 0;
		bool sorting = false;
		std::chrono::steady_clock::time_point sort_at;
	};
}
