// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// FilterAsset Implementation - JSON parsing and asset validation

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_asset.h"

#include <fstream>
#include <filesystem>
#include "absl/log/absl_log.h"
#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace segmecam {
namespace ar_filters {

// Static factory methods

absl::StatusOr<FilterAsset> FilterAsset::LoadFromDirectory(
    const std::string& filter_dir) {
  std::string json_path = filter_dir + "/filter.json";
  return LoadFromFile(json_path);
}

absl::StatusOr<FilterAsset> FilterAsset::LoadFromFile(
    const std::string& json_path) {
  // Check if file exists
  if (!fs::exists(json_path)) {
    return absl::NotFoundError(
        absl::StrFormat("Filter JSON not found: %s", json_path));
  }
  
  // Read JSON file
  std::ifstream file(json_path);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open filter JSON: %s", json_path));
  }
  
  json j;
  try {
    file >> j;
  } catch (const json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("JSON parse error in %s: %s", json_path, e.what()));
  }
  
  FilterAsset asset;
  asset.filter_directory_ = fs::path(json_path).parent_path().string();
  
  // Parse each section
  auto metadata_status = ParseMetadata(j, asset.metadata_);
  if (!metadata_status.ok()) {
    return metadata_status;
  }
  
  auto attachments_status = ParseAttachments(j, asset.attachments_);
  if (!attachments_status.ok()) {
    return attachments_status;
  }
  
  auto materials_status = ParseMaterials(j, asset.materials_);
  if (!materials_status.ok()) {
    return materials_status;
  }
  
  auto behaviors_status = ParseBehaviors(j, asset.behaviors_);
  if (!behaviors_status.ok()) {
    return behaviors_status;
  }
  
  // Validate assets exist
  auto validation_status = asset.ValidateAssets();
  if (!validation_status.ok()) {
    ABSL_LOG(WARNING) << "Filter asset validation warnings: " 
                      << validation_status.message();
  }
  
  ABSL_LOG(INFO) << "Loaded filter: " << asset.metadata_.name 
                 << " (" << asset.metadata_.id << ")";
  
  return asset;
}

std::vector<std::string> FilterAsset::EnumerateFilters(
    const std::string& filters_root) {
  std::vector<std::string> filter_dirs;
  
  if (!fs::exists(filters_root) || !fs::is_directory(filters_root)) {
    ABSL_LOG(WARNING) << "Filters root directory not found: " << filters_root;
    return filter_dirs;
  }
  
  // Search for subdirectories containing filter.json
  for (const auto& entry : fs::directory_iterator(filters_root)) {
    if (entry.is_directory()) {
      std::string json_path = entry.path().string() + "/filter.json";
      if (fs::exists(json_path)) {
        filter_dirs.push_back(entry.path().string());
      }
    }
  }
  
  ABSL_LOG(INFO) << "Found " << filter_dirs.size() << " filters in " 
                 << filters_root;
  
  return filter_dirs;
}

// Asset validation

absl::Status FilterAsset::ValidateAssets() const {
  std::vector<std::string> missing_files;
  
  // Note: Thumbnail and icon are optional UI assets - not validated
  
  // Check attachment assets (required)
  for (const auto& attachment : attachments_) {
    // Check model file
    std::string model_path = GetAssetPath(attachment.model_path);
    if (!fs::exists(model_path)) {
      missing_files.push_back(model_path);
    }
    
    // Check texture file (optional)
    if (!attachment.texture_path.empty()) {
      std::string texture_path = GetAssetPath(attachment.texture_path);
      if (!fs::exists(texture_path)) {
        missing_files.push_back(texture_path);
      }
    }
  }
  
  if (!missing_files.empty()) {
    std::string error_msg = "Missing asset files:\n";
    for (const auto& file : missing_files) {
      error_msg += "  - " + file + "\n";
    }
    return absl::FailedPreconditionError(error_msg);
  }
  
  return absl::OkStatus();
}

std::string FilterAsset::GetAssetPath(const std::string& relative_path) const {
  if (relative_path.empty()) {
    return "";
  }
  
  // If already absolute, return as-is
  fs::path p(relative_path);
  if (p.is_absolute()) {
    return relative_path;
  }
  
  // Otherwise resolve relative to filter directory
  return (fs::path(filter_directory_) / relative_path).string();
}

// JSON parsing implementations

absl::Status FilterAsset::ParseMetadata(const json& j,
                                        FilterMetadata& metadata) {
  if (!j.contains("filter")) {
    return absl::InvalidArgumentError("Missing required 'filter' section");
  }
  
  const json& filter = j["filter"];
  
  // Required fields
  if (!filter.contains("name")) {
    return absl::InvalidArgumentError("Missing required field: filter.name");
  }
  metadata.name = filter["name"].get<std::string>();
  
  if (!filter.contains("id")) {
    return absl::InvalidArgumentError("Missing required field: filter.id");
  }
  metadata.id = filter["id"].get<std::string>();
  
  if (!filter.contains("version")) {
    return absl::InvalidArgumentError("Missing required field: filter.version");
  }
  metadata.version = filter["version"].get<std::string>();
  
  // Optional fields with defaults
  metadata.category = filter.value("category", "other");
  metadata.author = filter.value("author", "Unknown");
  metadata.description = filter.value("description", "");
  metadata.thumbnail_path = filter.value("thumbnail", "");
  metadata.icon_path = filter.value("icon", "");
  
  return absl::OkStatus();
}

absl::Status FilterAsset::ParseAttachments(const json& j,
                                           std::vector<FilterAttachment>& attachments) {
  if (!j.contains("attachments")) {
    return absl::InvalidArgumentError("Missing required 'attachments' section");
  }
  
  const json& attachments_json = j["attachments"];
  if (!attachments_json.is_array()) {
    return absl::InvalidArgumentError("'attachments' must be an array");
  }
  
  if (attachments_json.empty()) {
    return absl::InvalidArgumentError("'attachments' array cannot be empty");
  }
  
  for (const auto& att_json : attachments_json) {
    FilterAttachment attachment;
    
    // Required fields
    if (!att_json.contains("id")) {
      return absl::InvalidArgumentError("Missing required field: attachment.id");
    }
    attachment.id = att_json["id"].get<std::string>();
    
    if (!att_json.contains("anchor")) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Missing required field: attachment[%s].anchor", 
                         attachment.id));
    }
    attachment.anchor_name = att_json["anchor"].get<std::string>();
    
    if (!att_json.contains("model")) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Missing required field: attachment[%s].model",
                         attachment.id));
    }
    attachment.model_path = att_json["model"].get<std::string>();
    
    // Optional fields
    attachment.texture_path = att_json.value("texture", "");
    attachment.visible = att_json.value("visible", true);
    attachment.opacity = att_json.value("opacity", 1.0f);
    
    // Parse vec3 fields
    if (att_json.contains("scale")) {
      auto scale = ParseVec3(att_json, "scale");
      if (!scale.ok()) return scale.status();
      attachment.scale = *scale;
    }
    
    if (att_json.contains("offset")) {
      auto offset = ParseVec3(att_json, "offset");
      if (!offset.ok()) return offset.status();
      attachment.offset = *offset;
    }
    
    if (att_json.contains("rotation")) {
      auto rotation = ParseVec3(att_json, "rotation");
      if (!rotation.ok()) return rotation.status();
      attachment.rotation = *rotation;
    }
    
    attachments.push_back(std::move(attachment));
  }
  
  return absl::OkStatus();
}

absl::Status FilterAsset::ParseMaterials(const json& j,
                                         std::map<std::string, FilterMaterial>& materials) {
  // Materials section is optional
  if (!j.contains("materials")) {
    return absl::OkStatus();
  }
  
  const json& materials_json = j["materials"];
  if (!materials_json.is_object()) {
    return absl::InvalidArgumentError("'materials' must be an object");
  }
  
  for (auto it = materials_json.begin(); it != materials_json.end(); ++it) {
    const std::string& material_id = it.key();
    const json& mat_json = it.value();
    
    FilterMaterial material;
    
    // All fields are optional with defaults
    if (mat_json.contains("ambient")) {
      auto ambient = ParseVec3(mat_json, "ambient");
      if (!ambient.ok()) return ambient.status();
      material.ambient = *ambient;
    }
    
    if (mat_json.contains("diffuse")) {
      auto diffuse = ParseVec3(mat_json, "diffuse");
      if (!diffuse.ok()) return diffuse.status();
      material.diffuse = *diffuse;
    }
    
    if (mat_json.contains("specular")) {
      auto specular = ParseVec3(mat_json, "specular");
      if (!specular.ok()) return specular.status();
      material.specular = *specular;
    }
    
    material.shininess = mat_json.value("shininess", 32.0f);
    material.opacity = mat_json.value("opacity", 1.0f);
    
    materials[material_id] = material;
  }
  
  return absl::OkStatus();
}

absl::Status FilterAsset::ParseBehaviors(const json& j,
                                         std::vector<FilterBehavior>& behaviors) {
  // Behaviors section is optional
  if (!j.contains("behaviors")) {
    return absl::OkStatus();
  }
  
  const json& behaviors_json = j["behaviors"];
  if (!behaviors_json.is_object()) {
    return absl::InvalidArgumentError("'behaviors' must be an object");
  }
  
  for (auto it = behaviors_json.begin(); it != behaviors_json.end(); ++it) {
    const std::string& behavior_id = it.key();
    const json& behavior_json = it.value();
    
    FilterBehavior behavior;
    
    // Required: type
    if (!behavior_json.contains("type")) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Missing required field: behaviors[%s].type",
                         behavior_id));
    }
    
    std::string type_str = behavior_json["type"].get<std::string>();
    auto type_result = ParseBehaviorType(type_str);
    if (!type_result.ok()) {
      return type_result.status();
    }
    behavior.type = *type_result;
    
    // Required: target
    if (!behavior_json.contains("target")) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Missing required field: behaviors[%s].target",
                         behavior_id));
    }
    behavior.target_attachment_id = behavior_json["target"].get<std::string>();
    
    // Required: blendshape
    if (!behavior_json.contains("blendshape")) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Missing required field: behaviors[%s].blendshape",
                         behavior_id));
    }
    behavior.blendshape_name = behavior_json["blendshape"].get<std::string>();
    
    // Optional fields
    behavior.threshold = behavior_json.value("threshold", 0.5f);
    behavior.intensity = behavior_json.value("intensity", 1.0f);
    behavior.gravity = behavior_json.value("gravity", 9.8f);
    
    // Type-specific parameters
    if (behavior_json.contains("target_scale")) {
      auto scale = ParseVec3(behavior_json, "target_scale");
      if (!scale.ok()) return scale.status();
      behavior.target_scale = *scale;
    }
    
    if (behavior_json.contains("target_rotation")) {
      auto rotation = ParseVec3(behavior_json, "target_rotation");
      if (!rotation.ok()) return rotation.status();
      behavior.target_rotation = *rotation;
    }
    
    if (behavior_json.contains("target_color")) {
      auto color = ParseVec3(behavior_json, "target_color");
      if (!color.ok()) return color.status();
      behavior.target_color = *color;
    }
    
    behaviors.push_back(behavior);
  }
  
  return absl::OkStatus();
}

// Helper functions

absl::StatusOr<glm::vec3> FilterAsset::ParseVec3(const json& j,
                                                  const std::string& field_name) {
  if (!j[field_name].is_array()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Field '%s' must be an array", field_name));
  }
  
  const json& arr = j[field_name];
  if (arr.size() != 3) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Field '%s' must have exactly 3 elements (got %d)",
                       field_name, arr.size()));
  }
  
  try {
    return glm::vec3(
        arr[0].get<float>(),
        arr[1].get<float>(),
        arr[2].get<float>()
    );
  } catch (const json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse '%s' as vec3: %s", 
                       field_name, e.what()));
  }
}

absl::StatusOr<FilterBehavior::Type> FilterAsset::ParseBehaviorType(
    const std::string& type_str) {
  if (type_str == "shake") {
    return FilterBehavior::Type::SHAKE;
  } else if (type_str == "fall_off") {
    return FilterBehavior::Type::FALL_OFF;
  } else if (type_str == "scale") {
    return FilterBehavior::Type::SCALE;
  } else if (type_str == "rotate") {
    return FilterBehavior::Type::ROTATE;
  } else if (type_str == "hide") {
    return FilterBehavior::Type::HIDE;
  } else if (type_str == "color_change") {
    return FilterBehavior::Type::COLOR_CHANGE;
  } else {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unknown behavior type: '%s'", type_str));
  }
}

} // namespace ar_filters
} // namespace segmecam
