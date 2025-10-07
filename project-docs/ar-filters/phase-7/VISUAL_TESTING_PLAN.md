# Phase 7 Day 3: Visual Testing Plan

## Created: 2025-10-03
## Status: 🔄 In Progress

## Overview
This document outlines the visual testing strategy for the AR Filter behavior system completed in Phase 7 Day 3.

## Test Environment

### Hardware Requirements
- Webcam (V4L2 compatible)
- OpenGL 3.3+ capable GPU (or software rendering)
- Minimum 4GB RAM
- x86_64 Linux system

### Software Requirements
- MediaPipe models installed
- v4l2loopback module loaded (optional - for virtual camera output)
- SDL2, OpenGL libraries
- SegmeCam built with AR filter support

## Test Asset: classic-glasses-v1

### Asset Details
- **Location**: `assets/filters/classic-glasses-v1/`
- **Geometry**: Simple rectangular glasses (20 vertices, 5 faces)
- **Behaviors**: 2x SHAKE (left/right eye blink)
- **Anchor**: nose_bridge
- **Materials**: Black matte with specular highlights

### Asset Validation
✅ filter.json created with valid schema
✅ glasses.obj created with proper OBJ format
✅ Placeholder images created (texture, thumbnail, icon)
⏳ Need actual PNG textures for full visual quality

## Visual Test Scenarios

### 1. Filter Loading Test
**Objective**: Verify filter discovery and loading

**Steps**:
1. Start application
2. Open AR Filter panel (if UI integrated)
3. Check filter list for "Classic Glasses"
4. Select filter to load

**Expected Results**:
- Filter appears in available filters list
- Metadata displays correctly (name, description, thumbnail)
- No errors during filter.json parsing
- ARFilterManager logs successful load

**Pass Criteria**:
- ✅ Filter discovered via `DiscoverFilters()`
- ✅ `LoadFilter("classic-glasses-v1")` returns `absl::OkStatus()`
- ✅ Filter appears in `GetLoadedFilters()` list

---

### 2. Face Detection & Anchor Placement
**Objective**: Verify glasses appear on detected face

**Steps**:
1. Position face in camera view
2. Wait for MediaPipe face detection
3. Observe glasses placement on nose bridge

**Expected Results**:
- Glasses appear within 500ms of face detection
- Glasses positioned at nose bridge (between eyes)
- Glasses scale appropriately to face size
- Glasses track face movement smoothly

**Pass Criteria**:
- ✅ Glasses visible when face detected
- ✅ Position centered on nose bridge
- ✅ No jitter or lag in tracking
- ✅ Maintains 30+ FPS with filter active

**Metrics to Check**:
- `GetPerformanceStats()`: check render_time_ms < 16ms
- Frame rate stability (no drops below 25 FPS)

---

### 3. Left Eye Blink Behavior
**Objective**: Verify SHAKE behavior triggers on left eye blink

**Steps**:
1. With glasses active, blink left eye only
2. Observe glasses movement
3. Repeat 5 times

**Expected Results**:
- Glasses shake/vibrate when left eye closes >70%
- Shake stops when eye reopens
- Shake direction is random but small (±2 pixels)
- No permanent offset accumulation

**Pass Criteria**:
- ✅ Visible shake on left eye blink
- ✅ Blendshape value `eyeBlinkLeft` exceeds 0.7 threshold
- ✅ `SetModelInstanceOffset()` called with random offset
- ✅ Offset magnitude <= 2.0 * intensity (4.0 units max)
- ✅ Behavior deactivates when blink ends

**Debug Checks**:
- Monitor `blendshape_values_["eyeBlinkLeft"]` in logs
- Check `behavior_states_["glasses_frame"].active` flag
- Verify `shake_offset` vector in state

---

### 4. Right Eye Blink Behavior
**Objective**: Verify SHAKE behavior triggers on right eye blink

**Steps**:
1. Blink right eye only
2. Observe glasses movement
3. Repeat 5 times

**Expected Results**:
- Same as Test 3, but for right eye
- Independent from left eye behavior

**Pass Criteria**:
- ✅ Visible shake on right eye blink
- ✅ Blendshape value `eyeBlinkRight` exceeds 0.7 threshold
- ✅ Behavior activates independently from left eye
- ✅ Shake characteristics match left eye behavior

---

### 5. Simultaneous Blink Behavior
**Objective**: Test both eyes blinking together

**Steps**:
1. Blink both eyes simultaneously
2. Observe glasses response
3. Repeat 5 times

**Expected Results**:
- Both SHAKE behaviors activate
- Combined effect may be stronger (additive offsets)
- Still returns to rest position when eyes open

**Pass Criteria**:
- ✅ Both behaviors trigger
- ✅ Shake magnitude potentially larger (both offsets applied)
- ✅ Glasses return to original position after blink
- ✅ No visual artifacts or glitches

---

### 6. Rapid Blink Test (Stress Test)
**Objective**: Verify stability under rapid behavior changes

**Steps**:
1. Blink eyes rapidly 10 times
2. Alternate left/right blinking
3. Check for performance degradation

**Expected Results**:
- Glasses respond to each blink
- No memory leaks or state corruption
- Frame rate remains stable
- No accumulated offsets

**Pass Criteria**:
- ✅ Maintains 30+ FPS throughout
- ✅ Memory usage stable (check with `top`)
- ✅ No visual artifacts
- ✅ Glasses return to rest position between blinks

**Performance Targets**:
- Behavior processing: <0.1ms per frame
- Total frame time: <16ms (60 FPS)
- Memory: <500MB total application memory

---

### 7. Edge Cases

#### 7a. Partial Blink (Below Threshold)
**Test**: Close eyes 50% (below 0.7 threshold)
**Expected**: No shake behavior triggers
**Pass**: ✅ Behavior remains inactive, glasses static

#### 7b. Face Lost/Regained
**Test**: Move face out of frame, then back
**Expected**: Glasses disappear, reappear on detection
**Pass**: ✅ Clean state reset, no dangling behaviors

#### 7c. Multiple Faces
**Test**: Two people in frame
**Expected**: Glasses on first detected face only
**Pass**: ✅ Single filter instance per face (current design)

#### 7d. Low Light Conditions
**Test**: Reduce lighting
**Expected**: May affect face detection accuracy
**Pass**: ✅ Degrades gracefully, no crashes

---

### 8. Integration Test
**Objective**: Verify ARFilterManager → ARRenderer pipeline

**Steps**:
1. Enable verbose logging
2. Blink eye
3. Review log sequence

**Expected Log Sequence**:
```
[ARFilterManager] UpdateBehaviors called
[ARFilterManager] GetBlendshapeValue(eyeBlinkLeft) = 0.85
[ARFilterManager] Behavior SHAKE activated (threshold exceeded)
[ARFilterManager] ApplyShakeBehavior: offset=(0.12, -0.08, 0.03)
[ARRenderer] SetModelInstanceOffset(glasses_frame, offset)
[ARRenderer] Transform matrix updated
```

**Pass Criteria**:
- ✅ Complete call chain executes
- ✅ Blendshape value calculated correctly
- ✅ ARRenderer API called with expected parameters
- ✅ Visual result matches logged offset

---

## Performance Benchmarks

### Target Metrics
| Metric | Target | Acceptable | Unacceptable |
|--------|--------|------------|--------------|
| FPS (with filter) | 60 | 30 | <25 |
| Frame time | <16ms | <33ms | >40ms |
| Behavior overhead | <0.1ms | <0.5ms | >1ms |
| Memory usage | <400MB | <600MB | >800MB |
| Filter load time | <200ms | <500ms | >1000ms |

### Measurement Tools
- `GetPerformanceStats()`: Built-in profiling
- `perf`: Linux profiler (optional)
- `valgrind --tool=massif`: Memory profiling
- Frame counter: SDL2 timer + frame counting

---

## Known Issues & Limitations

### Current Limitations
1. **No Texture Rendering**: Placeholder PNG files need actual graphics
2. **Simple Geometry**: Basic rectangular frames (not production-quality)
3. **No Color Tinting**: ColorChange behavior not visually testable yet
4. **Single Face Only**: Multi-face support not implemented

### Expected Artifacts
1. **Minor Jitter**: Small tracking inaccuracies from MediaPipe are normal
2. **Blink Detection Lag**: ~16-33ms delay is acceptable (1-2 frames)
3. **Initial Load Time**: First filter may take 300-500ms (asset loading)

### Issues to Ignore (Out of Scope)
- Texture quality (placeholders only)
- Advanced lighting/shading (deferred to Phase 8)
- UI polish (Phase 8 concern)
- Multiple filter layering (future feature)

---

## Success Criteria Summary

### Must Pass (Critical)
✅ Filter loads without errors
✅ Glasses appear on face detection
✅ SHAKE behavior triggers on blink
✅ Glasses track face movement
✅ Maintains 30+ FPS
✅ No crashes or memory leaks

### Should Pass (Important)
✅ Smooth behavior transitions
✅ Accurate blendshape calculations
✅ Clean state management (no accumulated offsets)
✅ Proper threshold enforcement

### Nice to Have (Optional)
⚪ 60 FPS sustained
⚪ <0.1ms behavior overhead
⚪ Perfect tracking (no jitter)
⚪ Production-quality visuals

---

## Test Execution Plan

### Phase 1: Static Tests (No Camera)
1. ✅ Asset validation (files exist, schemas valid)
2. ⏳ Filter discovery test (DiscoverFilters)
3. ⏳ Filter loading test (LoadFilter)
4. ⏳ Asset parsing validation

### Phase 2: Camera Tests (Live Face Detection)
5. ⏳ Face detection + anchor placement
6. ⏳ Basic tracking (no behaviors)
7. ⏳ Behavior triggering (blink tests 3-6)
8. ⏳ Edge cases (test 7a-7d)

### Phase 3: Performance Tests
9. ⏳ FPS benchmarking
10. ⏳ Memory profiling
11. ⏳ Rapid behavior changes (stress test)
12. ⏳ Long-running stability (5+ minutes)

### Phase 4: Integration Validation
13. ⏳ Log analysis (call chain verification)
14. ⏳ State inspection (behavior_states_ map)
15. ⏳ ARRenderer API verification

---

## Next Steps

### Immediate (Today)
1. Build application with ARFilterManager integrated
2. Run Phase 1 tests (asset validation)
3. Attempt camera test if build succeeds
4. Document any issues encountered

### Short-term (This Week)
1. Create actual PNG textures (glasses_texture.png, etc.)
2. Implement unit tests (ar_filter_manager_test.cpp)
3. Complete all visual tests
4. Create DAY_3_COMPLETE.md documentation

### Medium-term (Phase 7 Completion)
1. Add more sample filters (mustache, hat, etc.)
2. Test all 6 behavior types (currently only SHAKE)
3. Performance optimization if needed
4. Phase 8 planning (UI integration)

---

## Test Results Template

```markdown
## Test Results: classic-glasses-v1

**Date**: YYYY-MM-DD
**Tester**: [Name]
**Build**: [Commit Hash]
**System**: [CPU, GPU, RAM]

### Test 1: Filter Loading
- Status: PASS/FAIL
- Notes: [Observations]

### Test 2: Face Detection
- Status: PASS/FAIL
- FPS: [Average FPS]
- Notes: [Observations]

### Test 3: Left Eye Blink
- Status: PASS/FAIL
- Blendshape Max: [Value]
- Notes: [Observations]

... (repeat for all tests)

### Performance Summary
- Average FPS: [Value]
- Frame Time: [ms]
- Memory Usage: [MB]
- Behavior Overhead: [ms]

### Issues Found
1. [Issue description]
2. [Issue description]

### Conclusion
[Overall assessment]
```

---

## Appendix: Manual Test Commands

```bash
# Build with AR filter support
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH \
  --cxxopt=-I/usr/include/opencv4 \
  //mediapipe/examples/desktop/segmecam:segmecam

# Run with verbose logging
GLOG_v=1 ./bazel-bin/mediapipe/examples/desktop/segmecam/segmecam

# Check filter discovery
ls -lah assets/filters/classic-glasses-v1/

# Validate JSON syntax
python3 -m json.tool assets/filters/classic-glasses-v1/filter.json

# Check OBJ format
cat assets/filters/classic-glasses-v1/glasses.obj

# Monitor performance
perf stat -e cycles,instructions,cache-misses \
  ./bazel-bin/mediapipe/examples/desktop/segmecam/segmecam

# Memory profiling
valgrind --tool=massif --massif-out-file=massif.out \
  ./bazel-bin/mediapipe/examples/desktop/segmecam/segmecam
```
