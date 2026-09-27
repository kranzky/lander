cmake_minimum_required(VERSION 3.16)
project(lander VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to an optimized build; the game is unplayably slow without one
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
    # World coordinates are 8.24 fixed point that wrap every 256 tiles, relying
    # on 32-bit two's complement overflow exactly as the original ARM code did.
    # Make that overflow well-defined rather than undefined behaviour.
    add_compile_options(-fwrapv)
endif()

find_package(SDL2 REQUIRED)

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
target_link_libraries(lander_core PUBLIC ${SDL2_LIBRARIES})

add_executable(lander src/main.cpp)
target_link_libraries(lander PRIVATE lander_core)

if(APPLE)
    target_link_libraries(lander PRIVATE "-framework Cocoa")
endif()

if(WIN32)
    # Version information shown in Explorer and by code signing tools
    enable_language(RC)
    configure_file(${CMAKE_SOURCE_DIR}/windows/lander.rc.in
                   ${CMAKE_BINARY_DIR}/generated/lander.rc @ONLY)
    target_sources(lander PRIVATE ${CMAKE_BINARY_DIR}/generated/lander.rc)
endif()

if(MSVC)
    # Windows: Copy SDL2.dll to output directory
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
