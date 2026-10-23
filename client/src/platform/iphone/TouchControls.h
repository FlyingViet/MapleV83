//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
//
// On-screen direction pad and buttons for a phone with nothing but a touchscreen.
//
// The controls float over the game (a phone shows no v83 status bar; its HUD
// is UIStatusBar's top-left corner). They press the same GLFW keys a keyboard
// or gamepad would, through UI::send_key, so the player's key bindings decide
// what each button does.
//
// Coordinates are drawable pixels, origin top-left.
#pragma once

#include <cstdint>

namespace ms
{
	namespace TouchControls
	{
		struct Insets
		{
			float left = 0.0f;
			float top = 0.0f;
			float right = 0.0f;
			float bottom = 0.0f;
		};

		// The drawable, where the scene landed on it, how many drawable
		// pixels make a point, and the safe-area insets in pixels.
		void set_geometry(int panel_w, int panel_h, int fit_x, int fit_y, int fit_w, int fit_h,
			float px_per_pt, Insets insets);

		// A gamepad takes over from the touch controls while it is connected.
		void set_gamepad(bool connected);

		// Whether the controls are on screen right now: in game, no gamepad.
		bool active();

		// Each returns true when the finger belongs to a control and must not
		// also reach the game as a tap.
		bool finger_down(int64_t id, float px, float py);
		bool finger_move(int64_t id, float px, float py);
		bool finger_up(int64_t id, float px, float py);

		// Lets go of every held key - backgrounding, map changes, a gamepad
		// arriving mid-press.
		void release_all();

		// Into whatever framebuffer is bound, after the scene was presented.
		void draw();
	}
}
