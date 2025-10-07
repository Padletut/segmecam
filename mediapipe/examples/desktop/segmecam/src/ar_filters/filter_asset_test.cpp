// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// FilterAsset Unit Tests
//
// Tests JSON parsing, asset loading, validation, and error handling.

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_asset.h"

#include <fstream>
#include <filesystem>
#include "absl/log/absl_log.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"

namespace segmecam {
namespace ar_filters {
namespace {

namespace fs = std::filesystem;

// Test helper: Create temporary filter directory
class TempFilterDir {
 public:
  TempFilterDir(const std::string& name) {
    path_ = fs::temp_directory_path() / "segmecam_test" / name;
    fs::create_directories(path_);
    fs::create_directories(path_ / "models");
    fs::create_directories(path_ / "textures");
  }
  
  ~TempFilterDir() {
    if (fs::exists(path_)) {
      fs::remove_all(path_);
    }
  }
  
  std::string GetPath() const { return path_.string(); }
  
  void WriteFile(const std::string& relative_path, const std::string& content) {
    auto file_path = path_ / relative_path;
    std::ofstream out(file_path);
    out << content;
    out.close();
  }
  
 private:
  fs::path path_;
};

// Test 1: Load valid minimal filter
bool TestLoadMinimalFilter() {
  ABSL_LOG(INFO) << "Test 1: Load minimal valid filter";
  
  TempFilterDir temp("minimal");
  temp.WriteFile("filter.json", R"({
    "filter": {
      "name": "Test Filter",
      "id": "test-filter-v1",
      "version": "1.0.0"
    },
    "attachments": [
      {
        "id": "test_obj",
        "anchor": "nose_bridge",
        "model": "models/test.obj"
      }
    ]
  })");
  
  // Create dummy model file
  temp.WriteFile("models/test.obj", "# dummy obj");
  
  auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
  if (!result.ok()) {
    ABSL_LOG(ERROR) << "Failed to load minimal filter: " << result.status();
    return false;
  }
  
  const auto& meta = result->GetMetadata();
  if (meta.name != "Test Filter") {
    ABSL_LOG(ERROR) << "Expected name 'Test Filter', got: " << meta.name;
    return false;
  }
  
  if (meta.id != "test-filter-v1") {
    ABSL_LOG(ERROR) << "Expected id 'test-filter-v1', got: " << meta.id;
    return false;
  }
  
  const auto& attachments = result->GetAttachments();
  if (attachments.size() != 1) {
    ABSL_LOG(ERROR) << "Expected 1 attachment, got: " << attachments.size();
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Test 1 passed";
  return true;
}

// Test 2: Load filter with full metadata
bool TestLoadFullMetadata() {
  ABSL_LOG(INFO) << "Test 2: Load filter with full metadata";
  
  TempFilterDir temp("fullmeta");
  temp.WriteFile("filter.json", R"({
    "filter": {
      "name": "Full Test",
      "id": "full-test-v1",
      "category": "test",
      "version": "2.0.0",
      "author": "Test Author",
      "description": "Test description",
      "thumbnail": "thumb.png",
      "icon": "icon.png"
    },
    "attachments": [
      {
        "id": "obj1",
        "anchor": "forehead",
        "model": "models/test.obj"
      }
    ]
  })");
  
  temp.WriteFile("models/test.obj", "# dummy");
  temp.WriteFile("thumb.png", "fake png");
  temp.WriteFile("icon.png", "fake icon");
  
  auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
  if (!result.ok()) {
    ABSL_LOG(ERROR) << "Failed to load full metadata: " << result.status();
    return false;
  }
  
  const auto& meta = result->GetMetadata();
  if (meta.category != "test" || meta.author != "Test Author") {
    ABSL_LOG(ERROR) << "Metadata fields not parsed correctly";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Test 2 passed";
  return true;
}

// Test 3: Load multiple attachments
bool TestLoadMultipleAttachments() {
  ABSL_LOG(INFO) << "Test 3: Load multiple attachments";
  
  TempFilterDir temp("multi");
  temp.WriteFile("filter.json", R"({
    "filter": {
      "name": "Multi",
      "id": "multi-v1",
      "version": "1.0.0"
    },
    "attachments": [
      {
        "id": "left",
        "anchor": "left_ear",
        "model": "models/left.obj",
        "scale": [0.8, 0.8, 0.8],
        "offset": [-0.02, 0.05, 0.0]
      },
      {
        "id": "right",
        "anchor": "right_ear",
        "model": "models/right.obj",
        "scale": [0.8, 0.8, 0.8],
        "offset": [0.02, 0.05, 0.0]
      }
    ]
  })");
  
  temp.WriteFile("models/left.obj", "# left");
  temp.WriteFile("models/right.obj", "# right");
  
  auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
  if (!result.ok()) {
    ABSL_LOG(ERROR) << "Failed to load multiple attachments: " << result.status();
    return false;
  }
  
  const auto& attachments = result->GetAttachments();
  if (attachments.size() != 2) {
    ABSL_LOG(ERROR) << "Expected 2 attachments, got: " << attachments.size();
    return false;
  }
  
  if (attachments[0].id != "left" || attachments[1].id != "right") {
    ABSL_LOG(ERROR) << "Attachment IDs not parsed correctly";
    return false;
  }
  
  // Check scale parsing
  if (attachments[0].scale.x != 0.8f) {
    ABSL_LOG(ERROR) << "Scale not parsed correctly";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Test 3 passed";
  return true;
}

// Test 4: Load materials
bool TestLoadMaterials() {
  ABSL_LOG(INFO) << "Test 4: Load materials";
  
  TempFilterDir temp("materials");
  temp.WriteFile("filter.json", R"({
    "filter": {
      "name": "Materials Test",
      "id": "mat-v1",
      "version": "1.0.0"
    },
    "attachments": [
      {
        "id": "obj1",
        "anchor": "nose_bridge",
        "model": "models/test.obj"
      }
    ],
    "materials": {
      "obj1": {
        "ambient": [0.1, 0.2, 0.3],
        "diffuse": [0.4, 0.5, 0.6],
        "specular": [0.7, 0.8, 0.9],
        "shininess": 64.0,
        "opacity": 0.8
      }
    }
  })");
  
  temp.WriteFile("models/test.obj", "# dummy");
  
  auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
  if (!result.ok()) {
    ABSL_LOG(ERROR) << "Failed to load materials: " << result.status();
    return false;
  }
  
  const auto& materials = result->GetMaterials();
  if (materials.size() != 1) {
    ABSL_LOG(ERROR) << "Expected 1 material, got: " << materials.size();
    return false;
  }
  
  auto mat_it = materials.find("obj1");
  if (mat_it == materials.end()) {
    ABSL_LOG(ERROR) << "Material 'obj1' not found";
    return false;
  }
  
  const auto& mat = mat_it->second;
  if (mat.ambient.x != 0.1f || mat.shininess != 64.0f || mat.opacity != 0.8f) {
    ABSL_LOG(ERROR) << "Material properties not parsed correctly";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Test 4 passed";
  return true;
}

// Test 5: Load behaviors
bool TestLoadBehaviors() {
  ABSL_LOG(INFO) << "Test 5: Load behaviors";
  
  TempFilterDir temp("behaviors");
  temp.WriteFile("filter.json", R"({
    "filter": {
      "name": "Behaviors Test",
      "id": "behav-v1",
      "version": "1.0.0"
    },
    "attachments": [
      {
        "id": "obj1",
        "anchor": "forehead",
        "model": "models/test.obj"
      }
    ],
    "behaviors": {
      "shake_test": {
        "type": "shake",
        "target": "obj1",
        "blendshape": "headRotate",
        "threshold": 15.0,
        "intensity": 0.8
      },
      "scale_test": {
        "type": "scale",
        "target": "obj1",
        "blendshape": "mouthSmile",
        "threshold": 0.5,
        "intensity": 0.5,
        "target_scale": [1.2, 1.2, 1.2]
      }
    }
  })");
  
  temp.WriteFile("models/test.obj", "# dummy");
  
  auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
  if (!result.ok()) {
    ABSL_LOG(ERROR) << "Failed to load behaviors: " << result.status();
    return false;
  }
  
  const auto& behaviors = result->GetBehaviors();
  if (behaviors.size() != 2) {
    ABSL_LOG(ERROR) << "Expected 2 behaviors, got: " << behaviors.size();
    return false;
  }
  
  // Check both behaviors exist (order doesn't matter since map iteration is unordered)
  bool has_shake = false;
  bool has_scale = false;
  for (const auto& behavior : behaviors) {
    if (behavior.type == FilterBehavior::Type::SHAKE) {
      has_shake = true;
      if (behavior.blendshape_name != "headRotate") {
        ABSL_LOG(ERROR) << "SHAKE blendshape should be headRotate, got: " << behavior.blendshape_name;
        return false;
      }
    }
    if (behavior.type == FilterBehavior::Type::SCALE) {
      has_scale = true;
      if (behavior.blendshape_name != "mouthSmile") {
        ABSL_LOG(ERROR) << "SCALE blendshape should be mouthSmile, got: " << behavior.blendshape_name;
        return false;
      }
    }
  }
  
  if (!has_shake || !has_scale) {
    ABSL_LOG(ERROR) << "Missing SHAKE or SCALE behavior";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Test 5 passed";
  return true;
}

// Test 6: Missing required fields
bool TestMissingRequiredFields() {
  ABSL_LOG(INFO) << "Test 6: Missing required fields";
  
  // Missing filter name
  {
    TempFilterDir temp("missing_name");
    temp.WriteFile("filter.json", R"({
      "filter": {
        "id": "test-v1",
        "version": "1.0.0"
      },
      "attachments": [{"id": "a", "anchor": "nose_bridge", "model": "m.obj"}]
    })");
    
    auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
    if (result.ok()) {
      ABSL_LOG(ERROR) << "Should have failed with missing name";
      return false;
    }
  }
  
  // Missing attachment anchor
  {
    TempFilterDir temp("missing_anchor");
    temp.WriteFile("filter.json", R"({
      "filter": {"name": "T", "id": "t-v1", "version": "1.0.0"},
      "attachments": [{"id": "a", "model": "m.obj"}]
    })");
    
    auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
    if (result.ok()) {
      ABSL_LOG(ERROR) << "Should have failed with missing anchor";
      return false;
    }
  }
  
  ABSL_LOG(INFO) << "✓ Test 6 passed";
  return true;
}

// Test 7: Malformed JSON
bool TestMalformedJSON() {
  ABSL_LOG(INFO) << "Test 7: Malformed JSON";
  
  TempFilterDir temp("malformed");
  temp.WriteFile("filter.json", "{ this is not valid json }");
  
  auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
  if (result.ok()) {
    ABSL_LOG(ERROR) << "Should have failed with malformed JSON";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Test 7 passed";
  return true;
}

// Test 8: ValidateAssets - Missing files
bool TestValidateAssetsMissing() {
  ABSL_LOG(INFO) << "Test 8: ValidateAssets with missing files";
  
  TempFilterDir temp("validate");
  temp.WriteFile("filter.json", R"({
    "filter": {"name": "T", "id": "t-v1", "version": "1.0.0"},
    "attachments": [
      {
        "id": "a",
        "anchor": "nose_bridge",
        "model": "models/missing.obj",
        "texture": "textures/missing.png"
      }
    ]
  })");
  
  // Don't create the model/texture files
  auto result = FilterAsset::LoadFromDirectory(temp.GetPath());
  if (!result.ok()) {
    ABSL_LOG(ERROR) << "Should load even with missing files: " << result.status();
    return false;
  }
  
  // Validation should fail
  auto validation = result->ValidateAssets();
  if (validation.ok()) {
    ABSL_LOG(ERROR) << "ValidateAssets should have failed";
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Test 8 passed";
  return true;
}

// Test 9: EnumerateFilters
bool TestEnumerateFilters() {
  ABSL_LOG(INFO) << "Test 9: EnumerateFilters";
  
  auto base_path = fs::temp_directory_path() / "segmecam_test" / "enumerate";
  fs::create_directories(base_path);
  
  // Create multiple filter directories (only at top level - EnumerateFilters uses directory_iterator, not recursive)
  auto filter1_path = base_path / "filter1";
  auto filter2_path = base_path / "filter2";
  auto filter3_path = base_path / "filter3";
  
  fs::create_directories(filter1_path);
  fs::create_directories(filter2_path);
  fs::create_directories(filter3_path);
  
  // Create filter.json files
  std::ofstream(filter1_path / "filter.json") << "{}";
  std::ofstream(filter2_path / "filter.json") << "{}";
  std::ofstream(filter3_path / "filter.json") << "{}";
  
  auto filters = FilterAsset::EnumerateFilters(base_path.string());
  
  // Clean up
  fs::remove_all(base_path);
  
  if (filters.size() != 3) {
    ABSL_LOG(ERROR) << "Expected 3 filters, got: " << filters.size();
    return false;
  }
  
  ABSL_LOG(INFO) << "✓ Test 9 passed";
  return true;
}

// Test 10: Load real sample filters
bool TestLoadRealSamples() {
  ABSL_LOG(INFO) << "Test 10: Load real sample filters";
  
  const char* sample_filters[] = {
    "assets/filters/classic-glasses",
    "assets/filters/party-hat",
    "assets/filters/cat-ears"
  };
  
  int loaded = 0;
  for (const char* path : sample_filters) {
    if (!fs::exists(path)) {
      ABSL_LOG(WARNING) << "Sample filter not found: " << path;
      continue;
    }
    
    auto result = FilterAsset::LoadFromDirectory(path);
    if (!result.ok()) {
      ABSL_LOG(ERROR) << "Failed to load " << path << ": " << result.status();
      return false;
    }
    
    ABSL_LOG(INFO) << "Loaded: " << result->GetMetadata().name;
    loaded++;
  }
  
  if (loaded == 0) {
    ABSL_LOG(WARNING) << "No sample filters found (this is OK for unit test)";
  }
  
  ABSL_LOG(INFO) << "✓ Test 10 passed (" << loaded << " samples loaded)";
  return true;
}

}  // namespace
}  // namespace ar_filters
}  // namespace segmecam

int main(int argc, char** argv) {
  using namespace segmecam::ar_filters;
  
  ABSL_LOG(INFO) << "FilterAsset Unit Tests";
  ABSL_LOG(INFO) << "=====================";
  
  int passed = 0;
  int failed = 0;
  
  struct Test {
    const char* name;
    bool (*func)();
  };
  
  Test tests[] = {
    {"LoadMinimalFilter", TestLoadMinimalFilter},
    {"LoadFullMetadata", TestLoadFullMetadata},
    {"LoadMultipleAttachments", TestLoadMultipleAttachments},
    {"LoadMaterials", TestLoadMaterials},
    {"LoadBehaviors", TestLoadBehaviors},
    {"MissingRequiredFields", TestMissingRequiredFields},
    {"MalformedJSON", TestMalformedJSON},
    {"ValidateAssetsMissing", TestValidateAssetsMissing},
    {"EnumerateFilters", TestEnumerateFilters},
    {"LoadRealSamples", TestLoadRealSamples}
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
      ABSL_LOG(ERROR) << "✗ Test threw exception: " << test.name << ": " << e.what();
    }
  }
  
  ABSL_LOG(INFO) << "";
  ABSL_LOG(INFO) << "Results: " << passed << " passed, " << failed << " failed";
  
  return (failed == 0) ? 0 : 1;
}
