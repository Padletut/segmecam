// SDL2 + OpenGL + ImGui rendering of MediaPipe GPU segmentation (refactored version)

#include <iostream>
#include <fstream>
#include <SDL.h>
#include <opencv2/core/ocl.hpp>

// New modular includes
#include "args_parser.h"
#include "mediapipe_manager.h"
#include "camera_manager.h"
#include "ui_manager.h"
#include "app_state.h"
#include "app_loop.h"

using namespace segmecam;

int main(int argc, char** argv) {
  // Parse command line arguments
  AppArgs args = ArgsParser::ParseArgs(argc, argv);
  ArgsParser::SetupResourceRootDir(args.resource_root_dir);
  
  // Initialize application state
  AppState state;
  state.opencl_available = cv::ocl::haveOpenCL();
  state.fps_last_ms = SDL_GetTicks();
  state.perf_last_log_ms = SDL_GetTicks();
  state.dbg_last_ms = SDL_GetTicks();
  
  // Initialize MediaPipe manager
  MediaPipeManager mediapipe;
  if (!mediapipe.Initialize(args.graph_path)) {
    std::fprintf(stderr, "Failed to initialize MediaPipe\n");
    return 1;
  }
  
  if (!mediapipe.Start()) {
    std::fprintf(stderr, "Failed to start MediaPipe graph\n");
    return 2;
  }
  
  // Initialize camera manager
  CameraManager camera;
  if (!camera.Initialize(args.cam_index)) {
    std::fprintf(stderr, "Failed to initialize camera\n");
    return 3;
  }
  
  // Initialize UI manager
  UIManager ui;
  if (!ui.Initialize()) {
    std::fprintf(stderr, "Failed to initialize UI\n");
    return 4;
  }
  
  // Load default profile if present
  auto profiles = camera.ListProfiles();
  if (!profiles.empty()) {
    // Try to load "default" profile
    for (const auto& prof : profiles) {
      if (prof == "default_profile") {
        camera.LoadProfile(prof, [&state](const cv::FileNode& root) {
          state.LoadFromProfile(root);
        });
        break;
      }
    }
  }
  
  // Run main application loop
  AppLoop app_loop(state, ui, camera, mediapipe);
  app_loop.Run();
  
  // Cleanup
  if (state.vcam.IsOpen()) { 
    state.vcam.Close(); 
  }
  
  mediapipe.CloseInputStream();
  mediapipe.WaitUntilDone();
  
  return 0;
}