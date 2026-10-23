//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
// The few UIKit facts SDL2 does not expose.
#pragma once

#include "../shared/PlatformConfig.h"

#if defined(PLATFORM_IOS)

namespace ms
{
	namespace IOSPlatform
	{
		// Safe-area insets of the key window, in points.
		void safe_insets(float& left, float& top, float& right, float& bottom);

		// Drawable pixels per point.
		float screen_scale();

		// A light tap from the Taptic Engine.
		void haptic_tap();

		// A short device description for bug reports, e.g. "iPhone17,2 iOS 26.1".
		const char* device_name();
	}
}

#endif
