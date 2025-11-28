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

RowBuffer::RowBuffer()
{
    triangles.reserve(MAX_TRIANGLES);
}

void RowBuffer::addTriangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color)
{
    // Don't exceed buffer capacity
    if (triangles.size() >= MAX_TRIANGLES) {
        return;
    }

    BufferedTriangle tri;
    tri.x1 = static_cast<int16_t>(x1);
    tri.y1 = static_cast<int16_t>(y1);
    tri.x2 = static_cast<int16_t>(x2);
    tri.y2 = static_cast<int16_t>(y2);
    tri.x3 = static_cast<int16_t>(x3);
    tri.y3 = static_cast<int16_t>(y3);
    tri.color = color;

    triangles.push_back(tri);
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

GraphicsBufferSystem::GraphicsBufferSystem()
{
    // Buffers are initialized by their constructors
}

void GraphicsBufferSystem::addTriangle(int row, int x1, int y1, int x2, int y2,
                                        int x3, int y3, Color color)
{
    // Validate row index
    if (row < 0 || row >= TILES_Z) {
        return;
    }

    buffers[row].addTriangle(x1, y1, x2, y2, x3, y3, color);
}

void GraphicsBufferSystem::addShadowTriangle(int row, int x1, int y1, int x2, int y2,
                                              int x3, int y3, Color color)
{
    // Validate row index
    if (row < 0 || row >= TILES_Z) {
        return;
    }

    shadowBuffers[row].addTriangle(x1, y1, x2, y2, x3, y3, color);
}

void GraphicsBufferSystem::drawAndClearRow(int row, ScreenBuffer& screen)
{
    // Validate row index
    if (row < 0 || row >= TILES_Z) {
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
    for (int i = 0; i < TILES_Z; i++) {
        buffers[i].clear();
        shadowBuffers[i].clear();
    }
}

size_t GraphicsBufferSystem::getTriangleCount(int row) const
{
    if (row < 0 || row >= TILES_Z) {
        return 0;
    }
    return buffers[row].getTriangleCount() + shadowBuffers[row].getTriangleCount();
}

size_t GraphicsBufferSystem::getTotalTriangleCount() const
{
    size_t total = 0;
    for (int i = 0; i < TILES_Z; i++) {
        total += buffers[i].getTriangleCount();
        total += shadowBuffers[i].getTriangleCount();
    }
    return total;
}
