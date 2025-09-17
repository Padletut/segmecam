#ifndef APPLICATION_RUN_H
#define APPLICATION_RUN_H

#include <memory>
#include <opencv2/opencv.hpp>
#include <SDL.h>
#include <GL/gl.h>
#include "application/manager_coordination.h"
#include "app_state.h"

// Forward declarations
namespace segmecam {
    class UIManager;
    class EffectsManager;
}

// MediaPipe includes for complete types
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/output_stream_poller.h"

namespace segmecam {

/**
 * Application main loop module - handles the core application execution loop
 * 
 * This module follows the modular architecture pattern and provides the main
 * application loop functionality including frame processing, UI rendering,
 * and event handling.
 */
class ApplicationRun {
public:
    /**
     * Execute the main application loop
     * @param managers Reference to manager coordination structure
     * @param mediapipe_graph MediaPipe graph for processing
     * @param mask_poller Output stream poller for segmentation masks
     * @param multi_face_landmarks_poller Optional face landmarks poller
     * @param face_rects_poller Optional face rects poller
     * @param window SDL window for rendering
     * @param app_state Application state for shared data
     * @return Exit code (0 for success, non-zero for error)
     */
    static int ExecuteMainLoop(
        ManagerCoordination::Managers& managers,
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        SDL_Window* window,
        AppState& app_state
    );

    /**
     * Sync status FROM EffectsManager back TO app_state (e.g., OpenCL availability)
     */
    static void SyncStatusFromEffectsManager(const EffectsManager& effects_manager, AppState& app_state);

    /**
     * Sync app_state settings to EffectsManager
     */
    static void SyncSettingsToEffectsManager(EffectsManager& effects_manager, const AppState& app_state);

private:
    /**
     * Helper function to convert MediaPipe ImageFrame to OpenCV Mat
     */
    static void MatToImageFrame(const cv::Mat& mat_bgr, std::unique_ptr<mediapipe::ImageFrame>& frame);
    
    /**
     * Process SDL events and handle ImGui integration
     */
    static bool ProcessEvents(bool& running);
    
    /**
     * Update FPS tracking and performance metrics
     */
    static void UpdateFPSTracking(double& fps, uint64_t& fps_frames, uint32_t& fps_last_ms);
    
    /**
     * Render video feed as fullscreen background
     */
    static void RenderVideoBackground(const cv::Mat& display_rgb, int window_width, int window_height);
    
    /**
     * Create and manage OpenGL texture from video frame
     */
    static GLuint CreateVideoTexture(const cv::Mat& display_rgb);
    
    /**
     * Render comprehensive UI with all panels using UIManager Enhanced
     */
    static void RenderComprehensiveUI(ManagerCoordination::Managers& managers,
                                     const AppState& app_state, 
                                     UIManager& ui_manager,
                                     bool& running);
};

} // namespace segmecam

#endif // APPLICATION_RUN_H