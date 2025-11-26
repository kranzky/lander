# Lander C++/SDL Port

A faithful C++/SDL port of **Lander** (Acorn Archimedes, 1987) by David Braben.

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

**Run the game:**
```bash
./lander
```

Press `Escape` to exit.

**Run tests:**
```bash
./test_fixed
```

Or via CTest:
```bash
ctest --output-on-failure
```

## Project Structure

```
├── src/              # Source files
├── test/             # Test files
├── CMakeLists.txt
├── PLAN.md           # Implementation plan
├── TASKS.md          # Task breakdown
└── README.md
```

## Controls

| Input | Action |
|-------|--------|
| Mouse movement | Ship orientation |
| Left mouse button | Full thrust |
| Middle mouse button | Hover thrust |
| Right mouse button | Fire bullets |
| Escape | Exit game |
