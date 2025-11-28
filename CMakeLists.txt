cmake_minimum_required(VERSION 3.16)
project(lander VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SDL2
find_package(SDL2 REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
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
)

# Create executable
add_executable(lander ${SOURCES})

# Include directories
target_include_directories(lander PRIVATE
    ${SDL2_INCLUDE_DIRS}
    src
)

# Link libraries
target_link_libraries(lander PRIVATE ${SDL2_LIBRARIES})

# Platform-specific settings
if(APPLE)
    # macOS: SDL2 from Homebrew or Framework
    target_link_libraries(lander PRIVATE "-framework Cocoa")
endif()

if(MSVC)
    # Windows: Copy SDL2.dll to output directory
    add_custom_command(TARGET lander POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:lander>
    )
endif()

# =============================================================================
# Tests
# =============================================================================

# Test for fixed-point math
add_executable(test_fixed test/test_fixed.cpp)
target_include_directories(test_fixed PRIVATE src)

# Test for lookup tables
add_executable(test_lookup_tables
    test/test_lookup_tables.cpp
    src/lookup_tables.cpp
)
target_include_directories(test_lookup_tables PRIVATE src)

# Test for 3D math (vectors and matrices)
add_executable(test_math3d
    test/test_math3d.cpp
    src/math3d.cpp
    src/lookup_tables.cpp
)
target_include_directories(test_math3d PRIVATE src)

# Test for screen buffer
add_executable(test_screen
    test/test_screen.cpp
    src/screen.cpp
)
target_include_directories(test_screen PRIVATE src)

# Test for color palette
add_executable(test_palette
    test/test_palette.cpp
    src/palette.cpp
)
target_include_directories(test_palette PRIVATE src)

# Test for 3D projection
add_executable(test_projection
    test/test_projection.cpp
    src/projection.cpp
    src/math3d.cpp
    src/lookup_tables.cpp
    src/screen.cpp
    src/camera.cpp
)
target_include_directories(test_projection PRIVATE src)

# Test for landscape altitude generation
add_executable(test_landscape
    test/test_landscape.cpp
    src/landscape.cpp
    src/lookup_tables.cpp
)
target_include_directories(test_landscape PRIVATE src)

# Test for polar coordinate conversion
add_executable(test_polar_coords
    test/test_polar_coords.cpp
    src/polar_coords.cpp
    src/lookup_tables.cpp
)
target_include_directories(test_polar_coords PRIVATE src)

# Test for 3D object data
add_executable(test_object3d
    test/test_object3d.cpp
    src/object3d.cpp
)
target_include_directories(test_object3d PRIVATE src)

# Test for object rendering
add_executable(test_object_renderer
    test/test_object_renderer.cpp
    src/object_renderer.cpp
    src/object3d.cpp
    src/math3d.cpp
    src/lookup_tables.cpp
    src/screen.cpp
    src/projection.cpp
    src/camera.cpp
    src/landscape.cpp
)
target_include_directories(test_object_renderer PRIVATE src)

# Test for particle system
add_executable(test_particles
    test/test_particles.cpp
    src/particles.cpp
    src/landscape.cpp
    src/lookup_tables.cpp
)
target_include_directories(test_particles PRIVATE src)

# Enable testing
enable_testing()
add_test(NAME test_fixed COMMAND test_fixed)
add_test(NAME test_lookup_tables COMMAND test_lookup_tables)
add_test(NAME test_math3d COMMAND test_math3d)
add_test(NAME test_screen COMMAND test_screen)
add_test(NAME test_palette COMMAND test_palette)
add_test(NAME test_projection COMMAND test_projection)
add_test(NAME test_landscape COMMAND test_landscape)
add_test(NAME test_polar_coords COMMAND test_polar_coords)
add_test(NAME test_object3d COMMAND test_object3d)
add_test(NAME test_object_renderer COMMAND test_object_renderer)
add_test(NAME test_particles COMMAND test_particles)
