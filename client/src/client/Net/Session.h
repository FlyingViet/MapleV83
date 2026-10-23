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

#include <chrono>

#include "Cryptography.h"
#include "PacketSwitch.h"

#include "../Error.h"
#include "../MapleStory.h"

#include "../Template/Singleton.h"

#ifdef MS_PLATFORM_WASM
#include "SocketWeb.h"
#elif defined(USE_ASIO)
#include "SocketAsio.h"
#else
#include "SocketWinsock.h"
#endif

namespace ms
{
	class Session : public Singleton<Session>
	{
	public:
		Session();
		~Session();

		// Connect using host and port from the configuration file
		Error init();
		// Send a packet to the server
		void write(int8_t* bytes, size_t length);
		// Check for incoming packets and handle them
		void read();
		// Closes the current connection and opens a new one with default connection settings
		void reconnect();
		// Closes the current connection and opens a new one
		void reconnect(const char* address, const char* port);
		// Check if the connection is alive
		bool is_connected() const;

		// The connection counts as lost unless the server sends something
		// within ms (checking a connection after the app was suspended).
		void expect_reply(int32_t ms);
		// Drop the connection now.
		void drop();

	private:
		bool init(const char* host, const char* port);
		void process(const int8_t* bytes, size_t available);

		Cryptography cryptography;
		PacketSwitch packetswitch;

		int8_t buffer[MAX_PACKET_LENGTH];
		size_t length;
		size_t pos;
		bool connected;
		std::chrono::steady_clock::time_point reply_deadline;
		bool awaiting_reply = false;

#ifdef MS_PLATFORM_WASM
		SocketWeb socket;
#elif defined(USE_ASIO)
		SocketAsio socket;
#else
		SocketWinsock socket;
#endif
	};
}