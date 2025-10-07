# Phase 6: AR Filter Asset Definition - Implementation Plan

**Status**: ✅ **PHASE 6 COMPLETE**  
**Started**: October 3, 2025  
**Completed Day 1**: October 3, 2025 (Commit: ee492df, 6d15a1d, 1d2cf2a, 41fd7dc)  
**Completed Day 2**: October 3, 2025 (Commit: ea01d84)  
**Estimated Duration**: 1-2 days  
**Depends On**: Phase 5 ✅ Complete

---

## Executive Summary

Phase 6 defines the **asset format and metadata schema** for AR filters. This establishes how filters are packaged, configured, and loaded into SegmeCam.

**Goal**: Create a flexible, JSON-based filter definition system that supports:

- Multiple 3D models per filter
- Face anchor attachment points
- Material and lighting properties
- Blendshape-driven behaviors
- Filter categorization and metadata

**Success Criteria**: Load `simple_glasses.json`, parse configuration, validate assets exist! 🥽✨

---

## What We Need to Implement

### Core Components

1. **FilterAsset Class** - Parse and validate filter JSON files
2. **JSON Schema** - Define filter configuration format
3. **Asset Validation** - Verify models, textures, and resources exist
4. **Filter Enumeration** - Discover available filters in directory
5. **Sample Filters** - Create example filter definitions (glasses, hat, mask)

### JSON Schema Design

```json
{
  "filter": {
    "name": "Classic Glasses",
    "id": "classic-glasses-v1",
    "category": "glasses",
    "version": "1.0.0",
    "author": "SegmeCam",
    "description": "Simple black-framed glasses",
    "thumbnail": "thumbnail.png",
    "icon": "icon.png"
  },
  "attachments": [
    {
      "id": "main_glasses",
      "anchor": "nose_bridge",
      "model": "glasses.obj",
      "texture": "glasses_diffuse.png",
      "scale": [1.0, 1.0, 1.0],
      "offset": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "visible": true,
      "opacity": 1.0
    }
  ],
  "materials": {
    "main_glasses": {
      "ambient": [0.2, 0.2, 0.2],
      "diffuse": [0.8, 0.8, 0.8],
      "specular": [1.0, 1.0, 1.0],
      "shininess": 32.0,
      "opacity": 1.0
    }
  },
  "behaviors": {
    "eyeBlinkLeft": {
      "type": "shake",
      "target": "main_glasses",
      "threshold": 0.7,
      "intensity": 0.05
    },
    "jawOpen": {
      "type": "fall_off",
      "target": "main_glasses",
      "threshold": 0.8,
      "gravity": 9.8
    }
  }
}
```

---

## Implementation Timeline

### Day 1: Core FilterAsset Implementation

**Goal**: Parse JSON, validate assets, enumerate filters

#### Task 1.1: Create FilterAsset Header

**File**: `include/ar_filters/filter_asset.h`

**Structures**:

```cpp
namespace segmecam {
namespace ar_filters {

struct FilterMetadata {
  std::string name;
  std::string id;
  std::string category;
  std::string version;
  std::string author;
  std::string description;
  std::string thumbnail_path;
  std::string icon_path;
};

struct FilterAttachment {
  std::string id;
  std::string anchor_name;      // "nose_bridge", "left_ear", etc.
  std::string model_path;
  std::string texture_path;
  glm::vec3 scale{1.0f, 1.0f, 1.0f};
  glm::vec3 offset{0.0f, 0.0f, 0.0f};
  glm::vec3 rotation{0.0f, 0.0f, 0.0f};
  bool visible = true;
  float opacity = 1.0f;
};

struct FilterMaterial {
  glm::vec3 ambient{0.2f, 0.2f, 0.2f};
  glm::vec3 diffuse{0.8f, 0.8f, 0.8f};
  glm::vec3 specular{1.0f, 1.0f, 1.0f};
  float shininess = 32.0f;
  float opacity = 1.0f;
};

struct FilterBehavior {
  enum class Type {
    SHAKE,           // Jitter on blendshape activation
    FALL_OFF,        // Physics drop on threshold
    SCALE,           // Scale with blendshape value
    ROTATE,          // Rotate with blendshape value
    HIDE,            // Hide when threshold reached
    COLOR_CHANGE     // Change color on activation
  };
  
  Type type;
  std::string target_attachment_id;
  std::string blendshape_name;
  float threshold = 0.5f;
  float intensity = 1.0f;
  float gravity = 9.8f;  // For FALL_OFF type
};

class FilterAsset {
public:
  FilterAsset() = default;
  ~FilterAsset() = default;
  
  // Load filter from directory containing filter.json
  static absl::StatusOr<FilterAsset> LoadFromDirectory(
      const std::string& filter_dir);
  
  // Load filter from explicit JSON file path
  static absl::StatusOr<FilterAsset> LoadFromFile(
      const std::string& json_path);
  
  // Enumerate all filters in a root directory
  static std::vector<std::string> EnumerateFilters(
      const std::string& filters_root);
  
  // Validate that all referenced assets exist
  absl::Status ValidateAssets() const;
  
  // Getters
  const FilterMetadata& GetMetadata() const { return metadata_; }
  const std::vector<FilterAttachment>& GetAttachments() const { 
    return attachments_; 
  }
  const std::map<std::string, FilterMaterial>& GetMaterials() const {
    return materials_;
  }
  const std::vector<FilterBehavior>& GetBehaviors() const {
    return behaviors_;
  }
  
  std::string GetFilterDirectory() const { return filter_directory_; }

private:
  FilterMetadata metadata_;
  std::vector<FilterAttachment> attachments_;
  std::map<std::string, FilterMaterial> materials_;
  std::vector<FilterBehavior> behaviors_;
  std::string filter_directory_;  // Base directory for relative paths
  
  // JSON parsing helpers
  static absl::Status ParseMetadata(const nlohmann::json& json,
                                    FilterMetadata& metadata);
  static absl::Status ParseAttachments(const nlohmann::json& json,
                                       std::vector<FilterAttachment>& attachments);
  static absl::Status ParseMaterials(const nlohmann::json& json,
                                     std::map<std::string, FilterMaterial>& materials);
  static absl::Status ParseBehaviors(const nlohmann::json& json,
                                     std::vector<FilterBehavior>& behaviors);
};

} // namespace ar_filters
} // namespace segmecam
```

#### Task 1.2: Implement FilterAsset Parser

**File**: `src/ar_filters/filter_asset.cpp`

**Key Implementation Points**:

1. Use `nlohmann/json` for JSON parsing (already in project)
2. Load JSON file with error handling
3. Parse each section (metadata, attachments, materials, behaviors)
4. Resolve relative paths based on filter directory
5. Validate required fields are present
6. Validate asset files exist on disk

#### Task 1.3: Add JSON Dependency to BUILD

**File**: `src/ar_filters/BUILD`

```python
cc_library(
    name = "filter_asset",
    srcs = ["filter_asset.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/filter_asset.h"],
    includes = ["../.."],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@glm//:glm",
        "@nlohmann_json//:json",  # JSON parsing library
    ],
    copts = ["-std=c++17"],
)
```

---

### Day 2: Sample Filters and Testing

**Goal**: Create sample filter definitions and test loading

#### Task 2.1: Create Sample Filter Assets

**Directory Structure**:

```
assets/filters/
├── classic-glasses/
│   ├── filter.json
│   ├── glasses.obj
│   ├── glasses_diffuse.png
│   ├── thumbnail.png
│   └── icon.png
├── party-hat/
│   ├── filter.json
│   ├── hat.obj
│   ├── hat_diffuse.png
│   ├── thumbnail.png
│   └── icon.png
└── cat-ears/
    ├── filter.json
    ├── left_ear.obj
    ├── right_ear.obj
    ├── ear_texture.png
    ├── thumbnail.png
    └── icon.png
```

**Sample filter.json for classic-glasses**:

```json
{
  "filter": {
    "name": "Classic Glasses",
    "id": "classic-glasses-v1",
    "category": "glasses",
    "version": "1.0.0",
    "author": "SegmeCam",
    "description": "Simple black-framed glasses for everyday use",
    "thumbnail": "thumbnail.png",
    "icon": "icon.png"
  },
  "attachments": [
    {
      "id": "glasses_frame",
      "anchor": "nose_bridge",
      "model": "glasses.obj",
      "texture": "glasses_diffuse.png",
      "scale": [1.0, 1.0, 1.0],
      "offset": [0.0, 0.0, 0.0],
      "rotation": [0.0, 0.0, 0.0],
      "visible": true,
      "opacity": 1.0
    }
  ],
  "materials": {
    "glasses_frame": {
      "ambient": [0.2, 0.2, 0.2],
      "diffuse": [0.1, 0.1, 0.1],
      "specular": [0.8, 0.8, 0.8],
      "shininess": 64.0,
      "opacity": 1.0
    }
  },
  "behaviors": {
    "eyeBlinkLeft": {
      "type": "shake",
      "target": "glasses_frame",
      "threshold": 0.7,
      "intensity": 0.03
    },
    "eyeBlinkRight": {
      "type": "shake",
      "target": "glasses_frame",
      "threshold": 0.7,
      "intensity": 0.03
    }
  }
}
```

#### Task 2.2: Create Unit Tests

**File**: `src/ar_filters/filter_asset_test.cpp`

**Test Cases**:

1. Load valid filter JSON
2. Parse all metadata fields correctly
3. Load multiple attachments
4. Parse materials and behaviors
5. Validate missing required fields fail
6. Validate missing asset files fail
7. Enumerate filters in directory
8. Handle malformed JSON gracefully

#### Task 2.3: Integration with ARRenderer

Update ARRenderer to accept FilterAsset:

```cpp
// In ar_renderer.h
absl::Status LoadFilter(const FilterAsset& filter);
absl::Status UnloadFilter(const std::string& filter_id);
```

---

## File Checklist

- [ ] `include/ar_filters/filter_asset.h` - FilterAsset class definition
- [ ] `src/ar_filters/filter_asset.cpp` - JSON parsing implementation
- [ ] `src/ar_filters/BUILD` - Build configuration with JSON dep
- [ ] `src/ar_filters/filter_asset_test.cpp` - Unit tests
- [ ] `assets/filters/classic-glasses/filter.json` - Sample filter 1
- [ ] `assets/filters/party-hat/filter.json` - Sample filter 2
- [ ] `assets/filters/cat-ears/filter.json` - Sample filter 3
- [ ] `assets/filters/README.md` - Filter asset documentation

---

## Success Criteria

✅ FilterAsset class compiles and links  
✅ Load sample filter.json successfully  
✅ Parse all metadata, attachments, materials, behaviors  
✅ Validate asset files exist  
✅ Enumerate multiple filters in directory  
✅ Unit tests pass (8+ test cases)  
✅ Integration with ARRenderer (LoadFilter method)  
✅ Documentation complete

---

## ✅ Phase 6 Day 1 Completion Summary

**Commits:**
- `ee492df` - FilterAsset class implementation (header + source)
- `6d15a1d` - Sample filter definitions and comprehensive documentation
- `1d2cf2a` - Unit tests and bug fixes (10 tests, 100% pass)

**Deliverables:**
- ✅ `filter_asset.h` - FilterAsset class API (~230 lines)
- ✅ `filter_asset.cpp` - JSON parsing implementation (~450 lines)
- ✅ `filter_asset_test.cpp` - Comprehensive unit tests (~560 lines, 10 test cases)
- ✅ `assets/filters/README.md` - Complete documentation (~600 lines)
- ✅ 3 sample filters: classic-glasses, party-hat, cat-ears
- ✅ Full JSON schema support: metadata, attachments, materials, behaviors
- ✅ Build integration (BUILD file configuration)
- ✅ All Codacy checks pass (no issues)

**Tests:** 10/10 passing ✅
1. Load minimal valid filter
2. Load filter with full metadata
3. Load multiple attachments
4. Load materials
5. Load behaviors
6. Missing required fields error handling
7. Malformed JSON error handling
8. ValidateAssets with missing files
9. EnumerateFilters directory scanning
10. Load real sample filters

---

## Next Phase Preview

**Phase 7: AR Filter Manager** will:

- Use FilterAsset to load/unload filters
- Manage active filter state
- Connect filter definitions to ARRenderer
- Handle filter lifecycle and switching
- Apply blendshape behaviors in real-time

---

## ✅ Phase 6 Day 2 Completion Summary

**Commit:** `ea01d84` - ARRenderer + FilterAsset integration

**Goal Achieved:** ARRenderer can now load filter definitions from JSON and convert them to renderable 3D objects with proper transforms and face anchoring.

**Deliverables:**

### Code Changes (~500 lines total)

- ✅ `ar_renderer.h` modifications:
  - Added `LoadFilter(const FilterAsset&)` method declaration
  - Added `UnloadFilter(const std::string& filter_id)` method
  - Added `HasFilter(const std::string& filter_id)` method
  - Added `loaded_filters_` tracking map (filter_id → instance names)
  - Added FilterAsset forward declaration

- ✅ `ar_renderer.cpp` implementation (~130 lines):
  - **LoadFilter**: Converts FilterAsset to ModelInstance objects
    - Validates renderer initialization
    - Prevents duplicate filter loading (AlreadyExistsError)
    - Iterates through filter attachments
    - Loads 3D models via ModelLoader
    - Creates ModelInstance with transform data (offset, scale, rotation)
    - Converts Euler angles (degrees) to quaternions for OpenGL
    - Sets anchor points from filter definition
    - Loads textures via TextureManager
    - Tracks loaded instances by filter_id
    - Returns absl::Status for error handling
  
  - **UnloadFilter**: Removes all model instances for a filter
    - Returns NotFoundError if filter not loaded
    - Cleans up from loaded_filters_ tracking map
  
  - **HasFilter**: Checks if filter currently loaded

- ✅ `ar_renderer_filter_test.cpp` (~230 lines):
  - Integration test with 4 test cases:
    1. **TestLoadFilterIntoRenderer**: Load classic-glasses, verify HasFilter, unload
    2. **TestLoadMultipleFilters**: Load all 3 sample filters simultaneously
    3. **TestLoadFilterTwice**: Verify AlreadyExistsError on duplicate load
    4. **TestUnloadNonexistentFilter**: Verify NotFoundError on invalid unload
  - Graceful handling of missing OpenGL context (headless/CI compatibility)
  - All tests build successfully ✅

- ✅ `BUILD` file updates:
  - Added `:filter_asset` dependency to `ar_renderer` target
  - Added OpenCV include paths to multiple targets:
    - `model_loader`: `-I/usr/include/opencv4`
    - `texture_manager`: `-I/usr/include/opencv4`
    - `ar_renderer`: `-I/usr/include/opencv4`
    - `ar_renderer_filter_test`: `-I/usr/include/opencv4`
  - Added OpenCV deps to `ar_renderer`:
    - `//mediapipe/framework/port:opencv_core`
    - `//mediapipe/framework/port:opencv_imgproc`
  - Added OpenCV linkopts:
    - `ar_renderer`: `-lopencv_core`, `-lopencv_imgproc`
    - `ar_renderer_filter_test`: `-lopencv_core`, `-lopencv_imgproc`, `-lopencv_imgcodecs`
  - Created `ar_renderer_filter_test` binary target

### Technical Implementation Details

**Transform Conversion Pipeline:**
1. FilterAttachment stores offset (vec3), scale (vec3), rotation (vec3 Euler angles in degrees)
2. LoadFilter converts rotation to radians: `glm::radians(attachment.rotation)`
3. Creates quaternion from Euler angles: `glm::quat(euler_radians)`
4. Passes quaternion to ModelInstance for OpenGL rendering

**Face Anchor Mapping:**
- Filter JSON specifies anchor string ("nose_bridge", "left_ear", "right_ear", etc.)
- ARRenderer::LoadFilter passes anchor to ModelInstance
- ModelInstance will use anchor to position model relative to face landmarks
- 9 supported anchors: nose_bridge, left_ear, right_ear, forehead, chin, left_eye, right_eye, mouth_center, face_center

**Error Handling:**
- Returns `absl::InvalidArgumentError` if renderer not initialized
- Returns `absl::AlreadyExistsError` if filter_id already loaded
- Returns `absl::NotFoundError` if trying to unload non-existent filter
- Logs errors for missing models or textures but continues loading other attachments

### Build & Test Results

- ✅ Build successful with proper flags: `-c opt --action_env=PKG_CONFIG_PATH --cxxopt=-I/usr/local/include/opencv4`
- ✅ All Codacy checks pass (0 issues)
- ⚠️ Runtime tests require OpenGL context (expected limitation for headless environments)
- ✅ Code compiles and links correctly with all dependencies

### Integration Points

**From FilterAsset (Day 1) → To ARRenderer (Day 2):**
- `FilterAsset::GetAttachments()` → `ARRenderer::LoadFilter()` iteration
- `FilterAttachment.model_path` → `ModelLoader::LoadModel()`
- `FilterAttachment.texture_path` → `TextureManager::LoadTexture()`
- `FilterAttachment.{offset, scale, rotation}` → `ModelInstance` transform
- `FilterAttachment.anchor_name` → `ModelInstance` face anchor
- `FilterAsset.GetMetadata().id` → `loaded_filters_` tracking key

### Lines of Code

- Day 2 additions: ~500 lines
  - ar_renderer.cpp: ~130 lines of LoadFilter/UnloadFilter/HasFilter logic
  - ar_renderer_filter_test.cpp: ~230 lines of integration tests
  - ar_renderer.h: ~10 lines of declarations
  - BUILD: ~50 lines of configuration changes

---

## Phase 6 Total Statistics

**Total Lines Added:** ~1,600 lines across Days 1-2
- FilterAsset system (Day 1): ~680 lines (header + source)
- Documentation (Day 1): ~600 lines
- Unit tests (Day 1): ~560 lines
- ARRenderer integration (Day 2): ~370 lines
- BUILD configuration: ~100 lines across both days

**Commits:** 5 total
- Day 1: ee492df, 6d15a1d, 1d2cf2a, 41fd7dc (4 commits)
- Day 2: ea01d84 (1 commit)

**Test Coverage:**
- Unit tests: 10/10 passing ✅
- Integration tests: 4/4 compiling ✅ (runtime requires OpenGL context)

---

## Next Phase Preview

**Phase 7: AR Filter Manager** will:

- Create FilterManager class for lifecycle management
- Load/unload filters using FilterAsset + ARRenderer integration
- Manage active filter state and switching
- Apply blendshape-driven behaviors in real-time
- Handle filter categories and enumeration for UI
- Integrate with application main loop

**Phase 7 Prerequisites (Complete):**
- ✅ FilterAsset JSON parsing (Phase 6 Day 1)
- ✅ ARRenderer LoadFilter/UnloadFilter API (Phase 6 Day 2)
- ✅ Sample filter definitions (3 filters)
- ✅ Face landmark detection (Phase 1)
- ✅ 3D rendering pipeline (Phases 4-5)

---

**Document Version**: 2.0  
**Created**: October 3, 2025  
**Day 1 Complete**: October 3, 2025  
**Day 2 Complete**: October 3, 2025  
**Status**: ✅ Phase 6 Complete - Ready for Phase 7 🚀
