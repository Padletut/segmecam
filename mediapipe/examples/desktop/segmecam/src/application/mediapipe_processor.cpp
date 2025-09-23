#include "include/application/mediapipe_processor.h"
#include "segmecam_composite.h"
#include "include/application/app_state.h"

// Include MediaPipe for output stream polling
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"

// Include OpenCV for image processing
#include <opencv2/opencv.hpp>

#include <iostream>
#include <vector>

namespace segmecam {

void MediaPipeProcessor::ProcessMediaPipeOutputs(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    AppState& app_state,
    int frame_count,
    bool has_landmarks
) {
    // Process mask output
    ProcessMaskOutput(output_data, mask_poller, app_state, frame_count);

    // Process face landmarks if available
    if (has_landmarks && multi_face_landmarks_poller) {
        ProcessFaceLandmarks(output_data, multi_face_landmarks_poller, face_rects_poller, frame_count);
    }
}

void MediaPipeProcessor::ProcessMaskOutput(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
    AppState& app_state,
    int frame_count
) {
    mediapipe::Packet pkt;
    while (mask_poller->QueueSize() > 0 && mask_poller->Next(&pkt)) {
        const auto& mask = pkt.Get<mediapipe::ImageFrame>();
        static bool first_mask_info = false;
        output_data.last_mask_u8 = DecodeMaskToU8(mask, &first_mask_info);
        app_state.last_mask_u8 = output_data.last_mask_u8.clone();

        if (frame_count <= 5) {
            double min_val, max_val;
            cv::minMaxLoc(output_data.last_mask_u8, &min_val, &max_val);
            std::cout << "✅ Mask received: " << mask.Width() << "x" << mask.Height()
                      << " (format: " << mask.NumberOfChannels() << "ch, " << mask.ByteDepth() << "bd -> 8UC1: " << min_val << "-" << max_val << ")" << std::endl;
        }
    }
}

void MediaPipeProcessor::ProcessFaceLandmarks(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    int frame_count
) {
    try {
        ProcessFaceLandmarksData(output_data, multi_face_landmarks_poller, face_rects_poller, frame_count);
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception during landmarks polling: " << e.what() << std::endl;
    }
}

void MediaPipeProcessor::ProcessFaceLandmarksData(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    int frame_count
) {
    // Process landmark packets
    ProcessLandmarkPackets(output_data, multi_face_landmarks_poller, frame_count);

    // Process face rects if available
    if (face_rects_poller) {
        ProcessFaceRects(output_data, face_rects_poller);
    }
}

void MediaPipeProcessor::ProcessLandmarkPackets(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    int frame_count
) {
    mediapipe::Packet lp;
    int queue_size = multi_face_landmarks_poller->QueueSize();
    if (frame_count <= 5) {
        std::cout << "📍 Landmarks queue size: " << queue_size << std::endl;
    }

    while (queue_size > 0 && multi_face_landmarks_poller->Next(&lp)) {
        try {
            const auto& v = lp.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
            if (!v.empty()) {
                output_data.latest_lms = v[0];
                output_data.have_lms = true;
                if (frame_count <= 5) {
                    std::cout << "✅ Got landmarks with " << output_data.latest_lms.landmark_size() << " points" << std::endl;
                }
            }
            queue_size = multi_face_landmarks_poller->QueueSize();
        } catch (const std::exception& e) {
            std::cerr << "❌ Error processing landmarks packet: " << e.what() << std::endl;
            break;
        }
    }
}

void MediaPipeProcessor::ProcessFaceRects(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller
) {
    mediapipe::Packet rp;
    while (face_rects_poller->QueueSize() > 0 && face_rects_poller->Next(&rp)) {
        output_data.latest_rects = rp.Get<std::vector<mediapipe::NormalizedRect>>();
    }
}

} // namespace segmecam