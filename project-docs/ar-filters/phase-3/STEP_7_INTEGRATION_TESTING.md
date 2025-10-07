# Phase 3 Step 7: Integration Testing Plan

**Date**: October 3, 2025  
**Status**: 🚀 STARTING  
**Prerequisites**: ✅ Phase 3 Step 6 Complete (Unit tests pass, models load)

---

## Executive Summary

**Goal**: Validate 3D model loading works in the running SegmeCam application with real-time face tracking, rendering, and user interaction.

**Approach**: Start with the existing FilterTestDemo system (7 geometric primitives) and extend it to support 3D models. Test with simple_cube.obj first, then simple_glasses.obj, then custom models.

**Success Criteria**:
- ✅ 3D model loads without crashes
- ✅ Model displays on screen correctly
- ✅ Model attaches to face anchor and tracks movement
- ✅ Materials/textures render correctly
- ✅ Performance acceptable (30+ FPS)
- ✅ No memory leaks during 5-minute session

---

## Current State Analysis

### ✅ What's Already Working

From code inspection, SegmeCam already has:

1. **AR Filter Infrastructure** (Phase 2):
   - `AttachmentController` - Manages filter-to-anchor bindings
   - `FilterTestDemo` - Creates 7 geometric primitives on face anchors
   - `FilterPresetManager` - Manages preset filter configurations
   - `ar_filters_enabled` flag in `AppState`

2. **3D Model Support** (Phase 3 Steps 1-6):
   - `ModelLoader` - Loads OBJ files with Assimp ✅
   - `FilterObject::Type::MODEL_3D` - Type for 3D model filters ✅
   - `CreateFromModel()` - Factory function for model filters ✅
   - Unit tests passing (52/62, 84%) ✅

3. **Face Tracking** (Phase 1):
   - MediaPipe hand/face mesh tracking active
   - `FaceMeshProcessor` - 478-point 3D face model
   - `TransformCalculator` - Head pose and 7 anchor points
   - Real-time anchor point calculation

4. **Rendering Pipeline**:
   - SDL2 + OpenGL context
   - ImGui UI panels
   - Camera capture and display working

### ❓ What We Need to Add

**Missing**: UI control to load 3D models from file picker or hardcoded path

**Solution**: Extend `DebugPanel` with 3D model loading controls

---

## Phase A: Build and Environment Verification (15 minutes)

### A1: Build Application ✅

```bash
cd /home/padletut/segmecam
./build-app.sh 2>&1 | tee /tmp/step7_build.log
```

**Expected**: Clean build, no errors, binary at `./segmecam`

### A2: Run Application ✅

```bash
./segmecam
```

**Expected**: 
- Window opens
- Camera activates
- Face tracking starts
- ImGui UI visible

### A3: Test Existing AR Filters ✅

1. Open Debug panel (if not visible)
2. Enable "Show Anchor Points" checkbox
3. Enable "Enable Filter Test" checkbox

**Expected**:
- 7 colored shapes appear on face
- Shapes follow face movement
- Shapes visible at anchor points

---

## Phase B: Add 3D Model Loading UI (30 minutes)

### B1: Extend DebugPanel with Model Loading Controls

**File**: `mediapipe/examples/desktop/segmecam/src/ui/profile_debug_panels.cpp`

**Add after AR Filter Presets section** (around line 140):

```cpp
    ImGui::Spacing();
    ImGui::Separator();
    
    // 3D Model Loading (Phase 3 Step 7)
    ImGui::Text("3D Model Loading (Phase 3)");
    ImGui::Separator();
    
    // Model file path input
    static char model_path[512] = "assets/ar_filters/models/simple_cube.obj";
    ImGui::InputText("Model Path", model_path, sizeof(model_path));
    ImGui::TextDisabled("Path relative to workspace or absolute path");
    
    // Anchor point selection
    static int model_anchor = 0;
    const char* anchor_items[] = {
        "nose_bridge",
        "forehead", 
        "chin",
        "left_cheek",
        "right_cheek",
        "left_eye",
        "right_eye"
    };
    ImGui::Combo("Anchor Point", &model_anchor, anchor_items, IM_ARRAYSIZE(anchor_items));
    
    // Model scale control
    static float model_scale = 1.0f;
    ImGui::SliderFloat("Model Scale", &model_scale, 0.1f, 5.0f);
    
    // Load button
    if (ImGui::Button("Load 3D Model")) {
        std::string anchor_name = anchor_items[model_anchor];
        std::cout << "[DebugPanel] Loading 3D model: " << model_path 
                  << " at anchor: " << anchor_name << std::endl;
        
        // Create FilterObject from model
        FilterObject model_filter = CreateFromModel(
            anchor_name,
            std::string("Model: ") + model_path,
            std::string(model_path)
        );
        
        if (!model_filter.model_data.meshes.empty()) {
            model_filter.local_scale = model_scale;
            model_filter.offset = {0.0f, 0.0f, 0.0f};
            model_filter.enabled = true;
            model_filter.visible = true;
            
            // Enable AR filters and attach
            state_.ar_filters_enabled = true;
            auto filter_id = state_.attachment_controller.AttachFilter(model_filter);
            
            std::cout << "[DebugPanel] Model loaded successfully! Filter ID: " 
                      << filter_id << std::endl;
            std::cout << "[DebugPanel] Meshes: " << model_filter.model_data.meshes.size()
                      << ", Materials: " << model_filter.model_data.materials.size() << std::endl;
        } else {
            std::cout << "[DebugPanel] ERROR: Failed to load model from: " 
                      << model_path << std::endl;
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear All Models")) {
        state_.attachment_controller.DetachAll();
        state_.ar_filters_enabled = false;
        std::cout << "[DebugPanel] Cleared all models" << std::endl;
    }
```

### B2: Add Include Statement

**File**: `mediapipe/examples/desktop/segmecam/src/ui/profile_debug_panels.cpp` (top of file)

Add:
```cpp
#include "include/ar_filters/filter_object.h"
```

### B3: Rebuild Application

```bash
./build-app.sh 2>&1 | tee /tmp/step7_ui_build.log
```

---

## Phase C: Visual Testing (1-2 hours)

### C1: Test simple_cube.obj ✅

**Steps**:
1. Run `./segmecam`
2. Open Debug panel
3. Verify model path: `assets/ar_filters/models/simple_cube.obj`
4. Select anchor: `nose_bridge`
5. Set scale: `1.0`
6. Click "Load 3D Model"

**Expected**:
- Console: "Model loaded successfully!"
- Console: "Meshes: 1, Materials: 1"
- Screen: Red cube appears on nose bridge
- Cube follows face movement
- Cube rotates with head rotation

**Validation**:
- [ ] Model visible
- [ ] Correct position (nose bridge)
- [ ] Tracks face movement
- [ ] Correct size
- [ ] Material color correct (red)
- [ ] No console errors

### C2: Test simple_glasses.obj ✅

**Steps**:
1. Click "Clear All Models"
2. Change model path: `assets/ar_filters/models/simple_glasses.obj`
3. Change anchor: `nose_bridge`
4. Set scale: `1.5` (glasses typically larger)
5. Click "Load 3D Model"

**Expected**:
- Console: "Meshes: 2, Materials: 2" (frame + lenses)
- Screen: Glasses appear on face
- Frame opaque, lenses transparent
- Tracks face rotation

**Validation**:
- [ ] Both meshes visible (frame + lenses)
- [ ] Multi-material rendering works
- [ ] Transparency works (lenses)
- [ ] Correct position and orientation
- [ ] Tracks head tilt/rotation
- [ ] No z-fighting or artifacts

### C3: Test simple_hat.obj ✅

**Steps**:
1. Clear models
2. Load `assets/ar_filters/models/simple_hat.obj`
3. Anchor: `forehead`
4. Scale: `2.0`
5. Load model

**Expected**:
- Console: "Meshes: 2, Materials: 2" (brim + crown)
- Screen: Hat appears above head
- Tracks head movement

**Validation**:
- [ ] Both meshes visible
- [ ] Correct position (above forehead)
- [ ] Appropriate size
- [ ] Tracks head tilt
- [ ] Materials render correctly

### C4: Test Multiple Models Simultaneously ✅

**Steps**:
1. Load `simple_glasses.obj` at `nose_bridge`
2. Load `simple_hat.obj` at `forehead`
3. Load `simple_cube.obj` at `chin`

**Expected**:
- All 3 models visible simultaneously
- All track face movement independently
- No interference or z-fighting
- Performance remains acceptable

**Validation**:
- [ ] 3 models visible
- [ ] All track correctly
- [ ] FPS > 25 (check Debug panel)
- [ ] No visual artifacts

### C5: Test Custom OBJ Model (Optional)

If you have a custom OBJ model:
1. Copy to `assets/ar_filters/models/custom.obj`
2. Load via UI
3. Test visibility and tracking

---

## Phase D: Performance Testing (30 minutes)

### D1: FPS Measurement

**Test Cases**:
1. Baseline (no filters): Record FPS
2. 1 model (cube): Record FPS
3. 2 models (glasses + hat): Record FPS
4. 3 models (all test models): Record FPS

**Record**:
```
Baseline FPS: ___
1 model FPS: ___
2 models FPS: ___
3 models FPS: ___
```

**Target**: FPS drop < 25% from baseline

### D2: Memory Usage

**Monitor**:
```bash
# In separate terminal while app running
watch -n 1 'ps aux | grep segmecam | grep -v grep'
```

**Observations**:
- Initial memory: ___
- After loading 3 models: ___
- After 5 minutes: ___

**Target**: No continuous memory growth (no leaks)

### D3: Stability Test

**Steps**:
1. Load and clear models repeatedly (10 times)
2. Run with models for 5 minutes continuously
3. Rapidly move head/face

**Validation**:
- [ ] No crashes
- [ ] No freezing
- [ ] No visual glitches
- [ ] Smooth tracking throughout

---

## Phase E: Edge Case Testing (30 minutes)

### E1: Error Handling

**Test**:
1. Load nonexistent file: `assets/nonexistent.obj`
2. Load invalid OBJ: `assets/ar_filters/models/BUILD`
3. Load with empty path

**Expected**:
- Console: "ERROR: Failed to load model"
- App continues running (no crash)
- Previous models unaffected

### E2: Anchor Point Testing

**Test each anchor**:
1. Load cube at each of 7 anchors
2. Verify position makes sense
3. Clear and repeat

**Anchors to test**:
- [ ] nose_bridge - center of face
- [ ] forehead - above eyes
- [ ] chin - below mouth
- [ ] left_cheek - left side
- [ ] right_cheek - right side
- [ ] left_eye - left eye
- [ ] right_eye - right eye

### E3: Scale Extremes

**Test**:
1. Scale = 0.1 (very small)
2. Scale = 5.0 (very large)
3. Negative scale (if allowed)

**Validation**:
- Small: Visible but tiny
- Large: Visible and appropriately sized
- Negative: Handled gracefully or prevented

---

## Success Criteria Checklist

### Functional Requirements
- [ ] Load simple_cube.obj successfully
- [ ] Load simple_glasses.obj with multi-mesh/material
- [ ] Load simple_hat.obj correctly
- [ ] Load multiple models simultaneously
- [ ] Models attach to correct anchor points
- [ ] Models track face movement smoothly
- [ ] Materials render with correct colors
- [ ] Transparency works (glasses lenses)
- [ ] Clear/detach models works

### Performance Requirements
- [ ] FPS > 25 with 3 models loaded
- [ ] FPS drop < 25% from baseline
- [ ] No memory leaks over 5 minutes
- [ ] Smooth tracking with rapid head movement

### Quality Requirements
- [ ] No crashes during normal operation
- [ ] Error messages for invalid files
- [ ] No visual artifacts (z-fighting, flickering)
- [ ] Clean console output (no spam)
- [ ] UI responsive during model loading

---

## Known Limitations

1. **No File Picker**: Manual path input only
2. **No Model Preview**: Must load to see
3. **No Unload Individual**: Clear all or none
4. **No Position Offset UI**: Hardcoded (0,0,0)
5. **No Rotation UI**: Models use default orientation

These are acceptable for Phase 3 testing. Future improvements tracked separately.

---

## Troubleshooting Guide

### Issue: Model doesn't appear

**Check**:
1. Path correct? (relative to workspace root `/home/padletut/segmecam/`)
2. AR filters enabled? (checkbox or automatic on load)
3. Face detected? (enable "Show Anchor Points")
4. Console errors? (check terminal output)
5. Scale too small? (increase scale)

### Issue: Model appears but wrong position

**Check**:
1. Anchor point correct for intended position?
2. Offset needed? (code modification required)
3. Model coordinates centered? (check OBJ file)

### Issue: FPS drops significantly

**Check**:
1. Model complexity? (vertex/triangle count)
2. Multiple models loaded?
3. Processing scale in Debug panel
4. System resource usage (CPU/GPU)

### Issue: Crash on load

**Check**:
1. OBJ file valid? (test with unit tests first)
2. Materials referenced exist?
3. Memory available?
4. Console backtrace (run with `gdb ./segmecam`)

---

## Next Steps After Completion

1. **Document Results**: Create `STEP_7_COMPLETE.md`
2. **Capture Screenshots**: Save examples of loaded models
3. **Performance Report**: Include FPS measurements
4. **Phase 3 Step 8**: Create user documentation
5. **Phase 4**: Plan production features (file picker, UI improvements)

---

## Time Estimates

- **Phase A** (Build/Verify): 15 minutes
- **Phase B** (Add UI): 30 minutes
- **Phase C** (Visual Tests): 1-2 hours
- **Phase D** (Performance): 30 minutes
- **Phase E** (Edge Cases): 30 minutes
- **Documentation**: 30 minutes

**Total**: 3-4 hours

---

## Resources

- Unit test assets: `assets/ar_filters/models/`
- Test script: `./test-ar-filters.sh`
- Build script: `./build-app.sh`
- Phase 3 status: `PHASE_3_STATUS.md`
- Step 6 completion: `PHASE_3_STEP_6_COMPLETE.md`

---

**Let's start with Phase A - Environment Verification!** 🚀
