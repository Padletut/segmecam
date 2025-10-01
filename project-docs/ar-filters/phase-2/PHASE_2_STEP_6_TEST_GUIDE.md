# 🎯 Phase 2 Step 6: Testing & Validation Guide

**Ready for User Testing!**

---

## 🚀 Quick Start

```bash
# Run the test script
./test-filter-presets.sh
```

This will:
1. Display testing instructions
2. Launch SegmeCam with face mesh support
3. Guide you through testing each preset

---

## 📋 Test Checklist

### Visual Tests

#### 1. Classic Glasses 👓

- [ ] Launch SegmeCam
- [ ] Open Debug Panel (bottom right)
- [ ] Scroll to "AR Filter Presets"
- [ ] Select "Classic Glasses" from dropdown
- [ ] **Verify**: 2 transparent cylinders appear over eyes
- [ ] **Verify**: Small bridge piece connects the lenses
- [ ] **Verify**: Glasses track smoothly with head movement
- [ ] **Verify**: Glasses visible when facing camera
- [ ] Record update time (should be < 0.2μs)

#### 2. Party Hat 🎉

- [ ] Select "Party Hat" from dropdown
- [ ] **Verify**: Pink/magenta cone appears above forehead
- [ ] **Verify**: Yellow sphere (pom-pom) on top of cone
- [ ] **Verify**: Hat stays positioned above head
- [ ] **Verify**: Hat rotates with head movement
- [ ] **Verify**: Hat visible when looking up slightly
- [ ] Record update time (should be < 0.15μs)

#### 3. Face Mask 😷

- [ ] Select "Face Mask" from dropdown
- [ ] **Verify**: Light blue shapes cover nose/mouth area
- [ ] **Verify**: 3 main sections (top, middle, bottom)
- [ ] **Verify**: 2 side pieces on cheeks
- [ ] **Verify**: Mask tracks with face movement
- [ ] **Verify**: Coverage looks appropriate
- [ ] Record update time (should be < 0.3μs)

#### 4. Preset Switching

- [ ] Switch between all 3 presets without closing app
- [ ] **Verify**: Old filters disappear when switching
- [ ] **Verify**: New filters appear immediately
- [ ] **Verify**: No crashes or errors
- [ ] **Verify**: Statistics update correctly

#### 5. Clear Function

- [ ] With any preset active, click "Clear Preset"
- [ ] **Verify**: All filters disappear
- [ ] **Verify**: Dropdown resets to "None"
- [ ] **Verify**: Statistics show 0 filters

### Performance Tests

#### Baseline Comparison

| Preset | Filters | Target | Your Result | Pass? |
|--------|---------|--------|-------------|-------|
| None | 0 | 0μs | _____μs | ☐ |
| Classic Glasses | 3 | < 0.2μs | _____μs | ☐ |
| Party Hat | 2 | < 0.15μs | _____μs | ☐ |
| Face Mask | 5 | < 0.3μs | _____μs | ☐ |
| Test Demo | 7 | 0.31μs | 0.31μs | ☑ |

**How to measure**:
1. Select preset
2. Look at Debug Panel statistics
3. Note "Update: X.XXXXX ms" value
4. Convert to microseconds (multiply by 1000)
5. Record in table above

#### Frame Rate Test

- [ ] With no preset: Record FPS: _____
- [ ] With Classic Glasses: Record FPS: _____
- [ ] With Party Hat: Record FPS: _____
- [ ] With Face Mask: Record FPS: _____
- [ ] **Verify**: FPS stays above 25 for all presets

### Stress Tests

#### Multiple Face Angles

For each preset:

- [ ] Face camera directly
- [ ] Turn head left 45°
- [ ] Turn head right 45°
- [ ] Tilt head up 30°
- [ ] Tilt head down 30°
- [ ] **Verify**: Filters track smoothly at all angles

#### Visibility Detection

- [ ] Turn head far left (profile)
  - **Verify**: Some filters disappear (not visible)
  - **Verify**: Statistics show reduced visible count
- [ ] Turn back to center
  - **Verify**: All filters reappear
  - **Verify**: Statistics show full count

#### Rapid Switching

- [ ] Rapidly switch between presets 5 times
- [ ] **Verify**: No crashes
- [ ] **Verify**: No memory leaks
- [ ] **Verify**: Performance stays consistent

---

## 📊 Results Template

Copy this template and fill in your results:

```
=== PHASE 2 STEP 6 TEST RESULTS ===
Date: _______________
Tester: _______________

VISUAL TESTS:
✓/✗ Classic Glasses appearance: ___
✓/✗ Classic Glasses tracking: ___
✓/✗ Party Hat appearance: ___
✓/✗ Party Hat tracking: ___
✓/✗ Face Mask appearance: ___
✓/✗ Face Mask tracking: ___

PERFORMANCE:
- Classic Glasses: _____μs (target < 0.2μs)
- Party Hat: _____μs (target < 0.15μs)
- Face Mask: _____μs (target < 0.3μs)

FRAME RATES:
- Baseline (no filters): _____fps
- With Classic Glasses: _____fps
- With Party Hat: _____fps
- With Face Mask: _____fps

ISSUES FOUND:
[List any problems here]

OVERALL ASSESSMENT:
☐ All tests pass - Ready for Phase 2 completion
☐ Minor issues - Needs tweaking
☐ Major issues - Needs rework
```

---

## 🔧 Troubleshooting

### Filters Don't Appear

**Symptoms**: Dropdown changes but no filters visible

**Solutions**:
1. Check Debug Panel shows "Filters: X/Y visible"
2. Ensure face mesh is detected (blue landmarks visible)
3. Try moving closer to camera
4. Check lighting conditions

### Poor Performance

**Symptoms**: FPS drops below 25, laggy tracking

**Solutions**:
1. Close other applications
2. Reduce camera resolution
3. Check CPU/GPU usage
4. Verify GPU acceleration is enabled

### Filters Don't Track

**Symptoms**: Filters appear but don't move with face

**Solutions**:
1. Check head pose estimation is working
2. Verify anchor points are updating
3. Look for transform calculation errors in console
4. Restart SegmeCam

### Filters Disappear Randomly

**Symptoms**: Filters flicker or vanish unexpectedly

**Solutions**:
1. Improve lighting (face must be well-lit)
2. Keep face centered in frame
3. Avoid extreme head angles
4. Check for face detection failures

---

## 💾 Saving Test Results

After completing tests, save your results:

```bash
# Create results file
nano PHASE_2_STEP_6_TEST_RESULTS.txt

# Paste your completed results template
# Save and exit (Ctrl+X, Y, Enter)
```

---

## 🎉 Success Criteria

Phase 2 Step 6 is complete when:

- ✅ All 3 presets render correctly
- ✅ All update times < 1ms (ideally < 0.3μs)
- ✅ FPS stays above 25 for all presets
- ✅ Smooth tracking with head movement
- ✅ Preset switching works reliably
- ✅ No crashes or memory leaks
- ✅ Clear preset function works

---

## 📝 Reporting Results

When testing is complete:

1. Fill out the results template above
2. Share results in session comments
3. Mention any issues or suggestions
4. Confirm readiness for Phase 2 completion documentation

---

**Ready to test? Run `./test-filter-presets.sh` now!** 🚀
