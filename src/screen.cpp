#include "screen.h"
#include <algorithm>

// Include stb_image_write implementation in this compilation unit
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

ScreenBuffer::ScreenBuffer() {
    buffer = new uint8_t[getBufferSize()];
    clear();
}

ScreenBuffer::~ScreenBuffer() {
    delete[] buffer;
}

void ScreenBuffer::clear(Color color) {
    // For black, use memset for efficiency
    if (color.r == 0 && color.g == 0 && color.b == 0 && color.a == 255) {
        // Set all to 0, then fix alpha
        std::memset(buffer, 0, getBufferSize());
        // Set alpha channel to 255 for all pixels
        for (size_t i = 3; i < getBufferSize(); i += 4) {
            buffer[i] = 255;
        }
    } else {
        // Fill with the specified color
        for (size_t i = 0; i < getBufferSize(); i += 4) {
            buffer[i + 0] = color.r;
            buffer[i + 1] = color.g;
            buffer[i + 2] = color.b;
            buffer[i + 3] = color.a;
        }
    }
}

void ScreenBuffer::plotPixel(int x, int y, Color color) {
    // Convert logical to physical and plot single pixel
    plotPhysicalPixel(toPhysicalX(x), toPhysicalY(y), color);
}

void ScreenBuffer::plotPhysicalPixel(int px, int py, Color color) {
    // Bounds check in physical coordinates
    if (!inPhysicalBounds(px, py)) {
        return;
    }

    size_t offset = physicalToOffset(px, py);
    buffer[offset + 0] = color.r;
    buffer[offset + 1] = color.g;
    buffer[offset + 2] = color.b;
    buffer[offset + 3] = color.a;
}

void ScreenBuffer::drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color) {
    // Sort vertices by y-coordinate (y0 <= y1 <= y2)
    // This matches the original Lander algorithm which processes from bottom to top
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    if (y1 > y2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    // Now y0 <= y1 <= y2 (top to bottom in screen coordinates)

    // Degenerate triangle check
    if (y0 == y2) {
        // Horizontal line - just draw from min x to max x
        int minX = std::min({x0, x1, x2});
        int maxX = std::max({x0, x1, x2});
        drawHorizontalLine(minX, maxX, y0, color);
        return;
    }

    // Use 16.16 fixed-point for edge slopes (matching original's precision)
    // The original uses shifts of 16 bits for fractional precision

    // Calculate inverse slopes (dx/dy) for the two edges from top vertex
    // Edge from (x0,y0) to (x2,y2) - the long edge spanning full height
    int dy02 = y2 - y0;
    int64_t dx02 = ((int64_t)(x2 - x0) << 16) / dy02;

    if (y0 == y1) {
        // Flat-top triangle: just draw bottom half
        int dy12 = y2 - y1;
        int64_t dx12 = ((int64_t)(x2 - x1) << 16) / dy12;

        int64_t curx1 = (int64_t)x0 << 16;
        int64_t curx2 = (int64_t)x1 << 16;

        // Ensure curx1 is the left edge
        if (curx1 > curx2) {
            std::swap(curx1, curx2);
            std::swap(dx02, dx12);
        }

        for (int y = y0; y <= y2; y++) {
            drawHorizontalLine((int)(curx1 >> 16), (int)(curx2 >> 16), y, color);
            curx1 += dx02;
            curx2 += dx12;
        }
    } else if (y1 == y2) {
        // Flat-bottom triangle: just draw top half
        int dy01 = y1 - y0;
        int64_t dx01 = ((int64_t)(x1 - x0) << 16) / dy01;

        int64_t curx1 = (int64_t)x0 << 16;
        int64_t curx2 = (int64_t)x0 << 16;

        // Determine which slope goes left vs right
        if (dx01 > dx02) {
            std::swap(dx01, dx02);
        }

        for (int y = y0; y <= y1; y++) {
            drawHorizontalLine((int)(curx1 >> 16), (int)(curx2 >> 16), y, color);
            curx1 += dx01;
            curx2 += dx02;
        }
    } else {
        // General case: split into flat-bottom and flat-top triangles
        int dy01 = y1 - y0;
        int64_t dx01 = ((int64_t)(x1 - x0) << 16) / dy01;

        int dy12 = y2 - y1;
        int64_t dx12 = ((int64_t)(x2 - x1) << 16) / dy12;

        // Draw top half (from y0 to y1)
        int64_t curx1 = (int64_t)x0 << 16;
        int64_t curx2 = (int64_t)x0 << 16;

        // Determine which edge is left vs right for top half
        int64_t slope_left = dx01;
        int64_t slope_right = dx02;
        if (slope_left > slope_right) {
            std::swap(slope_left, slope_right);
        }

        for (int y = y0; y < y1; y++) {
            drawHorizontalLine((int)(curx1 >> 16), (int)(curx2 >> 16), y, color);
            curx1 += slope_left;
            curx2 += slope_right;
        }

        // Draw bottom half (from y1 to y2)
        // Reset one edge to start at (x1, y1)
        // The long edge (0->2) continues, short edge restarts at vertex 1
        int64_t long_edge_x = (int64_t)x0 << 16;
        long_edge_x += dx02 * (y1 - y0);

        int64_t short_edge_x = (int64_t)x1 << 16;

        // Determine left/right for bottom half
        if (long_edge_x > short_edge_x) {
            curx1 = short_edge_x;
            curx2 = long_edge_x;
            slope_left = dx12;
            slope_right = dx02;
        } else {
            curx1 = long_edge_x;
            curx2 = short_edge_x;
            slope_left = dx02;
            slope_right = dx12;
        }

        for (int y = y1; y <= y2; y++) {
            drawHorizontalLine((int)(curx1 >> 16), (int)(curx2 >> 16), y, color);
            curx1 += slope_left;
            curx2 += slope_right;
        }
    }
}

void ScreenBuffer::drawHorizontalLine(int x1, int x2, int y, Color color) {
    // Early reject if y is off-screen
    if (y < 0 || y >= PHYSICAL_HEIGHT) {
        return;
    }

    // Ensure x1 <= x2
    if (x1 > x2) {
        int tmp = x1;
        x1 = x2;
        x2 = tmp;
    }

    // Clip to screen bounds
    if (x2 < 0 || x1 >= PHYSICAL_WIDTH) {
        return;  // Entirely off-screen
    }
    if (x1 < 0) {
        x1 = 0;
    }
    if (x2 >= PHYSICAL_WIDTH) {
        x2 = PHYSICAL_WIDTH - 1;
    }

    // Calculate start position in buffer
    size_t offset = physicalToOffset(x1, y);
    int length = x2 - x1 + 1;

    // Draw the line
    // Pack color into a 32-bit value for potential optimization
    uint32_t rgba = (static_cast<uint32_t>(color.r)) |
                    (static_cast<uint32_t>(color.g) << 8) |
                    (static_cast<uint32_t>(color.b) << 16) |
                    (static_cast<uint32_t>(color.a) << 24);

    uint32_t* dest = reinterpret_cast<uint32_t*>(buffer + offset);
    for (int i = 0; i < length; i++) {
        dest[i] = rgba;
    }
}

Color ScreenBuffer::getPhysicalPixel(int px, int py) const {
    if (!inPhysicalBounds(px, py)) {
        return Color::black();
    }

    size_t offset = physicalToOffset(px, py);
    return Color(
        buffer[offset + 0],
        buffer[offset + 1],
        buffer[offset + 2],
        buffer[offset + 3]
    );
}

bool ScreenBuffer::savePNG(const char* filename) const {
    // stbi_write_png expects: filename, width, height, components, data, stride
    // Components = 4 for RGBA
    int result = stbi_write_png(
        filename,
        PHYSICAL_WIDTH,
        PHYSICAL_HEIGHT,
        4,
        buffer,
        getPitch()
    );
    return result != 0;
}
