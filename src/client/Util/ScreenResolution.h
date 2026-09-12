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

#include "../Configuration.h"

#ifdef _WIN32
#include <windef.h>
#include <WinUser.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace ms
{
	class ScreenResolution
	{
	public:
		ScreenResolution()
		{
#if defined(__EMSCRIPTEN__)
			// The browser's screen is the "desktop": check_events() and the
			// option menu compare the window size against these limits.
			int screen_width = EM_ASM_INT({
				return (window.screen && window.screen.width) ? window.screen.width : 1920;
			});
			int screen_height = EM_ASM_INT({
				return (window.screen && window.screen.height) ? window.screen.height : 1080;
			});

			Configuration::get().set_max_width(static_cast<int16_t>(screen_width));
			Configuration::get().set_max_height(static_cast<int16_t>(screen_height));
#elif !defined(_WIN32)
			// No desktop metrics available: keep Configuration's defaults.
#else
			RECT desktop;

			// Get a handle to the desktop window
			const HWND hDesktop = GetDesktopWindow();

			// Get the size of screen to the variable desktop
			GetWindowRect(hDesktop, &desktop);

			// The top left corner will have coordinates (0, 0) and the bottom right corner will have coordinates (horizontal, vertical)
			Configuration::get().set_max_width(desktop.right);
			Configuration::get().set_max_height(desktop.bottom);
#endif
		}
	};
}