#include "object_renderer.h"
#include "palette.h"
#include "landscape.h"
#include "graphics_buffer.h"
#include <algorithm>

// =============================================================================
// 3D Object Renderer Implementation
// =============================================================================
//
// Direct port of DrawObject from Lander.arm lines 4979-5640.
//
// The original uses an intricate precision-maximizing technique where coordinates
// are scaled up as high as possible before calculations. We use 64-bit
// intermediates instead, which gives equivalent precision with simpler code.
//
// =============================================================================

// Calculate lit color from base color and face normal
// Based on Lander.arm lines 5509-5576
Color calculateLitColor(uint16_t baseColor, const Vec3& rotatedNormal) {
    // Extract RGB components from 12-bit color (0xRGB format)
    int r = (baseColor >> 8) & 0xF;
    int g = (baseColor >> 4) & 0xF;
    int b = baseColor & 0xF;

    // Calculate brightness based on normal direction
    // Light source is from above (negative Y) and slightly to the left (negative X)
    //
    // Original formula (lines 5512-5535):
    //   brightness = (0x80000000 - yVertex) >> 28
    //   if xVertex < 0: brightness += 1
    //   brightness = max(0, brightness - 5)
    //
    // This gives brightness 0-4 based on how much the normal points up

    // Get the Y component of the rotated normal
    // More negative Y = pointing more upward = brighter
    int32_t yNormal = rotatedNormal.y.raw;

    // Calculate base brightness: higher when normal points up (negative Y)
    // Range: 0 to ~8 before adjustment
    int brightness = static_cast<int>((0x80000000u - static_cast<uint32_t>(yNormal)) >> 28);

    // Slightly brighter if normal points left (negative X)
    if (rotatedNormal.x.raw < 0) {
        brightness += 1;
    }

    // Adjust to final range 0-4
    brightness = brightness - 5;
    if (brightness < 0) brightness = 0;
    if (brightness > 4) brightness = 4;

    // Add brightness to each channel, clamping to 15
    r = r + brightness;
    if (r > 15) r = 15;

    g = g + brightness;
    if (g > 15) g = 15;

    b = b + brightness;
    if (b > 15) b = 15;

    // Convert to Color struct (8 bits per channel)
    // Scale 4-bit values (0-15) to 8-bit (0-255) by multiplying by 17
    return Color(
        static_cast<uint8_t>(r * 17),
        static_cast<uint8_t>(g * 17),
        static_cast<uint8_t>(b * 17)
    );
}

// =============================================================================
// Shared Rendering Core
// =============================================================================
//
// Drawing and buffering differ only in where each triangle goes, so both are
// built on these templates, which pass each visible triangle to `emit`.
//
// =============================================================================

namespace
{
    Vec3 toVec3(int32_t x, int32_t y, int32_t z)
    {
        return Vec3(Fixed::fromRaw(x), Fixed::fromRaw(y), Fixed::fromRaw(z));
    }

    // Blueprint vector in world orientation (static objects don't rotate)
    Vec3 orient(const ObjectBlueprint& blueprint, const Mat3x3& rotation, const Vec3& v)
    {
        return (blueprint.flags & ObjectFlags::ROTATES) ? rotation * v : v;
    }

    bool faceIsDrawable(const ProjectedVertex2D* vertices, const ObjectFace& face)
    {
        return vertices[face.vertex0].visible &&
               vertices[face.vertex1].visible &&
               vertices[face.vertex2].visible;
    }

    template <typename Emit>
    void emitFace(const ProjectedVertex2D* vertices, const ObjectFace& face, Color color, Emit emit)
    {
        emit(vertices[face.vertex0].x, vertices[face.vertex0].y,
             vertices[face.vertex1].x, vertices[face.vertex1].y,
             vertices[face.vertex2].x, vertices[face.vertex2].y,
             color);
    }

    // Port of DrawObject Parts 1-3 (Lander.arm lines 5138-5640): transform and
    // project the vertices, then emit each face that faces the camera, lit by
    // its normal
    template <typename Emit>
    void renderObject(const ObjectBlueprint& blueprint, const Vec3& position,
                      const Mat3x3& rotation, Emit emit)
    {
        ProjectedVertex2D projected[MAX_VERTICES];
        int vertexCount = std::min<int>(blueprint.vertexCount, MAX_VERTICES);

        for (int i = 0; i < vertexCount; i++) {
            const ObjectVertex& vertex = blueprint.vertices[i];
            Vec3 v = orient(blueprint, rotation, toVec3(vertex.x, vertex.y, vertex.z));
            ProjectedVertex p = projectVertex(position + v);
            projected[i] = {p.screenX, p.screenY, p.visible};
        }

        bool isRotating = (blueprint.flags & ObjectFlags::ROTATES) != 0;

        for (uint32_t i = 0; i < blueprint.faceCount; i++) {
            const ObjectFace& face = blueprint.faces[i];
            Vec3 normal = orient(blueprint, rotation, toVec3(face.normalX, face.normalY, face.normalZ));

            // Backface culling (Lander.arm lines 5357-5379): a rotating object's
            // face is visible when its normal points towards the camera. Static
            // objects draw every face (the original sets R3 = -1).
            if (isRotating) {
                int64_t dot = static_cast<int64_t>(position.x.raw) * normal.x.raw +
                              static_cast<int64_t>(position.y.raw) * normal.y.raw +
                              static_cast<int64_t>(position.z.raw) * normal.z.raw;
                if ((dot >> 24) >= 0) {
                    continue;
                }
            }

            if (faceIsDrawable(projected, face)) {
                emitFace(projected, face, calculateLitColor(face.color, normal), emit);
            }
        }
    }

    // Port of DrawObject Part 4 (Lander.arm lines 5385-5465): drop each vertex
    // onto the terrain below it, then emit a black triangle for every face that
    // points up
    template <typename Emit>
    void renderShadow(const ObjectBlueprint& blueprint, const Vec3& cameraRelPos,
                      const Mat3x3& rotation, const Vec3& worldPos,
                      const Vec3& cameraWorldPos, Emit emit)
    {
        if (blueprint.flags & ObjectFlags::NO_SHADOW) {
            return;
        }

        ProjectedVertex2D projected[MAX_VERTICES];
        int vertexCount = std::min<int>(blueprint.vertexCount, MAX_VERTICES);

        for (int i = 0; i < vertexCount; i++) {
            const ObjectVertex& vertex = blueprint.vertices[i];
            Vec3 v = orient(blueprint, rotation, toVec3(vertex.x, vertex.y, vertex.z));

            Fixed terrainY = getLandscapeAltitude(worldPos.x + v.x, worldPos.z + v.z);
            Vec3 shadowPos(cameraRelPos.x + v.x, terrainY - cameraWorldPos.y, cameraRelPos.z + v.z);

            ProjectedVertex p = projectVertex(shadowPos);
            projected[i] = {p.screenX, p.screenY, p.visible};
        }

        for (uint32_t i = 0; i < blueprint.faceCount; i++) {
            const ObjectFace& face = blueprint.faces[i];
            Vec3 normal = orient(blueprint, rotation, toVec3(face.normalX, face.normalY, face.normalZ));

            // Only faces pointing up (negative Y) cast shadows
            if (normal.y.raw < 0 && faceIsDrawable(projected, face)) {
                emitFace(projected, face, Color::black(), emit);
            }
        }
    }
}

// =============================================================================
// Public Entry Points
// =============================================================================

void drawObject(const ObjectBlueprint& blueprint, const Vec3& position,
                const Mat3x3& rotation, ScreenBuffer& screen)
{
    renderObject(blueprint, position, rotation,
                 [&screen](int x1, int y1, int x2, int y2, int x3, int y3, Color color) {
                     screen.drawTriangle(x1, y1, x2, y2, x3, y3, color);
                 });
}

void bufferObject(const ObjectBlueprint& blueprint, const Vec3& position,
                  const Mat3x3& rotation, int row)
{
    renderObject(blueprint, position, rotation,
                 [row](int x1, int y1, int x2, int y2, int x3, int y3, Color color) {
                     graphicsBuffers.addTriangle(row, x1, y1, x2, y2, x3, y3, color);
                 });
}

void bufferObjectShadow(const ObjectBlueprint& blueprint, const Vec3& cameraRelPos,
                        const Mat3x3& rotation, const Vec3& worldPos,
                        const Vec3& cameraWorldPos, int row)
{
    renderShadow(blueprint, cameraRelPos, rotation, worldPos, cameraWorldPos,
                 [row](int x1, int y1, int x2, int y2, int x3, int y3, Color color) {
                     graphicsBuffers.addShadowTriangle(row, x1, y1, x2, y2, x3, y3, color);
                 });
}
