// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Attachment Controller - Phase 2 Step 5
// Manages filter-to-anchor bindings and coordinate transforms

#ifndef SEGMECAM_AR_FILTERS_ATTACHMENT_CONTROLLER_H_
#define SEGMECAM_AR_FILTERS_ATTACHMENT_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>
#include <opencv2/core.hpp>

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_object.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/transform_calculator.h"

namespace segmecam {

// AttachmentController manages the binding of filter objects to anchor points
// and updates their transforms each frame based on head pose and anchor positions.
//
// Usage:
//   AttachmentController controller;
//   
//   // Attach a filter to an anchor
//   FilterObject cube = CreateCube("nose_bridge");
//   std::string id = controller.AttachFilter(cube);
//   
//   // Each frame, update filter transforms
//   controller.UpdateFilterTransforms(anchor_points, head_pose);
//   
//   // Render filters
//   for (const auto& filter : controller.GetActiveFilters()) {
//     RenderFilter(filter);
//   }
class AttachmentController {
 public:
  AttachmentController();
  ~AttachmentController();
  
  // ===== Filter Management =====
  
  // Attach a filter to its specified anchor point
  // Returns: Unique filter ID for later reference
  // Note: Filter's anchor_name must match a valid anchor point
  std::string AttachFilter(const FilterObject& filter);
  
  // Detach a specific filter by ID
  // Returns: true if filter was found and removed
  bool DetachFilter(const std::string& filter_id);
  
  // Detach all active filters
  void DetachAll();
  
  // Get number of currently attached filters
  size_t GetFilterCount() const { return active_filters_.size(); }
  
  // ===== Transform Updates =====
  
  // Update all filter transforms based on current anchor positions and head pose
  // This should be called once per frame after anchor calculation
  // anchors: Current anchor points from TransformCalculator
  // head_pose: Current head pose from TransformCalculator
  void UpdateFilterTransforms(
      const std::vector<AnchorPoint>& anchors,
      const HeadPose& head_pose);
  
  // ===== Access =====
  
  // Get all currently attached filters (const reference)
  const std::vector<FilterObject>& GetActiveFilters() const {
    return active_filters_;
  }
  
  // Get a specific filter by ID (mutable pointer, returns nullptr if not found)
  FilterObject* GetFilter(const std::string& filter_id);
  
  // Get a specific filter by ID (const pointer, returns nullptr if not found)
  const FilterObject* GetFilter(const std::string& filter_id) const;
  
  // ===== Configuration =====
  
  // Set global scale multiplier for all filters
  // Useful for adjusting filter sizes based on camera distance
  void SetGlobalScale(float scale) { global_scale_ = scale; }
  float GetGlobalScale() const { return global_scale_; }
  
  // Enable/disable all filters
  void SetAllFiltersEnabled(bool enabled);
  
  // Enable/disable all filters visibility
  void SetAllFiltersVisible(bool visible);
  
  // ===== Statistics =====
  
  struct Statistics {
    size_t total_filters;
    size_t enabled_filters;
    size_t visible_filters;
    size_t filters_with_valid_anchors;
    float average_update_time_ms;
  };
  
  Statistics GetStatistics() const;
  
 private:
  // Active filters (indexed by insertion order)
  std::vector<FilterObject> active_filters_;
  
  // Map filter ID to index in active_filters_
  std::map<std::string, size_t> filter_id_map_;
  
  // Global configuration
  float global_scale_;
  
  // Performance tracking
  float last_update_time_ms_;
  
  // ===== Helper Methods =====
  
  // Find an anchor by name in the anchor list
  // Returns: Pointer to anchor if found, nullptr otherwise
  const AnchorPoint* FindAnchor(
      const std::vector<AnchorPoint>& anchors,
      const std::string& anchor_name) const;
  
  // Rebuild filter_id_map after modification
  void RebuildIdMap();
};

}  // namespace segmecam

#endif  // SEGMECAM_AR_FILTERS_ATTACHMENT_CONTROLLER_H_
