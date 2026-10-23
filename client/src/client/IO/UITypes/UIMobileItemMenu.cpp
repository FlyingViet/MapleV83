//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileItemMenu.h"

#include "UIMobileEquip.h"
#include "UIMobileInventory.h"
#include "UIMobileStore.h"

#include "../ItemActions.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Data/EquipData.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Gameplay/Stage.h"

#include <algorithm>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 216;
		constexpr int16_t W_DETAILS = 310;
		constexpr int16_t PAD = 10;
		constexpr int16_t HEADER = 54;
		constexpr int16_t ROW_H = 42;
		constexpr int16_t GAP = 6;
		// Distance from the tapped slot when the menu floats beside it
		constexpr int16_t OFFSET = 30;

		std::string mesos(int32_t value)
		{
			std::string digits = std::to_string(std::max(0, value));
			std::string out;

			for (size_t i = 0; i < digits.size(); i++)
			{
				if (i > 0 && (digits.size() - i) % 3 == 0)
					out += ',';

				out += digits[i];
			}

			return out + " mesos";
		}

		const char* tab_label(InventoryType::Id tab)
		{
			switch (tab)
			{
				case InventoryType::Id::EQUIP: return "Equipment";
				case InventoryType::Id::USE: return "Use";
				case InventoryType::Id::SETUP: return "Set-up";
				case InventoryType::Id::ETC: return "Etc";
				case InventoryType::Id::CASH: return "Cash";
				case InventoryType::Id::EQUIPPED: return "Equipped";
				default: return "";
			}
		}
	}

	UIMobileItemMenu::UIMobileItemMenu(Source src, InventoryType::Id t, int16_t s, int32_t id, Point<int16_t> anchor) :
		UIElement(Point<int16_t>(0, 0), Point<int16_t>(W, 0)), source(src), tab(t), slot(s), item_id(id)
	{
		const ItemData& data = ItemData::get(item_id);
		name = data.get_name();
		item_icon = data.get_icon(false);

		is_equip = source == Source::EQUIPPED || tab == InventoryType::Id::EQUIP;

		if (source == Source::EQUIPPED)
		{
			primary = "Unequip";
		}
		else
		{
			int32_t category = item_id / 10000;

			switch (tab)
			{
				case InventoryType::Id::EQUIP:
					primary = "Equip";
					break;
				case InventoryType::Id::USE:
					// Scrolls go onto worn gear: pick them up and tap the item.
					if (category == 204)
					{
						primary = "Apply to gear";
						primary_moves = true;
					}
					else
					{
						primary = "Use";
					}
					break;
				case InventoryType::Id::SETUP:
					primary = category == 301 ? "Sit" : "Use";
					break;
				case InventoryType::Id::CASH:
					// Salon coupons open their salon.
					primary = category == 515 ? "Redeem" : "Use";
					break;
				default:
					break;
			}
		}

		const Inventory& inventory = Stage::get().get_player().get_inventory();
		int16_t count = source == Source::INVENTORY ? inventory.get_item_count(tab, slot) : 1;

		detail = source == Source::EQUIPPED && EquipSlot::is_pet(slot) ? "Pet gear"
			: EquipSlot::pet_slot(item_id) ? "Pet gear"
			: source == Source::EQUIPPED && slot > EquipSlot::CASH_OFFSET ? "Cosmetic" : tab_label(tab);

		if (count > 1)
			detail += "  \xC2\xB7  x" + std::to_string(count);

		// A shop is open: selling comes first.
		if (source == Source::INVENTORY && UIMobileStore::selling() && UIMobileStore::sell_price(tab, slot, false) >= 0)
		{
			actions.push_back(SELL);

			if (UIMobileStore::sells_as_stack(tab, slot))
				actions.push_back(SELL_ALL);
		}

		if (!primary.empty())
			actions.push_back(PRIMARY);

		actions.push_back(MOVE);

		// What it needs and gives, compared with what is worn in its slot.
		if (is_equip)
		{
			auto equip = inventory.get_equip(source == Source::EQUIPPED ? InventoryType::Id::EQUIPPED : InventoryType::Id::EQUIP, slot);

			if (equip)
			{
				const Equip* worn = nullptr;
				Optional<const Equip> current;

				if (source == Source::INVENTORY)
				{
					current = inventory.get_equip(InventoryType::Id::EQUIPPED, EquipData::get(item_id).get_eqslot());
					worn = current ? current.get() : nullptr;
				}

				details.emplace(*equip, worn);

				if (equip->get_level() > 0)
					name += " (+" + std::to_string(equip->get_level()) + ")";
			}
		}
		else
		{
			details.emplace(item_id);
		}

		int16_t width = details && !details->empty() ? W_DETAILS : W;

		if (details && !details->empty())
			details_h = static_cast<int16_t>(details->height(static_cast<int16_t>(width - PAD * 2 - 4)) + 12);

		int16_t vh_limit = static_cast<int16_t>(Constants::Constants::get().get_viewheight() - 12);
		int16_t actions_h = static_cast<int16_t>(actions.size() * ROW_H + (actions.size() - 1) * GAP);
		details_h = std::min<int16_t>(details_h, static_cast<int16_t>(vh_limit - PAD * 2 - HEADER - actions_h - GAP));

		// Beside the tapped slot, on the side away from its panel.
		int16_t height = static_cast<int16_t>(PAD + HEADER + details_h + actions_h + PAD);
		dimension = Point<int16_t>(width, height);
		const int16_t W = width;

		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		Range<int16_t> free = UI::get().free_span();
		int16_t x;

		if (free.second() > free.first() && anchor.x() >= free.second())
		{
			x = static_cast<int16_t>(free.second() - 10 - W);
			on_left = true;
		}
		else if (free.second() > free.first() && anchor.x() < free.first())
		{
			x = static_cast<int16_t>(free.first() + 10);
			on_left = false;
		}
		else
		{
			x = static_cast<int16_t>(anchor.x() - OFFSET - W);
			on_left = true;

			if (x < 6)
			{
				x = static_cast<int16_t>(anchor.x() + OFFSET);
				on_left = false;
			}
		}

		x = std::clamp<int16_t>(x, 6, static_cast<int16_t>(std::max(6, vw - W - 6)));
		int16_t y = std::clamp<int16_t>(static_cast<int16_t>(anchor.y() - height / 2), 6,
			static_cast<int16_t>(std::max(6, vh - height - 6)));

		position = Point<int16_t>(x, y);
	}

	Rect UIMobileItemMenu::card() const
	{
		return rect(position.x(), position.y(), dimension.x(), dimension.y());
	}

	Rect UIMobileItemMenu::action_rect(size_t index) const
	{
		return rect(static_cast<int16_t>(position.x() + PAD),
			static_cast<int16_t>(position.y() + PAD + HEADER + details_h + index * (ROW_H + GAP)),
			static_cast<int16_t>(dimension.x() - PAD * 2), ROW_H);
	}

	void UIMobileItemMenu::draw(float) const
	{
		Rect c = card();

		// A soft shadow lifts the card off the panel beneath it.
		fill(rect(static_cast<int16_t>(c.left() - 2), static_cast<int16_t>(c.top() + 3), static_cast<int16_t>(c.width() + 4),
			static_cast<int16_t>(c.height() + 4)), RGBA{ 0.0f, 0.0f, 0.0f, 0.35f }, 18);
		fill(c, Theme::PANEL, 16);

		Rect well = rect(static_cast<int16_t>(c.left() + PAD), static_cast<int16_t>(c.top() + PAD), 44, 44);
		fill(well, Theme::CARD, 10);
		icon(item_icon, inset(well, 6, 6));

		int16_t text_x = static_cast<int16_t>(well.right() + 10);
		int16_t text_w = static_cast<int16_t>(c.right() - PAD - text_x);
		text(ellipsize(name, Font::BODY_BOLD, text_w), Font::BODY_BOLD, Theme::TEXT,
			Point<int16_t>(text_x, static_cast<int16_t>(well.top() + 4)));
		text(detail, Font::SMALL, Theme::MUTED, Point<int16_t>(text_x, static_cast<int16_t>(well.top() + 24)));

		if (details && details_h > 0)
		{
			int16_t top = static_cast<int16_t>(c.top() + PAD + HEADER + 2);
			GraphicsGL::get().begin_clip(top, static_cast<int16_t>(top + details_h - 8));
			details->draw(rect(static_cast<int16_t>(c.left() + PAD + 2), top, static_cast<int16_t>(c.width() - PAD * 2 - 4), 0));
			GraphicsGL::get().end_clip();
		}

		for (size_t i = 0; i < actions.size(); i++)
		{
			Action a = actions[i];
			Rect r = action_rect(i);
			bool down = pressed == a;

			switch (a)
			{
				case PRIMARY:
					button(r, primary, Style::PRIMARY, true, down);
					break;
				case MOVE:
					button(r, "Move", Style::SECONDARY, true, down);
					break;
				case SELL:
				case SELL_ALL:
				{
					int32_t price = UIMobileStore::sell_price(tab, slot, a == SELL_ALL);
					std::string what = a == SELL_ALL ? "Sell all" : UIMobileStore::sells_as_stack(tab, slot) ? "Sell 1" : "Sell";
					button(r, what + "  \xC2\xB7  " + mesos(price), a == SELL_ALL ? Style::PRIMARY : Style::SECONDARY, price >= 0, down);
					break;
				}
				default:
					break;
			}
		}
	}

	void UIMobileItemMenu::update()
	{
		if (pressed != NONE && !UI::get().is_mouse_held())
			pressed = NONE;

		// The item was used up, moved or its window closed.
		if (!still_there())
			deactivate();
	}

	bool UIMobileItemMenu::is_in_range(Point<int16_t> pos) const
	{
		return card().contains(pos);
	}

	UIMobileItemMenu::Action UIMobileItemMenu::hit(Point<int16_t> pos) const
	{
		for (size_t i = 0; i < actions.size(); i++)
			if (action_rect(i).contains(pos))
				return actions[i];

		return NONE;
	}

	Cursor::State UIMobileItemMenu::send_cursor(bool down, Point<int16_t> pos)
	{
		if (down)
		{
			uint32_t serial = UI::get().get_press_serial();

			if (serial != press)
			{
				press = serial;
				pressed = hit(pos);
			}

			return Cursor::State::CLICKING;
		}

		Action a = pressed;
		pressed = NONE;

		if (a != NONE && hit(pos) == a)
			run(a);

		return Cursor::State::IDLE;
	}

	void UIMobileItemMenu::send_key(int32_t, bool pressed_key, bool escape)
	{
		if (pressed_key && escape)
			close();
	}

	int16_t UIMobileItemMenu::slot_for(Source src, InventoryType::Id t) const
	{
		return src == source && t == tab ? slot : 0;
	}

	bool UIMobileItemMenu::still_there() const
	{
		const Inventory& inventory = Stage::get().get_player().get_inventory();

		if (source == Source::EQUIPPED)
		{
			auto equips = UI::get().get_element<UIMobileEquip>();

			return equips && equips->is_active()
				&& inventory.get_item_id(InventoryType::Id::EQUIPPED, slot) == item_id;
		}

		auto items = UI::get().get_element<UIMobileInventory>();

		return items && items->is_active() && items->get_tab() == tab
			&& inventory.get_item_id(tab, slot) == item_id;
	}

	void UIMobileItemMenu::run(Action a)
	{
		if (!still_there())
		{
			close();
			return;
		}

		if (a == SELL || a == SELL_ALL)
		{
			InventoryType::Id t = tab;
			int16_t s = slot;
			deactivate();
			UIMobileStore::sell_item(t, s, a == SELL_ALL);
			return;
		}

		bool move = a == MOVE || (a == PRIMARY && primary_moves);

		// Close first: the action may open a notice or pick the item up.
		deactivate();

		if (source == Source::EQUIPPED)
		{
			EquipSlot::Id eqslot = static_cast<EquipSlot::Id>(slot);

			if (!move)
				ItemActions::unequip(eqslot);
			else if (auto equips = UI::get().get_element<UIMobileEquip>())
				equips->pick_up(eqslot);
		}
		else
		{
			if (!move)
				ItemActions::use(tab, slot);
			else if (auto items = UI::get().get_element<UIMobileInventory>())
				items->pick_up(slot);
		}
	}

	void UIMobileItemMenu::close()
	{
		Sound(Sound::Name::MENUDOWN).play();
		deactivate();
	}

	UIElement::Type UIMobileItemMenu::get_type() const
	{
		return TYPE;
	}
}
