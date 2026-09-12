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

#include "../MapleStory.h"

#ifdef MS_PLATFORM_WASM
#include "NetConstants.h"

#include <string>

namespace ms
{
#ifdef USE_CRYPTO
	const size_t HANDSHAKE_LEN = 16;
#else
	const size_t HANDSHAKE_LEN = 2;
#endif

	// Socket for the web build. The browser cannot open TCP connections, so the
	// client talks to a WebSocket->TCP proxy instead: the first WebSocket message
	// is the text "host:port" of the real game server, after which raw packet
	// bytes flow both ways. Presents the same interface (and the same handshake
	// behaviour) as SocketWinsock/SocketAsio so Session needs no changes.
	class SocketWeb
	{
	public:
		SocketWeb();
		~SocketWeb();

		// Connect to the proxy, ask it to connect to address:port and read the
		// server handshake into the buffer. Returns true when exactly
		// HANDSHAKE_LEN bytes were read, exactly like SocketWinsock::open.
		bool open(const char* address, const char* port);
		// Close the WebSocket
		bool close();
		// Non-blocking read of whatever bytes have arrived into the buffer.
		// Sets *connected to false when the connection is gone.
		size_t receive(bool* connected);
		// The receive buffer (after open() it holds the handshake)
		const int8_t* get_buffer() const;
		// Send bytes to the server through the proxy
		bool dispatch(const int8_t* bytes, size_t length) const;

		// Hostname of the page the client is running in ("127.0.0.1" when
		// unavailable). Used when ProxyIP is empty and for wildcard hosts.
		static std::string browser_hostname();

	private:
		int sock;
		int8_t buffer[MAX_PACKET_LENGTH];
	};
}
#endif
