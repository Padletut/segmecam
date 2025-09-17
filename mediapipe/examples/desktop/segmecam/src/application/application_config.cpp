#include "include/application/application_config.h"

#include <iostream>
#include <cstdlib>

namespace segmecam {

ApplicationConfig ApplicationConfig::FromCommandLine(int argc, char** argv) {
    ApplicationConfig config;
    
    std::cout << "🔧 Parsing " << argc << " command line arguments:" << std::endl;
    for (int i = 0; i < argc; ++i) {
        std::cout << "  argv[" << i << "] = '" << argv[i] << "'" << std::endl;
    }
    
    // Parse command line arguments in --key=value format
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        
        if (arg.find("--graph_path=") == 0) {
            config.graph_path = arg.substr(13); // Remove "--graph_path="
            std::cout << "  ✅ Parsed graph_path: '" << config.graph_path << "'" << std::endl;
        } else if (arg.find("--resource_root_dir=") == 0) {
            config.resource_root_dir = arg.substr(20); // Remove "--resource_root_dir="
            std::cout << "  ✅ Parsed resource_root_dir: '" << config.resource_root_dir << "'" << std::endl;
        } else if (arg.find("--camera_id=") == 0) {
            config.cam_index = std::atoi(arg.substr(12).c_str()); // Remove "--camera_id="
            std::cout << "  ✅ Parsed camera_id: " << config.cam_index << std::endl;
        } else if (arg.find("--") == 0) {
            // Handle other flags if needed in the future
            std::cout << "  ⚠️  Unknown flag: " << arg << std::endl;
        } else {
            // Fallback: treat as positional arguments for backwards compatibility
            if (i == 1) {
                config.graph_path = arg;
                std::cout << "  ✅ Positional graph_path: '" << config.graph_path << "'" << std::endl;
            } else if (i == 2) {
                config.resource_root_dir = arg;
                std::cout << "  ✅ Positional resource_root_dir: '" << config.resource_root_dir << "'" << std::endl;
            } else if (i == 3) {
                config.cam_index = std::atoi(arg.c_str());
                std::cout << "  ✅ Positional camera_id: " << config.cam_index << std::endl;
            }
        }
    }
    
    std::cout << "🔧 Final config:" << std::endl;
    std::cout << "  graph_path: '" << config.graph_path << "'" << std::endl;
    std::cout << "  resource_root_dir: '" << config.resource_root_dir << "'" << std::endl;
    std::cout << "  cam_index: " << config.cam_index << std::endl;
    
    return config;
}

bool ApplicationConfig::IsValid() const {
    return !graph_path.empty() && !resource_root_dir.empty() && cam_index >= 0;
}

void ApplicationConfig::SetDefaults() {
    if (graph_path.empty()) {
        graph_path = "mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt";
    }
    if (resource_root_dir.empty()) {
        resource_root_dir = ".";
    }
    if (cam_index < 0) {
        cam_index = 0;
    }
}

} // namespace segmecam