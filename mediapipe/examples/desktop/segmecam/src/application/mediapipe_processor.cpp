#include "include/application/mediapipe_processor.h"
#include "include/render/segmecam_composite.h"
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
    std::unique_ptr<mediapipe::OutputStreamPoller>& blendshapes_poller,
    AppState& app_state,
    int frame_count,
    bool has_landmarks
) {
    // Process mask output
    ProcessMaskOutput(output_data, mask_poller, app_state, frame_count);

    // Process face landmarks if available
    if (has_landmarks && multi_face_landmarks_poller) {
        ProcessFaceLandmarks(output_data, multi_face_landmarks_poller, face_rects_poller, frame_count);
        
        // Process face mesh from landmarks (Phase 1 & 2)
        ProcessFaceMesh(output_data, app_state, frame_count);
    }

    // Process blendshapes if available
    if (has_landmarks && blendshapes_poller) {
        ProcessBlendshapes(blendshapes_poller, app_state, frame_count);
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

void MediaPipeProcessor::ProcessBlendshapes(
    std::unique_ptr<mediapipe::OutputStreamPoller>& blendshapes_poller,
    AppState& app_state,
    int frame_count
) {
    mediapipe::Packet bp;
    int queue_size = blendshapes_poller->QueueSize();
    if (frame_count <= 5) {
        std::cout << "🎭 Blendshapes queue size: " << queue_size << std::endl;
    }

    // Process all pending blendshape packets (use latest)
    while (queue_size > 0 && blendshapes_poller->Next(&bp)) {
        try {
            // Get ClassificationList from MediaPipe
            const auto& classifications = bp.Get<std::vector<mediapipe::ClassificationList>>();
            
            if (!classifications.empty() && classifications[0].classification_size() == 52) {
                // Update blendshapes in app_state
                app_state.blendshapes_processor.Update(classifications[0]);
                app_state.blendshapes = app_state.blendshapes_processor.GetSmoothedBlendshapes();
                app_state.blendshapes_available = true;
                
                if (frame_count <= 5) {
                    std::cout << "✅ Got 52 blendshapes - smile intensity: " 
                              << app_state.blendshapes_processor.GetSmileIntensity() << std::endl;
                }
            } else {
                if (frame_count <= 5) {
                    std::cout << "⚠️  Unexpected blendshape format" << std::endl;
                }
            }
            queue_size = blendshapes_poller->QueueSize();
        } catch (const std::exception& e) {
            std::cerr << "❌ Error processing blendshapes packet: " << e.what() << std::endl;
            app_state.blendshapes_available = false;
            break;
        }
    }
}

void MediaPipeProcessor::ProcessFaceMesh(
    MediaPipeOutputData& output_data,
    AppState& app_state,
    int frame_count
) {
    if (!output_data.have_lms || output_data.latest_lms.landmark_size() != 478) {
        app_state.face_mesh_available = false;
        return;
    }
    
    try {
        // Update face mesh processor with 478 landmarks
        app_state.face_mesh_processor.Update(
            output_data.latest_lms,
            app_state.camera_width,
            app_state.camera_height
        );
        
        // Get processed face mesh data
        app_state.face_mesh = app_state.face_mesh_processor.GetFaceMesh();
        app_state.face_mesh_available = app_state.face_mesh_processor.IsAvailable();
        
        // Update transform calculator with face mesh data (Phase 2: Transform Calculation)
        if (app_state.face_mesh_available) {
            app_state.transform_calculator.Update(app_state.face_mesh);
            app_state.head_pose = app_state.transform_calculator.GetHeadPose();
            app_state.anchor_points = app_state.transform_calculator.GetAnchors();
            app_state.transform_data_available = true;
        } else {
            app_state.transform_data_available = false;
        }
        
        if (frame_count <= 5 && app_state.face_mesh_available) {
            std::cout << "🎯 Face mesh: " << output_data.latest_lms.landmark_size() << " points, "
                      << "yaw=" << app_state.face_mesh_processor.GetYaw() << "°, "
                      << "pitch=" << app_state.face_mesh_processor.GetPitch() << "°, "
                      << "scale=" << app_state.face_mesh_processor.GetFaceScale() << std::endl;
            
            if (app_state.transform_data_available) {
                std::cout << "🎯 Transform: "
                          << "pos=[" << app_state.head_pose.position[0] << "," 
                          << app_state.head_pose.position[1] << "," 
                          << app_state.head_pose.position[2] << "], "
                          << "anchors=" << app_state.anchor_points.size() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Error processing face mesh: " << e.what() << std::endl;
        app_state.face_mesh_available = false;
    }
}

} // namespace segmecam
