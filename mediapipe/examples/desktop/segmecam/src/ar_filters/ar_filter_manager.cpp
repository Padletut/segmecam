// Copyright 2025 SegmeCam Authors. All Rights Reserved.
//
// AR Filter Manager Implementation - Main coordinator for AR filter system

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_filter_manager.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_asset.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_renderer.h"

#include <filesystem>
#include <chrono>
#include <algorithm>
#include <random>
#include <cmath>
#include "absl/log/absl_log.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

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

  // Initialize ARRenderer with configuration
  ARRenderer::ARConfig renderer_config;
  renderer_config.render_width = 1920;
  renderer_config.render_height = 1080;
  renderer_config.use_multisampling = true;
  renderer_config.msaa_samples = 4;
  ar_renderer_ = std::make_unique<ARRenderer>(renderer_config);
  
  auto status = ar_renderer_->Initialize();
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

  // Convert protobuf landmarks to flat float vector [x0,y0,z0, x1,y1,z1, ...]
  std::vector<float> landmarks_flat;
  landmarks_flat.reserve(face_landmarks.size() * 3);
  for (const auto& landmark : face_landmarks) {
    landmarks_flat.push_back(landmark.x());
    landmarks_flat.push_back(landmark.y());
    landmarks_flat.push_back(landmark.z());
  }
  
  // Update face landmarks in renderer
  auto status = ar_renderer_->UpdateFaceLandmarks(landmarks_flat);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to update face landmarks: " << status.message();
  }

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
  auto result_or = ar_renderer_->RenderToTexture(
      input_frame.data, input_frame.cols, input_frame.rows);

  auto end_time = std::chrono::steady_clock::now();
  auto render_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time).count();

  // Update performance stats
  UpdatePerformanceStats(render_time_us);

  // Check if render succeeded
  if (!result_or.ok()) {
    LOG(WARNING) << "Filter render failed: " << result_or.status().message();
    return input_frame.clone();
  }

  const auto& result = result_or.value();
  if (!result.success) {
    LOG(WARNING) << "Filter render failed: " << result.error_message;
    return input_frame.clone();
  }

  // Update stats from render result
  state_.performance_stats.models_rendered_per_frame = result.models_rendered;
  state_.performance_stats.triangles_per_frame = result.triangles_rendered;

  // Convert output texture to cv::Mat (placeholder - needs GPU texture readback)
  // For now, return original frame as we need to implement texture-to-Mat conversion
  // TODO Phase 7 Day 3: Implement GPU texture readback for composited result
  LOG(WARNING) << "GPU texture to cv::Mat conversion not yet implemented";
  return input_frame.clone();
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

  // Enumerate all filter directories
  auto filter_paths = FilterAsset::EnumerateFilters(config_.filters_directory);
  
  LOG(INFO) << "Found " << filter_paths.size() << " filter definitions";

  for (const auto& filter_path : filter_paths) {
    // Load filter metadata (without loading full assets yet)
    auto asset_or = FilterAsset::LoadFromFile(filter_path);
    
    if (!asset_or.ok()) {
      LOG(WARNING) << "Failed to load filter from " << filter_path 
                   << ": " << asset_or.status().message();
      continue;
    }

    // Create FilterInfo for UI
    FilterInfo info = CreateFilterInfo(asset_or.value());
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
  
  // Load FilterAsset (returns StatusOr<FilterAsset>)
  auto asset_or = FilterAsset::LoadFromFile(filter_path);
  if (!asset_or.ok()) {
    return asset_or.status();
  }
  
  // Move asset to unique_ptr
  auto asset = std::make_unique<FilterAsset>(std::move(asset_or.value()));

  // Validate assets exist
  auto status = asset->ValidateAssets();
  if (!status.ok()) {
    return status;
  }

  loaded_filter_assets_[filter_id] = std::move(asset);
  return absl::OkStatus();
}

void ARFilterManager::UpdateBehaviors(
    const std::vector<mediapipe::NormalizedLandmark>& landmarks) {
  if (!HasActiveFilter() || landmarks.empty()) {
    return;
  }

  // Get current filter's behaviors
  auto it = loaded_filter_assets_.find(state_.active_filter_id);
  if (it == loaded_filter_assets_.end()) {
    return;
  }

  const auto& behaviors = it->second->GetBehaviors();
  if (behaviors.empty()) {
    return;
  }
  
  // Process each behavior
  for (const auto& behavior : behaviors) {
    // Get blendshape value (0.0 to 1.0)
    float blendshape_value = GetBlendshapeValue(behavior.blendshape_name, landmarks);
    
    // Store for debugging/monitoring
    blendshape_values_[behavior.blendshape_name] = blendshape_value;
    
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

// Behavior System Implementation

float ARFilterManager::GetBlendshapeValue(
    const std::string& blendshape_name,
    const std::vector<mediapipe::NormalizedLandmark>& landmarks) {
  
  // Map common blendshape names to landmark-based calculations
  // MediaPipe Face Mesh provides 468 landmarks
  // We approximate blendshapes using landmark geometry
  
  if (blendshape_name == "eyeBlinkLeft") {
    // Left eye vertical distance
    // Landmarks: 159 (upper eyelid), 145 (lower eyelid)
    if (landmarks.size() > 159) {
      float eye_height = std::abs(landmarks[159].y() - landmarks[145].y());
      // Normalize: fully open = 0.03, fully closed = 0.005
      return std::clamp(1.0f - ((eye_height - 0.005f) / 0.025f), 0.0f, 1.0f);
    }
  } else if (blendshape_name == "eyeBlinkRight") {
    // Right eye vertical distance
    // Landmarks: 386 (upper eyelid), 374 (lower eyelid)
    if (landmarks.size() > 386) {
      float eye_height = std::abs(landmarks[386].y() - landmarks[374].y());
      return std::clamp(1.0f - ((eye_height - 0.005f) / 0.025f), 0.0f, 1.0f);
    }
  } else if (blendshape_name == "jawOpen" || blendshape_name == "mouthOpen") {
    // Mouth vertical opening
    // Landmarks: 13 (upper lip center), 14 (lower lip center)
    if (landmarks.size() > 14) {
      float mouth_height = std::abs(landmarks[13].y() - landmarks[14].y());
      // Normalize: closed = 0.01, wide open = 0.15
      return std::clamp((mouth_height - 0.01f) / 0.14f, 0.0f, 1.0f);
    }
  } else if (blendshape_name == "mouthSmile") {
    // Mouth width expansion
    // Landmarks: 61 (left corner), 291 (right corner)
    if (landmarks.size() > 291) {
      float mouth_width = std::abs(landmarks[61].x() - landmarks[291].x());
      // Normalize: neutral = 0.15, smile = 0.25
      return std::clamp((mouth_width - 0.15f) / 0.10f, 0.0f, 1.0f);
    }
  } else if (blendshape_name == "browRaiserLeft") {
    // Left eyebrow height
    // Landmarks: 70 (eyebrow), 159 (eyelid)
    if (landmarks.size() > 159) {
      float brow_height = std::abs(landmarks[70].y() - landmarks[159].y());
      // Normalize: neutral = 0.02, raised = 0.04
      return std::clamp((brow_height - 0.02f) / 0.02f, 0.0f, 1.0f);
    }
  } else if (blendshape_name == "browRaiserRight") {
    // Right eyebrow height
    // Landmarks: 300 (eyebrow), 386 (eyelid)
    if (landmarks.size() > 386) {
      float brow_height = std::abs(landmarks[300].y() - landmarks[386].y());
      return std::clamp((brow_height - 0.02f) / 0.02f, 0.0f, 1.0f);
    }
  }
  
  return 0.0f;  // Unknown blendshape
}

void ARFilterManager::ApplyShakeBehavior(const FilterBehavior& behavior, 
                                         float blendshape_value) {
  if (blendshape_value < behavior.threshold) {
    return;
  }

  // Generate random shake offset based on intensity
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

  // Scale shake by blendshape value and intensity
  float shake_amount = (blendshape_value - behavior.threshold) * behavior.intensity * 0.01f;
  
  glm::vec3 shake_offset(
      dis(gen) * shake_amount,
      dis(gen) * shake_amount,
      dis(gen) * shake_amount
  );

  // Update behavior state
  auto& state = behavior_states_[behavior.target_attachment_id];
  state.active = true;
  state.shake_offset = shake_offset;
  state.intensity = blendshape_value;

  // Apply shake to target attachment via ARRenderer
  // Note: This requires ARRenderer to support offset modification
  // For now, we log the shake event
  LOG(INFO) << "SHAKE behavior activated for " << behavior.target_attachment_id 
            << " with offset (" << shake_offset.x << ", " << shake_offset.y << ", " << shake_offset.z << ")";
}

void ARFilterManager::ApplyScaleBehavior(const FilterBehavior& behavior, 
                                         float blendshape_value) {
  if (blendshape_value < behavior.threshold) {
    return;
  }

  // Calculate scale multiplier based on blendshape value
  float scale_factor = 1.0f + (blendshape_value - behavior.threshold) * behavior.intensity;
  
  // Apply target scale if specified, otherwise proportional scaling
  glm::vec3 scale_vec = behavior.target_scale;
  if (glm::length(scale_vec - glm::vec3(1.0f)) < 0.001f) {
    // No target scale specified, use uniform scaling
    scale_vec = glm::vec3(scale_factor);
  } else {
    // Interpolate towards target scale
    scale_vec = glm::mix(glm::vec3(1.0f), scale_vec, blendshape_value * behavior.intensity);
  }

  // Update behavior state
  auto& state = behavior_states_[behavior.target_attachment_id];
  state.active = true;
  state.scale_multiplier = scale_factor;
  state.intensity = blendshape_value;

  LOG(INFO) << "SCALE behavior activated for " << behavior.target_attachment_id 
            << " with scale " << scale_factor;
}

void ARFilterManager::ApplyHideBehavior(const FilterBehavior& behavior, 
                                        float blendshape_value) {
  bool should_hide = blendshape_value >= behavior.threshold;
  
  // Update behavior state
  auto& state = behavior_states_[behavior.target_attachment_id];
  state.active = should_hide;
  state.hidden = should_hide;
  state.intensity = blendshape_value;

  if (should_hide) {
    LOG(INFO) << "HIDE behavior activated for " << behavior.target_attachment_id;
  }
}

void ARFilterManager::ApplyRotateBehavior(const FilterBehavior& behavior, 
                                          float blendshape_value) {
  if (blendshape_value < behavior.threshold) {
    return;
  }

  // Calculate rotation based on target rotation and blendshape value
  glm::vec3 rotation = behavior.target_rotation * blendshape_value * behavior.intensity;

  // Update behavior state
  auto& state = behavior_states_[behavior.target_attachment_id];
  state.active = true;
  state.intensity = blendshape_value;

  LOG(INFO) << "ROTATE behavior activated for " << behavior.target_attachment_id 
            << " with rotation (" << rotation.x << ", " << rotation.y << ", " << rotation.z << ")";
}

void ARFilterManager::ApplyFallOffBehavior(const FilterBehavior& behavior, 
                                           float blendshape_value) {
  if (blendshape_value < behavior.threshold) {
    return;
  }

  auto& state = behavior_states_[behavior.target_attachment_id];
  
  // Initialize fall-off if just triggered
  if (!state.active) {
    state.active = true;
    state.activation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    LOG(INFO) << "FALL_OFF behavior triggered for " << behavior.target_attachment_id;
  }

  // Calculate elapsed time since activation
  uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  float elapsed_seconds = (now_us - state.activation_time_us) / 1000000.0f;

  // Apply gravity: y = y0 + v0*t + 0.5*g*t^2
  float fall_distance = 0.5f * behavior.gravity * elapsed_seconds * elapsed_seconds;
  state.shake_offset = glm::vec3(0.0f, -fall_distance, 0.0f);
  state.intensity = blendshape_value;

  LOG(INFO) << "FALL_OFF physics: " << behavior.target_attachment_id 
            << " fell " << fall_distance << " units";
}

void ARFilterManager::ApplyColorChangeBehavior(const FilterBehavior& behavior, 
                                               float blendshape_value) {
  if (blendshape_value < behavior.threshold) {
    return;
  }

  // Interpolate color from white to target color
  glm::vec3 color = glm::mix(glm::vec3(1.0f), behavior.target_color, 
                             blendshape_value * behavior.intensity);

  // Update behavior state
  auto& state = behavior_states_[behavior.target_attachment_id];
  state.active = true;
  state.color_tint = color;
  state.intensity = blendshape_value;

  LOG(INFO) << "COLOR_CHANGE behavior activated for " << behavior.target_attachment_id 
            << " with color (" << color.r << ", " << color.g << ", " << color.b << ")";
}

} // namespace ar_filters
} // namespace segmecam
