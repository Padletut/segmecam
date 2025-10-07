// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// AttachmentController Unit Tests - Phase 3 Step 6
// Tests for type-agnostic filter management

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/attachment_controller.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_object.h"

#include <gtest/gtest.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace segmecam {
namespace {

// Test fixture for AttachmentController tests
class AttachmentControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    controller_ = std::make_unique<AttachmentController>();
    
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
    
    // Create sample anchors
    CreateSampleAnchors();
  }
  
  void TearDown() override {
    controller_.reset();
  }
  
  void CreateSampleAnchors() {
    // Create typical face anchors
    AnchorPoint nose;
    nose.name = "nose_bridge";
    nose.position_2d = cv::Point2f(100, 100);
    nose.position_world = cv::Vec3f(0, 0, 0);
    anchors_.push_back(nose);
    
    AnchorPoint forehead;
    forehead.name = "forehead";
    forehead.position_2d = cv::Point2f(100, 50);
    forehead.position_world = cv::Vec3f(0, 0.05, 0);
    anchors_.push_back(forehead);
    
    AnchorPoint left_eye;
    left_eye.name = "left_eye";
    left_eye.position_2d = cv::Point2f(80, 90);
    left_eye.position_world = cv::Vec3f(-0.03, 0, 0);
    anchors_.push_back(left_eye);
    
    AnchorPoint right_eye;
    right_eye.name = "right_eye";
    right_eye.position_2d = cv::Point2f(120, 90);
    right_eye.position_world = cv::Vec3f(0.03, 0, 0);
    anchors_.push_back(right_eye);
    
    AnchorPoint chin;
    chin.name = "chin";
    chin.position_2d = cv::Point2f(100, 150);
    chin.position_world = cv::Vec3f(0, -0.05, 0);
    anchors_.push_back(chin);
  }
  
  bool FileExists(const std::string& path) {
    return fs::exists(path);
  }
  
  std::unique_ptr<AttachmentController> controller_;
  std::vector<AnchorPoint> anchors_;
  HeadPose head_pose_;
  
  std::string workspace_root_;
  std::string assets_dir_;
  std::string cube_model_path_;
  std::string glasses_model_path_;
};

// ===== Basic Attachment Tests =====

TEST_F(AttachmentControllerTest, AttachPrimitive) {
  // Test attaching a primitive filter
  auto cube = CreateCube("nose_bridge", "Test Cube");
  
  std::string id = controller_->AttachFilter(cube);
  
  EXPECT_FALSE(id.empty()) << "Should return valid ID";
  EXPECT_EQ(controller_->GetFilterCount(), 1) << "Should have 1 filter";
  
  // Verify filter is accessible
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  EXPECT_EQ(filter->type, FilterObject::Type::PRIMITIVE);
  EXPECT_EQ(filter->name, "Test Cube");
}

TEST_F(AttachmentControllerTest, AttachModel) {
  // Test attaching a model filter
  if (!FileExists(cube_model_path_)) {
    GTEST_SKIP() << "Test asset not available";
  }
  
  auto model = CreateFromModel("nose_bridge", "Test Model", cube_model_path_);
  
  std::string id = controller_->AttachFilter(model);
  
  EXPECT_FALSE(id.empty());
  EXPECT_EQ(controller_->GetFilterCount(), 1);
  
  // Verify filter is accessible
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  EXPECT_EQ(filter->type, FilterObject::Type::MODEL_3D);
  EXPECT_EQ(filter->name, "Test Model");
}

TEST_F(AttachmentControllerTest, AttachMultiplePrimitives) {
  // Test attaching multiple primitive filters
  auto cube = CreateCube("nose_bridge", "Cube");
  auto sphere = CreateSphere("forehead", "Sphere");
  auto cylinder = CreateCylinder("left_eye", "Cylinder");
  
  std::string cube_id = controller_->AttachFilter(cube);
  std::string sphere_id = controller_->AttachFilter(sphere);
  std::string cylinder_id = controller_->AttachFilter(cylinder);
  
  EXPECT_EQ(controller_->GetFilterCount(), 3);
  
  // All IDs should be unique
  EXPECT_NE(cube_id, sphere_id);
  EXPECT_NE(sphere_id, cylinder_id);
  EXPECT_NE(cube_id, cylinder_id);
}

TEST_F(AttachmentControllerTest, AttachMixedTypes) {
  // Test attaching both primitives and models
  auto cube = CreateCube("left_eye", "Cube");
  
  std::string cube_id = controller_->AttachFilter(cube);
  
  if (FileExists(glasses_model_path_)) {
    auto glasses = CreateFromModel("nose_bridge", "Glasses", glasses_model_path_);
    std::string glasses_id = controller_->AttachFilter(glasses);
    
    EXPECT_EQ(controller_->GetFilterCount(), 2);
    EXPECT_NE(cube_id, glasses_id);
    
    // Verify types
    const auto* cube_filter = controller_->GetFilter(cube_id);
    const auto* glasses_filter = controller_->GetFilter(glasses_id);
    
    ASSERT_NE(cube_filter, nullptr);
    ASSERT_NE(glasses_filter, nullptr);
    
    EXPECT_EQ(cube_filter->type, FilterObject::Type::PRIMITIVE);
    EXPECT_EQ(glasses_filter->type, FilterObject::Type::MODEL_3D);
  }
}

// ===== Detachment Tests =====

TEST_F(AttachmentControllerTest, DetachFilter) {
  // Test detaching a filter
  auto cube = CreateCube("nose_bridge", "Cube");
  std::string id = controller_->AttachFilter(cube);
  
  EXPECT_EQ(controller_->GetFilterCount(), 1);
  
  bool removed = controller_->DetachFilter(id);
  
  EXPECT_TRUE(removed);
  EXPECT_EQ(controller_->GetFilterCount(), 0);
  
  // Filter should no longer be accessible
  EXPECT_EQ(controller_->GetFilter(id), nullptr);
}

TEST_F(AttachmentControllerTest, DetachNonexistent) {
  // Test detaching a filter that doesn't exist
  bool removed = controller_->DetachFilter("nonexistent_id");
  
  EXPECT_FALSE(removed);
}

TEST_F(AttachmentControllerTest, DetachAll) {
  // Test detaching all filters
  controller_->AttachFilter(CreateCube("nose_bridge", "Cube1"));
  controller_->AttachFilter(CreateSphere("forehead", "Sphere"));
  controller_->AttachFilter(CreateCylinder("left_eye", "Cylinder"));
  
  EXPECT_EQ(controller_->GetFilterCount(), 3);
  
  controller_->DetachAll();
  
  EXPECT_EQ(controller_->GetFilterCount(), 0);
}

// ===== Transform Update Tests =====

TEST_F(AttachmentControllerTest, UpdateTransformsPrimitive) {
  // Test updating transforms for primitive filter
  auto cube = CreateCube("nose_bridge", "Cube");
  std::string id = controller_->AttachFilter(cube);
  
  // Update transforms
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  // Get updated filter
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  
  // Filter should be visible (anchor exists)
  EXPECT_TRUE(filter->visible);
  
  // Position should be updated to anchor position (100, 100)
  EXPECT_FLOAT_EQ(filter->position[0], 100.0f);
  EXPECT_FLOAT_EQ(filter->position[1], 100.0f);
  
  // Frame count should increment
  EXPECT_GT(filter->frame_count, 0);
}

TEST_F(AttachmentControllerTest, UpdateTransformsModel) {
  // Test updating transforms for model filter
  if (!FileExists(cube_model_path_)) {
    GTEST_SKIP() << "Test asset not available";
  }
  
  auto model = CreateFromModel("nose_bridge", "Model", cube_model_path_);
  std::string id = controller_->AttachFilter(model);
  
  // Update transforms
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  // Get updated filter
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  
  // Should work exactly like primitive
  EXPECT_TRUE(filter->visible);
  EXPECT_FLOAT_EQ(filter->position[0], 100.0f);
  EXPECT_FLOAT_EQ(filter->position[1], 100.0f);
}

TEST_F(AttachmentControllerTest, UpdateTransformsMixedTypes) {
  // Test updating transforms for both types simultaneously
  auto cube = CreateCube("left_eye", "Cube");
  std::string cube_id = controller_->AttachFilter(cube);
  
  if (FileExists(glasses_model_path_)) {
    auto glasses = CreateFromModel("nose_bridge", "Glasses", glasses_model_path_);
    std::string glasses_id = controller_->AttachFilter(glasses);
    
    // Update both
    controller_->UpdateFilterTransforms(anchors_, head_pose_);
    
    // Both should be visible
    const auto* cube_filter = controller_->GetFilter(cube_id);
    const auto* glasses_filter = controller_->GetFilter(glasses_id);
    
    ASSERT_NE(cube_filter, nullptr);
    ASSERT_NE(glasses_filter, nullptr);
    
    EXPECT_TRUE(cube_filter->visible);
    EXPECT_TRUE(glasses_filter->visible);
    
    // Cube should be at left_eye position (80, 90)
    EXPECT_FLOAT_EQ(cube_filter->position[0], 80.0f);
    EXPECT_FLOAT_EQ(cube_filter->position[1], 90.0f);
    
    // Glasses should be at nose_bridge position (100, 100)
    EXPECT_FLOAT_EQ(glasses_filter->position[0], 100.0f);
    EXPECT_FLOAT_EQ(glasses_filter->position[1], 100.0f);
  }
}

TEST_F(AttachmentControllerTest, UpdateWithMissingAnchor) {
  // Test behavior when anchor doesn't exist
  auto cube = CreateCube("nonexistent_anchor", "Cube");
  std::string id = controller_->AttachFilter(cube);
  
  // Update transforms with anchors that don't include "nonexistent_anchor"
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  
  // Filter should be hidden (anchor not found)
  EXPECT_FALSE(filter->visible);
}

TEST_F(AttachmentControllerTest, UpdateWithOffset) {
  // Test that offset is applied correctly
  auto cube = CreateCube("nose_bridge", "Cube");
  cube.offset = cv::Vec3f(10.0f, 20.0f, 5.0f);
  
  std::string id = controller_->AttachFilter(cube);
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  
  // Position should be anchor + offset
  EXPECT_FLOAT_EQ(filter->position[0], 110.0f);  // 100 + 10
  EXPECT_FLOAT_EQ(filter->position[1], 120.0f);  // 100 + 20
  EXPECT_FLOAT_EQ(filter->position[2], 5.0f);    // offset Z
}

TEST_F(AttachmentControllerTest, UpdateDisabledFilter) {
  // Test that disabled filters are skipped
  auto cube = CreateCube("nose_bridge", "Cube");
  cube.enabled = false;
  
  std::string id = controller_->AttachFilter(cube);
  
  // Get initial frame count
  const auto* filter_before = controller_->GetFilter(id);
  ASSERT_NE(filter_before, nullptr);
  int frame_count_before = filter_before->frame_count;
  
  // Update transforms
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  // Frame count should not change (filter was skipped)
  const auto* filter_after = controller_->GetFilter(id);
  EXPECT_EQ(filter_after->frame_count, frame_count_before);
}

// ===== Access Tests =====

TEST_F(AttachmentControllerTest, GetActiveFilters) {
  // Test GetActiveFilters returns all attached filters
  controller_->AttachFilter(CreateCube("nose_bridge", "Cube1"));
  controller_->AttachFilter(CreateSphere("forehead", "Sphere"));
  controller_->AttachFilter(CreateCylinder("left_eye", "Cylinder"));
  
  const auto& filters = controller_->GetActiveFilters();
  
  EXPECT_EQ(filters.size(), 3);
}

TEST_F(AttachmentControllerTest, GetFilterById) {
  // Test GetFilter by ID
  auto cube = CreateCube("nose_bridge", "Cube");
  std::string id = controller_->AttachFilter(cube);
  
  const auto* filter = controller_->GetFilter(id);
  
  ASSERT_NE(filter, nullptr);
  EXPECT_EQ(filter->id, id);
  EXPECT_EQ(filter->name, "Cube");
}

TEST_F(AttachmentControllerTest, GetFilterMutable) {
  // Test mutable filter access
  auto cube = CreateCube("nose_bridge", "Cube");
  std::string id = controller_->AttachFilter(cube);
  
  auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  
  // Modify filter
  filter->scale = cv::Vec3f(2.0f, 2.0f, 2.0f);
  
  // Verify modification
  const auto* filter_const = controller_->GetFilter(id);
  EXPECT_FLOAT_EQ(filter_const->scale[0], 2.0f);
}

// ===== Configuration Tests =====

TEST_F(AttachmentControllerTest, SetGlobalScale) {
  // Test global scale setting
  controller_->SetGlobalScale(2.0f);
  
  EXPECT_FLOAT_EQ(controller_->GetGlobalScale(), 2.0f);
}

TEST_F(AttachmentControllerTest, SetAllFiltersEnabled) {
  // Test enabling/disabling all filters
  controller_->AttachFilter(CreateCube("nose_bridge", "Cube1"));
  controller_->AttachFilter(CreateSphere("forehead", "Sphere"));
  
  // Disable all
  controller_->SetAllFiltersEnabled(false);
  
  const auto& filters = controller_->GetActiveFilters();
  for (const auto& filter : filters) {
    EXPECT_FALSE(filter.enabled);
  }
  
  // Enable all
  controller_->SetAllFiltersEnabled(true);
  
  for (const auto& filter : filters) {
    EXPECT_TRUE(filter.enabled);
  }
}

TEST_F(AttachmentControllerTest, SetAllFiltersVisible) {
  // Test showing/hiding all filters
  controller_->AttachFilter(CreateCube("nose_bridge", "Cube1"));
  controller_->AttachFilter(CreateSphere("forehead", "Sphere"));
  
  // Hide all
  controller_->SetAllFiltersVisible(false);
  
  const auto& filters = controller_->GetActiveFilters();
  for (const auto& filter : filters) {
    EXPECT_FALSE(filter.visible);
  }
  
  // Show all
  controller_->SetAllFiltersVisible(true);
  
  for (const auto& filter : filters) {
    EXPECT_TRUE(filter.visible);
  }
}

// ===== Statistics Tests =====

TEST_F(AttachmentControllerTest, GetStatistics) {
  // Test statistics gathering
  controller_->AttachFilter(CreateCube("nose_bridge", "Cube1"));
  controller_->AttachFilter(CreateSphere("forehead", "Sphere"));
  
  auto stats = controller_->GetStatistics();
  
  EXPECT_EQ(stats.total_filters, 2);
  EXPECT_EQ(stats.enabled_filters, 2);  // All enabled by default
  EXPECT_GE(stats.visible_filters, 0);  // May not be visible until update
}

TEST_F(AttachmentControllerTest, StatisticsAfterUpdate) {
  // Test statistics after transform update
  controller_->AttachFilter(CreateCube("nose_bridge", "Cube1"));
  controller_->AttachFilter(CreateSphere("forehead", "Sphere"));
  controller_->AttachFilter(CreateCylinder("left_eye", "Cylinder"));
  
  // Update transforms
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  auto stats = controller_->GetStatistics();
  
  EXPECT_EQ(stats.total_filters, 3);
  EXPECT_EQ(stats.enabled_filters, 3);
  EXPECT_EQ(stats.visible_filters, 3);  // All should be visible after update
  EXPECT_EQ(stats.filters_with_valid_anchors, 3);
}

// ===== Edge Cases =====

TEST_F(AttachmentControllerTest, AttachWithEmptyAnchorName) {
  // Test attaching filter with empty anchor name
  auto cube = CreateCube("", "Invalid");
  
  std::string id = controller_->AttachFilter(cube);
  
  EXPECT_TRUE(id.empty()) << "Should reject filter with empty anchor";
  EXPECT_EQ(controller_->GetFilterCount(), 0);
}

TEST_F(AttachmentControllerTest, UpdateWithEmptyAnchors) {
  // Test updating with no anchors
  auto cube = CreateCube("nose_bridge", "Cube");
  std::string id = controller_->AttachFilter(cube);
  
  std::vector<AnchorPoint> empty_anchors;
  controller_->UpdateFilterTransforms(empty_anchors, head_pose_);
  
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  
  // Filter should be hidden (no anchors available)
  EXPECT_FALSE(filter->visible);
}

TEST_F(AttachmentControllerTest, ManyFilters) {
  // Test with many filters (stress test)
  const int num_filters = 100;
  
  for (int i = 0; i < num_filters; ++i) {
    std::string name = "Filter" + std::to_string(i);
    // Alternate anchor names
    std::string anchor = (i % 2 == 0) ? "nose_bridge" : "forehead";
    controller_->AttachFilter(CreateCube(anchor, name));
  }
  
  EXPECT_EQ(controller_->GetFilterCount(), num_filters);
  
  // Update all
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  // All should be visible
  const auto& filters = controller_->GetActiveFilters();
  int visible_count = 0;
  for (const auto& filter : filters) {
    if (filter.visible) ++visible_count;
  }
  
  EXPECT_EQ(visible_count, num_filters);
}

// ===== Type-Agnostic Verification =====

TEST_F(AttachmentControllerTest, WorksWithPrimitives) {
  // Comprehensive test: AttachmentController works perfectly with primitives
  auto cube = CreateCube("nose_bridge", "Test Cube");
  ASSERT_EQ(cube.type, FilterObject::Type::PRIMITIVE);
  
  std::string id = controller_->AttachFilter(cube);
  EXPECT_FALSE(id.empty());
  EXPECT_EQ(controller_->GetFilterCount(), 1);
  
  // Update transforms
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  // Verify filter is visible and positioned
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  EXPECT_TRUE(filter->visible);
  EXPECT_FLOAT_EQ(filter->position[0], 100.0f);
  EXPECT_FLOAT_EQ(filter->position[1], 100.0f);
}

TEST_F(AttachmentControllerTest, WorksWithModels) {
  // Comprehensive test: AttachmentController works perfectly with models
  if (!FileExists(glasses_model_path_)) {
    GTEST_SKIP() << "Test asset not available";
  }
  
  auto glasses = CreateFromModel("nose_bridge", "Test Glasses", glasses_model_path_);
  ASSERT_EQ(glasses.type, FilterObject::Type::MODEL_3D);
  
  std::string id = controller_->AttachFilter(glasses);
  EXPECT_FALSE(id.empty());
  EXPECT_EQ(controller_->GetFilterCount(), 1);
  
  // Update transforms
  controller_->UpdateFilterTransforms(anchors_, head_pose_);
  
  // Verify filter is visible and positioned (same as primitive!)
  const auto* filter = controller_->GetFilter(id);
  ASSERT_NE(filter, nullptr);
  EXPECT_TRUE(filter->visible);
  EXPECT_FLOAT_EQ(filter->position[0], 100.0f);
  EXPECT_FLOAT_EQ(filter->position[1], 100.0f);
}

}  // namespace
}  // namespace segmecam
