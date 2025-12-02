# Code Cleanup and Improvement Suggestions

This document outlines potential improvements to the codebase for better maintainability, readability, and organization.

## 1. Main.cpp Decomposition

### Current State
`main.cpp` is over 1,200 lines and contains the entire Game class with many responsibilities.

### Suggested Improvements

**Split into separate files:**
- `game.h/cpp` - Game class definition and core loop
- `game_state.h/cpp` - Game state machine (Playing, Exploding, GameOver)
- `game_input.h/cpp` - Input handling and key bindings
- `game_render.h/cpp` - Rendering coordination
- `game_audio.h/cpp` - Sound effect triggers and spatial audio
- `ui.h/cpp` - Score bar, fuel gauge, FPS overlay, pause indicator

**Benefits:**
- Easier navigation
- Better separation of concerns
- Reduced compilation times when modifying one area
- Clearer dependencies

## 2. Configuration System

### Current State
Constants are scattered across multiple files and namespaces (GameConstants, PlayerConstants, StarConfig, etc.).

### Suggested Improvements

**Centralized configuration:**
```cpp
// config.h
namespace Config {
    namespace Display { ... }
    namespace Physics { ... }
    namespace Particles { ... }
    namespace Sound { ... }
}
```

**Runtime configuration file:**
- Load tunable parameters from a config file
- Enable tweaking without recompilation
- Separate debug/release configurations

## 3. Particle System Refactoring

### Current State
- All particle types share the same Particle struct
- Star-specific fields (starSize, starBrightness, initialLifespan) added to base struct
- Type-specific behavior scattered across update and render code

### Suggested Improvements

**Polymorphic particles or component system:**
```cpp
struct ParticleBase { position, velocity, lifespan, flags };
struct StarParticle : ParticleBase { size, brightness, initialLifespan };
struct RockParticle : ParticleBase { rotationMatrix };
```

**Or use a variant:**
```cpp
using Particle = std::variant<StandardParticle, StarParticle, RockParticle>;
```

**Dedicated particle pools:**
- Separate arrays for different particle types
- Avoid checking flags for every particle
- Better cache locality for type-specific updates

## 4. Graphics Buffer Improvements

### Current State
- Raw arrays with magic numbers
- Manual buffer management
- Triangle data stored as flat arrays

### Suggested Improvements

**Typed buffer entries:**
```cpp
struct BufferedTriangle {
    ProjectedVertex vertices[3];
    Color color;
};

struct BufferedRect {
    int x, y, width, height;
    Color color;
};
```

**Dynamic sizing or better bounds checking:**
- Assert on overflow in debug builds
- Configurable buffer sizes
- Statistics on buffer usage

## 5. Error Handling

### Current State
- Some functions return bool for success/failure
- SDL errors logged but not always handled gracefully
- No consistent error reporting strategy

### Suggested Improvements

**Result types or exceptions:**
```cpp
enum class InitError { SDLFailed, WindowFailed, RendererFailed, ... };
std::expected<void, InitError> Game::init();
```

**Graceful degradation:**
- Continue without sound if audio init fails
- Use software rendering if hardware unavailable
- Report capabilities at startup

## 6. Magic Number Elimination

### Current State
Magic numbers appear in various places:
- `10 * CHAR_WIDTH` for UI positioning
- `15` for ship visual depth
- `0x01000000` for tile sizes in raw fixed-point

### Suggested Improvements

**Named constants for all values:**
```cpp
namespace UI {
    constexpr int PAUSED_TEXT_COLUMN = 10;
    constexpr int LIVES_COLUMN = 30;
    constexpr int HIGH_SCORE_COLUMN = 35;
}

namespace Rendering {
    constexpr int SHIP_VISUAL_DEPTH_TILES = 15;
}
```

## 7. Input Abstraction

### Current State
- Direct SDL key checks in processEvents
- Key bindings hardcoded
- No input remapping support

### Suggested Improvements

**Action-based input:**
```cpp
enum class Action { Thrust, Hover, Fire, Pause, ToggleDebug, ... };

class InputMapper {
    Action getAction(SDL_Keycode key);
    void rebind(Action action, SDL_Keycode key);
};
```

**Benefits:**
- Configurable controls
- Gamepad support possible
- Cleaner event handling

## 8. Rendering Pipeline Cleanup

### Current State
- Rendering logic spread across multiple functions
- Buffer flushing interleaved with landscape rendering
- Complex coordination between systems

### Suggested Improvements

**Clear rendering phases:**
```cpp
void Game::render() {
    beginFrame();
    renderBackground();
    renderLandscape();
    renderObjects();
    renderParticles();
    renderShip();
    renderUI();
    endFrame();
}
```

**Render command buffer:**
- Queue all draw calls
- Sort and batch for efficiency
- Cleaner separation from game logic

## 9. Test Improvements

### Current State
- Tests use ad-hoc verification
- Some tests marked as failing (projection)
- Limited edge case coverage

### Suggested Improvements

**Use a proper test framework:**
- Google Test or Catch2
- Better assertion messages
- Test fixtures and parameterized tests

**Fix or remove failing tests:**
- Investigate projection test failures
- Update tests for current behavior
- Document known limitations

**Add integration tests:**
- Full game loop tests
- Collision scenario tests
- State machine verification

## 10. Documentation

### Current State
- Good function-level comments
- Some design rationale in comments
- Limited API documentation

### Suggested Improvements

**Doxygen comments for public APIs:**
```cpp
/// @brief Spawn exhaust particles behind the ship
/// @param pos Ship world position
/// @param vel Ship velocity (added to particle velocity)
/// @param exhaust Exhaust direction vector (from rotation matrix)
/// @param fullThrust true for 8 particles, false for 2 particles
void spawnExhaustParticles(...);
```

**Architecture documentation:**
- Coordinate system diagram
- Rendering pipeline overview
- Particle system documentation

## 11. Code Style Consistency

### Current State
- Mix of brace styles in some places
- Inconsistent spacing around operators
- Variable naming mostly consistent

### Suggested Improvements

**Apply clang-format:**
- Create .clang-format file
- Consistent style throughout
- Automated formatting on commit

**Naming conventions document:**
- camelCase for functions and variables
- PascalCase for types and classes
- UPPER_CASE for constants

## Priority Ranking

| Priority | Item | Effort | Impact |
|----------|------|--------|--------|
| High | Main.cpp decomposition | High | High |
| High | Magic number elimination | Medium | Medium |
| Medium | Configuration system | Medium | Medium |
| Medium | Particle system refactoring | High | Medium |
| Medium | Test improvements | Medium | Medium |
| Low | Input abstraction | Medium | Low |
| Low | Rendering pipeline cleanup | High | Medium |
| Low | Error handling | Medium | Low |

## Conclusion

The codebase is functional and well-commented, but could benefit from better organization as it has grown. The highest-priority improvements are decomposing the monolithic main.cpp and eliminating magic numbers, as these would have the most immediate impact on maintainability.
