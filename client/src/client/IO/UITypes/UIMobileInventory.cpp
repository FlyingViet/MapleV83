//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileInventory.h"

#include "UIMobileItemMenu.h"

#include "../ItemActions.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Data/EquipData.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/InventoryPackets.h"

#include <algorithm>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 400;
		constexpr int16_t PAD = 16;
		constexpr int16_t TAB_Y = 76;
		constexpr int16_t TAB_H = 34;
		constexpr int16_t GRID_Y = TAB_Y + TAB_H + 12;
		constexpr int16_t COLS = 6;
		constexpr int16_t CELL = 54;
		constexpr int16_t GAP = 8;
		constexpr int16_t PITCH = CELL + GAP;
		constexpr int16_t GRID_X = PAD + (W - PAD * 2 - (COLS * CELL + (COLS - 1) * GAP)) / 2;
		constexpr int16_t FOOTER_H = 52;

		constexpr InventoryType::Id TABS[5] = {
			InventoryType::Id::EQUIP,
			InventoryType::Id::USE,
			InventoryType::Id::SETUP,
			InventoryType::Id::ETC,
			InventoryType::Id::CASH
		};

		constexpr const char* TAB_NAMES[5] = { "Equip", "Use", "Set-up", "Etc", "Cash" };

		std::string with_commas(int64_t value)
		{
			std::string digits = std::to_string(value < 0 ? -value : value);
			std::string out;

			for (size_t i = 0; i < digits.size(); i++)
			{
				if (i > 0 && (digits.size() - i) % 3 == 0)
					out += ',';

				out += digits[i];
			}

			return value < 0 ? "-" + out : out;
		}
	}

	UIMobileInventory::UIMobileInventory(const Inventory& inv) : UIMobilePanel(Edge::RIGHT, W), inventory(inv) {}

	// ---- Layout -------------------------------------------------------------

	Rect UIMobileInventory::tab_rect(int index) const
	{
		int16_t w = static_cast<int16_t>((W - PAD * 2 - 6 * 4) / 5);

		return rect(static_cast<int16_t>(position.x() + PAD + index * (w + 6)), static_cast<int16_t>(position.y() + TAB_Y), w, TAB_H);
	}

	Rect UIMobileInventory::footer_rect() const
	{
		return rect(static_cast<int16_t>(position.x() + PAD), static_cast<int16_t>(position.y() + HEIGHT - PAD - FOOTER_H),
			static_cast<int16_t>(W - PAD * 2), FOOTER_H);
	}

	Rect UIMobileInventory::sort_rect() const
	{
		Rect f = footer_rect();

		return rect(static_cast<int16_t>(f.right() - 10 - 96), static_cast<int16_t>(f.top() + 9), 96, 34);
	}

	Rectangle<int16_t> UIMobileInventory::scroll_area() const
	{
		int16_t top = static_cast<int16_t>(position.y() + GRID_Y);
		int16_t bottom = static_cast<int16_t>(footer_rect().top() - 10);

		return rect(static_cast<int16_t>(position.x() + PAD), top, static_cast<int16_t>(W - PAD * 2), static_cast<int16_t>(bottom - top));
	}

	int16_t UIMobileInventory::scroll_content() const
	{
		int16_t rows = static_cast<int16_t>((inventory.get_slotmax(tab) + COLS - 1) / COLS);

		return static_cast<int16_t>(rows * PITCH - GAP);
	}

	Rect UIMobileInventory::cell_rect(int16_t slot) const
	{
		int16_t index = static_cast<int16_t>(slot - 1);
		Rectangle<int16_t> area = scroll_area();

		return rect(static_cast<int16_t>(position.x() + GRID_X + (index % COLS) * PITCH),
			static_cast<int16_t>(area.top() + (index / COLS) * PITCH - scroll), CELL, CELL);
	}

	int16_t UIMobileInventory::slot_at(Point<int16_t> pos) const
	{
		Rectangle<int16_t> area = scroll_area();

		if (!area.contains(pos))
			return 0;

		int16_t col = static_cast<int16_t>((pos.x() - position.x() - GRID_X) / PITCH);
		int16_t row = static_cast<int16_t>((pos.y() - area.top() + scroll) / PITCH);

		if (pos.x() < position.x() + GRID_X || col < 0 || col >= COLS || row < 0)
			return 0;

		int16_t slot = static_cast<int16_t>(row * COLS + col + 1);

		if (slot > inventory.get_slotmax(tab) || !cell_rect(slot).contains(pos))
			return 0;

		return slot;
	}

	int16_t UIMobileInventory::used_slots() const
	{
		int16_t used = 0;

		for (int16_t slot = 1; slot <= inventory.get_slotmax(tab); slot++)
			if (inventory.get_item_id(tab, slot))
				used++;

		return used;
	}

	// ---- Input --------------------------------------------------------------

	int32_t UIMobileInventory::hit(Point<int16_t> pos) const
	{
		if (panel_close(frame()).contains(pos))
			return CLOSE_TARGET;

		for (int i = 0; i < 5; i++)
			if (tab_rect(i).contains(pos))
				return TAB + i;

		if (sort_rect().contains(pos))
			return SORT;

		if (int16_t slot = slot_at(pos))
			if (inventory.get_item_id(tab, slot))
				return SLOT_TARGET + slot;

		return NO_TARGET;
	}

	void UIMobileInventory::activate(int32_t target)
	{
		// The release of a double tap: the item was already used.
		if (UI::get().get_press_serial() == double_press)
			return;

		if (target >= SLOT_TARGET)
		{
			open_menu(static_cast<int16_t>(target - SLOT_TARGET));
		}
		else if (target == SORT)
		{
			if (sorting)
				return;

			Sound(Sound::Name::BUTTONCLICK).play();
			sorting = true;
			sort_at = std::chrono::steady_clock::now();
			GatherItemsPacket(tab).dispatch();
		}
		else if (target >= TAB)
		{
			change_tab(TABS[target - TAB]);
		}
	}

	void UIMobileInventory::doubleclick(Point<int16_t> pos)
	{
		int16_t slot = slot_at(pos);

		if (!slot || !inventory.get_item_id(tab, slot))
			return;

		double_press = UI::get().get_press_serial();
		UI::get().remove(UIElement::Type::MOBILEITEMMENU);
		ItemActions::use(tab, slot);
	}

	bool UIMobileInventory::send_icon(const Icon& icon, Point<int16_t> pos)
	{
		// Onto a tab: switch to it and keep carrying.
		for (int i = 0; i < 5; i++)
		{
			if (tab_rect(i).contains(pos))
			{
				change_tab(TABS[i]);

				return false;
			}
		}

		int16_t slot = slot_at(pos);

		if (!slot)
			return true;

		int32_t target = inventory.get_item_id(tab, slot);
		bool equip = target && tab == InventoryType::Id::EQUIP;
		EquipSlot::Id eqslot = equip ? inventory.find_equipslot(target) : EquipSlot::Id::NONE;

		return icon.drop_on_items(tab, eqslot, slot, equip);
	}

	void UIMobileInventory::open_menu(int16_t slot)
	{
		Sound(Sound::Name::BUTTONCLICK).play();

		Rect cell = cell_rect(slot);
		Point<int16_t> anchor(cell.left(), static_cast<int16_t>(cell.top() + CELL / 2));

		UI::get().emplace<UIMobileItemMenu>(UIMobileItemMenu::Source::INVENTORY, tab, slot,
			inventory.get_item_id(tab, slot), anchor);
	}

	void UIMobileInventory::change_tab(InventoryType::Id t)
	{
		if (t == tab)
			return;

		Sound(Sound::Name::TAB).play();
		tab = t;
		scroll = 0;
		UI::get().remove(UIElement::Type::MOBILEITEMMENU);
	}

	bool UIMobileInventory::pick_up(int16_t slot)
	{
		if (carry)
			UI::get().purge_icon(carry.get());

		carry = ItemActions::make_item_icon(tab, slot);

		if (!carry)
			return false;

		carry->start_drag(Point<int16_t>(0, 0));
		UI::get().drag_icon(carry.get());
		Sound(Sound::Name::DRAGSTART).play();

		return true;
	}

	void UIMobileInventory::on_gathered()
	{
		if (sorting)
			SortItemsPacket(tab).dispatch();
	}

	void UIMobileInventory::on_sorted()
	{
		sorting = false;
	}

	void UIMobileInventory::update()
	{
		UIMobilePanel::update();
		clamp_scroll();

		// The server answers both steps at once; don't wait on it forever.
		if (sorting && std::chrono::steady_clock::now() - sort_at > std::chrono::seconds(3))
			sorting = false;
	}

	UIElement::Type UIMobileInventory::get_type() const
	{
		return TYPE;
	}

	// ---- Drawing ------------------------------------------------------------

	void UIMobileInventory::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();

		int tab_index = 0;

		for (int i = 0; i < 5; i++)
			if (TABS[i] == tab)
				tab_index = i;

		panel(f, "Inventory", std::string(TAB_NAMES[tab_index]) + "  \xC2\xB7  " + std::to_string(used_slots())
			+ " / " + std::to_string(inventory.get_slotmax(tab)) + " slots");
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		for (int i = 0; i < 5; i++)
			button(tab_rect(i), TAB_NAMES[i], TABS[i] == tab ? Style::SELECTED : Style::GHOST, true, pressed == TAB + i, Font::SMALL_BOLD);

		// Which slot the menu is open on, and which one is being carried.
		int16_t menu_slot = 0;

		if (auto menu = UI::get().get_element<UIMobileItemMenu>())
			if (menu->is_active())
				menu_slot = menu->slot_for(UIMobileItemMenu::Source::INVENTORY, tab);

		int16_t carried = 0;

		if (carry && carry->get_drag() && carry->get_source_tab() == tab)
			carried = carry->get_source_slot();

		Rectangle<int16_t> area = scroll_area();
		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		for (int16_t slot = 1; slot <= inventory.get_slotmax(tab); slot++)
		{
			Rect cell = cell_rect(slot);

			if (cell.bottom() < area.top() || cell.top() > area.bottom())
				continue;

			int32_t item_id = inventory.get_item_id(tab, slot);
			bool down = pressed == SLOT_TARGET + slot;

			if (slot == menu_slot || slot == carried)
			{
				fill(cell, Theme::ACCENT, 12);
				fill(inset(cell, 2, 2), down ? Theme::CARD_HI : Theme::CARD, 10);
			}
			else
			{
				fill(cell, item_id ? (down ? Theme::CARD_HI : Theme::CARD) : Theme::CARD.alpha(0.75f), 12);
			}

			if (!item_id)
				continue;

			// Gear this character can't wear yet is dimmed with a red mark.
			bool wearable = tab != InventoryType::Id::EQUIP || ItemActions::meets_requirements(item_id);
			float opacity = slot == carried ? 0.35f : (wearable ? 1.0f : 0.5f);

			MobileUI::icon(ItemData::get(item_id).get_icon(false), inset(cell, 9, 9), opacity);

			if (!wearable)
				fill(rect(static_cast<int16_t>(cell.right() - 13), static_cast<int16_t>(cell.top() + 5), 8, 8), Theme::RED, 4);

			if (tab != InventoryType::Id::EQUIP)
			{
				int16_t count = inventory.get_item_count(tab, slot);

				if (count > 1)
				{
					std::string label = count >= 1000 ? std::to_string(count / 1000) + "k" : std::to_string(count);
					int16_t w = static_cast<int16_t>(text_width(label, Font::SMALL_BOLD) + 10);
					Rect badge = rect(static_cast<int16_t>(cell.right() - w - 3), static_cast<int16_t>(cell.bottom() - 19), w, 16);

					fill(badge, RGBA{ 0.0f, 0.0f, 0.0f, 0.62f }, 8);
					text_in(label, Font::SMALL_BOLD, Theme::TEXT, badge);
				}
			}
		}

		GraphicsGL::get().end_clip();

		// Scroll position, in the right margin.
		if (int16_t max = scroll_max())
		{
			float visible = static_cast<float>(area.height()) / scroll_content();
			int16_t bar = static_cast<int16_t>(std::max(24.0f, area.height() * visible));
			int16_t y = static_cast<int16_t>(area.top() + (area.height() - bar) * (static_cast<float>(scroll) / max));

			fill(rect(static_cast<int16_t>(f.right() - 7), y, 3, bar), Theme::FAINT.alpha(0.7f), 2);
		}

		// Mesos and sort.
		Rect foot = footer_rect();
		fill(foot, Theme::CARD, 14);
		fill(rect(static_cast<int16_t>(foot.left() + 14), static_cast<int16_t>(foot.top() + 18), 16, 16), Theme::GOLD, 8);
		text("MESOS", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(foot.left() + 40), static_cast<int16_t>(foot.top() + 8)));
		text(with_commas(inventory.get_meso()), Font::TITLE, Theme::GOLD,
			Point<int16_t>(static_cast<int16_t>(foot.left() + 40), static_cast<int16_t>(foot.top() + 23)));
		button(sort_rect(), sorting ? "Sorting..." : "Sort", Style::SECONDARY, !sorting, pressed == SORT, Font::SMALL_BOLD);
	}
}
