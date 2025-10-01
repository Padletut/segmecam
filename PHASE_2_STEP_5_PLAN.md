# 🎯 Phase 2 Step 5: Filter Attachment System

**Status**: 🚧 IN PROGRESS  
**Started**: January 11, 2025  
**Branch**: feature/ar-filters-foundation  
**Goal**: Create attachment system connecting AR filter objects to tracked anchor points

---

## 📋 Overview

This step bridges anchor tracking (Phase 2 Steps 1-4) with actual 3D model rendering (Phase 3+). We'll define how filter objects attach to anchor points and move with head pose.

**Key Principle**: Test with simple geometric primitives first, before loading complex 3D models.

---

## 🎯 Objectives

1. **Define FilterObject class** - Represent a single filter component
2. **Implement AttachmentController** - Manage filter-to-anchor bindings
3. **Add coordinate transforms** - Convert anchor positions to filter transforms
4. **Create simple test primitives** - Spheres/cubes for visual validation
5. **Test with head movement** - Ensure smooth tracking

---

## 📐 Architecture Design

### Data Flow
```
AnchorPoint (from TransformCalculator)
    ↓
AttachmentController.AttachFilter()
    ↓
FilterObject (position, rotation, scale)
    ↓
Render as OpenGL primitive (cube/sphere)
    ↓
Visual validation: Does it track smoothly?
```

### Class Hierarchy
```
FilterObject
├── Geometry (vertices, indices)
├── Transform (position, rotation, scale)
├── Material (color, alpha)
└── AttachmentConfig (anchor_name, offset, rotation)

AttachmentController
├── active_filters: vector<FilterObject>
├── AttachFilter(anchor_name, filter_config)
├── UpdateFilterTransforms(anchor_points, head_pose)
├── DetachFilter(filter_id)
└── DetachAll()
```

---

## 📁 Files to Create

### 1. `include/ar_filters/filter_object.h`
**Purpose**: Define FilterObject data structure and simple geometry primitives

**Key Components**:
```cpp
struct FilterObject {
  std::string id;
  std::string anchor_name;        // "nose_bridge", "forehead", etc.
  
  // Geometry (simple primitives for now)
  std::vector<glm::vec3> vertices;
  std::vector<unsigned int> indices;
  
  // Transform
  glm::vec3 position;             // World position
  glm::quat rotation;             // Orientation
  glm::vec3 scale;                // Size
  
  // Attachment config
  glm::vec3 offset;               // Offset from anchor point
  glm::vec3 local_rotation_euler; // Local rotation (degrees)
  
  // Material
  glm::vec4 color;                // RGBA
  float alpha;                    // Transparency
  
  // State
  bool visible;
  bool enabled;
};

// Simple geometry generators
FilterObject CreateCube(const std::string& anchor_name, float size = 0.05f);
FilterObject CreateSphere(const std::string& anchor_name, int segments = 16);
FilterObject CreateCylinder(const std::string& anchor_name, float radius = 0.02f, float height = 0.1f);
```

### 2. `include/ar_filters/attachment_controller.h`
**Purpose**: Manage filter-to-anchor bindings and coordinate transforms

**Key Components**:
```cpp
class AttachmentController {
public:
  AttachmentController();
  ~AttachmentController();
  
  // Filter management
  std::string AttachFilter(const FilterObject& filter);
  bool DetachFilter(const std::string& filter_id);
  void DetachAll();
  
  // Transform updates (called each frame)
  void UpdateFilterTransforms(
    const std::vector<AnchorPoint>& anchors,
    const HeadPose& head_pose
  );
  
  // Access
  const std::vector<FilterObject>& GetActiveFilters() const;
  FilterObject* GetFilter(const std::string& filter_id);
  
  // Configuration
  void SetGlobalScale(float scale);
  void SetCoordinateSystem(CoordinateSystem system);
  
private:
  std::vector<FilterObject> active_filters_;
  std::map<std::string, size_t> filter_id_map_;
  float global_scale_ = 1.0f;
  
  // Helper methods
  glm::mat4 CalculateFilterTransform(
    const FilterObject& filter,
    const AnchorPoint& anchor,
    const HeadPose& head_pose
  );
  
  AnchorPoint* FindAnchor(
    const std::vector<AnchorPoint>& anchors,
    const std::string& anchor_name
  );
};
```

### 3. `src/ar_filters/filter_object.cpp`
**Implementation**: FilterObject and primitive geometry generators

### 4. `src/ar_filters/attachment_controller.cpp`
**Implementation**: AttachmentController logic

---

## 🔧 Implementation Steps

### Step 5.1: Create FilterObject Structure ✅ COMPLETE

- [x] Define `FilterObject` struct in `include/ar_filters/filter_object.h`
- [x] Implement primitive geometry generators (cube, sphere, cylinder, cone)
- [x] Add to BUILD file
- [x] Successful compilation verified

### Step 5.2: Implement AttachmentController ✅ COMPLETE

- [x] Create `AttachmentController` class in `include/ar_filters/attachment_controller.h`
- [x] Implement `AttachFilter()` - bind filter to anchor
- [x] Implement `UpdateFilterTransforms()` - update positions each frame
- [x] Implement `DetachFilter()` and `DetachAll()`
- [x] Add to BUILD file
- [x] Successful compilation verified

### Step 5.3: Coordinate Transform Logic ✅ COMPLETE

- [x] Implement `CalculateFilterTransform()` - anchor → filter matrix
- [x] Handle offset application (local space)
- [x] Handle rotation application (euler → quaternion)
- [x] Handle scale application
- [x] Test with identity transforms first

### Step 5.4: Integration with FrameProcessor ⏳ IN PROGRESS

- [ ] Add `AttachmentController` instance to `FrameProcessor`
- [ ] Call `UpdateFilterTransforms()` after anchor calculation
- [ ] Store filter transforms in `AppState` for rendering access
- [ ] Add debug logging for first 5 frames

### Step 5.5: Simple Rendering Test ⏳ PENDING

- [ ] Add `RenderFilterPrimitives()` helper in `frame_processor.cpp`
- [ ] Use OpenCV drawing for initial validation (circles/rectangles)
- [ ] Verify filters move with anchor points
- [ ] Test with head movement

### Step 5.6: Testing & Validation ⏳ PENDING

- [ ] Attach cube to `nose_bridge` anchor
- [ ] Attach sphere to `forehead` anchor
- [ ] Test offset application (move filter away from anchor)
- [ ] Test rotation application (rotate filter)
- [ ] Test scale application
- [ ] Verify smooth tracking with head movement
- [ ] Check stability with stability thresholds

---

## 🧪 Testing Strategy

### Unit Tests
```cpp
// Test primitive generation
FilterObject cube = CreateCube("nose_bridge", 0.05f);
EXPECT_EQ(cube.vertices.size(), 8);  // 8 vertices for cube

// Test attachment
AttachmentController controller;
std::string id = controller.AttachFilter(cube);
EXPECT_FALSE(id.empty());

// Test transform calculation
FilterObject filter = CreateSphere("forehead");
filter.offset = glm::vec3(0.0f, 0.05f, 0.0f);  // 5cm above anchor
// ... test transform calculation
```

### Visual Tests
1. **Static Attachment**: Attach cube to nose bridge, verify position
2. **Head Movement**: Move head left/right, verify cube follows
3. **Offset Test**: Set offset to (0, 0.1, 0), verify cube above anchor
4. **Rotation Test**: Set rotation to (0, 90, 0), verify cube rotates
5. **Multiple Filters**: Attach 2-3 filters to different anchors
6. **Stability Impact**: Low stability anchors should show jitter

---

## 📊 Success Criteria

- ✅ FilterObject struct defined with all required fields
- ✅ Primitive geometry generators working (cube, sphere, cylinder)
- ✅ AttachmentController can attach/detach filters
- ✅ Filters update positions each frame
- ✅ Coordinate transforms correctly map anchor → filter
- ✅ Offsets and rotations apply correctly
- ✅ Filters track smoothly with head movement
- ✅ No performance regression (< 1ms overhead)

---

## 🚀 Next Steps (Phase 2 Step 6)

After Step 5 is complete:
- Create 2-3 actual filter examples using this system
- Add filter selection UI
- Performance profiling
- Documentation

---

## 📝 Notes

- **Keep it simple**: Use basic primitives (cubes/spheres) for initial testing
- **OpenGL later**: For now, render with OpenCV circles/rectangles to validate transforms
- **Coordinate system**: Need to ensure anchor 2D positions convert correctly to filter 3D positions
- **Performance**: AttachmentController should be very lightweight (< 1ms per frame)

---

## 🔗 Related Documents

- [PHASE_2_STEP_4_COMPLETE.md](PHASE_2_STEP_4_COMPLETE.md) - Anchor stability tracking
- [AR_FILTERS_IMPLEMENTATION_PLAN.md](AR_FILTERS_IMPLEMENTATION_PLAN.md) - Overall AR filters plan
- [PHASE_2_TRANSFORM_PLAN.md](PHASE_2_TRANSFORM_PLAN.md) - Phase 2 overview
