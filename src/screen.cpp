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
