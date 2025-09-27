#ifndef APPLICATION_INITIALIZATION_H
#define APPLICATION_INITIALIZATION_H

#include <memory>
#include <string>
#include <SDL.h>
#include "application/manager_coordination.h"
#include "application/application_config.h"
#include "application/gpu_setup.h"
#include "include/application/app_state.h"

// MediaPipe includes for complete types
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/output_stream_poller.h"

namespace segmecam {

/**
 * Struct to group MediaPipe initialization parameters and reduce method parameter count
 */
struct MediaPipeInitParams {
    std::unique_ptr<mediapipe::CalculatorGraph>& graph;
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller;
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller;
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller;
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_blendshapes_poller;
};

/**
 * Struct to group SDL/OpenGL initialization parameters
 */
struct SDLInitParams {
    SDL_Window*& window;
    SDL_GLContext& gl_context;
};

/**
 * Application initialization module - handles complete application setup
 * 
 * This module follows the modular architecture pattern and provides comprehensive
 * initialization including GPU setup, MediaPipe graph initialization, SDL/OpenGL
 * context creation, ImGui setup, and manager coordination.
 */
class ApplicationInitialization {
public:
    /**
     * Initialize the complete application system
     * @param config Application configuration from command line
     * @param managers Manager coordination structure to initialize
     * @param app_state Application state to populate
     * @param mediapipe_params MediaPipe initialization parameters
     * @param sdl_params SDL/OpenGL initialization parameters
     * @param gpu_setup_state GPU setup state to populate
     * @return 0 for success, negative error code for failure
     */
    static int InitializeApplication(
        const ApplicationConfig& config,
        ManagerCoordination::Managers& managers,
        AppState& app_state,
        MediaPipeInitParams& mediapipe_params,
        SDLInitParams& sdl_params,
        GPUSetupState& gpu_setup_state
    );

private:
    /**
     * Initialize MediaPipe graph
     */
    static int InitializeMediaPipeGraph(
        const ApplicationConfig& config,
        const GPUSetupState& gpu_setup_state,
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
        AppState& app_state
    );
    
    /**
     * Setup MediaPipe output stream pollers
     */
    static int SetupMediaPipePollers(
        const ApplicationConfig& config,
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_blendshapes_poller
    );
    
    /**
     * Start MediaPipe graph processing
     */
    static int StartMediaPipeGraph(
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph
    );
    
    /**
     * Initialize SDL, OpenGL context, and window
     */
    static int InitializeSDLAndOpenGL(
        SDL_Window*& window,
        SDL_GLContext& gl_context
    );
    
    /**
     * Initialize ImGui for enhanced UI
     */
    static int InitializeImGui(SDL_Window* window, SDL_GLContext gl_context);
    
    /**
     * Initialize all Phase 1-7 managers
     */
    static int InitializeManagers(
        ManagerCoordination::Managers& managers,
        AppState& app_state
    );
};

} // namespace segmecam

#endif // APPLICATION_INITIALIZATION_H