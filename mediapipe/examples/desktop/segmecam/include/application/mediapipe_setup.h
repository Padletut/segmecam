#ifndef MEDIAPIPE_SETUP_H
#define MEDIAPIPE_SETUP_H

#include <string>
#include <memory>
#include "application/application_config.h"
#include "gpu_detector.h"
#include "mediapipe/framework/calculator_graph.h"

namespace segmecam {

class MediaPipeSetup {
public:
    // Graph selection and path resolution
    static std::string SelectGraphPath(const ApplicationConfig& config, const GPUCapabilities& gpu_caps);
    
    // MediaPipe graph initialization (without starting)
    static int InitializeGraph(std::unique_ptr<mediapipe::CalculatorGraph>& graph, 
                              const std::string& graph_path, 
                              const GPUCapabilities& gpu_caps,
                              const ApplicationConfig& config);
    
    // Setup MediaPipe resource directory for model files
    static void SetupResourceDirectory(const std::string& user_resource_root_dir = "");
    
    // Start the MediaPipe graph (call after setting up output stream pollers)
    static int StartGraph(std::unique_ptr<mediapipe::CalculatorGraph>& graph);
    
private:
    // Helper functions
    static std::string ResolveGraphPath(const std::string& graph_path, 
                                       const std::string& resource_root_dir,
                                       bool force_cpu);
};

} // namespace segmecam

#endif // SEGMECAM_MEDIAPIPE_SETUP_H