// Copyright 2025 SegmeCam Authors
// Test tool for Snapchat Lens Importer

#include "include/ar_filters/snap_lens_importer.h"
#include "absl/log/absl_log.h"
#include <iostream>

using namespace segmecam::ar_filters;

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <lens_directory>" << std::endl;
    std::cerr << "Example: " << argv[0] << " assets/filters/snap-lens" << std::endl;
    return 1;
  }

  std::string lens_dir = argv[1];
  std::cout << "\n🔍 Analyzing Snapchat Lens: " << lens_dir << "\n" << std::endl;

  SnapLensImporter importer;

  // Get compatibility report
  auto report_result = importer.GetCompatibilityReport(lens_dir);
  if (!report_result.ok()) {
    std::cerr << "❌ Error: " << report_result.status().message() << std::endl;
    return 1;
  }

  std::cout << "📊 Compatibility Report:\n" << *report_result << "\n" << std::endl;

  // Parse scene details
  auto scene_result = importer.ParseScene(lens_dir);
  if (scene_result.ok()) {
    const auto& scene = *scene_result;
    std::cout << "📦 Scene Details:" << std::endl;
    std::cout << "  Version: " << scene.version << std::endl;
    std::cout << "  Core Version: " << scene.core_version << std::endl;
    std::cout << "  Textures: " << scene.textures.size() << std::endl;
    std::cout << "  Materials: " << scene.materials.size() << std::endl;
    std::cout << "  Meshes: " << scene.meshes.size() << "\n" << std::endl;
  }

  // Try to import
  std::cout << "🔄 Attempting import..." << std::endl;
  auto filter_result = importer.ImportLens(lens_dir);
  
  if (!filter_result.ok()) {
    std::cerr << "❌ Import failed: " << filter_result.status().message() << std::endl;
    return 1;
  }

  std::cout << "✅ Import successful!" << std::endl;
  std::cout << "  Filter: " << filter_result->GetMetadata().name << std::endl;
  std::cout << "  Attachments: " << filter_result->GetAttachments().size() << std::endl;

  // Save to JSON
  std::string output_path = lens_dir + "/../snap-lens-converted/filter.json";
  // Note: SaveToJSON() would need to be implemented in FilterAsset
  std::cout << "\n💾 Would save to: " << output_path << std::endl;

  return 0;
}
