//////////////////////////////////////////////////////////////////////////////////
//	OpenStory2-iOS — GL Compatibility Header
//	Provides OpenGL type definitions for desktop, iOS and Emscripten/WebGL2 builds.
//
//	Platform macros used by the GL layer:
//	  PLATFORM_IOS   iOS build (Xcode). OpenGL ES 3.0 via GLKit, BGRA via the
//	                 APPLE extension.
//	  PLATFORM_GLES  Emscripten/WebGL2 build (defined by src/client/CMakeLists.txt).
//	                 Shares the GLSL ES 3.00 shaders, VAO and indexed-triangle draw
//	                 path with iOS, but WebGL2 has no BGRA/RED upload formats, so
//	                 GraphicsGL.cpp converts to RGBA before glTexSubImage2D.
//	  Neither        desktop OpenGL through GLEW.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#if defined(PLATFORM_IOS)
	#include <OpenGLES/ES3/gl.h>
	#include <OpenGLES/ES3/glext.h>
	// GL_BGRA is available on iOS via GL_APPLE_texture_format_BGRA8888
	#ifndef GL_BGRA
		#define GL_BGRA GL_BGRA_EXT
	#endif
#elif defined(PLATFORM_MACOS_SDL)
	// The native macOS app: OpenGL 4.1 core profile, the same GLSL 3.x shaders
	// as GL ES 3.0 (only the #version line differs) and the same VAO path.
	#ifndef GL_SILENCE_DEPRECATION
		#define GL_SILENCE_DEPRECATION
	#endif
	#include <OpenGL/gl3.h>
	#include <OpenGL/gl3ext.h>
#elif defined(__EMSCRIPTEN__) || defined(PLATFORM_GLES)
	// Core OpenGL ES 3.0 header from the Emscripten sysroot; there is no GLEW.
	#include <GLES3/gl3.h>
	// WebGL2 cannot upload BGRA at all. The token is only defined so shared code
	// compiles; every BGRA upload in GraphicsGL.cpp is swapped to RGBA first.
	#ifndef GL_BGRA
		#define GL_BGRA 0x80E1
	#endif
#else
	#define GLEW_STATIC
	#include <glew.h>
#endif
