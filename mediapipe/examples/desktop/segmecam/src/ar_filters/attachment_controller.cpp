// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Attachment Controller Implementation - Phase 2 Step 5
// Filter-to-anchor binding and transform management

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/attachment_controller.h"

#include <algorithm>
#include <chrono>

#include "mediapipe/examples/desktop/segmecam/third_party/glm/gtx/euler_angles.hpp"

namespace segmecam {

AttachmentController::AttachmentController()
    : global_scale_(1.0f),
      last_update_time_ms_(0.0f) {
}

AttachmentController::~AttachmentController() {
  DetachAll();
}

// Attach a filter to its anchor point
std::string AttachmentController::AttachFilter(const FilterObject& filter) {
  // Validate filter has an anchor name
  if (filter.anchor_name.empty()) {
    return "";  // Invalid filter - no anchor specified
  }
  
  // Validate filter has a valid ID (should be generated)
  if (filter.id.empty()) {
    return "";  // Invalid filter - no ID
  }
  
  // Check for duplicate ID
  if (filter_id_map_.find(filter.id) != filter_id_map_.end()) {
    return "";  // ID already exists
  }
  
  // Add filter to active list
  active_filters_.push_back(filter);
  size_t index = active_filters_.size() - 1;
  filter_id_map_[filter.id] = index;
  
  return filter.id;
}

// Detach a filter by ID
bool AttachmentController::DetachFilter(const std::string& filter_id) {
  auto it = filter_id_map_.find(filter_id);
  if (it == filter_id_map_.end()) {
    return false;  // Filter not found
  }
  
  size_t index = it->second;
  
  // Remove from active filters (swap with last and pop)
  if (index < active_filters_.size() - 1) {
    std::swap(active_filters_[index], active_filters_.back());
  }
  active_filters_.pop_back();
  
  // Rebuild ID map to fix indices
  RebuildIdMap();
  
  return true;
}

// Detach all filters
void AttachmentController::DetachAll() {
  active_filters_.clear();
  filter_id_map_.clear();
}

// Update all filter transforms
void AttachmentController::UpdateFilterTransforms(
    const std::vector<AnchorPoint>& anchors,
    const HeadPose& head_pose) {
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  for (auto& filter : active_filters_) {
    if (!filter.enabled) {
      continue;  // Skip disabled filters
    }
    
    // Find the anchor this filter is attached to
    const AnchorPoint* anchor = FindAnchor(anchors, filter.anchor_name);
    if (!anchor) {
      filter.visible = false;  // Hide filter if anchor not found
      continue;
    }
    
    // Calculate transform matrix
    glm::mat4 transform = CalculateFilterTransform(filter, *anchor, head_pose);
    
    // Extract position from transform
    filter.position = glm::vec3(transform[3]);
    
    // Extract rotation (approximate - assumes no scale/shear in rotation part)
    glm::mat3 rotation_matrix(transform);
    filter.rotation = glm::quat_cast(rotation_matrix);
    
    // Ensure filter is visible if anchor is valid
    filter.visible = filter.enabled;
    
    // Update frame counter
    filter.frame_count++;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  last_update_time_ms_ = std::chrono::duration<float, std::milli>(end_time - start_time).count();
}

// Get mutable filter by ID
FilterObject* AttachmentController::GetFilter(const std::string& filter_id) {
  auto it = filter_id_map_.find(filter_id);
  if (it == filter_id_map_.end()) {
    return nullptr;
  }
  return &active_filters_[it->second];
}

// Get const filter by ID
const FilterObject* AttachmentController::GetFilter(const std::string& filter_id) const {
  auto it = filter_id_map_.find(filter_id);
  if (it == filter_id_map_.end()) {
    return nullptr;
  }
  return &active_filters_[it->second];
}

// Enable/disable all filters
void AttachmentController::SetAllFiltersEnabled(bool enabled) {
  for (auto& filter : active_filters_) {
    filter.enabled = enabled;
  }
}

// Show/hide all filters
void AttachmentController::SetAllFiltersVisible(bool visible) {
  for (auto& filter : active_filters_) {
    filter.visible = visible;
  }
}

// Get statistics
AttachmentController::Statistics AttachmentController::GetStatistics() const {
  Statistics stats;
  stats.total_filters = active_filters_.size();
  stats.enabled_filters = 0;
  stats.visible_filters = 0;
  stats.filters_with_valid_anchors = 0;
  stats.average_update_time_ms = last_update_time_ms_;
  
  for (const auto& filter : active_filters_) {
    if (filter.enabled) stats.enabled_filters++;
    if (filter.visible) stats.visible_filters++;
    if (filter.visible && filter.enabled) stats.filters_with_valid_anchors++;
  }
  
  return stats;
}

// ===== Helper Methods =====

// Calculate filter transform from anchor and head pose
glm::mat4 AttachmentController::CalculateFilterTransform(
    const FilterObject& filter,
    const AnchorPoint& anchor,
    const HeadPose& head_pose) const {
  
  // Start with identity matrix
  glm::mat4 transform(1.0f);
  
  // For Phase 2, we use anchor's 2D position directly
  // Phase 3+ will use proper 3D projection with camera matrix
  
  // Convert anchor 2D position to 3D (using z=0 for now)
  glm::vec3 anchor_pos_3d(anchor.position_2d.x, anchor.position_2d.y, 0.0f);
  
  // Apply anchor position
  transform = glm::translate(transform, anchor_pos_3d);
  
  // Apply anchor rotation (if available)
  // For Phase 2, we'll use head pose rotation as anchor rotation
  transform = transform * glm::mat4_cast(head_pose.rotation);
  
  // Apply filter's local offset (in anchor's rotated space)
  if (glm::length(filter.offset) > 0.0f) {
    transform = glm::translate(transform, filter.offset);
  }
  
  // Apply filter's local rotation
  if (glm::length(filter.local_rotation_euler) > 0.0f) {
    glm::quat local_rot = CombineRotations(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 
                                           filter.local_rotation_euler);
    transform = transform * glm::mat4_cast(local_rot);
  }
  
  // Apply filter's scale
  glm::vec3 final_scale = filter.scale * filter.local_scale * global_scale_;
  transform = glm::scale(transform, final_scale);
  
  return transform;
}

// Find anchor by name
const AnchorPoint* AttachmentController::FindAnchor(
    const std::vector<AnchorPoint>& anchors,
    const std::string& anchor_name) const {
  
  for (const auto& anchor : anchors) {
    if (anchor.name == anchor_name) {
      return &anchor;
    }
  }
  return nullptr;
}

// Apply local offset in anchor's coordinate frame
glm::vec3 AttachmentController::ApplyLocalOffset(
    const glm::vec3& anchor_position,
    const glm::quat& anchor_rotation,
    const glm::vec3& local_offset) const {
  
  // Rotate offset by anchor's rotation, then add to anchor position
  glm::vec3 rotated_offset = anchor_rotation * local_offset;
  return anchor_position + rotated_offset;
}

// Combine rotations
glm::quat AttachmentController::CombineRotations(
    const glm::quat& anchor_rotation,
    const glm::vec3& local_euler_degrees) const {
  
  // Convert euler angles (degrees) to radians
  glm::vec3 euler_radians = glm::radians(local_euler_degrees);
  
  // Create quaternion from euler angles (order: YXZ)
  glm::quat local_quat = glm::quat(euler_radians);
  
  // Combine rotations (anchor rotation first, then local rotation)
  return anchor_rotation * local_quat;
}

// Rebuild ID map after vector modification
void AttachmentController::RebuildIdMap() {
  filter_id_map_.clear();
  for (size_t i = 0; i < active_filters_.size(); ++i) {
    filter_id_map_[active_filters_[i].id] = i;
  }
}

}  // namespace segmecam
