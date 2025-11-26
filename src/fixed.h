#ifndef LANDER_FIXED_H
#define LANDER_FIXED_H

#include <cstdint>

// =============================================================================
// Fixed-Point Math Library
// =============================================================================
//
// The original Lander uses 32-bit fixed-point arithmetic with an 8.24 format:
// - Upper 8 bits: signed integer part (-128 to 127)
// - Lower 24 bits: fractional part (1/16777216 precision)
//
// TILE_SIZE = 0x01000000 = 1.0 in this format
//
// This gives a range of approximately -128.0 to +127.999999940395
// with a precision of about 0.00000006 (6e-8)
//
// =============================================================================

class Fixed {
public:
    // The raw 32-bit value
    int32_t raw;

    // Number of fractional bits (24 bits = 16777216 units per integer)
    static constexpr int FRAC_BITS = 24;
    static constexpr int32_t ONE = 1 << FRAC_BITS;  // 0x01000000

    // Constructors
    constexpr Fixed() : raw(0) {}
    constexpr explicit Fixed(int32_t rawValue) : raw(rawValue) {}

    // Named constructors for clarity
    static constexpr Fixed fromRaw(int32_t rawValue) {
        return Fixed(rawValue);
    }

    static constexpr Fixed fromInt(int value) {
        return Fixed(static_cast<int32_t>(value) << FRAC_BITS);
    }

    static constexpr Fixed fromFloat(float value) {
        return Fixed(static_cast<int32_t>(value * ONE));
    }

    static constexpr Fixed fromDouble(double value) {
        return Fixed(static_cast<int32_t>(value * ONE));
    }

    // Conversion to other types
    constexpr int toInt() const {
        // Arithmetic right shift preserves sign
        return raw >> FRAC_BITS;
    }

    constexpr float toFloat() const {
        return static_cast<float>(raw) / ONE;
    }

    constexpr double toDouble() const {
        return static_cast<double>(raw) / ONE;
    }

    // Arithmetic operators
    constexpr Fixed operator+(Fixed other) const {
        return Fixed(raw + other.raw);
    }

    constexpr Fixed operator-(Fixed other) const {
        return Fixed(raw - other.raw);
    }

    constexpr Fixed operator-() const {
        return Fixed(-raw);
    }

    // Multiplication: (a * b) >> FRAC_BITS
    // Use 64-bit intermediate to avoid overflow
    constexpr Fixed operator*(Fixed other) const {
        int64_t result = static_cast<int64_t>(raw) * other.raw;
        return Fixed(static_cast<int32_t>(result >> FRAC_BITS));
    }

    // Division: (a << FRAC_BITS) / b
    // Use 64-bit intermediate to avoid overflow
    constexpr Fixed operator/(Fixed other) const {
        int64_t numerator = static_cast<int64_t>(raw) << FRAC_BITS;
        return Fixed(static_cast<int32_t>(numerator / other.raw));
    }

    // Compound assignment operators
    constexpr Fixed& operator+=(Fixed other) {
        raw += other.raw;
        return *this;
    }

    constexpr Fixed& operator-=(Fixed other) {
        raw -= other.raw;
        return *this;
    }

    constexpr Fixed& operator*=(Fixed other) {
        *this = *this * other;
        return *this;
    }

    constexpr Fixed& operator/=(Fixed other) {
        *this = *this / other;
        return *this;
    }

    // Comparison operators
    constexpr bool operator==(Fixed other) const { return raw == other.raw; }
    constexpr bool operator!=(Fixed other) const { return raw != other.raw; }
    constexpr bool operator<(Fixed other) const { return raw < other.raw; }
    constexpr bool operator>(Fixed other) const { return raw > other.raw; }
    constexpr bool operator<=(Fixed other) const { return raw <= other.raw; }
    constexpr bool operator>=(Fixed other) const { return raw >= other.raw; }

    // Bit shift operators (operate on raw value)
    constexpr Fixed operator>>(int shift) const {
        return Fixed(raw >> shift);
    }

    constexpr Fixed operator<<(int shift) const {
        return Fixed(raw << shift);
    }

    constexpr Fixed& operator>>=(int shift) {
        raw >>= shift;
        return *this;
    }

    constexpr Fixed& operator<<=(int shift) {
        raw <<= shift;
        return *this;
    }

    // Absolute value
    constexpr Fixed abs() const {
        return Fixed(raw >= 0 ? raw : -raw);
    }
};

// =============================================================================
// Game Constants (from Lander.arm)
// =============================================================================

namespace GameConstants {
    // Tile and landscape dimensions
    constexpr Fixed TILE_SIZE = Fixed::fromRaw(0x01000000);  // 1.0
    constexpr int TILES_X = 13;  // 12 tiles + 1 corner
    constexpr int TILES_Z = 11;  // 10 tiles + 1 corner

    // Altitudes (Y is inverted - larger values = lower altitude)
    constexpr Fixed LAUNCHPAD_ALTITUDE = Fixed::fromRaw(0x03500000);  // 3.3125
    constexpr Fixed SEA_LEVEL = Fixed::fromRaw(0x05500000);           // 5.3125
    constexpr Fixed HIGHEST_ALTITUDE = Fixed::fromRaw(0x34000000);    // 52.0

    // Physics
    constexpr Fixed LANDING_SPEED = Fixed::fromRaw(0x00200000);       // 0.125
    constexpr Fixed UNDERCARRIAGE_Y = Fixed::fromRaw(0x00640000);     // 0.390625
    constexpr Fixed SMOKE_RISING_SPEED = Fixed::fromRaw(0x00080000);  // 0.03125

    // Particles
    constexpr int MAX_PARTICLES = 484;

    // Calculated constants
    constexpr Fixed LAUNCHPAD_Y = Fixed::fromRaw(
        LAUNCHPAD_ALTITUDE.raw - UNDERCARRIAGE_Y.raw);
    constexpr Fixed LAUNCHPAD_SIZE = Fixed::fromRaw(TILE_SIZE.raw * 8);
    constexpr Fixed SPLASH_HEIGHT = Fixed::fromRaw(TILE_SIZE.raw / 16);
    constexpr Fixed CRASH_CLOUD_Y = Fixed::fromRaw(TILE_SIZE.raw * 5 / 16);
    constexpr Fixed SMOKE_HEIGHT = Fixed::fromRaw(TILE_SIZE.raw * 3 / 4);
    constexpr Fixed SAFE_HEIGHT = Fixed::fromRaw(TILE_SIZE.raw * 3 / 2);
    constexpr Fixed LAND_MID_HEIGHT = Fixed::fromRaw(TILE_SIZE.raw * 5);
    constexpr Fixed ROCK_HEIGHT = Fixed::fromRaw(TILE_SIZE.raw * 32);

    // Camera and landscape offsets
    constexpr Fixed CAMERA_PLAYER_Z = Fixed::fromRaw((TILES_Z - 6) * TILE_SIZE.raw);
    constexpr Fixed LANDSCAPE_Z_DEPTH = Fixed::fromRaw((TILES_Z - 1) * TILE_SIZE.raw);
    constexpr Fixed LANDSCAPE_X_WIDTH = Fixed::fromRaw((TILES_X - 2) * TILE_SIZE.raw);
    constexpr Fixed LANDSCAPE_X = Fixed::fromRaw(LANDSCAPE_X_WIDTH.raw / 2);
    constexpr Fixed LANDSCAPE_Y = Fixed::fromRaw(0);
    constexpr Fixed LANDSCAPE_Z = Fixed::fromRaw(
        LANDSCAPE_Z_DEPTH.raw + (10 * TILE_SIZE.raw));
    constexpr int HALF_TILES_X = TILES_X / 2;
    constexpr Fixed LANDSCAPE_X_HALF = Fixed::fromRaw(HALF_TILES_X * TILE_SIZE.raw);
    constexpr Fixed LANDSCAPE_Z_BEYOND = Fixed::fromRaw(
        LANDSCAPE_Z_DEPTH.raw + TILE_SIZE.raw);
    constexpr Fixed LANDSCAPE_Z_FRONT = Fixed::fromRaw(
        LANDSCAPE_Z.raw - LANDSCAPE_Z_DEPTH.raw);
    constexpr Fixed LANDSCAPE_Z_MID = Fixed::fromRaw(
        LANDSCAPE_Z.raw - CAMERA_PLAYER_Z.raw);
    constexpr Fixed PLAYER_FRONT_Z = Fixed::fromRaw((TILES_Z - 5) * TILE_SIZE.raw);
}

#endif // LANDER_FIXED_H
