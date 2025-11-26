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

# Enable testing
enable_testing()
add_test(NAME test_fixed COMMAND test_fixed)
add_test(NAME test_lookup_tables COMMAND test_lookup_tables)
