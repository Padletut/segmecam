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
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "mediapipe/framework/output_stream_poller.h"

namespace segmecam {

/**
 * Data structure for MediaPipe output processing
 */
struct MediaPipeOutputData {
    cv::Mat last_mask_u8;
    mediapipe::NormalizedLandmarkList latest_lms;
    bool have_lms = false;
    std::vector<mediapipe::NormalizedRect> latest_rects;
};

/**
 * Data structure for frame processing parameters
 */
struct FrameProcessingParams {
    ManagerCoordination::Managers& managers;
    std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph;
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller;
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller;
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller;
    SDL_Window* window;
    AppState& app_state;
    UIManager& ui_manager;
    int64_t& frame_id;
    double& fps;
    uint64_t& fps_frames;
    uint32_t& fps_last_ms;
    int frame_count;
    bool has_landmarks;
    bool& running;
};

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
     * Run the main processing loop
     */
    static void RunMainLoop(
        ManagerCoordination::Managers& managers,
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        SDL_Window* window,
        AppState& app_state,
        UIManager& ui_manager,
        bool& running,
        int64_t& frame_id,
        double& fps,
        uint64_t& fps_frames,
        uint32_t& fps_last_ms,
        bool has_landmarks);

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

    /**
     * Process frame capture and initial validation
     */
    static bool ProcessFrameCapture(CameraManager& camera_mgr, cv::Mat& frame_bgr, int frame_count);

    /**
     * Process MediaPipe outputs and update state
     */
    static void ProcessMediaPipeOutputs(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        AppState& app_state,
        int frame_count,
        bool has_landmarks);

    /**
     * Process mask output from MediaPipe
     */
    static void ProcessMaskOutput(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        AppState& app_state,
        int frame_count);

    /**
     * Process face landmarks from MediaPipe
     */
    static void ProcessFaceLandmarks(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        int frame_count);

    /**
     * Process frame effects and prepare for display
     */
    static cv::Mat ProcessAndDisplayFrame(
        const cv::Mat& frame_bgr,
        const cv::Mat& last_mask_u8,
        const mediapipe::NormalizedLandmarkList* landmarks_ptr,
        EffectsManager& effects_mgr,
        AppState& app_state,
        bool have_lms);

    /**
     * Handle virtual camera output
     */
    static void HandleVirtualCameraOutput(
        CameraManager& camera_mgr,
        AppState& app_state,
        const cv::Mat& display_rgb);

    /**
     * Handle dropped files processing
     */
    static void HandleDroppedFiles(
        UIManager& ui_manager,
        AppState& app_state);

    /**
     * Initialize application components
     */
    static bool InitializeApplication(
        ManagerCoordination::Managers& managers,
        SDL_Window* window,
        AppState& app_state,
        UIManager& ui_manager);

    /**
     * Process single frame in main loop
     */
    static bool ProcessFrame(FrameProcessingParams& params);

    /**
     * Render complete frame with UI overlay
     */
    static void RenderFrame(
        UIManager& ui_manager,
        const cv::Mat& display_rgb,
        SDL_Window* window,
        int frame_count,
        bool& running);

};

} // namespace segmecam

#endif // APPLICATION_RUN_H