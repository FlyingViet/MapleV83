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

#include "../OutPacket.h"

namespace ms
{
	// Opcode: BUY_CS_ITEM(229) = Cosmic's CASHSHOP_OPERATION. Buy is
	// action 0x03 (0x1E for packages): byte action, byte pad, int currency
	// (1 = NX credit, 2 = maple points, 4 = prepaid), int commodity SN.
	class BuyCashItemPacket : public OutPacket
	{
	public:
		// Request the server to purchase a cash shop item
		BuyCashItemPacket(int8_t currency, int32_t sn_item_id, bool package = false) : OutPacket(OutPacket::Opcode::BUY_CS_ITEM)
		{
			write_byte(package ? 0x1E : 0x03);
			write_byte(0);
			write_int(currency);
			write_int(sn_item_id);
		}
	};

	// CASHSHOP_OPERATION action 0x0D: move a locker item into the character's
	// inventory. Cosmic reads the cash id as an int; the low half of the long
	// comes first, and the client's inventory type and slot follow.
	class TakeFromCashInventoryPacket : public OutPacket
	{
	public:
		TakeFromCashInventoryPacket(int64_t cash_id, int8_t inventory_type) : OutPacket(OutPacket::Opcode::BUY_CS_ITEM)
		{
			write_byte(0x0D);
			write_long(cash_id);
			write_byte(inventory_type);
			write_short(-1);
		}
	};

	// Opcode: COUPON_CODE(216)
	class CouponCodePacket : public OutPacket
	{
	public:
		// Redeem a coupon code in the cash shop
		CouponCodePacket(const std::string& code) : OutPacket(OutPacket::Opcode::COUPON_CODE)
		{
			write_string(code);
		}
	};
}
