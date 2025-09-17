#ifndef APPLICATION_INITIALIZATION_H
#define APPLICATION_INITIALIZATION_H

#include <memory>
#include <string>
#include <SDL.h>
#include "application/manager_coordination.h"
#include "application/application_config.h"
#include "application/gpu_setup.h"
#include "app_state.h"

// MediaPipe includes for complete types
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/output_stream_poller.h"

namespace segmecam {

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
     * @param mediapipe_graph MediaPipe graph to initialize
     * @param mask_poller Output stream poller to create
     * @param multi_face_landmarks_poller Face landmarks poller to create
     * @param face_rects_poller Face rects poller to create  
     * @param window SDL window to create
     * @param gl_context SDL OpenGL context to create
     * @param gpu_setup_state GPU setup state to populate
     * @return 0 for success, negative error code for failure
     */
    static int InitializeApplication(
        const ApplicationConfig& config,
        ManagerCoordination::Managers& managers,
        AppState& app_state,
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        SDL_Window*& window,
        SDL_GLContext& gl_context,
        GPUSetupState& gpu_setup_state
    );

private:
    /**
     * Initialize MediaPipe graph and output stream pollers
     */
    static int InitializeMediaPipe(
        const ApplicationConfig& config,
        const GPUSetupState& gpu_setup_state,
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller
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