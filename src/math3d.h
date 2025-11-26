#ifndef LANDER_MATH3D_H
#define LANDER_MATH3D_H

#include "fixed.h"
#include "lookup_tables.h"

// =============================================================================
// 3D Vector and Matrix Math
// =============================================================================
//
// This module provides 3D vector and 3x3 matrix operations matching the
// original Lander's coordinate system and fixed-point arithmetic.
//
// The original uses a rotation matrix of the form:
//   [ xNoseV  xRoofV  xSideV ]   (nose = forward, roof = up, side = right)
//   [ yNoseV  yRoofV  ySideV ]
//   [ zNoseV  zRoofV  zSideV ]
//
// =============================================================================

// =============================================================================
// Vec3 - 3D Vector using Fixed-point coordinates
// =============================================================================

struct Vec3 {
    Fixed x, y, z;

    // Constructors
    constexpr Vec3() : x(), y(), z() {}
    constexpr Vec3(Fixed x_, Fixed y_, Fixed z_) : x(x_), y(y_), z(z_) {}

    // Vector addition
    constexpr Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    // Vector subtraction
    constexpr Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    // Negation
    constexpr Vec3 operator-() const {
        return Vec3(-x, -y, -z);
    }

    // Scalar multiplication
    constexpr Vec3 operator*(Fixed scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    // Scalar division
    constexpr Vec3 operator/(Fixed scalar) const {
        return Vec3(x / scalar, y / scalar, z / scalar);
    }

    // Compound assignment
    constexpr Vec3& operator+=(const Vec3& other) {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }

    constexpr Vec3& operator-=(const Vec3& other) {
        x -= other.x; y -= other.y; z -= other.z;
        return *this;
    }

    // Dot product
    constexpr Fixed dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    // Right shift (for scaling down)
    constexpr Vec3 operator>>(int shift) const {
        return Vec3(x >> shift, y >> shift, z >> shift);
    }

    // Left shift (for scaling up)
    constexpr Vec3 operator<<(int shift) const {
        return Vec3(x << shift, y << shift, z << shift);
    }
};

// =============================================================================
// Mat3x3 - 3x3 Rotation Matrix
// =============================================================================
//
// The matrix is stored in column-major order to match the original:
//   [ col[0].x  col[1].x  col[2].x ]   [ xNoseV  xRoofV  xSideV ]
//   [ col[0].y  col[1].y  col[2].y ] = [ yNoseV  yRoofV  ySideV ]
//   [ col[0].z  col[1].z  col[2].z ]   [ zNoseV  zRoofV  zSideV ]
//
// Column 0 = Nose vector (forward direction)
// Column 1 = Roof vector (up direction)
// Column 2 = Side vector (right direction)
//
// =============================================================================

struct Mat3x3 {
    Vec3 col[3];  // Three column vectors

    // Constructors
    Mat3x3() = default;

    // Construct from three column vectors
    constexpr Mat3x3(const Vec3& nose, const Vec3& roof, const Vec3& side)
        : col{nose, roof, side} {}

    // Access columns by name
    constexpr Vec3& nose() { return col[0]; }
    constexpr const Vec3& nose() const { return col[0]; }

    constexpr Vec3& roof() { return col[1]; }
    constexpr const Vec3& roof() const { return col[1]; }

    constexpr Vec3& side() { return col[2]; }
    constexpr const Vec3& side() const { return col[2]; }

    // Identity matrix
    static Mat3x3 identity() {
        return Mat3x3(
            Vec3(Fixed::fromInt(1), Fixed::fromInt(0), Fixed::fromInt(0)),
            Vec3(Fixed::fromInt(0), Fixed::fromInt(1), Fixed::fromInt(0)),
            Vec3(Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(1))
        );
    }

    // Multiply matrix by vector: result = M * v
    Vec3 operator*(const Vec3& v) const {
        // Each component of result is dot product of row with v
        // Row i is [col[0][i], col[1][i], col[2][i]]
        return Vec3(
            col[0].x * v.x + col[1].x * v.y + col[2].x * v.z,
            col[0].y * v.x + col[1].y * v.y + col[2].y * v.z,
            col[0].z * v.x + col[1].z * v.y + col[2].z * v.z
        );
    }

    // Multiply two matrices
    Mat3x3 operator*(const Mat3x3& other) const {
        Mat3x3 result;
        for (int c = 0; c < 3; c++) {
            result.col[c] = (*this) * other.col[c];
        }
        return result;
    }
};

// =============================================================================
// Rotation Matrix Calculation
// =============================================================================
//
// Calculate the rotation matrix from two angles (a and b).
// This matches the original CalculateRotationMatrix routine.
//
// The resulting matrix is:
//   [  cos(a)*cos(b)   -sin(a)*cos(b)    sin(b) ]
//   [       sin(a)           cos(a)         0   ]
//   [ -cos(a)*sin(b)    sin(a)*sin(b)    cos(b) ]
//
// Where angles are in the range 0 to 0xFFFFFFFF representing 0 to 2π.
//
// =============================================================================

Mat3x3 calculateRotationMatrix(int32_t angleA, int32_t angleB);

// =============================================================================
// Utility Functions
// =============================================================================

// Multiply two signed 32-bit values and return result
// This matches the original shift-and-add multiplication
// The result is scaled appropriately for the fixed-point format
Fixed multiplyFixed(Fixed a, Fixed b);

#endif // LANDER_MATH3D_H
