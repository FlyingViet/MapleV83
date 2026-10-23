//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileEquip.h"

#include "UIMobileItemMenu.h"

#include "../ItemActions.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"

#include <algorithm>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 400;
		constexpr int16_t PAD = 16;
		constexpr int16_t TOP_Y = 76;
		constexpr int16_t TILE = 58;
		constexpr int16_t SIDE_GAP = 8;
		constexpr int16_t SIDE_ROWS = 6;
		constexpr int16_t BOTTOM_Y = TOP_Y + SIDE_ROWS * (TILE + SIDE_GAP) + 4;
		constexpr int16_t BOTTOM_COUNT = 6;

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

	const std::array<UIMobileEquip::SlotInfo, UIMobileEquip::SLOT_COUNT>& UIMobileEquip::slots()
	{
		// Left column, right column (6 each), then the bottom row.
		static const std::array<SlotInfo, SLOT_COUNT> SLOTS = { {
			{ EquipSlot::Id::HAT, "Hat" },
			{ EquipSlot::Id::FACE, "Face" },
			{ EquipSlot::Id::EYEACC, "Eyes" },
			{ EquipSlot::Id::EARACC, "Earrings" },
			{ EquipSlot::Id::PENDANT1, "Pendant" },
			{ EquipSlot::Id::CAPE, "Cape" },
			{ EquipSlot::Id::WEAPON, "Weapon" },
			{ EquipSlot::Id::SHIELD, "Shield" },
			{ EquipSlot::Id::TOP, "Top" },
			{ EquipSlot::Id::BOTTOM, "Bottom" },
			{ EquipSlot::Id::GLOVES, "Gloves" },
			{ EquipSlot::Id::SHOES, "Shoes" },
			{ EquipSlot::Id::BELT, "Belt" },
			{ EquipSlot::Id::MEDAL, "Medal" },
			{ EquipSlot::Id::RING1, "Ring" },
			{ EquipSlot::Id::RING2, "Ring" },
			{ EquipSlot::Id::RING3, "Ring" },
			{ EquipSlot::Id::RING4, "Ring" }
		} };

		return SLOTS;
	}

	const std::array<UIMobileEquip::PetSlotInfo, UIMobileEquip::PET_COUNT>& UIMobileEquip::pet_slots()
	{
		// Left column, then right column.
		static const std::array<PetSlotInfo, PET_COUNT> SLOTS = { {
			{ EquipSlot::Pet::ITEM_POUCH, "Item Pouch" },
			{ EquipSlot::Pet::MESO_MAGNET, "Meso Magnet" },
			{ EquipSlot::Pet::AUTO_HP, "Auto HP" },
			{ EquipSlot::Pet::AUTO_MP, "Auto MP" },
			{ EquipSlot::Pet::WING_BOOTS, "Wing Boots" },
			{ EquipSlot::Pet::BINOCULAR, "Binocular" },
			{ EquipSlot::Pet::MAGIC_SCALES, "Magic Scales" },
			{ EquipSlot::Pet::ITEM_IGNORE, "Item Ignore" },
			{ EquipSlot::Pet::WEAR, "Pet Wear" },
			{ EquipSlot::Pet::LABEL, "Label Ring" },
			{ EquipSlot::Pet::QUOTE, "Quote Ring" }
		} };

		return SLOTS;
	}

	UIMobileEquip::UIMobileEquip(const Inventory& inv, const CharStats& st) : UIMobilePanel(Edge::LEFT, W), inventory(inv), stats(st)
	{
		refresh_look();
	}

	// ---- Layout -------------------------------------------------------------

	Rect UIMobileEquip::slot_rect(size_t index) const
	{
		int16_t x0 = position.x();
		int16_t y0 = position.y();

		if (index < SIDE_ROWS * 2)
		{
			bool right = index >= SIDE_ROWS;
			int16_t row = static_cast<int16_t>(index % SIDE_ROWS);
			int16_t x = right ? static_cast<int16_t>(x0 + W - PAD - TILE) : static_cast<int16_t>(x0 + PAD);

			return rect(x, static_cast<int16_t>(y0 + TOP_Y + row * (TILE + SIDE_GAP)), TILE, TILE);
		}

		int16_t col = static_cast<int16_t>(index - SIDE_ROWS * 2);
		int16_t gap = static_cast<int16_t>((W - PAD * 2 - BOTTOM_COUNT * TILE) / (BOTTOM_COUNT - 1));

		return rect(static_cast<int16_t>(x0 + PAD + col * (TILE + gap)), static_cast<int16_t>(y0 + BOTTOM_Y), TILE, TILE);
	}

	Rect UIMobileEquip::stage_rect() const
	{
		int16_t left = static_cast<int16_t>(position.x() + PAD + TILE + 10);
		int16_t right = static_cast<int16_t>(position.x() + W - PAD - TILE - 10);

		return rect(left, static_cast<int16_t>(position.y() + TOP_Y), static_cast<int16_t>(right - left),
			static_cast<int16_t>(SIDE_ROWS * (TILE + SIDE_GAP) - SIDE_GAP));
	}

	Rect UIMobileEquip::layer_rect(int layer) const
	{
		constexpr int16_t SEG_W = 68;
		constexpr int16_t SEG_GAP = 4;
		int16_t right = static_cast<int16_t>(position.x() + W - 60);
		int16_t left = static_cast<int16_t>(right - SEG_W * 3 - SEG_GAP * 2);

		return rect(static_cast<int16_t>(left + layer * (SEG_W + SEG_GAP)), static_cast<int16_t>(position.y() + 16), SEG_W, 32);
	}

	int16_t UIMobileEquip::shown_slot(size_t index) const
	{
		if (pets)
			return index < PET_COUNT ? pet_slots()[index].slot : 0;

		int16_t id = static_cast<int16_t>(slots()[index].id);

		return cosmetic ? static_cast<int16_t>(id + EquipSlot::CASH_OFFSET) : id;
	}

	int32_t UIMobileEquip::shown_item(size_t index) const
	{
		if (pets)
			return worn_pet[index];

		return cosmetic ? worn_cash[index] : worn[index];
	}

	size_t UIMobileEquip::slot_at(Point<int16_t> pos) const
	{
		for (size_t i = 0; i < SLOT_COUNT; i++)
			if (slot_rect(i).contains(pos))
				return i;

		return SLOT_COUNT;
	}

	// ---- State --------------------------------------------------------------

	void UIMobileEquip::refresh_look()
	{
		look = Stage::get().get_player().get_look();
		look.set_stance(Stance::Id::STAND1);
	}

	void UIMobileEquip::update()
	{
		UIMobilePanel::update();

		// Redress the preview when anything worn changes.
		bool changed = false;

		for (size_t i = 0; i < SLOT_COUNT; i++)
		{
			int16_t slot = static_cast<int16_t>(slots()[i].id);
			int32_t id = inventory.get_item_id(InventoryType::Id::EQUIPPED, slot);
			int32_t cash = inventory.get_item_id(InventoryType::Id::EQUIPPED, static_cast<int16_t>(slot + EquipSlot::CASH_OFFSET));

			if (id != worn[i] || cash != worn_cash[i])
			{
				worn[i] = id;
				worn_cash[i] = cash;
				changed = true;
			}
		}

		for (size_t i = 0; i < PET_COUNT; i++)
			worn_pet[i] = inventory.get_item_id(InventoryType::Id::EQUIPPED, pet_slots()[i].slot);

		if (changed)
			refresh_look();

		look.update(Constants::TIMESTEP);
	}

	// ---- Input --------------------------------------------------------------

	int32_t UIMobileEquip::hit(Point<int16_t> pos) const
	{
		if (panel_close(frame()).contains(pos))
			return CLOSE_TARGET;

		if (layer_rect(0).contains(pos))
			return LAYER_GEAR;

		if (layer_rect(1).contains(pos))
			return LAYER_COSMETIC;

		if (layer_rect(2).contains(pos))
			return LAYER_PET;

		size_t index = slot_at(pos);

		if (index < SLOT_COUNT && shown_item(index))
			return SLOT + static_cast<int32_t>(index);

		return NO_TARGET;
	}

	void UIMobileEquip::activate(int32_t target)
	{
		// The release of a double tap: the item already came off.
		if (UI::get().get_press_serial() == double_press)
			return;

		if (target >= SLOT)
		{
			open_menu(static_cast<size_t>(target - SLOT));
		}
		else if (target == LAYER_GEAR || target == LAYER_COSMETIC || target == LAYER_PET)
		{
			bool cash = target == LAYER_COSMETIC;
			bool pet = target == LAYER_PET;

			if (cash != cosmetic || pet != pets)
			{
				Sound(Sound::Name::TAB).play();
				cosmetic = cash;
				pets = pet;
				UI::get().remove(UIElement::Type::MOBILEITEMMENU);
			}
		}
	}

	void UIMobileEquip::doubleclick(Point<int16_t> pos)
	{
		size_t index = slot_at(pos);

		if (index >= SLOT_COUNT || !shown_item(index))
			return;

		double_press = UI::get().get_press_serial();
		UI::get().remove(UIElement::Type::MOBILEITEMMENU);
		ItemActions::unequip(static_cast<EquipSlot::Id>(shown_slot(index)));
	}

	bool UIMobileEquip::send_icon(const Icon& icon, Point<int16_t> pos)
	{
		size_t index = slot_at(pos);

		// The item picks its layer: cash items go over the gear.
		if (pets && index < PET_COUNT)
			icon.drop_on_equips(static_cast<EquipSlot::Id>(pet_slots()[index].slot));
		else if (!pets && index < SLOT_COUNT)
			icon.drop_on_equips(slots()[index].id);

		return true;
	}

	void UIMobileEquip::open_menu(size_t index)
	{
		Sound(Sound::Name::BUTTONCLICK).play();

		Rect r = slot_rect(index);
		Point<int16_t> anchor(r.right(), static_cast<int16_t>(r.top() + TILE / 2));

		UI::get().emplace<UIMobileItemMenu>(UIMobileItemMenu::Source::EQUIPPED, InventoryType::Id::EQUIPPED,
			shown_slot(index), shown_item(index), anchor);
	}

	bool UIMobileEquip::pick_up(EquipSlot::Id slot)
	{
		if (carry)
			UI::get().purge_icon(carry.get());

		carry = ItemActions::make_worn_icon(slot);
		carried = slot;

		if (!carry)
			return false;

		carry->start_drag(Point<int16_t>(0, 0));
		UI::get().drag_icon(carry.get());
		Sound(Sound::Name::DRAGSTART).play();

		return true;
	}

	UIElement::Type UIMobileEquip::get_type() const
	{
		return TYPE;
	}

	// ---- Drawing ------------------------------------------------------------

	void UIMobileEquip::draw(float inter) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();

		panel(f, "Equipment", "Lv " + std::to_string(stats.get_stat(MapleStat::Id::LEVEL)) + "  \xC2\xB7  " + stats.get_jobname());
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		bool gear = !cosmetic && !pets;
		button(layer_rect(0), "Gear", gear ? Style::SELECTED : Style::GHOST, true, pressed == LAYER_GEAR, Font::SMALL_BOLD);
		button(layer_rect(1), "Cosmetic", cosmetic ? Style::SELECTED : Style::GHOST, true, pressed == LAYER_COSMETIC, Font::SMALL_BOLD);
		button(layer_rect(2), "Pet", pets ? Style::SELECTED : Style::GHOST, true, pressed == LAYER_PET, Font::SMALL_BOLD);

		// The character, with the numbers the gear adds up to.
		Rect stage = stage_rect();
		fill(stage, Theme::CARD, 16);

		if (pets)
		{
			draw_pet_stage(stage);
		}
		else
		{

		int16_t cx = static_cast<int16_t>(stage.left() + stage.width() / 2);
		int16_t floor = static_cast<int16_t>(stage.bottom() - 112);

		fill(rect(static_cast<int16_t>(cx - 52), static_cast<int16_t>(floor - 6), 104, 12), RGBA{ 0.0f, 0.0f, 0.0f, 0.28f }, 6);
		look.draw(DrawArgument(Point<int16_t>(cx, floor), 2.0f, 2.0f), inter);

		text(stats.get_name(), Font::TITLE, Theme::TEXT, Point<int16_t>(cx, static_cast<int16_t>(floor + 14)), Text::Alignment::CENTER);

		struct Figure { const char* label; std::string value; };
		Figure figures[3] = {
			{ "ATTACK", with_commas(stats.get_mindamage()) + " ~ " + with_commas(stats.get_maxdamage()) },
			{ "W. DEF", std::to_string(stats.get_total(EquipStat::Id::WDEF)) },
			{ "MAGIC", std::to_string(stats.get_total(EquipStat::Id::MAGIC)) }
		};

		int16_t fy = static_cast<int16_t>(stage.bottom() - 62);
		int16_t fw = static_cast<int16_t>((stage.width() - 24) / 3);

		for (int i = 0; i < 3; i++)
		{
			Rect cell = rect(static_cast<int16_t>(stage.left() + 8 + i * (fw + 4)), fy, fw, 52);
			fill(cell, Theme::TRACK.alpha(0.6f), 10);
			text(figures[i].label, Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(cell.left() + fw / 2), static_cast<int16_t>(cell.top() + 7)), Text::Alignment::CENTER);
			text(ellipsize(figures[i].value, Font::BODY_BOLD, static_cast<int16_t>(fw - 8)), Font::BODY_BOLD, Theme::TEXT,
				Point<int16_t>(static_cast<int16_t>(cell.left() + fw / 2), static_cast<int16_t>(cell.top() + 25)), Text::Alignment::CENTER);
		}
		}

		// Which slot the item menu is open on, and which one is being carried.
		int16_t menu_slot = 0;

		if (auto menu = UI::get().get_element<UIMobileItemMenu>())
			if (menu->is_active())
				menu_slot = menu->slot_for(UIMobileItemMenu::Source::EQUIPPED, InventoryType::Id::EQUIPPED);

		bool carrying = carry && carry->get_drag();
		bool overall = !pets && shown_item(8) / 10000 == 105;

		for (size_t i = 0; i < SLOT_COUNT; i++)
		{
			if (pets && i >= PET_COUNT)
				continue;

			const SlotInfo& info = slots()[i];
			const char* label = pets ? pet_slots()[i].label : info.label;
			Rect r = slot_rect(i);
			int16_t slot = shown_slot(i);
			int32_t item = shown_item(i);
			bool down = pressed == SLOT + static_cast<int32_t>(i);
			bool held = carrying && carried == slot;

			if (slot == menu_slot || held)
			{
				fill(r, Theme::ACCENT, 13);
				fill(inset(r, 2, 2), down ? Theme::CARD_HI : Theme::CARD, 11);
			}
			else
			{
				fill(r, item ? (down ? Theme::CARD_HI : Theme::CARD) : Theme::CARD.alpha(0.75f), 13);
			}

			if (item)
			{
				MobileUI::icon(ItemData::get(item).get_icon(false), inset(r, 10, 10), held ? 0.35f : 1.0f);
			}
			else if (cosmetic && !pets && worn[i])
			{
				// The gear under an empty cosmetic slot, faintly.
				MobileUI::icon(ItemData::get(worn[i]).get_icon(false), inset(r, 14, 14), 0.22f);
			}
			else if (info.id == EquipSlot::Id::BOTTOM && overall)
			{
				text_in("Overall", Font::SMALL, Theme::FAINT, r);
			}
			else
			{
				text(label, Font::SMALL, Theme::FAINT, Point<int16_t>(static_cast<int16_t>(r.left() + r.width() / 2),
					static_cast<int16_t>(r.top() + (std::string(label).find(' ') != std::string::npos ? 13 : 20))), Text::Alignment::CENTER,
					static_cast<int16_t>(r.width() - 6));
			}

			// On the gear layer, a cosmetic is worn over this slot.
			if (!cosmetic && !pets && worn_cash[i])
				fill(rect(static_cast<int16_t>(r.right() - 13), static_cast<int16_t>(r.top() + 5), 8, 8), Theme::PURPLE, 4);
		}

		text(pets ? "Item Pouch fetches items, Meso Magnet fetches mesos."
			: cosmetic ? "Cosmetics show over your gear; stats come from the gear under them."
			: "Tap gear for stats, Unequip or Move. Purple dot: a cosmetic covers it.", Font::SMALL, Theme::FAINT,
			Point<int16_t>(static_cast<int16_t>(f.left() + W / 2), static_cast<int16_t>(f.top() + BOTTOM_Y + TILE + 14)), Text::Alignment::CENTER);
	}

	void UIMobileEquip::draw_pet_stage(const Rect& stage) const
	{
		Player& player = Stage::get().get_player();
		const PetLook& pet = player.get_pet(0);
		int16_t cx = static_cast<int16_t>(stage.left() + stage.width() / 2);

		if (pet.get_itemid() == 0)
		{
			text_in("No pet out", Font::BODY_BOLD, Theme::MUTED, rect(stage.left(), static_cast<int16_t>(stage.top() + 40), stage.width(), 24));
			text("Summon one from your Cash tab to use its gear.", Font::SMALL, Theme::FAINT, Point<int16_t>(cx, static_cast<int16_t>(stage.top() + 72)),
				Text::Alignment::CENTER, static_cast<int16_t>(stage.width() - 20));
		}
		else
		{
			Rect portrait = rect(static_cast<int16_t>(cx - 48), static_cast<int16_t>(stage.top() + 26), 96, 96);
			fill(portrait, Theme::TRACK.alpha(0.6f), 48);
			MobileUI::icon(ItemData::get(pet.get_itemid()).get_icon(false), inset(portrait, 18, 18));
			text(pet.get_name(), Font::TITLE, Theme::TEXT, Point<int16_t>(cx, static_cast<int16_t>(portrait.bottom() + 10)), Text::Alignment::CENTER);
		}

		// What the gear lets the pet do.
		struct Ability { const char* label; int16_t slot; };
		const Ability abilities[] = {
			{ "Picks up items", EquipSlot::Pet::ITEM_POUCH },
			{ "Picks up mesos", EquipSlot::Pet::MESO_MAGNET },
			{ "Uses HP potions", EquipSlot::Pet::AUTO_HP },
			{ "Uses MP potions", EquipSlot::Pet::AUTO_MP }
		};

		int16_t y = static_cast<int16_t>(stage.bottom() - 4 * 26 - 14);

		for (const Ability& a : abilities)
		{
			bool on = inventory.get_item_id(InventoryType::Id::EQUIPPED, a.slot) != 0;
			fill(rect(static_cast<int16_t>(stage.left() + 18), static_cast<int16_t>(y + 5), 10, 10), on ? Theme::GREEN : Theme::FAINT, 5);
			text(a.label, Font::SMALL_BOLD, on ? Theme::TEXT : Theme::FAINT, Point<int16_t>(static_cast<int16_t>(stage.left() + 36), y));
			text(on ? "On" : "Off", Font::SMALL_BOLD, on ? Theme::GREEN : Theme::FAINT,
				Point<int16_t>(static_cast<int16_t>(stage.right() - 16), y), Text::Alignment::RIGHT);
			y = static_cast<int16_t>(y + 26);
		}
	}
}
