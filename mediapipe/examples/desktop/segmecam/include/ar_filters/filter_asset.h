// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// FilterAsset - AR Filter Asset Definition and JSON Parser
//
// Phase 6: Defines the schema and loader for AR filter configuration files.
// Filters are defined as JSON files with metadata, 3D models, materials,
// and blendshape-driven behaviors.

#pragma once

#include <string>
#include <vector>
#include <map>
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "glm/glm.hpp"
#include <nlohmann/json.hpp>

namespace segmecam {
namespace ar_filters {

/**
 * Filter metadata - Basic information about the filter
 */
struct FilterMetadata {
  std::string name;           // Display name (e.g., "Classic Glasses")
  std::string id;             // Unique identifier (e.g., "classic-glasses-v1")
  std::string category;       // Category (e.g., "glasses", "hats", "masks")
  std::string version;        // Semantic version (e.g., "1.0.0")
  std::string author;         // Creator name
  std::string description;    // Human-readable description
  std::string thumbnail_path; // Preview image (relative to filter dir)
  std::string icon_path;      // Small icon (relative to filter dir)
};

/**
 * Filter attachment - A 3D model attached to a face anchor point
 */
struct FilterAttachment {
  std::string id;             // Unique ID for this attachment
  std::string anchor_name;    // Face anchor: "nose_bridge", "left_ear", etc.
  std::string model_path;     // Path to .obj file (relative to filter dir)
  std::string texture_path;   // Path to texture image (relative)
  std::string opacity_map_path;   // Path to opacity/alpha map (relative)
  std::string emissive_map_path;  // Path to emissive/glow map (relative)
  
  // Transform parameters
  glm::vec3 scale{1.0f, 1.0f, 1.0f};
  glm::vec3 offset{0.0f, 0.0f, 0.0f};
  glm::vec3 rotation{0.0f, 0.0f, 0.0f};  // Euler angles in radians
  
  // Rendering properties
  bool visible = true;
  bool flip_z = false;        // Flip Z-axis to fix inside-out models
  float opacity = 1.0f;
};

/**
 * Filter material - OpenGL material properties
 */
struct FilterMaterial {
  glm::vec3 ambient{0.2f, 0.2f, 0.2f};   // Ambient color
  glm::vec3 diffuse{0.8f, 0.8f, 0.8f};   // Diffuse color
  glm::vec3 specular{1.0f, 1.0f, 1.0f};  // Specular highlight color
  float shininess = 32.0f;                // Specular exponent
  float opacity = 1.0f;                   // Transparency (0=transparent, 1=opaque)
};

/**
 * Filter behavior - Dynamic reactions to facial expressions
 * 
 * Behaviors connect MediaPipe blendshapes to visual effects:
 * - SHAKE: Jitter the attachment when blendshape exceeds threshold
 * - FALL_OFF: Apply physics drop when threshold reached
 * - SCALE: Scale attachment proportional to blendshape value
 * - ROTATE: Rotate attachment based on blendshape value
 * - HIDE: Hide attachment when threshold exceeded
 * - COLOR_CHANGE: Modify material color on activation
 */
struct FilterBehavior {
  enum class Type {
    SHAKE,          // Jitter on blendshape activation
    FALL_OFF,       // Physics drop on threshold
    SCALE,          // Scale with blendshape value
    ROTATE,         // Rotate with blendshape value
    HIDE,           // Hide when threshold reached
    COLOR_CHANGE    // Change color on activation
  };
  
  Type type;
  std::string target_attachment_id;  // Which attachment this affects
  std::string blendshape_name;       // MediaPipe blendshape (e.g., "eyeBlinkLeft")
  float threshold = 0.5f;            // Activation threshold (0.0-1.0)
  float intensity = 1.0f;            // Effect strength multiplier
  float gravity = 9.8f;              // Gravity for FALL_OFF type (m/s²)
  
  // Additional parameters for specific behavior types
  glm::vec3 target_scale{1.0f, 1.0f, 1.0f};    // For SCALE type
  glm::vec3 target_rotation{0.0f, 0.0f, 0.0f}; // For ROTATE type
  glm::vec3 target_color{1.0f, 1.0f, 1.0f};    // For COLOR_CHANGE type
};

/**
 * FilterAsset - Complete AR filter definition loaded from JSON
 * 
 * Usage:
 *   auto filter = FilterAsset::LoadFromDirectory("assets/filters/classic-glasses");
 *   if (filter.ok()) {
 *     const auto& metadata = filter->GetMetadata();
 *     const auto& attachments = filter->GetAttachments();
 *     // Use filter with ARRenderer...
 *   }
 */
class FilterAsset {
public:
  FilterAsset() = default;
  ~FilterAsset() = default;
  
  // Movable but not copyable (contains parsed data)
  FilterAsset(FilterAsset&&) = default;
  FilterAsset& operator=(FilterAsset&&) = default;
  FilterAsset(const FilterAsset&) = delete;
  FilterAsset& operator=(const FilterAsset&) = delete;
  
  /**
   * Load filter from directory containing filter.json
   * 
   * @param filter_dir Directory containing filter.json and assets
   * @return FilterAsset instance or error status
   */
  static absl::StatusOr<FilterAsset> LoadFromDirectory(
      const std::string& filter_dir);
  
  /**
   * Load filter from explicit JSON file path
   * 
   * @param json_path Full path to filter.json file
   * @return FilterAsset instance or error status
   */
  static absl::StatusOr<FilterAsset> LoadFromFile(
      const std::string& json_path);
  
  /**
   * Enumerate all filters in a root directory
   * 
   * Searches for subdirectories containing filter.json files.
   * 
   * @param filters_root Root directory to search (e.g., "assets/filters")
   * @return Vector of filter directory paths
   */
  static std::vector<std::string> EnumerateFilters(
      const std::string& filters_root);
  
  /**
   * Validate that all referenced assets exist on disk
   * 
   * Checks:
   * - Model files (.obj) exist
   * - Texture files exist
   * - Thumbnail and icon exist
   * 
   * @return OK if all assets found, error otherwise
   */
  absl::Status ValidateAssets() const;
  
  // Getters for filter components
  const FilterMetadata& GetMetadata() const { return metadata_; }
  const std::vector<FilterAttachment>& GetAttachments() const { 
    return attachments_; 
  }
  const std::map<std::string, FilterMaterial>& GetMaterials() const {
    return materials_;
  }
  const std::vector<FilterBehavior>& GetBehaviors() const {
    return behaviors_;
  }
  
  std::string GetFilterDirectory() const { return filter_directory_; }
  
  // Utility: Get absolute path for a relative asset path
  std::string GetAssetPath(const std::string& relative_path) const;

private:
  FilterMetadata metadata_;
  std::vector<FilterAttachment> attachments_;
  std::map<std::string, FilterMaterial> materials_;
  std::vector<FilterBehavior> behaviors_;
  std::string filter_directory_;  // Base directory for resolving relative paths
  
  // JSON parsing helpers
  static absl::Status ParseMetadata(const nlohmann::json& json,
                                    FilterMetadata& metadata);
  static absl::Status ParseAttachments(const nlohmann::json& json,
                                       std::vector<FilterAttachment>& attachments);
  static absl::Status ParseMaterials(const nlohmann::json& json,
                                     std::map<std::string, FilterMaterial>& materials);
  static absl::Status ParseBehaviors(const nlohmann::json& json,
                                     std::vector<FilterBehavior>& behaviors);
  
  // Helper: Parse glm::vec3 from JSON array [x, y, z]
  static absl::StatusOr<glm::vec3> ParseVec3(const nlohmann::json& json,
                                             const std::string& field_name);
  
  // Helper: Convert string to behavior type enum
  static absl::StatusOr<FilterBehavior::Type> ParseBehaviorType(
      const std::string& type_str);
};

} // namespace ar_filters
} // namespace segmecam
