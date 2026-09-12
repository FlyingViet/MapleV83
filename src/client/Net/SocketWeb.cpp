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
#include "SocketWeb.h"

#ifdef MS_PLATFORM_WASM
#include "../Configuration.h"

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ms
{
	namespace
	{
		// Proxy port used when the ProxyPort setting is empty
		const char* const DEFAULT_PROXY_PORT = "8080";
		// How long open() waits for the server handshake, in milliseconds
		const size_t HANDSHAKE_TIMEOUT_MS = 30000;
		// Length of the Asyncify sleeps used while waiting on the socket
		const size_t POLL_INTERVAL_MS = 5;
		// Returned by the ws_* helpers on failure, like SOCKET_ERROR for Winsock
		const int WS_SOCK_ERROR = -1;

		// Ported from nmnsnv's Util/Misc.h getBrowserHostname().
		std::string get_browser_hostname()
		{
			// Pass a dummy argument to avoid pedantic zero-arg variadic macro warnings.
			char* hostname = (char*)EM_ASM_PTR({
				var hostname = window.location.hostname;
				var len = lengthBytesUTF8(hostname) + 1;
				var ptr = _malloc(len);
				stringToUTF8(hostname, ptr, len);
				return ptr;
			}, 0);

			std::string result = "127.0.0.1";

			if (hostname)
			{
				if (std::strlen(hostname) > 0)
					result = hostname;

				std::free(hostname);
			}

			return result;
		}

		enum class WebSockState
		{
			CONNECTING,
			OPEN,
			CLOSED
		};

		// One WebSocket plus the bytes received on it that nobody has read yet
		class WebSockInstance
		{
		public:
			WebSockInstance(EMSCRIPTEN_WEBSOCKET_T s, size_t initial_buffer_size) : socket(s), state(WebSockState::CONNECTING), deleted(false)
			{
				buffer.reserve(initial_buffer_size);
			}

			~WebSockInstance()
			{
				disconnect();
			}

			void on_open()
			{
				state = WebSockState::OPEN;
			}

			void on_closed()
			{
				state = WebSockState::CLOSED;
			}

			void on_received(const uint8_t* data, size_t len)
			{
				buffer.insert(buffer.end(), data, data + len);
			}

			// Close (if still open) and release the emscripten handle. Safe to call twice.
			void disconnect()
			{
				if (deleted)
					return;

				if (state != WebSockState::CLOSED)
				{
					emscripten_websocket_close(socket, 1000, "Client closing");

					state = WebSockState::CLOSED;
				}

				emscripten_websocket_delete(socket);

				deleted = true;
			}

			bool send(const void* data, size_t len)
			{
				if (state != WebSockState::OPEN)
					return false;

				EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(socket, const_cast<void*>(data), static_cast<uint32_t>(len));

				return result == EMSCRIPTEN_RESULT_SUCCESS;
			}

			bool send(const std::string& text)
			{
				if (state != WebSockState::OPEN)
					return false;

				EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(socket, text.c_str());

				return result == EMSCRIPTEN_RESULT_SUCCESS;
			}

			WebSockState get_state() const
			{
				return state;
			}

			size_t available() const
			{
				return buffer.size();
			}

			// Move up to len buffered bytes into data, like recv() on a stream socket
			size_t receive(char* data, size_t len)
			{
				if (buffer.empty())
					return 0;

				size_t bytes_to_copy = std::min(buffer.size(), len);

				std::memcpy(data, buffer.data(), bytes_to_copy);

				buffer.erase(buffer.begin(), buffer.begin() + bytes_to_copy);

				return bytes_to_copy;
			}

		private:
			EMSCRIPTEN_WEBSOCKET_T socket;
			std::vector<uint8_t> buffer;
			WebSockState state;
			bool deleted;
		};

		EM_BOOL onopen(int eventType, const EmscriptenWebSocketOpenEvent* event, void* userData);
		EM_BOOL onmessage(int eventType, const EmscriptenWebSocketMessageEvent* event, void* userData);
		EM_BOOL onerror(int eventType, const EmscriptenWebSocketErrorEvent* event, void* userData);
		EM_BOOL onclose(int eventType, const EmscriptenWebSocketCloseEvent* event, void* userData);

		// Owns every open WebSocket, keyed by its emscripten handle
		class WebSockManager
		{
		public:
			static WebSockManager& get()
			{
				static WebSockManager instance;

				return instance;
			}

			// Build the proxy URL from the settings, connect, wait (via Asyncify)
			// until the socket is open and send the "host:port" target line.
			// Returns the socket handle, or 0 on failure.
			EMSCRIPTEN_WEBSOCKET_T connect(const std::string& address, const std::string& port)
			{
				std::string ws_url = Setting<AssetsServerProtocol>::get().load() + "://";

				std::string proxy_ip = Setting<ProxyIP>::get().load();
				std::string proxy_port = Setting<ProxyPort>::get().load();
				std::string proxy_path = Setting<ProxyPath>::get().load();

				if (!proxy_ip.empty())
					ws_url += proxy_ip;
				else
					ws_url += get_browser_hostname();

				ws_url += ":";

				if (!proxy_port.empty())
					ws_url += proxy_port;
				else
					ws_url += DEFAULT_PROXY_PORT;

				// Optional path for path-routed reverse proxies (tailscale serve, nginx, ...)
				if (!proxy_path.empty() && proxy_path[0] != '/')
					ws_url += "/";

				ws_url += proxy_path;

				std::cout << "[SocketWeb] Connecting to WebSocket proxy: " << ws_url << std::endl;

				if (!emscripten_websocket_is_supported())
				{
					std::cout << "[SocketWeb] WebSocket is not supported in this browser" << std::endl;

					return 0;
				}

				EmscriptenWebSocketCreateAttributes attrs;
				emscripten_websocket_init_create_attributes(&attrs);

				attrs.url = ws_url.c_str();
				attrs.protocols = nullptr;
				attrs.createOnMainThread = EM_TRUE;

				EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attrs);

				if (socket <= 0)
				{
					std::cout << "[SocketWeb] Failed to create WebSocket" << std::endl;

					return 0;
				}

				auto inserted = instances.emplace(socket, std::make_unique<WebSockInstance>(socket, MAX_PACKET_LENGTH));
				WebSockInstance* instance = inserted.first->second.get();

				emscripten_websocket_set_onopen_callback(socket, instance, onopen);
				emscripten_websocket_set_onmessage_callback(socket, instance, onmessage);
				emscripten_websocket_set_onerror_callback(socket, instance, onerror);
				emscripten_websocket_set_onclose_callback(socket, instance, onclose);

				// Block (Asyncify) until the socket leaves the CONNECTING state
				while (instance->get_state() == WebSockState::CONNECTING)
					emscripten_sleep(POLL_INTERVAL_MS);

				if (instance->get_state() != WebSockState::OPEN)
				{
					std::cout << "[SocketWeb] WebSocket connection to proxy failed" << std::endl;

					remove_instance(socket);

					return 0;
				}

				// Tell the proxy which TCP endpoint to bridge to
				if (!instance->send(address + ":" + port))
				{
					std::cout << "[SocketWeb] Failed to send target to proxy" << std::endl;

					remove_instance(socket);

					return 0;
				}

				return socket;
			}

			WebSockInstance* get_instance(EMSCRIPTEN_WEBSOCKET_T socket)
			{
				auto iter = instances.find(socket);

				if (iter == instances.end())
					return nullptr;

				return iter->second.get();
			}

			void remove_instance(EMSCRIPTEN_WEBSOCKET_T socket)
			{
				auto iter = instances.find(socket);

				// The instance destructor closes and deletes the WebSocket
				if (iter != instances.end())
					instances.erase(iter);
			}

		private:
			WebSockManager() {}

			std::unordered_map<EMSCRIPTEN_WEBSOCKET_T, std::unique_ptr<WebSockInstance>> instances;
		};

		EM_BOOL onopen(int, const EmscriptenWebSocketOpenEvent*, void* userData)
		{
			static_cast<WebSockInstance*>(userData)->on_open();

			return EM_TRUE;
		}

		EM_BOOL onmessage(int, const EmscriptenWebSocketMessageEvent* event, void* userData)
		{
			auto instance = static_cast<WebSockInstance*>(userData);

			if (event->isText)
			{
				// The proxy only ever sends binary game data; a text frame is a
				// diagnostic from the proxy, not something to feed into Session.
				std::cout << "[SocketWeb] Proxy: " << reinterpret_cast<const char*>(event->data) << std::endl;
			}
			else
			{
				instance->on_received(event->data, event->numBytes);
			}

			return EM_TRUE;
		}

		EM_BOOL onerror(int, const EmscriptenWebSocketErrorEvent*, void* userData)
		{
			// Keep the instance so bytes that arrived before the error can still
			// be read; ws_recv reports the disconnect once the buffer is drained.
			static_cast<WebSockInstance*>(userData)->on_closed();

			return EM_TRUE;
		}

		EM_BOOL onclose(int, const EmscriptenWebSocketCloseEvent*, void* userData)
		{
			static_cast<WebSockInstance*>(userData)->on_closed();

			return EM_TRUE;
		}

		int ws_connect(const std::string& address, const std::string& port)
		{
			return WebSockManager::get().connect(address, port);
		}

		int ws_send(int socket, const char* bytes, size_t length)
		{
			auto instance = WebSockManager::get().get_instance(socket);

			if (instance == nullptr)
				return WS_SOCK_ERROR;

			if (!instance->send(bytes, length))
				return WS_SOCK_ERROR;

			return static_cast<int>(length);
		}

		// Wait (Asyncify) until at least min_bytes are buffered, the socket closes
		// or timeout milliseconds pass. Returns false when the socket is gone.
		bool ws_wait(int socket, size_t min_bytes, size_t timeout)
		{
			size_t time_slept = 0;

			while (true)
			{
				auto instance = WebSockManager::get().get_instance(socket);

				if (instance == nullptr)
					return false;

				if (instance->available() >= min_bytes)
					return true;

				if (instance->get_state() != WebSockState::OPEN)
					return instance->available() > 0;

				if (time_slept >= timeout)
					return true;

				size_t sleep_len = std::min(POLL_INTERVAL_MS, timeout - time_slept);

				emscripten_sleep(sleep_len);

				time_slept += sleep_len;
			}
		}

		// recv() equivalent: copies up to length buffered bytes. With timeout 0 it
		// never blocks. Returns WS_SOCK_ERROR once the connection is closed and
		// nothing is left to read.
		int ws_recv(int socket, char* bytes, size_t length, size_t timeout)
		{
			if (timeout > 0)
				ws_wait(socket, 1, timeout);

			auto instance = WebSockManager::get().get_instance(socket);

			if (instance == nullptr)
				return WS_SOCK_ERROR;

			if (instance->available() == 0)
			{
				if (instance->get_state() == WebSockState::CLOSED)
					return WS_SOCK_ERROR;

				return 0;
			}

			return static_cast<int>(instance->receive(bytes, length));
		}

		void ws_closesocket(int socket)
		{
			WebSockManager::get().remove_instance(socket);
		}
	}

	SocketWeb::SocketWeb() : sock(0) {}

	SocketWeb::~SocketWeb()
	{
		if (sock != 0)
			ws_closesocket(sock);
	}

	bool SocketWeb::open(const char* iaddr, const char* port)
	{
		std::cout << "[SocketWeb] Opening connection to " << iaddr << ":" << port << std::endl;

		sock = ws_connect(iaddr, port);

		if (sock == 0)
			return false;

		// Mirror SocketWinsock::open: block until the server handshake arrives,
		// read at most 32 bytes of it and succeed only on exactly HANDSHAKE_LEN.
		// Waiting for HANDSHAKE_LEN bytes (rather than for the first byte) only
		// makes a split handshake succeed where Winsock's single recv would not.
		ws_wait(sock, HANDSHAKE_LEN, HANDSHAKE_TIMEOUT_MS);

		int result = ws_recv(sock, (char*)buffer, 32, 0);

		if (result == static_cast<int>(HANDSHAKE_LEN))
		{
			return true;
		}
		else
		{
			std::cout << "[SocketWeb] Bad handshake (" << result << " bytes)" << std::endl;

			ws_closesocket(sock);

			sock = 0;

			return false;
		}
	}

	bool SocketWeb::close()
	{
		std::cout << "[SocketWeb] Closing connection" << std::endl;

		if (sock != 0)
			ws_closesocket(sock);

		sock = 0;

		return true;
	}

	bool SocketWeb::dispatch(const int8_t* bytes, size_t length) const
	{
		return ws_send(sock, (const char*)bytes, length) != WS_SOCK_ERROR;
	}

	size_t SocketWeb::receive(bool* success)
	{
		int result = ws_recv(sock, (char*)buffer, MAX_PACKET_LENGTH, 0);

		if (result == WS_SOCK_ERROR)
		{
			*success = false;

			return 0;
		}
		else
		{
			return result;
		}
	}

	const int8_t* SocketWeb::get_buffer() const
	{
		return buffer;
	}

	std::string SocketWeb::browser_hostname()
	{
		return get_browser_hostname();
	}
}
#endif
