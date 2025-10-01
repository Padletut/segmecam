# Phase 1: Face Mesh Extraction - Implementation Plan

## Overview

**Goal**: Extract MediaPipe's 478-point 3D face mesh to enable accurate AR filter placement and tracking.

**Duration**: Week 2 (Oct 1-7, 2025)  
**Status**: 🚧 In Progress  
**Branch**: `feature/ar-filters-phase1-face-mesh`

## Dependencies

✅ **Phase 0 Complete**: BlendshapeProcessor provides 52 expression coefficients  
✅ **MediaPipe Integration**: FaceLandmarker already outputs face_landmarks  
✅ **Build System**: ar_filters module established

## Objectives

### 1. Face Mesh Data Structure
- [ ] Create `FaceMesh` struct to hold 478 3D points
- [ ] Store both normalized (0-1) and pixel coordinates
- [ ] Include face geometry metadata (triangulation, UV coordinates)
- [ ] Add confidence scores per landmark

### 2. FaceMeshProcessor Class
- [ ] Parse MediaPipe FaceLandmarker output (478 points)
- [ ] Convert from normalized to pixel/world coordinates
- [ ] Track face pose (rotation, translation, scale)
- [ ] Detect face visibility (left profile, right profile, frontal)
- [ ] Calculate face bounding box in 3D

### 3. MediaPipe Integration
- [ ] Update graph to output full 478-point mesh (if not already)
- [ ] Add face_mesh_poller to application pipeline
- [ ] Process face mesh packets in main loop
- [ ] Store mesh in app_state

### 4. Face Regions & Landmarks
- [ ] Define semantic regions (forehead, nose, cheeks, chin, etc.)
- [ ] Key landmark indices for AR attachment points:
  * Nose bridge (glasses attachment)
  * Forehead center (hat/crown attachment)
  * Temples (glasses arms)
  * Chin (beard/accessories)
  * Cheeks (face paint)
  * Eyes (makeup)
  * Lips (lipstick enhancement)

### 5. Testing & Validation
- [ ] Verify 478 points extracted correctly
- [ ] Test face pose tracking accuracy
- [ ] Validate coordinate transformations
- [ ] Debug visualization (render mesh points)

## Technical Details

### MediaPipe Face Mesh Structure

MediaPipe FaceLandmarker provides:
- **478 landmarks** in 3D space (x, y, z)
- **Normalized coordinates** (0-1 relative to image)
- **Depth values** (z-coordinate for 3D positioning)
- **Face pose** (rotation matrix and translation vector)

### Key Landmark Indices (from MediaPipe)

```cpp
// Critical attachment points for AR filters
constexpr int NOSE_TIP = 1;
constexpr int NOSE_BRIDGE = 6;
constexpr int FOREHEAD_CENTER = 10;
constexpr int LEFT_EYE_CENTER = 468;
constexpr int RIGHT_EYE_CENTER = 473;
constexpr int LEFT_TEMPLE = 234;
constexpr int RIGHT_TEMPLE = 454;
constexpr int CHIN_CENTER = 152;
constexpr int LEFT_CHEEK = 205;
constexpr int RIGHT_CHEEK = 425;
constexpr int MOUTH_CENTER = 13;
```

### Face Mesh Regions

1. **Silhouette** (35 points): Face outline for masking
2. **Left Eye** (71 points): Eye region for effects
3. **Right Eye** (71 points): Eye region for effects
4. **Left Eyebrow** (18 points): Brow positioning
5. **Right Eyebrow** (18 points): Brow positioning
6. **Nose** (28 points): Glasses bridge attachment
7. **Lips** (40 points): Lipstick enhancement
8. **Face Oval** (36 points): Face boundary

## Implementation Steps

### Step 1: Create FaceMeshProcessor (Similar to BlendshapeProcessor)

```cpp
// src/ar_filters/face_mesh_processor.h
namespace segmecam {

struct FaceMesh {
    std::array<cv::Point3f, 478> landmarks_3d;  // 3D coordinates
    std::array<cv::Point2f, 478> landmarks_2d;  // Pixel coordinates
    cv::Mat rotation_matrix;                     // 3x3 rotation
    cv::Vec3f translation_vector;                // Translation
    float scale;                                 // Face scale
    float confidence;                            // Detection confidence
    int64_t timestamp_us;                        // Timestamp
    
    cv::Point3f GetLandmark3D(int index) const;
    cv::Point2f GetLandmark2D(int index) const;
};

class FaceMeshProcessor {
public:
    void Update(const mediapipe::NormalizedLandmarkList& landmarks,
                int image_width, int image_height);
    
    const FaceMesh& GetFaceMesh() const;
    
    // Face pose queries
    cv::Vec3f GetFaceRotation() const;
    cv::Vec3f GetFaceTranslation() const;
    float GetFaceScale() const;
    
    // Visibility queries
    bool IsFrontalView() const;  // Face looking forward
    bool IsLeftProfile() const;   // Face turned left
    bool IsRightProfile() const;  // Face turned right
    
    // AR attachment points
    cv::Point3f GetNoseBridge() const;
    cv::Point3f GetForeheadCenter() const;
    cv::Point3f GetLeftTemple() const;
    cv::Point3f GetRightTemple() const;
    
    void Reset();
    
private:
    FaceMesh current_mesh_;
    
    void ConvertToPixelCoordinates(const mediapipe::NormalizedLandmarkList& landmarks,
                                   int image_width, int image_height);
    void CalculateFacePose();
    void CalculateFaceScale();
};

} // namespace segmecam
```

### Step 2: Update MediaPipe Processing

```cpp
// In mediapipe_processor.cpp
void MediaPipeProcessor::ProcessFaceMesh(
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_mesh_poller,
    AppState& app_state,
    int frame_count,
    int image_width,
    int image_height
) {
    mediapipe::Packet packet;
    while (face_mesh_poller->QueueSize() > 0 && face_mesh_poller->Next(&packet)) {
        try {
            const auto& landmarks = packet.Get<mediapipe::NormalizedLandmarkList>();
            
            if (landmarks.landmark_size() == 478) {
                app_state.face_mesh_processor.Update(landmarks, image_width, image_height);
                app_state.face_mesh = app_state.face_mesh_processor.GetFaceMesh();
                app_state.face_mesh_available = true;
                
                if (frame_count <= 5) {
                    std::cout << "✅ Got 478-point face mesh" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error processing face mesh: " << e.what() << std::endl;
            app_state.face_mesh_available = false;
        }
    }
}
```

### Step 3: Update app_state

```cpp
// Add to app_state.h
#include "src/ar_filters/face_mesh_processor.h"

struct AppState {
    // ... existing fields ...
    
    // Face Mesh (478 3D points)
    FaceMeshProcessor face_mesh_processor;
    FaceMesh face_mesh;
    bool face_mesh_available = false;
};
```

### Step 4: Visualization (Debug Mode)

Create debug panel to visualize mesh:
- Render 478 points as small circles
- Color-code by region (eyes=blue, nose=red, mouth=green, etc.)
- Show face pose axes (X, Y, Z)
- Display confidence score

## Testing Plan

### Unit Tests
- [ ] Test coordinate conversion (normalized → pixel)
- [ ] Test face pose calculation
- [ ] Test landmark access methods
- [ ] Test visibility detection

### Integration Tests
- [ ] Verify mesh extraction from MediaPipe
- [ ] Test with different face angles
- [ ] Test with multiple faces (use first face)
- [ ] Test with low-quality input

### Performance Tests
- [ ] Benchmark mesh processing time (<1ms target)
- [ ] Verify no memory leaks
- [ ] Test real-time stability (30 FPS)

## Success Criteria

✅ 478 face mesh points extracted every frame  
✅ Face pose (rotation/translation) calculated accurately  
✅ AR attachment points correctly identified  
✅ No performance regression (maintain 30+ FPS)  
✅ Debug visualization shows correct mesh  
✅ All tests pass with zero Codacy issues

## File Structure

```
mediapipe/examples/desktop/segmecam/
├── src/ar_filters/
│   ├── blendshape_processor.h          ✅ Phase 0
│   ├── blendshape_processor.cpp        ✅ Phase 0
│   ├── face_mesh_processor.h           🚧 Phase 1 (NEW)
│   ├── face_mesh_processor.cpp         🚧 Phase 1 (NEW)
│   └── BUILD                           ✅ Updated
├── include/application/
│   ├── app_state.h                     🚧 Update with FaceMesh
│   ├── mediapipe_processor.h           🚧 Add ProcessFaceMesh
│   └── ...
└── ...
```

## Timeline

**Day 1-2**: FaceMeshProcessor class implementation  
**Day 3-4**: MediaPipe integration and testing  
**Day 5**: Debug visualization  
**Day 6**: Performance optimization  
**Day 7**: Documentation and Phase 2 planning

## Next Phase Preview

**Phase 2: 3D Rendering Engine** (Week 3)
- OpenGL ES 3.0 shader pipeline
- 3D object loading (OBJ/FBX support)
- Texture management
- Camera projection matrix
- AR object rendering on face mesh

## Resources

- [MediaPipe Face Landmark Detection Guide](https://developers.google.com/mediapipe/solutions/vision/face_landmarker)
- [Face Mesh 478 Landmark Indices](https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_geometry/data/canonical_face_model.obj)
- OpenCV coordinate transformation functions
- SegmeCam Phase 0 implementation (reference)

---

**Status**: Ready to begin implementation! 🚀
