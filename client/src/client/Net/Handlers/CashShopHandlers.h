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

#include "../PacketHandler.h"

#include <string>
#include <vector>

namespace ms
{
	// Handler for entering the Cash Shop
	class SetCashShopHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;

	private:
		void transition() const;
	};

	// Last cash balances the server reported (QUERY_CASH_RESULT):
	// 0 = NX credit, 1 = maple points, 2 = NX prepaid
	int32_t get_cash_balance(int which);

	// A purchase waiting in the cash shop locker (the account's "cash
	// inventory") until it is moved to the character's inventory.
	struct CashLockerItem
	{
		int64_t cash_id;
		int32_t item_id;
		int32_t sn;
		int16_t count;
		int64_t expiration;
	};

	namespace CashShopData
	{
		const std::vector<CashLockerItem>& locker();

		// Buy one commodity: currency 1 = NX credit, 2 = maple points, 4 = prepaid.
		void buy(int32_t sn, int32_t item_id, int32_t currency);
		bool buying();

		// Move a locker item to the character's inventory.
		void take_out(const CashLockerItem& item);
		bool taking_out(int64_t cash_id);

		// The latest result for the UI to show; serial changes with each one.
		struct Notice
		{
			std::string text;
			bool error = false;
			uint32_t serial = 0;
		};

		const Notice& notice();
		void post(const std::string& text, bool error);

		// Requests the server never answered (it stays silent on some refusals).
		void expire_requests();
	}

	// Handler for Cash Shop operation responses (buy, coupon, etc.)
	class CashShopOperationHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;
	};

	// Handler for entering MTS (SET_ITC, opcode 126)
	class SetITCHandler : public PacketHandler
	{
	public:
		void handle(InPacket& recv) const override;
	};

	// Cash shop cash query result
	class QueryCashResultHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Cash shop name change check
	class CashShopNameChangeHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Cash shop name change possible result
	class CashShopNameChangePossibleHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Cash shop world transfer possible result
	class CashShopTransferWorldHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};

	// Cash shop gachapon item result
	class CashGachaponResultHandler : public PacketHandler
	{
		void handle(InPacket& recv) const override;
	};
}