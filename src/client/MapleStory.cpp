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
#include "Net/Session.h"
#include "Util/CrashLog.h"
#ifndef MS_PLATFORM_WASM
#include "Util/HardwareInfo.h"
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
			for (; accumulator >= timestep; accumulator -= timestep)
				update();

			// Draw the game. Interpolate to account for remaining time.
			float alpha = static_cast<float>(accumulator) / timestep;
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

int main()
{
#ifndef MS_PLATFORM_WASM
	ms::install_crash_logger();
	ms::HardwareInfo();
	ms::ScreenResolution();
#endif
	ms::start();

	return 0;
}
