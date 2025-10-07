// Copyright 2025 SegmeCam Project
// Licensed under the Apache License, Version 2.0

#include "include/ar_filters/filter_presets.h"
#include "include/ar_filters/attachment_controller.h"
#include "include/ar_filters/filter_object.h"
#include <cmath>
#include <iostream>

namespace segmecam {

FilterPresetManager::FilterPresetManager()
    : current_preset_(FilterPreset::None), preset_active_(false) {}

void FilterPresetManager::ApplyPreset(FilterPreset preset,
                                      AttachmentController& controller,
                                      bool& enabled) {
  // Clear any existing preset first
  ClearCurrentPreset(controller, enabled);

  // Apply new preset
  current_preset_ = preset;
  preset_active_ = true;
  enabled = true;

  switch (preset) {
    case FilterPreset::ClassicGlasses:
      CreateClassicGlasses(controller);
      break;
    case FilterPreset::PartyHat:
      CreatePartyHat(controller);
      break;
    case FilterPreset::FaceMask:
      CreateFaceMask(controller);
      break;
    case FilterPreset::TestDemo:
      // TestDemo requires FilterTestDemo class - cannot be created here
      // User should use the "Enable Filter Test" checkbox instead
      std::cout << "[FilterPresetManager] Test Demo preset requires using 'Enable Filter Test' checkbox" << std::endl;
      preset_active_ = false;
      enabled = false;
      break;
    case FilterPreset::None:
    default:
      preset_active_ = false;
      enabled = false;
      break;
  }
}

void FilterPresetManager::ClearCurrentPreset(AttachmentController& controller,
                                              bool& enabled) {
  // Remove all filters from current preset
  for (const auto& filter_id : active_filter_ids_) {
    controller.DetachFilter(filter_id);
  }

  active_filter_ids_.clear();
  current_preset_ = FilterPreset::None;
  preset_active_ = false;
  enabled = false;
}

std::string FilterPresetManager::GetPresetName(FilterPreset preset) const {
  switch (preset) {
    case FilterPreset::None:
      return "None";
    case FilterPreset::ClassicGlasses:
      return "Classic Glasses";
    case FilterPreset::PartyHat:
      return "Party Hat";
    case FilterPreset::FaceMask:
      return "Face Mask";
    case FilterPreset::TestDemo:
      return "Test Demo (7 Filters)";
    default:
      return "Unknown";
  }
}

std::string FilterPresetManager::GetPresetDescription(
    FilterPreset preset) const {
  switch (preset) {
    case FilterPreset::None:
      return "No filter active";
    case FilterPreset::ClassicGlasses:
      return "Simple glasses with frames on eyes and bridge";
    case FilterPreset::PartyHat:
      return "Cone-shaped party hat with pom-pom on top";
    case FilterPreset::FaceMask:
      return "Protective face mask covering nose and mouth";
    case FilterPreset::TestDemo:
      return "Test demo with 7 colored primitives";
    default:
      return "";
  }
}

int FilterPresetManager::GetPresetFilterCount(FilterPreset preset) const {
  switch (preset) {
    case FilterPreset::None:
      return 0;
    case FilterPreset::ClassicGlasses:
      return 3;  // 2 lenses + 1 bridge
    case FilterPreset::PartyHat:
      return 2;  // 1 hat + 1 pom-pom
    case FilterPreset::FaceMask:
      return 5;  // 3 main sections + 2 side edges
    case FilterPreset::TestDemo:
      return 7;
    default:
      return 0;
  }
}

std::vector<FilterPreset> FilterPresetManager::GetAllPresets() const {
  return {FilterPreset::None, FilterPreset::ClassicGlasses,
          FilterPreset::PartyHat, FilterPreset::FaceMask,
          FilterPreset::TestDemo};
}

// ============================================================================
// PRESET IMPLEMENTATIONS
// ============================================================================

void FilterPresetManager::CreateClassicGlasses(
    AttachmentController& controller) {
  active_filter_ids_.clear();

  // Left lens frame - cylinder positioned on left eye
  FilterObject left_lens = CreateCylinder("left_eye", "Glasses Left Lens", 15.0f, 8.0f, 16);
  left_lens.color = cv::Vec4f(0.1f, 0.1f, 0.1f, 0.3f);  // Dark gray with alpha
  left_lens.alpha = 0.3f;  // Semi-transparent
  left_lens.offset = cv::Vec3f(0.0f, 0.0f, 0.0f);
  std::string left_id = controller.AttachFilter(left_lens);
  active_filter_ids_.push_back(left_id);

  // Right lens frame - cylinder positioned on right eye
  FilterObject right_lens = CreateCylinder("right_eye", "Glasses Right Lens", 15.0f, 8.0f, 16);
  right_lens.color = cv::Vec4f(0.1f, 0.1f, 0.1f, 0.3f);
  right_lens.alpha = 0.3f;
  right_lens.offset = cv::Vec3f(0.0f, 0.0f, 0.0f);
  std::string right_id = controller.AttachFilter(right_lens);
  active_filter_ids_.push_back(right_id);

  // Bridge piece - connects the two lenses
  FilterObject bridge = CreateCylinder("nose_bridge", "Glasses Bridge", 5.0f, 15.0f, 8);
  bridge.color = cv::Vec4f(0.1f, 0.1f, 0.1f, 0.8f);
  bridge.alpha = 0.8f;  // More opaque for visibility
  bridge.offset = cv::Vec3f(0.0f, 0.0f, 0.0f);
  std::string bridge_id = controller.AttachFilter(bridge);
  active_filter_ids_.push_back(bridge_id);
}

void FilterPresetManager::CreatePartyHat(AttachmentController& controller) {
  active_filter_ids_.clear();

  // Hat cone - large cone positioned above forehead
  FilterObject hat = CreateCone("forehead", "Party Hat", 40.0f, 60.0f, 16);
  hat.color = cv::Vec4f(1.0f, 0.2f, 0.8f, 0.85f);  // Pink/magenta with alpha
  hat.alpha = 0.85f;
  hat.offset = cv::Vec3f(0.0f, -30.0f, 0.0f);  // Above forehead
  std::string hat_id = controller.AttachFilter(hat);
  active_filter_ids_.push_back(hat_id);

  // Pom-pom on top - sphere at the tip of the cone
  FilterObject pompom = CreateSphere("forehead", "Pom-pom", 12, 8.0f);
  pompom.color = cv::Vec4f(1.0f, 1.0f, 0.0f, 0.9f);  // Yellow with alpha
  pompom.alpha = 0.9f;
  pompom.offset = cv::Vec3f(0.0f, -90.0f, 0.0f);  // Top of hat
  std::string pompom_id = controller.AttachFilter(pompom);
  active_filter_ids_.push_back(pompom_id);
}

void FilterPresetManager::CreateFaceMask(AttachmentController& controller) {
  active_filter_ids_.clear();

  // Top section - covers nose area
  FilterObject mask_top = CreateCube("nose_bridge", "Mask Top", 50.0f);
  mask_top.color = cv::Vec4f(0.4f, 0.6f, 0.9f, 0.85f);  // Light blue with alpha
  mask_top.alpha = 0.85f;
  mask_top.offset = cv::Vec3f(0.0f, 10.0f, 0.0f);
  std::string top_id = controller.AttachFilter(mask_top);
  active_filter_ids_.push_back(top_id);

  // Middle section - main mask body
  FilterObject mask_middle = CreateCube("nose_bridge", "Mask Middle", 55.0f);
  mask_middle.color = cv::Vec4f(0.4f, 0.6f, 0.9f, 0.85f);
  mask_middle.alpha = 0.85f;
  mask_middle.offset = cv::Vec3f(0.0f, 30.0f, 0.0f);
  std::string middle_id = controller.AttachFilter(mask_middle);
  active_filter_ids_.push_back(middle_id);

  // Bottom section - chin area
  FilterObject mask_bottom = CreateCube("chin", "Mask Bottom", 50.0f);
  mask_bottom.color = cv::Vec4f(0.4f, 0.6f, 0.9f, 0.85f);
  mask_bottom.alpha = 0.85f;
  mask_bottom.offset = cv::Vec3f(0.0f, -10.0f, 0.0f);
  std::string bottom_id = controller.AttachFilter(mask_bottom);
  active_filter_ids_.push_back(bottom_id);

  // Left edge - covers left cheek area
  FilterObject mask_left = CreateCube("left_cheek", "Mask Left", 20.0f);
  mask_left.color = cv::Vec4f(0.4f, 0.6f, 0.9f, 0.85f);
  mask_left.alpha = 0.85f;
  mask_left.offset = cv::Vec3f(0.0f, 15.0f, 0.0f);
  std::string left_id = controller.AttachFilter(mask_left);
  active_filter_ids_.push_back(left_id);

  // Right edge - covers right cheek area
  FilterObject mask_right = CreateCube("right_cheek", "Mask Right", 20.0f);
  mask_right.color = cv::Vec4f(0.4f, 0.6f, 0.9f, 0.85f);
  mask_right.alpha = 0.85f;
  mask_right.offset = cv::Vec3f(0.0f, 15.0f, 0.0f);
  std::string right_id = controller.AttachFilter(mask_right);
  active_filter_ids_.push_back(right_id);
}

}  // namespace segmecam
