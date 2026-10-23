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

#include "../Components/Slider.h"

namespace ms
{
	// The Item inventory (v83 UIWindow.img/Item)
	class UIItemInventory : public UIDragElement<PosINV>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::ITEMINVENTORY;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIItemInventory(const Inventory& inventory);

		void draw(float inter) const override;
		void update() override;

		void doubleclick(Point<int16_t> position) override;
		bool send_icon(const Icon& icon, Point<int16_t> position) override;
		void toggle_active() override;
		void remove_cursor() override;
		Cursor::State send_cursor(bool pressed, Point<int16_t> position) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		void modify(InventoryType::Id type, int16_t pos, int8_t mode, int16_t arg);
		void set_sort(bool enabled);
		void change_tab(InventoryType::Id type);
		void clear_new();

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void show_item(int16_t slot);
		void clear_tooltip();
		void load_icons();
		void update_slot(int16_t slot);
		bool is_visible(int16_t slot) const;
		bool is_not_visible(int16_t slot) const;
		int16_t slot_by_position(Point<int16_t> position) const;
		uint16_t button_by_tab(InventoryType::Id tab) const;
		// Top-left of a slot cell in the current view (regular or expanded)
		Point<int16_t> get_slotpos(int16_t slot) const;
		// Top-left of a slot cell on the expanded (FullBackgrnd) view
		Point<int16_t> get_full_slotpos(int16_t slot) const;
		// Top-left of a tab cell on the tab strip
		Point<int16_t> get_tabpos(InventoryType::Id tab) const;
		Icon* get_icon(int16_t slot);
		void set_full(bool enabled);
		// Fit the scrollbar to the rows that actually hold items: the range
		// grows with the highest occupied slot, and the bar greys out when
		// everything already fits in the visible rows
		void update_slider();
		int16_t used_rows() const;

		class ItemIcon : public Icon::Type
		{
		public:
			ItemIcon(const UIItemInventory& parent, InventoryType::Id sourcetab, EquipSlot::Id eqsource, int16_t source, int32_t item_id, int16_t count, bool untradable, bool cashitem);

			void drop_on_stage() const override;
			void drop_on_equips(EquipSlot::Id eqslot) const override;
			bool drop_on_items(InventoryType::Id tab, EquipSlot::Id eqslot, int16_t slot, bool equip) const override;
			void drop_on_bindings(Point<int16_t> cursorposition, bool remove) const override;
			void set_count(int16_t count) override;
			Icon::IconType get_type() override;
			int16_t get_source_slot() const override { return source; }
			InventoryType::Id get_source_tab() const override { return sourcetab; }
			int32_t get_action_id() const override { return item_id; }

		private:
			InventoryType::Id sourcetab;
			EquipSlot::Id eqsource;
			int16_t source;
			int32_t item_id;
			int16_t count;
			bool untradable;
			bool cashitem;
			const UIItemInventory& parent;
		};

		// v83 UIWindow.img/Item layout. All values were measured on the
		// backgrnd (175x289) and FullBackgrnd (603x289) bitmaps: the slot
		// cells are 31px squares on a 36px horizontal / 34px vertical pitch,
		// the tab strip is a 165px bar at y=24..41 split into five 33px cells
		// (New/Tab0 highlight frames are 34x18 - one cell), and the meso box
		// is the white field at x=25..137, y=267..280 on both backgrounds.
		static constexpr int16_t COLUMNS = 4;
		static constexpr int16_t ROWS = 6;
		static constexpr int16_t MAXSLOTS = ROWS * COLUMNS;
		static constexpr int16_t FULL_PAGES = 4;
		static constexpr int16_t MAXFULLSLOTS = FULL_PAGES * MAXSLOTS;
		static constexpr int16_t SLOT_X = 9;
		static constexpr int16_t SLOT_Y = 51;
		static constexpr int16_t FULL_SLOT_X = 8;
		static constexpr int16_t SLOT_PITCH_X = 36;
		static constexpr int16_t SLOT_PITCH_Y = 34;
		static constexpr int16_t SLOT_SIZE = 32;
		static constexpr int16_t FULL_PAGE_PITCH = 149;
		static constexpr int16_t NUM_TABS = 5;
		static constexpr int16_t TAB_X = 5;
		static constexpr int16_t TAB_Y = 24;
		static constexpr int16_t TAB_PITCH = 33;
		static constexpr int16_t TAB_HEIGHT = 18;
		// Small (12x12, origin 6,6) title-bar buttons are placed by centre,
		// measured back from the window's right edge next to BtClose
		static constexpr int16_t TITLE_BUTTON_Y = 12;
		static constexpr int16_t CLOSE_RIGHT_INSET = 18;
		static constexpr int16_t FULL_RIGHT_INSET = 26;
		static constexpr int16_t SORT_RIGHT_INSET = 40;
		// Scrollbar runs down the light strip right of the slot block
		static constexpr int16_t SLIDER_X = 152;
		static constexpr int16_t SLIDER_TOP = 51;
		static constexpr int16_t SLIDER_BOTTOM = 240;

		enum Buttons
		{
			BT_CLOSE,
			BT_TAB_EQUIP,
			BT_TAB_USE,
			BT_TAB_SETUP,
			BT_TAB_ETC,
			BT_TAB_CASH,
			BT_COIN,
			BT_GATHER,
			BT_SORT,
			BT_FULL,
			BT_SMALL,
			BT_CASHSHOP
		};

		const Inventory& inventory;

		Animation newitemslot;
		Animation newitemtab;
		Animation newitemtab_active;
		Texture projectile;
		Texture disabled;
		Text mesolabel;
		Slider slider;

		std::map<int16_t, std::unique_ptr<Icon>> icons;
		std::map<InventoryType::Id, std::pair<int16_t, int16_t>> slotrange;

		InventoryType::Id tab;
		InventoryType::Id newtab;
		int16_t scroll_rows;
		int16_t newslot;
		bool ignore_tooltip;

		bool sort_enabled;
		bool full_enabled;
		Texture backgrnd;
		Texture full_backgrnd;
		Point<int16_t> bg_dimensions;
		Point<int16_t> bg_full_dimensions;
	};
}
