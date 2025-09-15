#pragma once

#include <string>

namespace segmecam {

struct AppArgs {
  std::string graph_path = "mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt";
  std::string resource_root_dir = ".";
  int cam_index = 0;
};

class ArgsParser {
public:
  static AppArgs ParseArgs(int argc, char** argv);
  static void SetupResourceRootDir(const std::string& resource_root_dir);
};

} // namespace segmecam