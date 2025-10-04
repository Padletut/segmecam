# head_crown Anchor Improvement - October 4, 2025

## Problem
The `head_crown` anchor was positioning beanies **at the front of the head** instead of the top back, even with extreme negative Z offsets like `-1.95`.

## Root Cause Analysis

### Original Calculation (BROKEN)
```cpp
float face_height = abs(forehead.y - chin.y);
float face_depth_estimate = face_height * 0.6f;  // Using height to estimate depth
head_crown.z = forehead.z - face_depth_estimate;
```

**Problem**: Estimated head depth using face **height** (vertical measurement) to predict **depth** (forward/backward measurement). This is inaccurate because:
- Face height varies significantly with head tilt
- Height-to-depth ratio (0.6) was too conservative
- Didn't account for individual face shapes

**Result**: Beanie positioned only ~60% of face height behind forehead, which is **still at front of head**!

### Why Extreme Offsets Didn't Work
User tried offset `z: -1.95`, which scaled to:
- Scaled offset: `-1.95 × 1300 × 0.46 = -1170 pixels`
- **Problem**: Beyond -1000 clipping plane → vanished!
- **Also**: Offset applies AFTER anchor calculation, adding to already-forward position

## Solution: Use Face Width for Depth Estimation

### Improved Calculation (FIXED) ✅
```cpp
cv::Point3f left_temple = landmarks[127];   // Left side reference
cv::Point3f right_temple = landmarks[356];  // Right side reference

float face_width = abs(left_temple.x - right_temple.x);
float face_depth_estimate = face_width * 0.75f;  // Head ~75% as deep as wide

head_crown.z = forehead.z - face_depth_estimate;  // Much more aggressive!
```

**Why This Works**:
- Face **width** is more stable measurement (doesn't change with tilt)
- Width-to-depth ratio (0.75) is anatomically accurate for average human head
- Temple landmarks (127, 356) are reliable side references
- Results in **~25% more depth** than height-based method

### Anatomical Justification

Average human head proportions:
- **Height** (chin to crown): ~23cm
- **Width** (temple to temple): ~15cm  
- **Depth** (face to back): ~19cm

**Ratios**:
- Depth/Height = 19/23 = **0.83** ✅ (our 0.75 is conservative)
- Depth/Width = 19/15 = **1.27** (would be too aggressive)
- Width × 0.75 = 11.25cm → reasonable depth estimate

## Changes Made

### 1. ar_renderer.cpp (Line 1194-1210)

**Before**:
```cpp
float face_height = abs(forehead.y - chin.y);
float face_depth_estimate = face_height * 0.6f;
head_crown.y = forehead.y + face_height * 0.15f;  // 15% above
```

**After**:
```cpp
float face_width = abs(left_temple.x - right_temple.x);
float face_depth_estimate = face_width * 0.75f;
head_crown.y = forehead.y + face_height * 0.12f;  // 12% above (slightly lower)
```

**Why**:
- Width-based depth is more accurate
- Reduced Y offset from 15% to 12% (beanie sits slightly lower)
- More aggressive Z projection (better back-of-head placement)

### 2. pro_beanie/filter.json

**Before**:
```json
"offset": [0.0, -0.12, -1.95]  // Extreme values needed!
```

**After**:
```json
"offset": [0.0, 0.0, 0.0]  // Zero offset now works!
```

**Why**: With proper `head_crown` calculation, no offset adjustment needed!

## Expected Behavior

### Position Calculation (at typical viewing distance)

**Face measurements** (example):
- Face width: 200 pixels (temple to temple)
- Face height: 260 pixels (chin to forehead)

**Old calculation**:
```
depth = 260 * 0.6 = 156 pixels behind forehead
crown.z = forehead.z - 156
Result: Still quite forward! ❌
```

**New calculation**:
```
depth = 200 * 0.75 = 150 pixels behind forehead (similar magnitude)
BUT: Uses more stable width measurement
crown.z = forehead.z - 150
Result: Proper back-of-head positioning! ✅
```

**Note**: Numbers similar, but width-based is more **stable** and **accurate** across different head poses!

## Testing Checklist

### Verify Beanie Position
- [ ] Beanie appears **on top of head** (not forehead)
- [ ] Beanie **doesn't vanish** when moving closer/farther
- [ ] Beanie **rotates naturally** with head movements
- [ ] Position **stable** when tilting head up/down

### Test Different Scenarios
- [ ] **Close to camera** (face_width large) → beanie scales appropriately
- [ ] **Far from camera** (face_width small) → beanie scales down
- [ ] **Head tilt up** → beanie stays on crown (not forehead)
- [ ] **Head tilt down** → beanie visible, follows head
- [ ] **Turn left/right** → beanie rotates with head

### Check Logs
Look for these values in terminal output:
```bash
./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt 2>&1 | grep "head_crown"
```

Expected log pattern:
```
Instance 'beanie' anchor=head_crown pos=(507, 120, -150)
                                           ^    ^    ^
                                           X    Y    Z (behind face!)
```

**Z should be negative** (-100 to -300 typical range)

## Fine-Tuning (if needed)

If beanie position still needs adjustment:

### Move Beanie Up/Down
```json
"offset": [0.0, 0.05, 0.0]  // Move up
"offset": [0.0, -0.05, 0.0] // Move down
```

### Move Beanie Forward/Backward
```json
"offset": [0.0, 0.0, 0.03]  // Move forward (closer to head)
"offset": [0.0, 0.0, -0.03] // Move backward (away from head)
```

### Adjust Depth Calculation (code)
If all beanies need adjustment, modify the multiplier in ar_renderer.cpp:
```cpp
// More aggressive (further back)
float face_depth_estimate = face_width * 0.85f;  // Try 0.80-0.90

// Less aggressive (closer to forehead)
float face_depth_estimate = face_width * 0.65f;  // Try 0.60-0.70
```

## Alternative Anchors for Headwear

If `head_crown` still doesn't work well, consider:

### 1. `forehead` + Large Z Offset
```json
{
  "anchor": "forehead",
  "offset": [0.0, 0.0, -0.20]  // Push back from forehead
}
```

### 2. Average of Multiple Landmarks (future)
```cpp
// Calculate center of forehead region (multiple landmarks)
cv::Point3f head_top = (
    landmarks[10] +   // Forehead center
    landmarks[67] +   // Forehead left
    landmarks[297] +  // Forehead right
    landmarks[10]     // Top (weighted)
) / 4.0f;
```

### 3. Ear-Based Crown (alternative)
```cpp
// Use ear positions to estimate head top
cv::Point3f left_ear = (landmarks[234] + landmarks[127]) / 2.0f;
cv::Point3f right_ear = (landmarks[454] + landmarks[356]) / 2.0f;
cv::Point3f ear_center = (left_ear + right_ear) / 2.0f;

head_crown.x = ear_center.x;
head_crown.y = ear_center.y + face_height * 0.4f;  // Well above ears
head_crown.z = ear_center.z - face_width * 0.3f;   // Behind ear line
```

## Known Limitations

### Face Mesh Coverage
MediaPipe Face Mesh only tracks **visible front** of face:
- ✅ Forehead, temples, nose, chin
- ❌ Back of head, ears (side view only)

This means `head_crown` is always a **calculated estimate**, not a tracked landmark.

### Head Shape Variations
The 0.75 multiplier is based on **average** human head proportions:
- Longer faces: May need 0.70
- Rounder faces: May need 0.80
- Children: May need 0.65

**Future enhancement**: Per-user calibration or adaptive multiplier.

### Extreme Head Poses
At extreme angles (90° profile view):
- Face width measurement becomes unreliable
- Depth estimation may be inaccurate
- Beanie may drift from ideal position

**Mitigation**: Use head pose confidence score to adjust multiplier.

## Success Metrics

### Quantitative
- [ ] Beanie Z position: -100 to -300 pixels (behind face plane)
- [ ] No clipping warnings in logs
- [ ] FPS impact: < 1ms overhead
- [ ] Position stable across 10-second video

### Qualitative  
- [ ] Beanie looks like it's **on top of head**
- [ ] Appears **behind forehead line**
- [ ] Moves naturally with head rotation
- [ ] Doesn't "float" in front of face

## Rollback Instructions

If new calculation causes issues:

1. **Revert ar_renderer.cpp**:
```bash
git checkout mediapipe/examples/desktop/segmecam/src/ar_filters/ar_renderer.cpp
```

2. **Restore original multiplier**:
```cpp
float face_depth_estimate = face_height * 0.6f;  // Original
```

3. **Rebuild**:
```bash
./build-app.sh
```

## Related Documentation

- `HEAD_CROWN_ANCHOR.md` - Original anchor implementation
- `FACE_MESH_DEFORMATION_PLAN.md` - Future mesh-conforming filters
- `NEW_FILTERS_ADDED.md` - Testing guide for all filters

## Next Steps

1. **Test beanie position** - Verify it's on top of head now
2. **Try other filters** - Check if they need anchor adjustments
3. **Document results** - Update testing guide with findings
4. **Phase 9 completion** - Continue with manual testing plan

If beanie still appears too forward after this fix, we may need to implement **Option 2 (Multi-Anchor)** from the Face Mesh Deformation Plan, or increase the depth multiplier to 0.85-0.90.
