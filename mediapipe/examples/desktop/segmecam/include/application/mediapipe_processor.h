#ifndef MEDIAPIPE_PROCESSOR_H
#define MEDIAPIPE_PROCESSOR_H

#include <memory>
#include <opencv2/opencv.hpp>
#include "include/application/app_state.h"

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
 * Utility class for MediaPipe output processing in SegmeCam
 */
class MediaPipeProcessor {
public:
    /**
     * Process all MediaPipe outputs (mask and face landmarks)
     * @param output_data Structure to store processed output data
     * @param mask_poller Poller for segmentation mask output
     * @param multi_face_landmarks_poller Poller for face landmarks output
     * @param face_rects_poller Poller for face rectangles output
     * @param app_state Current application state
     * @param frame_count Current frame count for debugging
     * @param has_landmarks Whether face landmarks are available
     */
    static void ProcessMediaPipeOutputs(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        AppState& app_state,
        int frame_count,
        bool has_landmarks
    );

    /**
     * Process segmentation mask output from MediaPipe
     * @param output_data Structure to store processed output data
     * @param mask_poller Poller for segmentation mask output
     * @param app_state Current application state
     * @param frame_count Current frame count for debugging
     */
    static void ProcessMaskOutput(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        AppState& app_state,
        int frame_count
    );

    /**
     * Process face landmarks output from MediaPipe
     * @param output_data Structure to store processed output data
     * @param multi_face_landmarks_poller Poller for face landmarks output
     * @param face_rects_poller Poller for face rectangles output
     * @param frame_count Current frame count for debugging
     */
    static void ProcessFaceLandmarks(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        int frame_count
    );

    /**
     * Process face landmarks data from pollers
     * @param output_data Structure to store processed output data
     * @param multi_face_landmarks_poller Poller for face landmarks output
     * @param face_rects_poller Poller for face rectangles output
     * @param frame_count Current frame count for debugging
     */
    static void ProcessFaceLandmarksData(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        int frame_count
    );

    /**
     * Process landmark packets from MediaPipe
     * @param output_data Structure to store processed output data
     * @param multi_face_landmarks_poller Poller for face landmarks output
     * @param frame_count Current frame count for debugging
     */
    static void ProcessLandmarkPackets(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        int frame_count
    );

    /**
     * Process face rectangles from MediaPipe
     * @param output_data Structure to store processed output data
     * @param face_rects_poller Poller for face rectangles output
     */
    static void ProcessFaceRects(
        MediaPipeOutputData& output_data,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller
    );
};

} // namespace segmecam

#endif // MEDIAPIPE_PROCESSOR_H