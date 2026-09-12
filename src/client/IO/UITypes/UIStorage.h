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
#include "../Components/Icon.h"
#include "../Components/Slider.h"

#include "../../Character/Inventory/Inventory.h"
#include "../../Character/Inventory/InventoryType.h"
#include "../../Graphics/Animation.h"

#include <set>
#include <vector>

namespace ms
{
	// v83 storage (UIWindow.img/Trunk, 463x318). Two list panels:
	//   left  (x 3..229)  - storage keeper portrait, 5 rows of stored items
	//                       (current tab), storage meso box
	//   right (x 233..459) - player portrait, inventory tabs, 4 rows of the
	//                       player's items, wallet meso box
	// Items are shown as icon + name rows (not a cube grid); each panel
	// scrolls with the Basic.img VScr slider in its white track.
	class UIStorage : public UIDragElement<PosSTORAGE>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::STORAGE;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = true;

		struct ItemEntry
		{
			int32_t itemid = 0;
			int16_t count = 0;
		};

		UIStorage(const Inventory& inventory);

		void draw(float alpha) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> position) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		void send_scroll(double yoffset) override;
		void rightclick(Point<int16_t> cursorpos) override;
		void doubleclick(Point<int16_t> cursorpos) override;
		bool send_icon(const Icon& icon, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		// Called by handler
		void open(int32_t npcid, uint8_t slots, int32_t meso);
		void close();
		void set_items_for_all(InventoryType::Id types[], size_t ntypes,
			std::vector<ItemEntry> items_by_type[]);
		void set_items_for_tab(InventoryType::Id type, const std::vector<ItemEntry>& items);
		void set_slots(uint8_t value);
		void set_meso(int32_t meso);
		void modify(InventoryType::Id type);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		/// v83 layout constants (pixel-measured on UIWindow.img/Trunk/backgrnd)
		// Row art: 34 px icon box (x 7..40) + 162 px name strip (x 43..204),
		// 35 px tall, repeated every 40 px. The left panel has no tab band
		// so its rows start higher and it fits one more row.
		static constexpr int16_t ROW_PITCH = 40;
		static constexpr int16_t ROW_ART_H = 35;
		static constexpr int16_t ROW_W = 198;
		static constexpr int16_t NAME_STRIP_DX = 36;
		static constexpr int16_t STORAGE_X = 7;
		static constexpr int16_t STORAGE_ROW_Y = 88;
		static constexpr int16_t STORAGE_ROWS = 5;
		static constexpr int16_t INVENTORY_X = 237;
		static constexpr int16_t INVENTORY_ROW_Y = 127;
		static constexpr int16_t INVENTORY_ROWS = 4;
		// Panel split (the 2 px frame between the panels)
		static constexpr int16_t PANEL_DIVIDER_X = 231;
		// Scrollbar columns (12 px LINE_CYAN VScr in the white tracks)
		static constexpr int16_t STORAGE_SLIDER_X = 212;
		static constexpr int16_t INVENTORY_SLIDER_X = 442;
		static constexpr int16_t SLIDER_BOTTOM = 270;
		// Tab band (y 91..115) on the right panel; text bitmaps 9-12 px tall
		static constexpr int16_t TAB_Y = 97;
		static constexpr int16_t TAB_X = 245;
		static constexpr int16_t TAB_GAP = 12;
		// Portrait feet (shadow blobs in the header boxes)
		static constexpr Point<int16_t> NPC_POS = Point<int16_t>(53, 78);
		static constexpr Point<int16_t> CHAR_POS = Point<int16_t>(283, 78);
		// Meso boxes (y 296..309): x 43..194 (storage) and 274..424 (wallet),
		// each followed by a baked-in "mesos" label; text is right-aligned
		static constexpr Point<int16_t> STORAGE_MESO_POS = Point<int16_t>(192, 296);
		static constexpr Point<int16_t> PLAYER_MESO_POS = Point<int16_t>(422, 296);

		// Icon type for storage items (drag from storage = take out)
		class StorageItemIcon : public Icon::Type
		{
		public:
			StorageItemIcon(int16_t index, InventoryType::Id tab);

			void drop_on_stage() const override {}
			void drop_on_equips(EquipSlot::Id) const override {}
			bool drop_on_items(InventoryType::Id tab, EquipSlot::Id eqslot, int16_t slot, bool equip) const override;
			void drop_on_bindings(Point<int16_t>, bool) const override {}
			void set_count(int16_t) override {}
			Icon::IconType get_type() override { return Icon::IconType::ITEM; }

		private:
			int16_t index;
			InventoryType::Id tab;
		};

		// Icon type for player inventory items (drag from inventory = store)
		class PlayerItemIcon : public Icon::Type
		{
		public:
			PlayerItemIcon(int16_t slot, int32_t itemid, int16_t count);

			void drop_on_stage() const override {}
			void drop_on_equips(EquipSlot::Id) const override {}
			bool drop_on_items(InventoryType::Id tab, EquipSlot::Id eqslot, int16_t slot, bool equip) const override;
			void drop_on_bindings(Point<int16_t>, bool) const override {}
			void set_count(int16_t) override {}
			Icon::IconType get_type() override { return Icon::IconType::ITEM; }

		private:
			int16_t inv_slot;
			int32_t itemid;
			int16_t count;
		};

		struct InventoryEntry
		{
			int32_t itemid = 0;
			int16_t count = 0;
			int16_t slot = 0;
		};

		void clear_tooltip();
		void refresh_storage_icons();
		void refresh_inventory_icons();
		void refresh_inventory_tab();
		void change_tab(InventoryType::Id type);

		void request_take_out(int16_t index);
		// Partial take-out: take the whole stack, then immediately
		// store `full_count - keep` back. The v83 protocol has no
		// quantity field on take-out so this is the only way to let
		// the player keep just some of a stack.
		void request_take_out_partial(int16_t index, int16_t keep);
		void request_store(int16_t inv_slot, int32_t itemid, int16_t count);
		void request_sort();
		void request_meso_change(bool store_to_storage);
		void close_with_packet();

		uint16_t button_by_tab(InventoryType::Id type) const;
		// Index of the Trunk/Tab bitmap ("0" Equip, "1" Use, "2" Set-up, "3" Etc, "4" Cash)
		static int16_t tab_art_index(InventoryType::Id type);

		// Row position helpers. "Visual slot" = index into the panel's
		// slot_map; rows scroll by the panel's offset.
		int16_t storage_slot_by_cursor(Point<int16_t> cursoroffset) const;
		int16_t inventory_slot_by_cursor(Point<int16_t> cursoroffset) const;
		Point<int16_t> storage_icon_pos(int16_t index) const;
		Point<int16_t> inventory_icon_pos(int16_t index) const;
		bool in_left_panel(Point<int16_t> cursoroffset) const;
		bool in_right_panel(Point<int16_t> cursoroffset) const;
		// Draw one item row (icon, count, name) at the row's top-left
		void draw_row(Point<int16_t> rowpos, int32_t itemid, int16_t count, const Text& name, bool dragged, float alpha) const;

		const Inventory& inventory;

		// Storage items organized by tab
		std::vector<ItemEntry> storage_equip;
		std::vector<ItemEntry> storage_use;
		std::vector<ItemEntry> storage_setup;
		std::vector<ItemEntry> storage_etc;
		std::vector<ItemEntry> storage_cash;

		std::vector<ItemEntry>& get_storage_tab(InventoryType::Id type);
		const std::vector<ItemEntry>& get_storage_tab(InventoryType::Id type) const;

		// Player inventory items for current tab
		std::vector<InventoryEntry> inventory_items;

		// Icon objects for drag-and-drop (keyed by item index in the
		// tab's item vector, not by visual row).
		std::map<int16_t, std::unique_ptr<Icon>> storage_icons;
		std::map<int16_t, std::unique_ptr<Icon>> inv_icons;

		// Item name labels, parallel to the tab's item vectors
		std::vector<Text> storage_names;
		std::vector<Text> inventory_names;

		// Visual layout tables: slot_map[visual_row] = item index (or -1).
		// Rebuilt on refresh; mutated on same-panel drag/drop so the user
		// can reorder rows instead of being forced into the server's
		// dense 0..N-1 order.
		std::vector<int16_t> storage_slot_map;
		std::vector<int16_t> inventory_slot_map;

		// Persist per-itemid visual slot across refreshes so the player's
		// layout survives take-out/store operations that reshuffle the
		// server list. Keyed by itemid (storage) or server slot (inv).
		std::map<int32_t, int16_t> storage_preferred_slot;
		std::map<int16_t, int16_t> inventory_preferred_slot;

		int16_t drag_source_slot;

		// Row highlights: Trunk/select (162x35 name strip) for the
		// selected rows, Trunk/en (199x35 full row) for the hovered row
		Texture select_row;
		Texture hover_row;
		Animation npc_anim;
		Text storage_mesolabel;
		Text player_mesolabel;

		// List scrolling
		Slider storage_slider;
		Slider inventory_slider;
		int16_t storage_offset;
		int16_t inventory_offset;
		int16_t hover_storage_row;
		int16_t hover_inventory_row;

		InventoryType::Id current_tab;
		// Multi-select — right-click toggles each item in/out of the set.
		// Retrieve / Store buttons act on every selected item.
		std::set<int16_t> storage_selection;
		std::set<int16_t> inventory_selection;

		// Pending partial take-outs — waiting for the take-out response
		// so we can re-store the excess.
		struct PendingRestore
		{
			int32_t itemid;
			int16_t keep;
			InventoryType::Id tab;
		};
		std::vector<PendingRestore> pending_restores;
		Point<int16_t> lastcursorpos;
		enum DragSource : int8_t { DRAG_NONE, DRAG_STORAGE, DRAG_INVENTORY };
		DragSource drag_source;

		int32_t storage_meso;
		uint8_t storage_slots;
		int32_t npc_id;

		enum Buttons : int16_t
		{
			BT_GET,
			BT_PUT,
			BT_SORT,
			BT_IN_COIN,
			BT_OUT_COIN,
			BT_EXIT,
			BT_TAB_EQUIP,
			BT_TAB_USE,
			BT_TAB_ETC,
			BT_TAB_SETUP,
			BT_TAB_CASH,
			NUM_BUTTONS
		};
	};
}
