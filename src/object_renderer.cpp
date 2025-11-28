#include "object_renderer.h"
#include "palette.h"
#include "landscape.h"

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

void drawObject(
    const ObjectBlueprint& blueprint,
    const Vec3& position,
    const Mat3x3& rotation,
    ScreenBuffer& screen
) {
    // Arrays to store projected vertices
    ProjectedVertex2D projectedVertices[MAX_VERTICES];

    bool isRotating = (blueprint.flags & ObjectFlags::ROTATES) != 0;

    // ==========================================================================
    // Part 1: Transform and project all vertices
    // ==========================================================================
    // Based on Lander.arm lines 5138-5268

    for (uint32_t i = 0; i < blueprint.vertexCount && i < MAX_VERTICES; i++) {
        const ObjectVertex& vertex = blueprint.vertices[i];

        // Get vertex coordinates as Fixed values
        Vec3 v;
        v.x = Fixed::fromRaw(vertex.x);
        v.y = Fixed::fromRaw(vertex.y);
        v.z = Fixed::fromRaw(vertex.z);

        // Transform vertex by rotation matrix if object rotates
        Vec3 rotated;
        if (isRotating) {
            rotated = rotation * v;
        } else {
            rotated = v;
        }

        // Add object position to get world coordinates
        Vec3 worldPos;
        worldPos.x = Fixed::fromRaw(position.x.raw + rotated.x.raw);
        worldPos.y = Fixed::fromRaw(position.y.raw + rotated.y.raw);
        worldPos.z = Fixed::fromRaw(position.z.raw + rotated.z.raw);

        // Project to screen
        ProjectedVertex projected = projectVertex(worldPos);

        projectedVertices[i].x = projected.screenX;
        projectedVertices[i].y = projected.screenY;
        projectedVertices[i].visible = projected.visible;
    }

    // ==========================================================================
    // Part 2: Process and draw each face
    // ==========================================================================
    // Based on Lander.arm lines 5284-5640

    for (uint32_t i = 0; i < blueprint.faceCount; i++) {
        const ObjectFace& face = blueprint.faces[i];

        // Get face normal as Vec3
        Vec3 normal;
        normal.x = Fixed::fromRaw(face.normalX);
        normal.y = Fixed::fromRaw(face.normalY);
        normal.z = Fixed::fromRaw(face.normalZ);

        // Transform normal by rotation matrix if object rotates
        Vec3 rotatedNormal;
        if (isRotating) {
            rotatedNormal = rotation * normal;
        } else {
            rotatedNormal = normal;
        }

        // =======================================================================
        // Backface culling
        // =======================================================================
        // Check if face is visible using dot product of:
        //   - Vector from camera to object (position)
        //   - Face normal (rotated)
        //
        // If dot product >= 0, face is pointing away from camera (hidden)
        // If dot product < 0, face is pointing towards camera (visible)
        //
        // Based on Lander.arm lines 5357-5379

        bool faceVisible = true;

        if (isRotating) {
            // Calculate dot product of position and rotatedNormal
            int64_t dotProduct =
                (static_cast<int64_t>(position.x.raw) * rotatedNormal.x.raw +
                 static_cast<int64_t>(position.y.raw) * rotatedNormal.y.raw +
                 static_cast<int64_t>(position.z.raw) * rotatedNormal.z.raw) >> 24;

            faceVisible = (dotProduct < 0);
        }
        // Static objects: all faces are visible (original sets R3 = -1)

        if (!faceVisible) {
            continue;
        }

        // Get vertex indices
        int v0 = face.vertex0;
        int v1 = face.vertex1;
        int v2 = face.vertex2;

        // Skip if any vertex is behind camera
        if (!projectedVertices[v0].visible ||
            !projectedVertices[v1].visible ||
            !projectedVertices[v2].visible) {
            continue;
        }

        // Calculate lit color
        Color litColor = calculateLitColor(face.color, rotatedNormal);

        // Draw the triangle
        screen.drawTriangle(
            projectedVertices[v0].x, projectedVertices[v0].y,
            projectedVertices[v1].x, projectedVertices[v1].y,
            projectedVertices[v2].x, projectedVertices[v2].y,
            litColor
        );
    }
}

// =============================================================================
// Shadow Rendering Implementation
// =============================================================================
//
// Port of DrawObject Part 4 from Lander.arm lines 5385-5465.
//
// Shadow algorithm:
// 1. For each vertex, calculate its world position (object position + rotated vertex)
// 2. Look up terrain altitude at that world (x, z) coordinate
// 3. Project the shadow vertex (same x, z but y = terrain altitude)
// 4. For each face whose rotated normal points upward (negative Y), draw it as
//    a black triangle using the shadow vertices
//
// =============================================================================

void drawObjectShadow(
    const ObjectBlueprint& blueprint,
    const Vec3& cameraRelPos,
    const Mat3x3& rotation,
    const Vec3& worldPos,
    const Vec3& cameraWorldPos,
    ScreenBuffer& screen
) {
    // Check if object has shadow rendering enabled
    // NO_SHADOW flag set = no shadow, NO_SHADOW flag clear = has shadow
    if ((blueprint.flags & ObjectFlags::NO_SHADOW) != 0) {
        return;
    }

    // Arrays to store shadow projected vertices
    ProjectedVertex2D shadowVertices[MAX_VERTICES];

    bool isRotating = (blueprint.flags & ObjectFlags::ROTATES) != 0;

    // ==========================================================================
    // Part 1: Calculate shadow vertices (project each vertex onto terrain)
    // ==========================================================================

    for (uint32_t i = 0; i < blueprint.vertexCount && i < MAX_VERTICES; i++) {
        const ObjectVertex& vertex = blueprint.vertices[i];

        // Get vertex coordinates as Fixed values
        Vec3 v;
        v.x = Fixed::fromRaw(vertex.x);
        v.y = Fixed::fromRaw(vertex.y);
        v.z = Fixed::fromRaw(vertex.z);

        // Transform vertex by rotation matrix if object rotates
        Vec3 rotated;
        if (isRotating) {
            rotated = rotation * v;
        } else {
            rotated = v;
        }

        // Calculate world position of this vertex
        Fixed worldX = Fixed::fromRaw(worldPos.x.raw + rotated.x.raw);
        Fixed worldZ = Fixed::fromRaw(worldPos.z.raw + rotated.z.raw);

        // Get terrain altitude at this world position
        Fixed terrainY = getLandscapeAltitude(worldX, worldZ);

        // Calculate camera-relative position of shadow vertex
        // X and Z are same as object vertex, Y is terrain altitude relative to camera
        Vec3 shadowPos;
        shadowPos.x = Fixed::fromRaw(cameraRelPos.x.raw + rotated.x.raw);
        shadowPos.y = Fixed::fromRaw(terrainY.raw - cameraWorldPos.y.raw);
        shadowPos.z = Fixed::fromRaw(cameraRelPos.z.raw + rotated.z.raw);

        // Project shadow vertex to screen
        ProjectedVertex projected = projectVertex(shadowPos);

        shadowVertices[i].x = projected.screenX;
        shadowVertices[i].y = projected.screenY;
        shadowVertices[i].visible = projected.visible;
    }

    // ==========================================================================
    // Part 2: Draw shadows for upward-facing faces
    // ==========================================================================
    // Only faces with normals pointing up (negative Y) cast shadows

    Color black = Color::black();

    for (uint32_t i = 0; i < blueprint.faceCount; i++) {
        const ObjectFace& face = blueprint.faces[i];

        // Get face normal as Vec3
        Vec3 normal;
        normal.x = Fixed::fromRaw(face.normalX);
        normal.y = Fixed::fromRaw(face.normalY);
        normal.z = Fixed::fromRaw(face.normalZ);

        // Transform normal by rotation matrix if object rotates
        Vec3 rotatedNormal;
        if (isRotating) {
            rotatedNormal = rotation * normal;
        } else {
            rotatedNormal = normal;
        }

        // Only draw shadow if face normal points upward (negative Y)
        // This matches the original: "we only draw shadows for faces that point up"
        if (rotatedNormal.y.raw >= 0) {
            continue;
        }

        // Get vertex indices
        int v0 = face.vertex0;
        int v1 = face.vertex1;
        int v2 = face.vertex2;

        // Skip if any shadow vertex is behind camera
        if (!shadowVertices[v0].visible ||
            !shadowVertices[v1].visible ||
            !shadowVertices[v2].visible) {
            continue;
        }

        // Draw the shadow triangle in black
        screen.drawTriangle(
            shadowVertices[v0].x, shadowVertices[v0].y,
            shadowVertices[v1].x, shadowVertices[v1].y,
            shadowVertices[v2].x, shadowVertices[v2].y,
            black
        );
    }
}
