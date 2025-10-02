// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// ModelLoader Unit Tests - Phase 3 Step 6
// Comprehensive tests for 3D model loading functionality

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/model_loader.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace segmecam {
namespace ar_filters {
namespace {

// Test fixture for ModelLoader tests
class ModelLoaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loader_ = std::make_unique<ModelLoader>();
    
    // Get workspace root (bazel test environment)
    // Bazel sets runfiles directory
    workspace_root_ = std::getenv("TEST_SRCDIR");
    if (workspace_root_.empty()) {
      workspace_root_ = ".";
    }
    workspace_root_ += "/mediapipe";
    
    // Asset paths
    assets_dir_ = workspace_root_ + "/assets/ar_filters/models";
    cube_path_ = assets_dir_ + "/simple_cube.obj";
    glasses_path_ = assets_dir_ + "/simple_glasses.obj";
    hat_path_ = assets_dir_ + "/simple_hat.obj";
  }
  
  void TearDown() override {
    loader_.reset();
  }
  
  // Check if a file exists
  bool FileExists(const std::string& path) {
    return fs::exists(path);
  }
  
  std::unique_ptr<ModelLoader> loader_;
  std::string workspace_root_;
  std::string assets_dir_;
  std::string cube_path_;
  std::string glasses_path_;
  std::string hat_path_;
};

// ===== Basic Loading Tests =====

TEST_F(ModelLoaderTest, LoadSimpleOBJ) {
  // Test loading the simplest model (cube)
  ASSERT_TRUE(FileExists(cube_path_)) << "Test asset not found: " << cube_path_;
  
  auto model_or = loader_->LoadModel(cube_path_);
  
  ASSERT_TRUE(model_or.ok()) << "Failed to load cube: " << model_or.status().message();
  
  auto& model = *model_or;
  
  // Verify basic properties
  EXPECT_GT(model.meshes.size(), 0) << "Model should have at least 1 mesh";
  EXPECT_EQ(model.vertex_count, 8) << "Cube should have 8 vertices";
  EXPECT_GT(model.triangle_count, 0) << "Model should have triangles";
  EXPECT_FALSE(model.name.empty()) << "Model should have a name";
  
  // Verify materials
  EXPECT_EQ(model.materials.size(), 1) << "Cube should have 1 material";
  EXPECT_TRUE(model.materials.find("cube_material") != model.materials.end())
      << "Should have cube_material";
  
  // Verify mesh has OpenGL buffers
  EXPECT_GT(model.meshes[0].vao, 0u) << "Mesh should have VAO";
  EXPECT_GT(model.meshes[0].vbo, 0u) << "Mesh should have VBO";
  EXPECT_GT(model.meshes[0].ebo, 0u) << "Mesh should have EBO";
  EXPECT_GT(model.meshes[0].index_count, 0u) << "Mesh should have indices";
  
  // Verify bounding box is reasonable
  EXPECT_LT(model.bounds_min[0], model.bounds_max[0]);
  EXPECT_LT(model.bounds_min[1], model.bounds_max[1]);
  EXPECT_LT(model.bounds_min[2], model.bounds_max[2]);
  
  // Cleanup
  loader_->UnloadModel(model);
}

TEST_F(ModelLoaderTest, LoadGlassesOBJ) {
  // Test loading multi-material model with transparency
  ASSERT_TRUE(FileExists(glasses_path_)) << "Test asset not found: " << glasses_path_;
  
  auto model_or = loader_->LoadModel(glasses_path_);
  
  ASSERT_TRUE(model_or.ok()) << "Failed to load glasses: " << model_or.status().message();
  
  auto& model = *model_or;
  
  // Verify properties
  EXPECT_GT(model.meshes.size(), 0) << "Model should have meshes";
  EXPECT_EQ(model.vertex_count, 36) << "Glasses should have 36 vertices";
  EXPECT_GT(model.triangle_count, 0) << "Model should have triangles";
  
  // Verify materials (lens_material and bridge_material)
  EXPECT_EQ(model.materials.size(), 2) << "Glasses should have 2 materials";
  
  auto lens_mat = model.materials.find("lens_material");
  ASSERT_TRUE(lens_mat != model.materials.end()) << "Should have lens_material";
  
  // Verify lens material has transparency (d = 0.6)
  EXPECT_FLOAT_EQ(lens_mat->second.transparency, 0.6f)
      << "Lens material should be 60% opaque";
  
  // Verify lens material is shiny (Ks = 0.9)
  EXPECT_GT(lens_mat->second.specular[0], 0.8f)
      << "Lens material should have high specular";
  
  auto bridge_mat = model.materials.find("bridge_material");
  ASSERT_TRUE(bridge_mat != model.materials.end()) << "Should have bridge_material";
  
  // Verify bridge material is opaque (d = 1.0)
  EXPECT_FLOAT_EQ(bridge_mat->second.transparency, 1.0f)
      << "Bridge material should be opaque";
  
  // Cleanup
  loader_->UnloadModel(model);
}

TEST_F(ModelLoaderTest, LoadHatOBJ) {
  // Test loading multi-mesh model (body + brim)
  ASSERT_TRUE(FileExists(hat_path_)) << "Test asset not found: " << hat_path_;
  
  auto model_or = loader_->LoadModel(hat_path_);
  
  ASSERT_TRUE(model_or.ok()) << "Failed to load hat: " << model_or.status().message();
  
  auto& model = *model_or;
  
  // Verify properties
  EXPECT_GT(model.meshes.size(), 0) << "Model should have meshes";
  EXPECT_EQ(model.vertex_count, 34) << "Hat should have 34 vertices";
  EXPECT_GT(model.triangle_count, 0) << "Model should have triangles";
  
  // Verify materials (hat_body_material and brim_material)
  EXPECT_EQ(model.materials.size(), 2) << "Hat should have 2 materials";
  
  auto body_mat = model.materials.find("hat_body_material");
  ASSERT_TRUE(body_mat != model.materials.end()) << "Should have hat_body_material";
  
  auto brim_mat = model.materials.find("brim_material");
  ASSERT_TRUE(brim_mat != model.materials.end()) << "Should have brim_material";
  
  // Verify both materials are shiny and black
  EXPECT_GT(body_mat->second.specular[0], 0.7f)
      << "Body material should be shiny";
  EXPECT_GT(brim_mat->second.specular[0], 0.6f)
      << "Brim material should be shiny";
  
  // Verify both are opaque
  EXPECT_FLOAT_EQ(body_mat->second.transparency, 1.0f);
  EXPECT_FLOAT_EQ(brim_mat->second.transparency, 1.0f);
  
  // Cleanup
  loader_->UnloadModel(model);
}

// ===== Error Handling Tests =====

TEST_F(ModelLoaderTest, LoadNonexistent) {
  // Test loading a file that doesn't exist
  auto model_or = loader_->LoadModel("/nonexistent/path/model.obj");
  
  EXPECT_FALSE(model_or.ok()) << "Should fail to load nonexistent file";
  EXPECT_FALSE(model_or.status().message().empty())
      << "Error message should be provided";
}

TEST_F(ModelLoaderTest, LoadEmptyPath) {
  // Test loading with empty path
  auto model_or = loader_->LoadModel("");
  
  EXPECT_FALSE(model_or.ok()) << "Should fail with empty path";
}

TEST_F(ModelLoaderTest, LoadInvalidOBJ) {
  // Create a temporary invalid OBJ file
  std::string temp_path = "/tmp/invalid_test.obj";
  std::ofstream out(temp_path);
  out << "This is not a valid OBJ file\n";
  out << "v 1 2\n";  // Invalid - only 2 components instead of 3
  out << "f 1\n";    // Invalid - face needs at least 3 vertices
  out.close();
  
  auto model_or = loader_->LoadModel(temp_path);
  
  // Should either fail or load with errors
  if (model_or.ok()) {
    // Assimp might still load it but with 0 meshes
    EXPECT_EQ(model_or->meshes.size(), 0) << "Invalid OBJ should have no meshes";
  } else {
    EXPECT_FALSE(model_or.status().message().empty());
  }
  
  // Cleanup
  fs::remove(temp_path);
}

// ===== Material Loading Tests =====

TEST_F(ModelLoaderTest, MaterialProperties) {
  // Test that material properties are loaded correctly
  ASSERT_TRUE(FileExists(cube_path_));
  
  auto model_or = loader_->LoadModel(cube_path_);
  ASSERT_TRUE(model_or.ok());
  
  auto& model = *model_or;
  auto mat_it = model.materials.find("cube_material");
  ASSERT_TRUE(mat_it != model.materials.end());
  
  const auto& mat = mat_it->second;
  
  // Verify ambient (Ka 0.2 0.2 0.2)
  EXPECT_NEAR(mat.ambient[0], 0.2f, 0.01f);
  EXPECT_NEAR(mat.ambient[1], 0.2f, 0.01f);
  EXPECT_NEAR(mat.ambient[2], 0.2f, 0.01f);
  
  // Verify diffuse (Kd 0.8 0.2 0.2 - red)
  EXPECT_NEAR(mat.diffuse[0], 0.8f, 0.01f);
  EXPECT_NEAR(mat.diffuse[1], 0.2f, 0.01f);
  EXPECT_NEAR(mat.diffuse[2], 0.2f, 0.01f);
  
  // Verify specular (Ks 0.5 0.5 0.5)
  EXPECT_NEAR(mat.specular[0], 0.5f, 0.01f);
  EXPECT_NEAR(mat.specular[1], 0.5f, 0.01f);
  EXPECT_NEAR(mat.specular[2], 0.5f, 0.01f);
  
  // Verify shininess (Ns 50)
  EXPECT_NEAR(mat.shininess, 50.0f, 1.0f);
  
  // Verify transparency (d 1.0 - opaque)
  EXPECT_FLOAT_EQ(mat.transparency, 1.0f);
  
  // Cleanup
  loader_->UnloadModel(model);
}

TEST_F(ModelLoaderTest, MaterialTransparency) {
  // Test transparency values (glasses have 60% transparent lenses)
  ASSERT_TRUE(FileExists(glasses_path_));
  
  auto model_or = loader_->LoadModel(glasses_path_);
  ASSERT_TRUE(model_or.ok());
  
  auto& model = *model_or;
  
  // Lens material should be 60% opaque (d = 0.6)
  auto lens_mat = model.materials.find("lens_material");
  ASSERT_TRUE(lens_mat != model.materials.end());
  EXPECT_FLOAT_EQ(lens_mat->second.transparency, 0.6f)
      << "Lens should be 60% opaque (40% transparent)";
  
  // Bridge material should be 100% opaque (d = 1.0)
  auto bridge_mat = model.materials.find("bridge_material");
  ASSERT_TRUE(bridge_mat != model.materials.end());
  EXPECT_FLOAT_EQ(bridge_mat->second.transparency, 1.0f)
      << "Bridge should be fully opaque";
  
  // Cleanup
  loader_->UnloadModel(model);
}

// ===== Bounding Box Tests =====

TEST_F(ModelLoaderTest, BoundingBoxCalculation) {
  // Test that bounding boxes are calculated correctly
  ASSERT_TRUE(FileExists(cube_path_));
  
  auto model_or = loader_->LoadModel(cube_path_);
  ASSERT_TRUE(model_or.ok());
  
  auto& model = *model_or;
  
  // Cube is 0.05 units, centered at origin
  // So bounds should be approximately -0.025 to 0.025
  EXPECT_NEAR(model.bounds_min[0], -0.025f, 0.001f);
  EXPECT_NEAR(model.bounds_max[0], 0.025f, 0.001f);
  EXPECT_NEAR(model.bounds_min[1], -0.025f, 0.001f);
  EXPECT_NEAR(model.bounds_max[1], 0.025f, 0.001f);
  EXPECT_NEAR(model.bounds_min[2], -0.025f, 0.001f);
  EXPECT_NEAR(model.bounds_max[2], 0.025f, 0.001f);
  
  // Verify mesh bounds too
  ASSERT_GT(model.meshes.size(), 0);
  const auto& mesh = model.meshes[0];
  EXPECT_LT(mesh.bounds_min[0], mesh.bounds_max[0]);
  EXPECT_LT(mesh.bounds_min[1], mesh.bounds_max[1]);
  EXPECT_LT(mesh.bounds_min[2], mesh.bounds_max[2]);
  
  // Cleanup
  loader_->UnloadModel(model);
}

TEST_F(ModelLoaderTest, GlassesBoundingBox) {
  // Test glasses bounding box (should span from left to right lens)
  ASSERT_TRUE(FileExists(glasses_path_));
  
  auto model_or = loader_->LoadModel(glasses_path_);
  ASSERT_TRUE(model_or.ok());
  
  auto& model = *model_or;
  
  // Glasses have lenses at (-0.04, 0, 0) and (0.04, 0, 0) with radius 0.025
  // So X should span approximately -0.065 to 0.065
  EXPECT_LT(model.bounds_min[0], -0.03f) << "Left lens should extend left";
  EXPECT_GT(model.bounds_max[0], 0.03f) << "Right lens should extend right";
  
  // Y and Z should be relatively small (lenses are thin)
  float y_span = model.bounds_max[1] - model.bounds_min[1];
  float z_span = model.bounds_max[2] - model.bounds_min[2];
  EXPECT_LT(y_span, 0.1f) << "Glasses should be relatively flat in Y";
  EXPECT_LT(z_span, 0.05f) << "Glasses should be thin in Z";
  
  // Cleanup
  loader_->UnloadModel(model);
}

// ===== Resource Management Tests =====

TEST_F(ModelLoaderTest, UnloadModel) {
  // Test that UnloadModel properly cleans up resources
  ASSERT_TRUE(FileExists(cube_path_));
  
  auto model_or = loader_->LoadModel(cube_path_);
  ASSERT_TRUE(model_or.ok());
  
  auto model = *model_or;
  
  // Verify buffers are allocated
  EXPECT_GT(model.meshes[0].vao, 0u);
  EXPECT_GT(model.meshes[0].vbo, 0u);
  EXPECT_GT(model.meshes[0].ebo, 0u);
  
  // Unload should delete OpenGL buffers
  loader_->UnloadModel(model);
  
  // After unload, buffers should be 0
  EXPECT_EQ(model.meshes[0].vao, 0u);
  EXPECT_EQ(model.meshes[0].vbo, 0u);
  EXPECT_EQ(model.meshes[0].ebo, 0u);
}

TEST_F(ModelLoaderTest, LoadMultipleModels) {
  // Test loading multiple models simultaneously
  ASSERT_TRUE(FileExists(cube_path_));
  ASSERT_TRUE(FileExists(glasses_path_));
  ASSERT_TRUE(FileExists(hat_path_));
  
  auto cube_or = loader_->LoadModel(cube_path_);
  auto glasses_or = loader_->LoadModel(glasses_path_);
  auto hat_or = loader_->LoadModel(hat_path_);
  
  ASSERT_TRUE(cube_or.ok());
  ASSERT_TRUE(glasses_or.ok());
  ASSERT_TRUE(hat_or.ok());
  
  // Verify all have unique buffers
  EXPECT_NE(cube_or->meshes[0].vao, glasses_or->meshes[0].vao);
  EXPECT_NE(glasses_or->meshes[0].vao, hat_or->meshes[0].vao);
  
  // Cleanup
  loader_->UnloadModel(*cube_or);
  loader_->UnloadModel(*glasses_or);
  loader_->UnloadModel(*hat_or);
}

// ===== Performance Tests =====

TEST_F(ModelLoaderTest, LoadingPerformance) {
  // Test that loading is reasonably fast (< 100ms for simple models)
  ASSERT_TRUE(FileExists(cube_path_));
  
  auto start = std::chrono::high_resolution_clock::now();
  auto model_or = loader_->LoadModel(cube_path_);
  auto end = std::chrono::high_resolution_clock::now();
  
  ASSERT_TRUE(model_or.ok());
  
  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
  EXPECT_LT(duration_ms, 100) << "Loading simple cube should take < 100ms, took " << duration_ms << "ms";
  
  // Cleanup
  loader_->UnloadModel(*model_or);
}

// ===== Format Support Tests =====

TEST_F(ModelLoaderTest, SupportsOBJFormat) {
  // Verify OBJ format is supported (primary format)
  ASSERT_TRUE(FileExists(cube_path_));
  
  auto model_or = loader_->LoadModel(cube_path_);
  EXPECT_TRUE(model_or.ok()) << "OBJ format should be supported";
  
  if (model_or.ok()) {
    loader_->UnloadModel(*model_or);
  }
}

// ===== Edge Cases =====

TEST_F(ModelLoaderTest, ModelWithNoMaterials) {
  // Create a temporary OBJ without materials
  std::string temp_path = "/tmp/no_materials_test.obj";
  std::ofstream out(temp_path);
  out << "# Cube without materials\n";
  out << "v -0.5 -0.5 -0.5\n";
  out << "v 0.5 -0.5 -0.5\n";
  out << "v 0.5 0.5 -0.5\n";
  out << "v -0.5 0.5 -0.5\n";
  out << "f 1 2 3\n";
  out << "f 1 3 4\n";
  out.close();
  
  auto model_or = loader_->LoadModel(temp_path);
  
  if (model_or.ok()) {
    // Should load but have 0 materials (or 1 default material)
    EXPECT_LE(model_or->materials.size(), 1)
        << "Model without MTL should have 0 or 1 default material";
    loader_->UnloadModel(*model_or);
  }
  
  // Cleanup
  fs::remove(temp_path);
}

TEST_F(ModelLoaderTest, VeryLargeModel) {
  // Create a model with many vertices (stress test)
  std::string temp_path = "/tmp/large_test.obj";
  std::ofstream out(temp_path);
  out << "# Large model with 1000 vertices\n";
  
  // Generate vertices in a grid
  for (int i = 0; i < 1000; ++i) {
    float x = (i % 32) * 0.1f;
    float y = (i / 32) * 0.1f;
    out << "v " << x << " " << y << " 0.0\n";
  }
  
  // Generate triangles
  for (int i = 0; i < 900; i += 3) {
    out << "f " << (i+1) << " " << (i+2) << " " << (i+3) << "\n";
  }
  out.close();
  
  auto model_or = loader_->LoadModel(temp_path);
  
  if (model_or.ok()) {
    EXPECT_EQ(model_or->vertex_count, 1000) << "Should have 1000 vertices";
    EXPECT_GT(model_or->triangle_count, 0) << "Should have triangles";
    loader_->UnloadModel(*model_or);
  }
  
  // Cleanup
  fs::remove(temp_path);
}

}  // namespace
}  // namespace ar_filters
}  // namespace segmecam
