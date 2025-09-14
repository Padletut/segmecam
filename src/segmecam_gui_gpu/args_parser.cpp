#include "args_parser.h"
#include <cstdlib>
#include <cstdlib>
#include "absl/flags/flag.h"
#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, resource_root_dir);

namespace segmecam {

AppArgs ArgsParser::ParseArgs(int argc, char** argv) {
  AppArgs args;
  
  if (argc > 1) args.graph_path = argv[1];
  if (argc > 2) args.resource_root_dir = argv[2];
  if (argc > 3) args.cam_index = std::atoi(argv[3]);
  
  return args;
}

void ArgsParser::SetupResourceRootDir(const std::string& resource_root_dir) {
  // If running under Bazel, point MediaPipe resource loader to runfiles.
  const char* rf = std::getenv("RUNFILES_DIR");
  if (rf && *rf) {
    absl::SetFlag(&FLAGS_resource_root_dir, std::string(rf));
  } else if (!resource_root_dir.empty()) {
    absl::SetFlag(&FLAGS_resource_root_dir, resource_root_dir);
  }
}

} // namespace segmecam