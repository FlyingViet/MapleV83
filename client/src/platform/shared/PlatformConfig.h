//////////////////////////////////////////////////////////////////////////////////
//	OpenStory2-iOS — Platform Configuration
//	Central platform detection and feature flags.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

// Platform detection
#if defined(__APPLE__)
	#include <TargetConditionals.h>
	#if TARGET_OS_IPHONE
		#define PLATFORM_IOS 1
	#else
		#define PLATFORM_MACOS 1
	#endif
#elif defined(_WIN32)
	#define PLATFORM_WINDOWS 1
#else
	#define PLATFORM_LINUX 1
#endif

// Feature flags for iOS
// The SDL2 + OpenGL (ES) client: the iPhone app, and the native macOS app
// (src/platform/macos, which defines PLATFORM_MACOS_SDL).
#if defined(PLATFORM_IOS) || defined(PLATFORM_MACOS_SDL)
	#ifndef PLATFORM_SDL
		#define PLATFORM_SDL 1
	#endif
#endif

#ifdef PLATFORM_SDL
	#define USE_NX 1
	#define USE_ASIO 1
	#define USE_CRYPTO 1
#endif
