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
#include "SocketAsio.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <vector>

#ifndef _WIN32
#include <sys/socket.h>
#endif

#if defined(USE_ASIO) && !defined(MS_PLATFORM_WASM)
namespace ms
{
	SocketAsio::SocketAsio() : resolver(iocontext), socket(iocontext) {}

	SocketAsio::~SocketAsio()
	{
		if (socket.is_open())
		{
			error_code error;
			socket.close(error);
		}
	}

	bool SocketAsio::open(const char* address, const char* port)
	{
		// A numeric address is used as is. Looking it up instead lets iOS on an
		// IPv6-only network (T-Mobile) turn it into a NAT64 address, which sends
		// a Tailscale/LAN address out over cellular where nothing answers.
		error_code error;
		std::vector<tcp::endpoint> endpoints;
		asio::ip::address literal = asio::ip::make_address(address, error);

		if (!error)
		{
			endpoints.emplace_back(literal, static_cast<unsigned short>(std::atoi(port)));
		}
		else
		{
			auto results = resolver.resolve(address, port, error);

			if (error)
				return false;

			for (const auto& entry : results)
				endpoints.push_back(entry.endpoint());

			// IPv4 first: the Mac is only reachable that way.
			std::stable_partition(endpoints.begin(), endpoints.end(),
				[](const tcp::endpoint& e) { return e.address().is_v4(); });
		}

		if (endpoints.empty())
			return false;

		// Connect and read the server's greeting within a deadline, so a
		// route that accepts but stalls fails instead of hanging the game.
		bool done = false;
		error_code result = asio::error::timed_out;
		size_t received = 0;

		asio::async_connect(socket, endpoints,
			[&](const error_code& ec, const tcp::endpoint&)
			{
				if (ec)
				{
					result = ec;
					done = true;
					return;
				}

				socket.async_read_some(asio::buffer(buffer),
					[&](const error_code& rec, size_t n)
					{
						result = rec;
						received = n;
						done = true;
					});
			});

		iocontext.restart();
		iocontext.run_for(std::chrono::seconds(CONNECT_TIMEOUT_S));

		if (!done)
		{
			// Cancel what is still pending and let its handlers finish.
			socket.close(error);
			iocontext.restart();
			iocontext.run();

			return false;
		}

		return !result && received == HANDSHAKE_LEN;
	}

	bool SocketAsio::close()
	{
		error_code error;
		socket.shutdown(tcp::socket::shutdown_both, error);
		socket.close(error);

		return !error;
	}

	size_t SocketAsio::receive(bool* recvok)
	{
		error_code error;
		size_t waiting = socket.available(error);

		if (error)
		{
			*recvok = false;
			return 0;
		}

		if (waiting > 0)
		{
			size_t result = socket.read_some(asio::buffer(buffer), error);
			*recvok = !error;

			return result;
		}

#ifndef _WIN32
		// A connection the server closed (or that iOS reclaimed while the app
		// was in the background) never has bytes waiting: only a read sees
		// the end. Peek without blocking to notice it.
		char probe;
		ssize_t peeked = ::recv(socket.native_handle(), &probe, 1, MSG_PEEK | MSG_DONTWAIT);

		if (peeked == 0 || (peeked < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
			*recvok = false;
#endif

		return 0;
	}

	const int8_t* SocketAsio::get_buffer() const
	{
		return buffer;
	}

	bool SocketAsio::dispatch(const int8_t* bytes, size_t length)
	{
		error_code error;
		size_t result = asio::write(socket, asio::buffer(bytes, length), error);

		return !error && (result == length);
	}
}
#endif