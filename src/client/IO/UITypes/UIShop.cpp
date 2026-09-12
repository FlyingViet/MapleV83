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
#include "UIShop.h"

#include "UINotice.h"
#include "UIStatusMessenger.h"

#include "../UI.h"

#include "../Components/AreaButton.h"
#include "../Components/Charset.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"

#include "../../Graphics/Geometry.h"

#include "../../Constants.h"

#include "../../Audio/Audio.h"
#include "../../Data/ItemData.h"

#include "../../Net/Packets/NpcInteractionPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIShop::UIShop(const CharLook& in_charlook, const Inventory& in_inventory) : UIDragElement<PosSHOP>(Point<int16_t>(463, 20)), charlook(in_charlook), inventory(in_inventory)
	{
		// v83: everything lives flat under UIWindow.img/Shop
		nl::node src = nl::nx::ui["UIWindow.img"]["Shop"];

		nl::node background = src["backgrnd"];
		Texture bg = background;

		auto bg_dimensions = bg.get_dimensions();

		sprites.emplace_back(background);

		// Header buttons. Each header box (y 7..82) has a portrait on the
		// left of a faint divider (x 105 / 335) and free space to its right:
		//   left  : BtBuy (80x18) + BtRecharge (30x16) side by side
		//   right : BtSell / BtExit stacked above the meso box (y 63..80)
		buttons[Buttons::BUY_ITEM] = std::make_unique<MapleButton>(src["BtBuy"], Point<int16_t>(112, 32));
		buttons[Buttons::RECHARGE] = std::make_unique<MapleButton>(src["BtRecharge"], Point<int16_t>(194, 33));
		buttons[Buttons::SELL_ITEM] = std::make_unique<MapleButton>(src["BtSell"], Point<int16_t>(342, 24));
		buttons[Buttons::EXIT] = std::make_unique<MapleButton>(src["BtExit"], Point<int16_t>(342, 43));

		buttons[Buttons::RECHARGE]->set_state(Button::State::DISABLED);

		// Buy-side "All" tab (TabBuy/0). The per-category buy tabs are created
		// lazily in finalize_buy_tabs() once the shop's item list is known,
		// so only categories that actually appear in the shop get a tab.
		nl::node buyen = src["TabBuy"]["enabled"];
		nl::node buydis = src["TabBuy"]["disabled"];

		buttons[Buttons::OVERALL] = std::make_unique<TwoSpriteButton>(buydis["0"], buyen["0"], Point<int16_t>(BUY_TAB_X, TAB_Y));

		// Inventory tabs on the right panel (TabSell: Equip, Use, Set-up, Etc, Cash)
		nl::node sellen = src["TabSell"]["enabled"];
		nl::node selldis = src["TabSell"]["disabled"];

		{
			int16_t tab_x = SELL_X + 8;
			const InventoryType::Id order[5] = {
				InventoryType::Id::EQUIP,
				InventoryType::Id::USE,
				InventoryType::Id::SETUP,
				InventoryType::Id::ETC,
				InventoryType::Id::CASH
			};

			for (InventoryType::Id t : order)
			{
				std::string tabnum = std::to_string(tab_art_index(t));
				buttons[tabbyinventory(t)] = std::make_unique<TwoSpriteButton>(selldis[tabnum], sellen[tabnum], Point<int16_t>(tab_x, TAB_Y));
				tab_x += Texture(selldis[tabnum]).get_dimensions().x() + TAB_GAP;
			}
		}

		buy_x = BUY_X;
		buy_width = ROW_W;

		for (uint16_t i = Buttons::BUY0; i <= Buttons::BUY4; i++)
		{
			Point<int16_t> pos(BUY_X, ROW_Y + ROW_PITCH * (i - Buttons::BUY0) + ROW_ART_Y);
			Point<int16_t> dim(ROW_W, ROW_ART_H);
			buttons[i] = std::make_unique<AreaButton>(pos, dim);
		}

		sell_x = SELL_X;
		sell_width = ROW_W;

		for (uint16_t i = Buttons::SELL0; i <= Buttons::SELL4; i++)
		{
			Point<int16_t> pos(SELL_X, ROW_Y + ROW_PITCH * (i - Buttons::SELL0) + ROW_ART_Y);
			Point<int16_t> dim(ROW_W, ROW_ART_H);
			buttons[i] = std::make_unique<AreaButton>(pos, dim);
		}

		// One 162x35 highlight serves both panels (it matches the name strip)
		buy_selection = src["select"];
		sell_selection = src["select"];
		meso = src["meso"];

		mesolabel = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::MINESHAFT);

		// v83 Basic.img only ships the plain "VScr" scrollbar, which is
		// Slider::Type::LINE_CYAN; every other type would resolve to a
		// missing "VScrN" node.
		buyslider = Slider(
			Slider::Type::LINE_CYAN, Range<int16_t>(SLIDER_TOP, SLIDER_BOTTOM), BUY_SLIDER_X, ROWS, 1,
			[&](bool upwards)
			{
				int16_t shift = upwards ? -1 : 1;
				bool above = buystate.offset + shift >= 0;
				bool below = buystate.offset + shift <= buystate.lastslot - ROWS;

				if (above && below)
					buystate.offset += shift;
			}
		);

		sellslider = Slider(
			Slider::Type::LINE_CYAN, Range<int16_t>(SLIDER_TOP, SLIDER_BOTTOM), SELL_SLIDER_X, ROWS, 1,
			[&](bool upwards)
			{
				int16_t shift = upwards ? -1 : 1;
				bool above = sellstate.offset + shift >= 0;
				bool below = sellstate.offset + shift <= sellstate.lastslot - ROWS;

				if (above && below)
					sellstate.offset += shift;
			}
		);

		rightclicksell = Configuration::get().get_rightclicksell();

		active = false;
		dimension = bg_dimensions;
		dragarea = Point<int16_t>(bg_dimensions.x(), 10);
	}

	void UIShop::draw(float alpha) const
	{
		UIElement::draw(alpha);

		npc.draw(DrawArgument(position + NPC_POS, true), alpha);

		// Flipped=false → faces the NPC on the left (the default
		// CharLook facing is right, so false mirrors it leftward).
		charlook.draw(position + CHAR_POS, /*flipped*/ false,
			Stance::Id::STAND1, Expression::Id::DEFAULT);

		mesolabel.draw(position + MESO_TEXT_POS);

		buystate.draw(position, buy_selection);
		sellstate.draw(position, sell_selection);

		buyslider.draw(position);
		sellslider.draw(position);
	}

	void UIShop::update()
	{
		npc.update();

		if (currency_item > 0)
		{
			InventoryType::Id ctype = InventoryType::by_item_id(currency_item);
			int32_t total = 0;

			for (int16_t i = 1; i <= inventory.get_slotmax(ctype); i++)
				if (inventory.get_item_id(ctype, i) == currency_item)
					total += inventory.get_item_count(ctype, i);

			std::string cstr = std::to_string(total);
			string_format::split_number(cstr);
			mesolabel.change_text(cstr + " " + currency_name);
		}
		else
		{
			int64_t num_mesos = inventory.get_meso();
			std::string mesostr = std::to_string(num_mesos);
			string_format::split_number(mesostr);
			mesolabel.change_text(mesostr);
		}

		// Recharge is only meaningful for a selected throwing star / bullet
		// stack on the USE tab; grey the button otherwise.
		bool can_recharge = false;

		if (sellstate.tab == InventoryType::Id::USE
			&& sellstate.selection >= 0 && sellstate.selection < sellstate.lastslot)
		{
			int32_t prefix = sellstate.items[sellstate.selection].get_id() / 10000;
			can_recharge = (prefix == 207 || prefix == 233);
		}

		Button::State rstate = buttons[Buttons::RECHARGE]->get_state();

		if (!can_recharge && rstate != Button::State::DISABLED)
			buttons[Buttons::RECHARGE]->set_state(Button::State::DISABLED);
		else if (can_recharge && rstate == Button::State::DISABLED)
			buttons[Buttons::RECHARGE]->set_state(Button::State::NORMAL);
	}

	Button::State UIShop::button_pressed(uint16_t buttonid)
	{
		clear_tooltip();

		constexpr Range<uint16_t> buy(Buttons::BUY0, Buttons::BUY4);
		constexpr Range<uint16_t> sell(Buttons::SELL0, Buttons::SELL4);

		if (buy.contains(buttonid))
		{
			int16_t selected = buttonid - Buttons::BUY0;
			buystate.select(selected);
			sellstate.selection = -1;

			return Button::State::NORMAL;
		}
		else if (sell.contains(buttonid))
		{
			int16_t selected = buttonid - Buttons::SELL0;
			sellstate.select(selected);
			buystate.selection = -1;

			return Button::State::NORMAL;
		}
		else
		{
			switch (buttonid)
			{
			case Buttons::BUY_ITEM:
				buystate.buy();

				return Button::State::NORMAL;
			case Buttons::SELL_ITEM:
				sellstate.sell(false);

				return Button::State::NORMAL;
			case Buttons::RECHARGE:
				sellstate.recharge();

				return Button::State::NORMAL;
			case Buttons::EXIT:
				exit_shop();

				return Button::State::PRESSED;
			case Buttons::EQUIP:
				changeselltab(InventoryType::Id::EQUIP);

				return Button::State::IDENTITY;
			case Buttons::USE:
				changeselltab(InventoryType::Id::USE);

				return Button::State::IDENTITY;
			case Buttons::ETC:
				changeselltab(InventoryType::Id::ETC);

				return Button::State::IDENTITY;
			case Buttons::SETUP:
				changeselltab(InventoryType::Id::SETUP);

				return Button::State::IDENTITY;
			case Buttons::CASH:
				changeselltab(InventoryType::Id::CASH);

				return Button::State::IDENTITY;

			case Buttons::OVERALL:
				changebuytab(InventoryType::Id::NONE);

				return Button::State::IDENTITY;
			case Buttons::BUY_TAB_EQUIP:
				changebuytab(InventoryType::Id::EQUIP);

				return Button::State::IDENTITY;
			case Buttons::BUY_TAB_USE:
				changebuytab(InventoryType::Id::USE);

				return Button::State::IDENTITY;
			case Buttons::BUY_TAB_ETC:
				changebuytab(InventoryType::Id::ETC);

				return Button::State::IDENTITY;
			case Buttons::BUY_TAB_SETUP:
				changebuytab(InventoryType::Id::SETUP);

				return Button::State::IDENTITY;
			case Buttons::BUY_TAB_CASH:
				changebuytab(InventoryType::Id::CASH);

				return Button::State::IDENTITY;
			}
		}

		return Button::State::PRESSED;
	}

	void UIShop::remove_cursor()
	{
		UIDragElement::remove_cursor();

		buyslider.remove_cursor();
		sellslider.remove_cursor();
	}

	Cursor::State UIShop::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Point<int16_t> cursoroffset = cursorpos - position;
		lastcursorpos = cursoroffset;

		if (buyslider.isenabled())
		{
			Cursor::State bstate = buyslider.send_cursor(cursoroffset, clicked);

			if (bstate != Cursor::State::IDLE)
			{
				clear_tooltip();

				return bstate;
			}
		}

		if (sellslider.isenabled())
		{
			Cursor::State sstate = sellslider.send_cursor(cursoroffset, clicked);

			if (sstate != Cursor::State::IDLE)
			{
				clear_tooltip();

				return sstate;
			}
		}

		int16_t xoff = cursoroffset.x();
		int16_t yoff = cursoroffset.y();
		int16_t slot = slot_by_position(yoff);

		if (slot >= 0 && slot < ROWS)
		{
			if (xoff >= buy_x && xoff <= buy_x + buy_width)
				show_item(slot, true);
			else if (xoff >= sell_x && xoff <= sell_x + sell_width)
				show_item(slot, false);
			else
				clear_tooltip();
		}
		else
		{
			clear_tooltip();
		}

		Cursor::State ret = clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		for (size_t i = 0; i < Buttons::NUM_BUTTONS; i++)
		{
			// Some button slots (e.g. buy-tab categories that don't exist
			// in the current shop) are intentionally missing from the map.
			auto bit = buttons.find(static_cast<uint16_t>(i));
			if (bit == buttons.end() || !bit->second)
				continue;

			bool is_action = i <= Buttons::RECHARGE;
			bool is_tab = i >= Buttons::OVERALL && i <= Buttons::BUY_TAB_CASH;

			if (buttons[i]->is_active() && buttons[i]->bounds(position).contains(cursorpos))
			{
				if (buttons[i]->get_state() == Button::State::NORMAL)
				{
					if (is_action)
					{
						Sound(Sound::Name::BUTTONOVER).play();

						buttons[i]->set_state(Button::State::MOUSEOVER);
						ret = Cursor::State::CANCLICK;
					}
					else
					{
						buttons[i]->set_state(Button::State::MOUSEOVER);
						ret = Cursor::State::IDLE;
					}
				}
				else if (buttons[i]->get_state() == Button::State::MOUSEOVER)
				{
					if (clicked)
					{
						if (is_tab)
							Sound(Sound::Name::TAB).play();
						else if (is_action)
							Sound(Sound::Name::BUTTONCLICK).play();

						buttons[i]->set_state(button_pressed(i));

						ret = Cursor::State::IDLE;
					}
					else
					{
						if (is_action)
							ret = Cursor::State::CANCLICK;
						else
							ret = Cursor::State::IDLE;
					}
				}
				else if (buttons[i]->get_state() == Button::State::PRESSED)
				{
					if (clicked)
					{
						if (is_tab)
						{
							Sound(Sound::Name::TAB).play();

							ret = Cursor::State::IDLE;
						}
					}
				}
			}
			else if (buttons[i] && buttons[i]->get_state() == Button::State::MOUSEOVER)
			{
				buttons[i]->set_state(Button::State::NORMAL);
			}
		}

		return ret;
	}

	void UIShop::send_scroll(double yoffset)
	{
		int16_t xoff = lastcursorpos.x();
		// Rows plus the scrollbar track to their right
		int16_t slider_width = 24;

		if (buyslider.isenabled())
			if (xoff >= buy_x && xoff <= buy_x + buy_width + slider_width)
				buyslider.send_scroll(yoffset);

		if (sellslider.isenabled())
			if (xoff >= sell_x && xoff <= sell_x + sell_width + slider_width)
				sellslider.send_scroll(yoffset);
	}

	void UIShop::rightclick(Point<int16_t> cursorpos)
	{
		if (rightclicksell)
		{
			Point<int16_t> cursoroffset = cursorpos - position;

			int16_t xoff = cursoroffset.x();
			int16_t yoff = cursoroffset.y();
			int16_t slot = slot_by_position(yoff);

			if (slot >= 0 && slot < ROWS)
			{
				if (xoff >= sell_x && xoff <= sell_x + sell_width)
				{
					clear_tooltip();

					sellstate.selection = slot + sellstate.offset;
					sellstate.sell(true);
					buystate.selection = -1;
				}
			}
		}
	}

	void UIShop::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			exit_shop();
	}

	UIElement::Type UIShop::get_type() const
	{
		return TYPE;
	}

	void UIShop::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::Parent::SHOP);
	}

	void UIShop::show_item(int16_t slot, bool buy)
	{
		if (buy)
			buystate.show_item(slot);
		else
			sellstate.show_item(slot);
	}

	void UIShop::finalize_buy_tabs()
	{
		// Remove any tabs left over from a previous shop session. Use
		// erase (not operator[].reset()) so the map never holds null
		// entries — reset() iterates `buttons` and dereferences each.
		for (uint16_t i = Buttons::BUY_TAB_EQUIP; i <= Buttons::BUY_TAB_CASH; i++)
			buttons.erase(i);

		// Which categories does this shop actually carry?
		bool present[6] = { false, false, false, false, false, false };
		for (const auto& item : buystate.all_items)
		{
			InventoryType::Id t = InventoryType::by_item_id(item.get_id());
			if (t >= InventoryType::Id::EQUIP && t <= InventoryType::Id::CASH)
				present[static_cast<size_t>(t)] = true;
		}

		// v83 has no dedicated buy-side category art, so the buy tabs reuse
		// the TabSell text bitmaps (same Equip / Use / Set-up / Etc / Cash
		// labels the right panel shows), packed right of the "All" tab.
		nl::node src = nl::nx::ui["UIWindow.img"]["Shop"];
		nl::node sellen = src["TabSell"]["enabled"];
		nl::node selldis = src["TabSell"]["disabled"];

		int16_t tab_x = BUY_TAB_X + Texture(src["TabBuy"]["disabled"]["0"]).get_dimensions().x() + TAB_GAP;

		const InventoryType::Id order[5] = {
			InventoryType::Id::EQUIP,
			InventoryType::Id::USE,
			InventoryType::Id::SETUP,
			InventoryType::Id::ETC,
			InventoryType::Id::CASH
		};

		for (InventoryType::Id t : order)
		{
			if (!present[static_cast<size_t>(t)]) continue;

			std::string tabnum = std::to_string(tab_art_index(t));
			uint16_t btn_id = buy_tab_by_inventory(t);

			buttons[btn_id] = std::make_unique<TwoSpriteButton>(selldis[tabnum], sellen[tabnum], Point<int16_t>(tab_x, TAB_Y));
			tab_x += Texture(selldis[tabnum]).get_dimensions().x() + TAB_GAP;
		}
	}

	void UIShop::changeselltab(InventoryType::Id type)
	{
		uint16_t oldtab = tabbyinventory(sellstate.tab);

		if (oldtab > 0)
			buttons[oldtab]->set_state(Button::State::NORMAL);

		uint16_t newtab = tabbyinventory(type);

		if (newtab > 0)
			buttons[newtab]->set_state(Button::State::PRESSED);

		sellstate.change_tab(inventory, type, meso);

		sellslider.setrows(ROWS, sellstate.lastslot);
		sellslider.setenabled(sellstate.lastslot > ROWS);

		for (size_t i = Buttons::SELL0; i <= Buttons::SELL4; i++)
		{
			if (i - Buttons::SELL0 < sellstate.lastslot)
				buttons[i]->set_state(Button::State::NORMAL);
			else
				buttons[i]->set_state(Button::State::DISABLED);
		}
	}

	void UIShop::reset(int32_t npcid)
	{
		currency_item = 0;
		currency_name.clear();

		std::string strid = string_format::extend_id(npcid, 7);
		// Load the shopkeeper's animated stand so the portrait breathes /
		// blinks instead of being a still frame. Follow an "info/link"
		// redirect (some NPCs point to a base NPC for art) and fall back
		// to the first state with frames if "stand" is empty.
		nl::node npc_src = nl::nx::npc[strid + ".img"];
		std::string link = (std::string)npc_src["info"]["link"];
		if (!link.empty())
			npc_src = nl::nx::npc[link + ".img"];

		nl::node chosen;
		size_t best_frames = 0;
		for (auto sub : npc_src)
		{
			if (sub.name() == "info") continue;
			size_t n = 0;
			for (auto f : sub)
				if (f.data_type() == nl::node::type::bitmap) n++;
			if (n > best_frames)
			{
				best_frames = n;
				chosen = sub;
			}
		}
		if (!chosen || chosen.size() == 0)
			chosen = npc_src["stand"];
		if (chosen.size() > 0)
			npc = Animation(chosen);

		for (auto& button : buttons)
			if (button.second)
				button.second->set_state(Button::State::NORMAL);

		if (buttons[Buttons::OVERALL])
			buttons[Buttons::OVERALL]->set_state(Button::State::PRESSED);
		if (buttons[Buttons::EQUIP])
			buttons[Buttons::EQUIP]->set_state(Button::State::PRESSED);

		buttons[Buttons::RECHARGE]->set_state(Button::State::DISABLED);

		buystate.reset();
		sellstate.reset();

		changeselltab(InventoryType::Id::EQUIP);

		makeactive();
		rightclicksell = Configuration::get().get_rightclicksell();
	}

	void UIShop::modify(InventoryType::Id type)
	{
		if (type == sellstate.tab)
			changeselltab(type);
	}

	void UIShop::set_currency_item(int32_t itemid)
	{
		currency_item = itemid;

		if (itemid > 0)
		{
			const ItemData& idata = ItemData::get(itemid);
			currency_icon = idata.get_icon(false);
			currency_name = idata.is_valid() ? idata.get_name() : "Coin";
		}
	}

	void UIShop::add_item(int32_t id, int32_t price, int32_t pitch, int32_t time, int16_t buyable)
	{
		add_rechargable(id, price, pitch, time, 0, buyable);
	}

	void UIShop::add_rechargable(int32_t id, int32_t price, int32_t pitch, int32_t time, int16_t chargeprice, int16_t buyable)
	{
		const bool coin = currency_item > 0;
		auto buyitem = coin
			? BuyItem(currency_icon, true, " " + currency_name, id, price, pitch, time, chargeprice, buyable)
			: BuyItem(meso, false, "meso", id, price, pitch, time, chargeprice, buyable);
		buystate.add(buyitem);

		// lastslot reflects the filtered view only; enable slider if
		// the visible list already overflows.
		buyslider.setrows(ROWS, buystate.lastslot);
		buyslider.setenabled(buystate.lastslot > ROWS);
	}

	int16_t UIShop::slot_by_position(int16_t y)
	{
		int16_t yoff = y - ROW_Y;

		if (yoff < 0 || yoff >= ROWS * ROW_PITCH)
			return -1;

		// Only the 35 px of row art count, not the separator below it
		int16_t in_row = yoff % ROW_PITCH;

		if (in_row < ROW_ART_Y || in_row >= ROW_ART_Y + ROW_ART_H)
			return -1;

		return yoff / ROW_PITCH;
	}

	uint16_t UIShop::tabbyinventory(InventoryType::Id type)
	{
		switch (type)
		{
		case InventoryType::Id::EQUIP:
			return Buttons::EQUIP;
		case InventoryType::Id::USE:
			return Buttons::USE;
		case InventoryType::Id::ETC:
			return Buttons::ETC;
		case InventoryType::Id::SETUP:
			return Buttons::SETUP;
		case InventoryType::Id::CASH:
			return Buttons::CASH;
		default:
			return 0;
		}
	}

	int16_t UIShop::tab_art_index(InventoryType::Id type)
	{
		// UIWindow.img/Shop/TabSell bitmap order: Equip, Use, Set-up, Etc, Cash
		switch (type)
		{
		case InventoryType::Id::EQUIP:
			return 0;
		case InventoryType::Id::USE:
			return 1;
		case InventoryType::Id::SETUP:
			return 2;
		case InventoryType::Id::ETC:
			return 3;
		case InventoryType::Id::CASH:
			return 4;
		default:
			return 0;
		}
	}

	uint16_t UIShop::buy_tab_by_inventory(InventoryType::Id type)
	{
		switch (type)
		{
		case InventoryType::Id::NONE:
			return Buttons::OVERALL;
		case InventoryType::Id::EQUIP:
			return Buttons::BUY_TAB_EQUIP;
		case InventoryType::Id::USE:
			return Buttons::BUY_TAB_USE;
		case InventoryType::Id::ETC:
			return Buttons::BUY_TAB_ETC;
		case InventoryType::Id::SETUP:
			return Buttons::BUY_TAB_SETUP;
		case InventoryType::Id::CASH:
			return Buttons::BUY_TAB_CASH;
		default:
			return Buttons::OVERALL;
		}
	}

	void UIShop::changebuytab(InventoryType::Id type)
	{
		// Highlight only the active buy-side tab.
		uint16_t old_btn = buy_tab_by_inventory(buystate.filter);
		uint16_t new_btn = buy_tab_by_inventory(type);

		if (buttons[old_btn]) buttons[old_btn]->set_state(Button::State::NORMAL);
		if (buttons[new_btn]) buttons[new_btn]->set_state(Button::State::PRESSED);

		buystate.set_filter(type);
		buyslider.setrows(ROWS, buystate.lastslot);
		buyslider.setenabled(buystate.lastslot > ROWS);
	}

	void UIShop::exit_shop()
	{
		clear_tooltip();

		deactivate();
		NpcShopActionPacket().dispatch();
	}

	UIShop::BuyItem::BuyItem(Texture cur, bool ic, const std::string& suffix, int32_t i, int32_t p, int32_t pt, int32_t t, int16_t cp, int16_t b) : currency(cur), item_currency(ic), id(i), price(p), pitch(pt), time(t), chargeprice(cp), buyable(b)
	{
		namelabel = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::MINESHAFT);
		pricelabel = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::MINESHAFT);

		const ItemData& item = ItemData::get(id);

		if (item.is_valid())
		{
			icon = item.get_icon(false);
			namelabel.change_text(item.get_name());
		}

		int32_t display_price = (chargeprice > 0 && price == 0) ? chargeprice : price;
		std::string mesostr = std::to_string(display_price);
		string_format::split_number(mesostr);
		pricelabel.change_text(mesostr + suffix);
	}

	// Row drawing. `pos` is the row's top-left (icon box edge, ROW_Y line).
	// Item icons are bottom-left anchored (Item.wz origin = (x, height)),
	// so they're placed at the icon box's floor; text goes on the name
	// strip that starts 36 px to the right.
	void UIShop::BuyItem::draw(Point<int16_t> pos) const
	{
		icon.draw(pos + Point<int16_t>(1, 37));
		namelabel.draw(pos + Point<int16_t>(40, 4));

		// 32px item icon at half scale where the meso coin sits
		if (item_currency)
			currency.draw(DrawArgument(pos + Point<int16_t>(40, 34), 0.5f, 0.5f));
		else
			currency.draw(pos + Point<int16_t>(40, 21));

		pricelabel.draw(pos + Point<int16_t>(55, 19));
	}

	int32_t UIShop::BuyItem::get_id() const
	{
		return id;
	}

	int16_t UIShop::BuyItem::get_buyable() const
	{
		return buyable;
	}

	int32_t UIShop::BuyItem::get_price() const
	{
		return price;
	}

	int32_t UIShop::BuyItem::get_pitch() const
	{
		return pitch;
	}

	UIShop::SellItem::SellItem(int32_t item_id, int16_t count, int16_t s, bool sc, Texture cur)
	{
		const ItemData& idata = ItemData::get(item_id);

		icon = idata.get_icon(false);
		id = item_id;
		sellable = count;
		slot = s;
		showcount = sc;

		namelabel = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::MINESHAFT);
		pricelabel = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::MINESHAFT);

		std::string name = idata.get_name();

		if (name.length() >= 28)
			name = name.substr(0, 28) + "..";

		namelabel.change_text(name);

		int32_t price = idata.get_price();
		std::string mesostr = std::to_string(price);
		string_format::split_number(mesostr);
		pricelabel.change_text(mesostr + "meso");
	}

	void UIShop::SellItem::draw(Point<int16_t> pos) const
	{
		icon.draw(pos + Point<int16_t>(1, 37));

		if (showcount)
		{
			static const Charset countset = Charset(nl::nx::ui["Basic.img"]["ItemNo"], Charset::Alignment::LEFT);
			countset.draw(std::to_string(sellable), pos + Point<int16_t>(0, 23));
		}

		namelabel.draw(pos + Point<int16_t>(40, 4));
		pricelabel.draw(pos + Point<int16_t>(40, 19));
	}

	int32_t UIShop::SellItem::get_id() const
	{
		return id;
	}

	int16_t UIShop::SellItem::get_slot() const
	{
		return slot;
	}

	int16_t UIShop::SellItem::get_sellable() const
	{
		return sellable;
	}

	void UIShop::BuyState::reset()
	{
		all_items.clear();
		items.clear();
		item_slots.clear();
		filter = InventoryType::Id::NONE;

		offset = 0;
		lastslot = 0;
		selection = -1;
	}

	void UIShop::BuyState::set_filter(InventoryType::Id f)
	{
		filter = f;
		items.clear();
		item_slots.clear();

		for (int16_t s = 0; s < static_cast<int16_t>(all_items.size()); s++)
		{
			if (filter == InventoryType::Id::NONE
				|| InventoryType::by_item_id(all_items[s].get_id()) == filter)
			{
				items.push_back(all_items[s]);
				item_slots.push_back(s);
			}
		}

		lastslot = static_cast<int16_t>(items.size());
		offset = 0;
		selection = -1;
	}

	void UIShop::BuyState::draw(Point<int16_t> parentpos, const Texture& selected) const
	{
		for (int16_t i = 0; i < ROWS; i++)
		{
			int16_t slot = i + offset;

			if (slot >= lastslot)
				break;

			auto itempos = Point<int16_t>(BUY_X, ROW_Y + ROW_PITCH * i);

			if (slot == selection)
				selected.draw(parentpos + itempos + Point<int16_t>(NAME_STRIP_DX, ROW_ART_Y));

			items[slot].draw(parentpos + itempos);
		}
	}

	void UIShop::BuyState::show_item(int16_t slot)
	{
		int16_t absslot = slot + offset;

		if (absslot < 0 || absslot >= lastslot)
			return;

		int32_t itemid = items[absslot].get_id();
		UI::get().show_item(Tooltip::Parent::SHOP, itemid);
	}

	void UIShop::BuyState::add(BuyItem item)
	{
		int16_t server_slot = static_cast<int16_t>(all_items.size());
		all_items.push_back(item);

		if (filter == InventoryType::Id::NONE
			|| InventoryType::by_item_id(item.get_id()) == filter)
		{
			items.push_back(item);
			item_slots.push_back(server_slot);
			lastslot = static_cast<int16_t>(items.size());
		}
	}

	void UIShop::BuyState::buy() const
	{
		if (selection < 0 || selection >= lastslot)
			return;

		const BuyItem& item = items[selection];
		int16_t buyable = item.get_buyable();
		int16_t slot = item_slots[selection];
		int32_t itemid = item.get_id();

		// Cosmic's Shop.buy() has 3 currency branches (meso / pitch /
		// Golden Maple Leaf token). If price == 0 AND pitch == 0, the
		// server-side code falls through every branch and returns
		// silently — no shopTransaction reply, so the client otherwise
		// just looks frozen. Bail out client-side with a status toast
		// so the player gets an explanation.
		if (item.get_price() <= 0 && item.get_pitch() <= 0)
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED,
					"This item requires a special currency that's not supported.");
			return;
		}

		if (buyable == 0 || buyable > 1)
		{
			// buyable == 0 means unlimited; buyable > 1 means limited quantity
			constexpr char* question = "How many are you willing to buy?";
			int16_t max_qty = (buyable == 0) ? 100 : buyable;

			auto onenter = [slot, itemid](int32_t qty)
			{
				auto shortqty = static_cast<int16_t>(qty);

				NpcShopActionPacket(slot, itemid, shortqty, true).dispatch();
			};

			// Shop's buy-quantity prompt uses its own layout — writing
			// indicator a bit lower than the default, OK/Cancel buttons
			// nudged down and to the left so they don't overlap.
			UI::get().emplace<UIEnterNumber>(question, onenter, max_qty, 1, -23, -50, 0);
		}
		else if (buyable > 0)
		{
			constexpr char* question = "Are you sure you want to buy it?";

			auto ondecide = [slot, itemid](bool yes)
			{
				if (yes)
					NpcShopActionPacket(slot, itemid, 1, true).dispatch();
			};

			UI::get().emplace<UIYesNo>(question, ondecide, Text::Alignment::CENTER, -60, 14);
		}
	}

	void UIShop::BuyState::select(int16_t selected)
	{
		int16_t slot = selected + offset;

		if (slot == selection)
			buy();
		else
			selection = slot;
	}

	void UIShop::SellState::reset()
	{
		items.clear();

		offset = 0;
		lastslot = 0;
		selection = -1;
		tab = InventoryType::Id::NONE;
	}

	void UIShop::SellState::change_tab(const Inventory& inventory, InventoryType::Id newtab, Texture meso)
	{
		tab = newtab;

		offset = 0;
		selection = -1;

		items.clear();

		int16_t slots = inventory.get_slotmax(tab);

		for (int16_t i = 1; i <= slots; i++)
		{
			if (int32_t item_id = inventory.get_item_id(tab, i))
			{
				int16_t count = inventory.get_item_count(tab, i);
				items.emplace_back(item_id, count, i, tab != InventoryType::Id::EQUIP, meso);
			}
		}

		lastslot = static_cast<int16_t>(items.size());
	}

	void UIShop::SellState::draw(Point<int16_t> parentpos, const Texture& selected) const
	{
		for (int16_t i = 0; i < ROWS; i++)
		{
			int16_t slot = i + offset;

			if (slot >= lastslot)
				break;

			Point<int16_t> itempos(SELL_X, ROW_Y + ROW_PITCH * i);

			if (slot == selection)
				selected.draw(parentpos + itempos + Point<int16_t>(NAME_STRIP_DX, ROW_ART_Y));

			items[slot].draw(parentpos + itempos);
		}
	}

	void UIShop::SellState::show_item(int16_t slot)
	{
		int16_t absslot = slot + offset;

		if (absslot < 0 || absslot >= lastslot)
			return;

		if (tab == InventoryType::Id::EQUIP)
		{
			int16_t realslot = items[absslot].get_slot();
			UI::get().show_equip(Tooltip::Parent::SHOP, realslot);
		}
		else
		{
			int32_t itemid = items[absslot].get_id();
			UI::get().show_item(Tooltip::Parent::SHOP, itemid);
		}
	}

	void UIShop::SellState::sell(bool skip_confirmation) const
	{
		if (selection < 0 || selection >= lastslot)
			return;

		const SellItem& item = items[selection];
		int32_t itemid = item.get_id();
		int16_t sellable = item.get_sellable();
		int16_t slot = item.get_slot();

		if (sellable > 1)
		{
			constexpr char* question = "How many are you willing to sell?";

			auto onenter = [itemid, slot](int32_t qty)
			{
				auto shortqty = static_cast<int16_t>(qty);

				NpcShopActionPacket(slot, itemid, shortqty, false).dispatch();
			};

			// Same layout as the buy prompt for consistency.
			UI::get().emplace<UIEnterNumber>(question, onenter, sellable, 1, -23, -50, 0);
		}
		else if (sellable > 0)
		{
			if (skip_confirmation)
			{
				NpcShopActionPacket(slot, itemid, 1, false).dispatch();
				return;
			}

			constexpr char* question = "Are you sure you want to sell it?";

			auto ondecide = [itemid, slot](bool yes)
			{
				if (yes)
					NpcShopActionPacket(slot, itemid, 1, false).dispatch();
			};

			UI::get().emplace<UIYesNo>(question, ondecide, Text::Alignment::CENTER, -60, 14);
		}
	}

	void UIShop::SellState::recharge() const
	{
		if (selection < 0 || selection >= lastslot)
			return;

		// The server prices the recharge itself; it only needs the slot
		NpcShopActionPacket(items[selection].get_slot()).dispatch();
	}

	void UIShop::SellState::select(int16_t selected)
	{
		int16_t slot = selected + offset;

		if (slot == selection)
			sell(false);
		else
			selection = slot;
	}
}
