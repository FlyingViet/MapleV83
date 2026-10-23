# iPhone / iPad build of the v83 client in src/client (OpenStory, v83 UI).
#
# The browser build lives in src/client/CMakeLists.txt; this one shares every
# source file with it and swaps in the iOS platform layer from this directory:
# SDL2 for the window, touch, keyboard and controllers, OpenGL ES 3 through the
# same PLATFORM_GLES path the browser uses, AVFoundation audio, embedded fonts.
#
#   platform/iphone/fetch_deps.sh
#   cmake -S src/platform/iphone -B build/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
#         -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake_minimum_required(VERSION 3.20)
project(MapleV83 C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT IOS)
    message(FATAL_ERROR "This project is the iOS build only (-G Xcode -DCMAKE_SYSTEM_NAME=iOS).")
endif()

get_filename_component(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CLIENT_DIR "${SRC_DIR}/client")
set(NLNX_DIR "${SRC_DIR}/nlnx")
set(PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(IOS_DEPS "${PLATFORM_DIR}/deps")

foreach(dep SDL2 freetype asio)
    if(NOT EXISTS "${IOS_DEPS}/${dep}")
        message(FATAL_ERROR "Missing ${IOS_DEPS}/${dep}: run src/platform/iphone/fetch_deps.sh")
    endif()
endforeach()

file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CLIENT_DIR}/*.cpp")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Net/SocketWeb\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Net/SocketWinsock\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/includes/")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/LazyFS/")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/build/")
# Replaced by the iOS layer below.
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Audio/Audio\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/Audio/AudioWeb\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/IO/Window\\.cpp$")

file(GLOB NLNX_SOURCES CONFIGURE_DEPENDS "${NLNX_DIR}/*.cpp")

set(IOS_SOURCES
    ${PLATFORM_DIR}/WindowSDL.cpp
    ${PLATFORM_DIR}/GamepadSDL.cpp
    ${PLATFORM_DIR}/TouchControls.cpp
    ${PLATFORM_DIR}/IOSPlatform.mm
    ${PLATFORM_DIR}/DataDownloadIOS.mm
    ${PLATFORM_DIR}/AudioIOS.mm)
set(IOS_RESOURCES ${PLATFORM_DIR}/LaunchScreen.storyboard ${PLATFORM_DIR}/Assets.xcassets)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
add_subdirectory(${IOS_DEPS}/SDL2 ${CMAKE_BINARY_DIR}/SDL2 EXCLUDE_FROM_ALL)

set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
add_subdirectory(${IOS_DEPS}/freetype ${CMAKE_BINARY_DIR}/freetype EXCLUDE_FROM_ALL)

add_executable(MapleV83 MACOSX_BUNDLE
    ${CLIENT_SOURCES} ${NLNX_SOURCES} ${CLIENT_DIR}/includes/lz4.c
    ${IOS_SOURCES} ${IOS_RESOURCES})
set_source_files_properties(${IOS_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

target_include_directories(MapleV83 PRIVATE
    ${CLIENT_DIR}
    ${CLIENT_DIR}/includes
    ${CLIENT_DIR}/includes/stb
    ${SRC_DIR}
    ${SRC_DIR}/platform/shared
    ${PLATFORM_DIR}
    ${IOS_DEPS}/SDL2/include
    ${IOS_DEPS}/freetype/include
    ${IOS_DEPS}/asio/asio/include)

target_compile_definitions(MapleV83 PRIVATE
    PLATFORM_IOS=1
    PLATFORM_SDL=1
    PLATFORM_GLES=1
    USE_NX=1 USE_ASIO=1 USE_CRYPTO=1
    NLNX_LOG_UI_MISSES
    ASIO_STANDALONE ASIO_HAS_STD_ADDRESSOF ASIO_HAS_STD_ARRAY ASIO_HAS_CSTDINT
    ASIO_HAS_STD_SHARED_PTR ASIO_HAS_STD_TYPE_TRAITS
    GLES_SILENCE_DEPRECATION)

target_compile_options(MapleV83 PRIVATE
    $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-c++11-narrowing>
    -Wno-deprecated-declarations)

target_link_libraries(MapleV83 SDL2main SDL2-static freetype
    "-framework OpenGLES" "-framework QuartzCore" "-framework UIKit"
    "-framework Foundation" "-framework CoreGraphics" "-framework AVFoundation"
    "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreMotion"
    "-framework GameController" "-framework CoreHaptics" "-framework Metal"
    "-framework CoreBluetooth" "-framework CoreFoundation")

set(MAPLE_BUNDLE_ID "com.flyingviet.maplev83" CACHE STRING "")
# Your Apple development team ID (-DMAPLE_TEAM_ID=... or the MV83_TEAM_ID environment variable).
set(MAPLE_TEAM_ID "$ENV{MV83_TEAM_ID}" CACHE STRING "")
set(MAPLE_VERSION "0.2.0" CACHE STRING "")
set(MAPLE_BUILD "1" CACHE STRING "")
set_target_properties(MapleV83 PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${PLATFORM_DIR}/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER ${MAPLE_BUNDLE_ID}
    MACOSX_BUNDLE_BUNDLE_VERSION ${MAPLE_BUILD}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${MAPLE_VERSION}
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${MAPLE_BUNDLE_ID}
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${PLATFORM_DIR}/MapleV83.entitlements"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${MAPLE_TEAM_ID}"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
    XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL[variant=Release] "s"
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
    XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
    XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)")
