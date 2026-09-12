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
#include "UIEquipInventory.h"

#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../UITypes/UIItemInventory.h"

#include "../../Audio/Audio.h"
#include "../../Data/ItemData.h"

#include "../../Net/Packets/InventoryPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIEquipInventory::UIEquipInventory(const Inventory& invent) : UIDragElement<PosEQINV>(Point<int16_t>(175, 20)), inventory(invent), pet_shown(false)
	{
		// Board cells (column, row) as labelled on the v83 backgrnd bitmap:
		//
		//   row 0:        CAP
		//   row 1: MEDAL  FOREHEAD            RING     RING
		//   row 2:                  EYE ACC   EAR ACC
		//   row 3: MANTLE CLOTHES   PENDANT   WEAPON   SHIELD
		//   row 4: GLOVES PANTS     BELT      RING     RING
		//   row 5:                  SHOES              PET MP
		//   row 6: TAMING SADDLE    MOB EQUIP PET ACC  PET HP
		//
		// The PET MP / PET HP / PET ACC / MOB EQUIP cells have no EquipSlot
		// id in this client and stay unmapped (0,0 = skipped when drawing).
		auto cell = [](int16_t col, int16_t row)
		{
			return Point<int16_t>(GRID_X + col * GRID_PITCH, GRID_Y + row * GRID_PITCH);
		};

		iconpositions[EquipSlot::Id::HAT]      = cell(1, 0);

		iconpositions[EquipSlot::Id::MEDAL]    = cell(0, 1);
		iconpositions[EquipSlot::Id::FACE]     = cell(1, 1);
		iconpositions[EquipSlot::Id::RING1]    = cell(3, 1);
		iconpositions[EquipSlot::Id::RING2]    = cell(4, 1);

		iconpositions[EquipSlot::Id::EYEACC]   = cell(2, 2);
		iconpositions[EquipSlot::Id::EARACC]   = cell(3, 2);

		iconpositions[EquipSlot::Id::CAPE]     = cell(0, 3);
		iconpositions[EquipSlot::Id::TOP]      = cell(1, 3);
		iconpositions[EquipSlot::Id::PENDANT1] = cell(2, 3);
		iconpositions[EquipSlot::Id::WEAPON]   = cell(3, 3);
		iconpositions[EquipSlot::Id::SHIELD]   = cell(4, 3);

		iconpositions[EquipSlot::Id::GLOVES]   = cell(0, 4);
		iconpositions[EquipSlot::Id::BOTTOM]   = cell(1, 4);
		iconpositions[EquipSlot::Id::BELT]     = cell(2, 4);
		iconpositions[EquipSlot::Id::RING3]    = cell(3, 4);
		iconpositions[EquipSlot::Id::RING4]    = cell(4, 4);

		iconpositions[EquipSlot::Id::SHOES]    = cell(2, 5);

		iconpositions[EquipSlot::Id::TAMEDMOB] = cell(0, 6);
		iconpositions[EquipSlot::Id::SADDLE]   = cell(1, 6);

		// v83 ships the equip window in UIWindow.img/Equip (no UIWindow2.img)
		nl::node Equip = nl::nx::ui["UIWindow.img"]["Equip"];

		nl::node backgrnd = Equip["backgrnd"];
		Point<int16_t> bg_dimensions = Texture(backgrnd).get_dimensions();

		sprites.emplace_back(backgrnd);

		pet_panel = Equip["pet"];

		// v83 Basic.img only has BtClose (12x12) and BtClose2 (14x14)
		nl::node close = nl::nx::ui["Basic.img"]["BtClose"];
		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close, Point<int16_t>(bg_dimensions.x() - 18, 6));

		// Bottom bar (y=277..300): "SLOT+" (32x11, origin 0,0) at the left,
		// "PET EQUIP >" (54x18, origin 0,0) at the right
		buttons[Buttons::BT_SLOTPLUS] = std::make_unique<MapleButton>(Equip["BtCashshop"], Point<int16_t>(8, 283));
		buttons[Buttons::BT_PETSHOW] = std::make_unique<MapleButton>(Equip["BtPetEquipShow"], Point<int16_t>(bg_dimensions.x() - 60, 280));

		// "<" (17x16, origin 8,8 - placed by centre) on the pet panel's own
		// bottom bar (panel y=150..181), 14px in from its left edge
		Point<int16_t> pet_offset = Point<int16_t>(bg_dimensions.x(), 0);
		buttons[Buttons::BT_PETHIDE] = std::make_unique<MapleButton>(Equip["BtPetEquipHide"], pet_offset + Point<int16_t>(22, 168));

		dimension = bg_dimensions;
		dragarea = Point<int16_t>(bg_dimensions.x(), 20);

		show_pet_panel(false);
		load_icons();
	}

	void UIEquipInventory::draw(float alpha) const
	{
		UIElement::draw_sprites(alpha);

		if (pet_shown)
			pet_panel.draw(position + pet_panel_offset());

		for (auto iter : icons)
		{
			// Slots without a cell on this board (pocket, emblem, ...) stay
			// at (0,0); skip them so no icon stacks in the corner.
			Point<int16_t> ipos = iconpositions[iter.first];

			if (iter.second && (ipos.x() != 0 || ipos.y() != 0))
				iter.second->draw(position + ipos);
		}

		// The hide button lies on the pet panel, so buttons go last
		UIElement::draw_buttons(alpha);
	}

	Button::State UIEquipInventory::button_pressed(uint16_t id)
	{
		switch (id)
		{
			case Buttons::BT_CLOSE:
			{
				toggle_active();
				break;
			}
			case Buttons::BT_SLOTPLUS:
			{
				// Equip slot expansion is bought in the Cash Shop
				OutPacket(OutPacket::Opcode::ENTER_CASHSHOP).dispatch();
				break;
			}
			case Buttons::BT_PETSHOW:
			{
				show_pet_panel(true);
				break;
			}
			case Buttons::BT_PETHIDE:
			{
				show_pet_panel(false);
				break;
			}
			default:
			{
				break;
			}
		}

		return Button::State::NORMAL;
	}

	void UIEquipInventory::update_slot(EquipSlot::Id slot)
	{
		// Forget a dangling drag reference before replacing/destroying the
		// icon (also lets the empty-slot branch actually free it — release()
		// leaked the icon to dodge exactly this dangling problem)
		if (icons[slot])
			UI::get().purge_icon(icons[slot].get());

		if (int32_t item_id = inventory.get_item_id(InventoryType::Id::EQUIPPED, slot))
		{
			const Texture& texture = ItemData::get(item_id).get_icon(false);

			icons[slot] = std::make_unique<Icon>(
				std::make_unique<EquipIcon>(slot),
				texture,
				-1
				);
		}
		else if (icons[slot])
		{
			icons[slot].reset();
		}

		clear_tooltip();
	}

	void UIEquipInventory::load_icons()
	{
		for (auto iter : EquipSlot::values)
			if (icons[iter])
				UI::get().purge_icon(icons[iter].get());

		icons.clear();

		for (auto iter : EquipSlot::values)
			update_slot(iter);
	}

	Cursor::State UIEquipInventory::send_cursor(bool pressed, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(pressed, cursorpos);

		if (dragged)
		{
			clear_tooltip();

			return dstate;
		}

		EquipSlot::Id slot = slot_by_position(cursorpos);

		if (auto icon = icons[slot].get())
		{
			if (pressed)
			{
				icon->start_drag(cursorpos - position - iconpositions[slot]);

				UI::get().drag_icon(icon);

				clear_tooltip();

				return Cursor::State::GRABBING;
			}
			else
			{
				show_equip(slot);

				return Cursor::State::CANGRAB;
			}
		}
		else
		{
			clear_tooltip();

			return Cursor::State::IDLE;
		}
	}

	void UIEquipInventory::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				toggle_active();
			}
		}
	}

	UIElement::Type UIEquipInventory::get_type() const
	{
		return TYPE;
	}

	void UIEquipInventory::doubleclick(Point<int16_t> cursorpos)
	{
		EquipSlot::Id slot = slot_by_position(cursorpos);

		if (icons[slot])
			if (int16_t freeslot = inventory.find_free_slot(InventoryType::Id::EQUIP))
				UnequipItemPacket(slot, freeslot).dispatch();
	}

	bool UIEquipInventory::is_in_range(Point<int16_t> cursorpos) const
	{
		Rectangle<int16_t> bounds = Rectangle<int16_t>(position, position + dimension);

		if (bounds.contains(cursorpos))
			return true;

		if (pet_shown && pet_panel.is_valid())
		{
			Point<int16_t> pet_pos = position + pet_panel_offset();
			Rectangle<int16_t> pet_bounds = Rectangle<int16_t>(pet_pos, pet_pos + pet_panel.get_dimensions());

			return pet_bounds.contains(cursorpos);
		}

		return false;
	}

	bool UIEquipInventory::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		if (EquipSlot::Id slot = slot_by_position(cursorpos))
			icon.drop_on_equips(slot);

		return true;
	}

	void UIEquipInventory::toggle_active()
	{
		clear_tooltip();

		UIElement::toggle_active();

		if (!active)
			show_pet_panel(false);
	}

	void UIEquipInventory::modify(int16_t pos, int8_t mode, int16_t arg)
	{
		EquipSlot::Id eqpos = EquipSlot::by_id(pos);
		EquipSlot::Id eqarg = EquipSlot::by_id(arg);

		switch (mode)
		{
			case 0:
			case 3:
				update_slot(eqpos);
				break;
			case 2:
				update_slot(eqpos);
				update_slot(eqarg);
				break;
		}
	}

	void UIEquipInventory::show_equip(EquipSlot::Id slot)
	{
		UI::get().show_equip(Tooltip::Parent::EQUIPINVENTORY, slot);
	}

	void UIEquipInventory::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::Parent::EQUIPINVENTORY);
	}

	EquipSlot::Id UIEquipInventory::slot_by_position(Point<int16_t> cursorpos) const
	{
		for (auto iter : iconpositions)
		{
			// Unmapped slots sit at (0,0); never resolve them
			if (iter.second.x() == 0 && iter.second.y() == 0)
				continue;

			Rectangle<int16_t> iconrect = Rectangle<int16_t>(
				position + iter.second,
				position + iter.second + Point<int16_t>(CELL_SIZE, CELL_SIZE)
				);

			if (iconrect.contains(cursorpos))
				return iter.first;
		}

		return EquipSlot::Id::NONE;
	}

	void UIEquipInventory::show_pet_panel(bool show)
	{
		pet_shown = show;

		buttons[Buttons::BT_PETSHOW]->set_active(!pet_shown);
		buttons[Buttons::BT_PETHIDE]->set_active(pet_shown);
	}

	Point<int16_t> UIEquipInventory::pet_panel_offset() const
	{
		// Docked flush to the right edge, top aligned
		return Point<int16_t>(dimension.x(), 0);
	}

	UIEquipInventory::EquipIcon::EquipIcon(int16_t s)
	{
		source = s;
	}

	void UIEquipInventory::EquipIcon::drop_on_stage() const
	{
		Sound(Sound::Name::DRAGEND).play();
	}

	void UIEquipInventory::EquipIcon::drop_on_equips(EquipSlot::Id slot) const
	{
		if (source == slot)
			Sound(Sound::Name::DRAGEND).play();
	}

	bool UIEquipInventory::EquipIcon::drop_on_items(InventoryType::Id tab, EquipSlot::Id eqslot, int16_t slot, bool equip) const
	{
		if (tab != InventoryType::Id::EQUIP)
		{
			if (auto iteminventory = UI::get().get_element<UIItemInventory>())
			{
				if (iteminventory->is_active())
				{
					iteminventory->change_tab(InventoryType::Id::EQUIP);
					return false;
				}
			}
		}

		if (equip)
		{
			if (eqslot == source)
				EquipItemPacket(slot, eqslot).dispatch();
		}
		else
		{
			UnequipItemPacket(source, slot).dispatch();
		}

		return true;
	}

	Icon::IconType UIEquipInventory::EquipIcon::get_type()
	{
		return Icon::IconType::EQUIP;
	}
}
