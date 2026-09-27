// graphics_buffer.cpp
// Depth-sorted graphics buffer system for deferred triangle rendering

#include "graphics_buffer.h"

// =============================================================================
// Global Instance
// =============================================================================

GraphicsBufferSystem graphicsBuffers;

// =============================================================================
// RowBuffer Implementation
// =============================================================================

void RowBuffer::addTriangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color)
{
    triangles.push_back({x1, y1, x2, y2, x3, y3, color});
}

void RowBuffer::draw(ScreenBuffer& screen)
{
    for (const auto& tri : triangles) {
        screen.drawTriangle(tri.x1, tri.y1, tri.x2, tri.y2, tri.x3, tri.y3, tri.color);
    }
}

void RowBuffer::clear()
{
    triangles.clear();
}

// =============================================================================
// GraphicsBufferSystem Implementation
// =============================================================================

namespace {
    bool isValidRow(int row) { return row >= 0 && row < TILES_Z; }
}

void GraphicsBufferSystem::addTriangle(int row, int x1, int y1, int x2, int y2,
                                        int x3, int y3, Color color)
{
    if (isValidRow(row)) {
        buffers[row].addTriangle(x1, y1, x2, y2, x3, y3, color);
    }
}

void GraphicsBufferSystem::addShadowTriangle(int row, int x1, int y1, int x2, int y2,
                                              int x3, int y3, Color color)
{
    if (isValidRow(row)) {
        shadowBuffers[row].addTriangle(x1, y1, x2, y2, x3, y3, color);
    }
}

void GraphicsBufferSystem::drawAndClearRow(int row, ScreenBuffer& screen)
{
    if (!isValidRow(row)) {
        return;
    }

    // Draw shadows first (they should appear under objects)
    shadowBuffers[row].draw(screen);
    shadowBuffers[row].clear();

    // Then draw objects (on top of shadows)
    buffers[row].draw(screen);
    buffers[row].clear();
}

void GraphicsBufferSystem::clearAll()
{
    // Clear every row, not just the current TILES_Z, in case the landscape
    // scale shrank since these rows were filled
    for (int i = 0; i < GameConstants::MAX_TILES_Z; i++) {
        buffers[i].clear();
        shadowBuffers[i].clear();
    }
}

size_t GraphicsBufferSystem::getTriangleCount(int row) const
{
    if (!isValidRow(row)) {
        return 0;
    }
    return buffers[row].getTriangleCount() + shadowBuffers[row].getTriangleCount();
}

size_t GraphicsBufferSystem::getTotalTriangleCount() const
{
    size_t total = 0;
    for (int i = 0; i < GameConstants::MAX_TILES_Z; i++) {
        total += buffers[i].getTriangleCount();
        total += shadowBuffers[i].getTriangleCount();
    }
    return total;
}
