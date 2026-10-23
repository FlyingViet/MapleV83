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

#include "../ItemDetails.h"

#include "../../Character/Inventory/InventoryType.h"

#include <optional>
#include <string>
#include <vector>

namespace ms
{
	// What a tap on an item shows on the iPhone: the item with its
	// requirements, stats and description, then its main action (Equip / Use /
	// Unequip ...) and Move. Sits beside the tapped slot and closes on any tap
	// outside it.
	class UIMobileItemMenu : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEITEMMENU;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		enum class Source { INVENTORY, EQUIPPED };

		// slot is the inventory slot, or the EquipSlot::Id for worn items;
		// anchor is the tap, in screen coordinates.
		UIMobileItemMenu(Source source, InventoryType::Id tab, int16_t slot, int32_t item_id, Point<int16_t> anchor);

		void draw(float alpha) const override;
		void update() override;

		bool is_in_range(Point<int16_t> cursorpos) const override;
		bool captures_hold() const override { return true; }
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		// The slot this menu is for, when it is for that panel and tab; 0 otherwise.
		int16_t slot_for(Source source, InventoryType::Id tab) const;

	private:
		enum Action : int8_t { NONE = -1, PRIMARY, MOVE, SELL, SELL_ALL };

		MobileUI::Rect card() const;
		MobileUI::Rect action_rect(size_t index) const;
		Action hit(Point<int16_t> pos) const;
		bool still_there() const;
		void run(Action action);
		void close();

		Source source;
		InventoryType::Id tab;
		int16_t slot;
		int32_t item_id;
		bool is_equip;
		// The main action picks the item up (scrolls go onto worn gear)
		bool primary_moves = false;

		std::string name;
		std::string detail;
		std::string primary;
		std::vector<Action> actions;
		Texture item_icon;

		bool on_left = true;
		std::optional<ItemDetails> details;
		int16_t details_h = 0;

		Action pressed = NONE;
		uint32_t press = 0;
	};
}
