//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
// ms::Window for the native macOS app, on SDL2 with an OpenGL 4.1 core context.
//
// Mouse and keyboard work as on the PC client. The logical view is v83's 600
// pixels high and as wide as the window's shape when the game starts; the
// window can then be resized or made full screen (green button, or Alt+Enter)
// and the view is scaled to fit with bars where the shape differs. Rendering is
// at the full Retina resolution.
#include "../shared/PlatformConfig.h"

#ifdef PLATFORM_MACOS_SDL

#include "IO/Window.h"
#include "IO/UI.h"
#include "IO/Gamepad.h"
#include "Net/Resume.h"

#include "Configuration.h"
#include "Constants.h"
#include "Graphics/GraphicsGL.h"

#include <SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <vector>

namespace ms
{
	namespace
	{
		// The window's drawable size in pixels, and pixels per window point.
		int panel_w = 0;
		int panel_h = 0;
		float px_per_pt = 1.0f;

		// Where the game's logical view lands in the drawable, top-left origin.
		int view_x = 0, view_y = 0, view_w = 0, view_h = 0;

		bool left_held = false;
		float scroll_accum = 0.0f;

		// v83's native height; the width follows the window's shape.
		constexpr float LOGICAL_HEIGHT = 600.0f;
		constexpr float WINDOW_ASPECT = 1.6f;

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
		}

		void fit_view()
		{
			int16_t lw = Constants::Constants::get().get_viewwidth();
			int16_t lh = Constants::Constants::get().get_viewheight();

			if (lw <= 0 || lh <= 0 || panel_w <= 0 || panel_h <= 0)
				return;

			float scale = std::min(static_cast<float>(panel_w) / lw, static_cast<float>(panel_h) / lh);
			view_w = static_cast<int>(lw * scale + 0.5f);
			view_h = static_cast<int>(lh * scale + 0.5f);
			view_x = (panel_w - view_w) / 2;
			view_y = (panel_h - view_h) / 2;
		}

		// Window points (SDL mouse coordinates) to the game's logical pixels.
		Point<int16_t> to_logical(float x_pt, float y_pt)
		{
			int16_t lw = Constants::Constants::get().get_viewwidth();
			int16_t lh = Constants::Constants::get().get_viewheight();

			if (view_w <= 0 || view_h <= 0)
				return Point<int16_t>(0, 0);

			float x = (x_pt * px_per_pt - view_x) * lw / view_w;
			float y = (y_pt * px_per_pt - view_y) * lh / view_h;

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
			case SDLK_CAPSLOCK: return GLFW_KEY_CAPS_LOCK;
			case SDLK_SCROLLLOCK: return GLFW_KEY_SCROLL_LOCK;
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
		// through the keycode path while text input supplies the characters.
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
			case GLFW_KEY_LEFT_CONTROL:
			case GLFW_KEY_RIGHT_CONTROL:
			case GLFW_KEY_LEFT_SHIFT:
			case GLFW_KEY_RIGHT_SHIFT:
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

		// Text input (and with it the IME) is on exactly while a text field has
		// focus; otherwise letters are game keys.
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
					rect.h = static_cast<int>(std::max<int>(bounds.height(), 14) * view_h / lh / px_per_pt);
					SDL_SetTextInputRect(&rect);
				}
			}

			if (wants_text && !SDL_IsTextInputActive())
				SDL_StartTextInput();
			else if (!wants_text && SDL_IsTextInputActive())
				SDL_StopTextInput();
		}

		// Cmd+C/V/X/A act as the game's Ctrl shortcuts, as Mac users expect.
		bool send_command_shortcut(int key)
		{
			if (key != GLFW_KEY_C && key != GLFW_KEY_V && key != GLFW_KEY_X && key != GLFW_KEY_A)
				return false;

			UI::get().send_key(GLFW_KEY_LEFT_CONTROL, true);
			UI::get().send_key(key, true);
			UI::get().send_key(key, false);
			UI::get().send_key(GLFW_KEY_LEFT_CONTROL, false);
			return true;
		}

		void write_png(const std::string& path)
		{
			if (panel_w <= 0 || panel_h <= 0)
				return;

			std::vector<uint8_t> pixels(static_cast<size_t>(panel_w) * panel_h * 4);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glReadPixels(0, 0, panel_w, panel_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

			std::error_code ec;
			std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

			stbi_flip_vertically_on_write(1);
			stbi_write_png(path.c_str(), panel_w, panel_h, 4, pixels.data(), panel_w * 4);
			stbi_flip_vertically_on_write(0);
		}

		// ---- Scripted input, for testing without a hand on the mouse ----
		//
		// If autotest.txt exists in the data folder at launch it is run once
		// (then renamed to autotest.done). One command per line:
		//   wait <ms>        click <x> <y>     dclick <x> <y>    rclick <x> <y>
		//   move <x> <y>     text <chars>      key <name>        press <name> <ms>
		//   shot <name>      quit
		// x/y are the game's logical pixels; key names as on iOS (enter esc tab
		// bksp space left right up down ctrl alt shift, letters, digits, f1-f12).
		std::vector<std::vector<std::string>> script;
		size_t script_pos = 0;
		Uint32 script_resume = 0;
		std::string pending_shot;
		bool script_loaded = false;
		bool script_quit = false;
		std::vector<std::pair<Uint32, std::function<void()>>> timed;

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

				std::vector<std::string> args;
				std::istringstream words(line);
				std::string word;
				words >> word;
				args.push_back(word);

				if (word == "text")
				{
					std::string rest;
					std::getline(words, rest);

					if (!rest.empty() && rest[0] == ' ')
						rest.erase(0, 1);

					args.push_back(rest);
				}
				else
				{
					while (words >> word)
						args.push_back(word);
				}

				script.push_back(args);
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

		void run_script()
		{
			if (!script_loaded)
				load_script();

			Uint32 now = SDL_GetTicks();

			for (size_t i = 0; i < timed.size();)
			{
				if (now >= timed[i].first)
				{
					auto action = timed[i].second;
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
				const std::vector<std::string>& a = script[script_pos++];
				const std::string& cmd = a[0];
				auto num = [&](size_t i) { return i < a.size() ? std::atoi(a[i].c_str()) : 0; };
				auto at = [&]() { return Point<int16_t>(static_cast<int16_t>(num(1)), static_cast<int16_t>(num(2))); };

				std::cout << "[Test] " << cmd;
				for (size_t i = 1; i < a.size(); i++)
					std::cout << " " << a[i];
				std::cout << std::endl;

				if (cmd == "wait")
				{
					script_resume = now + num(1);
				}
				else if (cmd == "move")
				{
					UI::get().send_cursor(at());
				}
				else if (cmd == "click" || cmd == "dclick")
				{
					// Spread over frames like a real click: hover, press, release.
					Point<int16_t> pos = at();
					bool twice = cmd == "dclick";
					UI::get().send_cursor(pos);
					timed.push_back({ now + 50, [] { UI::get().send_cursor(true); } });
					timed.push_back({ now + 130, [twice] {
						if (twice)
							UI::get().doubleclick();

						UI::get().send_cursor(false);
					} });
					script_resume = now + 200;
				}
				else if (cmd == "rclick")
				{
					UI::get().send_cursor(at());
					UI::get().rightclick();
				}
				else if (cmd == "text")
				{
					send_text(a.size() > 1 ? a[1].c_str() : "");
				}
				else if (cmd == "key")
				{
					int key = key_named(a.size() > 1 ? a[1] : "");
					UI::get().send_key(key, true);
					UI::get().send_key(key, false);
				}
				else if (cmd == "press")
				{
					int key = key_named(a.size() > 1 ? a[1] : "");
					UI::get().send_key(key, true);
					timed.push_back({ now + static_cast<Uint32>(num(2)), [key] { UI::get().send_key(key, false); } });
				}
				else if (cmd == "shot")
				{
					pending_shot = a.size() > 1 ? a[1] : "shot";
				}
				else if (cmd == "suspend")
				{
					// Freeze the whole app like iOS does in the background.
					Resume::backgrounded();
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(num(1))));
					Resume::foregrounded();
				}
				else if (cmd == "quit")
				{
					script_quit = true;
				}
			}
		}
	}

	Window::Window()
	{
		glwnd = nullptr;
		context = nullptr;
		closed = false;
		fullscreen = false;
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
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
		{
			std::cout << "[Init] SDL_Init failed: " << SDL_GetError() << std::endl;
			return Error::Code::GLFW;
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

		// A 16:10 window (a MacBook's full-screen shape below the notch) at
		// most 85% of the usable desktop.
		SDL_Rect usable{ 0, 0, 1440, 900 };
		SDL_GetDisplayUsableBounds(0, &usable);

		int win_h = static_cast<int>(std::min(usable.h * 0.85f, usable.w * 0.85f / WINDOW_ASPECT));
		int win_w = static_cast<int>(win_h * WINDOW_ASPECT);

		fullscreen = Setting<Fullscreen>::get().load();

		glwnd = SDL_CreateWindow(Configuration::get().get_title().c_str(),
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
			| (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));

		if (!glwnd)
		{
			std::cout << "[Init] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
			return Error::Code::WINDOW;
		}

		SDL_SetWindowMinimumSize(glwnd, 640, 400);

		context = SDL_GL_CreateContext(glwnd);

		if (!context)
		{
			std::cout << "[Init] SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
			return Error::Code::WINDOW;
		}

		SDL_GL_SetSwapInterval(1);
		SDL_StopTextInput();

		for (int i = 0; i < 10; i++)
			SDL_PumpEvents();

		measure();

		// The logical view keeps the window's shape at a height of 600; later
		// resizes scale it.
		float ui_scale = std::max(1.0f, panel_h / LOGICAL_HEIGHT);
		Constants::Constants::get().set_ui_scale(ui_scale);
		Constants::Constants::get().set_viewwidth(static_cast<int16_t>(panel_w));
		Constants::Constants::get().set_viewheight(static_cast<int16_t>(panel_h));

		// SetFieldHandler re-reads these on every map load.
		Setting<Width>::get().save(static_cast<uint16_t>(panel_w));
		Setting<Height>::get().save(static_cast<uint16_t>(panel_h));

		width = static_cast<int16_t>(panel_w);
		height = static_cast<int16_t>(panel_h);

		std::cout << "[Init] Window " << win_w << "x" << win_h << " pt, drawable " << panel_w << "x" << panel_h
			<< " px, UI scale " << ui_scale << ", logical view " << Constants::Constants::get().get_viewwidth()
			<< "x" << Constants::Constants::get().get_viewheight() << ", GL "
			<< reinterpret_cast<const char*>(glGetString(GL_VERSION)) << std::endl;

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

		if (script_quit)
			closed = true;

		SDL_Event ev;

		while (SDL_PollEvent(&ev))
		{
			switch (ev.type)
			{
			case SDL_QUIT:
				closed = true;
				break;

			case SDL_WINDOWEVENT:
				switch (ev.window.event)
				{
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				case SDL_WINDOWEVENT_RESIZED:
					measure();
					fit_view();
					break;
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					UI::get().send_focus(1);
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					left_held = false;
					UI::get().send_focus(0);
					break;
				case SDL_WINDOWEVENT_ENTER:
					// The game draws its own cursor inside the window.
					SDL_ShowCursor(SDL_DISABLE);
					break;
				case SDL_WINDOWEVENT_LEAVE:
					SDL_ShowCursor(SDL_ENABLE);
					break;
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

				if (ev.key.keysym.mod & KMOD_GUI)
				{
					if (ev.type == SDL_KEYDOWN && !ev.key.repeat)
						send_command_shortcut(key);

					break;
				}

				// While a text field has focus, characters arrive as text; a key
				// event for the same letter would type it twice.
				if (SDL_IsTextInputActive() && !is_editing_key(key) && !(ev.key.keysym.mod & KMOD_CTRL))
					break;

				if (ev.type == SDL_KEYDOWN && ev.key.repeat && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER))
					break;

				UI::get().send_key(key, ev.type == SDL_KEYDOWN);
				break;
			}

			case SDL_MOUSEMOTION:
			{
				Point<int16_t> pos = to_logical(static_cast<float>(ev.motion.x), static_cast<float>(ev.motion.y));

				if (left_held)
					UI::get().send_cursor_drag(pos);
				else
					UI::get().send_cursor(pos);

				break;
			}

			case SDL_MOUSEBUTTONDOWN:
				UI::get().send_cursor(to_logical(static_cast<float>(ev.button.x), static_cast<float>(ev.button.y)));

				if (ev.button.button == SDL_BUTTON_LEFT)
				{
					left_held = true;
					UI::get().send_cursor(true);
				}
				else if (ev.button.button == SDL_BUTTON_RIGHT)
				{
					UI::get().rightclick();
				}
				break;

			case SDL_MOUSEBUTTONUP:
				if (ev.button.button == SDL_BUTTON_LEFT)
				{
					left_held = false;

					if (ev.button.clicks == 2)
						UI::get().doubleclick();

					UI::get().send_cursor(false);
				}
				break;

			case SDL_MOUSEWHEEL:
			{
				float dy = ev.wheel.preciseY;

				if (ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
					dy = -dy;

				scroll_accum += dy;

				while (std::abs(scroll_accum) >= 1.0f)
				{
					float step = scroll_accum > 0.0f ? 1.0f : -1.0f;
					UI::get().send_scroll(step);
					scroll_accum -= step;
				}
				break;
			}
			}
		}

		Gamepad::get().poll();
	}

	void Window::begin() const
	{
		measure();
		fit_view();

		// The cash shop swaps in a fixed 1024x768 view and back; the renderer
		// has to follow.
		static int16_t last_w = 0, last_h = 0;
		int16_t lw = Constants::Constants::get().get_viewwidth();
		int16_t lh = Constants::Constants::get().get_viewheight();

		if (lw != last_w || lh != last_h)
		{
			last_w = lw;
			last_h = lh;
			GraphicsGL::get().reinit();
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, panel_w, panel_h);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(view_x, panel_h - (view_y + view_h), view_w, view_h);

		GraphicsGL::get().clearscene();
	}

	void Window::end() const
	{
		GraphicsGL::get().flush(opacity);

		if (!pending_shot.empty())
		{
			write_png("shots/" + pending_shot + ".png");
			std::cout << "[Test] wrote shots/" << pending_shot << ".png" << std::endl;
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
		fullscreen = !fullscreen;
		SDL_SetWindowFullscreen(glwnd, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
		Setting<Fullscreen>::get().save(fullscreen);
	}

	void Window::apply_mouse_speed(int)
	{
	}

	void Window::take_screenshot()
	{
		std::time_t t = std::time(nullptr);
		std::tm tm_local{};
		localtime_r(&t, &tm_local);

		char name[64];
		std::strftime(name, sizeof(name), "screenshots/maple_%Y%m%d_%H%M%S.png", &tm_local);
		write_png(name);
	}
}

#endif
