# Crown Position Adjustment Test - Finding #8

## Purpose
Test real-time crown position adjustment via keyboard controls after fixing the PnP validation bug that caused anchors to depend on initial head pose.

## Prerequisites
- SegmeCam built successfully
- AR filter with crown anchor (e.g., beanie filter)
- Camera available

## Test Steps

### 1. Launch Application
```bash
./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt
```

### 2. Activate AR Filter
- Press `2` to activate filter
- Colored debug markers should appear (green/cyan/yellow/magenta cross)
- Crown marker should be visible above forehead

### 3. Verify Position Consistency
- Look straight ahead - note crown position
- Turn head LEFT - crown should stay at same relative position (not move to right side)
- Turn head RIGHT - crown should stay at same relative position (not move to left side)
- Look UP - crown should stay at same relative position (not move down)
- Look DOWN - crown should stay at same relative position (not move up)
- ✅ **PASS if**: Crown stays at top-of-head regardless of rotation/tilt angle

### 4. Test Keyboard Controls

#### Move Crown UP (U key)
```
Press: U
Expected: Console output "⬆️ Crown UP (U key)"
Expected: Console output "Crown offset now: X (X%)"
Expected: Crown marker moves higher
```

#### Move Crown DOWN (D key)
```
Press: D
Expected: Console output "⬇️ Crown DOWN (D key)"
Expected: Console output "Crown offset now: X (X%)"
Expected: Crown marker moves lower
```

#### Reset Crown (Ctrl+R)
```
Press: Ctrl+R
Expected: Console output "🔄 Crown RESET (Ctrl+R)"
Expected: Console output "Crown offset reset to: 0.4 (40%)"
Expected: Crown marker returns to default position
```

### 5. Find Optimal Position
- Adjust crown using U/D keys until beanie appears exactly at top of head
- Note final offset value from console output
- Verify position looks natural at all head angles

## Keyboard Controls Summary

| Key | Action | Delta | Range |
|-----|--------|-------|-------|
| **U** | Move crown UP | +0.05 (+5%) | 0.0 - 1.0 |
| **D** | Move crown DOWN | -0.05 (-5%) | 0.0 - 1.0 |
| **Ctrl+R** | RESET to default | Set to 0.4 | Default 40% |

## Expected Behavior

### Position Consistency (✅ FIXED)
- Crown anchor should **NOT** depend on initial head pose
- Should stay at top-of-head when rotating head left/right
- PnP validation rejects invalid depth values (out of 300-3000mm range)
- Fallback system uses fixed 1.0m depth when PnP fails

### Debug Visualization
- **Multi-color markers**: Green (35px), Cyan (30px), Yellow (30px), White (25px)
- **Cross pattern**: Magenta horizontal + Cyan vertical lines (5px width)
- **Impossible to miss**: Multiple colors ensure visibility regardless of lighting

### Console Output Examples
```
⬆️ Crown UP (U key)
   Crown offset now: 0.45 (45%)

⬇️ Crown DOWN (D key)
   Crown offset now: 0.40 (40%)

🔄 Crown RESET (Ctrl+R)
   Crown offset reset to: 0.4 (40%)
```

## Success Criteria

1. ✅ Crown position consistent at all head angles
2. ✅ Debug markers visible and colored
3. ✅ U/D keys adjust position in real-time
4. ✅ Ctrl+R resets to default
5. ✅ Console shows current offset value
6. ✅ Beanie appears at optimal position

## Technical Details

### PnP Validation System
```cpp
// Validate PnP output on ALL axes after successful solve
double tx = translation_vec.at<double>(0);  // X: horizontal position
double ty = translation_vec.at<double>(1);  // Y: vertical position
double tz = translation_vec.at<double>(2);  // Z: depth

// Validate all axes - face should be within reasonable bounds
bool depth_valid = (tz > 300.0 && tz < 3000.0);        // Depth: 30cm - 3m
bool horizontal_valid = (std::abs(tx) < 2000.0);       // Horizontal: ±2m
bool vertical_valid = (std::abs(ty) < 2000.0);         // Vertical: ±2m

bool pnp_valid = depth_valid && horizontal_valid && vertical_valid;

if (!pnp_valid) {
  ABSL_LOG(WARNING) << "[PnP VALIDATION FAILED] tvec=[" << tx << ", " << ty << ", " << tz << "]";
  pose.confidence = 0.0f;
  head_pose_valid_ = false;
  return pose;
}
```

### Fallback Positioning
```cpp
bool use_pnp = head_pose_valid_ && 
               head_pose.translation.z > 300.0f && 
               head_pose.translation.z < 3000.0f;

if (use_pnp) {
  depth = std::abs(head_pose.translation.z / 1000.0f);
  face_offset = glm::vec3(head_pose.translation.x / 1000.0f,
                          head_pose.translation.y / 1000.0f, 0.0f);
} else {
  depth = 1.0f;  // Fixed 1 meter depth
  face_offset = glm::vec3(0.0f, 0.0f, 0.0f);  // Pure screen-space
}
```

### Crown Offset Calculation
```cpp
cv::Point3f crown = forehead;
crown.y += face_height * crown_offset_multiplier_;  // Default 0.4 = 40%
```

## Known Issues

None! The critical bugs have been fixed:
- ✅ PnP validation prevents catastrophic failures
- ✅ Fallback system handles rotated heads
- ✅ Debug visualization colors work correctly
- ✅ Keyboard controls wired properly

## Next Steps After Testing

1. Record optimal crown offset value
2. Update default in constructor if needed: `crown_offset_multiplier_(0.4f)` → `crown_offset_multiplier_(0.XX)`
3. Clean up diagnostic logging (reduce frequency)
4. Update AR_FILTERS_TROUBLESHOOTING.md with solution
5. Consider making offset configurable via UI panel
6. Document keyboard controls in main README

## Related Files

- `src/ar_filters/opengl_renderer.cpp` - PnP validation, fallback, crown calculation
- `src/ar_filters/ar_filter_manager.cpp` - Crown offset adjustment methods
- `src/ui/ui_manager_enhanced.cpp` - Keyboard event handling
- `include/ar_filters/opengl_renderer.h` - Crown offset member variable
- `include/ar_filters/ar_filter_manager.h` - Public API declarations
