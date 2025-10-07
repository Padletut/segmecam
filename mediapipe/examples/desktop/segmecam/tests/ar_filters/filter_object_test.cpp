// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// FilterObject Unit Tests - Phase 3 Step 6
// Tests for both PRIMITIVE and MODEL_3D filter types

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_object.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/model_loader.h"

#include <gtest/gtest.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace segmecam {
namespace {

// Test fixture for FilterObject tests
class FilterObjectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Get workspace root
    workspace_root_ = std::getenv("TEST_SRCDIR");
    if (workspace_root_.empty()) {
      workspace_root_ = ".";
    }
    workspace_root_ += "/mediapipe";
    
    // Asset paths
    assets_dir_ = workspace_root_ + "/assets/ar_filters/models";
    cube_model_path_ = assets_dir_ + "/simple_cube.obj";
    glasses_model_path_ = assets_dir_ + "/simple_glasses.obj";
    hat_model_path_ = assets_dir_ + "/simple_hat.obj";
  }
  
  bool FileExists(const std::string& path) {
    return fs::exists(path);
  }
  
  std::string workspace_root_;
  std::string assets_dir_;
  std::string cube_model_path_;
  std::string glasses_model_path_;
  std::string hat_model_path_;
};

// ===== Primitive Creation Tests =====

TEST_F(FilterObjectTest, CreateCube) {
  auto cube = CreateCube("nose_bridge", "Test Cube");
  
  // Verify type
  EXPECT_EQ(cube.type, FilterObject::Type::PRIMITIVE)
      << "Cube should be PRIMITIVE type";
  
  // Verify basic properties
  EXPECT_EQ(cube.anchor_name, "nose_bridge");
  EXPECT_EQ(cube.name, "Test Cube");
  EXPECT_FALSE(cube.id.empty()) << "Should have generated ID";
  EXPECT_TRUE(cube.enabled);
  EXPECT_TRUE(cube.visible);
  
  // Verify has geometry
  EXPECT_GT(cube.vertices.size(), 0) << "Cube should have vertices";
  EXPECT_GT(cube.indices.size(), 0) << "Cube should have indices";
  EXPECT_GT(cube.normals.size(), 0) << "Cube should have normals";
  
  // Verify vertices count (cube has 8 unique vertices)
  EXPECT_EQ(cube.vertices.size(), 8) << "Cube should have 8 vertices";
  
  // Verify indices count (cube has 12 triangles = 36 indices)
  EXPECT_EQ(cube.indices.size(), 36) << "Cube should have 36 indices";
  
  // Verify model pointer is null (primitive doesn't use model)
  EXPECT_EQ(cube.model, nullptr) << "Primitive should not have model";
}

TEST_F(FilterObjectTest, CreateSphere) {
  auto sphere = CreateSphere("forehead", "Test Sphere", 16, 16);
  
  // Verify type
  EXPECT_EQ(sphere.type, FilterObject::Type::PRIMITIVE);
  
  // Verify basic properties
  EXPECT_EQ(sphere.anchor_name, "forehead");
  EXPECT_EQ(sphere.name, "Test Sphere");
  
  // Verify has geometry
  EXPECT_GT(sphere.vertices.size(), 0);
  EXPECT_GT(sphere.indices.size(), 0);
  EXPECT_GT(sphere.normals.size(), 0);
  
  // Sphere with 16x16 resolution should have many vertices
  EXPECT_GE(sphere.vertices.size(), 100) << "Sphere should have many vertices";
  
  // Verify model pointer is null
  EXPECT_EQ(sphere.model, nullptr);
}

TEST_F(FilterObjectTest, CreateCylinder) {
  auto cylinder = CreateCylinder("left_eye", "Test Cylinder", 16);
  
  // Verify type
  EXPECT_EQ(cylinder.type, FilterObject::Type::PRIMITIVE);
  
  // Verify basic properties
  EXPECT_EQ(cylinder.anchor_name, "left_eye");
  EXPECT_EQ(cylinder.name, "Test Cylinder");
  
  // Verify has geometry
  EXPECT_GT(cylinder.vertices.size(), 0);
  EXPECT_GT(cylinder.indices.size(), 0);
  EXPECT_GT(cylinder.normals.size(), 0);
  
  // Verify model pointer is null
  EXPECT_EQ(cylinder.model, nullptr);
}

TEST_F(FilterObjectTest, CreateCone) {
  auto cone = CreateCone("right_eye", "Test Cone", 16);
  
  // Verify type
  EXPECT_EQ(cone.type, FilterObject::Type::PRIMITIVE);
  
  // Verify basic properties
  EXPECT_EQ(cone.anchor_name, "right_eye");
  EXPECT_EQ(cone.name, "Test Cone");
  
  // Verify has geometry
  EXPECT_GT(cone.vertices.size(), 0);
  EXPECT_GT(cone.indices.size(), 0);
  EXPECT_GT(cone.normals.size(), 0);
  
  // Verify model pointer is null
  EXPECT_EQ(cone.model, nullptr);
}

// ===== Model Creation Tests =====

TEST_F(FilterObjectTest, CreateFromModel) {
  // Test loading a 3D model from file
  ASSERT_TRUE(FileExists(cube_model_path_))
      << "Test asset not found: " << cube_model_path_;
  
  auto filter = CreateFromModel("nose_bridge", "Test Model", cube_model_path_);
  
  // Verify type
  EXPECT_EQ(filter.type, FilterObject::Type::MODEL_3D)
      << "Loaded model should be MODEL_3D type";
  
  // Verify basic properties
  EXPECT_EQ(filter.anchor_name, "nose_bridge");
  EXPECT_EQ(filter.name, "Test Model");
  EXPECT_FALSE(filter.id.empty());
  
  // Verify model is loaded
  EXPECT_NE(filter.model, nullptr) << "Should have loaded model";
  
  if (filter.model) {
    EXPECT_GT(filter.model->meshes.size(), 0) << "Model should have meshes";
    EXPECT_EQ(filter.model->vertex_count, 8) << "Cube should have 8 vertices";
    EXPECT_GT(filter.model->triangle_count, 0) << "Should have triangles";
  }
  
  // Verify primitive arrays are empty (model doesn't use them)
  EXPECT_EQ(filter.vertices.size(), 0) << "MODEL_3D should not use vertices array";
  EXPECT_EQ(filter.indices.size(), 0) << "MODEL_3D should not use indices array";
  EXPECT_EQ(filter.normals.size(), 0) << "MODEL_3D should not use normals array";
}

TEST_F(FilterObjectTest, CreateFromGlassesModel) {
  // Test loading glasses model
  ASSERT_TRUE(FileExists(glasses_model_path_));
  
  auto filter = CreateFromModel("nose_bridge", "Glasses", glasses_model_path_);
  
  EXPECT_EQ(filter.type, FilterObject::Type::MODEL_3D);
  EXPECT_NE(filter.model, nullptr);
  
  if (filter.model) {
    EXPECT_EQ(filter.model->vertex_count, 36) << "Glasses should have 36 vertices";
    EXPECT_EQ(filter.model->materials.size(), 2) << "Glasses should have 2 materials";
  }
}

TEST_F(FilterObjectTest, CreateFromHatModel) {
  // Test loading hat model
  ASSERT_TRUE(FileExists(hat_model_path_));
  
  auto filter = CreateFromModel("forehead", "Hat", hat_model_path_);
  
  EXPECT_EQ(filter.type, FilterObject::Type::MODEL_3D);
  EXPECT_NE(filter.model, nullptr);
  
  if (filter.model) {
    EXPECT_EQ(filter.model->vertex_count, 34) << "Hat should have 34 vertices";
    EXPECT_EQ(filter.model->materials.size(), 2) << "Hat should have 2 materials";
  }
}

// ===== Error Handling Tests =====

TEST_F(FilterObjectTest, InvalidModelPath) {
  // Test loading non-existent model
  auto filter = CreateFromModel("nose_bridge", "Invalid", "/nonexistent/model.obj");
  
  // Filter should be created but disabled
  EXPECT_EQ(filter.type, FilterObject::Type::MODEL_3D);
  EXPECT_FALSE(filter.enabled) << "Failed load should disable filter";
  EXPECT_FALSE(filter.visible) << "Failed load should hide filter";
  EXPECT_EQ(filter.model, nullptr) << "Failed load should have null model";
}

TEST_F(FilterObjectTest, EmptyModelPath) {
  // Test loading with empty path
  auto filter = CreateFromModel("nose_bridge", "Empty", "");
  
  EXPECT_FALSE(filter.enabled) << "Empty path should disable filter";
  EXPECT_EQ(filter.model, nullptr);
}

// ===== Type Field Tests =====

TEST_F(FilterObjectTest, TypeFieldCorrect) {
  // Verify type field is set correctly for all primitives
  auto cube = CreateCube("nose_bridge", "Cube");
  auto sphere = CreateSphere("nose_bridge", "Sphere");
  auto cylinder = CreateCylinder("nose_bridge", "Cylinder");
  auto cone = CreateCone("nose_bridge", "Cone");
  
  EXPECT_EQ(cube.type, FilterObject::Type::PRIMITIVE);
  EXPECT_EQ(sphere.type, FilterObject::Type::PRIMITIVE);
  EXPECT_EQ(cylinder.type, FilterObject::Type::PRIMITIVE);
  EXPECT_EQ(cone.type, FilterObject::Type::PRIMITIVE);
  
  // Verify type field for model
  if (FileExists(cube_model_path_)) {
    auto model = CreateFromModel("nose_bridge", "Model", cube_model_path_);
    EXPECT_EQ(model.type, FilterObject::Type::MODEL_3D);
  }
}

// ===== Backward Compatibility Tests =====

TEST_F(FilterObjectTest, PrimitivesStillWork) {
  // Verify that all Phase 2 primitive creation still works
  
  // Create all primitive types
  auto cube = CreateCube("nose_bridge", "Cube");
  auto sphere = CreateSphere("forehead", "Sphere");
  auto cylinder = CreateCylinder("left_eye", "Cylinder");
  auto cone = CreateCone("right_eye", "Cone");
  
  // All should be enabled and visible by default
  EXPECT_TRUE(cube.enabled);
  EXPECT_TRUE(cube.visible);
  EXPECT_TRUE(sphere.enabled);
  EXPECT_TRUE(sphere.visible);
  EXPECT_TRUE(cylinder.enabled);
  EXPECT_TRUE(cylinder.visible);
  EXPECT_TRUE(cone.enabled);
  EXPECT_TRUE(cone.visible);
  
  // All should have geometry
  EXPECT_GT(cube.vertices.size(), 0);
  EXPECT_GT(sphere.vertices.size(), 0);
  EXPECT_GT(cylinder.vertices.size(), 0);
  EXPECT_GT(cone.vertices.size(), 0);
  
  // All should have null model (primitives don't use model)
  EXPECT_EQ(cube.model, nullptr);
  EXPECT_EQ(sphere.model, nullptr);
  EXPECT_EQ(cylinder.model, nullptr);
  EXPECT_EQ(cone.model, nullptr);
}

// ===== Model Sharing Tests =====

TEST_F(FilterObjectTest, ModelSharing) {
  // Test that multiple filters can share the same model
  ASSERT_TRUE(FileExists(cube_model_path_));
  
  auto filter1 = CreateFromModel("left_eye", "Cube1", cube_model_path_);
  auto filter2 = CreateFromModel("right_eye", "Cube2", cube_model_path_);
  
  // Both should have valid models
  EXPECT_NE(filter1.model, nullptr);
  EXPECT_NE(filter2.model, nullptr);
  
  // Models are loaded separately (not shared via shared_ptr yet)
  // In future optimization, they could share the same model data
  
  // Verify both have same vertex count (loaded same model)
  if (filter1.model && filter2.model) {
    EXPECT_EQ(filter1.model->vertex_count, filter2.model->vertex_count);
  }
}

// ===== ID Generation Tests =====

TEST_F(FilterObjectTest, UniqueIDs) {
  // Verify that each filter gets a unique ID
  auto cube1 = CreateCube("nose_bridge", "Cube1");
  auto cube2 = CreateCube("nose_bridge", "Cube2");
  auto cube3 = CreateCube("nose_bridge", "Cube3");
  
  EXPECT_NE(cube1.id, cube2.id) << "Each filter should have unique ID";
  EXPECT_NE(cube2.id, cube3.id);
  EXPECT_NE(cube1.id, cube3.id);
  
  // IDs should not be empty
  EXPECT_FALSE(cube1.id.empty());
  EXPECT_FALSE(cube2.id.empty());
  EXPECT_FALSE(cube3.id.empty());
}

TEST_F(FilterObjectTest, IDsAcrossTypes) {
  // Verify IDs are unique across primitive and model types
  auto primitive = CreateCube("nose_bridge", "Primitive");
  
  std::string model_id;
  if (FileExists(cube_model_path_)) {
    auto model = CreateFromModel("nose_bridge", "Model", cube_model_path_);
    model_id = model.id;
    
    EXPECT_NE(primitive.id, model_id)
        << "Primitive and model IDs should be unique";
  }
}

// ===== Property Tests =====

TEST_F(FilterObjectTest, DefaultProperties) {
  // Verify default property values
  auto cube = CreateCube("nose_bridge", "Cube");
  
  // Position should be zero
  EXPECT_FLOAT_EQ(cube.position[0], 0.0f);
  EXPECT_FLOAT_EQ(cube.position[1], 0.0f);
  EXPECT_FLOAT_EQ(cube.position[2], 0.0f);
  
  // Offset should be zero
  EXPECT_FLOAT_EQ(cube.offset[0], 0.0f);
  EXPECT_FLOAT_EQ(cube.offset[1], 0.0f);
  EXPECT_FLOAT_EQ(cube.offset[2], 0.0f);
  
  // Scale should be one
  EXPECT_FLOAT_EQ(cube.scale[0], 1.0f);
  EXPECT_FLOAT_EQ(cube.scale[1], 1.0f);
  EXPECT_FLOAT_EQ(cube.scale[2], 1.0f);
  
  // Rotation should be identity quaternion (1, 0, 0, 0)
  EXPECT_FLOAT_EQ(cube.rotation[0], 1.0f);
  EXPECT_FLOAT_EQ(cube.rotation[1], 0.0f);
  EXPECT_FLOAT_EQ(cube.rotation[2], 0.0f);
  EXPECT_FLOAT_EQ(cube.rotation[3], 0.0f);
  
  // Frame count should be zero
  EXPECT_EQ(cube.frame_count, 0);
}

TEST_F(FilterObjectTest, PropertyModification) {
  // Verify properties can be modified
  auto cube = CreateCube("nose_bridge", "Cube");
  
  // Modify properties
  cube.position = cv::Vec3f(1.0f, 2.0f, 3.0f);
  cube.scale = cv::Vec3f(2.0f, 2.0f, 2.0f);
  cube.enabled = false;
  cube.visible = false;
  
  // Verify changes
  EXPECT_FLOAT_EQ(cube.position[0], 1.0f);
  EXPECT_FLOAT_EQ(cube.position[1], 2.0f);
  EXPECT_FLOAT_EQ(cube.position[2], 3.0f);
  EXPECT_FLOAT_EQ(cube.scale[0], 2.0f);
  EXPECT_FALSE(cube.enabled);
  EXPECT_FALSE(cube.visible);
}

// ===== Mixed Type Usage Tests =====

TEST_F(FilterObjectTest, MixedTypesInVector) {
  // Test that primitives and models can coexist in same vector
  std::vector<FilterObject> filters;
  
  // Add primitives
  filters.push_back(CreateCube("nose_bridge", "Cube"));
  filters.push_back(CreateSphere("forehead", "Sphere"));
  
  // Add model if available
  if (FileExists(cube_model_path_)) {
    filters.push_back(CreateFromModel("left_eye", "Model", cube_model_path_));
  }
  
  // Verify types
  EXPECT_EQ(filters[0].type, FilterObject::Type::PRIMITIVE);
  EXPECT_EQ(filters[1].type, FilterObject::Type::PRIMITIVE);
  
  if (filters.size() > 2) {
    EXPECT_EQ(filters[2].type, FilterObject::Type::MODEL_3D);
  }
  
  // Verify all have unique IDs
  for (size_t i = 0; i < filters.size(); ++i) {
    for (size_t j = i + 1; j < filters.size(); ++j) {
      EXPECT_NE(filters[i].id, filters[j].id);
    }
  }
}

// ===== Memory Management Tests =====

TEST_F(FilterObjectTest, ModelLifecycle) {
  // Test that model is properly managed through filter lifecycle
  
  if (!FileExists(cube_model_path_)) {
    GTEST_SKIP() << "Test asset not available";
  }
  
  {
    // Create filter in scope
    auto filter = CreateFromModel("nose_bridge", "Scoped", cube_model_path_);
    EXPECT_NE(filter.model, nullptr);
    EXPECT_GT(filter.model.use_count(), 0);
  }
  
  // Filter is destroyed, model should be cleaned up automatically
  // (shared_ptr handles cleanup)
}

TEST_F(FilterObjectTest, CopyFilter) {
  // Test that filters can be copied
  auto original = CreateCube("nose_bridge", "Original");
  auto copy = original;
  
  // Both should be valid
  EXPECT_EQ(copy.type, FilterObject::Type::PRIMITIVE);
  EXPECT_EQ(copy.anchor_name, original.anchor_name);
  EXPECT_EQ(copy.name, original.name);
  EXPECT_EQ(copy.id, original.id);  // Copy shares ID
  
  // Verify geometry is copied
  EXPECT_EQ(copy.vertices.size(), original.vertices.size());
  EXPECT_EQ(copy.indices.size(), original.indices.size());
}

TEST_F(FilterObjectTest, MoveFilter) {
  // Test that filters can be moved
  auto original = CreateCube("nose_bridge", "Original");
  std::string original_id = original.id;
  size_t original_vertex_count = original.vertices.size();
  
  auto moved = std::move(original);
  
  // Moved filter should have original's data
  EXPECT_EQ(moved.type, FilterObject::Type::PRIMITIVE);
  EXPECT_EQ(moved.id, original_id);
  EXPECT_EQ(moved.vertices.size(), original_vertex_count);
}

}  // namespace
}  // namespace segmecam
