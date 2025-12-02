# Lander C++/SDL Port

A faithful C++/SDL port of **Lander** (Acorn Archimedes, 1987) by David Braben.

This demo was a precursor to the game Zarch (1987) and later Virus (1988) for the Amiga.

## Features

- Full 3D landscape with Fourier-synthesized terrain
- Physics-based ship controls with thrust, gravity, and friction
- Destructible objects (trees, buildings, rockets)
- Particle effects (exhaust, bullets, explosions, smoke, splashes)
- Falling rocks hazard (score >= 800)
- Star particles at high altitude
- Sound effects with spatial audio
- Smooth edge clipping (Sutherland-Hodgman)
- Multiple display resolutions and frame rates
- Persistent settings

## Requirements

- CMake 3.16+
- C++17 compiler
- SDL2

### Installing SDL2

**macOS (Homebrew):**
```bash
brew install sdl2
```

**Ubuntu/Debian:**
```bash
sudo apt install libsdl2-dev
```

**Windows (vcpkg):**
```bash
vcpkg install sdl2
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
./lander
```

## Controls

### Flight Controls

| Input | Action |
|-------|--------|
| Mouse movement | Ship orientation (pitch and yaw) |
| Left mouse button | Full thrust |
| Middle mouse button | Hover thrust |
| Right mouse button | Fire bullets |

### Keyboard

| Key | Action |
|-----|--------|
| Escape | Exit game |
| P | Pause / Unpause |
| Tab | Toggle debug overlay |
| F11 or Alt+Enter | Toggle fullscreen |
| D | Toggle debug mode (keyboard flight) |

### Debug Mode (D)

When debug mode is enabled, use arrow keys to fly:
- Arrow keys: Move horizontally
- A/Z: Move vertically

### Settings (Keys 1-6)

| Key | Setting | Options |
|-----|---------|---------|
| 1 | Landscape scale | 1x (12x10), 2x (24x20), 4x (48x40), 8x (96x80) |
| 2 | Frame rate | 15, 30, 60, 120 FPS |
| 3 | Display resolution | 320x256, 640x512, 1280x1024 |
| 4 | Smooth edge clipping | On / Off |
| 5 | Sound effects | On / Off |
| 6 | Star particles | On / Off |

Settings are automatically saved to `settings.cfg`.

## Gameplay

- Take off from the launchpad and explore the landscape
- Shoot objects (trees, buildings, rockets) to score points
- Land gently on the launchpad to refuel
- Avoid crashing into terrain or objects
- At score >= 800, falling rocks begin to appear
- Fuel is limited - manage your thrust carefully
- Game over when all lives are lost

## Debug Overlay (Tab)

Shows current settings across the bottom of the screen:
```
12x10  60/58  1280x1024  CLIP  SFX  STAR
```
- Landscape tiles
- Target FPS / Actual FPS
- Display resolution
- Clipping (CLIP = on, clip = off)
- Sound (SFX = on, sfx = off)
- Stars (STAR = on, star = off)

## Project Structure

```
lander/
├── src/              # Source files
├── test/             # Test files
├── sounds/           # Sound effect WAV files
├── docs/             # Design documents
├── scripts/          # Build and release scripts
├── CMakeLists.txt
├── TASKS.md          # Task breakdown
├── PROGRESS.md       # Implementation progress
└── README.md
```

## Running Tests

```bash
ctest --output-on-failure
```

## Credits

- Original game: David Braben (1987)
- C++/SDL port: This project

## License

This is an educational port of the original Lander demo. The original game is copyright David Braben / Frontier Developments.
