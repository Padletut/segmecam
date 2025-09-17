#include "include/application/application.h"

// Include extracted modules
#include "include/application/application_config.h"
#include "include/application/gpu_setup.h"
#include "include/application/mediapipe_setup.h"
#include "include/application/manager_coordination.h"
#include "include/application/application_cleanup.h"
#include "include/application/application_run.h"
#include "include/application/application_initialization.h"

// Include managers used directly in application
#include "include/camera/camera_manager.h"
#include "include/effects/effects_manager.h"

// Include MediaPipe for output stream polling
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"

// Include SDL for OpenGL context (needed for MediaPipe GPU)
#include <SDL.h>
#include <SDL_opengl.h>

// Include ImGui for GUI
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h" 
#include "third_party/imgui/backends/imgui_impl_opengl3.h"

// Include OpenCV for camera capture and image processing
#include <opencv2/opencv.hpp>

// Include OpenGL for texture management
#include <GL/gl.h>

#include <iostream>
#include <chrono>
#include <thread>

namespace segmecam {

SegmeCamApplication::SegmeCamApplication() 
    : window_(nullptr), gl_context_(nullptr) {
}

SegmeCamApplication::~SegmeCamApplication() {
    Cleanup();
}

int SegmeCamApplication::Initialize(const ApplicationConfig& config) {
    config_ = config;
    
    // Use the extracted initialization module for complete application setup
    return ApplicationInitialization::InitializeApplication(
        config_, managers_, app_state_, mediapipe_graph_, mask_poller_, 
        multi_face_landmarks_poller_, face_rects_poller_,
        window_, gl_context_, gpu_setup_state_
    );
}

int SegmeCamApplication::Run() {
    // Use the extracted run module for main application loop
    return ApplicationRun::ExecuteMainLoop(managers_, mediapipe_graph_, mask_poller_, 
                                          multi_face_landmarks_poller_, face_rects_poller_,
                                          window_, app_state_);
}

void SegmeCamApplication::Cleanup() {
    // Use the extracted cleanup module for proper shutdown
    ApplicationCleanup::PerformCleanup(managers_, mediapipe_graph_, gl_context_, window_);
}

} // namespace segmecam

// Main function
int main(int argc, char** argv) {
    // Parse command line configuration using extracted module
    auto config = segmecam::ApplicationConfig::FromCommandLine(argc, argv);
    
    segmecam::SegmeCamApplication app;
    
    if (int result = app.Initialize(config); result != 0) {
        std::cerr << "❌ Application initialization failed with code: " << result << std::endl;
        return result;
    }
    
    int result = app.Run();
    
    // Cleanup is handled by destructor
    
    return result;
}