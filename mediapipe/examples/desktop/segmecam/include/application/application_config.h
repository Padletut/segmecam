#ifndef SEGMECAM_APPLICATION_CONFIG_H
#define SEGMECAM_APPLICATION_CONFIG_H

#include <string>

namespace segmecam {

struct ApplicationConfig {
    std::string graph_path = "mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt";
    std::string cpu_graph_path = "mediapipe_graphs/selfie_seg_cpu_min.pbtxt";
    std::string resource_root_dir = ".";
    int cam_index = 0;
    
    // Static factory method for command line parsing
    static ApplicationConfig FromCommandLine(int argc, char** argv);
    
    // Validation methods
    bool IsValid() const;
    void SetDefaults();
};

} // namespace segmecam

#endif // SEGMECAM_APPLICATION_CONFIG_H