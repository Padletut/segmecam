// Copyright 2025 SegmeCam Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// Snapchat Lens Importer - Parse extracted Snapchat lenses into SegmeCam format
//
// This utility reads Lens Studio .scn (SceneKit) exports and converts them
// to SegmeCam filter.json format. Supports:
// - Texture extraction from Files/ directory
// - Material parsing from scene XML
// - Basic geometry (if embedded in scene)
// - Anchor point estimation

#pragma once

#include <string>
#include <vector>
#include <map>
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "include/ar_filters/filter_asset.h"

namespace segmecam {
namespace ar_filters {

/**
 * Snapchat Lens scene asset reference
 */
struct SnapLensAsset {
  std::string uid;              // Asset UID from scene
  std::string type;             // Asset.Texture, Asset.Material, etc.
  std::string name;             // Display name
  std::string file_path;        // Relative path in Files/ directory
  std::map<std::string, std::string> properties;  // Additional properties
};

/**
 * Snapchat Lens scene information
 */
struct SnapLensScene {
  int version;
  int core_version;
  std::vector<SnapLensAsset> textures;
  std::vector<SnapLensAsset> materials;
  std::vector<SnapLensAsset> meshes;
  std::map<std::string, std::string> metadata;
};

/**
 * Snapchat Lens Importer
 * 
 * Converts extracted Snapchat Lens Studio projects to SegmeCam filter format.
 * 
 * Usage:
 *   SnapLensImporter importer;
 *   auto filter = importer.ImportLens("assets/filters/snap-lens");
 *   if (filter.ok()) {
 *     filter->SaveToJSON("assets/filters/converted-lens/filter.json");
 *   }
 */
class SnapLensImporter {
public:
  SnapLensImporter() = default;
  ~SnapLensImporter() = default;

  /**
   * Import Snapchat lens from directory
   * 
   * @param lens_dir Path to extracted lens directory (contains scene.scn, _scene_scn.xml)
   * @return FilterAsset if successful, error status otherwise
   */
  absl::StatusOr<FilterAsset> ImportLens(const std::string& lens_dir);

  /**
   * Get detailed scene information (for debugging)
   * 
   * @param lens_dir Path to extracted lens directory
   * @return SnapLensScene structure with all assets
   */
  absl::StatusOr<SnapLensScene> ParseScene(const std::string& lens_dir);

  /**
   * Get compatibility report
   * 
   * Analyzes lens and returns compatibility percentage and list of
   * supported/unsupported features
   * 
   * @param lens_dir Path to extracted lens directory
   * @return JSON string with compatibility report
   */
  absl::StatusOr<std::string> GetCompatibilityReport(const std::string& lens_dir);

private:
  // XML parsing
  absl::StatusOr<SnapLensScene> ParseSceneXML(const std::string& xml_path);
  absl::Status ParseAssetBlock(const std::string& xml_content, 
                                size_t& pos, 
                                SnapLensAsset& asset);

  // Asset extraction
  absl::Status ExtractTextures(const std::string& lens_dir,
                                const SnapLensScene& scene,
                                const std::string& output_dir);
  absl::Status CopyTextureFiles(const std::string& lens_dir,
                                 const std::string& output_dir,
                                 const std::vector<SnapLensAsset>& textures);

  // Filter generation
  FilterAsset CreateFilterFromScene(const SnapLensScene& scene,
                                     const std::string& lens_dir);
  FilterMetadata ExtractMetadata(const SnapLensScene& scene);
  std::vector<FilterAttachment> CreateAttachments(const SnapLensScene& scene);
  std::string GuessAnchorFromName(const std::string& name);

  // Compatibility analysis
  struct CompatibilityInfo {
    float percentage;
    std::vector<std::string> supported_features;
    std::vector<std::string> unsupported_features;
    std::vector<std::string> warnings;
  };
  CompatibilityInfo AnalyzeCompatibility(const SnapLensScene& scene);
};

} // namespace ar_filters
} // namespace segmecam
