# Native macOS (Apple Silicon) build of the v83 client in src/client (OpenStory, v83 UI).
#
# Shares every client source file and the SDL2 / FreeType / asio deps with the
# iPhone build (src/platform/iphone). This directory only swaps in the window:
# mouse and keyboard, a resizable Retina window, OpenGL 4.1 core.
#
#   src/platform/iphone/fetch_deps.sh
#   cmake -S src/platform/macos -B build/mac -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/mac          (or ~/Coding/MapleV83/tools/build_mac.sh)
cmake_minimum_required(VERSION 3.20)
project(MapleV83Mac C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")

if(NOT APPLE OR IOS)
    message(FATAL_ERROR "This project is the macOS build only.")
endif()

get_filename_component(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CLIENT_DIR "${SRC_DIR}/client")
set(NLNX_DIR "${SRC_DIR}/nlnx")
set(PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SHARED_SDL_DIR "${SRC_DIR}/platform/iphone")
set(DEPS "${SHARED_SDL_DIR}/deps")

foreach(dep SDL2 freetype asio)
    if(NOT EXISTS "${DEPS}/${dep}")
        message(FATAL_ERROR "Missing ${DEPS}/${dep}: run src/platform/iphone/fetch_deps.sh")
    endif()
endforeach()

file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CLIENT_DIR}/*.cpp")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Net/SocketWeb\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Net/SocketWinsock\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/includes/")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/LazyFS/")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/build/")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Audio/Audio\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Audio/AudioWeb\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/IO/Window\\.cpp$")

file(GLOB NLNX_SOURCES CONFIGURE_DEPENDS "${NLNX_DIR}/*.cpp")

set(MAC_SOURCES
    ${PLATFORM_DIR}/WindowMac.cpp
    ${SHARED_SDL_DIR}/GamepadSDL.cpp
    ${SHARED_SDL_DIR}/AudioIOS.mm
    ${SHARED_SDL_DIR}/DataDownloadIOS.mm)
set(MAC_ICON ${PLATFORM_DIR}/AppIcon.icns)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/SDL2 ${CMAKE_BINARY_DIR}/SDL2 EXCLUDE_FROM_ALL)

set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
add_subdirectory(${DEPS}/freetype ${CMAKE_BINARY_DIR}/freetype EXCLUDE_FROM_ALL)

add_executable(MapleV83 MACOSX_BUNDLE
    ${CLIENT_SOURCES} ${NLNX_SOURCES} ${CLIENT_DIR}/includes/lz4.c
    ${MAC_SOURCES} ${MAC_ICON})
set_source_files_properties(${MAC_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

target_include_directories(MapleV83 PRIVATE
    ${CLIENT_DIR}
    ${CLIENT_DIR}/includes
    ${CLIENT_DIR}/includes/stb
    ${SRC_DIR}
    ${SRC_DIR}/platform/shared
    ${SHARED_SDL_DIR}
    ${DEPS}/SDL2/include
    ${DEPS}/freetype/include
    ${DEPS}/asio/asio/include)

target_compile_definitions(MapleV83 PRIVATE
    PLATFORM_MACOS_SDL=1
    PLATFORM_SDL=1
    PLATFORM_GLES=1
    USE_NX=1 USE_ASIO=1 USE_CRYPTO=1
    ASIO_STANDALONE ASIO_HAS_STD_ADDRESSOF ASIO_HAS_STD_ARRAY ASIO_HAS_CSTDINT
    ASIO_HAS_STD_SHARED_PTR ASIO_HAS_STD_TYPE_TRAITS
    GL_SILENCE_DEPRECATION)

target_compile_options(MapleV83 PRIVATE
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-c++11-narrowing>
    $<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-arc>
    -Wno-deprecated-declarations)

target_link_libraries(MapleV83 SDL2-static freetype iconv
    "-framework Cocoa" "-framework OpenGL" "-framework IOKit" "-framework CoreVideo"
    "-framework Carbon" "-framework ForceFeedback" "-framework GameController"
    "-framework CoreHaptics" "-framework Metal" "-framework QuartzCore"
    "-framework AVFoundation" "-framework AudioToolbox" "-framework CoreAudio"
    "-framework Foundation" "-framework CoreFoundation" "-framework UniformTypeIdentifiers")

set(MAPLE_BUNDLE_ID "com.flyingviet.maplev83.mac" CACHE STRING "")
set(MAPLE_VERSION "0.3.0" CACHE STRING "")
set(MAPLE_BUILD "1" CACHE STRING "")
set_target_properties(MapleV83 PROPERTIES
    OUTPUT_NAME "Maple v83"
    MACOSX_BUNDLE_INFO_PLIST ${PLATFORM_DIR}/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER ${MAPLE_BUNDLE_ID}
    MACOSX_BUNDLE_BUNDLE_NAME "Maple v83"
    MACOSX_BUNDLE_BUNDLE_VERSION ${MAPLE_BUILD}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${MAPLE_VERSION}
    MACOSX_BUNDLE_ICON_FILE AppIcon)
