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

#include "../../Character/Char.h"
#include "../../Data/ItemData.h"

#include <vector>

namespace ms
{
	// Simple item entry for bot inventory display
	struct BotItem
	{
		int32_t item_id;
		int16_t count;
		int16_t slot;
	};

	struct BotInventoryData
	{
		int32_t char_id = 0;
		std::string name;
		int32_t level = 0;
		int32_t meso = 0;
		std::vector<BotItem> equip;     // inventoryType 1
		std::vector<BotItem> use;       // inventoryType 2
		std::vector<BotItem> setup;     // inventoryType 3
		std::vector<BotItem> etc;       // inventoryType 4
		std::vector<BotItem> equipped;  // inventoryType 5

		bool is_valid() const { return char_id != 0; }

		// idx: 0=Equip, 1=Use, 2=Setup, 3=Etc, 4=Equipped
		const std::vector<BotItem>& get_tab(int idx) const
		{
			switch (idx)
			{
			case 0: return equip;
			case 1: return use;
			case 2: return setup;
			case 3: return etc;
			case 4: return equipped;
			default: return equip;
			}
		}

		// invType for packets: 1=EQUIP, 2=USE, 3=SETUP, 4=ETC, 5=EQUIPPED
		int8_t get_inv_type(int idx) const
		{
			return static_cast<int8_t>(idx < 4 ? idx + 1 : 5);
		}
	};

	// v83 UIWindow.img/UserInfo "CHARACTER INFO" window. backgrnd (275x199)
	// is the plain window, backgrnd2 (275x399) the same with the pet section
	// below it, backgrnd5 (275x362) with the taming-mob section, backgrnd3
	// (240x162) the WISH LIST side panel. The bot inventory view reuses the
	// v83 UIWindow.img/Item window.
	class UICharInfo : public UIDragElement<PosCHARINFO>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CHARINFO;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UICharInfo(int32_t cid);

		void draw(float inter) const override;
		void update() override;

		Button::State button_pressed(uint16_t buttonid) override;

		bool is_in_range(Point<int16_t> cursorpos) const override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

		void update_stats(int32_t character_id, int16_t job_id, int16_t level, int16_t fame, std::string guild, std::string alliance);

		// Live-refresh just the fame number (e.g. after a fame/defame is
		// acknowledged by the server) without reopening the window.
		void update_fame(int16_t fame);

		void set_bot_inventory(BotInventoryData data);
		int32_t get_char_id() const;

		struct CharPetInfo
		{
			int32_t itemid;
			std::string name;
			int8_t level;
			int16_t closeness;
			int8_t fullness;
		};

		void set_extra_info(std::vector<CharPetInfo> pets, bool mount,
			int32_t mount_level, int32_t mount_exp, int32_t mount_tired,
			std::vector<int32_t> wish);

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		void send_scroll(double yoffset) override;
		bool send_icon(const Icon& icon, Point<int16_t> cursorpos) override;

	private:
		enum PanelMode : int8_t
		{
			PANEL_NONE,
			PANEL_PET,
			PANEL_TAMING
		};

		void set_panel(PanelMode mode);
		void set_bot_view(bool enable);
		void draw_main(float inter) const;
		void draw_pet_panel() const;
		void draw_taming_panel() const;
		void draw_wish_panel() const;
		void draw_bot_inventory(float inter) const;
		void draw_bot_item_grid(const std::vector<BotItem>& items) const;
		void load_bot_icons();
		int16_t bot_slot_by_position(Point<int16_t> cursor_relative) const;
		Point<int16_t> get_bot_slotpos(int16_t slot_index) const;
		Icon* get_bot_icon(int16_t slot_index);

		class BotItemIcon : public Icon::Type
		{
		public:
			BotItemIcon(int32_t bot_id, int8_t inv_type, int16_t slot, int32_t item_id, int16_t count, int8_t sub_tab);

			void drop_on_stage() const override;
			void drop_on_equips(EquipSlot::Id eqslot) const override;
			bool drop_on_items(InventoryType::Id tab, EquipSlot::Id eqslot, int16_t slot, bool equip) const override;
			void drop_on_bindings(Point<int16_t> cursorposition, bool remove) const override;
			void set_count(int16_t count) override;
			Icon::IconType get_type() override;
			int16_t get_source_slot() const override { return slot; }
			InventoryType::Id get_source_tab() const override { return InventoryType::Id::NONE; }

		private:
			int32_t bot_id;
			int8_t inv_type;
			int16_t slot;
			int32_t item_id;
			int16_t count;
			int8_t sub_tab;
		};

		enum Buttons : uint16_t
		{
			BtClose,
			BtParty,
			BtTrade,
			BtFamily,
			BtPopUp,
			BtPopDown,
			BtItem,
			BtWish,
			BtPetShow,
			BtPetHide,
			BtTamingShow,
			BtTamingHide,
			BtPet1,
			BtPet2,
			BtPet3,
			BtBotEquip,
			BtBotUse,
			BtBotSetup,
			BtBotEtc,
			BtBotEquipped
		};

		/// Main window (UserInfo/backgrnd 275x199)
		static constexpr int16_t WIN_W = 275;
		static constexpr int16_t WIN_H = 199;
		static constexpr int16_t PET_WIN_H = 399;        // UserInfo/backgrnd2
		static constexpr int16_t TAMING_WIN_H = 362;     // UserInfo/backgrnd5
		static constexpr int16_t DRAG_H = 20;
		static constexpr Point<int16_t> CLOSE_POS = Point<int16_t>(254, 5);          // Basic.img/BtClose 12x12 in the title bar
		static constexpr Point<int16_t> PORTRAIT_FEET = Point<int16_t>(54, 130);     // shadow ellipse in the portrait box (x 8..100, y 25..160)
		static constexpr Point<int16_t> NAME_PLATE = Point<int16_t>(53, 146);        // blue plate x 10..96, y 145..160
		static constexpr int16_t STAT_X = 160;           // value fields x 157..265 right of the LEVEL..ALLIANCE labels
		static constexpr int16_t STAT_Y = 78;            // LEVEL row; rows are 18px apart
		static constexpr int16_t STAT_PITCH = 18;
		static constexpr Point<int16_t> FAME_UP_POS = Point<int16_t>(238, 115);      // Basic.img/BtUP 12x12 at the FAME field's right end
		static constexpr Point<int16_t> FAME_DOWN_POS = Point<int16_t>(252, 115);    // Basic.img/BtDown 12x12
		// Blue action area (x 105..265, y 25..60): REQ.PARTY / REQ.TRADE (59x17)
		// stacked on the left, FAMILY (40x37) on the right of the divider
		static constexpr Point<int16_t> PARTY_POS = Point<int16_t>(108, 25);
		static constexpr Point<int16_t> TRADE_POS = Point<int16_t>(108, 43);
		static constexpr Point<int16_t> FAMILY_POS = Point<int16_t>(216, 24);
		// Bottom strip buttons, 24px above whichever background's bottom edge:
		// ITEM (60x18), WISHLIST (60x18), PET INFO (48x17), TAMING MOB (85x17)
		static constexpr int16_t STRIP_BTN_FROM_BOTTOM = 24;
		static constexpr int16_t ITEM_X = 6;
		static constexpr int16_t WISH_X = 70;
		static constexpr int16_t PET_X = 134;
		static constexpr int16_t TAMING_X = 186;

		/// Pet section of backgrnd2 (y 165..365)
		static constexpr Point<int16_t> PET_FEET = Point<int16_t>(54, 288);          // portrait box x 8..100, y 170..320
		static constexpr Point<int16_t> PET_NAME_PLATE = Point<int16_t>(53, 301);    // blue plate y 300..318
		static constexpr int16_t PET_SLOT_X = 185;       // BtPet1..3 (30x27, origin 15,13) in the 3 boxes at x 170/203/236, y 178
		static constexpr int16_t PET_SLOT_Y = 191;
		static constexpr int16_t PET_SLOT_PITCH = 33;
		static constexpr int16_t PET_ROW_X = 112;        // per-pet icon + name rows in the blue list (x 105..265, y 210..320)
		static constexpr int16_t PET_ROW_NAME_X = 146;
		static constexpr int16_t PET_ROW_Y = 214;
		static constexpr int16_t PET_ROW_PITCH = 36;
		static constexpr Point<int16_t> PET_TYPE_POS = Point<int16_t>(60, 331);      // value fields next to TYPE / LEVEL / CLOSENESS / FULLNESS
		static constexpr Point<int16_t> PET_LEVEL_POS = Point<int16_t>(60, 349);
		static constexpr Point<int16_t> PET_CLOSE_POS = Point<int16_t>(194, 331);
		static constexpr Point<int16_t> PET_FULL_POS = Point<int16_t>(194, 349);

		/// Taming-mob section of backgrnd5 (y 165..330)
		static constexpr Point<int16_t> MOUNT_NAME_PLATE = Point<int16_t>(53, 271);  // blue plate y 270..283
		static constexpr Point<int16_t> MOUNT_TYPE_POS = Point<int16_t>(60, 297);    // value fields next to TYPE / LEVEL / EXP / TIREDNESS
		static constexpr Point<int16_t> MOUNT_LEVEL_POS = Point<int16_t>(60, 314);
		static constexpr Point<int16_t> MOUNT_EXP_POS = Point<int16_t>(194, 297);
		static constexpr Point<int16_t> MOUNT_TIRED_POS = Point<int16_t>(194, 314);

		/// Wish list side panel (backgrnd3 240x162) drawn right of the window
		static constexpr int16_t WISH_PANEL_W = 240;
		static constexpr int16_t WISH_PANEL_H = 162;
		static constexpr int16_t WISH_PANEL_GAP = 2;
		static constexpr int16_t WISH_ROW_Y = 30;        // three 36px rows, icon box x 15..50, text area x 57..212
		static constexpr int16_t WISH_ROW_PITCH = 38;
		static constexpr int16_t WISH_ICON_X = 17;
		static constexpr int16_t WISH_TEXT_X = 57;
		static constexpr int16_t WISH_MAX_ROWS = 3;

		/// Bot inventory view (UIWindow.img/Item backgrnd 175x289)
		static constexpr int16_t BOT_W = 175;
		static constexpr int16_t BOT_H = 289;
		static constexpr Point<int16_t> BOT_CLOSE_POS = Point<int16_t>(156, 5);
		static constexpr int16_t BOT_TAB_X = 5;          // five 33px tab cells in the strip at y 24..42
		static constexpr int16_t BOT_TAB_Y = 24;
		static constexpr int16_t BOT_TAB_PITCH = 33;
		static constexpr int16_t BOT_TAB_H = 18;
		static constexpr int16_t BOT_GRID_X = 9;         // 4x6 grid of 32px slots
		static constexpr int16_t BOT_GRID_Y = 51;
		static constexpr int16_t BOT_ICON_W = 36;        // slot pitch
		static constexpr int16_t BOT_ICON_H = 34;
		static constexpr int16_t BOT_COLS = 4;
		static constexpr int16_t BOT_ROWS = 6;
		static constexpr int16_t BOT_MAX_SLOTS = BOT_COLS * BOT_ROWS;
		static constexpr Point<int16_t> BOT_NAME_POS = Point<int16_t>(150, 5);       // right-aligned in the title bar's free half
		static constexpr Point<int16_t> BOT_LEVEL_POS = Point<int16_t>(30, 268);     // white meso field x 28..137, y 267..281
		static constexpr Point<int16_t> BOT_MESO_POS = Point<int16_t>(135, 268);     // right-aligned in the same field

		/// Expander panels
		std::vector<CharPetInfo> pets_info;
		bool has_mount = false;
		int32_t mount_level = 0;
		int32_t mount_exp = 0;
		int32_t mount_tired = 0;
		std::vector<int32_t> wishlist;
		PanelMode panel_mode = PANEL_NONE;
		int8_t selected_pet = 0;     // which of BtPet1..3 the TYPE/LEVEL/CLOSENESS/FULLNESS fields describe
		bool wish_open = false;
		Texture main_bg;      // UserInfo/backgrnd
		Texture pet_bg;       // UserInfo/backgrnd2
		Texture taming_bg;    // UserInfo/backgrnd5
		Texture wish_bg;      // UserInfo/backgrnd3
		mutable Text panel_label;

		/// Main Window
		Text name;
		Text job;
		Text level;
		Text fame;
		Text guild;
		Text alliance;

		// Resolved from the stage every time it is needed, never cached. The Char
		// lives in the stage's character list and is destroyed when that player
		// despawns or the map changes; a stored pointer outlives it and this
		// window stays open across both, so caching one dangles and crashes on
		// the next draw.
		Char* target() const;
		int32_t target_char_id;

		// Snapshot of the target's look, refreshed while they are on screen.
		// When they walk out of view or the map changes their Char is destroyed,
		// and the window stays open -- this keeps the portrait showing the last
		// pose instead of going blank. Safe to hold: CharLook's pointers all
		// index static caches that are only inserted into, never erased.
		CharLook preview_look;
		bool has_preview = false;

		/// Bot Inventory
		BotInventoryData bot_inventory;
		bool bot_tab_active;
		int8_t bot_sub_tab;          // 0=Equip, 1=Use, 2=Setup, 3=Etc, 4=Equipped
		int16_t bot_scroll_row;      // first visible grid row when a tab holds more than 24 items

		Text bot_name_label;
		Text bot_level_label;
		Text bot_meso_label;

		Texture bot_backgrnd;        // Item/backgrnd
		Texture bot_slot_disabled;   // Item/disabled 32x32 cross

		std::map<int16_t, std::unique_ptr<Icon>> bot_icons;
	};
}
