// Copyright 2025 SegmeCam Authors. All Rights Reserved.
//
// AR Filter Manager Implementation - Main coordinator for AR filter system

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_filter_manager.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_asset.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_renderer.h"

#include <filesystem>
#include <chrono>
#include <algorithm>
#include "absl/log/absl_log.h"
#include "absl/log/log.h"
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
  // TODO: Implement blendshape behavior system in Day 2
  // This will read blendshape values and apply behaviors from FilterAsset
  (void)landmarks;  // Suppress unused parameter warning
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
