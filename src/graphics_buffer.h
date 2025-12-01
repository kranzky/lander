// graphics_buffer.h
// Depth-sorted graphics buffer system for deferred triangle rendering

#ifndef GRAPHICS_BUFFER_H
#define GRAPHICS_BUFFER_H

#include "fixed.h"
#include "screen.h"
#include <cstdint>
#include <vector>

// =============================================================================
// Graphics Buffer System
// =============================================================================
//
// Port of the graphics buffer system from Lander.arm.
//
// The original game uses a set of graphics buffers (one per tile row) to enable
// depth-sorted rendering. Objects are not drawn immediately; instead, their
// triangles are stored in the buffer corresponding to their tile row. After
// each row of landscape is drawn, the buffer for that row is flushed, drawing
// all objects at that depth level.
//
// This ensures objects appear at the correct depth relative to the landscape,
// with distant objects drawn before closer ones.
//
// Original implementation details (Lander.arm):
// - BUFFER_SIZE = 4308 bytes per buffer
// - 11 buffers total (one per TILES_Z row)
// - Triangle command uses 7 words: x1, y1, x2, y2, x3, y3, color
// - Command 18 = draw triangle, Command 19 = terminate buffer
//
// =============================================================================

using namespace GameConstants;

// Triangle data structure for buffered rendering
struct BufferedTriangle {
    int16_t x1, y1;
    int16_t x2, y2;
    int16_t x3, y3;
    Color color;
};

// Graphics buffer for a single tile row
class RowBuffer {
public:
    RowBuffer();

    // Add a triangle to this buffer
    void addTriangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color);

    // Draw all triangles in this buffer to the screen
    void draw(ScreenBuffer& screen);

    // Clear this buffer
    void clear();

    // Check if buffer is empty
    bool isEmpty() const { return triangles.empty(); }

    // Get triangle count
    size_t getTriangleCount() const { return triangles.size(); }

private:
    std::vector<BufferedTriangle> triangles;

    // Maximum triangles per buffer
    // Original: 4308 / 28 ≈ 153 triangles, but we have more particles and need headroom
    // At 484 max particles * 4 triangles each = 1936 triangles if all in one row (worst case)
    // Use 512 to provide comfortable headroom for typical cases
    static constexpr size_t MAX_TRIANGLES = 512;
};

// Main graphics buffer system managing all tile row buffers
class GraphicsBufferSystem {
public:
    GraphicsBufferSystem();

    // Add a triangle to the buffer for a specific tile row
    // Row 0 = furthest (back), Row TILES_Z-1 = nearest (front)
    void addTriangle(int row, int x1, int y1, int x2, int y2, int x3, int y3, Color color);

    // Add a shadow triangle to the shadow buffer for a specific tile row
    // Shadows are drawn before objects in the same row
    void addShadowTriangle(int row, int x1, int y1, int x2, int y2, int x3, int y3, Color color);

    // Draw all triangles in a specific row buffer and clear it
    // Draws shadows first, then objects
    void drawAndClearRow(int row, ScreenBuffer& screen);

    // Clear all buffers (call at start of each frame)
    void clearAll();

    // Get statistics
    size_t getTriangleCount(int row) const;
    size_t getTotalTriangleCount() const;

private:
    // One buffer per tile row for objects (sized for max scale)
    RowBuffer buffers[MAX_TILES_Z];
    // Separate buffer per tile row for shadows (drawn before objects)
    RowBuffer shadowBuffers[MAX_TILES_Z];
};

// Global graphics buffer system instance
extern GraphicsBufferSystem graphicsBuffers;

#endif // GRAPHICS_BUFFER_H
