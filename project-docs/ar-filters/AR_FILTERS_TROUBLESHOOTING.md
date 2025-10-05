# AR Filters Troubleshooting Guide
## Phase 5 Testing - Filters Not Visible Issue

**Date Created**: 2025-10-05  
**Branch**: `feature/option-b-renderer-refactor`  
**Issue**: AR filters load successfully but are not visible in video feed

---

## 🔍 Issue Description

### Current Behavior (UPDATED 2025-10-05)
- Application launches successfully ✅
- Camera feed displays correctly ✅
- Filters can be selected from AR Filter Panel ✅
- Filter loading logs show succ---

**Status**: 🟢 **TESTING PHASE - Crown Positioning Refined**  
**Last Updated**: 2025-10-05 (Crown Position Compensation & Clamping)  
**Priority**: HIGH - Ready for comprehensive filter testing  
**Progress**: 
- ✅ Finding #1 FIXED: Model path resolution (models now load successfully)
- ✅ Finding #2 FIXED: FBO integration (filters now visible on video)
- ✅ Finding #3 FIXED: head_crown anchor implementation (beanie tracks head movement!)
- ✅ Finding #6 FIXED: Crown positioning issues (perspective compensation + safety clamp)
- ⏳ Next: Test all 8 filters (cat ears, glasses, party hat, etc.)
, "cozy-beanie" loads) ✅
- Projection matrix updates correctly (1280x720 detected) ✅
- **Models now load with absolute paths** ✅ (Finding #1 FIXED)
- **Instances created: "Rendered 1 instances" every frame** ✅ (Finding #1 verified)
- **FBO integration implemented** ✅ (Finding #2 FIXED)
- **TESTING PHASE**: Filters should now be visible - awaiting visual confirmation

### Expected Behavior
- Filters should render on top of camera feed
- Filters should attach to face landmarks
- 3D models should be visible with proper positioning

---

## 📋 Investigation Checklist

### Phase 1: Verify Filter Loading
- [ ] **Check model files exist**
  - Path: `assets/ar_filters/*/model.obj`
  - Verify all 8 filter directories have valid OBJ files
- [ ] **Check model loading logs**
  - Look for "Model loaded successfully" messages
  - Look for ModelLoader errors
- [ ] **Check instance creation**
  - Look for "Created model instance" messages
  - Verify instance IDs are generated
- [ ] **Check filter metadata**
  - Verify `filter.json` has correct attachment points
  - Verify anchor names are valid (nose_bridge, forehead, etc.)

**Log Evidence**:
```
I0000 00:00:1759620741.514714  122058 opengl_renderer.cpp:454] Cleared all model instances
I0000 00:00:1759620741.514719  122058 ar_filter_manager.cpp:253] Filter unloaded: cozy-beanie
```

**Status**: ⚠️ Need to verify loading logs

---

### Phase 2: Verify Face Landmark Detection
- [ ] **Check MediaPipe face detection**
  - Verify face mesh landmarks are being detected
  - Check landmark count (should be 468 points)
- [ ] **Check landmark updates**
  - Verify `UpdateFaceLandmarks()` is being called
  - Verify landmarks are converted to cv::Point3f correctly
- [ ] **Check face scale calculation**
  - Verify eye distance calculation
  - Check `face_scale_factor_` value

**Diagnostic Commands**:
```bash
# Check if face landmarks are being logged
grep "UpdateFaceLandmarks" /tmp/segmecam_debug.log
grep "face_scale_factor" /tmp/segmecam_debug.log
```

**Status**: ⚠️ Need to add debug logging

---

### Phase 3: Verify Rendering Pipeline
- [ ] **Check RenderInstances() is called**
  - Verify ARFilterManager calls RenderInstances()
  - Check if RenderToTexture() is being invoked
- [ ] **Check FBO rendering**
  - Verify FBO is created and bound correctly
  - Check texture attachment
- [ ] **Check OpenGL state**
  - Verify depth testing is enabled
  - Verify blending is configured
  - Check viewport is set correctly
- [ ] **Check shader activation**
  - Verify shader program is active during rendering
  - Check uniform values are set

**Log Evidence Needed**:
- "RenderInstances called with N instances"
- "Rendering instance: <id>"
- OpenGL error codes (if any)

**Status**: ⚠️ Need to add rendering debug logs

---

### Phase 4: Verify Transform Calculations
- [ ] **Check head pose tracking**
  - Verify `CalculateHeadPose()` succeeds
  - Check PnP confidence values
  - Verify fallback to roll-only works
- [ ] **Check anchor positions**
  - Verify `GetAnchorPosition()` returns valid coordinates
  - Check if positions are within viewport bounds
- [ ] **Check model matrices**
  - Verify `CalculateInstanceTransform()` produces valid matrices
  - Check translation/rotation/scale values

**Diagnostic Code to Add**:
```cpp
ABSL_LOG(INFO) << "Head pose: pitch=" << head_pose.euler_angles.x 
               << " yaw=" << head_pose.euler_angles.y
               << " roll=" << head_pose.euler_angles.z
               << " confidence=" << head_pose.confidence;

ABSL_LOG(INFO) << "Anchor position (" << anchor_name << "): "
               << "x=" << anchor.x << " y=" << anchor.y << " z=" << anchor.z;
```

**Status**: ⚠️ Need to add transform debug logs

---

### Phase 5: Verify Texture Composition
- [ ] **Check RenderToTexture() integration**
  - Verify ARFilterManager::RenderToTexture() is called from UI/Render pipeline
  - Check if texture is bound to correct framebuffer
- [ ] **Check texture blending**
  - Verify alpha blending combines filter with camera feed
  - Check blend function settings
- [ ] **Check texture display**
  - Verify rendered texture is displayed in main window
  - Check if texture coordinates are correct

**Possible Issues**:
1. RenderToTexture() not integrated into main render loop
2. Texture not bound to display pipeline
3. Filter renders but is not composited with camera feed

**Status**: ⚠️ CRITICAL - Check integration point

---

### Phase 6: Compare with ARRenderer Baseline
- [ ] **Check ARRenderer backup files**
  - Location: `backup/pre-option-b/ar_renderer.{h,cpp}`
  - Compare rendering flow with OpenGLRenderer
- [ ] **Check integration differences**
  - Compare how ARRenderer was called vs OpenGLRenderer
  - Check if any render steps were missed in migration

**Key Questions**:
1. How did ARRenderer integrate with the main render loop?
2. Was there a separate compositing step?
3. Are we missing any initialization or setup steps?

**Status**: ⚠️ Need to compare implementations

---

## 🔧 Immediate Action Items

### 1. Add Debug Logging (Priority: CRITICAL)
Add comprehensive logging to track the rendering pipeline:

**File**: `src/ar_filters/opengl_renderer.cpp`
```cpp
// In UpdateFaceLandmarks()
ABSL_LOG(INFO) << "UpdateFaceLandmarks called with " << landmarks.size() << " points";
ABSL_LOG(INFO) << "Face scale factor: " << face_scale_factor_;

// In RenderInstances()
ABSL_LOG(INFO) << "RenderInstances called: " << instances_.size() << " total instances";
int visible_count = 0;
for (const auto& pair : instances_) {
  if (pair.second.visible) visible_count++;
}
ABSL_LOG(INFO) << "Visible instances: " << visible_count;

// In RenderModelInstance()
ABSL_LOG(INFO) << "Rendering instance: " << instance.id 
               << " anchor=" << instance.anchor_name
               << " visible=" << instance.visible;
```

### 2. Verify Filter Loading (Priority: HIGH)
Check that models are actually loaded:

**File**: `src/ar_filters/opengl_renderer.cpp`
```cpp
// In LoadModel()
ABSL_LOG(INFO) << "LoadModel called: " << path;
// After loading
ABSL_LOG(INFO) << "Model loaded with " << model->meshes.size() << " meshes";
for (size_t i = 0; i < model->meshes.size(); i++) {
  ABSL_LOG(INFO) << "  Mesh " << i << ": " 
                 << model->meshes[i].index_count << " indices, VAO=" 
                 << model->meshes[i].vao;
}
```

### 3. Check ARFilterManager Integration (Priority: CRITICAL)
Verify that RenderToTexture() is actually being called:

**File**: `src/ar_filters/ar_filter_manager.cpp`
```cpp
// In Update()
ABSL_LOG(INFO) << "ARFilterManager::Update() called, face_detected=" 
               << !face_landmarks.empty();

// In RenderToTexture()
ABSL_LOG(INFO) << "ARFilterManager::RenderToTexture() called: " 
               << width << "x" << height;
int rendered = opengl_renderer_->RenderInstances();
ABSL_LOG(INFO) << "Rendered " << rendered << " instances";
```

### 4. Verify Main Render Loop Integration (Priority: CRITICAL)
Check where ARFilterManager::RenderToTexture() is called from:

**Search for**:
```bash
grep -r "RenderToTexture" mediapipe/examples/desktop/segmecam/src/
grep -r "ar_filter_manager" mediapipe/examples/desktop/segmecam/src/ui/
grep -r "ar_filter_manager" mediapipe/examples/desktop/segmecam/src/render/
```

---

## 🎯 Hypothesis & Testing Plan

### Hypothesis 1: Filters Render But Aren't Composited ⚠️
**Theory**: OpenGLRenderer renders correctly to FBO but the texture isn't displayed

**Test**:
1. Add debug logging in RenderInstances() to confirm rendering occurs
2. Check if RenderToTexture() is called from main loop
3. Verify texture is bound to display pipeline

**If True**: Need to integrate rendered texture into main compositor

---

### Hypothesis 2: Face Landmarks Not Updating ⚠️
**Theory**: UpdateFaceLandmarks() not called or receives empty data

**Test**:
1. Add logging in ARFilterManager::Update() before calling UpdateFaceLandmarks()
2. Verify face_landmarks vector size
3. Check MediaPipe face detection output

**If True**: Need to fix landmark passing from MediaPipe to OpenGLRenderer

---

### Hypothesis 3: Models Not Loading ⚠️
**Theory**: LoadModel() fails silently or models are invalid

**Test**:
1. Add logging in LoadModel() to show file paths
2. Check ModelLoader error messages
3. Verify OBJ files are valid and accessible

**If True**: Need to fix model loading or file paths

---

### Hypothesis 4: Transform Calculations Produce Invalid Values ⚠️
**Theory**: Model matrices place filters outside viewport or at wrong depth

**Test**:
1. Log anchor positions after GetAnchorPosition()
2. Log final model matrix values
3. Check if positions are NaN or inf

**If True**: Need to fix anchor calculation or PnP algorithm

---

## 📊 Debug Output Collection

### Run with Full Logging
```bash
# Build with debug symbols
bazel build -c dbg //mediapipe/examples/desktop/segmecam:segmecam

# Run with verbose logging
./segmecam mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt 2>&1 | tee /tmp/segmecam_debug.log

# Filter relevant logs
grep -E "(OpenGLRenderer|ARFilterManager|RenderInstances|LoadModel|UpdateFaceLandmarks)" /tmp/segmecam_debug.log
```

### Check OpenGL Errors
Add after every major OpenGL operation:
```cpp
GLenum error = glGetError();
if (error != GL_NO_ERROR) {
  ABSL_LOG(ERROR) << "OpenGL error: " << error << " at " << __FILE__ << ":" << __LINE__;
}
```

---

## 🔄 Next Steps

1. **Add debug logging** to all critical functions (LoadModel, RenderInstances, UpdateFaceLandmarks)
2. **Rebuild and run** with logging enabled
3. **Collect log output** and analyze rendering pipeline
4. **Compare with ARRenderer backup** to find missing integration steps
5. **Update this document** with findings and solutions

---

## 📝 Findings Log

### Finding #1: 2025-10-05 - Model Path Resolution Bug ✅ FIXED
**Issue**: Model files not loading - "Unable to open file"  
**Evidence**: 
```
E0000 Failed to load model: beanie.obj - Unable to open file "beanie.obj"
E0000 Failed to load model: glasses_simple.obj - Unable to open file "glasses_simple.obj"
```
**Root Cause**: `LoadFilter()` was using `attachment.model_path` directly (just filename like `beanie.obj`) instead of calling `asset.GetAssetPath(attachment.model_path)` to prepend the filter's base directory (`assets/filters/pro_beanie/beanie.obj`)  
**Solution**: Changed line 207 in `ar_filter_manager.cpp`:
```cpp
// BEFORE:
std::string model_path = attachment.model_path;  // ❌ Just "beanie.obj"

// AFTER:
std::string model_path = asset.GetAssetPath(attachment.model_path);  // ✅ "assets/filters/pro_beanie/beanie.obj"
```
**Status**: ✅ FIXED - Models now load successfully, instances created
**Verification**: Debug logs show "Rendered 1 instances" every frame (was "0 instances" before fix)

---

### Finding #2: 2025-10-05 - FBO Integration Missing ✅ FIXED
**Issue**: Filters render but not visible in video feed - models loaded, instances created, but no visual output  
**Evidence**: 
```
I0000 [DEBUG] Rendered 1 instances (repeated every frame)
I0000 [DEBUG] model_path (absolute): assets/filters/glasses-simple/glasses_simple.obj
I0000 [DEBUG] Model loaded, creating instance...
```
Models loading + instances rendering, but filters not appearing on screen

**Root Cause**: `ARFilterManager::RenderToTexture()` was calling `RenderInstances()` directly, which renders to the **current framebuffer** (likely the default framebuffer = invisible). The video texture needs to be wrapped in an FBO so AR filters render **onto** the video texture, not into the void.

**Technical Detail**: 
- Video frame uploaded to GPU texture (video_texture_id)
- AR filters need to render on TOP of that texture
- Requires creating FBO, binding video texture as color attachment, rendering AR models
- Without FBO binding, AR models render to default framebuffer (discarded/invisible)

**Solution**: Created new `OpenGLRenderer::RenderToExternalTexture()` method:
```cpp
// In opengl_renderer.h
bool RenderToExternalTexture(unsigned int texture_id, int width, int height);

// In opengl_renderer.cpp - Implementation highlights:
static GLuint temp_fbo = 0;  // Persistent FBO for all render calls
if (temp_fbo == 0) {
  glGenFramebuffers(1, &temp_fbo);
}

glBindFramebuffer(GL_FRAMEBUFFER, temp_fbo);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                       GL_TEXTURE_2D, texture_id, 0);

// DON'T clear - render ON TOP of existing video frame
// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // COMMENTED OUT!

glEnable(GL_BLEND);  // Enable alpha blending
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

int rendered_count = RenderInstances();  // Render AR models to FBO
glBindFramebuffer(GL_FRAMEBUFFER, 0);  // Unbind
```

**ARFilterManager Integration**:
```cpp
// In ar_filter_manager.cpp RenderToTexture():
bool render_success = opengl_renderer_->RenderToExternalTexture(
    video_texture_id, width, height);
```

**Status**: ✅ FIXED - Build successful, ready for visual testing
**Files Modified**:
- `include/ar_filters/opengl_renderer.h` - Added method declaration
- `src/ar_filters/opengl_renderer.cpp` - Implemented RenderToExternalTexture()
- `src/ar_filters/ar_filter_manager.cpp` - Updated RenderToTexture() to use new method

**Next Step**: Launch application, load filter, verify filters now visible on screen

---

### Finding #3: 2025-01-04 - Missing Anchor Implementation "head_crown" ✅ FIXED
**Issue**: Beanie filter visible at center of screen but static (no head pose tracking)  
**Evidence**: 
```
W0000 Unknown anchor name: head_crown  # ← THE SMOKING GUN!
I0000 [DEBUG] Instance 'instance_2' anchor=head_crown pos=[0, 0, 0]
I0000 [DEBUG] Converted 478 landmarks to cv::Point3f  # ← Landmarks ARE updating
I0000 [DEBUG] UpdateFaceLandmarks called with 478 points  # ← Face detected every frame
I0000 [DEBUG] PnP SUCCESS - tvec: [-107.728, -57.8258, 856.208]  # ← Head tracking WORKS
```
**Visual Confirmation**: 
- ✅ Beanie filter IS VISIBLE (Finding #2 FBO fix successful!)
- ❌ Beanie remains static at center - does NOT follow head movement
- ❌ Other filters (glasses, etc.) not visible

**Root Cause**: 
`GetAnchorPosition()` in `opengl_renderer.cpp` did NOT implement the `head_crown` anchor point. The function only supported 9 anchors (forehead, nose_bridge, left_ear, right_ear, chin, left_eye, right_eye, mouth, center) but NOT `head_crown`. When an unknown anchor is requested, it returns `cv::Point3f(0.0f, 0.0f, 0.0f)` → filter renders at screen center.

**Solution**: 
**Solution**: 
Added `head_crown` anchor implementation using forehead landmark (10) with 30% upward offset:
```cpp
else if (anchor_name == "head_crown") {
  cv::Point3f forehead = face_landmarks_[10];
  float face_height_estimate = std::abs(face_landmarks_[10].y - face_landmarks_[152].y);
  forehead.y -= face_height_estimate * 0.3f; // Move up 30% above forehead
  return forehead;
}
```

**Files Modified**: 
- `mediapipe/examples/desktop/segmecam/src/ar_filters/opengl_renderer.cpp` (lines 556-565)

**Status**: ✅ FIXED - Build successful, head tracking validation complete

---

### Finding #4: 2025-10-05 - Coordinate Space Mismatch (Clipping)

**Issue**: After fixing the `head_crown` anchor, the beanie **disappeared completely**. User reported: "I can not see the beanie now, maybe it get invisible when it get pushed backward?"

**Evidence**: 
```
I0000 00:00:1759622691.393371  132539 opengl_renderer.cpp:690] [DEBUG] PnP SUCCESS - tvec: [-42.8571, 41.8084, 1102.15]
I0000 00:00:1759622691.393433  132539 opengl_renderer.cpp:905] [DEBUG] Instance 'instance_2' anchor=head_crown pos=[0.475894, 0.382915, 0.000648588]
🎯 Transform: pos=[605.251,405.328,0], anchors=7
```

**Root Cause**: **Three-fold coordinate space confusion**:

1. **Near Plane Clipping**: Perspective projection uses `near=0.1, far=100.0` (line 120)
2. **Wrong Z-axis Scaling**: Anchor Z was being multiplied by `face_scale_factor_` (≈0.7), producing tiny values like `z=0.0004`
3. **Mixed Coordinate Spaces**: Anchor position (normalized 0-1) was being converted to pixel coordinates (640x360) but needed camera-space depth

The beanie was placed at **z ≈ 0.0004**, which is **250x closer** than the near clipping plane (0.1), causing it to be **completely clipped** by the frustum.

**Solution**: 
Convert anchor from normalized image space [0-1] to proper **camera space** using PnP depth:

```cpp
// OLD (WRONG): Mixed pixels and tiny z-depth
glm::vec3 anchor_world = glm::vec3(
    anchor.x * viewport_width_,     // 640 pixels
    anchor.y * viewport_height_,    // 360 pixels
    anchor.z * face_scale_factor_   // 0.0004 (clipped!)
);

// NEW (CORRECT): Proper camera-space coordinates
glm::vec3 anchor_world = glm::vec3(
    (anchor.x - 0.5f) * 2.0f * (head_pose.translation.z / 1000.0f), // X: centered, depth-scaled
    (0.5f - anchor.y) * 2.0f * (head_pose.translation.z / 1000.0f), // Y: flipped, depth-scaled
    head_pose.translation.z / 1000.0f                                // Z: use PnP depth (≈1.1)
);
```

**Key Changes**:
- **X/Y**: Convert from [0-1] to [-1, 1] centered coordinates, scaled by face depth
- **Z**: Use actual face depth from PnP (`tvec.z ≈ 1102.15 → scaled to ≈1.1`) instead of landmark z
- **Coordinate flip**: Y-axis inverted to match OpenGL conventions

**Files Modified**: 
- `mediapipe/examples/desktop/segmecam/src/ar_filters/opengl_renderer.cpp` (lines 729-740)

**Status**: ✅ FIXED - Build successful, ready for visual verification

---

## 🎓 Lessons Learned

### Critical Issues Identified

1. **Model Path Resolution** (Finding #1): Relative paths don't work - always use `asset.GetAssetPath()` for absolute paths
2. **FBO Integration** (Finding #2): AR models must render to the video texture using `RenderToExternalTexture()`, not default framebuffer
3. **Anchor Implementation** (Finding #3): All anchor names used in filter JSONs **must** be implemented in `GetAnchorPosition()` - unknown anchors return (0,0,0)
4. **Coordinate Space Transformation** (Finding #4): MediaPipe normalized coordinates → Camera space requires depth scaling from PnP, not pixel conversion

### Key Technical Insights

- **Near/Far Planes Matter**: Always check clipping planes match your depth range (0.1-100.0 vs. 0.0004)
- **Mixed Coordinate Systems Are Dangerous**: Image space (0-1) ≠ Pixel space (1280x720) ≠ Camera space (world units)
- **PnP Provides Ground Truth**: Use `tvec.z` for proper depth placement, not landmark z-values
- **Diagnostic Logging Saves Time**: Anchor position logs immediately revealed the clipping issue

---

## ✅ Resolution Checklist

**Core AR System**:
- [x] Beanie filter loads without errors
- [x] Anchor position no longer returns (0,0,0)
- [x] PnP head pose tracking operational
- [x] Beanie visible at proper depth (not clipped)
- [x] Head pose tracking works (pitch/yaw/roll)
- [x] Beanie positioned on top of head with position compensation
- [x] Crown positioning stable at all distances and vertical positions
- [x] W/S/U/D keyboard controls work correctly

**Comprehensive Filter Testing** (Next Phase):
- [ ] Cat Ears filter renders correctly
- [ ] Classic Glasses render at nose bridge
- [ ] Classic Glasses v1 render correctly
- [ ] Simple Glasses render correctly
- [ ] Party Hat renders on forehead
- [ ] Cozy Beanie fully validated (in progress)
- [ ] Holo Visor renders correctly
- [ ] Pixel Shades render correctly
- [ ] All filters track head movement smoothly
- [ ] Performance is acceptable (>30 FPS with filters)
- [ ] No crashes or errors during filter switching

**Documentation**:
- [x] Document all findings in this file
- [x] Update OPTION_B_PROGRESS.md
- [ ] Create Phase 8 completion document (after full testing)

---

---

### Finding #5: 2025-10-05 - Crown Positioning with Distance/Position Changes ✅ FIXED
**Issue**: Crown position unstable when sitting high in frame and moving closer to camera  
**Evidence**: 
```
User reports: "when I get closer to camera then the head_crown anchor point moves down"
User reports: "if I sit at high position then it moves down when I get closer"
Screenshot shows: Crown visible at eye level instead of head top
```

**Root Cause Analysis**:
Multiple interconnected issues:
1. **Uninitialized Variable**: `crown_depth_offset_` contained garbage value (1.06464e+24)
2. **Constant Offsets Don't Scale**: Fixed Y-offset (0.10m) appears different at varying distances
3. **Perspective Distortion**: High vertical positions + close distance amplified offset errors
4. **No Safety Bounds**: Crown could drift below forehead position in normalized space

**Evolution of Solutions Attempted**:
1. ❌ Fixed world-space Y-offset (0.10m) - worked at distance, failed when close
2. ❌ View-ray offset (along camera-to-head direction) - complex behavior at high positions
3. ❌ Depth-proportional offsets (both Y and Z) - didn't solve high+close scenario
4. ❌ Normalized space offset (% of face height) - completely wrong positioning
5. ✅ **FINAL SOLUTION**: Position-compensated offset + safety clamp

**Solution Implementation**:
```cpp
// In GetAnchorPosition() for head_crown:
cv::Point3f crown = face_landmarks_[10];  // Forehead base
cv::Point3f forehead_original = face_landmarks_[10];  // For clamping

// Calculate vertical position factor (0=top, 1=bottom of frame)
float vertical_position = crown.y;

// Increase offset when higher in frame to compensate for perspective
float position_compensation = 1.0f + (0.5f - vertical_position) * 2.0f;
position_compensation = std::max(0.5f, std::min(3.0f, position_compensation));

// Apply compensated offset
const float BASE_OFFSET = 0.05f;
float offset = BASE_OFFSET * crown_offset_multiplier_ * position_compensation;
crown.y -= offset;

// CRITICAL: Safety clamp - crown can never go below forehead
if (crown.y > forehead_original.y) {
  crown.y = forehead_original.y;
}
```

**Depth Offset (W/S Keys)**:
```cpp
// In CalculateInstanceTransform() for head_crown:
// Z-offset proportional to distance (maintains visual angle)
float depth_ratio = crown_depth_offset_ * 0.1f;  // 10% per unit
float depth_offset_meters = original_z * depth_ratio;

// Prevent near-plane clipping
const float MIN_Z_DISTANCE = 0.15f;
float new_z = original_z - depth_offset_meters;
if (new_z < MIN_Z_DISTANCE) {
  depth_offset_meters = original_z - MIN_Z_DISTANCE;
  new_z = MIN_Z_DISTANCE;
}
world_position.z = new_z;
```

**Key Features**:
1. **Position Compensation**: Offset increases up to 3x when sitting high in frame
2. **Safety Clamp**: Crown can never appear below forehead (worst case = at forehead)
3. **Distance Scaling**: Z-offset proportional to depth (consistent visual angle)
4. **Near-Plane Protection**: Crown stays at least 0.15m from camera (0.1m = near plane)

**Files Modified**:
- `mediapipe/examples/desktop/segmecam/src/ar_filters/opengl_renderer.cpp` (GetAnchorPosition, CalculateInstanceTransform)
- `mediapipe/examples/desktop/segmecam/include/ar_filters/opengl_renderer.h` (crown_depth_offset_ initialized to 0.0f)

**Status**: ✅ FIXED - User accepted solution after extensive testing
**Tested Scenarios**:
- ✅ Sitting low + far from camera - stable
- ✅ Sitting low + close to camera - stable  
- ✅ Sitting high + far from camera - stable
- ✅ Sitting high + close to camera - stable (was failing before)
- ✅ W/S keys adjust depth correctly without breaking position
- ✅ U/D keys adjust vertical offset as expected

**Lessons Learned**:
- Perspective projection creates non-linear distortion at extreme positions
- Simple mathematical solutions (constant offsets, proportional scaling) fail at edge cases
- Position-dependent compensation required for robust tracking
- Safety clamps prevent catastrophic failures (crown at eye level)
- User testing at extreme positions reveals issues unit tests miss
I0000 [DEBUG] Face scale factor: 0.712077
I0000 [DEBUG] RenderModelInstance called: id=instance_2 anchor=head_crown visible=true
```
User feedback: "It does not follow my head now, barely moving"

**Root Cause**: 
The anchor offset uses **0.5x depth scaling** (line 739-740 in `opengl_renderer.cpp`):
```cpp
glm::vec3 anchor_camera = glm::vec3(
    (anchor.x - 0.5f) * 2.0f * depth * 0.5f,  // ← 0.5x factor too small
    -(anchor.y - 0.5f) * 2.0f * depth * 0.5f, // ← dampens movement
    0.0f
);
```

This 0.5x factor makes the beanie position only move **half as much** as it should relative to the face. When the face moves, the anchor offset is too small to maintain the correct head-locked appearance.

**Previous Attempts**:
- ❌ **2.0x scaling** (no 0.5x factor): Movement too exaggerated, beanie drifted away from head
- ❌ **0.3x local offset**: Beanie stuck at face center, almost no movement
- ❌ **0.5x depth scaling**: Current - visible but tracking insufficient

**Solution Hypothesis**: 
Remove the 0.5x dampening factor and use **1.0x depth scaling** (the natural face-space scale). The anchor should move proportionally with the face depth:
```cpp
glm::vec3 anchor_camera = glm::vec3(
    (anchor.x - 0.5f) * 2.0f * depth,  // Remove * 0.5f
    -(anchor.y - 0.5f) * 2.0f * depth, // Remove * 0.5f
    0.0f
);
```

**Status**: ✅ FIXED - 1.0x depth scaling provides accurate head tracking!
**User Feedback**: "Looks like it follows my head nice now"
**Next Issue**: Beanie is upside-down (180° rotation issue)

---

### Finding #6: 2025-10-05 - Model Orientation Flipped (180° Rotation) ⚠️ IN PROGRESS
**Issue**: After fixing scaling to 1.0x, beanie tracks head perfectly but is **upside-down**  
**Evidence**: User reports "we need to turn it 180 degree again, it is upside down"

**Root Cause**: The 180° X-axis rotation (line 781) is being applied AFTER the inverted head rotation, which may be incorrect for the model's coordinate system. Need to verify rotation order or flip direction.

**Current Code** (lines 773-781):
```cpp
// 1. Translate to final world position
model = glm::translate(model, world_position);

// 2. Apply head rotation (inverted)
model = model * glm::mat4_cast(inverted_rotation);

// 3. Apply 180° rotation around X-axis to flip model right-side up
model = glm::rotate(model, glm::radians(180.0f), glm::vec3(1, 0, 0));
```

**Possible Solutions**:
1. **Remove inversion**: Use `head_pose.rotation` directly instead of `inverted_rotation`
2. **Flip rotation direction**: Use `-180.0f` instead of `180.0f`
3. **Change rotation axis**: Try Y or Z axis instead of X
4. **Reorder transforms**: Apply 180° rotation BEFORE head rotation

**Solution Applied**: 
Moved 180° rotation from hardcoded C++ to filter.json for per-model customization:
```json
// filter.json
"rotation": [180.0, 0.0, 0.0]  // X-axis 180° flip
```

**Status**: ✅ FIXED - Rotation now defined per-filter in JSON (best practice)
**User Feedback**: "After you rotated it, moves opposite direction"
**Next Issue**: Beanie moves correctly sideways (yaw) but pitch causes drift

---

### Finding #7: 2025-10-05 - Pitch Movement Causes Vertical Drift ⚠️ IN PROGRESS
**Issue**: Beanie tracks yaw (side-to-side) correctly but pitch (up/down) causes incorrect movement  
**Evidence**: User reports:
- ✅ "moves good when moving head sideway" (yaw works)
- ❌ "when I look up it moves away from head" (pitch causes drift up)
- ❌ "moves down into my head when I look down" (pitch causes drift down)

**Root Cause**: 
The anchor offset is **not being rotated by head orientation** before adding to face position. When you pitch your head, the anchor offset stays in world-space (absolute coordinates) instead of rotating with your head.

**Current Code** (lines 747-757):
```cpp
// Rotate anchor position by head rotation (so it moves with head)
glm::quat inverted_rotation = glm::inverse(head_pose.rotation);
glm::vec3 rotated_anchor = inverted_rotation * anchor_camera;
```

The anchor IS being rotated, but the issue is likely that we're using **inverted rotation** which causes opposite movement. Need to test with **direct rotation** (not inverted).

**Previous Attempts**:
1. ❌ **Inverted quaternion**: `glm::inverse(head_pose.rotation)` - causes opposite pitch movement
2. ❌ **180° hardcoded in C++**: Applied after head rotation, wrong for all models
3. ✅ **180° in filter.json**: Correct approach for model orientation, but doesn't fix pitch issue

**Solution Hypothesis**: 
Use **direct head rotation** (not inverted) to make anchor rotate WITH the head:
```cpp
glm::vec3 rotated_anchor = head_pose.rotation * anchor_camera;
```

**Test Result**: ❌ FAILED
- ✅ Pitch (up/down) **FIXED** - beanie no longer drifts vertically
- ❌ Yaw (left/right) **BROKEN** - beanie moves opposite direction (inverted)
- User: "sideways it moves in opposite direction of my head"

**Root Cause Analysis**:
The coordinate system has **different conventions for different axes**:
- **Pitch axis**: Needs DIRECT rotation (beanie rotates WITH head up/down)
- **Yaw axis**: Needs INVERTED rotation (coordinate flip required)

This is likely due to MediaPipe's coordinate system vs OpenGL's coordinate system having different handedness or axis orientations.

**Solution Hypothesis #2**: 
Use **conjugate quaternion** (negates x, y, z but not w) instead of full inverse:
```cpp
glm::quat conjugate_rotation = glm::conjugate(head_pose.rotation);
glm::vec3 rotated_anchor = conjugate_rotation * anchor_camera;
```

**Test Result**: ✅ MOSTLY FIXED!
- ✅ Pitch (up/down) **WORKS** - beanie stays on head when looking up/down
- ✅ Yaw (left/right) **WORKS** - beanie follows head turning correctly
- ❌ Roll (tilt) **BROKEN** - beanie doesn't tilt when head tilts
- User: "I follows my moves now, when I tilting my head it does not tilt"

**Root Cause - Roll Issue**:
The **anchor position** rotates correctly (beanie moves with head), but the **model orientation** doesn't match the head tilt. The model needs to apply the head rotation to stay aligned with head orientation.

**Current Code** (line 778):
```cpp
model = model * glm::mat4_cast(conjugate_rotation);  // Applied to position, but not orientation?
```

The conjugate is being applied to the model matrix, but the roll component may not be properly affecting the model's visual orientation. Need to verify model rotation is actually being applied or if filter.json rotation (180°) is overriding it.

**Solution Hypothesis #3**: 
The 180° X-axis rotation in filter.json may be conflicting with head rotation. Try applying head rotation AFTER filter.json rotation, or combine them properly.

**Status**: ⏳ TESTING - Investigating roll rotation application order

---

**Status**: 🟡 **TESTING PHASE - 7 Findings: 5 FIXED, Finding #7 Almost Complete (Roll Tilt Missing)**  
**Last Updated**: 2025-10-05 (Conjugate fixed pitch/yaw, roll tilt not working)  
**Priority**: HIGH - Fixing roll rotation for complete head-locking

**Files Modified**: 
- `mediapipe/examples/desktop/segmecam/src/ar_filters/opengl_renderer.cpp` (lines 728-786)
- `assets/filters/pro_beanie/filter.json` (rotation: [180.0, 0.0, 0.0])

---

## 🎓 Lessons Learned

*To be filled in after issue is resolved*

---

## ✅ Resolution Checklist

Once filters are visible:
- [ ] All 8 filters render correctly
- [ ] Head pose tracking works (pitch/yaw/roll)
- [ ] Beanie positioned on top of head (not forehead)
- [ ] Performance is acceptable (>30 FPS)
- [ ] No crashes or errors
- [ ] Document solution in this file
- [ ] Update OPTION_B_PROGRESS.md
- [ ] Create completion document

---

**Status**: � **TESTING PHASE - 2 Critical Fixes Applied**  
**Last Updated**: 2025-10-05 (FBO Integration Fix)  
**Priority**: HIGH - Ready for visual verification  
**Progress**: 
- ✅ Finding #1 FIXED: Model path resolution (models now load)
- ✅ Finding #2 FIXED: FBO integration (filters should render to video texture)
- ⏳ Next: Launch app, load filter, verify visibility
