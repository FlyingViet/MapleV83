//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../MobileUI.h"

#include "../../Character/Inventory/InventoryType.h"

#include <string>
#include <vector>

namespace ms
{
	// The iPhone's shop for every NPC shop and the menu's store: Buy (with each
	// item's details a tap away), Sell from any inventory tab, and recharging
	// of the stars and bullets in the Use tab. The menu's store is an ordinary
	// NPC shop on the server for the non-existent NPC 0 (@store).
	class UIMobileStore : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILESTORE;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		// The NPC the server names for the menu's store.
		static constexpr int32_t NPC = 0;

		struct Entry
		{
			int32_t item_id;
			int32_t price;
			bool rechargeable;
			// Mesos per star or bullet when recharging, and a full set.
			double unit_price;
			int16_t slot_max;
		};

		UIMobileStore();

		// The shop the server opened for npc; stock in the order it numbers it.
		void set_stock(int32_t npc, std::vector<Entry> entries);

		// Tells the server the shop closed once it no longer shows, however
		// it was closed (close button, the dimmed game, another panel).
		static void watch();

		// While a shop is open, the bag's items can be sold from their tap
		// card: what one (or the stack) fetches, or -1 if it can't be sold.
		static bool selling();
		static int32_t sell_price(InventoryType::Id tab, int16_t slot, bool everything);
		static bool sells_as_stack(InventoryType::Id tab, int16_t slot);
		static void sell_item(InventoryType::Id tab, int16_t slot, bool everything);

		void draw(float alpha) const override;
		void update() override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		enum class Tab { BUY, SELL };

		struct Recharge
		{
			int16_t slot;
			int32_t item_id;
			int16_t count;
			int16_t max;
			int32_t cost;
		};

		struct Sale
		{
			int16_t slot;
			int32_t item_id;
			int16_t count;
			// Mesos for one, and for the whole stack (stars sell as a stack).
			int32_t each;
			int32_t all;
			bool sellable;
			bool single;
		};

		struct Layout
		{
			struct Spot
			{
				MobileUI::Rect rect;
				int32_t target;
			};

			struct Row
			{
				MobileUI::Rect card;
				size_t index;
			};

			std::vector<Spot> spots;
			std::vector<Row> buys;
			std::vector<Row> recharges;
			std::vector<Row> sales;
			int16_t recharge_header = 0;
			int16_t height = 0;
		};

		std::vector<Recharge> recharges() const;
		std::vector<Sale> sales() const;
		static bool appraise(InventoryType::Id tab, int16_t slot, Sale& out);
		static void sell(InventoryType::Id tab, const Sale& sale, bool everything);
		Layout layout() const;
		const Entry* stock_of(int32_t item_id) const;
		int16_t list_top() const;
		int16_t buy_height(size_t index) const;

		int32_t npc = NPC;
		std::string title;
		std::vector<Entry> stock;
		Tab tab = Tab::BUY;
		InventoryType::Id sell_tab = InventoryType::Id::USE;
		// The stock entry showing its details, or -1.
		int32_t expanded = -1;

		int64_t last_meso = -1;
		int64_t meso_change = 0;
		int32_t change_ticks = 0;

		static bool open_on_server;
	};
}
