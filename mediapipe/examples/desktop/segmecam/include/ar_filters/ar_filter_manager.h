// Copyright 2025 SegmeCam Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// AR Filter Manager - Main coordinator for AR filter system
// Manages filter lifecycle, behaviors, and integration with application

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "opencv2/core.hpp"
#include "glm/glm.hpp"

// Forward declarations
namespace segmecam {
namespace ar_filters {

class FilterAsset;
class ARRenderer;
struct FilterBehavior;

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
  std::string id;                    // Unique filter ID (from JSON)
  std::string directory_path;        // Actual directory path (for loading)
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
  float render_time_ms = 0.0f;              // Last render time in milliseconds
  float average_fps = 0.0f;                 // Average FPS with filter active
  int frames_rendered = 0;                  // Total frames rendered
  int models_rendered_per_frame = 0;        // Models rendered in last frame
  int triangles_per_frame = 0;              // Triangles rendered in last frame
  size_t gpu_memory_mb = 0;                 // GPU memory used (estimate)
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
  cv::Mat Render(const cv::Mat& input_frame);  // DEPRECATED Phase 8 Day 3 - causes 40ms freeze
  
  // Phase 8 Day 4: Direct GPU-to-GPU rendering (no CPU readback)
  absl::Status RenderToTexture(unsigned int video_texture_id, int width, int height);

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

  // Behavior system state
  struct BehaviorState {
    bool active = false;
    float intensity = 0.0f;
    uint64_t activation_time_us = 0;
    glm::vec3 shake_offset{0.0f};
    float scale_multiplier = 1.0f;
    bool hidden = false;
    glm::vec3 color_tint{1.0f, 1.0f, 1.0f};
  };

  // Internal methods
  absl::Status ScanFiltersDirectory();
  absl::Status LoadFilterAsset(const std::string& filter_id);
  void UpdateBehaviors(const std::vector<mediapipe::NormalizedLandmark>& landmarks);
  void UpdatePerformanceStats(uint64_t render_time_us);
  FilterInfo CreateFilterInfo(const FilterAsset& asset) const;
  
  // Behavior system methods
  float GetBlendshapeValue(const std::string& blendshape_name,
                           const std::vector<mediapipe::NormalizedLandmark>& landmarks);
  void ApplyShakeBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyScaleBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyHideBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyRotateBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyFallOffBehavior(const FilterBehavior& behavior, float blendshape_value);
  void ApplyColorChangeBehavior(const FilterBehavior& behavior, float blendshape_value);

  // Member variables
  ARFilterManagerConfig config_;
  ManagerState state_;
  
  // Component managers (created in Initialize)
  std::unique_ptr<ARRenderer> ar_renderer_;
  std::map<std::string, std::unique_ptr<FilterAsset>> loaded_filter_assets_;
  
  // Blendshape state for behaviors (updated each frame)
  std::map<std::string, float> blendshape_values_;
  std::map<std::string, BehaviorState> behavior_states_;  // behavior_id -> state
};

} // namespace ar_filters
} // namespace segmecam
