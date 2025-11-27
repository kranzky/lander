# CLAUDE.md - C++ Port

Instructions for Claude Code when working on this C++ port.

## CRITICAL: Git Workflow

**DO NOT COMMIT without explicit user approval.** Always:
1. Stage changes with `git add`
2. Show the user what will be committed
3. Wait for the user to test and approve
4. Only commit after explicit approval

This is a hard requirement - the user must test and approve all changes before committing.

## Build Commands

```bash
cd build
cmake ..
make           # Build main executable
make lander    # Build main executable
ctest          # Run all tests
./test_*       # Run individual tests
./lander       # Run the game
```

## Project Structure

- `src/` - Source files (.cpp, .h)
- `test/` - Test files (test_*.cpp)
- `build/` - Build output (created by cmake)
- `PROGRESS.md` - Task tracking with commit hashes

## Current Progress

See PROGRESS.md for task status. Each completed task has a commit hash.
