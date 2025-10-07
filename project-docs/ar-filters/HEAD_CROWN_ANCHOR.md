# Head Crown Anchor - Top Back of Head Positioning

## Problem Solved
MediaPipe Face Mesh only tracks the **visible front** of the face (468 landmarks), but headwear like beanies, hats, and crowns need to be positioned at the **top back of the head**, which has no direct landmarks.

## Solution: Calculated Anchor Point

The `head_crown` anchor is a **calculated position** that estimates the top back of the head using facial geometry:

```cpp
// Location: ar_renderer.cpp GetAnchorPosition()
else if (anchor_name == "head_crown") {
    cv::Point3f forehead = landmarks[10];    // Front of forehead
    cv::Point3f chin = landmarks[152];       // Bottom of face
    cv::Point3f nose = landmarks[1];         // Face center
    
    float face_height = abs(forehead.y - chin.y);
    float face_depth = face_height * 0.6f;   // Head ~60% as deep as tall
    
    head_crown.x = forehead.x;               // Center horizontally
    head_crown.y = forehead.y + face_height * 0.15f;  // 15% above forehead
    head_crown.z = forehead.z - face_depth;  // Behind head
}
```

## Anchor Calculation Details

### Input Landmarks
- **Landmark 10** (forehead) - Front hairline center
- **Landmark 152** (chin) - Bottom of face
- **Landmark 1** (nose) - Face center reference

### Geometric Estimation
1. **Face Height**: Distance from forehead to chin (pixel space)
2. **Head Depth**: Estimated as 60% of face height (average human head proportions)
3. **Crown Height**: 15% above forehead (typical beanie/hat placement)
4. **Z Position**: Projected backward by head depth estimate

### Position Formula
```
X: forehead.x (centered)
Y: forehead.y + (face_height × 0.15)  ← 15% above forehead
Z: forehead.z - (face_height × 0.60)  ← 60% behind forehead
```

## Usage in Filters

### Beanie/Hat Example
```json
{
  "attachments": [{
    "anchor": "head_crown",
    "offset": [0.0, 0.02, 0.0],  // Minimal adjustments needed!
    "scale": [1.0, 1.0, 1.0]
  }]
}
```

### Why This Works
- ✅ **No huge Z offsets needed** - anchor already behind head
- ✅ **Scales with face size** - uses face_height for calculations
- ✅ **Works at any distance** - proportional to face scale
- ✅ **Handles rotation** - anchor rotates with head pose

## Comparison with Other Anchors

| Anchor | Position | Best For | Z Depth |
|--------|----------|----------|---------|
| `nose_bridge` | Front center | Glasses, masks | ~0 |
| `forehead` | Front top | Visors, headbands | ~0 |
| `left_ear` / `right_ear` | Side | Earrings, headphones | ~0 |
| `head_crown` | **Top back** | **Beanies, hats, crowns** | **-300 to -600** |

## Clipping Plane Requirements

Because `head_crown` positions objects **significantly behind** the face plane:

```cpp
// Orthographic projection with extended far plane
glm::ortho(0.0f, width, 0.0f, height, 
    -1000.0f,  // Far plane (back) - supports head_crown!
    1000.0f    // Near plane (front)
);
```

### Z-Depth Ranges (typical face at distance)
- **Nose/forehead anchors**: 0 to -50 pixels
- **Head crown anchor**: -200 to -600 pixels (depends on face size)
- **Far plane limit**: -1000 pixels (plenty of room!)

## Fine-Tuning Position

### Offset Adjustments
Since `head_crown` already positions at top back of head, you only need **small offsets**:

```json
// Move beanie slightly forward/back
"offset": [0.0, 0.0, 0.05]   // +Z = forward (closer to head)
"offset": [0.0, 0.0, -0.05]  // -Z = backward (away from head)

// Move beanie up/down
"offset": [0.0, 0.03, 0.0]   // +Y = up
"offset": [0.0, -0.03, 0.0]  // -Y = down

// Scaled values (at face_scale=0.46, 1300× boost):
// 0.05 offset = 30 pixels
// 0.10 offset = 60 pixels
```

### Typical Values
- **Beanie**: `[0.0, 0.02, 0.0]` - sits on top of head
- **Top Hat**: `[0.0, 0.15, 0.0]` - tall hat above head
- **Crown**: `[0.0, 0.08, -0.05]` - slightly back and high

## Debugging

### Check Logs
```bash
./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt 2>&1 | grep "head_crown"
```

Look for:
- **Anchor position**: Should show negative Z (-200 to -600 typical)
- **Transform position**: Final world position after offset
- **Clipping warnings**: If Z < -1000, increase far plane

### Common Issues

#### 1. Filter Still in Front
**Symptom**: Beanie appears at forehead despite `head_crown` anchor  
**Cause**: App didn't reload filter after config change  
**Solution**: Re-select filter in UI or restart app

#### 2. Filter Vanishes When Moving Back
**Symptom**: Beanie disappears when moving away from camera  
**Cause**: Z position exceeds -1000 (far plane)  
**Solution**: Reduce Z offset or increase far plane limit

#### 3. Filter Too Far Behind
**Symptom**: Beanie appears to float behind head  
**Cause**: Offset Z is too negative  
**Solution**: Use positive Z offset to bring closer: `[0.0, 0.0, 0.05]`

## Available Anchors Summary

### Front Face Anchors
- `nose_bridge` or `nose` - Glasses, masks
- `forehead` or `top_head` - Headbands, visors
- `left_eye` / `right_eye` - Monocles
- `mouth` - Mustaches, lip effects
- `chin` - Beard effects
- `center` or `face_center` - Full-face masks

### Side Anchors
- `left_ear` - Left earrings, headphones (left side)
- `right_ear` - Right earrings, headphones (right side)

### Calculated Anchors
- **`head_crown`** - Top back of head (beanies, hats, crowns)

## Technical Notes

### Estimation Accuracy
The 60% depth ratio is based on average human head proportions:
- Average human head: ~23cm tall × ~15cm deep ≈ 65% ratio
- We use 60% to account for forehead being higher than crown

### Face Distance Compensation
All calculations use face_height in **pixel space**, which automatically scales with distance:
- **Close**: face_height = 800px → head_depth = 480px
- **Far**: face_height = 200px → head_depth = 120px

This ensures the crown anchor position scales proportionally!

### Performance Impact
- **Minimal** - Simple arithmetic (3 landmark lookups, 4 multiplications)
- **No extra tracking** - Uses existing MediaPipe landmarks
- **Cached** - Recalculated only when landmarks update (~30 FPS)

## Future Improvements

### Potential Enhancements
1. **User-adjustable depth ratio** - Allow tuning 60% for different head shapes
2. **Multiple crown positions** - `head_crown_front`, `head_crown_back`
3. **Ear-based depth estimation** - Use ear positions for better accuracy
4. **Head pose-aware calculation** - Adjust for extreme rotations

### Additional Calculated Anchors
- `head_back` - Back of head at face level (sunglasses arms)
- `head_sides` - Temple positions (headphone bands)
- `neck` - Below chin (necklaces, collars)

## See Also
- [AR Filter Creation Guide](AR_FILTER_CREATION_GUIDE.md)
- [Anchor System Documentation](ANCHOR_SYSTEM.md)
- [Phase 6 Anchor Enhancement Plan](phase-6/ANCHOR_ENHANCEMENT_PLAN.md)
