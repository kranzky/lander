# Performance Optimization Suggestions

This document outlines potential performance improvements for the Lander port. The game currently runs well on modern hardware, but these optimizations could improve performance on lower-end systems or enable higher resolutions and frame rates.

## Current Performance Characteristics

- Target frame rates: 15, 30, 60, 120 FPS
- Resolution scales: 320x256, 640x512, 1280x1024
- Landscape sizes: 12x10 to 96x80 tiles
- Maximum particles: 900
- CPU-rendered (no GPU acceleration for game graphics)

## 1. Rendering Optimizations

### 1.1 Dirty Rectangle Tracking

**Current:** Entire screen is redrawn every frame.

**Improvement:** Track which regions changed and only redraw those.

```cpp
class DirtyRegionTracker {
    std::vector<SDL_Rect> dirtyRects;
    void markDirty(int x, int y, int w, int h);
    void markClean();
    bool isDirty(int x, int y) const;
};
```

**Benefits:**
- Reduced pixel writes for static scenes
- Particularly effective when paused
- May help with VSync efficiency

### 1.2 Texture Caching for Tiles

**Current:** Each landscape tile is rendered from scratch every frame.

**Improvement:** Cache rendered tiles and only update when camera moves.

```cpp
struct TileCache {
    SDL_Texture* texture;
    int worldTileX, worldTileZ;
    bool valid;
};
```

**Considerations:**
- Memory usage for tile textures
- Invalidation when tile changes (object destruction)
- May not help if camera constantly moving

### 1.3 Batch Triangle Rendering

**Current:** Triangles drawn individually with per-vertex operations.

**Improvement:** Batch triangles by color and draw in groups.

```cpp
struct TriangleBatch {
    Color color;
    std::vector<Vertex> vertices;  // Multiple triangles
    void flush(ScreenBuffer& screen);
};
```

**Benefits:**
- Reduced function call overhead
- Better cache locality
- Potential for SIMD optimization

### 1.4 Horizontal Line Optimization

**Current:** Lines drawn with per-pixel operations.

**Improvement:** Use memset or SIMD for long spans.

```cpp
void drawHorizontalLine(int x1, int x2, int y, uint32_t color) {
    if (x2 - x1 > 8) {
        // Use memset or SIMD for middle portion
        std::memset(&pixels[y * width + x1], color, (x2 - x1) * 4);
    } else {
        // Individual pixels for short spans
    }
}
```

## 2. Particle System Optimizations

### 2.1 Spatial Partitioning

**Current:** All particles checked against all collision surfaces.

**Improvement:** Use grid-based spatial partitioning.

```cpp
class ParticleGrid {
    static constexpr int CELL_SIZE = 16;  // tiles
    std::vector<int> cells[16][16];  // particle indices

    void insert(int particleIndex, const Vec3& pos);
    void query(const Vec3& pos, int radius, std::vector<int>& out);
};
```

**Benefits:**
- O(1) lookup for nearby particles
- Faster collision detection
- Better for large particle counts

### 2.2 Particle Pool Separation

**Current:** All particles in single array, type checked per particle.

**Improvement:** Separate pools for different particle types.

```cpp
struct ParticlePools {
    FixedArray<StandardParticle, 400> standard;
    FixedArray<StarParticle, 400> stars;
    FixedArray<RockParticle, 10> rocks;
};
```

**Benefits:**
- No type checking during iteration
- Better cache locality
- Type-specific optimizations possible

### 2.3 Star Visibility Culling

**Current:** All stars projected and visibility-tested.

**Improvement:** Early frustum culling in world space.

```cpp
bool isInViewFrustum(const Vec3& pos, const Camera& cam) {
    // Quick bounds check before projection
    Vec3 camRel = cam.worldToCamera(pos);
    if (camRel.z.raw <= 0) return false;
    // Approximate screen bounds check
    Fixed screenX = camRel.x / camRel.z;
    Fixed screenY = camRel.y / camRel.z;
    return abs(screenX) < threshold && abs(screenY) < threshold;
}
```

### 2.4 Particle Update SIMD

**Current:** Sequential particle position updates.

**Improvement:** Use SIMD for position += velocity.

```cpp
// Process 4 particles at once with SSE/AVX
void updatePositionsSIMD(Particle* particles, int count) {
    for (int i = 0; i < count; i += 4) {
        __m128i pos = _mm_load_si128((__m128i*)&particles[i].position);
        __m128i vel = _mm_load_si128((__m128i*)&particles[i].velocity);
        _mm_store_si128((__m128i*)&particles[i].position, _mm_add_epi32(pos, vel));
    }
}
```

## 3. Fixed-Point Optimizations

### 3.1 Multiplication Optimization

**Current:** Full 64-bit multiplication for fixed-point.

**Improvement:** Use platform-specific intrinsics.

```cpp
// ARM NEON
int32_t fixedMul(int32_t a, int32_t b) {
    int64_t result = vmull_s32(vdup_n_s32(a), vdup_n_s32(b));
    return vget_lane_s32(vshrn_n_s64(result, 24), 0);
}
```

### 3.2 Division Avoidance

**Current:** Some divisions in hot paths.

**Improvement:** Replace with multiplication by reciprocal.

```cpp
// Instead of: x / 64
// Use: x >> 6 (already done in most places)

// For variable divisors, precompute reciprocals
Fixed reciprocal = Fixed::fromRaw(0x01000000 / divisor.raw);
result = a * reciprocal;
```

### 3.3 Lookup Table Expansion

**Current:** Sin/cos lookups with 1024 entries.

**Improvement:** Could use larger tables to reduce interpolation.

**Consideration:** May hurt cache performance; profile to verify benefit.

## 4. Memory Optimizations

### 4.1 Object Map Compression

**Current:** 256x256 byte array = 64KB.

**Improvement:** Run-length encoding or sparse representation.

```cpp
struct SparseObjectMap {
    std::unordered_map<uint16_t, uint8_t> objects;  // key = (x << 8) | z

    uint8_t get(int x, int z) const {
        auto it = objects.find((x << 8) | z);
        return it != objects.end() ? it->second : 0;
    }
};
```

**Benefits:**
- Much smaller memory footprint
- Most tiles are empty
- Could allow larger worlds

### 4.2 Vertex Data Optimization

**Current:** Object vertices as separate x, y, z integers.

**Improvement:** Pack into struct with better alignment.

```cpp
struct PackedVertex {
    int16_t x, y, z;  // 6 bytes instead of 12
    int16_t padding;
};
```

### 4.3 Screen Buffer Optimization

**Current:** RGBA8888 format (4 bytes per pixel).

**Improvement:** Consider RGB565 for memory bandwidth.

```cpp
// 2 bytes per pixel instead of 4
uint16_t packRGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
```

**Considerations:**
- Reduced color precision
- SDL texture format compatibility
- May complicate blending

## 5. Algorithm Optimizations

### 5.1 Landscape Altitude Caching

**Current:** Fourier synthesis computed on every altitude query.

**Improvement:** Cache recent altitude queries.

```cpp
class AltitudeCache {
    static constexpr int CACHE_SIZE = 256;
    struct Entry { int32_t x, z; Fixed altitude; };
    Entry cache[CACHE_SIZE];

    Fixed get(Fixed x, Fixed z);
};
```

**Benefits:**
- Same altitude often queried multiple times per frame
- Especially helpful for shadow calculations

### 5.2 Backface Culling Early-Out

**Current:** Full face processing before culling check.

**Improvement:** Check facing before vertex transformation.

```cpp
// Pre-transform normal check
Vec3 viewNormal = rotationMatrix * faceNormal;
if (viewNormal.z.raw >= 0) continue;  // Facing away
// Now do expensive vertex transformations
```

### 5.3 Object Visibility Pre-check

**Current:** All visible-tile objects processed.

**Improvement:** Quick bounding-sphere visibility check.

```cpp
bool isObjectVisible(const Vec3& objPos, Fixed radius, const Camera& cam) {
    Vec3 camRel = cam.worldToCamera(objPos);
    if (camRel.z.raw <= radius.raw) return false;  // Behind camera
    // Quick screen bounds estimate
    return true;  // Detailed check only if likely visible
}
```

## 6. Threading Opportunities

### 6.1 Particle Update Threading

**Current:** Sequential particle updates.

**Improvement:** Parallel update with thread pool.

```cpp
void updateParticlesParallel() {
    int count = particleSystem.getParticleCount();
    int perThread = count / numThreads;

    for (int t = 0; t < numThreads; t++) {
        threadPool.submit([=]() {
            updateParticleRange(t * perThread, (t+1) * perThread);
        });
    }
    threadPool.waitAll();
}
```

**Considerations:**
- Particle removal complicates threading
- May not be worth overhead for <1000 particles

### 6.2 Render Thread Separation

**Current:** Update and render on same thread.

**Improvement:** Separate threads for update and render.

```cpp
// Thread 1: Game logic
while (running) {
    update();
    swapBuffers();
}

// Thread 2: Rendering
while (running) {
    waitForBuffer();
    render();
    present();
}
```

**Considerations:**
- Complex synchronization
- Adds one frame of latency
- May help with VSync stalls

## Priority Ranking

| Priority | Optimization | Effort | Expected Impact |
|----------|-------------|--------|-----------------|
| High | Particle pool separation | Medium | Medium |
| High | Altitude caching | Low | Medium |
| Medium | Horizontal line SIMD | Low | Low-Medium |
| Medium | Early backface culling | Low | Low |
| Medium | Star visibility culling | Medium | Low-Medium |
| Low | Dirty rectangle tracking | High | Variable |
| Low | Tile caching | High | Low |
| Low | Threading | Very High | Variable |

## Profiling Recommendations

Before implementing optimizations:

1. **Profile first** - Use Instruments (macOS), perf (Linux), or VTune (Windows)
2. **Identify hotspots** - Focus on code taking >5% of frame time
3. **Measure baseline** - Record FPS at various settings
4. **A/B test changes** - Compare before/after with identical workloads
5. **Test on target hardware** - Low-end hardware may have different bottlenecks

## Conclusion

The current implementation is functional and runs well on modern hardware. Most optimizations would provide marginal benefit unless targeting very low-end systems or much higher resolutions. The highest-value improvements are likely:

1. Particle pool separation (cleaner code and slight speedup)
2. Altitude caching (avoids redundant calculations)
3. Early visibility culling (reduces wasted work)

Threading should only be considered if profiling shows CPU-bound frames, as the overhead may outweigh benefits for this workload size.
