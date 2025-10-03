# Classic Glasses Filter v1.0.0

## Overview
Classic black-framed eyeglasses with reactive shake behavior on eye blink.

## Author
SegmeCam Team

## Category
Glasses / Accessories

## Description
Simple black-framed glasses that shake slightly when the user blinks. Demonstrates the behavior system with dual-eye blink detection.

## Assets
- **filter.json**: Filter metadata and configuration
- **glasses.obj**: 3D model geometry (simple rectangular frames)
- **glasses_texture.png**: Texture map (black frame material)
- **thumbnail.png**: Preview image (256x256)
- **icon.png**: Filter icon (64x64)

## Behaviors

### SHAKE on Blink (Left Eye)
- **Trigger**: eyeBlinkLeft blendshape
- **Threshold**: 0.7 (70% eye closure)
- **Intensity**: 2.0 (moderate shake)
- **Effect**: Random offset applied to glasses position

### SHAKE on Blink (Right Eye)
- **Trigger**: eyeBlinkRight blendshape
- **Threshold**: 0.7 (70% eye closure)
- **Intensity**: 2.0 (moderate shake)
- **Effect**: Random offset applied to glasses position

## Technical Details

### Anchor Point
- **nose_bridge**: Positioned at the center between the eyes
- MediaPipe landmarks: midpoint of left/right eye centers

### Geometry
- **Vertices**: 20
- **Faces**: 5 quads (left lens, right lens, bridge, left temple, right temple)
- **Scale**: Normalized to face proportions (±0.15 units)
- **Materials**: Simple black diffuse with specular highlights

### Performance
- **Polygon Count**: Very low (5 faces)
- **Texture Size**: 256x256 (minimal)
- **Estimated Frame Time**: <0.5ms per frame

## Usage

1. Place this directory in `assets/filters/`
2. ARFilterManager will auto-discover via `DiscoverFilters()`
3. Load filter: `LoadFilter("classic-glasses-v1")`
4. Face detection will trigger behaviors automatically

## Testing

```bash
# Build and run
bazel build -c opt //mediapipe/examples/desktop/segmecam:segmecam
./bazel-bin/mediapipe/examples/desktop/segmecam/segmecam

# Expected behavior:
# 1. Glasses appear on detected face
# 2. Blink left eye → glasses shake
# 3. Blink right eye → glasses shake
# 4. Both eyes → stronger shake effect
```

## Customization

### Adjust Shake Intensity
Edit `filter.json`:
```json
"behaviors": [{
  "intensity": 3.0  // Increase for stronger shake
}]
```

### Change Blink Threshold
```json
"behaviors": [{
  "threshold": 0.5  // Lower = more sensitive (triggers on partial blink)
}]
```

### Modify Appearance
- Edit `glasses.obj` for different frame shapes
- Replace `glasses_texture.png` for colors/patterns
- Adjust `materials` in filter.json for reflectivity

## Version History

### v1.0.0 (Initial)
- Basic rectangular frame geometry
- Dual-eye shake behaviors
- Black matte material
- Nose bridge anchor point

## Future Enhancements
- Rounded lens shapes
- Colored frame variants (tortoiseshell, metal, etc.)
- SCALE behavior on eyebrow raise (surprised expression)
- ROTATE behavior on head tilt
- Multiple style presets
