#include "screen.h"

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
