// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Filter Attachment Test Demo Header - Phase 2 Step 5.5
// Declares test demo class for filter attachment system

#ifndef SEGMECAM_AR_FILTERS_FILTER_TEST_DEMO_H_
#define SEGMECAM_AR_FILTERS_FILTER_TEST_DEMO_H_

#include <string>
#include <vector>

namespace segmecam {

// Forward declaration
class AttachmentController;

// Test demo class for demonstrating filter attachment system
// Creates 7 test filters attached to different anchor points:
//   1. Red cube on nose_bridge (central reference)
//   2. Green sphere on forehead (top of face)
//   3. Blue cylinder on chin (bottom of face)
//   4. Yellow cone on left_cheek
//   5. Magenta cube on right_cheek (symmetry test)
//   6. Cyan sphere on left_eye (small marker)
//   7. Orange sphere on right_eye (small marker)
class FilterTestDemo {
 public:
  FilterTestDemo();
  ~FilterTestDemo();
  
  // Initialize test filters and attach to anchors
  void Initialize(AttachmentController& controller, bool& ar_filters_enabled);
  
  // Enable or disable test filters
  void SetActive(bool active, bool& ar_filters_enabled);
  
  // Check if test is active
  bool IsActive() const { return test_active_; }
  
  // Check if test is initialized
  bool IsInitialized() const { return test_initialized_; }
  
  // Get number of test filters
  size_t GetFilterCount() const { return filter_ids_.size(); }
  
  // Print statistics about test filters
  void PrintStatistics(const AttachmentController& controller) const;
  
  // Remove all test filters
  void Cleanup();
  
 private:
  // Create and attach test filters
  void CreateTestFilters(AttachmentController& controller);
  
  // Filter IDs for cleanup
  std::vector<std::string> filter_ids_;
  
  // Test state
  bool test_active_;
  bool test_initialized_;
};

}  // namespace segmecam

#endif  // SEGMECAM_AR_FILTERS_FILTER_TEST_DEMO_H_
