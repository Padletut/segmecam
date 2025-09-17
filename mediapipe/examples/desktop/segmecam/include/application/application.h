#ifndef SEGMECAM_APPLICATION_H
#define SEGMECAM_APPLICATION_H

#include <string>
#include <memory>
#include <thread>
#include <opencv2/opencv.hpp>
#include <SDL.h>

// Forward declarations for MediaPipe
namespace mediapipe {
class CalculatorGraph;
class OutputStreamPoller;
}

// Include extracted module headers
#include "application/application_config.h"
#include "application/gpu_setup.h"
#include "application/mediapipe_setup.h"
#include "application/manager_coordination.h"
#include "application/manager_coordination.h"
#include "app_state.h"

namespace segmecam {

/**
 * Main SegmeCam application class - Modular Architecture
 * 
 * This is the minimalist main application that coordinates extracted modules:
 * - ApplicationConfig: Command line parsing and configuration
 * - GPUSetup: GPU detection and environment setup
 * - MediaPipeSetup: Graph loading and MediaPipe initialization
 * - CameraSetup: Camera enumeration and capture setup
 * - ManagerCoordination: Phase 1-7 manager initialization and lifecycle
 * 
 * Each module has single responsibility and clear interfaces.
 */
class SegmeCamApplication {
public:
    SegmeCamApplication();
    ~SegmeCamApplication();

    /**
     * Initialize the application with the given configuration
     * @param config Application configuration from command line or defaults
     * @return 0 on success, error code on failure
     */
    int Initialize(const ApplicationConfig& config);

    /**
     * Run the main application loop
     * @return 0 on success, error code on failure
     */
    int Run();

    /**
     * Clean shutdown of all application resources
     */
    void Cleanup();

private:
    ApplicationConfig config_;
    
    // State for extracted modules
    GPUSetupState gpu_setup_state_;
    std::unique_ptr<mediapipe::CalculatorGraph> mediapipe_graph_;  // MediaPipe graph managed directly
    std::unique_ptr<mediapipe::OutputStreamPoller> mask_poller_;   // Output stream poller for masks
    std::unique_ptr<mediapipe::OutputStreamPoller> multi_face_landmarks_poller_;  // Face landmarks poller
    std::unique_ptr<mediapipe::OutputStreamPoller> face_rects_poller_;             // Face rects poller
    ManagerCoordination::Managers managers_;
    segmecam::AppState app_state_;  // Shared app state for manager coordination
    
    // TODO: Enhanced UI panels integration (Phase 8 next iteration)
    // std::unique_ptr<segmecam::UIManager> ui_manager_;
    
    // SDL window and OpenGL context (needed for MediaPipe GPU)
    SDL_Window* window_;
    SDL_GLContext gl_context_;
};

} // namespace segmecam

#endif // SEGMECAM_APPLICATION_H