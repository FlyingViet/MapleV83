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
#include "CashShopHandlers.h"

#include "Helpers/CashShopParser.h"
#include "Helpers/ItemParser.h"

#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../Packets/CashShopPackets.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIChatBar.h"
#include "../../IO/UITypes/UIStatusMessenger.h"
#include "../../IO/UITypes/UIMTS.h"
#include "../../IO/Window.h"

#include <algorithm>
#include <chrono>

namespace ms
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		std::vector<CashLockerItem> s_locker;
		CashShopData::Notice s_notice;
		bool s_buying = false;
		Clock::time_point s_buy_at;
		int64_t s_taking = 0;
		int32_t s_taking_item = 0;
		Clock::time_point s_take_at;

		// Cosmic's addCashItemInformation, outside gifts.
		CashLockerItem read_cash_item(InPacket& recv)
		{
			CashLockerItem item;
			item.cash_id = recv.read_long();
			recv.skip_int();	// account id
			recv.skip_int();
			item.item_id = recv.read_int();
			item.sn = recv.read_int();
			item.count = recv.read_short();
			recv.skip(13);		// gift sender
			item.expiration = recv.read_long();
			recv.skip_long();

			return item;
		}

		std::string message_for(uint8_t code)
		{
			switch (code)
			{
				case 0xA3: return "The request timed out. Please try again.";
				case 0xA5: return "You don't have enough NX for that.";
				case 0xAA: return "That item is for the other gender.";
				case 0xAC: return "Your Locker is full. Move some items to your inventory first.";
				case 0xBB: return "Your inventory is full.";
				case 0xBF: return "That item isn't for sale right now.";
				case 0xC0: return "That item is sold out.";
				case 0xC1: return "You've reached your NX spending limit.";
				case 0xB0: return "That coupon code is wrong.";
				case 0xB2: return "That coupon has expired.";
				case 0xB3: return "That coupon has already been used.";
				default:
				{
					static const char* HEX = "0123456789ABCDEF";
					std::string hex = { HEX[code >> 4], HEX[code & 15] };

					return "The Cash Shop couldn't do that (code " + hex + ").";
				}
			}
		}
	}

	namespace CashShopData
	{
		const std::vector<CashLockerItem>& locker()
		{
			return s_locker;
		}

		void buy(int32_t sn, int32_t item_id, int32_t currency)
		{
			if (s_buying)
				return;

			s_buying = true;
			s_buy_at = Clock::now();
			BuyCashItemPacket(static_cast<int8_t>(currency), sn, item_id / 1000000 == 9).dispatch();
		}

		bool buying()
		{
			return s_buying;
		}

		void take_out(const CashLockerItem& item)
		{
			if (s_taking)
				return;

			InventoryType::Id type = InventoryType::by_item_id(item.item_id);
			s_taking = item.cash_id;
			s_taking_item = item.item_id;
			s_take_at = Clock::now();
			TakeFromCashInventoryPacket(item.cash_id, type).dispatch();
		}

		bool taking_out(int64_t cash_id)
		{
			return s_taking != 0 && s_taking == cash_id;
		}

		const Notice& notice()
		{
			return s_notice;
		}

		void post(const std::string& text, bool error)
		{
			s_notice.text = text;
			s_notice.error = error;
			s_notice.serial++;
		}

		void expire_requests()
		{
			Clock::time_point now = Clock::now();

			// Cosmic answers a refused take-out (no room) with nothing at all.
			if (s_taking && now - s_take_at > std::chrono::seconds(4))
			{
				s_taking = 0;
				s_taking_item = 0;
				post("Couldn't move it. Make room in that inventory tab and try again.", true);
			}

			if (s_buying && now - s_buy_at > std::chrono::seconds(8))
			{
				s_buying = false;
				post("The Cash Shop didn't answer. Check your Locker before buying again.", true);
			}
		}
	}
	void SetCashShopHandler::handle(InPacket& recv) const
	{
		s_locker.clear();
		s_buying = false;
		s_taking = 0;
		s_taking_item = 0;

		CashShopParser::parseCharacterInfo(recv);

		recv.skip_byte();	// Not MTS
		recv.skip_string();	// account_name
		recv.skip_int();

		int16_t specialcashitem_size = recv.read_short();

		for (size_t i = 0; i < specialcashitem_size; i++)
		{
			recv.skip_int();	// sn
			recv.skip_int();	// mod
			recv.skip_byte();	// info
		}

		recv.skip(121);

		for (size_t cat = 1; cat <= 8; cat++)
		{
			for (size_t gender = 0; gender < 2; gender++)
			{
				for (size_t in = 0; in < 5; in++)
				{
					recv.skip_int(); // category
					recv.skip_int(); // gender
					recv.skip_int(); // commoditysn
				}
			}
		}

		recv.skip_int();
		recv.skip_short();
		recv.skip_byte();
		recv.skip_int();

		transition();
	}

	void CashShopOperationHandler::handle(InPacket& recv) const
	{
		uint8_t operation = static_cast<uint8_t>(recv.read_byte());

		switch (operation)
		{
			case 0x4B:
			{
				// The locker, sent on entering
				s_locker.clear();
				int16_t count = recv.read_short();

				for (int16_t i = 0; i < count; i++)
					s_locker.push_back(read_cash_item(recv));

				break;
			}
			case 0x57:
			{
				CashLockerItem item = read_cash_item(recv);
				s_locker.push_back(item);
				s_buying = false;
				CashShopData::post("Bought " + ItemData::get(item.item_id).get_name() + ". It's in your Locker.", false);
				break;
			}
			case 0x89:
			{
				int8_t count = recv.read_byte();

				for (int8_t i = 0; i < count; i++)
					s_locker.push_back(read_cash_item(recv));

				s_buying = false;
				CashShopData::post("Bought a package of " + std::to_string(count) + " items. They're in your Locker.", false);
				break;
			}
			case 0x68:
			{
				// Moved to the inventory: its slot, then the item as the inventory holds it
				int16_t slot = recv.read_short();
				int32_t item_id = s_taking_item;
				InventoryType::Id type = InventoryType::by_item_id(item_id);

				if (item_id && type != InventoryType::Id::NONE)
					ItemParser::parse_item(recv, type, slot, Stage::get().get_player().get_inventory());

				int64_t cash_id = s_taking;
				s_locker.erase(std::remove_if(s_locker.begin(), s_locker.end(),
					[cash_id](const CashLockerItem& item) { return item.cash_id == cash_id; }), s_locker.end());

				s_taking = 0;
				s_taking_item = 0;

				if (item_id)
					CashShopData::post("Moved " + ItemData::get(item_id).get_name() + " to your inventory.", false);

				break;
			}
			case 0x59:
			{
				CashShopData::post("Coupon redeemed!", false);
				break;
			}
			case 0x5C:
			{
				uint8_t code = static_cast<uint8_t>(recv.read_byte());
				s_buying = false;
				CashShopData::post(message_for(code), true);
				break;
			}
			default:
				break;
		}
	}

	void SetITCHandler::handle(InPacket& recv) const
	{
		// SET_ITC — MTS transition
		// Same as SET_CASH_SHOP but with MTS trailing data instead of CS items
		CashShopParser::parseCharacterInfo(recv);

		recv.skip_string();	// account_name

		// MTS config bytes (hardcoded by Cosmic)
		recv.skip(28);

		// Open the MTS UI
		UI::get().emplace<UIMTS>();
	}

	void SetCashShopHandler::transition() const
	{
		// The shop lays itself out in the game's own view, so the view
		// size and UI scale stay as they are.

		// Drop any focused textfield (e.g. the chat bar)
		UI::get().remove_textfield();

		float fadestep = 0.025f;

		Window::get().fadeout(
			fadestep,
			[]()
			{
				GraphicsGL::get().clear();

				Stage::get().load(-1, 0);

				GraphicsGL::get().unlock();

				UI::get().change_state(UI::State::CASHSHOP);
				UI::get().enable();
				Timer::get().start();
			}
		);

		GraphicsGL::get().lock();
		Stage::get().clear();
		Timer::get().start();
	}

	static int32_t s_cash_balances[3] = { 0, 0, 0 };

	int32_t get_cash_balance(int which)
	{
		return (which >= 0 && which < 3) ? s_cash_balances[which] : 0;
	}

	void QueryCashResultHandler::handle(InPacket& recv) const
	{
		// Cash query result — the balances render in the cash shop's
		// left panel next to the baked NX labels
		if (recv.available())
		{
			s_cash_balances[0] = recv.read_int(); // NX credit
			s_cash_balances[1] = recv.read_int(); // maple points
			s_cash_balances[2] = recv.read_int(); // NX prepaid
		}

		// A purchase answers with the item first; a refusal with only this.
		if (s_buying)
		{
			s_buying = false;
			CashShopData::post("That purchase didn't go through. Some items need a higher level.", true);
		}
	}

	void CashShopNameChangeHandler::handle(InPacket& recv) const
	{
		std::string name = recv.read_string();
		bool in_use = recv.read_bool();

		if (auto messenger = UI::get().get_element<UIStatusMessenger>())
		{
			if (in_use)
				messenger->show_status(Color::Name::RED, "Name '" + name + "' is already in use.");
			else
				messenger->show_status(Color::Name::WHITE, "Name '" + name + "' is available!");
		}
	}

	void CashShopNameChangePossibleHandler::handle(InPacket& recv) const
	{
		recv.read_int(); // 0
		int8_t error = recv.read_byte();
		recv.read_int(); // 0

		if (error == 0)
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::WHITE, "Name change is available.");
		}
		else
		{
			std::string msg;

			switch (error)
			{
			case 1: msg = "A name change has already been submitted."; break;
			case 2: msg = "Must wait at least one month between name changes."; break;
			case 3: msg = "Cannot change name due to a recent ban."; break;
			default: msg = "Name change is not available."; break;
			}

			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, msg);
		}
	}

	void CashShopTransferWorldHandler::handle(InPacket& recv) const
	{
		recv.read_int(); // 0
		int8_t error = recv.read_byte();
		recv.read_int(); // 0
		bool ok = recv.read_bool();

		if (ok)
		{
			int32_t world_count = recv.read_int();
			std::string world_list = "[CashShop] Available worlds: ";

			for (int32_t i = 0; i < world_count; i++)
			{
				std::string world_name = recv.read_string();

				if (i > 0)
					world_list += ", ";

				world_list += world_name;
			}

			chat::log(world_list, chat::LineType::YELLOW);
		}
		else if (error != 0)
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, "World transfer failed (error=" + std::to_string(error) + ").");
		}
	}

	void CashGachaponResultHandler::handle(InPacket& recv) const
	{
		int8_t op = recv.read_byte();

		if (op == (int8_t)0xE4) // Open failed
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, "Gachapon failed to open.");
		}
		else if (op == (int8_t)0xE5) // Open success
		{
			recv.read_long(); // box cash id
			int32_t remaining = recv.read_int();

			chat::log("[Gachapon] Opened! Remaining: " + std::to_string(remaining), chat::LineType::YELLOW);
			// CashItemInfo + reward data follows
		}
	}
}