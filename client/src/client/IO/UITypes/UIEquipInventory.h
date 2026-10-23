//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIDragElement.h"

namespace ms
{
	// The Equip inventory (v83 UIWindow.img/Equip)
	class UIEquipInventory : public UIDragElement<PosEQINV>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::EQUIPINVENTORY;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIEquipInventory(const Inventory& inventory);

		void draw(float inter) const override;

		void toggle_active() override;
		bool send_icon(const Icon& icon, Point<int16_t> position) override;

		void doubleclick(Point<int16_t> position) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		Cursor::State send_cursor(bool pressed, Point<int16_t> position) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		void modify(int16_t pos, int8_t mode, int16_t arg);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void show_equip(EquipSlot::Id slot);
		void clear_tooltip();
		void load_icons();
		void update_slot(EquipSlot::Id slot);
		EquipSlot::Id slot_by_position(Point<int16_t> position) const;
		// Show or hide the pet equipment panel docked to the right edge
		void show_pet_panel(bool show);
		// Top-left of the pet panel relative to the window
		Point<int16_t> pet_panel_offset() const;

		class EquipIcon : public Icon::Type
		{
		public:
			EquipIcon(int16_t source);

			void drop_on_stage() const override;
			void drop_on_equips(EquipSlot::Id slot) const override;
			bool drop_on_items(InventoryType::Id tab, EquipSlot::Id eqslot, int16_t slot, bool equip) const override;
			void drop_on_bindings(Point<int16_t>, bool) const override {}
			void set_count(int16_t) override {}
			Icon::IconType get_type() override;

		private:
			int16_t source;
		};

		// v83 UIWindow.img/Equip/backgrnd (175x304) is a 5 column x 7 row
		// board of 31px cells on a 33px pitch, first cell at (6,36); the
		// labelled cells were read off the bitmap. The bottom bar
		// (y=277..300) holds SLOT+ and PET EQUIP.
		static constexpr int16_t GRID_X = 6;
		static constexpr int16_t GRID_Y = 36;
		static constexpr int16_t GRID_PITCH = 33;
		static constexpr int16_t CELL_SIZE = 32;

		enum Buttons : uint16_t
		{
			BT_CLOSE,
			BT_SLOTPLUS,
			BT_PETSHOW,
			BT_PETHIDE
		};

		const Inventory& inventory;

		EnumMap<EquipSlot::Id, Point<int16_t>> iconpositions;
		EnumMap<EquipSlot::Id, std::unique_ptr<Icon>> icons;

		// UIWindow.img/Equip/pet (177x181): pet equipment board shown to the
		// right of the window. v83 EquipSlot has no pet slot ids, so the
		// panel is display-only.
		Texture pet_panel;
		bool pet_shown;
	};
}
