#ifndef FRAME_PROCESSOR_H
#define FRAME_PROCESSOR_H

#include <memory>
#include <opencv2/opencv.hpp>
#include <SDL.h>
#include "application/manager_coordination.h"
#include "include/application/app_state.h"

// Forward declarations
namespace segmecam {
    class UIManager;
    class EffectsManager;
    class CameraManager;
}

// MediaPipe includes for complete types
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "mediapipe/framework/output_stream_poller.h"

namespace segmecam {

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
    int& frame_count;
    bool& running;
    bool& has_landmarks;
};

/**
 * Utility class for frame processing operations in SegmeCam
 */
class FrameProcessor {
public:
    /**
     * Convert OpenCV Mat to MediaPipe ImageFrame
     * @param mat_bgr Input BGR image
     * @param frame Output MediaPipe ImageFrame
     */
    static void MatToImageFrame(const cv::Mat& mat_bgr, std::unique_ptr<mediapipe::ImageFrame>& frame);

    /**
     * Process frame capture from camera
     * @param camera_mgr Camera manager instance
     * @param frame_bgr Output BGR frame
     * @param frame_count Current frame count for debugging
     * @return true if capture successful, false otherwise
     */
    static bool ProcessFrameCapture(CameraManager& camera_mgr, cv::Mat& frame_bgr, int frame_count);

    /**
     * Update FPS tracking information
     * @param fps Current FPS value (output)
     * @param fps_frames Frame count accumulator
     * @param fps_last_ms Last measurement timestamp
     */
    static void UpdateFPSTracking(double& fps, uint64_t& fps_frames, uint32_t& fps_last_ms);

    /**
     * Update camera information in app state
     * @param params Frame processing parameters
     */
    static void UpdateCameraInfo(FrameProcessingParams& params);

    /**
     * Update auto FPS settings based on camera changes
     * @param params Frame processing parameters
     */
    static void UpdateAutoFPS(FrameProcessingParams& params);

    /**
     * Update auto processing scale if enabled
     * @param params Frame processing parameters
     */
    static void UpdateAutoProcessingScale(FrameProcessingParams& params);

    /**
     * Send frame to MediaPipe graph for processing
     * @param frame_bgr Input BGR frame
     * @param params Frame processing parameters
     * @return true if successful, false on error
     */
    static bool SendFrameToMediaPipe(const cv::Mat& frame_bgr, FrameProcessingParams& params);

    /**
     * Process frame with effects and prepare for display
     * @param frame_bgr Input BGR frame
     * @param last_mask_u8 Segmentation mask
     * @param landmarks_ptr Face landmarks (can be nullptr)
     * @param effects_mgr Effects manager instance
     * @param app_state Application state
     * @param have_lms Whether landmarks are available
     * @return Processed RGB frame for display
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
     * @param camera_mgr Camera manager instance
     * @param app_state Application state
     * @param display_rgb RGB frame to output
     */
    static void HandleVirtualCameraOutput(
        CameraManager& camera_mgr,
        AppState& app_state,
        const cv::Mat& display_rgb);

    /**
     * Handle dropped files (background images)
     * @param ui_manager UI manager instance
     * @param app_state Application state
     */
    static void HandleDroppedFiles(
        UIManager& ui_manager,
        AppState& app_state);

    /**
     * Main frame processing orchestration
     * @param params Frame processing parameters
     * @return true to continue, false to exit
     */
    static bool ProcessFrame(FrameProcessingParams& params);

    /**
     * Process frame capture and initial updates
     * @param params Frame processing parameters
     * @param frame_bgr Output BGR frame
     * @return true if successful, false to continue to next frame
     */
    static bool ProcessFrameCaptureAndUpdates(FrameProcessingParams& params, cv::Mat& frame_bgr);

    /**
     * Process MediaPipe and effects for frame
     * @param params Frame processing parameters
     * @param frame_bgr Input BGR frame
     * @param display_rgb Output RGB display frame
     * @return true if successful, false on error
     */
    static bool ProcessFrameMediaPipeAndEffects(FrameProcessingParams& params, const cv::Mat& frame_bgr, cv::Mat& display_rgb);

    /**
     * Process UI events and render frame
     * @param params Frame processing parameters
     * @param display_rgb RGB frame to render
     * @return true to continue, false to exit
     */
    static bool ProcessFrameUIAndRender(FrameProcessingParams& params, const cv::Mat& display_rgb);
};

} // namespace segmecam

#endif // FRAME_PROCESSOR_H