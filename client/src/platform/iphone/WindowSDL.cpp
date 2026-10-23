//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
// ms::Window for iPhone and iPad, on SDL2 (src/IO/Window.cpp is the GLFW one).
//
// The game draws straight into the screen at full resolution, inside the safe
// area. Its logical view is 600 pixels high - v83's native height - and as wide
// as the screen allows (Constants::ui_scale_for), so a phone shows more of a map
// than the original 800x600 window did. Views of a fixed size, such as the cash
// shop's 1024x768, are fitted into the same area with bars either side.
//
// One finger on the game is the mouse: it moves the game's cursor and presses
// where it lands, a quick second tap double-clicks, and holding still for half a
// second right-clicks. A second finger dragging up and down scrolls. Fingers on
// the on-screen controls (TouchControls) press keys instead.
#include "../shared/PlatformConfig.h"

#ifdef PLATFORM_IOS

#include "IO/Window.h"
#include "IO/UI.h"
#include "IO/Gamepad.h"
#include "IO/ItemActions.h"
#include "IO/Notifications.h"
#include "Gameplay/Stage.h"
#include "Net/Packets/InventoryPackets.h"
#include "Net/Packets/NpcInteractionPackets.h"
#include "IO/UITypes/UINotice.h"
#include "Net/Resume.h"

#include "Configuration.h"
#include "Constants.h"
#include "Timer.h"
#include "Graphics/GraphicsGL.h"

#include "IOSPlatform.h"
#include "TouchControls.h"

#include <SDL.h>
#include <SDL_syswm.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace ms
{
	namespace
	{
		// The framebuffer that reaches the display. SDL's UIKit view renders
		// into a framebuffer object of its own; binding 0 draws into nothing.
		GLuint screen_fbo = 0;

		int panel_w = 0;
		int panel_h = 0;
		float px_per_pt = 1.0f;
		TouchControls::Insets safe;

		// Where the game's logical view lands on the panel, top-left origin.
		int view_x = 0, view_y = 0, view_w = 0, view_h = 0;

		// The finger acting as the mouse.
		SDL_FingerID cursor_finger = -1;
		float cursor_down_x = 0.0f, cursor_down_y = 0.0f;
		Uint32 cursor_down_ms = 0;
		bool cursor_moved = false;
		bool long_press_fired = false;
		Uint32 last_tap_up_ms = 0;
		bool last_tap_clean = false;
		float last_tap_x = 0.0f, last_tap_y = 0.0f;

		// A second finger on the game scrolls.
		SDL_FingerID scroll_finger = -1;
		float scroll_last_y = 0.0f;
		float scroll_accum = 0.0f;

		// Whichever was used last decides whether the on-screen controls show:
		// a touch brings them back, a controller button hides them.
		bool controller_mode = false;

		void measure()
		{
			SDL_Window* window = SDL_GL_GetCurrentWindow();

			if (!window)
				return;

			int w = 0, h = 0;
			SDL_GL_GetDrawableSize(window, &w, &h);

			int pw = 0, ph = 0;
			SDL_GetWindowSize(window, &pw, &ph);

			if (w <= 0 || h <= 0)
				return;

			panel_w = w;
			panel_h = h;
			px_per_pt = pw > 0 ? static_cast<float>(w) / pw : 1.0f;

			float l, t, r, b;
			IOSPlatform::safe_insets(l, t, r, b);
			safe.left = l * px_per_pt;
			safe.top = t * px_per_pt;
			safe.right = r * px_per_pt;
			safe.bottom = b * px_per_pt;
		}

		// The area the game may use: the safe area across, the full height (the
		// home indicator floats over the bottom edge without hiding much).
		void game_area(int& x, int& w, int& h)
		{
			x = static_cast<int>(safe.left);
			w = panel_w - static_cast<int>(safe.left + safe.right);
			h = panel_h;
		}

		void fit_view()
		{
			int area_x, area_w, area_h;
			game_area(area_x, area_w, area_h);

			int16_t lw = Constants::Constants::get().get_viewwidth();
			int16_t lh = Constants::Constants::get().get_viewheight();

			if (lw <= 0 || lh <= 0 || area_w <= 0 || area_h <= 0)
				return;

			float scale = std::min(static_cast<float>(area_w) / lw, static_cast<float>(area_h) / lh);
			view_w = static_cast<int>(lw * scale + 0.5f);
			view_h = static_cast<int>(lh * scale + 0.5f);
			view_x = area_x + (area_w - view_w) / 2;
			view_y = (area_h - view_h) / 2;
		}

		Point<int16_t> to_logical(float px, float py)
		{
			int16_t lw = Constants::Constants::get().get_viewwidth();
			int16_t lh = Constants::Constants::get().get_viewheight();

			if (view_w <= 0 || view_h <= 0)
				return Point<int16_t>(0, 0);

			float x = (px - view_x) * lw / view_w;
			float y = (py - view_y) * lh / view_h;

			x = std::clamp(x, 0.0f, static_cast<float>(lw));
			y = std::clamp(y, 0.0f, static_cast<float>(lh));

			return Point<int16_t>(static_cast<int16_t>(x), static_cast<int16_t>(y));
		}

		int to_glfw_key(SDL_Keycode key)
		{
			if (key >= SDLK_a && key <= SDLK_z)
				return GLFW_KEY_A + (key - SDLK_a);

			if (key >= SDLK_0 && key <= SDLK_9)
				return GLFW_KEY_0 + (key - SDLK_0);

			if (key >= SDLK_F1 && key <= SDLK_F12)
				return GLFW_KEY_F1 + (key - SDLK_F1);

			switch (key)
			{
			case SDLK_RETURN: return GLFW_KEY_ENTER;
			case SDLK_KP_ENTER: return GLFW_KEY_KP_ENTER;
			case SDLK_ESCAPE: return GLFW_KEY_ESCAPE;
			case SDLK_BACKSPACE: return GLFW_KEY_BACKSPACE;
			case SDLK_TAB: return GLFW_KEY_TAB;
			case SDLK_SPACE: return GLFW_KEY_SPACE;
			case SDLK_DELETE: return GLFW_KEY_DELETE;
			case SDLK_INSERT: return GLFW_KEY_INSERT;
			case SDLK_HOME: return GLFW_KEY_HOME;
			case SDLK_END: return GLFW_KEY_END;
			case SDLK_PAGEUP: return GLFW_KEY_PAGE_UP;
			case SDLK_PAGEDOWN: return GLFW_KEY_PAGE_DOWN;
			case SDLK_LEFT: return GLFW_KEY_LEFT;
			case SDLK_RIGHT: return GLFW_KEY_RIGHT;
			case SDLK_UP: return GLFW_KEY_UP;
			case SDLK_DOWN: return GLFW_KEY_DOWN;
			case SDLK_LSHIFT: return GLFW_KEY_LEFT_SHIFT;
			case SDLK_RSHIFT: return GLFW_KEY_RIGHT_SHIFT;
			case SDLK_LCTRL: return GLFW_KEY_LEFT_CONTROL;
			case SDLK_RCTRL: return GLFW_KEY_RIGHT_CONTROL;
			case SDLK_LALT: return GLFW_KEY_LEFT_ALT;
			case SDLK_RALT: return GLFW_KEY_RIGHT_ALT;
			case SDLK_MINUS: return GLFW_KEY_MINUS;
			case SDLK_EQUALS: return GLFW_KEY_EQUAL;
			case SDLK_LEFTBRACKET: return GLFW_KEY_LEFT_BRACKET;
			case SDLK_RIGHTBRACKET: return GLFW_KEY_RIGHT_BRACKET;
			case SDLK_SEMICOLON: return GLFW_KEY_SEMICOLON;
			case SDLK_QUOTE: return GLFW_KEY_APOSTROPHE;
			case SDLK_BACKQUOTE: return GLFW_KEY_GRAVE_ACCENT;
			case SDLK_BACKSLASH: return GLFW_KEY_BACKSLASH;
			case SDLK_COMMA: return GLFW_KEY_COMMA;
			case SDLK_PERIOD: return GLFW_KEY_PERIOD;
			case SDLK_SLASH: return GLFW_KEY_SLASH;
			default: return GLFW_KEY_UNKNOWN;
			}
		}

		// Keys that edit text without producing a character, so they still go
		// through the keycode path while the iOS keyboard supplies the text.
		bool is_editing_key(int key)
		{
			switch (key)
			{
			case GLFW_KEY_BACKSPACE:
			case GLFW_KEY_DELETE:
			case GLFW_KEY_ENTER:
			case GLFW_KEY_KP_ENTER:
			case GLFW_KEY_TAB:
			case GLFW_KEY_ESCAPE:
			case GLFW_KEY_LEFT:
			case GLFW_KEY_RIGHT:
			case GLFW_KEY_UP:
			case GLFW_KEY_DOWN:
			case GLFW_KEY_HOME:
			case GLFW_KEY_END:
				return true;
			default:
				return false;
			}
		}

		void send_text(const char* utf8)
		{
			const unsigned char* p = reinterpret_cast<const unsigned char*>(utf8);

			while (*p)
			{
				uint32_t cp = *p;
				int extra = 0;

				if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
				else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
				else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }

				p++;

				for (int i = 0; i < extra && (*p & 0xC0) == 0x80; i++, p++)
					cp = (cp << 6) | (*p & 0x3F);

				UI::get().send_char(cp);
			}
		}

		// The iOS keyboard is up exactly while a text field has focus, and SDL
		// slides the view up to keep that field in sight above it.
		void sync_text_input()
		{
			Rectangle<int16_t> bounds;
			bool wants_text = UI::get().focused_textfield_bounds(bounds);

			if (wants_text)
			{
				int16_t lw = Constants::Constants::get().get_viewwidth();
				int16_t lh = Constants::Constants::get().get_viewheight();

				if (lw > 0 && lh > 0 && px_per_pt > 0.0f)
				{
					SDL_Rect rect;
					rect.x = static_cast<int>((view_x + bounds.left() * view_w / lw) / px_per_pt);
					rect.y = static_cast<int>((view_y + bounds.top() * view_h / lh) / px_per_pt);
					rect.w = static_cast<int>(std::max<int>(bounds.width(), 40) * view_w / lw / px_per_pt);
					rect.h = static_cast<int>(std::max<int>(bounds.height(), 14) * view_h / lh / px_per_pt) + 8;
					SDL_SetTextInputRect(&rect);
				}
			}

			if (wants_text && !SDL_IsTextInputActive())
				SDL_StartTextInput();
			else if (!wants_text && SDL_IsTextInputActive())
				SDL_StopTextInput();
		}

		void release_cursor_finger()
		{
			if (cursor_finger >= 0)
				UI::get().send_cursor(false);

			cursor_finger = -1;
			scroll_finger = -1;
		}

		void finger_down(const SDL_TouchFingerEvent& f)
		{
			float px = f.x * panel_w;
			float py = f.y * panel_h;

			if (TouchControls::finger_down(f.fingerId, px, py))
				return;

			if (cursor_finger >= 0)
			{
				if (scroll_finger < 0)
				{
					scroll_finger = f.fingerId;
					scroll_last_y = py;
					scroll_accum = 0.0f;
				}

				return;
			}

			cursor_finger = f.fingerId;
			cursor_down_x = px;
			cursor_down_y = py;
			cursor_down_ms = SDL_GetTicks();
			cursor_moved = false;
			long_press_fired = false;

			UI::get().send_cursor(to_logical(px, py));
			UI::get().send_cursor(true);
		}

		void finger_move(const SDL_TouchFingerEvent& f)
		{
			float px = f.x * panel_w;
			float py = f.y * panel_h;

			if (TouchControls::finger_move(f.fingerId, px, py))
				return;

			if (f.fingerId == scroll_finger)
			{
				// One notch per 24 points of drag; dragging up scrolls down,
				// like a trackpad.
				scroll_accum += (py - scroll_last_y) / px_per_pt;
				scroll_last_y = py;

				while (std::fabs(scroll_accum) >= 24.0f)
				{
					UI::get().send_scroll(scroll_accum > 0 ? 1.0 : -1.0);
					scroll_accum += scroll_accum > 0 ? -24.0f : 24.0f;
				}

				return;
			}

			if (f.fingerId != cursor_finger)
				return;

			if (std::hypot(px - cursor_down_x, py - cursor_down_y) > 10.0f * px_per_pt)
				cursor_moved = true;

			UI::get().send_cursor_drag(to_logical(px, py));
		}

		void finger_up(const SDL_TouchFingerEvent& f)
		{
			float px = f.x * panel_w;
			float py = f.y * panel_h;

			if (TouchControls::finger_up(f.fingerId, px, py))
				return;

			if (f.fingerId == scroll_finger)
			{
				scroll_finger = -1;
				return;
			}

			if (f.fingerId != cursor_finger)
				return;

			cursor_finger = -1;
			UI::get().send_cursor(to_logical(px, py));

			Uint32 now = SDL_GetTicks();

			// A double tap is two quick, short taps on the same spot. Taps on
			// neighbouring buttons in quick succession are two single taps,
			// and a third tap starts over instead of making a second double.
			bool clean = !long_press_fired && !cursor_moved && now - cursor_down_ms < 250;
			bool near_last = std::hypot(px - last_tap_x, py - last_tap_y) < 14.0f * px_per_pt;
			bool doubled = clean && last_tap_clean && near_last && now - last_tap_up_ms < 300;

			if (doubled)
				UI::get().doubleclick();

			UI::get().send_cursor(false);

			last_tap_up_ms = now;
			last_tap_x = px;
			last_tap_y = py;
			last_tap_clean = clean && !doubled;
		}

		// ---- Scripted input, for testing without a hand on the screen ----
		//
		// If Documents/autotest.txt exists at launch it is run once (then
		// renamed to autotest.done). One command per line:
		//   wait <ms>                 tap <x> <y>       dtap <x> <y>
		//   hold <x> <y> <ms>         drag <x1> <y1> <x2> <y2> <ms>
		//   text <characters>         key <name>        press <name> <ms>
		//   shot <name>               ctl <px> <py> <ms>   (raw panel pixels,
		//                             for the touch controls)
		// x/y are the game's logical pixels; key names are GLFW-ish:
		// enter esc tab bksp space left right up down ctrl alt shift, letters,
		// digits, f1-f12, ins del home end pgup pgdn.
		struct Step
		{
			std::vector<std::string> args;
		};

		std::vector<Step> script;
		size_t script_pos = 0;
		Uint32 script_resume = 0;
		std::string pending_shot;
		bool script_loaded = false;

		struct Timed
		{
			Uint32 at;
			std::function<void()> action;
		};

		std::vector<Timed> timed;

		void load_script()
		{
			script_loaded = true;
			std::ifstream in("autotest.txt");

			if (!in)
				return;

			std::string line;

			while (std::getline(in, line))
			{
				if (line.empty() || line[0] == '#')
					continue;

				Step step;
				std::istringstream words(line);
				std::string word;
				words >> word;
				step.args.push_back(word);

				if (word == "text" || word == "log")
				{
					std::string rest;
					std::getline(words, rest);

					if (!rest.empty() && rest[0] == ' ')
						rest.erase(0, 1);

					step.args.push_back(rest);
				}
				else
				{
					while (words >> word)
						step.args.push_back(word);
				}

				script.push_back(step);
			}

			in.close();
			std::rename("autotest.txt", "autotest.done");
			std::cout << "[Test] script with " << script.size() << " steps" << std::endl;
		}

		int key_named(const std::string& name)
		{
			static const std::map<std::string, int> names = {
				{ "enter", GLFW_KEY_ENTER }, { "esc", GLFW_KEY_ESCAPE }, { "tab", GLFW_KEY_TAB },
				{ "bksp", GLFW_KEY_BACKSPACE }, { "space", GLFW_KEY_SPACE }, { "left", GLFW_KEY_LEFT },
				{ "right", GLFW_KEY_RIGHT }, { "up", GLFW_KEY_UP }, { "down", GLFW_KEY_DOWN },
				{ "ctrl", GLFW_KEY_LEFT_CONTROL }, { "alt", GLFW_KEY_LEFT_ALT }, { "shift", GLFW_KEY_LEFT_SHIFT },
				{ "ins", GLFW_KEY_INSERT }, { "del", GLFW_KEY_DELETE }, { "home", GLFW_KEY_HOME },
				{ "end", GLFW_KEY_END }, { "pgup", GLFW_KEY_PAGE_UP }, { "pgdn", GLFW_KEY_PAGE_DOWN },
			};

			auto it = names.find(name);

			if (it != names.end())
				return it->second;

			if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z')
				return GLFW_KEY_A + (name[0] - 'a');

			if (name.size() == 1 && name[0] >= '0' && name[0] <= '9')
				return GLFW_KEY_0 + (name[0] - '0');

			if (name.size() >= 2 && name[0] == 'f')
				return GLFW_KEY_F1 + std::atoi(name.c_str() + 1) - 1;

			return GLFW_KEY_UNKNOWN;
		}

		SDL_Event finger_event(Uint32 type, float px, float py, SDL_FingerID id)
		{
			SDL_Event ev{};
			ev.type = type;
			ev.tfinger.fingerId = id;
			ev.tfinger.x = panel_w > 0 ? px / panel_w : 0.0f;
			ev.tfinger.y = panel_h > 0 ? py / panel_h : 0.0f;
			ev.tfinger.pressure = 1.0f;
			return ev;
		}

		void push_finger(Uint32 type, float px, float py, SDL_FingerID id)
		{
			SDL_Event ev = finger_event(type, px, py, id);
			SDL_PushEvent(&ev);
		}

		float panel_x(float lx)
		{
			int16_t lw = Constants::Constants::get().get_viewwidth();
			return view_x + lx * view_w / std::max<int16_t>(lw, 1);
		}

		float panel_y(float ly)
		{
			int16_t lh = Constants::Constants::get().get_viewheight();
			return view_y + ly * view_h / std::max<int16_t>(lh, 1);
		}

		void run_script()
		{
			if (!script_loaded)
				load_script();

			Uint32 now = SDL_GetTicks();

			for (size_t i = 0; i < timed.size();)
			{
				if (now >= timed[i].at)
				{
					auto action = timed[i].action;
					timed.erase(timed.begin() + i);
					action();
				}
				else
				{
					i++;
				}
			}

			while (script_pos < script.size() && now >= script_resume)
			{
				const std::vector<std::string>& a = script[script_pos++].args;
				const std::string& cmd = a[0];
				auto num = [&](size_t i) { return i < a.size() ? static_cast<float>(std::atof(a[i].c_str())) : 0.0f; };

				std::cout << "[Test] " << cmd;
				for (size_t i = 1; i < a.size(); i++)
					std::cout << " " << a[i];
				std::cout << std::endl;

				if (cmd == "wait")
				{
					script_resume = now + static_cast<Uint32>(num(1));
				}
				else if (cmd == "tap" || cmd == "dtap" || cmd == "hold")
				{
					float px = panel_x(num(1)), py = panel_y(num(2));
					Uint32 hold = cmd == "hold" ? static_cast<Uint32>(num(3)) : 60;
					push_finger(SDL_FINGERDOWN, px, py, 900);
					timed.push_back({ now + hold, [=]() { push_finger(SDL_FINGERUP, px, py, 900); } });

					if (cmd == "dtap")
					{
						timed.push_back({ now + 160, [=]() { push_finger(SDL_FINGERDOWN, px, py, 901); } });
						timed.push_back({ now + 220, [=]() { push_finger(SDL_FINGERUP, px, py, 901); } });
					}

					script_resume = now + hold + (cmd == "dtap" ? 300 : 120);
				}
				else if (cmd == "drag")
				{
					float x1 = panel_x(num(1)), y1 = panel_y(num(2));
					float x2 = panel_x(num(3)), y2 = panel_y(num(4));
					Uint32 ms = static_cast<Uint32>(std::max(num(5), 100.0f));
					push_finger(SDL_FINGERDOWN, x1, y1, 902);

					int steps = std::max<int>(10, static_cast<int>(ms / 16));

					for (int k = 1; k <= steps; k++)
					{
						float t = static_cast<float>(k) / steps;
						timed.push_back({ now + static_cast<Uint32>(ms * t), [=]() {
							push_finger(SDL_FINGERMOTION, x1 + (x2 - x1) * t, y1 + (y2 - y1) * t, 902); } });
					}

					timed.push_back({ now + ms + 40, [=]() { push_finger(SDL_FINGERUP, x2, y2, 902); } });
					script_resume = now + ms + 120;
				}
				else if (cmd == "ctl")
				{
					float px = num(1), py = num(2);
					Uint32 hold = static_cast<Uint32>(std::max(num(3), 60.0f));
					push_finger(SDL_FINGERDOWN, px, py, 903);
					timed.push_back({ now + hold, [=]() { push_finger(SDL_FINGERUP, px, py, 903); } });
					script_resume = now + hold + 80;
				}
				else if (cmd == "text")
				{
					for (unsigned char c : a[1])
						UI::get().send_char(c);
				}
				else if (cmd == "key" || cmd == "press")
				{
					int key = key_named(a.size() > 1 ? a[1] : "");
					Uint32 hold = cmd == "press" ? static_cast<Uint32>(num(2)) : 60;

					if (key != GLFW_KEY_UNKNOWN)
					{
						UI::get().send_key(key, true);
						timed.push_back({ now + hold, [=]() { UI::get().send_key(key, false); } });
					}

					script_resume = now + hold + 60;
				}
				// Sample notices and notifications, to see and tap them:
				// notice ok|yesno|number|text|wait|death, notify invite|quest|info.
				else if (cmd == "notice" || cmd == "notify")
				{
					std::string what = a.size() > 1 ? a[1] : "";
					auto log = [what](bool yes) { std::cerr << "[AUTOTEST] " << what << " answered " << (yes ? "yes" : "no") << std::endl; };

					if (cmd == "notify" && what == "invite")
						UIAlarmInvite::show("Rina has invited you to their party.", log);
					else if (cmd == "notify" && what == "quest")
						Notifications::notify("Quest Ready", "The Lost Hat is ready to complete!", log);
					else if (cmd == "notify")
						Notifications::info("Server", "Double EXP starts in 10 minutes.");
					else if (what == "yesno")
						UI::get().emplace<UIYesNo>("Are you sure you want to drop the Red Potion?", log);
					else if (what == "number")
						UI::get().emplace<UIEnterNumber>("How many Red Potions do you want to drop?",
							[](int32_t n) { std::cerr << "[AUTOTEST] number " << n << std::endl; }, 120, 1);
					else if (what == "text")
						UI::get().emplace<UIEnterText>("Name your new guild.",
							[](const std::string& t) { std::cerr << "[AUTOTEST] text " << t << std::endl; }, 12);
					else if (what == "wait")
						UI::get().emplace<UIWaitNotice>("Waiting for Rina to accept the trade...");
					else if (what == "death")
						UI::get().emplace<UIDeathNotice>("You have died.", log);
					else
						UI::get().emplace<UIOk>("You don't have enough mesos to buy this item.", log);
				}
				// useitem <id>: use the first stack of an item, as a tap on Use.
				// npcsel <n>: answer an NPC's list with entry n.
				else if (cmd == "useitem")
				{
					int32_t id = static_cast<int32_t>(num(1));
					InventoryType::Id tab = InventoryType::by_item_id(id);

					if (int16_t slot = Stage::get().get_player().get_inventory().find_item(tab, id))
						ItemActions::use(tab, slot);
					else
						std::cerr << "[AUTOTEST] no item " << id << std::endl;
				}
				// dropitem <id> <qty>: drop some of an item at the player's feet.
				else if (cmd == "dropitem")
				{
					int32_t id = static_cast<int32_t>(num(1));
					InventoryType::Id tab = InventoryType::by_item_id(id);

					if (int16_t slot = Stage::get().get_player().get_inventory().find_item(tab, id))
						MoveItemPacket(tab, slot, 0, static_cast<int16_t>(std::max(1.0f, num(2)))).dispatch();
				}
				// npcraw <lastmsg> <response>: answer an NPC prompt (1 1 = Yes).
				else if (cmd == "npcraw")
				{
					NpcTalkMorePacket(static_cast<int8_t>(num(1)), static_cast<int8_t>(num(2))).dispatch();
				}
				else if (cmd == "npcsel")
				{
					NpcTalkMorePacket(static_cast<int32_t>(num(1))).dispatch();
				}
				else if (cmd == "shot")
				{
					pending_shot = a.size() > 1 ? a[1] : "shot";
					script_resume = now + 50;
				}
				else if (cmd == "quit")
				{
					SDL_Event ev{};
					ev.type = SDL_QUIT;
					SDL_PushEvent(&ev);
				}
			}
		}

		void write_shot(const std::string& name)
		{
			std::vector<uint8_t> pixels(static_cast<size_t>(panel_w) * panel_h * 4);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glReadPixels(0, 0, panel_w, panel_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

			std::error_code ec;
			std::filesystem::create_directories("shots", ec);
			std::string path = "shots/" + name + ".png";

			stbi_flip_vertically_on_write(1);
			stbi_write_png(path.c_str(), panel_w, panel_h, 4, pixels.data(), panel_w * 4);
			stbi_flip_vertically_on_write(0);

			std::cout << "[Test] wrote " << path << std::endl;
		}

		void check_long_press()
		{
			if (cursor_finger < 0 || cursor_moved || long_press_fired)
				return;

			// The mobile panels' "+" repeats while held instead.
			if (UI::get().holds_press())
				return;

			if (SDL_GetTicks() - cursor_down_ms >= 500)
			{
				long_press_fired = true;
				UI::get().send_cursor(false);
				UI::get().rightclick();
				IOSPlatform::haptic_tap();
			}
		}
	}

	Window::Window()
	{
		glwnd = nullptr;
		context = nullptr;
		closed = false;
		fullscreen = true;
		opacity = 1.0f;
		opcstep = 0.0f;
		width = Constants::Constants::get().get_physicalwidth();
		height = Constants::Constants::get().get_physicalheight();
	}

	Window::~Window()
	{
		if (context)
			SDL_GL_DeleteContext(context);

		if (glwnd)
			SDL_DestroyWindow(glwnd);

		SDL_Quit();
	}

	Error Window::init()
	{
		SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
		// The home indicator would otherwise take the first swipe near the
		// bottom edge, where the controls are.
		SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
		// Fingers are handled as fingers; synthetic mouse events would press
		// twice.
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
		SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
		// Otherwise tilting the phone is a joystick.
		SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
		{
			std::cout << "[Init] SDL_Init failed: " << SDL_GetError() << std::endl;
			return Error::Code::GLFW;
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

		glwnd = SDL_CreateWindow(Configuration::get().get_title().c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
			SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI);

		if (!glwnd)
		{
			std::cout << "[Init] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
			return Error::Code::WINDOW;
		}

		context = SDL_GL_CreateContext(glwnd);

		if (!context)
		{
			std::cout << "[Init] SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
			return Error::Code::WINDOW;
		}

		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);

		if (SDL_GetWindowWMInfo(glwnd, &info))
			screen_fbo = info.info.uikit.framebuffer;

		SDL_GL_SetSwapInterval(1);

		// Let UIKit lay the window out, so the safe area is known before the
		// logical view is sized from it.
		for (int i = 0; i < 10; i++)
			SDL_PumpEvents();

		measure();

		int area_x, area_w, area_h;
		game_area(area_x, area_w, area_h);

		float ui_scale = Constants::Constants::ui_scale_for(area_w, area_h);
		Constants::Constants::get().set_ui_scale(ui_scale);
		Constants::Constants::get().set_viewwidth(static_cast<int16_t>(area_w));
		Constants::Constants::get().set_viewheight(static_cast<int16_t>(area_h));

		// SetFieldHandler re-reads these on every map load.
		Setting<Width>::get().save(static_cast<uint16_t>(area_w));
		Setting<Height>::get().save(static_cast<uint16_t>(area_h));

		width = static_cast<int16_t>(area_w);
		height = static_cast<int16_t>(area_h);

		std::cout << "[Init] Screen " << panel_w << "x" << panel_h << " px (" << px_per_pt << " px/pt), safe area L"
			<< safe.left << " R" << safe.right << " B" << safe.bottom << ", game " << area_w << "x" << area_h
			<< ", UI scale " << ui_scale << ", logical view " << Constants::Constants::get().get_viewwidth()
			<< "x" << Constants::Constants::get().get_viewheight() << ", screen fbo " << screen_fbo << std::endl;

		if (Error error = GraphicsGL::get().init())
			return error;

		return initwindow();
	}

	Error Window::initwindow()
	{
		GraphicsGL::get().reinit();
		fit_view();

		return Error::Code::NONE;
	}

	bool Window::not_closed() const
	{
		return !closed;
	}

	void Window::update()
	{
		updateopc();
	}

	void Window::updateopc()
	{
		if (opcstep != 0.0f)
		{
			opacity += opcstep;

			if (opacity >= 1.0f)
			{
				opacity = 1.0f;
				opcstep = 0.0f;
			}
			else if (opacity <= 0.0f)
			{
				opacity = 0.0f;
				opcstep = -opcstep;

				fadeprocedure();
			}
		}
	}

	void Window::check_events()
	{
		run_script();
		sync_text_input();
		check_long_press();

		SDL_Event ev;

		while (SDL_PollEvent(&ev))
		{
			switch (ev.type)
			{
			case SDL_QUIT:
				closed = true;
				break;

			case SDL_WINDOWEVENT:
				if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || ev.window.event == SDL_WINDOWEVENT_RESIZED)
				{
					measure();
					fit_view();
				}
				break;

			case SDL_TEXTINPUT:
				send_text(ev.text.text);
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				int key = to_glfw_key(ev.key.keysym.sym);

				if (key == GLFW_KEY_UNKNOWN)
					break;

				// While the iOS keyboard is up, characters arrive as text; a
				// key event for the same letter would type it twice.
				if (SDL_IsTextInputActive() && !is_editing_key(key))
					break;

				if (ev.type == SDL_KEYDOWN && ev.key.repeat && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER))
					break;

				UI::get().send_key(key, ev.type == SDL_KEYDOWN);
				break;
			}

			case SDL_FINGERDOWN:
				controller_mode = false;
				finger_down(ev.tfinger);
				break;

			case SDL_CONTROLLERBUTTONDOWN:
				controller_mode = true;
				break;

			case SDL_CONTROLLERAXISMOTION:
				if (std::abs(ev.caxis.value) > 16000)
					controller_mode = true;
				break;

			case SDL_FINGERMOTION:
				finger_move(ev.tfinger);
				break;

			case SDL_FINGERUP:
				finger_up(ev.tfinger);
				break;

			case SDL_APP_WILLENTERBACKGROUND:
				TouchControls::release_all();
				release_cursor_finger();
				UI::get().send_focus(0);
				break;

			case SDL_APP_DIDENTERBACKGROUND:
				Resume::backgrounded();
				break;

			case SDL_APP_DIDENTERFOREGROUND:
				UI::get().send_focus(1);
				Resume::foregrounded();
				break;
			}
		}

		Gamepad::get().poll();
		TouchControls::set_gamepad(controller_mode && Gamepad::get().is_connected());
	}

	void Window::begin() const
	{
		measure();
		fit_view();

		// The cash shop swaps in a fixed 1024x768 view and back; the renderer
		// has to follow, as the desktop window's re-creation does there.
		static int16_t last_w = 0, last_h = 0;
		int16_t lw = Constants::Constants::get().get_viewwidth();
		int16_t lh = Constants::Constants::get().get_viewheight();

		if (lw != last_w || lh != last_h)
		{
			last_w = lw;
			last_h = lh;
			GraphicsGL::get().reinit();
		}

		glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo);
		glViewport(view_x, panel_h - (view_y + view_h), view_w, view_h);

		GraphicsGL::get().clearscene();
	}

	void Window::end() const
	{
		GraphicsGL::get().flush(opacity);

		TouchControls::set_geometry(panel_w, panel_h, view_x, view_y, view_w, view_h, px_per_pt, safe);
		TouchControls::draw();

		if (!pending_shot.empty())
		{
			write_shot(pending_shot);
			pending_shot.clear();
		}

		SDL_GL_SwapWindow(glwnd);
	}

	void Window::fadeout(float step, std::function<void()> fadeproc)
	{
		opcstep = -step;
		fadeprocedure = fadeproc;
	}

	void Window::setclipboard(const std::string& text) const
	{
		SDL_SetClipboardText(text.c_str());
	}

	std::string Window::getclipboard() const
	{
		char* text = SDL_GetClipboardText();

		if (!text)
			return "";

		std::string result(text);
		SDL_free(text);

		return result;
	}

	void Window::toggle_fullscreen()
	{
	}

	void Window::apply_mouse_speed(int)
	{
	}

	void Window::take_screenshot()
	{
		if (panel_w <= 0 || panel_h <= 0)
			return;

		std::vector<uint8_t> pixels(static_cast<size_t>(panel_w) * panel_h * 4);
		glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, panel_w, panel_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		std::error_code ec;
		std::filesystem::create_directories("screenshots", ec);

		std::time_t t = std::time(nullptr);
		std::tm tm_local{};
		localtime_r(&t, &tm_local);

		char name[64];
		std::strftime(name, sizeof(name), "screenshots/maple_%Y%m%d_%H%M%S.png", &tm_local);

		stbi_flip_vertically_on_write(1);
		stbi_write_png(name, panel_w, panel_h, 4, pixels.data(), panel_w * 4);
		stbi_flip_vertically_on_write(0);
	}
}

#endif
