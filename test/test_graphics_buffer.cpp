// test_graphics_buffer.cpp
// Test the graphics buffer system

#include "graphics_buffer.h"
#include "screen.h"
#include <iostream>
#include <cassert>

void testRowBufferBasics()
{
    std::cout << "Testing RowBuffer basics..." << std::endl;

    RowBuffer buffer;

    // Initially empty
    assert(buffer.isEmpty());
    assert(buffer.getTriangleCount() == 0);

    // Add a triangle
    buffer.addTriangle(10, 20, 30, 40, 50, 60, Color{0xFF, 0x00, 0x00, 0xFF});
    assert(!buffer.isEmpty());
    assert(buffer.getTriangleCount() == 1);

    // Add another triangle
    buffer.addTriangle(100, 110, 120, 130, 140, 150, Color{0x00, 0xFF, 0x00, 0xFF});
    assert(buffer.getTriangleCount() == 2);

    // Clear the buffer
    buffer.clear();
    assert(buffer.isEmpty());
    assert(buffer.getTriangleCount() == 0);

    std::cout << "  PASS" << std::endl;
}

void testRowBufferDraw()
{
    std::cout << "Testing RowBuffer draw..." << std::endl;

    RowBuffer buffer;
    ScreenBuffer screen;

    // Add triangles
    buffer.addTriangle(100, 100, 150, 50, 200, 100, Color{0xFF, 0x00, 0x00, 0xFF});
    buffer.addTriangle(50, 150, 100, 100, 150, 150, Color{0x00, 0xFF, 0x00, 0xFF});

    // Draw should not crash and should draw both triangles
    buffer.draw(screen);

    // Verify buffer still has triangles after draw (draw doesn't clear)
    assert(buffer.getTriangleCount() == 2);

    std::cout << "  PASS" << std::endl;
}

void testGraphicsBufferSystemBasics()
{
    std::cout << "Testing GraphicsBufferSystem basics..." << std::endl;

    GraphicsBufferSystem system;

    // Clear all buffers
    system.clearAll();
    assert(system.getTotalTriangleCount() == 0);

    // Add triangles to different rows
    system.addTriangle(0, 10, 20, 30, 40, 50, 60, Color{0xFF, 0x00, 0x00, 0xFF});
    system.addTriangle(5, 100, 110, 120, 130, 140, 150, Color{0x00, 0xFF, 0x00, 0xFF});
    system.addTriangle(10, 200, 210, 220, 230, 240, 250, Color{0x00, 0x00, 0xFF, 0xFF});

    assert(system.getTriangleCount(0) == 1);
    assert(system.getTriangleCount(5) == 1);
    assert(system.getTriangleCount(10) == 1);
    assert(system.getTotalTriangleCount() == 3);

    // Clear all
    system.clearAll();
    assert(system.getTotalTriangleCount() == 0);

    std::cout << "  PASS" << std::endl;
}

void testGraphicsBufferSystemDrawAndClear()
{
    std::cout << "Testing GraphicsBufferSystem drawAndClearRow..." << std::endl;

    GraphicsBufferSystem system;
    ScreenBuffer screen;

    // Add triangles to rows 0 and 5
    system.addTriangle(0, 100, 100, 150, 50, 200, 100, Color{0xFF, 0x00, 0x00, 0xFF});
    system.addTriangle(5, 100, 150, 150, 100, 200, 150, Color{0x00, 0xFF, 0x00, 0xFF});

    assert(system.getTriangleCount(0) == 1);
    assert(system.getTriangleCount(5) == 1);

    // Draw and clear row 0
    system.drawAndClearRow(0, screen);
    assert(system.getTriangleCount(0) == 0);
    assert(system.getTriangleCount(5) == 1);  // Row 5 still has its triangle

    // Draw and clear row 5
    system.drawAndClearRow(5, screen);
    assert(system.getTriangleCount(5) == 0);

    std::cout << "  PASS" << std::endl;
}

void testInvalidRowHandling()
{
    std::cout << "Testing invalid row handling..." << std::endl;

    GraphicsBufferSystem system;
    ScreenBuffer screen;

    // These should not crash (invalid rows are silently ignored)
    system.addTriangle(-1, 10, 20, 30, 40, 50, 60, Color{0xFF, 0x00, 0x00, 0xFF});
    system.addTriangle(100, 10, 20, 30, 40, 50, 60, Color{0xFF, 0x00, 0x00, 0xFF});
    system.drawAndClearRow(-1, screen);
    system.drawAndClearRow(100, screen);

    assert(system.getTriangleCount(-1) == 0);
    assert(system.getTriangleCount(100) == 0);

    std::cout << "  PASS" << std::endl;
}

void testGlobalInstance()
{
    std::cout << "Testing global graphicsBuffers instance..." << std::endl;

    // The global instance should be accessible
    graphicsBuffers.clearAll();
    assert(graphicsBuffers.getTotalTriangleCount() == 0);

    graphicsBuffers.addTriangle(3, 50, 50, 100, 100, 150, 50, Color{0xFF, 0xFF, 0x00, 0xFF});
    assert(graphicsBuffers.getTriangleCount(3) == 1);

    graphicsBuffers.clearAll();
    assert(graphicsBuffers.getTotalTriangleCount() == 0);

    std::cout << "  PASS" << std::endl;
}

void testMultipleTrianglesPerRow()
{
    std::cout << "Testing multiple triangles per row..." << std::endl;

    GraphicsBufferSystem system;

    // Add many triangles to the same row
    for (int i = 0; i < 50; i++) {
        system.addTriangle(2, i*5, i*5, i*5+10, i*5+10, i*5+20, i*5,
                          Color{static_cast<uint8_t>(i*5), 0, 0, 0xFF});
    }

    assert(system.getTriangleCount(2) == 50);

    system.clearAll();
    assert(system.getTriangleCount(2) == 0);

    std::cout << "  PASS" << std::endl;
}

int main()
{
    std::cout << "=== Graphics Buffer Tests ===" << std::endl;

    testRowBufferBasics();
    testRowBufferDraw();
    testGraphicsBufferSystemBasics();
    testGraphicsBufferSystemDrawAndClear();
    testInvalidRowHandling();
    testGlobalInstance();
    testMultipleTrianglesPerRow();

    std::cout << std::endl;
    std::cout << "All graphics buffer tests passed!" << std::endl;

    return 0;
}
