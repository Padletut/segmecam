# Phase 7: AR Filter Manager - Implementation Plan

**Status**: 🚀 **READY TO START**  
**Started**: TBD  
**Estimated Duration**: 2-3 days  
**Depends On**: Phase 6 ✅ Complete

---

## Executive Summary

Phase 7 creates the **AR Filter Manager** - the main coordinator that brings together all AR filter subsystems into a unified, application-ready component.

**Goal**: Build a high-level manager that:

- Loads and manages filter lifecycle (load/unload/switch)
- Coordinates FilterAsset → ARRenderer integration
- Applies blendshape-driven behaviors in real-time
- Handles filter categories and enumeration for UI
- Provides simple API for application integration
- Maintains performance (30+ FPS with active filter)

**Success Criteria**: Load a filter, see it render on face with behaviors active! 🎭✨

---

## What We Need to Implement

### Core Components

1. **ARFilterManager Class** - Main coordinator for AR filter system
2. **Filter Lifecycle Management** - Load, unload, switch between filters
3. **Behavior System** - Apply blendshape-driven behaviors (shake, scale, hide, etc.)
4. **Category Management** - Organize filters by type (glasses, hats, masks)
5. **Performance Monitoring** - Track render time, FPS impact
6. **Application Integration** - Simple API for main application

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Application                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │            ARFilterManager                           │  │
│  │  ┌────────────┐  ┌────────────┐  ┌──────────────┐  │  │
│  │  │ FilterAsset│  │ ARRenderer │  │   Behavior   │  │  │
│  │  │   Loader   │→ │ Integration│→ │   System     │  │  │
│  │  └────────────┘  └────────────┘  └──────────────┘  │  │
│  │         ↓               ↓                ↓          │  │
│  │  ┌────────────┐  ┌────────────┐  ┌──────────────┐  │  │
│  │  │  Category  │  │Performance │  │    State     │  │  │
│  │  │  Manager   │  │  Monitor   │  │  Management  │  │  │
│  │  └────────────┘  └────────────┘  └──────────────┘  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                            ↓
              ┌─────────────────────────────┐
              │   MediaPipe Face Landmarks   │
              │   (468-point face mesh)      │
              └─────────────────────────────┘
```

---

## Implementation Plan

### Day 1: ARFilterManager Class & Lifecycle Management

**Goal**: Create the main manager class with filter loading/unloading

#### Task 1.1: Define ARFilterManager Header

**File**: `include/ar_filters/ar_filter_manager.h`

**Class Structure**:

```cpp
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "opencv2/core.hpp"

// Forward declarations
namespace segmecam {
namespace ar_filters {

class FilterAsset;
class ARRenderer;

// Configuration for ARFilterManager
struct ARFilterManagerConfig {
  std::string filters_directory = "assets/filters";  // Root directory for filters
  bool enable_behaviors = true;                      // Enable blendshape behaviors
  bool enable_smoothing = true;                      // Smooth filter transforms
  float smoothing_factor = 0.3f;                     // Transform smoothing (0-1)
  int target_fps = 30;                               // Target FPS for performance monitoring
};

// Filter information for UI/enumeration
struct FilterInfo {
  std::string id;                    // Unique filter ID
  std::string name;                  // Display name
  std::string category;              // Category (glasses, hats, masks, etc.)
  std::string author;                // Filter author
  std::string description;           // Brief description
  std::string thumbnail_path;        // Path to thumbnail image
  std::string icon_path;             // Path to icon image
  int attachment_count;              // Number of 3D models
  bool has_behaviors;                // Has blendshape behaviors
};

// Performance statistics
struct FilterPerformanceStats {
  float render_time_ms;              // Last render time in milliseconds
  float average_fps;                 // Average FPS with filter active
  int frames_rendered;               // Total frames rendered
  int models_rendered_per_frame;     // Models rendered in last frame
  int triangles_per_frame;           // Triangles rendered in last frame
  size_t gpu_memory_mb;              // GPU memory used (estimate)
};

class ARFilterManager {
public:
  ARFilterManager();
  ~ARFilterManager();

  // Initialization and cleanup
  absl::Status Initialize(const ARFilterManagerConfig& config);
  void Cleanup();
  bool IsInitialized() const;

  // Filter discovery and enumeration
  absl::StatusOr<std::vector<FilterInfo>> GetAvailableFilters();
  absl::StatusOr<std::vector<std::string>> GetFilterCategories();
  absl::StatusOr<std::vector<FilterInfo>> GetFiltersByCategory(const std::string& category);
  absl::StatusOr<FilterInfo> GetFilterInfo(const std::string& filter_id);

  // Filter lifecycle management
  absl::Status LoadFilter(const std::string& filter_id);
  absl::Status UnloadCurrentFilter();
  absl::Status SwitchFilter(const std::string& filter_id);
  bool HasActiveFilter() const;
  std::string GetActiveFilterId() const;

  // Main update and render
  void Update(const std::vector<mediapipe::NormalizedLandmark>& face_landmarks,
              int frame_width, int frame_height);
  cv::Mat Render(const cv::Mat& input_frame);

  // Behavior control
  void SetBehaviorsEnabled(bool enabled);
  bool AreBehaviorsEnabled() const;
  void SetSmoothingFactor(float factor);  // 0.0 = no smoothing, 1.0 = max smoothing

  // Performance monitoring
  FilterPerformanceStats GetPerformanceStats() const;
  float GetLastRenderTimeMs() const;
  float GetAverageFPS() const;

  // Configuration
  void SetConfig(const ARFilterManagerConfig& config);
  ARFilterManagerConfig GetConfig() const;

private:
  // Internal state
  struct ManagerState {
    bool initialized = false;
    std::string active_filter_id;
    std::vector<FilterInfo> available_filters;
    std::map<std::string, std::string> category_map;  // filter_id -> category
    FilterPerformanceStats performance_stats;
    uint64_t last_update_time_us = 0;
    int frame_count = 0;
  };

  // Internal methods
  absl::Status ScanFiltersDirectory();
  absl::Status LoadFilterAsset(const std::string& filter_id);
  void UpdateBehaviors(const std::vector<mediapipe::NormalizedLandmark>& landmarks);
  void UpdatePerformanceStats(uint64_t render_time_us);
  FilterInfo CreateFilterInfo(const FilterAsset& asset) const;

  // Member variables
  ARFilterManagerConfig config_;
  ManagerState state_;
  
  // Component managers (created in Initialize)
  std::unique_ptr<ARRenderer> ar_renderer_;
  std::map<std::string, std::unique_ptr<FilterAsset>> loaded_filter_assets_;
  
  // Blendshape state for behaviors (updated each frame)
  std::map<std::string, float> blendshape_values_;
};

} // namespace ar_filters
} // namespace segmecam
```

#### Task 1.2: Implement ARFilterManager Core

**File**: `src/ar_filters/ar_filter_manager.cpp`

**Implementation Outline**:

```cpp
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_filter_manager.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_asset.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_renderer.h"

#include <filesystem>
#include <chrono>
#include "absl/log/absl_log.h"
#include "absl/strings/str_format.h"

namespace segmecam {
namespace ar_filters {

ARFilterManager::ARFilterManager() = default;
ARFilterManager::~ARFilterManager() {
  Cleanup();
}

absl::Status ARFilterManager::Initialize(const ARFilterManagerConfig& config) {
  if (state_.initialized) {
    return absl::FailedPreconditionError("ARFilterManager already initialized");
  }

  config_ = config;

  // Initialize ARRenderer
  ar_renderer_ = std::make_unique<ARRenderer>();
  ARRenderer::ARConfig renderer_config;
  renderer_config.offscreen_width = 1920;
  renderer_config.offscreen_height = 1080;
  renderer_config.enable_msaa = true;
  renderer_config.msaa_samples = 4;
  
  auto status = ar_renderer_->Initialize(renderer_config);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to initialize ARRenderer: " << status;
    return status;
  }

  // Scan filters directory
  status = ScanFiltersDirectory();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to scan filters directory: " << status;
    return status;
  }

  state_.initialized = true;
  LOG(INFO) << "ARFilterManager initialized with " << state_.available_filters.size() 
            << " filters";
  
  return absl::OkStatus();
}

void ARFilterManager::Cleanup() {
  if (!state_.initialized) {
    return;
  }

  // Unload current filter
  if (HasActiveFilter()) {
    auto status = UnloadCurrentFilter();
    if (!status.ok()) {
      LOG(WARNING) << "Failed to unload current filter: " << status;
    }
  }

  // Cleanup renderer
  if (ar_renderer_) {
    ar_renderer_->Cleanup();
    ar_renderer_.reset();
  }

  // Clear state
  loaded_filter_assets_.clear();
  state_.available_filters.clear();
  state_.category_map.clear();
  state_.initialized = false;

  LOG(INFO) << "ARFilterManager cleaned up";
}

bool ARFilterManager::IsInitialized() const {
  return state_.initialized;
}

// Filter discovery and enumeration
absl::StatusOr<std::vector<FilterInfo>> ARFilterManager::GetAvailableFilters() {
  if (!state_.initialized) {
    return absl::FailedPreconditionError("ARFilterManager not initialized");
  }
  return state_.available_filters;
}

absl::StatusOr<std::vector<std::string>> ARFilterManager::GetFilterCategories() {
  if (!state_.initialized) {
    return absl::FailedPreconditionError("ARFilterManager not initialized");
  }

  std::set<std::string> unique_categories;
  for (const auto& filter : state_.available_filters) {
    unique_categories.insert(filter.category);
  }

  return std::vector<std::string>(unique_categories.begin(), unique_categories.end());
}

absl::StatusOr<std::vector<FilterInfo>> ARFilterManager::GetFiltersByCategory(
    const std::string& category) {
  if (!state_.initialized) {
    return absl::FailedPreconditionError("ARFilterManager not initialized");
  }

  std::vector<FilterInfo> result;
  for (const auto& filter : state_.available_filters) {
    if (filter.category == category) {
      result.push_back(filter);
    }
  }

  return result;
}

absl::StatusOr<FilterInfo> ARFilterManager::GetFilterInfo(const std::string& filter_id) {
  if (!state_.initialized) {
    return absl::FailedPreconditionError("ARFilterManager not initialized");
  }

  for (const auto& filter : state_.available_filters) {
    if (filter.id == filter_id) {
      return filter;
    }
  }

  return absl::NotFoundError(absl::StrFormat("Filter not found: %s", filter_id));
}

// Filter lifecycle management
absl::Status ARFilterManager::LoadFilter(const std::string& filter_id) {
  if (!state_.initialized) {
    return absl::FailedPreconditionError("ARFilterManager not initialized");
  }

  // Unload current filter if any
  if (HasActiveFilter()) {
    auto status = UnloadCurrentFilter();
    if (!status.ok()) {
      return status;
    }
  }

  // Load filter asset
  auto status = LoadFilterAsset(filter_id);
  if (!status.ok()) {
    return status;
  }

  // Get the loaded asset
  auto it = loaded_filter_assets_.find(filter_id);
  if (it == loaded_filter_assets_.end()) {
    return absl::InternalError("Filter asset not found after loading");
  }

  // Load filter into ARRenderer
  status = ar_renderer_->LoadFilter(*it->second);
  if (!status.ok()) {
    loaded_filter_assets_.erase(it);
    return status;
  }

  // Update state
  state_.active_filter_id = filter_id;
  state_.frame_count = 0;
  state_.performance_stats = FilterPerformanceStats{};

  LOG(INFO) << "Filter loaded: " << filter_id;
  return absl::OkStatus();
}

absl::Status ARFilterManager::UnloadCurrentFilter() {
  if (!state_.initialized) {
    return absl::FailedPreconditionError("ARFilterManager not initialized");
  }

  if (!HasActiveFilter()) {
    return absl::FailedPreconditionError("No active filter to unload");
  }

  // Unload from renderer
  auto status = ar_renderer_->UnloadFilter(state_.active_filter_id);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to unload filter from renderer: " << status;
  }

  // Remove from loaded assets
  loaded_filter_assets_.erase(state_.active_filter_id);

  // Clear state
  std::string old_filter = state_.active_filter_id;
  state_.active_filter_id.clear();
  state_.performance_stats = FilterPerformanceStats{};

  LOG(INFO) << "Filter unloaded: " << old_filter;
  return absl::OkStatus();
}

absl::Status ARFilterManager::SwitchFilter(const std::string& filter_id) {
  // Simply load the new filter (LoadFilter handles unloading current)
  return LoadFilter(filter_id);
}

bool ARFilterManager::HasActiveFilter() const {
  return !state_.active_filter_id.empty();
}

std::string ARFilterManager::GetActiveFilterId() const {
  return state_.active_filter_id;
}

// Main update and render
void ARFilterManager::Update(const std::vector<mediapipe::NormalizedLandmark>& face_landmarks,
                              int frame_width, int frame_height) {
  if (!state_.initialized || !HasActiveFilter()) {
    return;
  }

  // Update face landmarks in renderer
  ar_renderer_->UpdateFaceLandmarks(face_landmarks);

  // Apply behaviors if enabled
  if (config_.enable_behaviors) {
    UpdateBehaviors(face_landmarks);
  }

  // Update timing
  auto now = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  state_.last_update_time_us = now;
}

cv::Mat ARFilterManager::Render(const cv::Mat& input_frame) {
  if (!state_.initialized || !HasActiveFilter()) {
    return input_frame.clone();
  }

  auto start_time = std::chrono::steady_clock::now();

  // Render filter using ARRenderer
  auto result = ar_renderer_->RenderToTexture(
      input_frame.data, input_frame.cols, input_frame.rows);

  auto end_time = std::chrono::steady_clock::now();
  auto render_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time).count();

  // Update performance stats
  UpdatePerformanceStats(render_time_us);

  if (!result.success) {
    LOG(WARNING) << "Filter render failed";
    return input_frame.clone();
  }

  // Return the composited result
  return result.composited_frame;
}

// Performance monitoring
FilterPerformanceStats ARFilterManager::GetPerformanceStats() const {
  return state_.performance_stats;
}

float ARFilterManager::GetLastRenderTimeMs() const {
  return state_.performance_stats.render_time_ms;
}

float ARFilterManager::GetAverageFPS() const {
  return state_.performance_stats.average_fps;
}

// Behavior control
void ARFilterManager::SetBehaviorsEnabled(bool enabled) {
  config_.enable_behaviors = enabled;
}

bool ARFilterManager::AreBehaviorsEnabled() const {
  return config_.enable_behaviors;
}

void ARFilterManager::SetSmoothingFactor(float factor) {
  config_.smoothing_factor = std::clamp(factor, 0.0f, 1.0f);
}

// Configuration
void ARFilterManager::SetConfig(const ARFilterManagerConfig& config) {
  config_ = config;
}

ARFilterManagerConfig ARFilterManager::GetConfig() const {
  return config_;
}

// Private methods
absl::Status ARFilterManager::ScanFiltersDirectory() {
  namespace fs = std::filesystem;

  if (!fs::exists(config_.filters_directory)) {
    return absl::NotFoundError(
        absl::StrFormat("Filters directory not found: %s", config_.filters_directory));
  }

  state_.available_filters.clear();
  state_.category_map.clear();

  // Enumerate all filter.json files
  auto filter_paths = FilterAsset::EnumerateFilters(config_.filters_directory);
  
  LOG(INFO) << "Found " << filter_paths.size() << " filter definitions";

  for (const auto& filter_path : filter_paths) {
    // Load filter metadata (without loading full assets yet)
    auto asset = std::make_unique<FilterAsset>();
    auto status = asset->LoadFromFile(filter_path);
    
    if (!status.ok()) {
      LOG(WARNING) << "Failed to load filter from " << filter_path << ": " << status;
      continue;
    }

    // Create FilterInfo for UI
    FilterInfo info = CreateFilterInfo(*asset);
    state_.available_filters.push_back(info);
    state_.category_map[info.id] = info.category;

    LOG(INFO) << "Discovered filter: " << info.name << " (" << info.id << ")";
  }

  return absl::OkStatus();
}

absl::Status ARFilterManager::LoadFilterAsset(const std::string& filter_id) {
  // Check if already loaded
  if (loaded_filter_assets_.find(filter_id) != loaded_filter_assets_.end()) {
    return absl::OkStatus();
  }

  // Find filter path
  std::string filter_path = config_.filters_directory + "/" + filter_id + "/filter.json";
  
  auto asset = std::make_unique<FilterAsset>();
  auto status = asset->LoadFromFile(filter_path);
  if (!status.ok()) {
    return status;
  }

  // Validate assets exist
  status = asset->ValidateAssets();
  if (!status.ok()) {
    return status;
  }

  loaded_filter_assets_[filter_id] = std::move(asset);
  return absl::OkStatus();
}

void ARFilterManager::UpdateBehaviors(
    const std::vector<mediapipe::NormalizedLandmark>& landmarks) {
  // TODO: Implement blendshape behavior system in Day 2
  // This will read blendshape values and apply behaviors from FilterAsset
}

void ARFilterManager::UpdatePerformanceStats(uint64_t render_time_us) {
  state_.frame_count++;
  
  state_.performance_stats.render_time_ms = render_time_us / 1000.0f;
  
  // Calculate rolling average FPS
  if (state_.frame_count > 1) {
    float new_fps = 1000000.0f / render_time_us;  // Convert microseconds to FPS
    float alpha = 0.1f;  // Smoothing factor
    state_.performance_stats.average_fps = 
        alpha * new_fps + (1.0f - alpha) * state_.performance_stats.average_fps;
  } else {
    state_.performance_stats.average_fps = 1000000.0f / render_time_us;
  }
  
  state_.performance_stats.frames_rendered = state_.frame_count;
}

FilterInfo ARFilterManager::CreateFilterInfo(const FilterAsset& asset) const {
  const auto& metadata = asset.GetMetadata();
  const auto& attachments = asset.GetAttachments();
  const auto& behaviors = asset.GetBehaviors();

  FilterInfo info;
  info.id = metadata.id;
  info.name = metadata.name;
  info.category = metadata.category;
  info.author = metadata.author;
  info.description = metadata.description;
  info.thumbnail_path = metadata.thumbnail_path;
  info.icon_path = metadata.icon_path;
  info.attachment_count = attachments.size();
  info.has_behaviors = !behaviors.empty();

  return info;
}

} // namespace ar_filters
} // namespace segmecam
```

#### Task 1.3: Add ARFilterManager to BUILD

**File**: `src/ar_filters/BUILD`

Add new target:

```python
# Phase 7: AR Filter Manager - Main coordinator
cc_library(
    name = "ar_filter_manager",
    srcs = ["ar_filter_manager.cpp"],
    hdrs = ["//mediapipe/examples/desktop/segmecam:include/ar_filters/ar_filter_manager.h"],
    includes = ["../.."],
    visibility = ["//visibility:public"],
    deps = [
        ":filter_asset",
        ":ar_renderer",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_core",
        "//mediapipe/framework/port:opencv_imgproc",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/strings:str_format",
    ],
    copts = [
        "-std=c++17",
        "-I/usr/include/opencv4",
    ],
)
```

#### Task 1.4: Create Basic Unit Tests

**File**: `src/ar_filters/ar_filter_manager_test.cpp`

**Test Cases**:

1. `TestManagerCreation` - Create and destroy manager
2. `TestInitialization` - Initialize with valid config
3. `TestFilterDiscovery` - Scan and discover filters
4. `TestGetFilterCategories` - Enumerate categories
5. `TestLoadFilter` - Load a sample filter
6. `TestUnloadFilter` - Unload active filter
7. `TestSwitchFilter` - Switch between filters
8. `TestPerformanceStats` - Performance monitoring works

---

### Day 2: Behavior System Implementation

**Goal**: Implement blendshape-driven behaviors (shake, scale, hide, etc.)

#### Task 2.1: Define Behavior System

**Add to ar_filter_manager.h**:

```cpp
private:
  // Behavior system
  struct BehaviorState {
    bool active = false;
    float intensity = 0.0f;
    uint64_t activation_time_us = 0;
    glm::vec3 shake_offset{0.0f};
    float scale_multiplier = 1.0f;
  };
  
  std::map<std::string, BehaviorState> behavior_states_;  // behavior_id -> state
  
  void ApplyShakeBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyScaleBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyHideBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyRotateBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyFallOffBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyColorChangeBehavior(const FilterBehavior& behavior, float blendshape_value);
```

#### Task 2.2: Implement UpdateBehaviors

**In ar_filter_manager.cpp**:

```cpp
void ARFilterManager::UpdateBehaviors(
    const std::vector<mediapipe::NormalizedLandmark>& landmarks) {
  
  if (!HasActiveFilter()) {
    return;
  }

  // Get current filter's behaviors
  auto it = loaded_filter_assets_.find(state_.active_filter_id);
  if (it == loaded_filter_assets_.end()) {
    return;
  }

  const auto& behaviors = it->second->GetBehaviors();
  
  // TODO: Extract blendshape values from landmarks
  // For now, we'll implement basic behaviors based on landmark positions
  
  for (const auto& behavior : behaviors) {
    // Get blendshape value (0.0 to 1.0)
    float blendshape_value = GetBlendshapeValue(behavior.blendshape_name, landmarks);
    
    // Apply behavior based on type
    switch (behavior.type) {
      case FilterBehavior::Type::SHAKE:
        ApplyShakeBehavior(behavior, blendshape_value);
        break;
      case FilterBehavior::Type::SCALE:
        ApplyScaleBehavior(behavior, blendshape_value);
        break;
      case FilterBehavior::Type::HIDE:
        ApplyHideBehavior(behavior, blendshape_value);
        break;
      case FilterBehavior::Type::ROTATE:
        ApplyRotateBehavior(behavior, blendshape_value);
        break;
      case FilterBehavior::Type::FALL_OFF:
        ApplyFallOffBehavior(behavior, blendshape_value);
        break;
      case FilterBehavior::Type::COLOR_CHANGE:
        ApplyColorChangeBehavior(behavior, blendshape_value);
        break;
    }
  }
}

float ARFilterManager::GetBlendshapeValue(
    const std::string& blendshape_name,
    const std::vector<mediapipe::NormalizedLandmark>& landmarks) {
  
  // Map common blendshape names to landmark-based calculations
  // This is a simplified version - real blendshapes would come from MediaPipe Face Mesh
  
  if (blendshape_name == "eyeBlinkLeft") {
    // Calculate based on eye landmarks
    // Simplified: measure distance between upper and lower eyelid
    // Landmarks for left eye: 159 (upper), 145 (lower)
    if (landmarks.size() > 159) {
      float eye_height = std::abs(landmarks[159].y() - landmarks[145].y());
      return std::clamp(1.0f - (eye_height / 0.03f), 0.0f, 1.0f);
    }
  } else if (blendshape_name == "eyeBlinkRight") {
    // Landmarks for right eye: 386 (upper), 374 (lower)
    if (landmarks.size() > 386) {
      float eye_height = std::abs(landmarks[386].y() - landmarks[374].y());
      return std::clamp(1.0f - (eye_height / 0.03f), 0.0f, 1.0f);
    }
  } else if (blendshape_name == "jawOpen") {
    // Landmarks: 13 (upper lip), 14 (lower lip)
    if (landmarks.size() > 14) {
      float mouth_height = std::abs(landmarks[13].y() - landmarks[14].y());
      return std::clamp(mouth_height / 0.1f, 0.0f, 1.0f);
    }
  } else if (blendshape_name == "mouthSmile") {
    // Calculate based on mouth corners
    // Landmarks: 61 (left corner), 291 (right corner), 0 (nose tip)
    if (landmarks.size() > 291) {
      float mouth_width = std::abs(landmarks[61].x() - landmarks[291].x());
      return std::clamp((mouth_width - 0.2f) / 0.1f, 0.0f, 1.0f);
    }
  }
  
  return 0.0f;  // Unknown blendshape
}

void ARFilterManager::ApplyShakeBehavior(const FilterBehavior& behavior, 
                                         float blendshape_value) {
  if (blendshape_value < behavior.threshold) {
    return;
  }

  // Generate random shake offset
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

  glm::vec3 shake_offset(
      dis(gen) * behavior.intensity,
      dis(gen) * behavior.intensity,
      dis(gen) * behavior.intensity
  );

  // Apply shake to target attachment via ARRenderer
  ar_renderer_->SetModelInstanceOffset(behavior.target_attachment_id, shake_offset);
}

// Similar implementations for other behavior types...
```

#### Task 2.3: Add Behavior Tests

**New test cases**:

1. `TestShakeBehavior` - Shake on eye blink
2. `TestScaleBehavior` - Scale with mouth open
3. `TestHideBehavior` - Hide on specific blendshape
4. `TestBehaviorThreshold` - Only activate above threshold

---

### Day 3: Application Integration & Polish

**Goal**: Integrate ARFilterManager into main application, add UI hooks

#### Task 3.1: Add to Application

**File**: `include/application/application.h`

```cpp
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_filter_manager.h"

class ApplicationRun {
  // ...existing code...
  
private:
  std::unique_ptr<ARFilterManager> ar_filter_manager_;
  
  absl::Status InitializeARFilterManager();
};
```

**File**: `src/application/application.cpp`

```cpp
absl::Status ApplicationRun::InitializeARFilterManager() {
  ar_filter_manager_ = std::make_unique<ARFilterManager>();
  
  ARFilterManagerConfig config;
  config.filters_directory = "assets/filters";
  config.enable_behaviors = true;
  config.enable_smoothing = true;
  config.smoothing_factor = 0.3f;
  config.target_fps = 30;
  
  auto status = ar_filter_manager_->Initialize(config);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to initialize ARFilterManager: " << status;
    return status;
  }
  
  LOG(INFO) << "ARFilterManager initialized";
  return absl::OkStatus();
}

// In main loop, after MediaPipe processing:
if (ar_filter_manager_ && ar_filter_manager_->HasActiveFilter()) {
  // Update with face landmarks
  ar_filter_manager_->Update(face_landmarks, frame.cols, frame.rows);
  
  // Render filter onto frame
  frame = ar_filter_manager_->Render(frame);
}
```

#### Task 3.2: Add UI Panel for Filter Selection

**File**: `include/ui/ui_panels.h`

Add new panel:

```cpp
class ARFilterPanel {
public:
  void Render(ARFilterManager* manager);
  
private:
  std::string selected_category_ = "all";
  std::string selected_filter_id_;
  bool show_performance_stats_ = true;
};
```

#### Task 3.3: Documentation & Examples

**Files to create**:

1. `docs/AR_FILTERS_USER_GUIDE.md` - User guide for AR filters
2. `examples/ar_filter_example.cpp` - Standalone example
3. Update `README.md` with AR filter features

---

## Testing Strategy

### Unit Tests

**Target**: 15+ test cases across 3 test files

1. **ar_filter_manager_test.cpp** (8 tests):
   - Manager lifecycle (create, init, cleanup)
   - Filter discovery and enumeration
   - Filter loading/unloading/switching
   - Performance monitoring
   - Configuration management

2. **ar_filter_manager_behavior_test.cpp** (5 tests):
   - Shake behavior on eye blink
   - Scale behavior on mouth open
   - Hide behavior on threshold
   - Behavior enable/disable
   - Multiple behaviors simultaneously

3. **ar_filter_manager_integration_test.cpp** (3 tests):
   - Full render pipeline with sample filter
   - Filter switching during rendering
   - Performance under stress (30+ FPS)

### Integration Testing

1. Load all 3 sample filters (glasses, hat, ears)
2. Switch between filters in real-time
3. Verify behaviors activate correctly
4. Measure performance (should maintain 30+ FPS)
5. Test with various head poses and movements

### Visual Validation

1. See glasses render on face with correct alignment
2. Glasses shake when blinking
3. Hat stays on head during movement
4. Cat ears follow head rotation

---

## Success Criteria

- ✅ ARFilterManager class compiles and links
- ✅ All 15+ unit tests pass
- ✅ Can discover and enumerate filters
- ✅ Can load/unload/switch filters
- ✅ Behaviors activate on blendshapes
- ✅ Performance maintains 30+ FPS
- ✅ Visual validation: filters render correctly
- ✅ Application integration complete
- ✅ All Codacy checks pass

---

## Performance Targets

- **Filter Loading**: < 100ms per filter
- **Filter Switching**: < 50ms
- **Render Time**: < 16ms per frame (for 60 FPS)
- **Memory**: < 50MB per active filter
- **FPS Impact**: < 10% degradation with filter active

---

## Dependencies

**Phase 6 Complete** (required):
- ✅ FilterAsset class
- ✅ ARRenderer LoadFilter/UnloadFilter API
- ✅ Sample filter definitions

**Phase 5 Complete** (required):
- ✅ ARRenderer rendering pipeline
- ✅ Face landmark integration
- ✅ Transform calculations

**Phase 4 Complete** (required):
- ✅ ModelLoader, TextureManager, FBOManager
- ✅ 3D rendering infrastructure

---

## Risk Assessment

### Low Risk
- Filter discovery and enumeration (straightforward filesystem ops)
- Basic lifecycle management (well-defined state machine)
- Performance monitoring (simple timing measurements)

### Medium Risk
- Behavior system complexity (multiple behavior types)
- Blendshape value extraction (landmark-based approximation)
- Integration with application (coordination with existing managers)

### High Risk
- None identified (all prerequisites complete from Phase 6)

---

## Timeline Estimate

**Optimistic**: 1 day (if behavior system is simple)  
**Realistic**: 2 days (comprehensive behavior implementation)  
**Pessimistic**: 3 days (if complex integration issues arise)

**Most Likely**: 2 days based on Phase 6 experience

---

## Next Steps After Phase 7

After Phase 7 completion, we'll have:
- ✅ Complete AR filter system ready for use
- ✅ 3 sample filters working with behaviors
- ✅ Simple API for application integration
- ✅ Performance monitoring and optimization

**Phase 8** will focus on:
- Advanced shader effects (post-processing, shadows)
- More filter examples (10+ filters)
- UI polish and filter library
- Performance optimization for lower-end hardware

---

**Document Version**: 1.0  
**Created**: October 3, 2025  
**Status**: 🚀 Ready to Start - Prerequisites Complete!
