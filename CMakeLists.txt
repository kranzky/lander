cmake_minimum_required(VERSION 3.16)

# Release builds build SDL2 from source and link it statically, so the game
# needs nothing installed: a single exe on Windows, a self-contained app on
# macOS. Development builds can use an installed SDL2 (e.g. Homebrew's).
option(LANDER_VENDOR_SDL2 "Build SDL2 from source and link it statically" OFF)

# Self-contained macOS builds run on macOS 11 and later (this must be set
# before project() to take effect)
if(APPLE AND LANDER_VENDOR_SDL2 AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(lander VERSION 1.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to an optimized build; the game is unplayably slow without one
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Link the C/C++ runtime statically, so players don't need the Visual C++
# redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(LANDER_VENDOR_SDL2)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_FORCE_STATIC_VCRT ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
        URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    # Before our warning flags, which are for our own code
    FetchContent_MakeAvailable(SDL2)
    set(LANDER_SDL2_LIBRARIES SDL2::SDL2main SDL2::SDL2)
else()
    find_package(SDL2 REQUIRED)
    set(LANDER_SDL2_LIBRARIES ${SDL2_LIBRARIES})
endif()

if(MSVC)
    add_compile_options(/W3)
    # M_PI in <cmath>, and portable C stdio without "unsafe" warnings
    add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
    # World coordinates are 8.24 fixed point that wrap every 256 tiles, relying
    # on 32-bit two's complement overflow exactly as the original ARM code did.
    # Make that overflow well-defined rather than undefined behaviour.
    add_compile_options(-fwrapv)
endif()

# =============================================================================
# Game
# =============================================================================

# Everything except main(), shared by the game and the tests
add_library(lander_core STATIC
    src/screen.cpp
    src/palette.cpp
    src/projection.cpp
    src/math3d.cpp
    src/lookup_tables.cpp
    src/landscape.cpp
    src/landscape_renderer.cpp
    src/camera.cpp
    src/player.cpp
    src/polar_coords.cpp
    src/object3d.cpp
    src/object_renderer.cpp
    src/particles.cpp
    src/object_map.cpp
    src/graphics_buffer.cpp
    src/scale.cpp
    src/sound.cpp
    src/clipping.cpp
    src/settings.cpp
)
target_include_directories(lander_core PUBLIC src ${SDL2_INCLUDE_DIRS})
target_link_libraries(lander_core PUBLIC ${LANDER_SDL2_LIBRARIES})

add_executable(lander src/main.cpp)
target_link_libraries(lander PRIVATE lander_core)

if(APPLE)
    target_link_libraries(lander PRIVATE "-framework Cocoa")
endif()

if(WIN32)
    # A windowed app, with no console (SDL2main provides WinMain)
    set_target_properties(lander PROPERTIES WIN32_EXECUTABLE TRUE)

    # Version information shown in Explorer and by code signing tools
    enable_language(RC)
    configure_file(${CMAKE_SOURCE_DIR}/windows/lander.rc.in
                   ${CMAKE_BINARY_DIR}/generated/lander.rc @ONLY)
    target_sources(lander PRIVATE ${CMAKE_BINARY_DIR}/generated/lander.rc)
endif()

if(MSVC AND NOT LANDER_VENDOR_SDL2)
    # Windows with an installed SDL2: copy SDL2.dll to the output directory
    add_custom_command(TARGET lander POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:lander>
    )
endif()

if(MINGW)
    # Static link libgcc and libstdc++ to avoid DLL dependencies
    target_link_options(lander PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Copy sounds directory to build output
add_custom_command(TARGET lander POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/sounds
    $<TARGET_FILE_DIR:lander>/sounds
)

# =============================================================================
# Tests
# =============================================================================

enable_testing()

set(TESTS
    test_fixed
    test_lookup_tables
    test_math3d
    test_screen
    test_palette
    test_projection
    test_landscape
    test_polar_coords
    test_object3d
    test_object_renderer
    test_particles
    test_object_map
    test_graphics_buffer
    test_player
)

foreach(name ${TESTS})
    add_executable(${name} test/${name}.cpp)
    target_link_libraries(${name} PRIVATE lander_core)
    # Tests use assert(), so keep it enabled in optimized builds
    target_compile_options(${name} PRIVATE -UNDEBUG)
    add_test(NAME ${name} COMMAND ${name})
endforeach()
