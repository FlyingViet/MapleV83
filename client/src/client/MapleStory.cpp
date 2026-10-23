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
#include <iostream>
#include <thread>
#include <algorithm>

#ifdef MS_PLATFORM_WASM
#include <cstdlib>

#include <emscripten.h>
#endif

#include "Configuration.h"
#include "Constants.h"
#include "Gameplay/Stage.h"
#include "Graphics/Text.h"
#include "IO/UI.h"
#include "IO/Window.h"
#include "Net/Resume.h"
#include "Net/Session.h"
#include "Util/CrashLog.h"
#include "Util/HardwareInfo.h"
#ifndef MS_PLATFORM_WASM
#include "Util/ScreenResolution.h"
#endif

#ifdef USE_NX
#include "Util/NxFiles.h"
#else
#include "Util/WzFiles.h"
#endif

namespace ms
{
	Error init()
	{
#ifdef MS_PLATFORM_WASM
		// The page configures the client through the JS object Module.LazyFS
		// (see nmnsnv's Journey.cpp). Copy each string it defines into the
		// matching setting before anything reads them.
		auto readConfig = [](const char* key) -> std::string
		{
			char* val = (char*)EM_ASM_INT({
				var k = UTF8ToString($0);
				if (typeof Module !== 'undefined' && Module.LazyFS && Module.LazyFS[k] !== undefined && Module.LazyFS[k] !== null) {
					var str = Module.LazyFS[k].toString();
					var lengthBytes = lengthBytesUTF8(str) + 1;
					var stringOnWasmHeap = _malloc(lengthBytes);
					stringToUTF8(str, stringOnWasmHeap, lengthBytes);
					return stringOnWasmHeap;
				}
				return 0;
			}, key);

			std::string out;
			if (val)
			{
				out = val;
				std::free(val);
			}
			return out;
		};
		auto loadConfigString = [&readConfig](const char* key, auto& setting)
		{
			std::string v = readConfig(key);
			if (!v.empty())
				setting.save(v);
		};
		auto loadConfigShort = [&readConfig](const char* key, auto& setting)
		{
			std::string v = readConfig(key);
			if (!v.empty())
				setting.save(static_cast<uint16_t>(std::stoi(v)));
		};

		// nmnsnv's page uses MapleStoryServerIp/Port for the game server
		loadConfigString("MapleStoryServerIp", Setting<ServerIP>::get());
		loadConfigString("MapleStoryServerPort", Setting<ServerPort>::get());
		loadConfigString("ProxyIP", Setting<ProxyIP>::get());
		loadConfigString("ProxyPort", Setting<ProxyPort>::get());
		loadConfigString("ProxyPath", Setting<ProxyPath>::get());
		loadConfigString("AssetsServerProtocol", Setting<AssetsServerProtocol>::get());
		loadConfigString("AssetsServerIP", Setting<AssetsServerIP>::get());
		loadConfigString("AssetsServerPort", Setting<AssetsServerPort>::get());
		loadConfigString("AssetsServerPath", Setting<AssetsServerPath>::get());
		loadConfigShort("Width", Setting<Width>::get());
		loadConfigShort("Height", Setting<Height>::get());

		// Apply the configured render size now: OpenStory hard-codes a 1920x1080
		// surface with a 1.5 UI scale (1280x720 logical). Keep the logical layout and
		// derive the scale from the requested physical width, so phones can run a
		// smaller canvas without changing the UI layout.
		{
			int16_t w = static_cast<int16_t>(Setting<Width>::get().load());
			int16_t h = static_cast<int16_t>(Setting<Height>::get().load());
			if (w >= 640 && h >= 360)
			{
				Constants::Constants::get().set_viewwidth(w);
				Constants::Constants::get().set_viewheight(h);
				Constants::Constants::get().set_ui_scale(std::max(1.0f, w / 1280.0f));
			}
		}
#endif

		std::cout << "[Init] Connecting to server..." << std::endl;

		if (Error error = Session::get().init())
			return error;

		std::cout << "[Init] Loading game files..." << std::endl;

#ifdef USE_NX
		if (Error error = NxFiles::init())
			return error;
#else
		if (Error error = WzFiles::init())
			return error;
#endif

		std::cout << "[Init] Creating window..." << std::endl;

		if (Error error = Window::get().init())
			return error;

		std::cout << "[Init] Initializing audio..." << std::endl;

		if (Error error = Sound::init())
			return error;

		if (Error error = Music::init())
			return error;

		std::cout << "[Init] Loading game data..." << std::endl;

		Char::init();
		DamageNumber::init();
		MapPortals::init();
		Stage::get().init();
		UI::get().init();

		std::cout << "[Init] Ready." << std::endl;

		return Error::NONE;
	}

	void update()
	{
		Window::get().check_events();
		Window::get().update();
		Stage::get().update();
		UI::get().update();
		Session::get().read();
	}

	// Current frames-per-second, recomputed a few times a second in loop().
	int g_fps = 0;

	void draw(float alpha)
	{
		Window::get().begin();
		Stage::get().draw(alpha);
		UI::get().draw(alpha);

		// On-screen FPS counter (top-right, yellow).
		if (Configuration::get().get_show_fps())
		{
			static Text fpslabel(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::YELLOW);
			fpslabel.change_text("FPS " + std::to_string(g_fps));

			int16_t sw = static_cast<int16_t>(Constants::Constants::get().get_viewwidth());
			fpslabel.draw(DrawArgument(Point<int16_t>(static_cast<int16_t>(sw - 12), 6)));
		}

		Window::get().end();
	}

	bool running()
	{
		return Session::get().is_connected()
			&& UI::get().not_quitted()
			&& Window::get().not_closed();
	}

#ifdef MS_PLATFORM_WASM
	namespace
	{
		// The browser drives frames through emscripten_set_main_loop instead of
		// a blocking while loop, so the fixed-timestep state lives here and
		// persists between ticks.
		const int64_t TIMESTEP = Constants::TIMESTEP * 1000;
		int64_t accumulator = TIMESTEP;

		// FPS counter accumulators.
		int64_t fps_accum = 0;
		int32_t fps_frames = 0;

		void main_tick()
		{
			if (!running())
			{
				emscripten_cancel_main_loop();

				Sound::close();

				return;
			}

			int64_t elapsed = Timer::get().stop();

			// Same clamp as the native loop: a stalled tab must not fast-forward
			// physics by a burst of update() steps.
			accumulator += elapsed;

			const int64_t MAX_ACCUM = TIMESTEP * 5;

			if (accumulator > MAX_ACCUM)
				accumulator = MAX_ACCUM;

			// Update game with constant timestep as many times as possible.
			for (; accumulator >= TIMESTEP; accumulator -= TIMESTEP)
				update();

			// Draw the game. Interpolate to account for remaining time.
			float alpha = static_cast<float>(accumulator) / TIMESTEP;
			draw(alpha);

			// Recompute the on-screen FPS ~4 times per second.
			fps_accum += elapsed;
			fps_frames++;

			if (fps_accum >= 250000)
			{
				g_fps = static_cast<int>(fps_frames * 1000000LL / fps_accum);
				fps_frames = 0;
				fps_accum = 0;
			}
		}
	}

	void loop()
	{
		Timer::get().start();

		accumulator = TIMESTEP;
		fps_accum = 0;
		fps_frames = 0;

		// fps 0 = requestAnimationFrame; simulate_infinite_loop = 1 means this
		// call never returns. The browser paces frames, so the native FPSCap
		// sleep is deliberately absent here (sleep_for would stall the tab).
		emscripten_set_main_loop(main_tick, 0, 1);
	}
#else
	void loop()
	{
		Timer::get().start();

		int64_t timestep = Constants::TIMESTEP * 1000;
		int64_t accumulator = timestep;

		// FPS counter accumulators.
		int64_t fps_accum = 0;
		int32_t fps_frames = 0;

		// Frame-rate cap from settings (FPSCap). 0 = uncapped.
		uint8_t fps_cap = Setting<FPSCap>::get().load();
		int64_t FRAME_CAP_US = fps_cap > 0 ? 1000000 / fps_cap : 0;

		while (running())
		{
			auto frame_start = std::chrono::high_resolution_clock::now();

			int64_t elapsed = Timer::get().stop();

			// Clamp the accumulated time so a single long/stalled frame can't
			// trigger a huge catch-up burst of update() steps. Without this,
			// any hitch fast-forwards physics many steps at once and controlled
			// mobs visibly "teleport" forward (and get reported to the server
			// at the jumped-to position, desyncing every other client).
			accumulator += elapsed;

			const int64_t MAX_ACCUM = timestep * 5;

			if (accumulator > MAX_ACCUM)
				accumulator = MAX_ACCUM;

			// Update game with constant timestep as many times as possible.
			auto update_start = std::chrono::high_resolution_clock::now();
			int32_t steps = 0;

			for (; accumulator >= timestep; accumulator -= timestep, steps++)
				update();

			auto draw_start = std::chrono::high_resolution_clock::now();

			// iOS forbids GPU work in the background; the app is suspended
			// moments later anyway.
			if (Resume::in_background())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}

			// Draw the game. Interpolate to account for remaining time.
			float alpha = static_cast<float>(accumulator) / timestep;
			draw(alpha);

			// MV83_HITCH=1 logs frames that take over 20 ms of work.
			static const bool log_hitches = std::getenv("MV83_HITCH") != nullptr;

			if (log_hitches)
			{
				auto done = std::chrono::high_resolution_clock::now();
				auto us = [](auto a, auto b) { return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count(); };
				int64_t update_us = us(update_start, draw_start);
				int64_t draw_us = us(draw_start, done);

				if (update_us + draw_us > 20000)
					std::cout << "[Hitch] update " << update_us / 1000.0 << " ms (" << steps << " steps), draw "
						<< draw_us / 1000.0 << " ms" << std::endl;
			}

			// Recompute the on-screen FPS ~4 times per second.
			fps_accum += elapsed;
			fps_frames++;

			if (fps_accum >= 250000)
			{
				g_fps = static_cast<int>(fps_frames * 1000000LL / fps_accum);
				fps_frames = 0;
				fps_accum = 0;
			}

			// Cap framerate to the configured FPS (skip entirely if uncapped).
			if (FRAME_CAP_US > 0)
			{
				auto frame_end = std::chrono::high_resolution_clock::now();
				auto frame_us = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start).count();

				if (frame_us < FRAME_CAP_US)
					std::this_thread::sleep_for(std::chrono::microseconds(FRAME_CAP_US - frame_us));
			}
		}

		Sound::close();
	}
#endif

	void start()
	{
		// Initialize and check for errors
		if (Error error = init())
		{
			std::cerr << "[Error] " << error.get_message() << error.get_args() << std::endl;

#ifdef MS_PLATFORM_WASM
			// No console in the browser: leave the error in the JS console and stop.
			return;
#else
			bool can_retry = error.can_retry();

			if (can_retry)
			{
				std::cout << "Type 'retry' to try again: ";
				std::string command;
				std::cin >> command;

				if (command == "retry")
					start();
			}
			else
			{
				std::cout << "Press Enter to exit...";
				std::cin.get();
			}
#endif
		}
		else
		{
			loop();
		}
	}
}

#ifdef PLATFORM_SDL
#include <SDL.h>

#include "Audio/Audio.h"
#include "Character/Char.h"
#include "Gameplay/Combat/DamageNumber.h"
#include "Gameplay/MapleMap/MapPortals.h"
#include "Graphics/GraphicsGL.h"

#include "DataDownload.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

// The Mac running the Cosmic server and data server: put your own addresses in LocalServers.h
// (gitignored; copy LocalServers.example.h). Without it: 127.0.0.1 or the server IP in settings.
#if __has_include("LocalServers.h")
#include "LocalServers.h"
#endif
#ifndef MV83_HOME_SERVER
#define MV83_HOME_SERVER "127.0.0.1"
#endif
#ifndef MV83_TAILSCALE_SERVER
#define MV83_TAILSCALE_SERVER MV83_HOME_SERVER
#endif

namespace ms
{
	namespace
	{
		// The Mac running Cosmic, on the home LAN and on Tailscale (see LocalServers.h above).
		const char* const HOME_SERVER = MV83_HOME_SERVER;
		const char* const TAILSCALE_SERVER = MV83_TAILSCALE_SERVER;
		const char* const KNOWN_SERVERS[] = { HOME_SERVER, TAILSCALE_SERVER };

#ifdef PLATFORM_MACOS_SDL
		const char* const THIS_DEVICE = "this Mac";
		const char* const PLATFORM_NAME = "macOS";
#else
		const char* const THIS_DEVICE = "this phone";
		const char* const PLATFORM_NAME = "iOS";
#endif

		bool port_open(const std::string& address, int port, int timeout_ms)
		{
			int fd = socket(AF_INET, SOCK_STREAM, 0);

			if (fd < 0)
				return false;

			fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

			sockaddr_in addr{};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(static_cast<uint16_t>(port));

			bool ok = false;

			if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) == 1)
			{
				if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
				{
					ok = true;
				}
				else if (errno == EINPROGRESS)
				{
					pollfd p{ fd, POLLOUT, 0 };

					if (poll(&p, 1, timeout_ms) == 1)
					{
						int err = 0;
						socklen_t len = sizeof(err);
						getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
						ok = err == 0;
					}
				}
			}

			close(fd);
			return ok;
		}

		// On home Wi-Fi / Ethernet: a real interface (not a Tailscale utun) has an
		// address in the home subnet.
		bool on_home_network()
		{
			ifaddrs* list = nullptr;

			if (getifaddrs(&list) != 0)
				return false;

			in_addr home{};
			inet_pton(AF_INET, HOME_SERVER, &home);
			uint32_t home_net = ntohl(home.s_addr) & 0xFFFFFF00u;
			bool home_found = false;

			for (ifaddrs* i = list; i && !home_found; i = i->ifa_next)
			{
				if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_UP))
					continue;

				std::string name = i->ifa_name ? i->ifa_name : "";

				if (name.rfind("utun", 0) == 0 || name.rfind("lo", 0) == 0)
					continue;

				uint32_t ip = ntohl(reinterpret_cast<sockaddr_in*>(i->ifa_addr)->sin_addr.s_addr);
				home_found = (ip & 0xFFFFFF00u) == home_net;
			}

			freeifaddrs(list);

			return home_found;
		}

		// Where to look for the Mac, best first. Away from home the LAN address
		// can still answer through the router's Tailscale subnet route, but that
		// path is relayed and stalls; the Mac's own Tailscale address is direct.
		std::vector<std::string> server_candidates()
		{
			std::vector<std::string> candidates;
			std::string configured = Setting<ServerIP>::get().load();
			bool known = false;

			for (const char* k : KNOWN_SERVERS)
				known = known || configured == k;

			if (!configured.empty() && configured != "127.0.0.1" && !known)
				candidates.push_back(configured);

			bool home = on_home_network();
			candidates.push_back(home ? HOME_SERVER : TAILSCALE_SERVER);
			candidates.push_back(home ? TAILSCALE_SERVER : HOME_SERVER);

			std::cout << "[Net] " << (home ? "On home network" : "Away from home") << ", trying " << candidates.front()
				<< " first" << std::endl;

			return candidates;
		}

		// The first candidate that answers on `port` (0 = the game port), or "".
		std::string pick_server(int port = 0)
		{
			if (port == 0)
				port = std::atoi(Setting<ServerPort>::get().load().c_str());

			for (const std::string& candidate : server_candidates())
				if (port_open(candidate, port > 0 ? port : 8484, 2000))
					return candidate;

			return "";
		}

		// One frame of a plain message screen, pumping input so iOS stays
		// responsive while the game is not running yet.
		void message_frame(const std::string& title, const std::string& body, const std::string& detail)
		{
			Window::get().check_events();

			// No drawing in the background (iOS forbids GPU work there).
			if (Resume::in_background())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
				return;
			}

			Window::get().begin();

			const int16_t w = Constants::Constants::get().get_viewwidth();
			const int16_t h = Constants::Constants::get().get_viewheight();

			GraphicsGL::get().drawscreenfill(0.05f, 0.06f, 0.10f, 1.0f);
			Text(Text::Font::A18M, Text::Alignment::CENTER, Color::Name::WHITE, title)
				.draw(Point<int16_t>(w / 2, h / 2 - 90));
			Text(Text::Font::A13M, Text::Alignment::CENTER, Color::Name::WHITE, body, 700)
				.draw(Point<int16_t>(w / 2, h / 2 - 40));
			Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::YELLOW, detail, 700)
				.draw(Point<int16_t>(w / 2, h / 2 + 40));

			Window::get().end();
		}

		// Loads the NX files, re-checking every second while they cannot be
		// loaded (e.g. still being copied over USB with tools/push_data.sh).
		Error wait_for_data()
		{
			Error error = NxFiles::init();
			int frames = 0;

			while (error && Window::get().not_closed())
			{
				std::string what = std::string(error.get_message()) + (error.get_args() ? error.get_args() : "");

				message_frame("MapleStory v83 can't load its game data",
					"Relaunch the app on home Wi-Fi or Tailscale to download it from the server Mac, or copy the "
					".nx files with ~/Coding/MapleV83/tools/push_data.sh.",
					what + "  -  checking again every second.");

				if (++frames % 60 == 0)
					error = NxFiles::init();
			}

			return error;
		}

		bool local_data_present()
		{
			for (const char* filename : NxFiles::filenames)
				if (DataDownload::path_for(filename).empty())
					return false;

			return true;
		}

		std::string gigabytes(int64_t bytes)
		{
			char text[32];
			snprintf(text, sizeof(text), "%.2f GB", bytes / 1e9);
			return text;
		}

		// Game data built into the app with its manifest (tools/build_ios.sh
		// puts it in the bundle's nx folder). Empty when this build has none.
		std::string bundled_data_dir()
		{
			char* base = SDL_GetBasePath();

			if (!base)
				return "";

			std::string dir = std::string(base) + "nx/";
			SDL_free(base);

			return std::ifstream{ dir + "manifest.json" }.good() ? dir : "";
		}

		// Brings the game data in line with the converted set on the server Mac
		// (tools/data_server.py), then loads it. The iPhone app carries the
		// whole set and downloads only files that changed on the Mac; the Mac
		// app downloads everything the first time. Away from home without
		// Tailscale a complete local set is used as is.
		Error prepare_data()
		{
			std::string bundled = bundled_data_dir();
			DataDownload::set_bundled_directory(bundled);
			NxFiles::set_resolver(DataDownload::path_for);

			if (!bundled.empty())
				std::cout << "[Data] Game data built into the app at " << bundled << std::endl;

			int attempt = 0;

			while (Window::get().not_closed())
			{
				std::string mac = pick_server(DataDownload::PORT);

				if (mac.empty())
				{
					if (local_data_present())
						return wait_for_data();

					attempt++;

					for (int i = 0; i < 180 && Window::get().not_closed(); i++)
						message_frame("MapleStory v83 needs its game data",
							"The game data (3.9 GB) downloads from the server Mac the first time. Connect "
							+ std::string(THIS_DEVICE) + " to home Wi-Fi or Tailscale and keep the app open.",
							"Mac not reachable - attempt " + std::to_string(attempt) + ", retrying in "
							+ std::to_string(3 - i / 60) + " s");

					continue;
				}

				DataDownload::start(mac);
				DataDownload::Progress progress;
				uint32_t sample_ticks = SDL_GetTicks();
				int64_t sample_bytes = -1;
				double rate = 0.0;

				while (Window::get().not_closed())
				{
					progress = DataDownload::progress();

					if (progress.state == DataDownload::State::DONE || progress.state == DataDownload::State::FAILED)
						break;

					if (progress.state != DataDownload::State::DOWNLOADING || progress.total <= 0)
					{
						message_frame("Checking game data", "Comparing with the v83 data on the server Mac...", progress.file);
						continue;
					}

					uint32_t now = SDL_GetTicks();

					if (sample_bytes < 0)
					{
						sample_bytes = progress.received;
						sample_ticks = now;
					}
					else if (now - sample_ticks >= 2000)
					{
						double recent = (progress.received - sample_bytes) * 1000.0 / (now - sample_ticks);
						rate = rate > 0.0 ? rate * 0.6 + recent * 0.4 : recent;
						sample_bytes = progress.received;
						sample_ticks = now;
					}

					int percent = static_cast<int>(progress.received * 100 / progress.total);
					std::string detail = gigabytes(progress.received) + " of " + gigabytes(progress.total)
						+ " (" + std::to_string(percent) + "%)  -  " + progress.file;

					if (rate > 1.0)
					{
						int64_t seconds = static_cast<int64_t>((progress.total - progress.received) / rate);
						char eta[64];
						snprintf(eta, sizeof(eta), "  -  %.1f MB/s, about %lld min left",
							rate / 1e6, static_cast<long long>(seconds / 60 + 1));
						detail += eta;
					}

					message_frame(bundled.empty() ? "Downloading game data" : "Updating game data",
						"From the server Mac (" + mac + "). Keep the app open; if it is interrupted it "
						"resumes where it stopped.", detail);
				}

				if (!Window::get().not_closed())
					break;

				if (progress.state == DataDownload::State::DONE)
					return wait_for_data();

				// The manifest was unavailable but the phone has a full set:
				// play with it rather than block on the Mac.
				if (local_data_present())
					return wait_for_data();

				for (int i = 0; i < 300 && Window::get().not_closed(); i++)
					message_frame("Game data download stopped", progress.error
						+ (progress.file.empty() ? "" : " (" + progress.file + ")"),
						"Retrying in " + std::to_string(5 - i / 60) + " s - finished files are kept");
			}

			return Error::Code::MISSING_FILE;
		}

		// Connects to the Mac, retrying every few seconds with a screen that
		// says what is happening.
		bool connect_to_server(const std::string& title)
		{
			int attempt = 0;

			while (Window::get().not_closed())
			{
				attempt++;
				message_frame(title, "Looking for the server on the server Mac (home Wi-Fi, then Tailscale)...",
					attempt > 1 ? "Not reachable yet - attempt " + std::to_string(attempt) : "");

				// Each address in turn: one that accepts but never greets (a
				// stalled route) times out and the next is tried.
				int port = std::atoi(Setting<ServerPort>::get().load().c_str());

				for (const std::string& server : server_candidates())
				{
					if (!Window::get().not_closed())
						break;

					if (!port_open(server, port > 0 ? port : 8484, 2000))
						continue;

					Setting<ServerIP>::get().save(server);

					if (!Session::get().init())
					{
						std::cout << "[Init] Connected to " << server << std::endl;
						return true;
					}

					std::cout << "[Init] " << server << " answered but the login handshake failed" << std::endl;
				}

				for (int i = 0; i < 180 && Window::get().not_closed(); i++)
					message_frame(title, "Can't reach the server on the server Mac. Make sure it is on and that "
						+ std::string(THIS_DEVICE) + " is on home Wi-Fi or Tailscale.", "Retrying in " + std::to_string(3 - i / 60) + " s");
			}

			return false;
		}

		void start_ios()
		{
			std::cout << "[Init] Creating window..." << std::endl;

			if (Error error = Window::get().init())
			{
				std::cout << "[Error] " << error.get_message() << " " << (error.get_args() ? error.get_args() : "") << std::endl;
				return;
			}

			std::cout << "[Init] Loading game files..." << std::endl;

			if (prepare_data())
				return;

			if (!connect_to_server("Connecting"))
				return;

			std::cout << "[Init] Initializing audio..." << std::endl;

			if (Error error = Sound::init())
				std::cout << "[Init] No sound: " << error.get_message() << std::endl;

			Music::init();

			std::cout << "[Init] Loading game data..." << std::endl;

			Char::init();
			DamageNumber::init();
			MapPortals::init();
			Stage::get().init();
			UI::get().init();

			std::cout << "[Init] Ready." << std::endl;

			// An iPhone app does not quit itself: a lost connection goes back to
			// the login screen once the server answers again.
			while (true)
			{
				loop();

				if (!Window::get().not_closed() || !UI::get().not_quitted())
					break;

				if (!Resume::changing_character())
					std::cout << "[Net] Connection lost, reconnecting" << std::endl;

				// Reconnect only once the app is back in front, so a session
				// taken over by another device isn't grabbed back while away.
				while (Resume::in_background() && Window::get().not_closed())
				{
					Window::get().check_events();
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}

				// Lost in game: the lobby signs back in to the same character.
				bool switching = Resume::changing_character();
				bool resuming = Resume::arm();

				if (!connect_to_server(switching ? "Changing character" : resuming ? "Reconnecting" : "Connection lost"))
					break;

				Stage::get().clear();
				UI::get().change_state(UI::State::LOGIN);
			}

			Sound::close();
		}
	}
}

// SDL renames this to SDL_main and runs it from its UIKit app delegate.
int main(int argc, char* argv[])
{
	const char* home = std::getenv("HOME");
#ifdef PLATFORM_MACOS_SDL
	// ~/Library/Application Support/MapleV83 holds the NX files, Settings and
	// the log on the Mac.
	std::string documents = std::string(home ? home : ".") + "/Library/Application Support/MapleV83";
	mkdir(documents.c_str(), 0755);
#else
	// Documents holds the NX files, Settings and the log; it is visible in the
	// Files app and reachable with `devicectl device copy`.
	std::string documents = std::string(home ? home : ".") + "/Documents";
#endif

	if (chdir(documents.c_str()) != 0)
		return EXIT_FAILURE;

	// stdout goes nowhere on iOS (or from a Finder-launched Mac app).
	std::rename("client.log", "client.log.prev");

	if (std::freopen("client.log", "w", stdout))
	{
		setvbuf(stdout, nullptr, _IOLBF, 0);
		dup2(fileno(stdout), STDERR_FILENO);
	}

	std::cout << "[Init] OpenStory for " << ms::PLATFORM_NAME << ", data in " << documents << std::endl;

	// Configuration loaded during static initialisation, before the chdir.
	struct stat st;
	bool first_run = stat("Settings", &st) != 0;

	ms::Configuration::get().load();

	if (first_run)
	{
		ms::Setting<ms::ServerIP>::get().save(ms::KNOWN_SERVERS[0]);
		ms::Setting<ms::ServerPort>::get().save("8484");
	}

	ms::install_crash_logger();
	ms::HardwareInfo();
	ms::start_ios();

	std::cout << "[Exit]" << std::endl;
	std::exit(EXIT_SUCCESS);
}
#else
int main()
{
#ifndef MS_PLATFORM_WASM
	ms::install_crash_logger();
#endif
	ms::HardwareInfo();
#ifndef MS_PLATFORM_WASM
	ms::ScreenResolution();
#endif
	ms::start();

	return 0;
}
#endif
