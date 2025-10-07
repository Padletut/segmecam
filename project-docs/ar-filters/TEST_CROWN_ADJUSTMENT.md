# Crown Anchor Adjustment Test (Finding #8)

## Purpose
Test keyboard controls for adjusting the `head_crown` anchor position to fine-tune crown/hat placement on head.

## Keyboard Controls

### Vertical Adjustment (Up/Down)
- **U** = Move crown UP (increase Y-offset, away from forehead)
- **D** = Move crown DOWN (decrease Y-offset, toward forehead)
- Adjusts in 5% increments
- Valid range: 0% to 100% above forehead

### Depth Adjustment (Forward/Backward) - NEW!
- **W** = Move crown FORWARD (toward camera, negative Z-offset)
- **S** = Move crown BACKWARD (away from camera, positive Z-offset)
- Adjusts in 5% increments
- Valid range: -100% (forward) to +100% (backward)

### Reset
- **Ctrl+R** = Reset to default (Y: 40% above forehead, Z: 0% depth)

## Test Procedure

1. **Launch with debug visualization:**
   ```bash
   ./segmecam
   ```

2. **Load pro_beanie filter** (or any filter with head_crown anchor)

3. **Test vertical adjustment:**
   - Press **U** several times → Crown should move higher above forehead
   - Press **D** several times → Crown should move lower toward forehead
   - Verify: Crown moves in intuitive direction

4. **Test depth adjustment:**
   - Press **W** several times → Crown should move forward (closer to camera)
   - Press **S** several times → Crown should move backward (farther from camera)
   - Verify: Crown depth changes while maintaining position on head

5. **Test reset:**
   - Press **Ctrl+R** → Crown should snap back to default position (40% up, 0% depth)

6. **Test movement tracking:**
   - After adjusting position, move your head around
   - Verify: Crown stays properly positioned and follows head movement

## Expected Results

✅ **Crown above forehead** (not below)
✅ **U key moves crown UP** (higher above forehead)
✅ **D key moves crown DOWN** (closer to forehead)
✅ **W key moves crown FORWARD** (toward camera)
✅ **S key moves crown BACKWARD** (away from camera)
✅ **Ctrl+R resets** to default (40% up, 0% depth)
✅ **Crown follows head** movement smoothly in all directions
✅ **Crown rotates** with head tilt (if PnP rotation works smoothly)

## Debug Visualization

Look for colored markers:
- 🔴 **Red circle** = forehead anchor (baseline, 0%)
- 🔵 **Cyan cross** = head_crown anchor (adjusted position)

The cyan cross shows where the crown anchor is positioned relative to your face.

## Coordinate System Notes

**MediaPipe Landmarks:**
- X: left (-) to right (+)
- Y: top (0) to bottom (1) - **INVERTED from typical coordinates!**
- Z: camera (0) to away (+)

**Crown Calculation:**
```cpp
cv::Point3f crown = forehead;
crown.y -= face_height * crown_offset_multiplier_;  // SUBTRACT to move UP
crown.z += face_height * crown_depth_offset_;        // ADD to move BACKWARD
```

## Troubleshooting

**Issue**: Crown below forehead on startup
- **Cause**: Y-axis inverted in code
- **Fix**: Use subtraction for Y-offset: `crown.y -= offset`

**Issue**: U/D keys work opposite
- **Cause**: Sign of delta in keyboard handler wrong
- **Fix**: U should add positive, D should add negative to offset

**Issue**: Crown jitters during movement
- **Cause**: Transform smoothing needed
- **Fix**: Apply exponential smoothing to depth/position

**Issue**: Crown doesn't rotate with head tilt
- **Cause**: PnP rotation disabled or unreliable
- **Fix**: Try landmark-based rotation calculation

**Issue**: W/S keys don't move crown
- **Cause**: Depth offset not applied or Z-axis wrong direction
- **Fix**: Verify `crown.z += face_height * depth_offset` in GetAnchorPosition

## Next Steps

After successful adjustment:
1. Save optimal values to filter.json (if needed)
2. Test with other filters (glasses, masks, etc.)
3. Implement auto-calibration based on face shape
4. Add UI sliders for real-time adjustment
