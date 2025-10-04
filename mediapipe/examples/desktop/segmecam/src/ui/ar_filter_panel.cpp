// Copyright 2025 SegmeCam Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// AR Filter Panel - UI for filter selection and management (Phase 9)

#include "include/ui/ar_filter_panel.h"
#include "include/ar_filters/ar_filter_manager.h"
#include "include/ar_filters/transform_calculator.h"  // HeadPose
#include "mediapipe/framework/formats/landmark.pb.h"
#include "opencv2/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "absl/log/absl_log.h"
#include <epoxy/gl.h>  // OpenGL functions
#include <iostream>
#include <filesystem>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

namespace segmecam {

ARFilterPanel::ARFilterPanel(AppState& state, ar_filters::ARFilterManager& ar_mgr)
    : UIPanel("AR Filters"), state_(state), ar_mgr_(ar_mgr) {
}

ARFilterPanel::~ARFilterPanel() {
  Cleanup();
}

void ARFilterPanel::Initialize() {
  if (initialized_) return;
  
  ABSL_LOG(INFO) << "[ARFilterPanel] Initializing...";
  RefreshFilterList();
  
  // Restore previously active filter from saved state (Phase 9)
  if (!state_.ar_filters.active_filter_id.empty()) {
    ABSL_LOG(INFO) << "[ARFilterPanel] Restoring saved filter: " 
                   << state_.ar_filters.active_filter_id;
    SelectFilter(state_.ar_filters.active_filter_id);
  }
  
  initialized_ = true;
  ABSL_LOG(INFO) << "[ARFilterPanel] Initialized with " << available_filters_.size() << " filters";
}

void ARFilterPanel::Cleanup() {
  UnloadThumbnailTextures();
  available_filters_.clear();
  initialized_ = false;
}

void ARFilterPanel::Update() {
  // Sync active filter state with ARFilterManager
  std::string current_active = ar_mgr_.GetActiveFilterId();
  if (current_active != active_filter_id_) {
    active_filter_id_ = current_active;
  }
  
  // Refresh filter list if needed (e.g., after external changes)
  if (needs_refresh_) {
    RefreshFilterList();
    needs_refresh_ = false;
  }
}

void ARFilterPanel::Render() {
  if (!visible_) return;
  
  if (ImGui::CollapsingHeader("🎭 AR Filters", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderHeader();
    RenderFilterGrid();
    RenderFilterInfo();
    RenderPerformanceStats();
    RenderControls();
  }
}

// ============================================================================
// Main Rendering Sections
// ============================================================================

void ARFilterPanel::RenderHeader() {
  ImGui::Spacing();
  
  // Master enable toggle
  bool ar_enabled = state_.ar_filters_enabled;
  if (ImGui::Checkbox("Enable AR Filters", &ar_enabled)) {
    state_.ar_filters_enabled = ar_enabled;
    std::cout << "[ARFilterPanel] AR Filters " 
              << (ar_enabled ? "enabled" : "disabled") << std::endl;
  }
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Master switch for AR filter system");
  }
  
  ImGui::Spacing();
  ImGui::Separator();
}

void ARFilterPanel::RenderFilterGrid() {
  ImGui::Text("Available Filters (%zu)", available_filters_.size());
  
  if (available_filters_.empty()) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "⚠️  No filters found");
    ImGui::TextWrapped("Place filter definitions in assets/filters/");
    return;
  }
  
  ImGui::BeginChild("FilterGrid", ImVec2(0, 200), true);
  
  // "None" button (no thumbnail, just text)
  RenderNoFilterButton(active_filter_id_.empty());
  ImGui::SameLine();
  
  // Filter thumbnails (4 per row)
  int count = 1;  // Start at 1 ("None" is first)
  for (const auto& filter : available_filters_) {
    bool is_active = (filter.id == active_filter_id_);
    RenderFilterButton(filter, is_active);
    
    // New row every 4 items
    if (count % 4 != 0 && count < static_cast<int>(available_filters_.size())) {
      ImGui::SameLine();
    }
    count++;
  }
  
  ImGui::EndChild();
}

void ARFilterPanel::RenderFilterInfo() {
  if (active_filter_id_.empty()) {
    ImGui::Text("Status: No active filter");
    return;
  }
  
  // Find active filter info
  FilterUIInfo* active_filter = nullptr;
  for (auto& filter : available_filters_) {
    if (filter.id == active_filter_id_) {
      active_filter = &filter;
      break;
    }
  }
  
  if (!active_filter) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "⚠️  Active filter not found");
    return;
  }
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ Active Filter");
  ImGui::Indent();
  ImGui::Text("Name: %s", active_filter->name.c_str());
  ImGui::Text("Author: %s", active_filter->author.c_str());
  ImGui::Text("Category: %s", active_filter->category.c_str());
  
  if (!active_filter->description.empty()) {
    ImGui::Spacing();
    ImGui::TextWrapped("%s", active_filter->description.c_str());
  }
  ImGui::Unindent();
}

void ARFilterPanel::RenderPerformanceStats() {
  if (!ar_mgr_.HasActiveFilter()) return;
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("🔬 Performance");
  ImGui::Indent();
  
  auto perf = ar_mgr_.GetPerformanceStats();
  ImGui::Text("Render Time: %.3f ms", perf.render_time_ms);
  
  // Performance indicator
  if (perf.render_time_ms < 3.0f) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "(Excellent)");
  } else if (perf.render_time_ms < 5.0f) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "(Good)");
  } else {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "(Poor)");
    float fps_impact = (perf.render_time_ms / 16.67f) * 100.0f;
    ImGui::Text("FPS Impact: %.1f%%", fps_impact);
  }
  
  ImGui::Text("Models: %d, Triangles: %d", 
              perf.models_rendered_per_frame,
              perf.triangles_per_frame);
  
  if (perf.average_fps > 0.0f) {
    ImGui::Text("Avg FPS: %.1f", perf.average_fps);
  }
  
  ImGui::Unindent();
}

void ARFilterPanel::RenderControls() {
  ImGui::Spacing();
  ImGui::Separator();
  
  if (ImGui::Button("🔄 Refresh Filters")) {
    needs_refresh_ = true;
    std::cout << "[ARFilterPanel] Refreshing filter list..." << std::endl;
  }
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Scan for new filters and regenerate thumbnails if needed");
  }
  
  ImGui::SameLine();
  
  // Thumbnail generation button
  if (!thumbnails_generated_) {
    if (ImGui::Button("🎨 Generate Thumbnails")) {
      GenerateThumbnails();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Create 128x128 preview images for all filters");
    }
    ImGui::SameLine();
  } else {
    if (ImGui::Button("🔄 Regenerate Thumbnails")) {
      thumbnails_generated_ = false;  // Force regeneration
      GenerateThumbnails();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Recreate all thumbnail images");
    }
    ImGui::SameLine();
  }
  
  // Keyboard shortcuts hint
  ImGui::TextDisabled("Shortcuts: Ctrl+A (toggle), ESC (clear)");
  
  // Handle keyboard shortcuts
  if (ImGui::IsWindowFocused() || ImGui::IsWindowHovered()) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      ClearFilter();
    }
    
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
      state_.ar_filters_enabled = !state_.ar_filters_enabled;
    }
  }
}

// ============================================================================
// Filter Management
// ============================================================================

void ARFilterPanel::RefreshFilterList() {
  ABSL_LOG(INFO) << "[ARFilterPanel] Refreshing filter list...";
  
  // Unload existing thumbnails
  UnloadThumbnailTextures();
  available_filters_.clear();
  
  // Get available filters from ARFilterManager
  auto filters_result = ar_mgr_.GetAvailableFilters();
  if (!filters_result.ok()) {
    ABSL_LOG(ERROR) << "[ARFilterPanel] Failed to get filters: " 
                    << filters_result.status().message();
    return;
  }
  
  const auto& filters = *filters_result;
  ABSL_LOG(INFO) << "[ARFilterPanel] Found " << filters.size() << " filters";
  
  // Convert to FilterUIInfo and load thumbnails
  for (const auto& filter : filters) {
    FilterUIInfo ui_info;
    ui_info.id = filter.id;
    ui_info.name = filter.name;
    ui_info.author = filter.author;
    ui_info.category = filter.category;
    ui_info.description = filter.description;
    ui_info.thumbnail_path = filter.thumbnail_path;
    ui_info.is_available = true;
    
    // Load or generate thumbnail
    std::string thumbnail_path = "assets/filters/" + filter.id + "/thumbnail.png";
    if (std::filesystem::exists(thumbnail_path)) {
      // Load existing thumbnail
      ui_info.thumbnail_texture = LoadThumbnailTexture(thumbnail_path);
      ui_info.thumbnail_loaded = (ui_info.thumbnail_texture != 0);
      ui_info.thumbnail_path = thumbnail_path;
    } else {
      // Will generate thumbnail on first render
      ui_info.thumbnail_loaded = false;
    }
    
    available_filters_.push_back(ui_info);
  }
  
  // Sort by category, then name
  std::sort(available_filters_.begin(), available_filters_.end(),
            [](const FilterUIInfo& a, const FilterUIInfo& b) {
              if (a.category != b.category) return a.category < b.category;
              return a.name < b.name;
            });
  
  ABSL_LOG(INFO) << "[ARFilterPanel] Loaded " << available_filters_.size() 
                 << " filters successfully";
}

void ARFilterPanel::SelectFilter(const std::string& filter_id) {
  ABSL_LOG(INFO) << "[ARFilterPanel] Selecting filter: " << filter_id;
  
  // Load filter through ARFilterManager
  auto status = ar_mgr_.LoadFilter(filter_id);
  if (!status.ok()) {
    ABSL_LOG(ERROR) << "[ARFilterPanel] Failed to load filter: " 
                    << status.message();
    return;
  }
  
  // Update state
  active_filter_id_ = filter_id;
  state_.ar_filters_enabled = true;  // Auto-enable AR filters
  
  // Save selection to AppState for persistence (Phase 9)
  state_.ar_filters.active_filter_id = filter_id;
  state_.ar_filters.filters_enabled = true;
  
  std::cout << "[ARFilterPanel] ✅ Loaded filter: " << filter_id << std::endl;
}

void ARFilterPanel::ClearFilter() {
  if (active_filter_id_.empty()) return;
  
  ABSL_LOG(INFO) << "[ARFilterPanel] Clearing active filter";
  
  auto status = ar_mgr_.UnloadCurrentFilter();
  if (!status.ok()) {
    ABSL_LOG(ERROR) << "[ARFilterPanel] Failed to clear filter: " 
                    << status.message();
    return;
  }
  
  active_filter_id_.clear();
  
  // Save cleared state to AppState for persistence (Phase 9)
  state_.ar_filters.active_filter_id = "";
  
  std::cout << "[ARFilterPanel] Cleared active filter" << std::endl;
}

// ============================================================================
// Thumbnail Management
// ============================================================================

GLuint ARFilterPanel::LoadThumbnailTexture(const std::string& path) {
  cv::Mat image = cv::imread(path, cv::IMREAD_UNCHANGED);
  if (image.empty()) {
    ABSL_LOG(ERROR) << "[ARFilterPanel] Failed to load thumbnail: " << path;
    return 0;
  }
  
  // Convert BGR to RGB
  if (image.channels() == 3) {
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
  } else if (image.channels() == 4) {
    cv::cvtColor(image, image, cv::COLOR_BGRA2RGBA);
  }
  
  // Upload to GPU
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  
  GLenum format = (image.channels() == 4) ? GL_RGBA : GL_RGB;
  glTexImage2D(GL_TEXTURE_2D, 0, format, image.cols, image.rows, 
               0, format, GL_UNSIGNED_BYTE, image.data);
  
  ABSL_LOG(INFO) << "[ARFilterPanel] Loaded thumbnail texture: " << path 
                 << " (ID: " << texture_id << ")";
  
  return texture_id;
}

void ARFilterPanel::UnloadThumbnailTextures() {
  for (auto& filter : available_filters_) {
    if (filter.thumbnail_texture != 0) {
      glDeleteTextures(1, &filter.thumbnail_texture);
      filter.thumbnail_texture = 0;
      filter.thumbnail_loaded = false;
    }
  }
}

bool ARFilterPanel::GenerateThumbnail(FilterUIInfo& filter) {
  ABSL_LOG(INFO) << "[ARFilterPanel] Generating thumbnail for " << filter.id;
  
  // Step 1: Load the filter
  std::string previous_filter = ar_mgr_.GetActiveFilterId();
  auto load_status = ar_mgr_.LoadFilter(filter.id);
  if (!load_status.ok()) {
    ABSL_LOG(ERROR) << "[ARFilterPanel] Failed to load filter for thumbnail: " 
                    << load_status.message();
    return false;
  }
  
  // Step 2: Create thumbnail FBO (128x128)
  const int thumb_size = 128;
  GLuint fbo = 0, texture = 0, depth_rbo = 0;
  
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  
  // Create color attachment
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, thumb_size, thumb_size, 0, 
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  
  // Create depth attachment
  glGenRenderbuffers(1, &depth_rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, thumb_size, thumb_size);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);
  
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    ABSL_LOG(ERROR) << "[ARFilterPanel] FBO incomplete for thumbnail generation";
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
    glDeleteRenderbuffers(1, &depth_rbo);
    return false;
  }
  
  // Step 3: Render with neutral face pose
  glViewport(0, 0, thumb_size, thumb_size);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent background
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  // Get neutral landmarks and render
  auto neutral_landmarks = GetNeutralLandmarks();
  if (!neutral_landmarks.empty()) {
    // Create neutral head pose (identity quaternion, centered position)
    HeadPose neutral_pose;
    neutral_pose.rotation_quat = cv::Vec4f(1.0f, 0.0f, 0.0f, 0.0f);  // Identity (w=1, x=y=z=0)
    neutral_pose.position = cv::Vec3f(thumb_size / 2.0f, thumb_size / 2.0f, 0.0f);
    neutral_pose.scale = 1.0f;
    
    // Update ARFilterManager with neutral pose
    ar_mgr_.Update(neutral_landmarks, neutral_pose, thumb_size, thumb_size);
    
    // Render to our FBO
    auto render_status = ar_mgr_.RenderToTexture(texture, thumb_size, thumb_size);
    if (!render_status.ok()) {
      ABSL_LOG(WARNING) << "[ARFilterPanel] Failed to render thumbnail: " 
                        << render_status.message();
    }
  }
  
  // Step 4: Read pixels and save as PNG
  std::vector<uint8_t> pixels(thumb_size * thumb_size * 4);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(0, 0, thumb_size, thumb_size, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  
  // Convert to cv::Mat and flip (OpenGL Y is inverted)
  cv::Mat thumbnail_mat(thumb_size, thumb_size, CV_8UC4, pixels.data());
  cv::flip(thumbnail_mat, thumbnail_mat, 0);
  cv::cvtColor(thumbnail_mat, thumbnail_mat, cv::COLOR_RGBA2BGRA);
  
  // Ensure thumbnail directory exists
  std::string thumbnail_dir = "assets/filters/" + filter.id;
  filter.thumbnail_path = thumbnail_dir + "/thumbnail.png";
  
  // Save PNG
  bool saved = cv::imwrite(filter.thumbnail_path, thumbnail_mat);
  if (!saved) {
    ABSL_LOG(WARNING) << "[ARFilterPanel] Failed to save thumbnail: " 
                      << filter.thumbnail_path;
  }
  
  // Step 5: Load as OpenGL texture
  if (saved) {
    filter.thumbnail_texture = LoadThumbnailTexture(filter.thumbnail_path);
    filter.thumbnail_loaded = (filter.thumbnail_texture != 0);
    
    if (filter.thumbnail_loaded) {
      ABSL_LOG(INFO) << "[ARFilterPanel] ✅ Generated thumbnail: " << filter.id;
    }
  }
  
  // Cleanup FBO
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo);
  glDeleteRenderbuffers(1, &depth_rbo);
  // Don't delete texture - it's our thumbnail!
  
  // Restore previous filter
  if (!previous_filter.empty()) {
    ar_mgr_.LoadFilter(previous_filter);
  } else {
    ar_mgr_.UnloadCurrentFilter();
  }
  
  return filter.thumbnail_loaded;
}

void ARFilterPanel::GenerateThumbnails() {
  if (thumbnails_generated_) {
    ABSL_LOG(INFO) << "[ARFilterPanel] Thumbnails already generated";
    return;
  }
  
  ABSL_LOG(INFO) << "[ARFilterPanel] Generating thumbnails for " 
                 << available_filters_.size() << " filters...";
  
  int success_count = 0;
  thumbnail_generation_progress_ = 0;
  
  for (auto& filter : available_filters_) {
    // Skip if thumbnail already exists
    if (filter.thumbnail_loaded && filter.thumbnail_texture != 0) {
      success_count++;
      thumbnail_generation_progress_++;
      continue;
    }
    
    // Generate thumbnail
    bool success = GenerateThumbnail(filter);
    if (success) {
      success_count++;
    } else {
      ABSL_LOG(WARNING) << "[ARFilterPanel] Failed to generate thumbnail for " 
                        << filter.id;
    }
    
    thumbnail_generation_progress_++;
    
    // Log progress every 5 filters
    if (thumbnail_generation_progress_ % 5 == 0 || 
        thumbnail_generation_progress_ == available_filters_.size()) {
      ABSL_LOG(INFO) << "[ARFilterPanel] Thumbnail progress: " 
                     << thumbnail_generation_progress_ << "/" 
                     << available_filters_.size();
    }
  }
  
  thumbnails_generated_ = true;
  
  ABSL_LOG(INFO) << "[ARFilterPanel] ✅ Thumbnail generation complete: " 
                 << success_count << "/" << available_filters_.size() 
                 << " successful";
}

std::vector<mediapipe::NormalizedLandmark> ARFilterPanel::GetNeutralLandmarks() {
  // Create neutral forward-facing face landmarks (468 points)
  // Simplified version - just key points for AR filter positioning
  std::vector<mediapipe::NormalizedLandmark> landmarks(468);
  
  // Center point (nose tip - landmark 1)
  landmarks[1].set_x(0.5f);
  landmarks[1].set_y(0.5f);
  landmarks[1].set_z(0.0f);
  
  // Nose bridge (landmarks 6-9)
  for (int i = 6; i <= 9; i++) {
    landmarks[i].set_x(0.5f);
    landmarks[i].set_y(0.45f + (i - 6) * 0.02f);
    landmarks[i].set_z(-0.01f);
  }
  
  // Left eye (landmarks 33, 133, 159, 145)
  landmarks[33].set_x(0.40f);
  landmarks[33].set_y(0.40f);
  landmarks[33].set_z(-0.01f);
  
  landmarks[133].set_x(0.45f);
  landmarks[133].set_y(0.40f);
  landmarks[133].set_z(-0.01f);
  
  // Right eye (landmarks 263, 362, 386, 374)
  landmarks[263].set_x(0.60f);
  landmarks[263].set_y(0.40f);
  landmarks[263].set_z(-0.01f);
  
  landmarks[362].set_x(0.55f);
  landmarks[362].set_y(0.40f);
  landmarks[362].set_z(-0.01f);
  
  // Mouth (landmarks 0, 17, 61, 291)
  landmarks[0].set_x(0.5f);
  landmarks[0].set_y(0.65f);
  landmarks[0].set_z(-0.02f);
  
  landmarks[61].set_x(0.45f);
  landmarks[61].set_y(0.65f);
  landmarks[61].set_z(-0.02f);
  
  landmarks[291].set_x(0.55f);
  landmarks[291].set_y(0.65f);
  landmarks[291].set_z(-0.02f);
  
  // Forehead (landmarks 10, 151)
  landmarks[10].set_x(0.5f);
  landmarks[10].set_y(0.25f);
  landmarks[10].set_z(-0.03f);
  
  landmarks[151].set_x(0.5f);
  landmarks[151].set_y(0.30f);
  landmarks[151].set_z(-0.02f);
  
  // Chin (landmark 152)
  landmarks[152].set_x(0.5f);
  landmarks[152].set_y(0.75f);
  landmarks[152].set_z(-0.01f);
  
  // Left face contour (landmarks 234, 127)
  landmarks[234].set_x(0.35f);
  landmarks[234].set_y(0.50f);
  landmarks[234].set_z(0.0f);
  
  landmarks[127].set_x(0.30f);
  landmarks[127].set_y(0.60f);
  landmarks[127].set_z(0.0f);
  
  // Right face contour (landmarks 454, 356)
  landmarks[454].set_x(0.65f);
  landmarks[454].set_y(0.50f);
  landmarks[454].set_z(0.0f);
  
  landmarks[356].set_x(0.70f);
  landmarks[356].set_y(0.60f);
  landmarks[356].set_z(0.0f);
  
  // Fill remaining landmarks with interpolated values
  // (simplified - just copy nearest key landmark)
  for (int i = 0; i < 468; i++) {
    if (landmarks[i].x() == 0.0f && landmarks[i].y() == 0.0f) {
      // Default to center point
      landmarks[i].set_x(0.5f);
      landmarks[i].set_y(0.5f);
      landmarks[i].set_z(0.0f);
    }
  }
  
  ABSL_LOG(INFO) << "[ARFilterPanel] Created neutral face landmarks (468 points)";
  return landmarks;
}

// ============================================================================
// UI Helpers
// ============================================================================

bool ARFilterPanel::RenderFilterButton(const FilterUIInfo& filter, bool is_active) {
  ImGui::BeginGroup();
  
  // Highlight if active
  if (is_active) {
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
  }
  
  bool clicked = false;
  
  // Render thumbnail or fallback
  if (filter.thumbnail_texture != 0) {
    // Use ImGui::Image + InvisibleButton for compatibility
    ImGui::PushID(filter.id.c_str());  // Unique ID for button
    
    ImVec2 button_pos = ImGui::GetCursorScreenPos();
    ImGui::Image(
        (void*)(intptr_t)filter.thumbnail_texture,
        ImVec2(64, 64)
    );
    
    ImGui::SetCursorScreenPos(button_pos);
    clicked = ImGui::InvisibleButton("##thumb", ImVec2(64, 64));
    
    ImGui::PopID();
  } else {
    // Fallback: Text button with category emoji
    std::string emoji = "📦";
    if (filter.category == "glasses") emoji = "👓";
    else if (filter.category == "hats") emoji = "🎩";
    else if (filter.category == "masks") emoji = "😷";
    else if (filter.category == "fun") emoji = "😺";
    
    clicked = ImGui::Button((emoji + "\n" + filter.name).c_str(), ImVec2(64, 64));
  }
  
  if (is_active) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
  }
  
  // Filter name below button (truncated if needed)
  std::string display_name = filter.name;
  if (display_name.length() > 12) {
    display_name = display_name.substr(0, 10) + "..";
  }
  ImGui::Text("%s", display_name.c_str());
  
  // Tooltip on hover
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("%s", filter.name.c_str());
    ImGui::Separator();
    ImGui::Text("Author: %s", filter.author.c_str());
    ImGui::Text("Category: %s", filter.category.c_str());
    if (!filter.description.empty()) {
      ImGui::Spacing();
      ImGui::TextWrapped("%s", filter.description.c_str());
    }
    ImGui::EndTooltip();
  }
  
  ImGui::EndGroup();
  
  // Handle click
  if (clicked) {
    SelectFilter(filter.id);
  }
  
  return clicked;
}

void ARFilterPanel::RenderNoFilterButton(bool is_active) {
  // Highlight if "None" is active
  if (is_active) {
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
  }
  
  bool clicked = ImGui::Button("None\n(Clear)", ImVec2(64, 64));
  
  if (is_active) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
  }
  
  ImGui::Text("No Filter");
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Clear active filter");
  }
  
  if (clicked) {
    ClearFilter();
  }
}

} // namespace segmecam
