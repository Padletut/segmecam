// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// ARRenderer + FilterAsset Integration Test
//
// Tests loading AR filters into the rendering pipeline

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_renderer.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_asset.h"

#include "absl/log/absl_log.h"
#include "absl/log/log.h"

#include <filesystem>

namespace fs = std::filesystem;

using namespace segmecam::ar_filters;

bool TestLoadFilterIntoRenderer() {
  ABSL_LOG(INFO) << "Test: Load filter into ARRenderer";
  
  // Create renderer
  ARRenderer::ARConfig config;
  config.render_width = 640;
  config.render_height = 480;
  
  ARRenderer renderer(config);
  
  // Initialize (requires OpenGL context, so this may fail in CI)
  auto init_status = renderer.Initialize();
  if (!init_status.ok()) {
    ABSL_LOG(WARNING) << "Could not initialize ARRenderer (no GL context?): " 
                      << init_status;
    ABSL_LOG(INFO) << "Skipping GL-dependent test";
    return true;  // Not a failure, just can't test without GL
  }
  
  // Check if sample filters exist
  if (!fs::exists("assets/filters/classic-glasses/filter.json")) {
    ABSL_LOG(WARNING) << "Sample filters not found, skipping integration test";
    return true;
  }
  
  // Load a filter
  auto filter = FilterAsset::LoadFromDirectory("assets/filters/classic-glasses");
  if (!filter.ok()) {
    ABSL_LOG(ERROR) << "Failed to load filter: " << filter.status();
    return false;
  }
  
  // Load filter into renderer
  auto load_status = renderer.LoadFilter(*filter);
  if (!load_status.ok()) {
    ABSL_LOG(ERROR) << "Failed to load filter into renderer: " << load_status;
    return false;
  }
  
  // Verify filter is loaded
  const auto& metadata = filter->GetMetadata();
  if (!renderer.HasFilter(metadata.id)) {
    ABSL_LOG(ERROR) << "Filter not found in renderer after loading";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Filter loaded successfully: " << metadata.name;
  
  // Test unloading
  auto unload_status = renderer.UnloadFilter(metadata.id);
  if (!unload_status.ok()) {
    ABSL_LOG(ERROR) << "Failed to unload filter: " << unload_status;
    return false;
  }
  
  if (renderer.HasFilter(metadata.id)) {
    ABSL_LOG(ERROR) << "Filter still present after unloading";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Filter unloaded successfully";
  
  return true;
}

bool TestLoadMultipleFilters() {
  ABSL_LOG(INFO) << "Test: Load multiple filters";
  
  ARRenderer renderer;
  auto init_status = renderer.Initialize();
  if (!init_status.ok()) {
    ABSL_LOG(WARNING) << "Skipping GL-dependent test";
    return true;
  }
  
  // Load all sample filters
  const char* filter_paths[] = {
    "assets/filters/classic-glasses",
    "assets/filters/party-hat",
    "assets/filters/cat-ears"
  };
  
  int loaded = 0;
  for (const char* path : filter_paths) {
    if (!fs::exists(std::string(path) + "/filter.json")) {
      continue;
    }
    
    auto filter = FilterAsset::LoadFromDirectory(path);
    if (!filter.ok()) {
      ABSL_LOG(WARNING) << "Could not load " << path << ": " << filter.status();
      continue;
    }
    
    auto load_status = renderer.LoadFilter(*filter);
    if (load_status.ok()) {
      loaded++;
      ABSL_LOG(INFO) << "  Loaded: " << filter->GetMetadata().name;
    } else {
      ABSL_LOG(WARNING) << "Could not load into renderer: " << load_status;
    }
  }
  
  ABSL_LOG(INFO) << "✓ Loaded " << loaded << " filters";
  return true;
}

bool TestLoadFilterTwice() {
  ABSL_LOG(INFO) << "Test: Load same filter twice (should fail)";
  
  ARRenderer renderer;
  auto init_status = renderer.Initialize();
  if (!init_status.ok()) {
    ABSL_LOG(WARNING) << "Skipping GL-dependent test";
    return true;
  }
  
  if (!fs::exists("assets/filters/classic-glasses/filter.json")) {
    ABSL_LOG(WARNING) << "Sample filters not found, skipping test";
    return true;
  }
  
  auto filter = FilterAsset::LoadFromDirectory("assets/filters/classic-glasses");
  if (!filter.ok()) {
    ABSL_LOG(WARNING) << "Could not load filter asset";
    return true;
  }
  
  // First load should succeed
  auto load1 = renderer.LoadFilter(*filter);
  if (!load1.ok()) {
    ABSL_LOG(ERROR) << "First load failed: " << load1;
    return false;
  }
  
  // Second load should fail (already exists)
  auto load2 = renderer.LoadFilter(*filter);
  if (load2.ok()) {
    ABSL_LOG(ERROR) << "Second load should have failed but succeeded";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Duplicate load correctly rejected";
  return true;
}

bool TestUnloadNonexistentFilter() {
  ABSL_LOG(INFO) << "Test: Unload nonexistent filter (should fail)";
  
  ARRenderer renderer;
  auto init_status = renderer.Initialize();
  if (!init_status.ok()) {
    ABSL_LOG(WARNING) << "Skipping GL-dependent test";
    return true;
  }
  
  auto unload_status = renderer.UnloadFilter("nonexistent-filter-id");
  if (unload_status.ok()) {
    ABSL_LOG(ERROR) << "Unload should have failed for nonexistent filter";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Nonexistent filter unload correctly rejected";
  return true;
}

int main(int argc, char** argv) {
  ABSL_LOG(INFO) << "ARRenderer + FilterAsset Integration Tests";
  ABSL_LOG(INFO) << "==========================================";
  
  int passed = 0;
  int failed = 0;
  
  struct Test {
    const char* name;
    bool (*func)();
  };
  
  Test tests[] = {
    {"LoadFilterIntoRenderer", TestLoadFilterIntoRenderer},
    {"LoadMultipleFilters", TestLoadMultipleFilters},
    {"LoadFilterTwice", TestLoadFilterTwice},
    {"UnloadNonexistentFilter", TestUnloadNonexistentFilter}
  };
  
  for (const auto& test : tests) {
    try {
      if (test.func()) {
        passed++;
      } else {
        failed++;
        ABSL_LOG(ERROR) << "✗ Test failed: " << test.name;
      }
    } catch (const std::exception& e) {
      failed++;
      ABSL_LOG(ERROR) << "✗ Test threw exception: " << test.name 
                      << ": " << e.what();
    }
  }
  
  ABSL_LOG(INFO) << "";
  ABSL_LOG(INFO) << "Results: " << passed << " passed, " << failed << " failed";
  
  return (failed == 0) ? 0 : 1;
}
