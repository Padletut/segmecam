# 🎯 Phase 2: Head Pose & Transform Calculation - Implementation Plan

**Branch**: `feature/ar-filters-foundation`  
**Start Date**: October 1, 2025  
**Estimated Duration**: 5-7 days  
**Status**: 🚧 IN PROGRESS

---

## Overview

Phase 2 builds on Phase 1's face mesh extraction to create a comprehensive 3D transformation system for AR object placement. We'll implement smooth, jitter-free head tracking with multiple attachment points for placing AR filters (glasses, hats, masks, etc.) on the face.

**Prerequisites from Phase 1**: ✅
- Face mesh extraction (478 landmarks)
- Basic pose calculation (yaw/pitch/roll)
- 6 attachment points identified
- Coordinate transformation system

**Phase 2 Goals**:
- Enhanced pose calculation with quaternions for smooth interpolation
- 4×4 transformation matrices for OpenGL rendering
- Temporal smoothing to reduce jitter
- Anchor-specific local coordinate systems
- Kalman filtering for robust tracking

---

## Architecture Design

### Core Components

```
mediapipe/examples/desktop/segmecam/
├── include/ar_filters/
│   ├── transform_calculator.h         # Main transform system
│   └── face_mesh_processor.h          # [Phase 1] ✅
└── src/ar_filters/
    ├── transform_calculator.cpp       # Implementation
    └── face_mesh_processor.cpp        # [Phase 1] ✅
```

### Data Structures

#### 1. HeadPose (Enhanced from Phase 1)
```cpp
struct HeadPose {
    cv::Vec3f position;          // Head center in 3D space (world coordinates)
    cv::Vec4f rotation_quat;     // Quaternion (w, x, y, z) for smooth interpolation
    cv::Vec3f euler_angles;      // Yaw, pitch, roll in degrees (for debugging)
    float scale;                 // Head scale factor (1.0 = average)
    cv::Mat rotation_matrix;     // 3x3 rotation matrix
    cv::Mat transform_matrix;    // 4x4 transformation matrix for OpenGL
    double timestamp;            // Frame timestamp for temporal smoothing
};
```

#### 2. AnchorPoint (New)
```cpp
struct AnchorPoint {
    std::string name;            // "nose_bridge", "left_ear", "right_ear", etc.
    cv::Vec3f position_world;    // 3D position in world space
    cv::Vec3f position_local;    // 3D position in face-local space
    cv::Vec4f orientation_quat;  // Local orientation as quaternion
    cv::Mat transform_matrix;    // 4x4 local-to-world transform
    float scale;                 // Local scale factor (for asymmetric objects)
    bool is_visible;             // Visibility flag (false if occluded/out of frame)
};
```

#### 3. TransformState (For temporal smoothing)
```cpp
struct TransformState {
    HeadPose current_pose;       // Current frame pose
    HeadPose smoothed_pose;      // Temporally smoothed pose
    HeadPose predicted_pose;     // Kalman-predicted pose
    std::vector<AnchorPoint> anchors;  // All anchor points
    double last_update_time;     // Timestamp of last update
    int frame_count;             // Frame counter for filtering
};
```

---

## Implementation Steps

### Step 1: Basic TransformCalculator Structure ⏳

**Goal**: Create class skeleton with core data structures

**Files to Create**:
- `include/ar_filters/transform_calculator.h` (150-200 lines)
- `src/ar_filters/transform_calculator.cpp` (300-400 lines initially)

**Key Methods**:
```cpp
class TransformCalculator {
public:
    // Constructor
    TransformCalculator();
    
    // Initialize with camera intrinsics
    void Initialize(const cv::Mat& camera_matrix, int frame_width, int frame_height);
    
    // Update pose from face mesh
    void Update(const FaceMesh& face_mesh);
    
    // Get current head pose
    const HeadPose& GetHeadPose() const;
    
    // Get specific anchor point
    const AnchorPoint& GetAnchor(const std::string& name) const;
    
    // Get all anchor points
    const std::vector<AnchorPoint>& GetAnchors() const;
    
    // Enable/disable smoothing
    void SetSmoothingEnabled(bool enabled);
    void SetSmoothingAlpha(float alpha);  // 0.0 = no smoothing, 1.0 = max smoothing
    
private:
    // Core transformation methods
    HeadPose CalculatePose(const FaceMesh& face_mesh);
    std::vector<AnchorPoint> CalculateAnchors(const HeadPose& pose, const FaceMesh& face_mesh);
    
    // Coordinate transformations
    cv::Vec4f EulerToQuaternion(const cv::Vec3f& euler);
    cv::Vec3f QuaternionToEuler(const cv::Vec4f& quat);
    cv::Mat QuaternionToMatrix(const cv::Vec4f& quat);
    cv::Mat BuildTransformMatrix(const cv::Vec3f& position, const cv::Vec4f& rotation, float scale);
    
    // Smoothing
    HeadPose SmoothPose(const HeadPose& current, const HeadPose& target);
    
    // State
    TransformState state_;
    cv::Mat camera_matrix_;
    int frame_width_, frame_height_;
    bool smoothing_enabled_;
    float smoothing_alpha_;
    std::mutex state_mutex_;
};
```

**Testing**:
- Basic initialization
- Pose calculation without smoothing
- Quaternion conversions (Euler ↔ Quaternion ↔ Matrix)
- Transform matrix construction

---

### Step 2: Enhanced Pose Calculation ⏳

**Goal**: Improve pose estimation with quaternions and better rotation handling

**Key Improvements**:

1. **Quaternion-based Rotation**:
   - Convert Euler angles from Phase 1's solvePnP to quaternions
   - Enable smooth interpolation (SLERP) between frames
   - Avoid gimbal lock issues

2. **4×4 Transform Matrix**:
   - Combine rotation, translation, and scale
   - OpenGL-compatible format (column-major)
   - Ready for shader consumption in Phase 4+

3. **Improved Position Calculation**:
   - Head center estimation (average of key face landmarks)
   - Depth estimation from face scale
   - World-space positioning

**Implementation**:
```cpp
HeadPose TransformCalculator::CalculatePose(const FaceMesh& face_mesh) {
    HeadPose pose;
    
    // 1. Use Phase 1's Euler angles as starting point
    pose.euler_angles = cv::Vec3f(
        face_mesh.pose.yaw,
        face_mesh.pose.pitch,
        face_mesh.pose.roll
    );
    
    // 2. Convert to quaternion for smooth interpolation
    pose.rotation_quat = EulerToQuaternion(pose.euler_angles);
    
    // 3. Build rotation matrix
    pose.rotation_matrix = QuaternionToMatrix(pose.rotation_quat);
    
    // 4. Calculate head center position
    pose.position = CalculateHeadCenter(face_mesh);
    
    // 5. Get scale from face mesh
    pose.scale = face_mesh.scale;
    
    // 6. Build 4x4 transform matrix
    pose.transform_matrix = BuildTransformMatrix(pose.position, pose.rotation_quat, pose.scale);
    
    // 7. Timestamp
    pose.timestamp = GetCurrentTimeMs();
    
    return pose;
}
```

**Testing**:
- Quaternion math correctness (unit quaternions, proper normalization)
- Matrix conversions match expected results
- Transform matrix decomposes correctly (position, rotation, scale)

---

### Step 3: Anchor Point System ⏳

**Goal**: Calculate precise attachment points with local orientations

**Anchor Definitions** (7 total):

| Anchor Name | Landmarks | Purpose | Local Orientation |
|-------------|-----------|---------|-------------------|
| `nose_bridge` | 6, 197, 195 | Glasses, sunglasses | Forward along face plane |
| `forehead` | 10, 67, 297 | Hats, crowns, tiaras | Upward from face |
| `left_ear` | 234, 127, 162 | Earrings, headphones | Left side, slightly back |
| `right_ear` | 454, 356, 389 | Earrings, headphones | Right side, slightly back |
| `chin` | 152, 199, 175 | Beards, masks | Downward from face |
| `left_temple` | 139, 127, 162 | Glasses arm | Left side |
| `right_temple` | 368, 356, 389 | Glasses arm | Right side |

**Implementation**:
```cpp
std::vector<AnchorPoint> TransformCalculator::CalculateAnchors(
    const HeadPose& pose, 
    const FaceMesh& face_mesh
) {
    std::vector<AnchorPoint> anchors;
    
    // For each anchor type:
    // 1. Average landmark positions to get anchor center
    // 2. Calculate local orientation from nearby landmarks
    // 3. Transform to world space using head pose
    // 4. Check visibility (within frame bounds, not occluded)
    // 5. Build local transform matrix
    
    // Example: Nose bridge (for glasses)
    AnchorPoint nose_bridge;
    nose_bridge.name = "nose_bridge";
    
    // Average of landmarks 6, 197, 195
    cv::Vec3f local_pos = (
        face_mesh.landmarks_pixel[6] +
        face_mesh.landmarks_pixel[197] +
        face_mesh.landmarks_pixel[195]
    ) / 3.0f;
    
    // Calculate forward direction (perpendicular to face plane)
    cv::Vec3f forward = CalculateFaceNormal(face_mesh);
    
    // Local orientation (looking forward)
    nose_bridge.orientation_quat = DirectionToQuaternion(forward);
    
    // Transform to world space
    nose_bridge.position_world = pose.rotation_matrix * local_pos + pose.position;
    nose_bridge.position_local = local_pos;
    
    // Build transform matrix
    nose_bridge.transform_matrix = BuildTransformMatrix(
        nose_bridge.position_world,
        nose_bridge.orientation_quat,
        pose.scale
    );
    
    // Visibility check
    nose_bridge.is_visible = IsWithinFrame(nose_bridge.position_world);
    
    anchors.push_back(nose_bridge);
    
    // ... repeat for other anchors
    
    return anchors;
}
```

**Testing**:
- Anchor positions stable during head movement
- Local orientations correct (forward, up, side directions)
- Transform matrices produce expected object placement
- Visibility flags accurate

---

### Step 4: Temporal Smoothing ⏳

**Goal**: Reduce jitter and provide stable tracking

**Smoothing Techniques**:

1. **Exponential Moving Average (EMA)**:
   - Simple, fast, low latency
   - Configurable alpha (0.2-0.4 typical)
   - Applied separately to position, rotation, scale

2. **SLERP for Quaternions**:
   - Spherical linear interpolation
   - Smooth rotation transitions
   - No gimbal lock

3. **Velocity-based Prediction**:
   - Estimate head movement velocity
   - Predict next frame position
   - Reduce latency

**Implementation**:
```cpp
HeadPose TransformCalculator::SmoothPose(const HeadPose& current, const HeadPose& target) {
    HeadPose smoothed;
    
    if (!smoothing_enabled_) {
        return target;
    }
    
    float alpha = smoothing_alpha_;  // 0.3 typical
    
    // 1. Smooth position (EMA)
    smoothed.position = current.position * (1.0f - alpha) + target.position * alpha;
    
    // 2. Smooth rotation (SLERP)
    smoothed.rotation_quat = Slerp(current.rotation_quat, target.rotation_quat, alpha);
    
    // 3. Smooth scale (EMA)
    smoothed.scale = current.scale * (1.0f - alpha) + target.scale * alpha;
    
    // 4. Rebuild derived data
    smoothed.euler_angles = QuaternionToEuler(smoothed.rotation_quat);
    smoothed.rotation_matrix = QuaternionToMatrix(smoothed.rotation_quat);
    smoothed.transform_matrix = BuildTransformMatrix(
        smoothed.position, 
        smoothed.rotation_quat, 
        smoothed.scale
    );
    
    smoothed.timestamp = target.timestamp;
    
    return smoothed;
}

// Quaternion SLERP implementation
cv::Vec4f TransformCalculator::Slerp(const cv::Vec4f& q1, const cv::Vec4f& q2, float t) {
    // Compute dot product
    float dot = q1[0]*q2[0] + q1[1]*q2[1] + q1[2]*q2[2] + q1[3]*q2[3];
    
    // If negative, negate one quaternion to take shorter path
    cv::Vec4f q2_adjusted = (dot < 0) ? -q2 : q2;
    dot = std::abs(dot);
    
    // If very close, use linear interpolation
    if (dot > 0.9995f) {
        cv::Vec4f result = q1 * (1.0f - t) + q2_adjusted * t;
        return result / cv::norm(result);  // Normalize
    }
    
    // SLERP formula
    float theta = std::acos(dot);
    float sin_theta = std::sin(theta);
    float w1 = std::sin((1.0f - t) * theta) / sin_theta;
    float w2 = std::sin(t * theta) / sin_theta;
    
    return q1 * w1 + q2_adjusted * w2;
}
```

**Testing**:
- Visual smoothness assessment (30 FPS playback)
- Latency measurement (<16ms for 60 FPS)
- Stability during rapid head movements
- No overshooting or oscillation

---

### Step 5: Kalman Filtering (Optional Enhancement) ⏳

**Goal**: Advanced filtering for noisy tracking data

**Benefits**:
- Optimal state estimation under uncertainty
- Handles temporary occlusions
- Velocity and acceleration tracking

**Implementation** (Optional, if time permits):
```cpp
// Simple Kalman filter for 3D position
class KalmanFilter3D {
public:
    void Initialize(const cv::Vec3f& initial_position);
    cv::Vec3f Predict(float dt);
    cv::Vec3f Update(const cv::Vec3f& measurement);
    
private:
    cv::Mat state_;          // [x, y, z, vx, vy, vz]
    cv::Mat covariance_;     // 6x6 covariance matrix
    cv::Mat process_noise_;  // Q matrix
    cv::Mat measurement_noise_;  // R matrix
};
```

**Note**: Can be deferred to Phase 2 polish if time-constrained.

---

### Step 6: Integration & Testing ⏳

**Goal**: Integrate TransformCalculator into application pipeline

**Integration Points**:

1. **MediaPipeManager** (or ApplicationRun):
   ```cpp
   // In mediapipe_manager.cpp or application.cpp
   std::unique_ptr<TransformCalculator> transform_calculator_;
   
   // In Initialize()
   transform_calculator_ = std::make_unique<TransformCalculator>();
   transform_calculator_->Initialize(camera_matrix, width, height);
   
   // In ProcessFrame()
   transform_calculator_->Update(params.app_state.face_mesh);
   params.app_state.head_pose = transform_calculator_->GetHeadPose();
   params.app_state.anchor_points = transform_calculator_->GetAnchors();
   ```

2. **AppState Extensions**:
   ```cpp
   // In app_state.h
   struct AppState {
       // ... existing members
       
       // Phase 2: Transform system
       HeadPose head_pose;
       std::vector<AnchorPoint> anchor_points;
       bool transform_data_available = false;
   };
   ```

3. **Debug Visualization** (optional):
   ```cpp
   // Draw anchor points as colored spheres
   // Draw local coordinate axes at each anchor
   // Display transform matrix values
   ```

**Testing Checklist**:
- [ ] All anchor points calculated correctly
- [ ] Smooth tracking without jitter (visual assessment)
- [ ] Performance <1ms overhead per frame
- [ ] No crashes or memory leaks
- [ ] Works with rapid head movements
- [ ] Handles partial face occlusion
- [ ] Anchor visibility flags correct
- [ ] Transform matrices decompose correctly

---

## Performance Targets

| Metric | Target | Acceptable |
|--------|--------|------------|
| Processing Time | <1ms | <2ms |
| Memory Usage | <50KB | <100KB |
| Smoothing Latency | <16ms (1 frame) | <33ms (2 frames) |
| Quaternion Operations | <0.1ms | <0.2ms |
| Matrix Construction | <0.2ms | <0.5ms |

---

## Dependencies

### Required Libraries (already available):
- ✅ OpenCV (for math operations, matrices)
- ✅ MediaPipe (face mesh data from Phase 1)
- ✅ C++ STL (vectors, strings, etc.)

### Optional (for future enhancement):
- ⏳ GLM (OpenGL Mathematics) - for better 3D math
- ⏳ Eigen (for advanced matrix operations)

**Note**: Start with OpenCV, add GLM later if needed for OpenGL integration.

---

## Success Criteria

### Phase 2 Complete When:
- ✅ TransformCalculator class implemented and tested
- ✅ All 7 anchor points calculated with local orientations
- ✅ Quaternion-based smooth rotation tracking
- ✅ 4×4 transform matrices generated
- ✅ Temporal smoothing reduces jitter
- ✅ Performance targets met (<1ms overhead)
- ✅ Integration with existing pipeline
- ✅ Visual validation shows stable tracking
- ✅ Ready for Phase 3 (3D model loading)

---

## Timeline

| Day | Tasks | Deliverable |
|-----|-------|-------------|
| **Day 1** | Step 1: Class structure, quaternion math | Header + basic implementation |
| **Day 2** | Step 2: Enhanced pose calculation | Working pose system |
| **Day 3** | Step 3: Anchor point calculations | All 7 anchors functional |
| **Day 4** | Step 4: Temporal smoothing (EMA + SLERP) | Smooth tracking |
| **Day 5** | Step 5: Integration + testing | Full pipeline working |
| **Day 6-7** | Polish, optimization, documentation | Phase 2 complete ✅ |

---

## Next Steps After Phase 2

Once Phase 2 is complete, we'll have:
- ✅ Stable head tracking with 7 attachment points
- ✅ 4×4 transform matrices ready for OpenGL
- ✅ Smooth, jitter-free motion

**Phase 3 will add**:
- 3D model loading (OBJ/GLTF)
- Texture management
- OpenGL rendering pipeline
- First AR filter prototype (glasses)

---

## Notes & Decisions

### Design Decisions:
1. **Quaternions over Euler**: Prevents gimbal lock, enables SLERP
2. **OpenCV math**: Consistent with existing codebase, no new dependencies
3. **7 anchors**: Covers all major AR filter types (glasses, hats, masks, earrings)
4. **EMA smoothing**: Simple, effective, low latency
5. **Kalman filter**: Optional enhancement, defer if time-constrained

### Known Challenges:
1. **Gimbal lock**: Solved by quaternions
2. **Jitter**: Addressed by EMA + SLERP
3. **Latency**: Keep smoothing alpha moderate (0.2-0.4)
4. **Occlusion**: Anchor visibility flags + graceful degradation

---

**Document Version**: 1.0  
**Last Updated**: October 1, 2025  
**Status**: Ready to implement Step 1
