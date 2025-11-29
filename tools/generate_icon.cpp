// Icon Generator for Lander
// Renders the player ship on a black background at a good viewing angle
// Outputs multiple sizes for macOS icon generation

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../src/stb_image_write.h"

// Inline the minimal required types and data

struct Color {
    uint8_t r, g, b, a;
    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    static Color black() { return Color(0, 0, 0, 255); }
};

// Ship vertex data (from object3d.cpp)
struct Vertex { int32_t x, y, z; };
struct Face {
    int32_t nx, ny, nz;
    uint8_t v0, v1, v2;
    uint16_t color;
};

static const Vertex shipVertices[] = {
    { 0x01000000, 0x00500000, 0x00800000 },
    { 0x01000000, 0x00500000, (int32_t)0xFF800000 },
    { 0x00000000, 0x000A0000, (int32_t)0xFECCCCCD },
    { (int32_t)0xFF19999A, 0x00500000, 0x00000000 },
    { 0x00000000, 0x000A0000, 0x01333333 },
    { (int32_t)0xFFE66667, (int32_t)0xFF880000, 0x00000000 },
    { 0x00555555, 0x00500000, 0x00400000 },
    { 0x00555555, 0x00500000, (int32_t)0xFFC00000 },
    { (int32_t)0xFFCCCCCD, 0x00500000, 0x00000000 },
};

static const Face shipFaces[] = {
    { 0x457C441A, (int32_t)0x9E2A1F4C, 0x00000000, 0, 1, 5, 0x080 },
    { 0x35F5D83B, (int32_t)0x9BC03EC1, (int32_t)0xDA12D71D, 1, 2, 5, 0x040 },
    { 0x35F5D83B, (int32_t)0x9BC03EC1, 0x25ED28E3, 0, 5, 4, 0x040 },
    { (int32_t)0xB123D51C, (int32_t)0xAF3F50EE, (int32_t)0xD7417278, 2, 3, 5, 0x040 },
    { (int32_t)0xB123D51D, (int32_t)0xAF3F50EE, 0x28BE8D88, 3, 4, 5, 0x040 },
    { (int32_t)0xF765D8CD, 0x73242236, (int32_t)0xDF4FD176, 1, 2, 3, 0x088 },
    { (int32_t)0xF765D8CD, 0x73242236, 0x20B02E8A, 0, 3, 4, 0x088 },
    { 0x00000000, 0x78000000, 0x00000000, 0, 1, 3, 0x044 },
    { 0x00000000, 0x78000000, 0x00000000, 6, 7, 8, 0xC80 },
};

static const int NUM_VERTICES = 9;
static const int NUM_FACES = 9;

// Simple 3x3 matrix for rotation
struct Mat3 {
    double m[3][3];

    void rotateX(double angle) {
        double c = cos(angle), s = sin(angle);
        double t[3][3] = {{1,0,0}, {0,c,-s}, {0,s,c}};
        multiply(t);
    }

    void rotateY(double angle) {
        double c = cos(angle), s = sin(angle);
        double t[3][3] = {{c,0,s}, {0,1,0}, {-s,0,c}};
        multiply(t);
    }

    void rotateZ(double angle) {
        double c = cos(angle), s = sin(angle);
        double t[3][3] = {{c,-s,0}, {s,c,0}, {0,0,1}};
        multiply(t);
    }

    void multiply(double t[3][3]) {
        double r[3][3] = {0};
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                for (int k = 0; k < 3; k++)
                    r[i][j] += m[i][k] * t[k][j];
        memcpy(m, r, sizeof(m));
    }

    void identity() {
        memset(m, 0, sizeof(m));
        m[0][0] = m[1][1] = m[2][2] = 1.0;
    }

    void transform(double& x, double& y, double& z) const {
        double nx = m[0][0]*x + m[0][1]*y + m[0][2]*z;
        double ny = m[1][0]*x + m[1][1]*y + m[1][2]*z;
        double nz = m[2][0]*x + m[2][1]*y + m[2][2]*z;
        x = nx; y = ny; z = nz;
    }
};

// Image buffer
class Image {
public:
    int width, height;
    uint8_t* data;

    Image(int w, int h) : width(w), height(h) {
        data = new uint8_t[w * h * 4];
        clear();
    }

    ~Image() { delete[] data; }

    void clear() {
        // Transparent black background
        memset(data, 0, width * height * 4);
    }

    void setPixel(int x, int y, Color c) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        int idx = (y * width + x) * 4;
        data[idx+0] = c.r;
        data[idx+1] = c.g;
        data[idx+2] = c.b;
        data[idx+3] = c.a;
    }

    void drawHLine(int x1, int x2, int y, Color c) {
        if (y < 0 || y >= height) return;
        if (x1 > x2) std::swap(x1, x2);
        if (x2 < 0 || x1 >= width) return;
        x1 = std::max(0, x1);
        x2 = std::min(width - 1, x2);
        for (int x = x1; x <= x2; x++) setPixel(x, y, c);
    }

    void drawTriangle(double x0, double y0, double x1, double y1, double x2, double y2, Color c) {
        // Sort by Y
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
        if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }

        int iy0 = (int)y0, iy1 = (int)y1, iy2 = (int)y2;
        if (iy0 == iy2) return;

        for (int y = iy0; y <= iy2; y++) {
            double t02 = (y - y0) / (y2 - y0);
            double xa = x0 + t02 * (x2 - x0);
            double xb;
            if (y < iy1) {
                if (iy1 == iy0) continue;
                double t01 = (y - y0) / (y1 - y0);
                xb = x0 + t01 * (x1 - x0);
            } else {
                if (iy2 == iy1) continue;
                double t12 = (y - y1) / (y2 - y1);
                xb = x1 + t12 * (x2 - x1);
            }
            drawHLine((int)xa, (int)xb, y, c);
        }
    }

    bool savePNG(const char* filename) {
        return stbi_write_png(filename, width, height, 4, data, width * 4) != 0;
    }
};

Color colorFrom12bit(uint16_t c, double brightness) {
    int r = ((c >> 8) & 0xF);
    int g = ((c >> 4) & 0xF);
    int b = (c & 0xF);

    // Add brightness
    r = std::min(15, (int)(r + brightness * 4));
    g = std::min(15, (int)(g + brightness * 4));
    b = std::min(15, (int)(b + brightness * 4));

    return Color(r * 17, g * 17, b * 17, 255);
}

void renderShip(Image& img, double rotX, double rotY, double rotZ, double scale) {
    Mat3 mat;
    mat.identity();
    mat.rotateX(rotX);
    mat.rotateY(rotY);
    mat.rotateZ(rotZ);

    // Convert vertices to doubles and transform
    double verts[NUM_VERTICES][3];
    for (int i = 0; i < NUM_VERTICES; i++) {
        // Convert from 8.24 fixed point
        verts[i][0] = shipVertices[i].x / 16777216.0;
        verts[i][1] = shipVertices[i].y / 16777216.0;
        verts[i][2] = shipVertices[i].z / 16777216.0;
        mat.transform(verts[i][0], verts[i][1], verts[i][2]);
    }

    // Transform normals
    double norms[NUM_FACES][3];
    for (int i = 0; i < NUM_FACES; i++) {
        norms[i][0] = shipFaces[i].nx / 2147483648.0;
        norms[i][1] = shipFaces[i].ny / 2147483648.0;
        norms[i][2] = shipFaces[i].nz / 2147483648.0;
        mat.transform(norms[i][0], norms[i][1], norms[i][2]);
    }

    // Calculate face depths for sorting
    struct FaceDepth { int idx; double z; };
    FaceDepth depths[NUM_FACES];
    for (int i = 0; i < NUM_FACES; i++) {
        const Face& f = shipFaces[i];
        depths[i].idx = i;
        depths[i].z = (verts[f.v0][2] + verts[f.v1][2] + verts[f.v2][2]) / 3.0;
    }

    // Sort faces by depth (furthest first)
    for (int i = 0; i < NUM_FACES - 1; i++) {
        for (int j = i + 1; j < NUM_FACES; j++) {
            if (depths[i].z > depths[j].z) std::swap(depths[i], depths[j]);
        }
    }

    double cx = img.width / 2.0;
    double cy = img.height / 2.0;

    // Draw faces
    for (int di = 0; di < NUM_FACES; di++) {
        int i = depths[di].idx;
        const Face& f = shipFaces[i];

        // Backface culling (check if normal points towards viewer)
        if (norms[i][2] <= 0) continue;

        // Calculate lighting (simple directional light from upper-left)
        double brightness = -norms[i][1] * 0.5 - norms[i][0] * 0.3 + 0.3;
        brightness = std::max(0.0, std::min(1.0, brightness));

        Color c = colorFrom12bit(f.color, brightness);

        // Project vertices (simple orthographic)
        double x0 = cx + verts[f.v0][0] * scale;
        double y0 = cy + verts[f.v0][1] * scale;
        double x1 = cx + verts[f.v1][0] * scale;
        double y1 = cy + verts[f.v1][1] * scale;
        double x2 = cx + verts[f.v2][0] * scale;
        double y2 = cy + verts[f.v2][1] * scale;

        img.drawTriangle(x0, y0, x1, y1, x2, y2, c);
    }
}

int main(int argc, char* argv[]) {
    const char* outputDir = ".";
    if (argc > 1) outputDir = argv[1];

    // Good viewing angle: tilted back and rotated for a heroic pose
    // Ship nose pointing upper-left, showing the top and side
    double rotX = -0.4;   // Tilt nose up
    double rotY = 0.5;    // Rotate to show side
    double rotZ = 0.15;   // Slight roll for dynamic look

    // Generate multiple icon sizes
    int sizes[] = {1024, 512, 256, 128, 64, 32, 16};
    const int numSizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < numSizes; s++) {
        int size = sizes[s];
        Image img(size, size);

        // Scale the ship to fit nicely in the icon (about 70% of size)
        double scale = size * 0.35;

        renderShip(img, rotX, rotY, rotZ, scale);

        char filename[256];
        snprintf(filename, sizeof(filename), "%s/icon_%dx%d.png", outputDir, size, size);

        if (img.savePNG(filename)) {
            printf("Generated %s\n", filename);
        } else {
            fprintf(stderr, "Failed to write %s\n", filename);
            return 1;
        }
    }

    printf("All icons generated successfully.\n");
    return 0;
}
