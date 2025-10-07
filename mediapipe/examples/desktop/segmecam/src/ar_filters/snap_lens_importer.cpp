// Copyright 2025 SegmeCam Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// Snapchat Lens Importer Implementation

#include "include/ar_filters/snap_lens_importer.h"
#include "absl/log/absl_log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <nlohmann/json.hpp>

namespace segmecam {
namespace ar_filters {

namespace fs = std::filesystem;
using json = nlohmann::json;

absl::StatusOr<FilterAsset> SnapLensImporter::ImportLens(const std::string& lens_dir) {
  ABSL_LOG(INFO) << "🔍 Importing Snapchat lens from: " << lens_dir;

  // Step 1: Parse scene XML
  std::string xml_path = fs::path(lens_dir) / "_scene_scn.xml";
  if (!fs::exists(xml_path)) {
    return absl::NotFoundError(absl::StrCat("Scene XML not found: ", xml_path));
  }

  auto scene_result = ParseSceneXML(xml_path);
  if (!scene_result.ok()) {
    return scene_result.status();
  }
  SnapLensScene scene = *scene_result;

  ABSL_LOG(INFO) << "📦 Found " << scene.textures.size() << " textures, "
                 << scene.materials.size() << " materials";

  // Step 2: Analyze compatibility
  auto compat = AnalyzeCompatibility(scene);
  ABSL_LOG(INFO) << "⚖️  Compatibility: " << (compat.percentage * 100) << "%";
  
  for (const auto& warning : compat.warnings) {
    ABSL_LOG(WARNING) << "  ⚠️  " << warning;
  }

  // Step 3: Create FilterAsset from scene
  FilterAsset filter = CreateFilterFromScene(scene, lens_dir);

  // Step 4: Extract and copy texture files
  std::string output_dir = fs::path(lens_dir).parent_path() / "snap-lens-converted";
  fs::create_directories(output_dir);
  
  auto copy_status = CopyTextureFiles(lens_dir, output_dir, scene.textures);
  if (!copy_status.ok()) {
    ABSL_LOG(WARNING) << "Failed to copy textures: " << copy_status.message();
  }

  ABSL_LOG(INFO) << "✅ Lens import complete: " << filter.GetMetadata().name;
  return filter;
}

absl::StatusOr<SnapLensScene> SnapLensImporter::ParseScene(const std::string& lens_dir) {
  std::string xml_path = fs::path(lens_dir) / "_scene_scn.xml";
  return ParseSceneXML(xml_path);
}

absl::StatusOr<std::string> SnapLensImporter::GetCompatibilityReport(
    const std::string& lens_dir) {
  auto scene_result = ParseScene(lens_dir);
  if (!scene_result.ok()) {
    return scene_result.status();
  }

  auto compat = AnalyzeCompatibility(*scene_result);

  json report;
  report["compatibility_percentage"] = compat.percentage * 100;
  report["supported_features"] = compat.supported_features;
  report["unsupported_features"] = compat.unsupported_features;
  report["warnings"] = compat.warnings;

  return report.dump(2);
}

// ============================================================================
// XML Parsing
// ============================================================================

absl::StatusOr<SnapLensScene> SnapLensImporter::ParseSceneXML(const std::string& xml_path) {
  std::ifstream file(xml_path);
  if (!file.is_open()) {
    return absl::NotFoundError("Cannot open XML file");
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string xml_content = buffer.str();

  SnapLensScene scene;

  // Parse version
  std::regex version_regex(R"(<uint32 key="version">(\d+)</uint32>)");
  std::smatch match;
  if (std::regex_search(xml_content, match, version_regex)) {
    scene.version = std::stoi(match[1]);
  }

  // Parse core version
  std::regex core_version_regex(R"(<int32 key="core_version">(\d+)</int32>)");
  if (std::regex_search(xml_content, match, core_version_regex)) {
    scene.core_version = std::stoi(match[1]);
  }

  // Parse texture assets
  std::regex texture_block_regex(
      R"(<block>.*?<string key="type">Asset\.Texture</string>.*?<string key="name">(.*?)</string>.*?<string key="path">(.*?)</string>.*?</block>)",
      std::regex::multiline | std::regex::dotall
  );

  auto textures_begin = std::sregex_iterator(xml_content.begin(), xml_content.end(), texture_block_regex);
  auto textures_end = std::sregex_iterator();

  for (std::sregex_iterator i = textures_begin; i != textures_end; ++i) {
    std::smatch m = *i;
    SnapLensAsset asset;
    asset.type = "Asset.Texture";
    asset.name = m[1].str();
    asset.file_path = m[2].str();
    scene.textures.push_back(asset);
    
    ABSL_LOG(INFO) << "  📷 Texture: " << asset.name << " → " << asset.file_path;
  }

  return scene;
}

// ============================================================================
// Asset Extraction
// ============================================================================

absl::Status SnapLensImporter::CopyTextureFiles(
    const std::string& lens_dir,
    const std::string& output_dir,
    const std::vector<SnapLensAsset>& textures) {
  
  for (const auto& texture : textures) {
    fs::path source = fs::path(lens_dir) / texture.file_path;
    if (!fs::exists(source)) {
      ABSL_LOG(WARNING) << "Texture file not found: " << source;
      continue;
    }

    // Extract filename and copy
    fs::path dest = fs::path(output_dir) / source.filename();
    try {
      fs::copy_file(source, dest, fs::copy_options::overwrite_existing);
      ABSL_LOG(INFO) << "  📁 Copied: " << source.filename();
    } catch (const std::exception& e) {
      ABSL_LOG(ERROR) << "Failed to copy " << source << ": " << e.what();
    }
  }

  return absl::OkStatus();
}

// ============================================================================
// Filter Generation
// ============================================================================

FilterAsset SnapLensImporter::CreateFilterFromScene(
    const SnapLensScene& scene,
    const std::string& lens_dir) {
  
  FilterAsset filter;

  // Create metadata
  FilterMetadata metadata;
  metadata.name = "Imported Snapchat Lens";
  metadata.id = "snap-lens-import";
  metadata.category = "imported";
  metadata.version = "1.0.0";
  metadata.author = "Snapchat (imported)";
  metadata.description = "Imported from Lens Studio .scn format";
  
  filter.SetMetadata(metadata);

  // Create attachments (one per texture for now - simplified)
  std::vector<FilterAttachment> attachments;
  
  if (!scene.textures.empty()) {
    FilterAttachment attachment;
    attachment.id = "main_attachment";
    attachment.anchor_name = "nose_bridge";  // Default anchor
    
    // Use first texture as diffuse
    auto texture_path = fs::path(scene.textures[0].file_path).filename();
    attachment.texture_path = texture_path.string();
    
    // Set reasonable defaults
    attachment.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    attachment.offset = glm::vec3(0.0f, 0.0f, 0.0f);
    attachment.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    
    attachments.push_back(attachment);
  }

  filter.SetAttachments(attachments);

  return filter;
}

FilterMetadata SnapLensImporter::ExtractMetadata(const SnapLensScene& scene) {
  FilterMetadata metadata;
  metadata.name = "Imported Lens";
  metadata.id = "snap-lens-import";
  metadata.category = "imported";
  metadata.version = "1.0.0";
  metadata.author = "Snapchat";
  return metadata;
}

std::vector<FilterAttachment> SnapLensImporter::CreateAttachments(
    const SnapLensScene& scene) {
  return std::vector<FilterAttachment>();  // Placeholder
}

std::string SnapLensImporter::GuessAnchorFromName(const std::string& name) {
  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

  if (lower_name.find("nose") != std::string::npos) return "nose_bridge";
  if (lower_name.find("eye") != std::string::npos) return "between_eyes";
  if (lower_name.find("mouth") != std::string::npos) return "mouth_center";
  if (lower_name.find("ear") != std::string::npos) return "left_ear";
  if (lower_name.find("forehead") != std::string::npos) return "forehead";
  if (lower_name.find("chin") != std::string::npos) return "chin";

  return "nose_bridge";  // Default fallback
}

// ============================================================================
// Compatibility Analysis
// ============================================================================

SnapLensImporter::CompatibilityInfo SnapLensImporter::AnalyzeCompatibility(
    const SnapLensScene& scene) {
  
  CompatibilityInfo info;
  info.percentage = 0.0f;

  // Check for textures (basic requirement)
  if (!scene.textures.empty()) {
    info.supported_features.push_back("Textures (JPEG/PNG)");
    info.percentage += 0.4f;
  } else {
    info.warnings.push_back("No textures found");
  }

  // Check for materials
  if (!scene.materials.empty()) {
    info.supported_features.push_back("Materials");
    info.percentage += 0.2f;
  }

  // Always list common unsupported features
  info.unsupported_features.push_back("Custom shaders (GLSL)");
  info.unsupported_features.push_back("JavaScript behaviors");
  info.unsupported_features.push_back("Physics simulations");
  info.unsupported_features.push_back("3D models (need OBJ conversion)");

  // Add warnings for missing 3D models
  if (scene.meshes.empty()) {
    info.warnings.push_back("No 3D geometry found - textures only");
    info.warnings.push_back("You may need to manually create OBJ models");
  }

  return info;
}

} // namespace ar_filters
} // namespace segmecam
