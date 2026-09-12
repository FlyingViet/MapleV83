## CMake file for the OpenStory web client (Emscripten / WebGL2) ##
##
## Lives at src/client/CMakeLists.txt. scripts/build_wasm.sh runs
##   emcmake cmake <repo>/src/client && emmake make
## from <repo>/build, which produces build/JourneyClient.js + .wasm, the files
## web/index.html loads.
##
## Sources: every .cpp under src/client (minus the native socket backends and
## the vendored includes/ tree) + src/nlnx/*.cpp + includes/lz4.c.
## Fonts are baked into Graphics/EmbeddedFonts.cpp, so nothing is --embed-file'd.
cmake_minimum_required(VERSION 3.13)
project(JourneyClient CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG_SYMBOLS "Enable debug symbols (-g / source maps)" OFF)
option(CLIENT_COMPILE_STB_VORBIS "Also compile includes/stb/stb_vorbis.c (only if Audio/AudioWeb.cpp uses it header-only)" OFF)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "This CMakeLists.txt is the Emscripten build only. Run it through emcmake "
        "(scripts/build_wasm.sh). The native client uses OpenStory's Visual Studio project.")
endif()

set(CLIENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")            # src/client
get_filename_component(SRC_DIR "${CLIENT_DIR}/.." ABSOLUTE) # src
set(NLNX_DIR "${SRC_DIR}/nlnx")                            # src/nlnx
set(PLATFORM_SHARED_DIR "${SRC_DIR}/platform/shared")      # src/platform/shared

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CLIENT_DIR}/*.cpp")

# Native socket backends: the web build links Net/SocketWeb.cpp instead.
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Net/SocketAsio\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Net/SocketWinsock\\.cpp$")
# Vendored header libraries (stb ships test .cpp files) and any in-tree build dir.
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/includes/")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/build/")

# Audio/Audio.cpp is the BASS backend (#include <bass.h>, unguarded). When the
# web audio backend is present it replaces it outright; otherwise keep the
# BASS file in the list so the missing-header error points at the real gap.
if(EXISTS "${CLIENT_DIR}/Audio/AudioWeb.cpp")
    list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Audio/Audio\\.cpp$")
    message(STATUS "Audio: using Audio/AudioWeb.cpp (Audio/Audio.cpp excluded)")
else()
    message(WARNING "Audio/AudioWeb.cpp not found: Audio/Audio.cpp (BASS) will not compile under Emscripten")
endif()

file(GLOB NLNX_SOURCES CONFIGURE_DEPENDS "${NLNX_DIR}/*.cpp")

set(THIRD_PARTY_SOURCES "${CLIENT_DIR}/includes/lz4.c")
if(CLIENT_COMPILE_STB_VORBIS)
    list(APPEND THIRD_PARTY_SOURCES "${CLIENT_DIR}/includes/stb/stb_vorbis.c")
endif()

add_executable(JourneyClient
    ${CLIENT_SOURCES}
    ${NLNX_SOURCES}
    ${THIRD_PARTY_SOURCES}
)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(JourneyClient PRIVATE
    "${CLIENT_DIR}"               # "Graphics/GraphicsGL.h" style relative includes
    "${CLIENT_DIR}/includes"      # lz4.h
    "${SRC_DIR}"                  # <nlnx/nx.hpp> -> src/nlnx/nx.hpp
    "${PLATFORM_SHARED_DIR}"      # GLCompat.h, KeyCodes.h, PlatformConfig.h
    "${CLIENT_DIR}/includes/stb"  # <stb_image.h>, <stb_image_write.h>
)

# ---------------------------------------------------------------------------
# Defines
# ---------------------------------------------------------------------------
target_compile_definitions(JourneyClient PRIVATE
    MS_PLATFORM_WASM=1   # Window/Gamepad/Util/Session/NxFiles web paths
    PLATFORM_GLES=1      # GraphicsGL: GLSL ES 3.00 shaders, VAO, indexed draws, RGBA uploads
    USE_NX               # NX data files (MapleStory.h guards its own #define)
    USE_CRYPTO           # packet encryption (MapleStory.h guards its own #define)
    GLFW_INCLUDE_NONE    # <GLFW/glfw3.h> must not drag in GL/gl.h next to GLES3/gl3.h
)
# USE_ASIO is deliberately NOT defined: Session.h then falls through to the
# web socket backend (see MapleStory.h, which also #undefs it for this build).

# ---------------------------------------------------------------------------
# Compile flags
# ---------------------------------------------------------------------------
target_compile_options(JourneyClient PRIVATE
    "-fmacro-prefix-map=${SRC_DIR}/=/"
    --use-port=freetype
    -fexceptions
    -Wno-c++11-narrowing
    -Wno-deprecated-declarations
    -Wno-unused-variable
    -Wno-unused-but-set-variable
)

if(DEBUG_SYMBOLS)
    target_compile_options(JourneyClient PRIVATE -g)
endif()

# ---------------------------------------------------------------------------
# Link flags (Emscripten -s settings are linker settings)
# ---------------------------------------------------------------------------
target_link_options(JourneyClient PRIVATE
    -sUSE_GLFW=3
    -sUSE_WEBGL2=1
    # The GLSL ES 3.00 shaders need a WebGL2 context, never a WebGL1 fallback.
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sFULL_ES3=1
    -sWASM=1
    -sALLOW_MEMORY_GROWTH=1
    -sFORCE_FILESYSTEM=1
    -fexceptions
    -sUSE_ZLIB=1
    --use-port=freetype
    -lwebsocket.js
    -sASYNCIFY=1
    "-sASYNCIFY_IMPORTS=['emscripten_asm_const_int','emscripten_asm_const_double','emscripten_sleep']"
    -sASYNCIFY_STACK_SIZE=16777216
    -sSTACK_SIZE=16777216
    -sINITIAL_MEMORY=268435456
    -sEXPORTED_RUNTIME_METHODS=FS,ccall,cwrap,setCanvasSize
    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_mw_key
    "--pre-js=${CLIENT_DIR}/LazyFS/lazyfs.js"
    -sASSERTIONS=1
)

# Relink when the pre-js changes.
set_property(TARGET JourneyClient APPEND PROPERTY LINK_DEPENDS "${CLIENT_DIR}/LazyFS/lazyfs.js")

if(DEBUG_SYMBOLS)
    target_link_options(JourneyClient PRIVATE -g3 -gsource-map)
endif()
