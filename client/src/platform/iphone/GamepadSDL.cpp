//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
// GLFW's joystick calls (see GLFWGamepadShim.h) answered from SDL's game
// controller API. Only the first connected controller is used, as on desktop.
#include "../shared/PlatformConfig.h"

#ifdef PLATFORM_SDL

#include "GLFWGamepadShim.h"

#include <SDL.h>

namespace
{
	SDL_GameController* pad = nullptr;
	unsigned char raw_buttons[15];
	float raw_axes[6];

	SDL_GameController* current()
	{
		if (pad && SDL_GameControllerGetAttached(pad))
			return pad;

		if (pad)
		{
			SDL_GameControllerClose(pad);
			pad = nullptr;
		}

		for (int i = 0; i < SDL_NumJoysticks(); i++)
		{
			if (SDL_IsGameController(i))
			{
				pad = SDL_GameControllerOpen(i);

				if (pad)
					break;
			}
		}

		return pad;
	}

	// GLFW's button order, in SDL's names.
	constexpr SDL_GameControllerButton ORDER[15] = {
		SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
		SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
		SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_GUIDE,
		SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
		SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
		SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT
	};

	void read_state(SDL_GameController* c)
	{
		for (int i = 0; i < 15; i++)
			raw_buttons[i] = SDL_GameControllerGetButton(c, ORDER[i]) ? 1 : 0;

		auto axis = [c](SDL_GameControllerAxis a)
		{
			return SDL_GameControllerGetAxis(c, a) / 32767.0f;
		};

		raw_axes[0] = axis(SDL_CONTROLLER_AXIS_LEFTX);
		raw_axes[1] = axis(SDL_CONTROLLER_AXIS_LEFTY);
		raw_axes[2] = axis(SDL_CONTROLLER_AXIS_RIGHTX);
		raw_axes[3] = axis(SDL_CONTROLLER_AXIS_RIGHTY);
		// GLFW triggers run -1..1, SDL's 0..1.
		raw_axes[4] = axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) * 2.0f - 1.0f;
		raw_axes[5] = axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) * 2.0f - 1.0f;
	}
}

int glfwJoystickPresent(int jid)
{
	return jid == GLFW_JOYSTICK_1 && current() ? GLFW_TRUE : GLFW_FALSE;
}

const char* glfwGetJoystickName(int jid)
{
	SDL_GameController* c = jid == GLFW_JOYSTICK_1 ? current() : nullptr;
	return c ? SDL_GameControllerName(c) : nullptr;
}

int glfwJoystickIsGamepad(int jid)
{
	return glfwJoystickPresent(jid);
}

int glfwGetGamepadState(int jid, GLFWgamepadstate* state)
{
	SDL_GameController* c = jid == GLFW_JOYSTICK_1 ? current() : nullptr;

	if (!c || !state)
		return GLFW_FALSE;

	read_state(c);

	for (int i = 0; i < 15; i++)
		state->buttons[i] = raw_buttons[i];

	for (int i = 0; i < 6; i++)
		state->axes[i] = raw_axes[i];

	return GLFW_TRUE;
}

const unsigned char* glfwGetJoystickButtons(int jid, int* count)
{
	SDL_GameController* c = jid == GLFW_JOYSTICK_1 ? current() : nullptr;

	if (!c)
	{
		*count = 0;
		return nullptr;
	}

	read_state(c);
	*count = 15;
	return raw_buttons;
}

const float* glfwGetJoystickAxes(int jid, int* count)
{
	SDL_GameController* c = jid == GLFW_JOYSTICK_1 ? current() : nullptr;

	if (!c)
	{
		*count = 0;
		return nullptr;
	}

	read_state(c);
	*count = 6;
	return raw_axes;
}

#endif
