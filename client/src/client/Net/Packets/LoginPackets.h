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

#include "../../Configuration.h"

namespace ms
{
	// MapleV83 wire protocol, appended to the login packet. Raise it whenever
	// server <-> app packets change shape; the server turns older apps away
	// with "update required" instead of loading a character they can't read.
	//   3 = 0.19: 32-bit equipment stats (0.18 brought 32-bit stats and mesos)
	constexpr int32_t MV83_PROTOCOL_MAGIC = 0x3338564D; // "MV83"
	constexpr int16_t MV83_PROTOCOL = 6;	// 4: character level is a short; 5: SP is 32-bit; 6: 64-bit damage

	// Login failure code for an app older than the server's protocol.
	constexpr int32_t MV83_UPDATE_REQUIRED = 83;

	// Opcode: LOGIN(1)
	class LoginPacket : public OutPacket
	{
	public:
		// Request to be logged-in to an account
		LoginPacket(const std::string& acc, const std::string& pass) : OutPacket(OutPacket::Opcode::LOGIN)
		{
			std::string volumeSerialNumber = Configuration::get().get_vol_serial_num();

			// Platforms without a real volume serial (web) report an empty string; pad so
			// the fixed-width slices below cannot throw.
			if (volumeSerialNumber.size() < 8)
				volumeSerialNumber.append(8 - volumeSerialNumber.size(), '0');
			std::string part1 = volumeSerialNumber.substr(0, 2);
			std::string part2 = volumeSerialNumber.substr(2, 2);
			std::string part3 = volumeSerialNumber.substr(4, 2);
			std::string part4 = volumeSerialNumber.substr(6, 2);

			const int32_t h = hex_to_dec(part4);
			const int32_t w = hex_to_dec(part3);
			const int32_t i = hex_to_dec(part2);
			const int32_t d = hex_to_dec(part1);

			write_string(acc);
			write_string(pass);

			skip(6);

			write_byte(h);
			write_byte(w);
			write_byte(i);
			write_byte(d);

			write_int(MV83_PROTOCOL_MAGIC);
			write_short(MV83_PROTOCOL);
		}
	};

	// Opcode: CHARLIST_REQUEST(5)
	class CharlistRequestPacket : public OutPacket
	{
	public:
		// Requests the list of characters on a world
		CharlistRequestPacket(uint8_t world, uint8_t channel) : OutPacket(OutPacket::Opcode::CHARLIST_REQUEST)
		{
			write_byte(0);
			write_byte(world);
			write_byte(channel);
		}
	};

	// Opcode: SERVERSTATUS_REQUEST(6)
	class ServerStatusRequestPacket : public OutPacket
	{
	public:
		// Requests the status of the server
		ServerStatusRequestPacket(int16_t world) : OutPacket(OutPacket::Opcode::SERVERSTATUS_REQUEST)
		{
			write_short(world);
		}
	};

	// Opcode: ACCEPT_TOS(7)
	class TOSPacket : public OutPacket
	{
	public:
		// Accept the Terms of Service
		TOSPacket() : OutPacket(OutPacket::Opcode::ACCEPT_TOS)
		{
			write_byte(1);
		}
	};

	// Opcode: SET_GENDER(8)
	class GenderPacket : public OutPacket
	{
	public:
		// Send user selected Gender
		GenderPacket(const bool female) : OutPacket(OutPacket::Opcode::SET_GENDER)
		{
			write_byte(1);
			write_byte(female);
		}
	};

	// Opcode: SERVERLIST_REQUEST(11)
	class ServerRequestPacket : public OutPacket
	{
	public:
		// Requests the list of worlds and channels
		ServerRequestPacket() : OutPacket(OutPacket::Opcode::SERVERLIST_REQUEST) {}
	};

	// Opcode: PLAYER_LOGIN(20)
	class PlayerLoginPacket : public OutPacket
	{
	public:
		// Requests being logged-in to a channel server with the specified character
		PlayerLoginPacket(int32_t cid) : OutPacket(OutPacket::Opcode::PLAYER_LOGIN)
		{
			write_int(cid);
		}
	};

	// Opcode: LOGIN_START(35)
	class LoginStartPacket : public OutPacket
	{
	public:
		// Sends whenever we hit the start of the Login screen
		LoginStartPacket() : OutPacket(OutPacket::Opcode::LOGIN_START) {}
	};
}