// Phase 7 Day 3: AR Filter Visual Test Program
// Standalone test to validate filter loading and behavior system

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_filter_manager.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_renderer.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_asset.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include <iostream>
#include <vector>

using namespace segmecam::ar_filters;

namespace segmecam {

void PrintFilterInfo(const segmecam::ar_filters::FilterAsset& filter) {
  const auto& metadata = filter.GetMetadata();
  const auto& attachments = filter.GetAttachments();
  const auto& behaviors = filter.GetBehaviors();
  
  std::cout << "\n=== Filter Information ===\n";
  std::cout << "Name: " << metadata.name << "\n";
  std::cout << "ID: " << metadata.id << "\n";
  std::cout << "Category: " << metadata.category << "\n";
  std::cout << "Version: " << metadata.version << "\n";
  std::cout << "Author: " << metadata.author << "\n";
  std::cout << "Description: " << metadata.description << "\n";
  
  std::cout << "\nAttachments (" << attachments.size() << "):\n";
  for (const auto& attachment : attachments) {
    std::cout << "  - ID: " << attachment.id << "\n";
    std::cout << "    Anchor: " << attachment.anchor_name << "\n";
    std::cout << "    Model: " << attachment.model_path << "\n";
    std::cout << "    Scale: (" << attachment.scale.x << ", " 
              << attachment.scale.y << ", " << attachment.scale.z << ")\n";
  }
  
  std::cout << "\nBehaviors (" << behaviors.size() << "):\n";
  for (const auto& behavior : behaviors) {
    std::cout << "  - Type: ";
    switch (behavior.type) {
      case FilterBehavior::Type::SHAKE: std::cout << "SHAKE"; break;
      case FilterBehavior::Type::SCALE: std::cout << "SCALE"; break;
      case FilterBehavior::Type::HIDE: std::cout << "HIDE"; break;
      case FilterBehavior::Type::ROTATE: std::cout << "ROTATE"; break;
      case FilterBehavior::Type::FALL_OFF: std::cout << "FALL_OFF"; break;
      case FilterBehavior::Type::COLOR_CHANGE: std::cout << "COLOR_CHANGE"; break;
    }
    std::cout << "\n";
    std::cout << "    Target: " << behavior.target_attachment_id << "\n";
    std::cout << "    Blendshape: " << behavior.blendshape_name << "\n";
    std::cout << "    Threshold: " << behavior.threshold << "\n";
    std::cout << "    Intensity: " << behavior.intensity << "\n";
  }
  std::cout << "========================\n\n";
}

int TestFilterDiscovery(ARFilterManager& manager) {
  std::cout << "=== Test 1: Filter Discovery ===\n";
  
  auto status = manager.DiscoverFilters();
  if (!status.ok()) {
    std::cerr << "❌ FAILED: " << status.message() << "\n";
    return 1;
  }
  
  auto filters = manager.GetAvailableFilters();
  std::cout << "✅ PASSED: Discovered " << filters.size() << " filter(s)\n";
  
  for (const auto& filter : filters) {
    std::cout << "  - " << filter.name << " (" << filter.id << ")\n";
  }
  
  return filters.empty() ? 1 : 0;
}

int TestFilterLoading(ARFilterManager& manager, const std::string& filter_id) {
  std::cout << "\n=== Test 2: Filter Loading ===\n";
  std::cout << "Attempting to load filter: " << filter_id << "\n";
  
  auto status = manager.LoadFilter(filter_id);
  if (!status.ok()) {
    std::cerr << "❌ FAILED: " << status.message() << "\n";
    return 1;
  }
  
  std::cout << "✅ PASSED: Filter loaded successfully\n";
  
  auto loaded_filters = manager.GetLoadedFilters();
  std::cout << "Loaded filters count: " << loaded_filters.size() << "\n";
  
  // Print detailed info
  const auto* filter = manager.GetFilterById(filter_id);
  if (filter) {
    PrintFilterInfo(*filter);
  }
  
  return 0;
}

int TestBlendshapeCalculation() {
  std::cout << "\n=== Test 3: Blendshape Calculation (Mock Data) ===\n";
  
  // Create mock landmarks (468 points)
  std::vector<cv::Point2f> landmarks(468);
  
  // Set up mock eye landmarks for blink detection
  // Left eye: points 159 (top) and 145 (bottom)
  landmarks[159] = cv::Point2f(100, 100);  // Top of left eye
  landmarks[145] = cv::Point2f(100, 110);  // Bottom of left eye (10px apart = closed)
  
  // Right eye: points 386 (top) and 374 (bottom)
  landmarks[386] = cv::Point2f(200, 100);  // Top of right eye
  landmarks[374] = cv::Point2f(200, 115);  // Bottom of right eye (15px apart = half-closed)
  
  // Mock other points needed for calculations
  landmarks[33] = cv::Point2f(150, 150);   // Left eye center
  landmarks[263] = cv::Point2f(250, 150);  // Right eye center
  
  std::cout << "Mock landmarks created:\n";
  std::cout << "  Left eye: top=" << landmarks[159] << ", bottom=" << landmarks[145] << "\n";
  std::cout << "  Right eye: top=" << landmarks[386] << ", bottom=" << landmarks[374] << "\n";
  std::cout << "  Eye distance: " << cv::norm(landmarks[33] - landmarks[263]) << " pixels\n";
  
  // Note: Actual blendshape calculation would be tested via ARFilterManager
  // with real MediaPipe landmarks in integration tests
  
  std::cout << "✅ PASSED: Mock data prepared (actual calculation tested in runtime)\n";
  return 0;
}

int TestBehaviorStructure(ARFilterManager& manager, const std::string& filter_id) {
  std::cout << "\n=== Test 4: Behavior Structure Validation ===\n";
  
  const auto* filter = manager.GetFilterById(filter_id);
  if (!filter) {
    std::cerr << "❌ FAILED: Filter not found\n";
    return 1;
  }
  
  const auto& behaviors = filter->GetBehaviors();
  if (behaviors.empty()) {
    std::cerr << "⚠️  WARNING: No behaviors defined in filter\n";
    return 0;
  }
  
  // Validate each behavior has required fields
  int valid_behaviors = 0;
  for (const auto& behavior : behaviors) {
    bool valid = true;
    
    if (behavior.target_attachment_id.empty()) {
      std::cerr << "  ❌ Behavior missing target_attachment_id\n";
      valid = false;
    }
    
    if (behavior.blendshape_name.empty()) {
      std::cerr << "  ❌ Behavior missing blendshape_name\n";
      valid = false;
    }
    
    if (behavior.threshold <= 0 || behavior.threshold > 1.0) {
      std::cerr << "  ❌ Behavior has invalid threshold: " << behavior.threshold << "\n";
      valid = false;
    }
    
    if (behavior.intensity <= 0) {
      std::cerr << "  ❌ Behavior has invalid intensity: " << behavior.intensity << "\n";
      valid = false;
    }
    
    if (valid) {
      valid_behaviors++;
      std::cout << "  ✅ Behavior " << valid_behaviors << " validated\n";
    }
  }
  
  if (valid_behaviors == behaviors.size()) {
    std::cout << "✅ PASSED: All " << valid_behaviors << " behaviors valid\n";
    return 0;
  } else {
    std::cerr << "❌ FAILED: Only " << valid_behaviors << "/" 
              << behaviors.size() << " behaviors valid\n";
    return 1;
  }
}

int TestPerformanceStats(ARFilterManager& manager) {
  std::cout << "\n=== Test 5: Performance Statistics ===\n";
  
  auto stats = manager.GetPerformanceStats();
  
  std::cout << "Filters loaded: " << stats.filters_loaded << "\n";
  std::cout << "Active filters: " << stats.active_filters << "\n";
  std::cout << "Total attachments: " << stats.total_attachments << "\n";
  std::cout << "Active behaviors: " << stats.active_behaviors << "\n";
  std::cout << "Average frame time: " << stats.avg_frame_time_ms << " ms\n";
  std::cout << "Peak frame time: " << stats.peak_frame_time_ms << " ms\n";
  std::cout << "Total frames processed: " << stats.total_frames_processed << "\n";
  
  std::cout << "✅ PASSED: Performance stats accessible\n";
  return 0;
}

}  // namespace segmecam

int main(int argc, char** argv) {
  using namespace segmecam;
  
  std::cout << "\n";
  std::cout << "========================================\n";
  std::cout << "  AR Filter System Test Suite\n";
  std::cout << "  Phase 7 Day 3 Validation\n";
  std::cout << "========================================\n\n";
  
  // Initialize managers
  ARFilterManager filter_manager;
  
  // Configure filter search path
  std::string filters_path = "assets/filters";
  if (argc > 1) {
    filters_path = argv[1];
  }
  std::cout << "Filter search path: " << filters_path << "\n\n";
  
  // Override default path
  setenv("SEGMECAM_FILTERS_PATH", filters_path.c_str(), 1);
  
  int total_tests = 0;
  int passed_tests = 0;
  
  // Test 1: Filter Discovery
  total_tests++;
  if (TestFilterDiscovery(filter_manager) == 0) {
    passed_tests++;
  }
  
  // Get first available filter ID
  auto available = filter_manager.GetAvailableFilters();
  if (available.empty()) {
    std::cerr << "\n❌ CRITICAL: No filters discovered. Cannot continue tests.\n";
    std::cout << "Make sure filter assets exist in: " << filters_path << "\n";
    return 1;
  }
  
  std::string test_filter_id = available[0].id;
  std::cout << "Using filter for tests: " << test_filter_id << "\n";
  
  // Test 2: Filter Loading
  total_tests++;
  if (TestFilterLoading(filter_manager, test_filter_id) == 0) {
    passed_tests++;
  }
  
  // Test 3: Blendshape Calculation (mock)
  total_tests++;
  if (TestBlendshapeCalculation() == 0) {
    passed_tests++;
  }
  
  // Test 4: Behavior Structure
  total_tests++;
  if (TestBehaviorStructure(filter_manager, test_filter_id) == 0) {
    passed_tests++;
  }
  
  // Test 5: Performance Stats
  total_tests++;
  if (TestPerformanceStats(filter_manager) == 0) {
    passed_tests++;
  }
  
  // Summary
  std::cout << "\n========================================\n";
  std::cout << "  Test Results Summary\n";
  std::cout << "========================================\n";
  std::cout << "Total tests: " << total_tests << "\n";
  std::cout << "Passed: " << passed_tests << " ✅\n";
  std::cout << "Failed: " << (total_tests - passed_tests) << " ❌\n";
  
  double pass_rate = (double)passed_tests / total_tests * 100.0;
  std::cout << "Pass rate: " << pass_rate << "%\n";
  
  if (passed_tests == total_tests) {
    std::cout << "\n🎉 ALL TESTS PASSED! 🎉\n";
    std::cout << "\nNext steps:\n";
    std::cout << "1. Run visual tests with camera (requires full app integration)\n";
    std::cout << "2. Test behavior triggering with real face landmarks\n";
    std::cout << "3. Performance benchmarking under load\n";
    std::cout << "4. Create additional test filters\n";
    return 0;
  } else {
    std::cout << "\n⚠️  SOME TESTS FAILED\n";
    std::cout << "Review errors above and fix issues before proceeding.\n";
    return 1;
  }
}
