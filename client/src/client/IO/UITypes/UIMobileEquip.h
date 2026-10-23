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

#include "../../Character/CharStats.h"
#include "../../Character/Inventory/Inventory.h"
#include "../../Character/Look/CharLook.h"

#include <array>
#include <memory>

namespace ms
{
	// The iPhone's equipment: the character in the middle, worn gear in
	// labelled slots around it. Tap a piece for its stats, Unequip or Move;
	// carry an item from the bag onto a slot to put it on (or a scroll onto
	// gear to apply it).
	class UIMobileEquip : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEEQUIP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileEquip(const Inventory& inventory, const CharStats& stats);

		void draw(float alpha) const override;
		void update() override;

		void doubleclick(Point<int16_t> cursorpos) override;
		bool send_icon(const Icon& icon, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		// Item menu "Move": attach the worn item to the next tap.
		bool pick_up(EquipSlot::Id slot);

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;

	private:
		static constexpr int32_t SLOT = 100;
		static constexpr int32_t LAYER_GEAR = 10;
		static constexpr int32_t LAYER_COSMETIC = 11;
		static constexpr int32_t LAYER_PET = 12;
		static constexpr size_t PET_COUNT = 11;
		static constexpr size_t SLOT_COUNT = 18;

		struct SlotInfo
		{
			EquipSlot::Id id;
			const char* label;
		};

		static const std::array<SlotInfo, SLOT_COUNT>& slots();

		struct PetSlotInfo
		{
			int16_t slot;
			const char* label;
		};

		static const std::array<PetSlotInfo, PET_COUNT>& pet_slots();

		MobileUI::Rect slot_rect(size_t index) const;
		// 0 gear, 1 cosmetic, 2 pet.
		MobileUI::Rect layer_rect(int layer) const;
		void draw_pet_stage(const MobileUI::Rect& stage) const;
		// The worn slot a tile stands for in the layer being shown.
		int16_t shown_slot(size_t index) const;
		int32_t shown_item(size_t index) const;
		MobileUI::Rect stage_rect() const;
		size_t slot_at(Point<int16_t> pos) const;
		void open_menu(size_t index);
		void refresh_look();

		const Inventory& inventory;
		const CharStats& stats;

		CharLook look;
		std::array<int32_t, SLOT_COUNT> worn = {};
		std::array<int32_t, SLOT_COUNT> worn_cash = {};
		std::array<int32_t, SLOT_COUNT> worn_pet = {};
		// Showing the cash (cosmetic) items worn over the gear
		bool cosmetic = false;
		// Showing the pet's gear
		bool pets = false;

		std::unique_ptr<Icon> carry;
		EquipSlot::Id carried = EquipSlot::Id::NONE;
		uint32_t double_press = 0;
	};
}
