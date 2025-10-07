// Copyright 2025 SegmeCam Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// AR Filter Panel - UI for filter selection and management (Phase 9)

#pragma once

#include "include/ui/ui_panel_base.h"
#include "include/application/app_state.h"
#include "include/ar_filters/ar_filter_manager.h"
#include "imgui.h"
#include <epoxy/gl.h>  // OpenGL types (GLuint)
#include <memory>
#include <vector>
#include <string>
#include <map>

namespace segmecam {

// Forward declaration
namespace ar_filters {
  class ARFilterManager;
}

// Filter information with UI-specific data
struct FilterUIInfo {
  std::string id;                    // Filter ID
  std::string name;                  // Display name
  std::string author;                // Author
  std::string category;              // Category
  std::string description;           // Description
  std::string thumbnail_path;        // Path to thumbnail PNG
  GLuint thumbnail_texture = 0;      // OpenGL texture ID
  bool thumbnail_loaded = false;     // Whether thumbnail is loaded
  bool is_available = true;          // Whether filter can be loaded
};

// AR Filter Panel - User interface for AR filter selection and management
class ARFilterPanel : public UIPanel {
public:
  ARFilterPanel(AppState& state, ar_filters::ARFilterManager& ar_mgr);
  ~ARFilterPanel() override;

  void Render() override;
  
  // Lifecycle
  void Initialize();
  void Update();  // Called each frame to refresh state
  void Cleanup();

private:
  // Main rendering sections
  void RenderHeader();
  void RenderFilterGrid();
  void RenderFilterInfo();
  void RenderPerformanceStats();
  void RenderControls();

  // Filter management
  void RefreshFilterList();
  void SelectFilter(const std::string& filter_id);
  void ClearFilter();
  
  // Thumbnail management
  void GenerateThumbnails();
  GLuint LoadThumbnailTexture(const std::string& path);
  void UnloadThumbnailTextures();
  bool GenerateThumbnail(FilterUIInfo& filter);
  
  // UI helpers
  bool RenderFilterButton(const FilterUIInfo& filter, bool is_active);
  void RenderNoFilterButton(bool is_active);
  std::vector<mediapipe::NormalizedLandmark> GetNeutralLandmarks();

  // State
  AppState& state_;
  ar_filters::ARFilterManager& ar_mgr_;
  
  // Filter list
  std::vector<FilterUIInfo> available_filters_;
  std::string active_filter_id_;
  bool needs_refresh_ = true;
  bool initialized_ = false;
  
  // Thumbnail generation
  bool thumbnails_generated_ = false;
  int thumbnail_generation_progress_ = 0;
};

} // namespace segmecam
