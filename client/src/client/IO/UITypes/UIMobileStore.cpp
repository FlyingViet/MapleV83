//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileStore.h"

#include "UIMobileConfirm.h"

#include "../ItemDetails.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Data/EquipData.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/NpcInteractionPackets.h"

#include <nlnx/nx.hpp>

#include <cmath>
#include <string>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 420;
		constexpr int16_t PAD = 14;
		constexpr int16_t TABS_Y = 72;
		constexpr int16_t TABS_H = 36;
		constexpr int16_t CHIPS_H = 34;
		constexpr int16_t SECTION_H = 30;
		constexpr int16_t BUY_H = 92;
		constexpr int16_t RECHARGE_H = 70;
		constexpr int16_t SELL_H = 70;
		constexpr int16_t GAP = 8;
		constexpr int16_t BUTTON_H = 32;

		constexpr int16_t QUANTITIES[] = { 1, 10, 100 };

		// Targets, checked from the highest range down.
		constexpr int32_t T_TAB = 10;			// + 0 buy, 1 sell
		constexpr int32_t T_CATEGORY = 20;		// + chip
		constexpr int32_t BUY = 1000;			// + index * 10 + quantity
		constexpr int32_t RECHARGE = 2000;		// + index
		constexpr int32_t RECHARGE_ALL = 3000;
		constexpr int32_t DETAILS = 4000;		// + index
		constexpr int32_t SELL = 6000;			// + slot * 4 + (0 one, 1 all)

		const InventoryType::Id CATEGORIES[] = {
			InventoryType::Id::EQUIP, InventoryType::Id::USE, InventoryType::Id::SETUP, InventoryType::Id::ETC
		};
		const char* const CATEGORY_NAMES[] = { "Equip", "Use", "Set-up", "Etc" };

		std::string grouped(int64_t value)
		{
			std::string digits = std::to_string(value < 0 ? -value : value);
			std::string out = value < 0 ? "-" : "";

			for (size_t i = 0; i < digits.size(); i++)
			{
				if (i > 0 && (digits.size() - i) % 3 == 0)
					out += ',';

				out += digits[i];
			}

			return out;
		}

		bool is_rechargeable(int32_t item_id)
		{
			int32_t prefix = item_id / 10000;

			return prefix == 207 || prefix == 233;
		}

		bool is_equip(int32_t item_id)
		{
			return item_id / 1000000 == 1;
		}

		double unit_price(int32_t item_id)
		{
			std::string prefix = "0" + std::to_string(item_id / 10000);
			std::string id = "0" + std::to_string(item_id);
			nl::node n = nl::nx::item["Consume"][prefix + ".img"][id]["info"]["unitPrice"];

			return n.data_type() == nl::node::type::real ? n.get_real() : static_cast<double>(n.get_integer());
		}

		// A short line under an item: what equipment asks of the wearer.
		std::string needs(int32_t item_id)
		{
			if (!is_equip(item_id))
				return "";

			int16_t level = EquipData::get(item_id).get_reqstat(MapleStat::Id::LEVEL);

			return level > 0 ? "Lv " + std::to_string(level) + "  \xC2\xB7  " + EquipData::get(item_id).get_type()
				: EquipData::get(item_id).get_type();
		}
	}

	bool UIMobileStore::open_on_server = false;

	UIMobileStore::UIMobileStore() : UIMobilePanel(Edge::LEFT, W), title("Store") {}

	void UIMobileStore::set_stock(int32_t npc_id, std::vector<Entry> entries)
	{
		if (npc_id != npc || stock.empty())
		{
			tab = Tab::BUY;
			expanded = -1;
			scroll = 0;
		}

		npc = npc_id;
		stock = std::move(entries);
		open_on_server = true;

		if (npc == NPC)
		{
			title = "Store";
		}
		else
		{
			std::string name = nl::nx::string["Npc.img"][std::to_string(npc)]["name"];
			title = name.empty() ? "Shop" : name;
		}

		clamp_scroll();
	}

	void UIMobileStore::watch()
	{
		if (!open_on_server)
			return;

		auto store = UI::get().get_element<UIMobileStore>();

		if (!store || !store->is_active())
		{
			open_on_server = false;
			NpcShopActionPacket().dispatch();
		}
	}

	void UIMobileStore::update()
	{
		UIMobilePanel::update();

		// What a purchase or sale did to the purse, briefly.
		int64_t meso = Stage::get().get_player().get_inventory().get_meso();

		if (last_meso >= 0 && meso != last_meso)
		{
			meso_change = change_ticks > 0 ? meso_change + (meso - last_meso) : meso - last_meso;
			change_ticks = 180;
		}
		else if (change_ticks > 0)
		{
			change_ticks--;
		}

		last_meso = meso;
	}

	// ---- data -------------------------------------------------------------

	const UIMobileStore::Entry* UIMobileStore::stock_of(int32_t item_id) const
	{
		for (const Entry& e : stock)
			if (e.item_id == item_id)
				return &e;

		return nullptr;
	}

	std::vector<UIMobileStore::Recharge> UIMobileStore::recharges() const
	{
		std::vector<Recharge> out;
		const Inventory& inventory = Stage::get().get_player().get_inventory();
		int16_t slots = inventory.get_slotmax(InventoryType::Id::USE);

		for (int16_t slot = 1; slot <= slots; slot++)
		{
			int32_t item_id = inventory.get_item_id(InventoryType::Id::USE, slot);

			if (item_id <= 0 || !is_rechargeable(item_id))
				continue;

			const Entry* entry = stock_of(item_id);

			if (!entry || entry->slot_max <= 0)
				continue;

			int16_t count = inventory.get_item_count(InventoryType::Id::USE, slot);
			int16_t missing = static_cast<int16_t>(std::max(0, entry->slot_max - count));
			int32_t cost = static_cast<int32_t>(std::ceil(entry->unit_price * missing));

			out.push_back({ slot, item_id, count, entry->slot_max, cost });
		}

		return out;
	}

	bool UIMobileStore::appraise(InventoryType::Id tab, int16_t slot, Sale& sale)
	{
		const Inventory& inventory = Stage::get().get_player().get_inventory();
		int32_t item_id = inventory.get_item_id(tab, slot);

		if (item_id <= 0)
			return false;

		const ItemData& data = ItemData::get(item_id);
		int16_t count = std::max<int16_t>(1, inventory.get_item_count(tab, slot));
		int32_t price = data.get_price();
		bool star = is_rechargeable(item_id);

		sale = {};
		sale.slot = slot;
		sale.item_id = item_id;
		sale.count = count;
		sale.single = star || is_equip(item_id) || count == 1;
		sale.each = price;
		sale.all = star ? static_cast<int32_t>(price + std::ceil(count * unit_price(item_id))) : price * count;
		// The server takes items it pays nothing for: those stay put.
		sale.sellable = price > 0 && !data.is_unsellable() && !data.is_cashitem();

		return true;
	}

	std::vector<UIMobileStore::Sale> UIMobileStore::sales() const
	{
		std::vector<Sale> out;
		const Inventory& inventory = Stage::get().get_player().get_inventory();
		int16_t slots = inventory.get_slotmax(sell_tab);

		for (int16_t slot = 1; slot <= slots; slot++)
		{
			Sale sale;

			if (appraise(sell_tab, slot, sale))
				out.push_back(sale);
		}

		return out;
	}

	bool UIMobileStore::selling()
	{
		auto store = UI::get().get_element<UIMobileStore>();

		return open_on_server && store && store->is_active();
	}

	int32_t UIMobileStore::sell_price(InventoryType::Id tab, int16_t slot, bool everything)
	{
		Sale sale;

		if (!appraise(tab, slot, sale) || !sale.sellable)
			return -1;

		return everything || sale.single ? sale.all : sale.each;
	}

	bool UIMobileStore::sells_as_stack(InventoryType::Id tab, int16_t slot)
	{
		Sale sale;

		return appraise(tab, slot, sale) && sale.sellable && !sale.single;
	}

	void UIMobileStore::sell_item(InventoryType::Id tab, int16_t slot, bool everything)
	{
		Sale sale;

		if (selling() && appraise(tab, slot, sale) && sale.sellable)
			sell(tab, sale, everything || sale.single);
	}

	// ---- layout -----------------------------------------------------------

	int16_t UIMobileStore::list_top() const
	{
		return static_cast<int16_t>(TABS_Y + TABS_H + 10 + (tab == Tab::SELL ? CHIPS_H + 10 : 0));
	}

	int16_t UIMobileStore::buy_height(size_t index) const
	{
		if (static_cast<int32_t>(index) != expanded)
			return BUY_H;

		ItemDetails details(stock[index].item_id);

		return static_cast<int16_t>(BUY_H + (details.empty() ? 0 : details.height(W - PAD * 2 - 24) + 10));
	}

	UIMobileStore::Layout UIMobileStore::layout() const
	{
		Layout out;
		Rect f = frame();
		int16_t x = static_cast<int16_t>(f.left() + PAD);
		int16_t w = W - PAD * 2;
		int16_t y = static_cast<int16_t>(f.top() + list_top() - scroll);
		int16_t start = y;

		if (tab == Tab::BUY)
		{
			y = static_cast<int16_t>(y + SECTION_H);
			bool sells_stars = false;

			for (size_t i = 0; i < stock.size(); i++)
			{
				const Entry& e = stock[i];
				sells_stars = sells_stars || e.rechargeable;

				if (e.price <= 0)
					continue;

				Rect card = rect(x, y, w, buy_height(i));
				int16_t by = static_cast<int16_t>(card.top() + BUY_H - 12 - BUTTON_H);

				if (e.rechargeable || is_equip(e.item_id))
				{
					out.spots.push_back({ rect(static_cast<int16_t>(card.right() - 12 - 128), by, 128, BUTTON_H),
						BUY + static_cast<int32_t>(i) * 10 });
				}
				else
				{
					int16_t bw = 64;

					for (int q = 2; q >= 0; q--)
					{
						int16_t bx = static_cast<int16_t>(card.right() - 12 - (3 - q) * bw - (2 - q) * 6);
						out.spots.push_back({ rect(bx, by, bw, BUTTON_H), BUY + static_cast<int32_t>(i) * 10 + q });
					}
				}

				// The rest of the card shows or hides the item's details.
				out.spots.push_back({ card, DETAILS + static_cast<int32_t>(i) });
				out.buys.push_back({ card, i });
				y = static_cast<int16_t>(y + card.height() + GAP);
			}

			// Recharging, where the shop deals in stars or bullets.
			if (sells_stars)
			{
				y = static_cast<int16_t>(y + 8);
				out.recharge_header = y;
				std::vector<Recharge> list = recharges();
				bool any = false;

				for (const Recharge& r : list)
					any = any || r.count < r.max;

				if (any)
					out.spots.push_back({ rect(static_cast<int16_t>(x + w - 156), static_cast<int16_t>(y - 2), 156, 28), RECHARGE_ALL });

				y = static_cast<int16_t>(y + SECTION_H);

				for (size_t i = 0; i < list.size(); i++)
				{
					Rect card = rect(x, y, w, RECHARGE_H);

					if (list[i].count < list[i].max)
						out.spots.push_back({ rect(static_cast<int16_t>(card.right() - 12 - 132),
							static_cast<int16_t>(card.top() + (RECHARGE_H - BUTTON_H) / 2), 132, BUTTON_H),
							RECHARGE + static_cast<int32_t>(i) });

					out.recharges.push_back({ card, i });
					y = static_cast<int16_t>(y + RECHARGE_H + GAP);
				}

				if (list.empty())
					y = static_cast<int16_t>(y + 44);
			}
		}
		else
		{
			std::vector<Sale> list = sales();

			for (size_t i = 0; i < list.size(); i++)
			{
				const Sale& s = list[i];
				Rect card = rect(x, y, w, SELL_H);
				int16_t by = static_cast<int16_t>(card.top() + (SELL_H - BUTTON_H) / 2);
				int32_t base = SELL + s.slot * 4;

				if (s.sellable)
				{
					if (s.single)
					{
						out.spots.push_back({ rect(static_cast<int16_t>(card.right() - 12 - 84), by, 84, BUTTON_H), base });
					}
					else
					{
						out.spots.push_back({ rect(static_cast<int16_t>(card.right() - 12 - 84), by, 84, BUTTON_H), base + 1 });
						out.spots.push_back({ rect(static_cast<int16_t>(card.right() - 12 - 84 - 6 - 62), by, 62, BUTTON_H), base });
					}
				}

				out.sales.push_back({ card, i });
				y = static_cast<int16_t>(y + SELL_H + GAP);
			}

			if (list.empty())
				y = static_cast<int16_t>(y + 44);
		}

		out.height = static_cast<int16_t>(y - start + 8);

		return out;
	}

	Rectangle<int16_t> UIMobileStore::scroll_area() const
	{
		Rect f = frame();
		int16_t top = static_cast<int16_t>(f.top() + list_top());

		return rect(f.left(), top, W, static_cast<int16_t>(f.bottom() - 10 - top));
	}

	int16_t UIMobileStore::scroll_content() const
	{
		return layout().height;
	}

	// ---- input ------------------------------------------------------------

	int32_t UIMobileStore::hit(Point<int16_t> pos) const
	{
		Rect f = frame();

		if (inset(panel_close(f), -6, -6).contains(pos))
			return CLOSE_TARGET;

		int16_t tab_w = static_cast<int16_t>((W - PAD * 2 - 6) / 2);

		for (int t = 0; t < 2; t++)
			if (rect(static_cast<int16_t>(f.left() + PAD + t * (tab_w + 6)), static_cast<int16_t>(f.top() + TABS_Y), tab_w, TABS_H).contains(pos))
				return T_TAB + t;

		if (tab == Tab::SELL)
		{
			int16_t chip_w = static_cast<int16_t>((W - PAD * 2 - 18) / 4);

			for (int c = 0; c < 4; c++)
				if (rect(static_cast<int16_t>(f.left() + PAD + c * (chip_w + 6)), static_cast<int16_t>(f.top() + TABS_Y + TABS_H + 10), chip_w, CHIPS_H).contains(pos))
					return T_CATEGORY + c;
		}

		if (!scroll_area().contains(pos))
			return NO_TARGET;

		for (const Layout::Spot& spot : layout().spots)
			if (inset(spot.rect, -2, -4).contains(pos))
				return spot.target;

		return NO_TARGET;
	}

	void UIMobileStore::sell(InventoryType::Id from, const Sale& sale, bool everything)
	{
		int16_t slot = sale.slot;
		int32_t item_id = sale.item_id;
		int16_t quantity = everything ? sale.count : 1;

		auto send = [from, slot, item_id, quantity]()
		{
			// Still the same item in that slot.
			if (Stage::get().get_player().get_inventory().get_item_id(from, slot) == item_id)
				NpcShopActionPacket(slot, item_id, quantity, false).dispatch();
		};

		// Equipment and whole stacks ask first; single items just go.
		if (is_equip(item_id) || (everything && quantity > 1))
		{
			std::string name = ItemData::get(item_id).get_name() + (quantity > 1 ? " \xC3\x97" + std::to_string(quantity) : "");
			int32_t price = everything ? sale.all : sale.each;
			UI::get().emplace<UIMobileConfirm>("Sell " + name + "?", "You'll get " + grouped(price) + " mesos.", "Sell", true, send);
			return;
		}

		Sound(Sound::Name::BUTTONCLICK).play();
		send();
	}

	void UIMobileStore::activate(int32_t target)
	{
		if (target >= SELL)
		{
			int16_t slot = static_cast<int16_t>((target - SELL) / 4);
			bool everything = (target - SELL) % 4 == 1;

			for (const Sale& s : sales())
				if (s.slot == slot && s.sellable)
					sell(sell_tab, s, everything);

			return;
		}

		if (target >= DETAILS)
		{
			int32_t i = target - DETAILS;
			Sound(Sound::Name::BUTTONCLICK).play();
			expanded = expanded == i ? -1 : i;
			clamp_scroll();
			return;
		}

		if (target >= RECHARGE_ALL)
		{
			for (const Recharge& r : recharges())
				if (r.count < r.max)
					NpcShopActionPacket(r.slot).dispatch();

			Sound(Sound::Name::BUTTONCLICK).play();
			return;
		}

		if (target >= RECHARGE)
		{
			std::vector<Recharge> list = recharges();
			size_t i = static_cast<size_t>(target - RECHARGE);

			if (i < list.size() && list[i].count < list[i].max)
			{
				NpcShopActionPacket(list[i].slot).dispatch();
				Sound(Sound::Name::BUTTONCLICK).play();
			}

			return;
		}

		if (target >= BUY)
		{
			size_t i = static_cast<size_t>((target - BUY) / 10);
			int q = (target - BUY) % 10;

			if (i >= stock.size())
				return;

			const Entry& e = stock[i];
			// A star set comes full whatever the count; the server fills it.
			int16_t quantity = e.rechargeable || is_equip(e.item_id) ? 1 : QUANTITIES[q];

			NpcShopActionPacket(static_cast<int16_t>(i), e.item_id, quantity, true).dispatch();
			Sound(Sound::Name::BUTTONCLICK).play();
			return;
		}

		if (target >= T_CATEGORY && target < T_CATEGORY + 4)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			sell_tab = CATEGORIES[target - T_CATEGORY];
			scroll = 0;
			return;
		}

		if (target == T_TAB || target == T_TAB + 1)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			Tab next = target == T_TAB ? Tab::BUY : Tab::SELL;

			if (next != tab)
			{
				tab = next;
				scroll = 0;
			}
		}
	}

	// ---- drawing ----------------------------------------------------------

	void UIMobileStore::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();
		const Inventory& inventory = Stage::get().get_player().get_inventory();
		int64_t meso = inventory.get_meso();

		panel(f, title, "Mesos  " + grouped(meso));
		Rect close = panel_close(f);
		icon_button(close, Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		// The last purchase or sale.
		if (change_ticks > 0 && meso_change != 0)
		{
			std::string change = (meso_change > 0 ? "+" : "") + grouped(meso_change);
			int16_t cw = static_cast<int16_t>(text_width(change, Font::SMALL_BOLD) + 20);
			Rect pill = rect(static_cast<int16_t>(close.left() - 10 - cw), static_cast<int16_t>(close.top() + 4), cw, 26);
			float fade = std::min(1.0f, change_ticks / 30.0f);
			RGBA tint = meso_change > 0 ? Theme::GREEN : Theme::GOLD;
			fill(pill, tint.alpha(0.18f * fade), 13);
			text_in(change, Font::SMALL_BOLD, tint.alpha(fade), pill);
		}

		int16_t tab_w = static_cast<int16_t>((W - PAD * 2 - 6) / 2);

		for (int t = 0; t < 2; t++)
		{
			bool on = (t == 0) == (tab == Tab::BUY);
			button(rect(static_cast<int16_t>(f.left() + PAD + t * (tab_w + 6)), static_cast<int16_t>(f.top() + TABS_Y), tab_w, TABS_H),
				t == 0 ? "Buy" : "Sell", on ? Style::SELECTED : Style::SECONDARY, true, pressed == T_TAB + t, Font::SMALL_BOLD);
		}

		if (tab == Tab::SELL)
		{
			int16_t chip_w = static_cast<int16_t>((W - PAD * 2 - 18) / 4);

			for (int c = 0; c < 4; c++)
			{
				Rect chip = rect(static_cast<int16_t>(f.left() + PAD + c * (chip_w + 6)), static_cast<int16_t>(f.top() + TABS_Y + TABS_H + 10), chip_w, CHIPS_H);
				bool on = CATEGORIES[c] == sell_tab;
				fill(chip, on ? Theme::ACCENT.alpha(0.22f) : (pressed == T_CATEGORY + c ? Theme::CARD_HI : Theme::CARD), 17);
				text_in(CATEGORY_NAMES[c], Font::SMALL_BOLD, on ? Theme::ACCENT : Theme::TEXT, chip);
			}
		}

		Rect area = scroll_area();
		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		Layout lay = layout();
		auto spot_of = [&](int32_t target) -> Rect
		{
			for (const Layout::Spot& s : lay.spots)
				if (s.target == target)
					return s.rect;

			return {};
		};

		int16_t x = static_cast<int16_t>(f.left() + PAD);
		int16_t w = W - PAD * 2;

		auto item_head = [&](const Rect& card, int32_t item_id, int16_t text_right, const std::string& line2, RGBA line2_tint)
		{
			const ItemData& data = ItemData::get(item_id);
			Rect slot = rect(static_cast<int16_t>(card.left() + 12), static_cast<int16_t>(card.top() + 12), 44, 44);
			fill(slot, Theme::TRACK, 10);
			MobileUI::icon(data.get_icon(false), inset(slot, 6, 6));

			int16_t tx = static_cast<int16_t>(slot.right() + 12);
			int16_t tw = static_cast<int16_t>(text_right - tx);
			text(ellipsize(data.get_name(), Font::BODY_BOLD, tw), Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(tx, static_cast<int16_t>(card.top() + 12)));
			text(ellipsize(line2, Font::SMALL, tw), Font::SMALL, line2_tint, Point<int16_t>(tx, static_cast<int16_t>(card.top() + 33)));
		};

		if (tab == Tab::BUY)
		{
			int16_t top = static_cast<int16_t>(f.top() + list_top() - scroll);
			text(npc == NPC ? "SUPPLIES" : "FOR SALE", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(x + 2), static_cast<int16_t>(top + 4)));

			for (const Layout::Row& row : lay.buys)
			{
				const Rect& card = row.card;

				if (card.bottom() < area.top() || card.top() > area.bottom())
					continue;

				const Entry& e = stock[row.index];
				bool open = static_cast<int32_t>(row.index) == expanded;
				fill(card, pressed == DETAILS + static_cast<int32_t>(row.index) || open ? Theme::CARD_HI : Theme::CARD, 14);

				std::string price = e.rechargeable
					? grouped(e.price) + " mesos for a set of " + std::to_string(e.slot_max)
					: grouped(e.price) + " mesos" + (is_equip(e.item_id) ? "" : " each");
				item_head(card, e.item_id, static_cast<int16_t>(card.right() - 12), price, Theme::GOLD);

				std::string sub = is_equip(e.item_id) ? needs(e.item_id) : "You have " + grouped(inventory.get_total_item_count(e.item_id));
				text(ellipsize(sub, Font::SMALL, 150), Font::SMALL, Theme::MUTED,
					Point<int16_t>(static_cast<int16_t>(card.left() + 12), static_cast<int16_t>(card.top() + BUY_H - 30)));

				if (e.rechargeable || is_equip(e.item_id))
				{
					int32_t target = BUY + static_cast<int32_t>(row.index) * 10;
					button(spot_of(target), e.rechargeable ? "Buy set" : "Buy", Style::PRIMARY, meso >= e.price, pressed == target, Font::SMALL_BOLD);
				}
				else
				{
					for (int q = 0; q < 3; q++)
					{
						int32_t target = BUY + static_cast<int32_t>(row.index) * 10 + q;
						bool afford = meso >= static_cast<int64_t>(e.price) * QUANTITIES[q];
						button(spot_of(target), "\xC3\x97" + std::to_string(QUANTITIES[q]), q == 0 ? Style::SECONDARY : Style::PRIMARY,
							afford, pressed == target, Font::SMALL_BOLD);
					}
				}

				if (open)
				{
					ItemDetails details(e.item_id);
					Rect body = rect(static_cast<int16_t>(card.left() + 12), static_cast<int16_t>(card.top() + BUY_H), static_cast<int16_t>(card.width() - 24), 0);
					fill(rect(body.left(), static_cast<int16_t>(body.top() - 4), body.width(), 1), Theme::LINE);
					details.draw(rect(body.left(), static_cast<int16_t>(body.top() + 6), body.width(), 0));
				}
			}

			if (lay.recharge_header > 0)
			{
				std::vector<Recharge> list = recharges();
				int32_t total = 0;
				bool any = false;

				for (const Recharge& r : list)
				{
					total += r.cost;
					any = any || r.count < r.max;
				}

				// Some starter stars recharge for nothing.
				auto cost = [](int32_t c) { return c > 0 ? grouped(c) : std::string("Free"); };

				text("RECHARGE", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(x + 2), static_cast<int16_t>(lay.recharge_header + 4)));

				if (any)
					button(spot_of(RECHARGE_ALL), "All \xC2\xB7 " + cost(total), Style::PRIMARY, meso >= total, pressed == RECHARGE_ALL, Font::SMALL_BOLD);

				for (const Layout::Row& row : lay.recharges)
				{
					const Recharge& r = list[row.index];
					const Rect& card = row.card;
					fill(card, Theme::CARD, 14);

					const ItemData& data = ItemData::get(r.item_id);
					Rect slot = rect(static_cast<int16_t>(card.left() + 12), static_cast<int16_t>(card.top() + 13), 44, 44);
					fill(slot, Theme::TRACK, 10);
					MobileUI::icon(data.get_icon(false), inset(slot, 6, 6));

					int16_t tx = static_cast<int16_t>(slot.right() + 12);
					int16_t tw = static_cast<int16_t>(card.right() - 12 - 132 - 8 - tx);
					text(ellipsize(data.get_name(), Font::BODY_BOLD, tw), Font::BODY_BOLD, Theme::TEXT,
						Point<int16_t>(tx, static_cast<int16_t>(card.top() + 14)));

					float fraction = r.max > 0 ? static_cast<float>(r.count) / r.max : 0.0f;
					MobileUI::bar(rect(tx, static_cast<int16_t>(card.top() + 38), tw, 14), fraction, Theme::BLUE);
					text(grouped(r.count) + " / " + grouped(r.max), Font::SMALL_BOLD, Theme::TEXT,
						Point<int16_t>(static_cast<int16_t>(tx + 6), static_cast<int16_t>(card.top() + 34)));

					int32_t target = RECHARGE + static_cast<int32_t>(row.index);

					if (r.count < r.max)
						button(spot_of(target), "Refill \xC2\xB7 " + cost(r.cost), Style::PRIMARY, meso >= r.cost, pressed == target, Font::SMALL_BOLD);
					else
						text_in("Full", Font::SMALL_BOLD, Theme::GREEN,
							rect(static_cast<int16_t>(card.right() - 12 - 132), static_cast<int16_t>(card.top() + (RECHARGE_H - BUTTON_H) / 2), 132, BUTTON_H));
				}

				if (list.empty())
					text("Throwing stars and bullets in your Use tab show here.", Font::SMALL, Theme::MUTED,
						Point<int16_t>(static_cast<int16_t>(x + 2), static_cast<int16_t>(lay.recharge_header + SECTION_H + 4)));
			}
		}
		else
		{
			std::vector<Sale> list = sales();

			for (const Layout::Row& row : lay.sales)
			{
				const Sale& s = list[row.index];
				const Rect& card = row.card;

				if (card.bottom() < area.top() || card.top() > area.bottom())
					continue;

				fill(card, Theme::CARD, 14);

				std::string worth = !s.sellable ? "Can't be sold here"
					: s.single ? grouped(s.all) + " mesos"
					: grouped(s.all) + " for all  \xC2\xB7  " + grouped(s.each) + " each";
				std::string count = s.count > 1 && !is_equip(s.item_id) ? "  \xC3\x97" + grouped(s.count) : "";
				int16_t right = static_cast<int16_t>(card.right() - 12 - (s.sellable ? (s.single ? 84 : 152) : 0) - 8);

				const ItemData& data = ItemData::get(s.item_id);
				Rect slot = rect(static_cast<int16_t>(card.left() + 12), static_cast<int16_t>(card.top() + 13), 44, 44);
				fill(slot, Theme::TRACK, 10);
				MobileUI::icon(data.get_icon(false), inset(slot, 6, 6));

				int16_t tx = static_cast<int16_t>(slot.right() + 12);
				int16_t tw = static_cast<int16_t>(right - tx);
				text(ellipsize(data.get_name() + count, Font::BODY_BOLD, tw), Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(tx, static_cast<int16_t>(card.top() + 14)));
				text(ellipsize(worth, Font::SMALL, tw), Font::SMALL, s.sellable ? Theme::GOLD : Theme::FAINT, Point<int16_t>(tx, static_cast<int16_t>(card.top() + 36)));

				int32_t base = SELL + s.slot * 4;

				if (s.sellable && s.single)
				{
					button(spot_of(base), "Sell", Style::PRIMARY, true, pressed == base, Font::SMALL_BOLD);
				}
				else if (s.sellable)
				{
					button(spot_of(base), "Sell 1", Style::SECONDARY, true, pressed == base, Font::SMALL_BOLD);
					button(spot_of(base + 1), "Sell all", Style::PRIMARY, true, pressed == base + 1, Font::SMALL_BOLD);
				}
			}

			if (list.empty())
				text("Nothing in this tab to sell.", Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(x + 2),
					static_cast<int16_t>(f.top() + list_top() - scroll + 8)));

			(void)w;
		}

		GraphicsGL::get().end_clip();
	}

	UIElement::Type UIMobileStore::get_type() const
	{
		return TYPE;
	}
}
