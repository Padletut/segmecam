// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Filter Attachment Test Demo - Phase 2 Step 5.5
// Demonstrates the filter attachment system with simple geometric primitives

#include "include/ar_filters/filter_test_demo.h"
#include "include/ar_filters/filter_object.h"
#include "include/ar_filters/attachment_controller.h"

#include <iostream>

namespace segmecam {

FilterTestDemo::FilterTestDemo()
    : test_active_(false),
      test_initialized_(false) {
}

FilterTestDemo::~FilterTestDemo() {
  Cleanup();
}

// Initialize test filters and attach them to anchor points
void FilterTestDemo::Initialize(AttachmentController& controller, bool& ar_filters_enabled) {
  if (test_initialized_) {
    std::cout << "[FilterTestDemo] Already initialized" << std::endl;
    return;
  }
  
  std::cout << "[FilterTestDemo] Initializing AR filter test demo..." << std::endl;
  
  // Enable AR filters
  ar_filters_enabled = true;
  
  // Create and attach test filters
  CreateTestFilters(controller);
  
  test_initialized_ = true;
  test_active_ = true;
  
  std::cout << "[FilterTestDemo] Test demo initialized with " 
            << controller.GetFilterCount() 
            << " filters" << std::endl;
}

// Create various test filters attached to different anchor points
void FilterTestDemo::CreateTestFilters(AttachmentController& controller) {
  // Test 1: Small red cube on nose bridge (central reference point)
  FilterObject nose_cube = CreateCube(
      "nose_bridge",           // Anchor point
      "Nose Cube",             // Name
      20.0f                    // Size (pixels)
  );
  nose_cube.color = {1.0f, 0.0f, 0.0f};  // Red
  nose_cube.alpha = 0.8f;
  nose_cube.offset = {0.0f, 0.0f, 0.0f};  // Centered on anchor
  nose_cube.local_scale = 1.0f;
  filter_ids_.push_back(controller.AttachFilter(nose_cube));
  
  // Test 2: Green sphere on forehead (above eyes)
  FilterObject forehead_sphere = CreateSphere(
      "forehead",
      "Forehead Sphere",
      25.0f,                   // Radius
      16                       // Segments (smoothness)
  );
  forehead_sphere.color = {0.0f, 1.0f, 0.0f};  // Green
  forehead_sphere.alpha = 0.7f;
  forehead_sphere.offset = {0.0f, -15.0f, 0.0f};  // Slightly above anchor
  forehead_sphere.local_scale = 1.2f;
  filter_ids_.push_back(controller.AttachFilter(forehead_sphere));
  
  // Test 3: Blue cylinder on chin (bottom of face)
  FilterObject chin_cylinder = CreateCylinder(
      "chin",
      "Chin Cylinder",
      15.0f,                   // Radius
      40.0f,                   // Height
      12                       // Segments
  );
  chin_cylinder.color = {0.0f, 0.5f, 1.0f};  // Light blue
  chin_cylinder.alpha = 0.75f;
  chin_cylinder.offset = {0.0f, 10.0f, 0.0f};  // Below anchor
  chin_cylinder.local_scale = 0.8f;
  filter_ids_.push_back(controller.AttachFilter(chin_cylinder));
  
  // Test 4: Yellow cone on left cheek
  FilterObject left_cheek_cone = CreateCone(
      "left_cheek",
      "Left Cheek Cone",
      18.0f,                   // Base radius
      35.0f,                   // Height
      12                       // Segments
  );
  left_cheek_cone.color = {1.0f, 1.0f, 0.0f};  // Yellow
  left_cheek_cone.alpha = 0.8f;
  left_cheek_cone.offset = {-10.0f, 0.0f, 0.0f};  // To the left
  left_cheek_cone.local_scale = 0.9f;
  filter_ids_.push_back(controller.AttachFilter(left_cheek_cone));
  
  // Test 5: Magenta cube on right cheek (for symmetry testing)
  FilterObject right_cheek_cube = CreateCube(
      "right_cheek",
      "Right Cheek Cube",
      18.0f
  );
  right_cheek_cube.color = {1.0f, 0.0f, 1.0f};  // Magenta
  right_cheek_cube.alpha = 0.8f;
  right_cheek_cube.offset = {10.0f, 0.0f, 0.0f};  // To the right
  right_cheek_cube.local_scale = 0.9f;
  filter_ids_.push_back(controller.AttachFilter(right_cheek_cube));
  
  // Test 6: Cyan sphere on left eye (small, subtle)
  FilterObject left_eye_sphere = CreateSphere(
      "left_eye",
      "Left Eye Marker",
      10.0f,
      8
  );
  left_eye_sphere.color = {0.0f, 1.0f, 1.0f};  // Cyan
  left_eye_sphere.alpha = 0.6f;
  left_eye_sphere.offset = {0.0f, 0.0f, 0.0f};
  left_eye_sphere.local_scale = 1.0f;
  filter_ids_.push_back(controller.AttachFilter(left_eye_sphere));
  
  // Test 7: Orange sphere on right eye (small, subtle)
  FilterObject right_eye_sphere = CreateSphere(
      "right_eye",
      "Right Eye Marker",
      10.0f,
      8
  );
  right_eye_sphere.color = {1.0f, 0.5f, 0.0f};  // Orange
  right_eye_sphere.alpha = 0.6f;
  right_eye_sphere.offset = {0.0f, 0.0f, 0.0f};
  right_eye_sphere.local_scale = 1.0f;
  filter_ids_.push_back(controller.AttachFilter(right_eye_sphere));
  
  std::cout << "[FilterTestDemo] Created " << filter_ids_.size() << " test filters:" << std::endl;
  std::cout << "  1. Red cube on nose_bridge (central)" << std::endl;
  std::cout << "  2. Green sphere on forehead (top)" << std::endl;
  std::cout << "  3. Blue cylinder on chin (bottom)" << std::endl;
  std::cout << "  4. Yellow cone on left_cheek" << std::endl;
  std::cout << "  5. Magenta cube on right_cheek" << std::endl;
  std::cout << "  6. Cyan sphere on left_eye" << std::endl;
  std::cout << "  7. Orange sphere on right_eye" << std::endl;
}

// Enable/disable test filters
void FilterTestDemo::SetActive(bool active, bool& ar_filters_enabled) {
  if (active == test_active_) return;
  
  test_active_ = active;
  ar_filters_enabled = active;
  
  std::cout << "[FilterTestDemo] Test " << (active ? "ENABLED" : "DISABLED") << std::endl;
}

// Remove all test filters
void FilterTestDemo::Cleanup() {
  if (!test_initialized_) return;
  
  std::cout << "[FilterTestDemo] Cleaning up test demo..." << std::endl;
  filter_ids_.clear();
  test_initialized_ = false;
  test_active_ = false;
}

// Get test statistics
void FilterTestDemo::PrintStatistics(const AttachmentController& controller) const {
  if (!test_initialized_) {
    std::cout << "[FilterTestDemo] Not initialized" << std::endl;
    return;
  }
  
  auto stats = controller.GetStatistics();
  
  std::cout << "\n=== Filter Test Demo Statistics ===" << std::endl;
  std::cout << "Test Active: " << (test_active_ ? "YES" : "NO") << std::endl;
  std::cout << "Filters Created: " << filter_ids_.size() << std::endl;
  std::cout << "Filters Total: " << stats.total_filters << std::endl;
  std::cout << "Filters Enabled: " << stats.enabled_filters << std::endl;
  std::cout << "Filters Visible: " << stats.visible_filters << std::endl;
  std::cout << "Valid Anchors: " << stats.filters_with_valid_anchors << std::endl;
  std::cout << "Avg Update: " << stats.average_update_time_ms << " ms" << std::endl;
  std::cout << "=================================\n" << std::endl;
}

}  // namespace segmecam
