# Phase 6: AR Filter Asset Definition - Implementation Plan

**Status**: 🔄 **IN PROGRESS**  
**Started**: October 3, 2025  
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

## Next Phase Preview

**Phase 7: AR Filter Manager** will:
- Use FilterAsset to load/unload filters
- Manage active filter state
- Connect filter definitions to ARRenderer
- Handle filter lifecycle and switching
- Apply blendshape behaviors in real-time

---

**Document Version**: 1.0  
**Created**: October 3, 2025  
**Status**: Phase 6 Ready to Start 🚀
