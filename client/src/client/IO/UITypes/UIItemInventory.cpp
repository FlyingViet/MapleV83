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
#include "UIItemInventory.h"

#include "UINotice.h"
#include "UIAvatarMegaphone.h"
#include "UIItemMegaphone.h"
#include "UIMapleTV.h"
#include "UIMegaphone.h"

#include "../ItemActions.h"
#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "../UITypes/UIKeyConfig.h"

#include "../../Data/EquipData.h"
#include "../../Gameplay/Stage.h"

#include "../../Net/Packets/InventoryPackets.h"
#include "../../Net/Packets/TradePackets.h"
#include "../../Gameplay/MiniRooms.h"
#include "UIOwl.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIItemInventory::UIItemInventory(const Inventory& invent) : UIDragElement<PosINV>(Point<int16_t>(175, 20)), inventory(invent), ignore_tooltip(false), tab(InventoryType::Id::EQUIP), sort_enabled(false), full_enabled(false)
	{
		// v83 ships the item window in UIWindow.img/Item (there is no UIWindow2.img)
		nl::node Item = nl::nx::ui["UIWindow.img"]["Item"];

		// 175x289 regular window / 603x289 expanded window (4 slot blocks side by side)
		backgrnd = Item["backgrnd"];
		full_backgrnd = Item["FullBackgrnd"];

		bg_dimensions = backgrnd.get_dimensions();
		bg_full_dimensions = full_backgrnd.get_dimensions();

		nl::node New = Item["New"];
		// 36x36 frame (origin 2,2) pulsing on the slot that just received an item
		newitemslot = New["inventory"];
		// 34x18 grey tab cell pulsing on an unselected tab that holds a new item
		newitemtab = New["Tab0"];
		// 34x19 pink tab cell for the selected tab
		newitemtab_active = New["Tab1"];

		// 36x36 frame (origin 2,2) around the active projectile slot
		projectile = Item["activeIcon"];
		// 32x32 cross drawn over slots beyond the tab's slot limit
		disabled = Item["disabled"];

		nl::node Tab = Item["Tab"];
		nl::node taben = Tab["enabled"];
		nl::node tabdis = Tab["disabled"];

		// The tab strip itself is baked into backgrnd; Tab/enabled|disabled/0-4
		// are only the label glyphs (origin 0,0) in the v83 strip order
		// Equip, Use, Set-up, Etc, Cash. Centre each glyph in its 33px cell.
		const uint16_t tab_buttons[NUM_TABS] = {
			Buttons::BT_TAB_EQUIP,
			Buttons::BT_TAB_USE,
			Buttons::BT_TAB_SETUP,
			Buttons::BT_TAB_ETC,
			Buttons::BT_TAB_CASH
		};

		for (int16_t i = 0; i < NUM_TABS; i++)
		{
			std::string key = std::to_string(i);
			Point<int16_t> cell = Point<int16_t>(TAB_X + TAB_PITCH * i, TAB_Y);
			Point<int16_t> ndim = Texture(tabdis[key]).get_dimensions();
			Point<int16_t> sdim = Texture(taben[key]).get_dimensions();
			Point<int16_t> npos = cell + Point<int16_t>((TAB_PITCH - ndim.x()) / 2, (TAB_HEIGHT - ndim.y()) / 2);
			Point<int16_t> spos = cell + Point<int16_t>((TAB_PITCH - sdim.x()) / 2, (TAB_HEIGHT - sdim.y()) / 2);

			buttons[tab_buttons[i]] = std::make_unique<TwoSpriteButton>(tabdis[key], taben[key], npos, spos);
		}

		// v83 Basic.img only has BtClose (12x12) and BtClose2 (14x14)
		nl::node close = nl::nx::ui["Basic.img"]["BtClose"];
		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close);

		// 14x14 coin (origin 0,0) drawn over the coin baked into the meso bar
		buttons[Buttons::BT_COIN] = std::make_unique<MapleButton>(Item["BtCoin"], Point<int16_t>(8, 267));

		// 12x12 (origin 6,6) title-bar buttons; set_full() places them by
		// the current window width
		buttons[Buttons::BT_GATHER] = std::make_unique<MapleButton>(Item["BtGather"]);
		buttons[Buttons::BT_SORT] = std::make_unique<MapleButton>(Item["BtSort"]);
		buttons[Buttons::BT_FULL] = std::make_unique<MapleButton>(Item["BtFull"]);
		buttons[Buttons::BT_SMALL] = std::make_unique<MapleButton>(Item["BtSmall"]);

		// "+ INVENTORY" (92x17, origin 46,8) sits in the empty right half of
		// the expanded window's meso bar; only shown in the expanded view
		buttons[Buttons::BT_CASHSHOP] = std::make_unique<MapleButton>(Item["BtCashshop"], Point<int16_t>(bg_full_dimensions.x() - 54, 273));

		buttons[button_by_tab(tab)]->set_state(Button::State::PRESSED);

		mesolabel = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK);

		slotrange[InventoryType::Id::EQUIPPED] = { 1, MAXSLOTS };
		slotrange[InventoryType::Id::EQUIP] = { 1, MAXSLOTS };
		slotrange[InventoryType::Id::USE] = { 1, MAXSLOTS };
		slotrange[InventoryType::Id::SETUP] = { 1, MAXSLOTS };
		slotrange[InventoryType::Id::ETC] = { 1, MAXSLOTS };
		slotrange[InventoryType::Id::CASH] = { 1, MAXSLOTS };

		scroll_rows = ROWS;

		// Basic.img/VScr5 (NORMAL_ROCKBLUE): the pale blue-grey bar that
		// matches the v83 window palette. VScr9 (DEFAULT_SILVER) does not
		// exist in v83.
		slider = Slider(
			Slider::Type::NORMAL_ROCKBLUE, Range<int16_t>(SLIDER_TOP, SLIDER_BOTTOM), SLIDER_X, ROWS, ROWS,
			[&](bool upwards)
			{
				int16_t shift = upwards ? -COLUMNS : COLUMNS;
				bool above = slotrange[tab].first + shift > 0;
				bool below = slotrange[tab].second + shift <= scroll_rows * COLUMNS;

				if (above && below)
				{
					slotrange[tab].first += shift;
					slotrange[tab].second += shift;
				}
			}
		);

		set_full(false);
		clear_new();
		update_slider();
	}

	void UIItemInventory::draw(float alpha) const
	{
		UIElement::draw_sprites(alpha);

		if (full_enabled)
		{
			full_backgrnd.draw(position);
		}
		else
		{
			backgrnd.draw(position);
			slider.draw(position);
		}

		// Right-aligned inside the white meso field (x=25..137, y=267..280),
		// which sits at the same spot on both backgrounds
		mesolabel.draw(position + Point<int16_t>(134, 266));

		auto range = slotrange.at(tab);

		int16_t numslots = inventory.get_slotmax(tab);
		int16_t firstslot = full_enabled ? 1 : range.first;
		int16_t lastslot = full_enabled ? MAXFULLSLOTS : range.second;

		for (int16_t i = firstslot; i <= lastslot; i++)
		{
			Point<int16_t> slotpos = get_slotpos(i);
			auto iter = icons.find(i);

			if (iter != icons.end() && iter->second)
				iter->second->draw(position + slotpos);
			else if (i > numslots)
				disabled.draw(position + slotpos);
		}

		int16_t bulletslot = inventory.get_bulletslot();

		if (tab == InventoryType::Id::USE && is_visible(bulletslot))
			projectile.draw(position + get_slotpos(bulletslot));

		if (newtab != InventoryType::Id::NONE)
		{
			if (newtab == tab)
			{
				// The pink cell is one pixel taller than the strip cell
				newitemtab_active.draw(position + get_tabpos(newtab) - Point<int16_t>(0, 1), alpha);

				if (is_visible(newslot))
					newitemslot.draw(position + get_slotpos(newslot), alpha);
			}
			else
			{
				newitemtab.draw(position + get_tabpos(newtab), alpha);
			}
		}

		// Tab labels draw over the highlight cells
		UIElement::draw_buttons(alpha);
	}

	void UIItemInventory::update()
	{
		UIElement::update();

		newitemtab.update(6);
		newitemtab_active.update(6);
		newitemslot.update(6);

		std::string meso_str = std::to_string(inventory.get_meso());
		string_format::split_number(meso_str);

		mesolabel.change_text(meso_str);
	}

	void UIItemInventory::update_slot(int16_t slot)
	{
		// This slot's icon may currently be the UI's dragged icon (rebuild
		// arriving mid-drag) — forget that reference before destroying it
		if (auto icon_iter = icons.find(slot); icon_iter != icons.end() && icon_iter->second)
			UI::get().purge_icon(icon_iter->second.get());

		if (int32_t item_id = inventory.get_item_id(tab, slot))
		{
			int16_t count;

			if (tab == InventoryType::Id::EQUIP)
				count = -1;
			else
				count = inventory.get_item_count(tab, slot);

			const bool untradable = ItemData::get(item_id).is_untradable();
			const bool cashitem = ItemData::get(item_id).is_cashitem();
			int32_t item_prefix = item_id / 10000;
			bool is_card = (item_prefix == 238 || item_prefix == 239);
			const Texture& texture = ItemData::get(item_id).get_icon(is_card);
			EquipSlot::Id eqslot = inventory.find_equipslot(item_id);

			icons[slot] = std::make_unique<Icon>(
				std::make_unique<ItemIcon>(*this, tab, eqslot, slot, item_id, count, untradable, cashitem),
				texture, count
				);
		}
		else if (icons.count(slot))
		{
			icons.erase(slot);
		}
	}

	void UIItemInventory::load_icons()
	{
		for (auto& icon_pair : icons)
			if (icon_pair.second)
				UI::get().purge_icon(icon_pair.second.get());

		icons.clear();

		int16_t numslots = inventory.get_slotmax(tab);

		for (int16_t i = 1; i <= numslots; i++)
			update_slot(i);
	}

	int16_t UIItemInventory::used_rows() const
	{
		int16_t last = 0;

		for (int16_t i = 1; i <= inventory.get_slotmax(tab); i++)
			if (inventory.get_item_id(tab, i))
				last = i;

		return (last + COLUMNS - 1) / COLUMNS;
	}

	void UIItemInventory::update_slider()
	{
		int16_t rows = used_rows();
		scroll_rows = rows > ROWS ? rows : ROWS;

		// The current scroll offset may sit past the shrunken range
		int16_t maxfirst = 1 + (scroll_rows - ROWS) * COLUMNS;

		while (slotrange[tab].first > maxfirst)
		{
			slotrange[tab].first -= COLUMNS;
			slotrange[tab].second -= COLUMNS;
		}

		slider.setrows(slotrange[tab].first / COLUMNS, ROWS, scroll_rows);
		slider.setenabled(scroll_rows > ROWS);
	}

	Button::State UIItemInventory::button_pressed(uint16_t buttonid)
	{
		InventoryType::Id oldtab = tab;

		switch (buttonid)
		{
			case Buttons::BT_CLOSE:
			{
				toggle_active();

				return Button::State::NORMAL;
			}
			case Buttons::BT_TAB_EQUIP:
			{
				tab = InventoryType::Id::EQUIP;
				break;
			}
			case Buttons::BT_TAB_USE:
			{
				tab = InventoryType::Id::USE;
				break;
			}
			case Buttons::BT_TAB_SETUP:
			{
				tab = InventoryType::Id::SETUP;
				break;
			}
			case Buttons::BT_TAB_ETC:
			{
				tab = InventoryType::Id::ETC;
				break;
			}
			case Buttons::BT_TAB_CASH:
			{
				tab = InventoryType::Id::CASH;
				break;
			}
			case Buttons::BT_GATHER:
			{
				GatherItemsPacket(tab).dispatch();
				break;
			}
			case Buttons::BT_SORT:
			{
				SortItemsPacket(tab).dispatch();
				break;
			}
			case Buttons::BT_FULL:
			{
				set_full(true);

				return Button::State::NORMAL;
			}
			case Buttons::BT_SMALL:
			{
				set_full(false);

				return Button::State::NORMAL;
			}
			case Buttons::BT_COIN:
			{
				// Meso drop has no packet in this client yet
				return Button::State::NORMAL;
			}
			case Buttons::BT_CASHSHOP:
			{
				OutPacket(OutPacket::Opcode::ENTER_CASHSHOP).dispatch();

				return Button::State::NORMAL;
			}
		}

		if (tab != oldtab)
		{
			update_slider();

			buttons[button_by_tab(oldtab)]->set_state(Button::State::NORMAL);
			buttons[button_by_tab(tab)]->set_state(Button::State::PRESSED);

			load_icons();
			set_sort(false);
		}

		return Button::State::IDENTITY;
	}

	void UIItemInventory::doubleclick(Point<int16_t> cursorpos)
	{
		int16_t slot = slot_by_position(cursorpos - position);

		if (icons.count(slot) && is_visible(slot))
			ItemActions::use(tab, slot);
	}

	bool UIItemInventory::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		int16_t slot = slot_by_position(cursorpos - position);

		if (slot > 0)
		{
			int32_t item_id = inventory.get_item_id(tab, slot);
			EquipSlot::Id eqslot;
			bool equip;

			if (item_id && tab == InventoryType::Id::EQUIP)
			{
				eqslot = inventory.find_equipslot(item_id);
				equip = true;
			}
			else
			{
				eqslot = EquipSlot::Id::NONE;
				equip = false;
			}

			ignore_tooltip = true;

			return icon.drop_on_items(tab, eqslot, slot, equip);
		}

		return true;
	}

	Cursor::State UIItemInventory::send_cursor(bool pressed, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(pressed, cursorpos);

		if (dragged)
		{
			clear_tooltip();

			return dstate;
		}

		Point<int16_t> cursor_relative = cursorpos - position;

		if (!full_enabled && slider.isenabled())
		{
			Cursor::State sstate = slider.send_cursor(cursor_relative, pressed);

			if (sstate != Cursor::State::IDLE)
			{
				clear_tooltip();

				return sstate;
			}
		}

		int16_t slot = slot_by_position(cursor_relative);
		Icon* icon = get_icon(slot);
		bool is_icon = icon && is_visible(slot);

		if (is_icon)
		{
			if (pressed)
			{
				Point<int16_t> slotpos = get_slotpos(slot);
				icon->start_drag(cursor_relative - slotpos);
				UI::get().drag_icon(icon);

				clear_tooltip();

				return Cursor::State::GRABBING;
			}
			else if (!ignore_tooltip)
			{
				show_item(slot);

				return Cursor::State::CANGRAB;
			}
			else
			{
				ignore_tooltip = false;

				return Cursor::State::CANGRAB;
			}
		}
		else
		{
			clear_tooltip();

			Cursor::State ret = UIElement::send_cursor(pressed, cursorpos);

			// UIElement::send_cursor flips the active tab to MOUSEOVER on
			// hover and back to NORMAL on leave, wiping the PRESSED state
			// set in button_pressed(). Re-pin it so the current tab keeps
			// its highlighted look even when the cursor moves elsewhere.
			uint16_t active_btn = button_by_tab(tab);
			auto it = buttons.find(active_btn);
			if (it != buttons.end() && it->second
				&& it->second->get_state() != Button::State::PRESSED
				&& it->second->get_state() != Button::State::DISABLED)
			{
				it->second->set_state(Button::State::PRESSED);
			}

			return ret;
		}
	}

	void UIItemInventory::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				toggle_active();
			}
			else if (keycode == KeyAction::Id::TAB)
			{
				clear_tooltip();

				// Cycle in the v83 strip order: Equip, Use, Set-up, Etc, Cash
				InventoryType::Id nexttab;

				switch (tab)
				{
					case InventoryType::Id::EQUIP:
						nexttab = InventoryType::Id::USE;
						break;
					case InventoryType::Id::USE:
						nexttab = InventoryType::Id::SETUP;
						break;
					case InventoryType::Id::SETUP:
						nexttab = InventoryType::Id::ETC;
						break;
					case InventoryType::Id::ETC:
						nexttab = InventoryType::Id::CASH;
						break;
					default:
						nexttab = InventoryType::Id::EQUIP;
						break;
				}

				button_pressed(button_by_tab(nexttab));
			}
		}
	}

	UIElement::Type UIItemInventory::get_type() const
	{
		return TYPE;
	}

	void UIItemInventory::modify(InventoryType::Id type, int16_t slot, int8_t mode, int16_t arg)
	{
		if (slot <= 0)
			return;

		if (type == tab)
		{
			switch (mode)
			{
				case Inventory::Modification::ADD:
				{
					update_slot(slot);

					newtab = type;
					newslot = slot;
					break;
				}
				case Inventory::Modification::CHANGECOUNT:
				case Inventory::Modification::ADDCOUNT:
				{
					if (auto icon = get_icon(slot))
						icon->set_count(arg);

					break;
				}
				case Inventory::Modification::SWAP:
				{
					if (arg != slot)
					{
						update_slot(slot);
						update_slot(arg);
					}

					break;
				}
				case Inventory::Modification::REMOVE:
				{
					update_slot(slot);
					break;
				}
			}

			update_slider();
		}

		switch (mode)
		{
			case Inventory::Modification::ADD:
			case Inventory::Modification::ADDCOUNT:
			{
				newtab = type;
				newslot = slot;
				break;
			}
			case Inventory::Modification::CHANGECOUNT:
			case Inventory::Modification::SWAP:
			case Inventory::Modification::REMOVE:
			{
				if (newslot == slot && newtab == type)
					clear_new();

				break;
			}
		}
	}

	void UIItemInventory::set_sort(bool enabled)
	{
		// v83 has a single Gather/Sort pair that shares one spot: Gather is
		// offered until the server enables sorting for the tab
		sort_enabled = enabled;

		buttons[Buttons::BT_SORT]->set_active(sort_enabled);
		buttons[Buttons::BT_GATHER]->set_active(!sort_enabled);
	}

	void UIItemInventory::change_tab(InventoryType::Id type)
	{
		button_pressed(button_by_tab(type));
	}

	void UIItemInventory::clear_new()
	{
		newtab = InventoryType::Id::NONE;
		newslot = 0;
	}

	void UIItemInventory::toggle_active()
	{
		UIElement::toggle_active();

		if (!active)
		{
			clear_new();
			clear_tooltip();
		}
	}

	void UIItemInventory::send_scroll(double yoffset)
	{
		// Mouse wheel drives the same slider that the scrollbar does, so the
		// visible slot range shifts a row per notch (same as storage/shop panels).
		if (!full_enabled && slider.isenabled())
			slider.send_scroll(yoffset);
	}

	void UIItemInventory::remove_cursor()
	{
		UIDragElement::remove_cursor();

		slider.remove_cursor();
	}

	void UIItemInventory::show_item(int16_t slot)
	{
		if (tab == InventoryType::Id::EQUIP)
		{
			UI::get().show_equip(Tooltip::Parent::ITEMINVENTORY, slot);
		}
		else
		{
			int32_t item_id = inventory.get_item_id(tab, slot);
			UI::get().show_item(Tooltip::Parent::ITEMINVENTORY, item_id);
		}
	}

	void UIItemInventory::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::Parent::ITEMINVENTORY);
	}

	bool UIItemInventory::is_visible(int16_t slot) const
	{
		return !is_not_visible(slot);
	}

	bool UIItemInventory::is_not_visible(int16_t slot) const
	{
		if (full_enabled)
			return slot < 1 || slot > MAXFULLSLOTS;

		auto range = slotrange.at(tab);

		return slot < range.first || slot > range.second;
	}

	int16_t UIItemInventory::slot_by_position(Point<int16_t> cursorpos) const
	{
		int16_t xoff = cursorpos.x() - (full_enabled ? FULL_SLOT_X : SLOT_X);
		int16_t yoff = cursorpos.y() - SLOT_Y;

		if (xoff < 0 || yoff < 0 || yoff >= ROWS * SLOT_PITCH_Y)
			return 0;

		int16_t page = 0;

		if (full_enabled)
		{
			// The expanded view lays four 24-slot blocks side by side
			page = xoff / FULL_PAGE_PITCH;
			xoff = xoff % FULL_PAGE_PITCH;

			if (page >= FULL_PAGES)
				return 0;
		}

		if (xoff >= COLUMNS * SLOT_PITCH_X)
			return 0;

		// The gutters between cells belong to no slot
		if (xoff % SLOT_PITCH_X >= SLOT_SIZE || yoff % SLOT_PITCH_Y >= SLOT_SIZE)
			return 0;

		int16_t col = xoff / SLOT_PITCH_X;
		int16_t row = yoff / SLOT_PITCH_Y;
		int16_t first = full_enabled ? 1 + page * MAXSLOTS : slotrange.at(tab).first;
		int16_t slot = first + row * COLUMNS + col;

		return is_visible(slot) ? slot : 0;
	}

	Point<int16_t> UIItemInventory::get_slotpos(int16_t slot) const
	{
		if (full_enabled)
			return get_full_slotpos(slot);

		int16_t absslot = slot - slotrange.at(tab).first;

		return Point<int16_t>(
			SLOT_X + (absslot % COLUMNS) * SLOT_PITCH_X,
			SLOT_Y + (absslot / COLUMNS) * SLOT_PITCH_Y
			);
	}

	Point<int16_t> UIItemInventory::get_full_slotpos(int16_t slot) const
	{
		int16_t absslot = slot - 1;
		int16_t page = absslot / MAXSLOTS;
		int16_t inpage = absslot % MAXSLOTS;

		return Point<int16_t>(
			FULL_SLOT_X + page * FULL_PAGE_PITCH + (inpage % COLUMNS) * SLOT_PITCH_X,
			SLOT_Y + (inpage / COLUMNS) * SLOT_PITCH_Y
			);
	}

	Point<int16_t> UIItemInventory::get_tabpos(InventoryType::Id tb) const
	{
		// InventoryType EQUIP..CASH (1..5) map straight onto strip cells 0..4
		int16_t index = tb - InventoryType::Id::EQUIP;

		if (index < 0 || index >= NUM_TABS)
			index = 0;

		return Point<int16_t>(TAB_X + TAB_PITCH * index, TAB_Y);
	}

	uint16_t UIItemInventory::button_by_tab(InventoryType::Id tb) const
	{
		switch (tb)
		{
			case InventoryType::Id::EQUIP:
				return Buttons::BT_TAB_EQUIP;
			case InventoryType::Id::USE:
				return Buttons::BT_TAB_USE;
			case InventoryType::Id::SETUP:
				return Buttons::BT_TAB_SETUP;
			case InventoryType::Id::ETC:
				return Buttons::BT_TAB_ETC;
			default:
				return Buttons::BT_TAB_CASH;
		}
	}

	Icon* UIItemInventory::get_icon(int16_t slot)
	{
		auto iter = icons.find(slot);

		if (iter != icons.end())
			return iter->second.get();
		else
			return nullptr;
	}

	void UIItemInventory::set_full(bool enabled)
	{
		full_enabled = enabled;

		dimension = full_enabled ? bg_full_dimensions : bg_dimensions;
		dragarea = Point<int16_t>(dimension.x(), 20);

		// Title-bar buttons hug the right edge of whichever background is
		// shown: BtClose (12x12, origin 0,0), then the expand/shrink and
		// gather/sort pairs (origin 6,6, so placed by centre)
		buttons[Buttons::BT_CLOSE]->set_position(Point<int16_t>(dimension.x() - CLOSE_RIGHT_INSET, 6));
		buttons[Buttons::BT_FULL]->set_position(Point<int16_t>(dimension.x() - FULL_RIGHT_INSET, TITLE_BUTTON_Y));
		buttons[Buttons::BT_SMALL]->set_position(Point<int16_t>(dimension.x() - FULL_RIGHT_INSET, TITLE_BUTTON_Y));
		buttons[Buttons::BT_GATHER]->set_position(Point<int16_t>(dimension.x() - SORT_RIGHT_INSET, TITLE_BUTTON_Y));
		buttons[Buttons::BT_SORT]->set_position(Point<int16_t>(dimension.x() - SORT_RIGHT_INSET, TITLE_BUTTON_Y));

		buttons[Buttons::BT_FULL]->set_active(!full_enabled);
		buttons[Buttons::BT_SMALL]->set_active(full_enabled);
		buttons[Buttons::BT_CASHSHOP]->set_active(full_enabled);

		set_sort(sort_enabled);
		load_icons();
	}

	void UIItemInventory::ItemIcon::set_count(int16_t c)
	{
		count = c;
	}

	Icon::IconType UIItemInventory::ItemIcon::get_type()
	{
		return Icon::IconType::ITEM;
	}

	UIItemInventory::ItemIcon::ItemIcon(const UIItemInventory& parent, InventoryType::Id st, EquipSlot::Id eqs, int16_t s, int32_t iid, int16_t c, bool u, bool cash) : parent(parent)
	{
		sourcetab = st;
		eqsource = eqs;
		source = s;
		item_id = iid;
		count = c;
		untradable = u;
		cashitem = cash;
	}

	void UIItemInventory::ItemIcon::drop_on_stage() const
	{
		ItemActions::drop(sourcetab, source, item_id, count, untradable, cashitem);
	}

	void UIItemInventory::ItemIcon::drop_on_equips(EquipSlot::Id eqslot) const
	{
		switch (sourcetab)
		{
			case InventoryType::Id::EQUIP:
			{
				if (eqsource == eqslot)
					if (ItemActions::can_wear(source))
						EquipItemPacket(source, eqslot).dispatch();

				Sound(Sound::Name::DRAGEND).play();
				break;
			}
			case InventoryType::Id::USE:
			{
				ScrollEquipPacket(source, eqslot).dispatch();
				break;
			}
		}
	}

	bool UIItemInventory::ItemIcon::drop_on_items(InventoryType::Id tab, EquipSlot::Id, int16_t slot, bool) const
	{
		if (tab != sourcetab || slot == source)
			return true;

		MoveItemPacket(tab, source, slot, 1).dispatch();

		return true;
	}

	void UIItemInventory::ItemIcon::drop_on_bindings(Point<int16_t> cursorposition, bool remove) const
	{
		if (sourcetab == InventoryType::Id::USE || sourcetab == InventoryType::Id::SETUP)
		{
			auto keyconfig = UI::get().get_element<UIKeyConfig>();
			Keyboard::Mapping mapping = Keyboard::Mapping(KeyType::ITEM, item_id);

			if (remove)
				keyconfig->unstage_mapping(mapping);
			else
				keyconfig->stage_mapping(cursorposition, mapping);
		}
	}
}
