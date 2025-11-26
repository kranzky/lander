#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "../src/math3d.h"

// =============================================================================
// Simple Test Framework
// =============================================================================

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::printf("  %s... ", #name); \
    testsRun++; \
    try { \
        test_##name(); \
        testsPassed++; \
        std::printf("PASSED\n"); \
    } catch (...) { \
        testsFailed++; \
        std::printf("FAILED (exception)\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        std::printf("FAILED\n    Assertion failed: %s\n    at %s:%d\n", \
                    #cond, __FILE__, __LINE__); \
        testsFailed++; \
        testsRun++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::printf("FAILED\n    Expected %s == %s\n    at %s:%d\n", \
                    #a, #b, __FILE__, __LINE__); \
        testsFailed++; \
        testsRun++; \
        return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tolerance) do { \
    if (std::fabs((a) - (b)) > (tolerance)) { \
        std::printf("FAILED\n    Expected %s ~= %s (tolerance %f)\n    Got %f vs %f\n    at %s:%d\n", \
                    #a, #b, (double)(tolerance), (double)(a), (double)(b), __FILE__, __LINE__); \
        testsFailed++; \
        testsRun++; \
        return; \
    } \
} while(0)

// =============================================================================
// Vec3 Tests
// =============================================================================

TEST(vec3_construction) {
    Vec3 v1;
    ASSERT_EQ(v1.x.raw, 0);
    ASSERT_EQ(v1.y.raw, 0);
    ASSERT_EQ(v1.z.raw, 0);

    Vec3 v2(Fixed::fromInt(1), Fixed::fromInt(2), Fixed::fromInt(3));
    ASSERT_EQ(v2.x.toInt(), 1);
    ASSERT_EQ(v2.y.toInt(), 2);
    ASSERT_EQ(v2.z.toInt(), 3);
}

TEST(vec3_addition) {
    Vec3 a(Fixed::fromInt(1), Fixed::fromInt(2), Fixed::fromInt(3));
    Vec3 b(Fixed::fromInt(4), Fixed::fromInt(5), Fixed::fromInt(6));
    Vec3 c = a + b;

    ASSERT_EQ(c.x.toInt(), 5);
    ASSERT_EQ(c.y.toInt(), 7);
    ASSERT_EQ(c.z.toInt(), 9);
}

TEST(vec3_subtraction) {
    Vec3 a(Fixed::fromInt(10), Fixed::fromInt(20), Fixed::fromInt(30));
    Vec3 b(Fixed::fromInt(3), Fixed::fromInt(5), Fixed::fromInt(7));
    Vec3 c = a - b;

    ASSERT_EQ(c.x.toInt(), 7);
    ASSERT_EQ(c.y.toInt(), 15);
    ASSERT_EQ(c.z.toInt(), 23);
}

TEST(vec3_negation) {
    Vec3 a(Fixed::fromInt(1), Fixed::fromInt(-2), Fixed::fromInt(3));
    Vec3 b = -a;

    ASSERT_EQ(b.x.toInt(), -1);
    ASSERT_EQ(b.y.toInt(), 2);
    ASSERT_EQ(b.z.toInt(), -3);
}

TEST(vec3_scalar_multiply) {
    Vec3 a(Fixed::fromInt(2), Fixed::fromInt(3), Fixed::fromInt(4));
    Vec3 b = a * Fixed::fromInt(5);

    ASSERT_EQ(b.x.toInt(), 10);
    ASSERT_EQ(b.y.toInt(), 15);
    ASSERT_EQ(b.z.toInt(), 20);
}

TEST(vec3_scalar_divide) {
    Vec3 a(Fixed::fromInt(10), Fixed::fromInt(20), Fixed::fromInt(30));
    Vec3 b = a / Fixed::fromInt(2);

    ASSERT_EQ(b.x.toInt(), 5);
    ASSERT_EQ(b.y.toInt(), 10);
    ASSERT_EQ(b.z.toInt(), 15);
}

TEST(vec3_dot_product) {
    Vec3 a(Fixed::fromInt(1), Fixed::fromInt(2), Fixed::fromInt(3));
    Vec3 b(Fixed::fromInt(4), Fixed::fromInt(5), Fixed::fromInt(6));
    Fixed dot = a.dot(b);

    // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    ASSERT_EQ(dot.toInt(), 32);
}

TEST(vec3_dot_product_perpendicular) {
    // X and Y axes should be perpendicular
    Vec3 xAxis(Fixed::fromInt(1), Fixed::fromInt(0), Fixed::fromInt(0));
    Vec3 yAxis(Fixed::fromInt(0), Fixed::fromInt(1), Fixed::fromInt(0));
    Fixed dot = xAxis.dot(yAxis);

    ASSERT_EQ(dot.raw, 0);
}

TEST(vec3_compound_assignment) {
    Vec3 a(Fixed::fromInt(1), Fixed::fromInt(2), Fixed::fromInt(3));
    Vec3 b(Fixed::fromInt(4), Fixed::fromInt(5), Fixed::fromInt(6));

    a += b;
    ASSERT_EQ(a.x.toInt(), 5);
    ASSERT_EQ(a.y.toInt(), 7);
    ASSERT_EQ(a.z.toInt(), 9);

    a -= b;
    ASSERT_EQ(a.x.toInt(), 1);
    ASSERT_EQ(a.y.toInt(), 2);
    ASSERT_EQ(a.z.toInt(), 3);
}

TEST(vec3_shift) {
    Vec3 a(Fixed::fromRaw(0x08000000), Fixed::fromRaw(0x10000000), Fixed::fromRaw(0x20000000));
    Vec3 b = a >> 1;

    ASSERT_EQ(b.x.raw, 0x04000000);
    ASSERT_EQ(b.y.raw, 0x08000000);
    ASSERT_EQ(b.z.raw, 0x10000000);

    Vec3 c = b << 2;
    ASSERT_EQ(c.x.raw, 0x10000000);
    ASSERT_EQ(c.y.raw, 0x20000000);
    ASSERT_EQ(c.z.raw, 0x40000000);
}

// =============================================================================
// Mat3x3 Tests
// =============================================================================

TEST(mat3x3_identity) {
    Mat3x3 I = Mat3x3::identity();

    // Diagonal should be 1
    ASSERT_EQ(I.col[0].x.toInt(), 1);
    ASSERT_EQ(I.col[1].y.toInt(), 1);
    ASSERT_EQ(I.col[2].z.toInt(), 1);

    // Off-diagonal should be 0
    ASSERT_EQ(I.col[0].y.raw, 0);
    ASSERT_EQ(I.col[0].z.raw, 0);
    ASSERT_EQ(I.col[1].x.raw, 0);
    ASSERT_EQ(I.col[1].z.raw, 0);
    ASSERT_EQ(I.col[2].x.raw, 0);
    ASSERT_EQ(I.col[2].y.raw, 0);
}

TEST(mat3x3_identity_multiply_vector) {
    Mat3x3 I = Mat3x3::identity();
    Vec3 v(Fixed::fromInt(3), Fixed::fromInt(5), Fixed::fromInt(7));
    Vec3 result = I * v;

    ASSERT_EQ(result.x.toInt(), 3);
    ASSERT_EQ(result.y.toInt(), 5);
    ASSERT_EQ(result.z.toInt(), 7);
}

TEST(mat3x3_column_access) {
    Mat3x3 m;
    m.nose() = Vec3(Fixed::fromInt(1), Fixed::fromInt(2), Fixed::fromInt(3));
    m.roof() = Vec3(Fixed::fromInt(4), Fixed::fromInt(5), Fixed::fromInt(6));
    m.side() = Vec3(Fixed::fromInt(7), Fixed::fromInt(8), Fixed::fromInt(9));

    ASSERT_EQ(m.col[0].x.toInt(), 1);
    ASSERT_EQ(m.col[1].y.toInt(), 5);
    ASSERT_EQ(m.col[2].z.toInt(), 9);
}

TEST(mat3x3_multiply_vector) {
    // Simple rotation matrix that swaps X and Y
    Mat3x3 m(
        Vec3(Fixed::fromInt(0), Fixed::fromInt(1), Fixed::fromInt(0)),  // nose
        Vec3(Fixed::fromInt(1), Fixed::fromInt(0), Fixed::fromInt(0)),  // roof
        Vec3(Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(1))   // side
    );

    Vec3 v(Fixed::fromInt(3), Fixed::fromInt(5), Fixed::fromInt(7));
    Vec3 result = m * v;

    // Matrix is:
    // [ 0 1 0 ]   [ 3 ]   [ 5 ]
    // [ 1 0 0 ] * [ 5 ] = [ 3 ]
    // [ 0 0 1 ]   [ 7 ]   [ 7 ]
    ASSERT_EQ(result.x.toInt(), 5);
    ASSERT_EQ(result.y.toInt(), 3);
    ASSERT_EQ(result.z.toInt(), 7);
}

TEST(mat3x3_multiply_matrices) {
    Mat3x3 I = Mat3x3::identity();
    Mat3x3 m(
        Vec3(Fixed::fromInt(1), Fixed::fromInt(2), Fixed::fromInt(3)),
        Vec3(Fixed::fromInt(4), Fixed::fromInt(5), Fixed::fromInt(6)),
        Vec3(Fixed::fromInt(7), Fixed::fromInt(8), Fixed::fromInt(9))
    );

    // I * M = M
    Mat3x3 result = I * m;
    ASSERT_EQ(result.col[0].x.toInt(), 1);
    ASSERT_EQ(result.col[0].y.toInt(), 2);
    ASSERT_EQ(result.col[0].z.toInt(), 3);
    ASSERT_EQ(result.col[1].x.toInt(), 4);
    ASSERT_EQ(result.col[1].y.toInt(), 5);
    ASSERT_EQ(result.col[1].z.toInt(), 6);
}

// =============================================================================
// Rotation Matrix Tests
// =============================================================================

TEST(rotation_matrix_zero_angles) {
    // With both angles at 0:
    // cos(0) = 1, sin(0) = 0
    // Matrix should be:
    // [ 1*1   -0*1   0 ]   [ 1 0 0 ]
    // [  0      1    0 ] = [ 0 1 0 ]
    // [-1*0    0*0   1 ]   [ 0 0 1 ]
    Mat3x3 m = calculateRotationMatrix(0, 0);

    // Should be approximately identity
    ASSERT_NEAR(m.col[0].x.toFloat(), 1.0f, 0.01f);
    ASSERT_NEAR(m.col[1].y.toFloat(), 1.0f, 0.01f);
    ASSERT_NEAR(m.col[2].z.toFloat(), 1.0f, 0.01f);

    ASSERT_NEAR(m.col[0].y.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[0].z.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[1].x.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[1].z.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[2].x.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[2].y.toFloat(), 0.0f, 0.01f);
}

TEST(rotation_matrix_90_degree_a) {
    // Angle A = 90 degrees = 0x40000000 (quarter of full circle)
    // cos(90) = 0, sin(90) = 1
    // With B = 0: cos(0) = 1, sin(0) = 0
    // Matrix should be:
    // [ 0*1   -1*1   0 ]   [ 0 -1 0 ]
    // [  1      0    0 ] = [ 1  0 0 ]
    // [-0*0    1*0   1 ]   [ 0  0 1 ]
    Mat3x3 m = calculateRotationMatrix(0x40000000, 0);

    ASSERT_NEAR(m.col[0].x.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[0].y.toFloat(), 1.0f, 0.01f);
    ASSERT_NEAR(m.col[0].z.toFloat(), 0.0f, 0.01f);

    ASSERT_NEAR(m.col[1].x.toFloat(), -1.0f, 0.01f);
    ASSERT_NEAR(m.col[1].y.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[1].z.toFloat(), 0.0f, 0.01f);

    ASSERT_NEAR(m.col[2].x.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[2].y.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[2].z.toFloat(), 1.0f, 0.01f);
}

TEST(rotation_matrix_90_degree_b) {
    // Angle B = 90 degrees = 0x40000000
    // With A = 0: cos(0) = 1, sin(0) = 0
    // cos(90) = 0, sin(90) = 1
    // Matrix should be:
    // [ 1*0   -0*0   1 ]   [ 0 0 1 ]
    // [  0      1    0 ] = [ 0 1 0 ]
    // [-1*1    0*1   0 ]   [-1 0 0 ]
    Mat3x3 m = calculateRotationMatrix(0, 0x40000000);

    ASSERT_NEAR(m.col[0].x.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[0].y.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[0].z.toFloat(), -1.0f, 0.01f);

    ASSERT_NEAR(m.col[1].x.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[1].y.toFloat(), 1.0f, 0.01f);
    ASSERT_NEAR(m.col[1].z.toFloat(), 0.0f, 0.01f);

    ASSERT_NEAR(m.col[2].x.toFloat(), 1.0f, 0.01f);
    ASSERT_NEAR(m.col[2].y.toFloat(), 0.0f, 0.01f);
    ASSERT_NEAR(m.col[2].z.toFloat(), 0.0f, 0.01f);
}

TEST(rotation_matrix_45_degrees) {
    // Angle A = 45 degrees = 0x20000000
    // cos(45) ≈ 0.707, sin(45) ≈ 0.707
    // With B = 0: cos(0) = 1, sin(0) = 0
    Mat3x3 m = calculateRotationMatrix(0x20000000, 0);

    float cos45 = 0.7071f;
    float sin45 = 0.7071f;

    ASSERT_NEAR(m.col[0].x.toFloat(), cos45, 0.02f);  // cos(a)*cos(b)
    ASSERT_NEAR(m.col[0].y.toFloat(), sin45, 0.02f);  // sin(a)
    ASSERT_NEAR(m.col[1].x.toFloat(), -sin45, 0.02f); // -sin(a)*cos(b)
    ASSERT_NEAR(m.col[1].y.toFloat(), cos45, 0.02f);  // cos(a)
}

TEST(rotation_preserves_vector_length) {
    // A rotation should preserve vector length
    Mat3x3 m = calculateRotationMatrix(0x30000000, 0x15000000);  // Some arbitrary angles

    Vec3 v(Fixed::fromFloat(1.0f), Fixed::fromFloat(0.0f), Fixed::fromFloat(0.0f));
    Vec3 rotated = m * v;

    // Calculate length squared
    Fixed lenSq = rotated.dot(rotated);
    ASSERT_NEAR(lenSq.toFloat(), 1.0f, 0.05f);  // Allow some fixed-point error
}

TEST(rotation_matrix_vector_transform) {
    // Rotate a unit X vector by 90 degrees around Z axis (angle A)
    Mat3x3 m = calculateRotationMatrix(0x40000000, 0);  // 90 degrees in A

    Vec3 unitX(Fixed::fromFloat(1.0f), Fixed::fromFloat(0.0f), Fixed::fromFloat(0.0f));
    Vec3 result = m * unitX;

    // Should now point in Y direction
    ASSERT_NEAR(result.x.toFloat(), 0.0f, 0.02f);
    ASSERT_NEAR(result.y.toFloat(), 1.0f, 0.02f);
    ASSERT_NEAR(result.z.toFloat(), 0.0f, 0.02f);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::printf("Math3D Unit Tests\n");
    std::printf("=================\n\n");

    std::printf("Vec3 tests:\n");
    RUN_TEST(vec3_construction);
    RUN_TEST(vec3_addition);
    RUN_TEST(vec3_subtraction);
    RUN_TEST(vec3_negation);
    RUN_TEST(vec3_scalar_multiply);
    RUN_TEST(vec3_scalar_divide);
    RUN_TEST(vec3_dot_product);
    RUN_TEST(vec3_dot_product_perpendicular);
    RUN_TEST(vec3_compound_assignment);
    RUN_TEST(vec3_shift);

    std::printf("\nMat3x3 tests:\n");
    RUN_TEST(mat3x3_identity);
    RUN_TEST(mat3x3_identity_multiply_vector);
    RUN_TEST(mat3x3_column_access);
    RUN_TEST(mat3x3_multiply_vector);
    RUN_TEST(mat3x3_multiply_matrices);

    std::printf("\nRotation matrix tests:\n");
    RUN_TEST(rotation_matrix_zero_angles);
    RUN_TEST(rotation_matrix_90_degree_a);
    RUN_TEST(rotation_matrix_90_degree_b);
    RUN_TEST(rotation_matrix_45_degrees);
    RUN_TEST(rotation_preserves_vector_length);
    RUN_TEST(rotation_matrix_vector_transform);

    std::printf("\n=================\n");
    std::printf("Tests: %d total, %d passed, %d failed\n",
                testsRun, testsPassed, testsFailed);

    return testsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
