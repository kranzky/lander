# Star Particles Design Document

## Overview

Stars provide visual feedback for speed and direction when the player is flying at high altitude where the landscape is no longer visible. They create a sense of motion through a 3D starfield effect.

## Visual Appearance

### Color
- Random shades of light grey
- RGB values in range approximately 160-255 for each channel
- Each star has a fixed color for its lifetime

### Size
- Random size between 1x1 and 3x3 pixels (at 320x256 base resolution)
- Size scales with display resolution
- Each star has a fixed size for its lifetime

### Fade Effect
- Stars quickly fade in when spawned
- Stars fade out before despawning
- Fade creates smooth appearance/disappearance rather than popping

## Spawn Behavior

### Altitude Threshold
- Stars begin appearing at approximately 20 tiles above ground level
- Density increases with altitude up to maximum height (engine cutout at 52 tiles)
- No stars visible below the threshold

### Spawn Volume
- Stars spawn in a cube centered on the player
- Cube dimensions: 24x24x24 tiles
- This creates a consistent starfield that moves with the player

### Spawn Rate and Limits
- Maximum approximately 100 stars active at any time
- Spawn rate adjusts based on:
  - Current altitude (more stars at higher altitude)
  - Current star count (spawn more when below target density)

## Lifetime and Animation

### Duration
- Each star lives for a few seconds (tunable, start with ~3 seconds)
- Lifetime has some random variation to prevent synchronized despawning

### Fade Timing
- Quick fade-in: approximately 0.2-0.3 seconds
- Longer fade-out: approximately 0.5-1.0 seconds
- Fade affects alpha/opacity

## Rendering

### Depth
- Stars are depth sorted with other particles
- Stars render behind the landscape when appropriate
- Full integration with existing depth sorting system

### Projection
- Stars use the same 3D projection as other particles
- Position is relative to camera, creating parallax effect
- Stars further from camera appear to move slower (depth cue)

## Implementation Considerations

### Particle System Integration
- Stars are a new particle type in the existing particle system
- Reuse existing particle infrastructure (spawning, updating, rendering, depth sorting)

### Performance
- 100 particles is modest; should have minimal performance impact
- Simple square rendering (no complex shapes)
- No physics simulation needed (stars are stationary in world space)

### Tunable Parameters
```cpp
namespace StarConfig {
    int maxStars = 100;              // Maximum active stars
    Fixed minAltitude = 20 tiles;    // Altitude where stars begin
    Fixed spawnRadius = 12 tiles;    // Half the spawn cube size
    float lifetimeMin = 2.5f;        // Minimum lifetime in seconds
    float lifetimeMax = 3.5f;        // Maximum lifetime in seconds
    float fadeInTime = 0.25f;        // Fade-in duration
    float fadeOutTime = 0.75f;       // Fade-out duration
    int minBrightness = 160;         // Minimum grey value
    int maxBrightness = 255;         // Maximum grey value
    int minSize = 1;                 // Minimum size in pixels
    int maxSize = 3;                 // Maximum size in pixels
}
```

## Altitude-Based Density

The star density should scale smoothly with altitude:

```
density = 0                          when altitude < minAltitude
density = (altitude - minAltitude) / (maxAltitude - minAltitude)
                                     when altitude >= minAltitude
```

Target star count = maxStars * density

This creates a gradual transition as the player ascends/descends.

## Resolved Design Decisions

- Stars are purely decorative (no gameplay feedback)
- Pure grey only (no color tint)
- Star size is uniformly random (no weighting)
- No movement or twinkling
- Linear fade in and fade out
- Toggle on/off with key 6 (enabled by default)
- Uses existing particle system with depth sorting

## Testing Checklist

- [ ] Stars invisible below altitude threshold
- [ ] Star density increases with altitude
- [ ] Stars provide sense of speed when moving
- [ ] Stars provide sense of direction (parallax)
- [ ] Fade in/out looks smooth
- [ ] No performance impact with 100 stars
- [ ] Stars render behind landscape
- [ ] Star count stays within limits
