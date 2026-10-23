//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "IOSPlatform.h"

#if defined(PLATFORM_IOS)

#import <UIKit/UIKit.h>
#include <sys/utsname.h>
#include <string>

namespace ms
{
	namespace IOSPlatform
	{
		namespace
		{
			UIWindow* key_window()
			{
				for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
				{
					if (![scene isKindOfClass:UIWindowScene.class])
						continue;

					for (UIWindow* window in ((UIWindowScene*)scene).windows)
						if (window.isKeyWindow)
							return window;
				}

				return nil;
			}
		}

		void safe_insets(float& left, float& top, float& right, float& bottom)
		{
			left = top = right = bottom = 0.0f;

			if (UIWindow* window = key_window())
			{
				UIEdgeInsets insets = window.safeAreaInsets;
				left = insets.left;
				top = insets.top;
				right = insets.right;
				bottom = insets.bottom;
			}
		}

		float screen_scale()
		{
			if (UIWindow* window = key_window())
				return window.screen.nativeScale;

			return 3.0f;
		}

		void haptic_tap()
		{
			static UIImpactFeedbackGenerator* generator =
				[[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];

			[generator impactOccurred];
		}

		const char* device_name()
		{
			static std::string name;

			if (name.empty())
			{
				struct utsname info;
				uname(&info);
				name = std::string(info.machine) + " iOS "
					+ UIDevice.currentDevice.systemVersion.UTF8String;
			}

			return name.c_str();
		}
	}
}

#endif
