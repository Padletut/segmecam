#include "include/application/frame_processor.h"
#include "include/application/mediapipe_processor.h"
#include "include/application/render_utils.h"
#include "include/application/portal_utils.h"
#include "include/application/application_run.h"
#include "include/effects/effects_manager.h"
#include "include/ui/ui_manager_enhanced.h"
#include "include/ui/ui_utils.h"
#include "include/application/app_state.h"

// Include MediaPipe for graph operations
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"

// Include OpenCV for image processing
#include <opencv2/opencv.hpp>

// Include SDL for timing
#include <SDL.h>

#include <iostream>
#include <chrono>
#include <thread>

namespace segmecam {

void FrameProcessor::MatToImageFrame(const cv::Mat& mat_bgr, std::unique_ptr<mediapipe::ImageFrame>& frame) {
    frame = std::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, mat_bgr.cols, mat_bgr.rows);
    cv::Mat frame_rgb;
    cv::cvtColor(mat_bgr, frame_rgb, cv::COLOR_BGR2RGB);
    frame_rgb.copyTo(cv::Mat(frame->Height(), frame->Width(), CV_8UC3, frame->MutablePixelData()));
}

bool FrameProcessor::ProcessFrameCapture(CameraManager& camera_mgr, cv::Mat& frame_bgr, int frame_count) {
    if (frame_count <= 5) {
        std::cout << "📸 Calling CaptureFrame()..." << std::endl;
    }
    if (!camera_mgr.CaptureFrame(frame_bgr) || frame_bgr.empty()) {
        if (frame_count < 10) {  // Only log first few failures
            std::cout << "⚠️  Frame capture failed or empty on frame " << frame_count << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return false;
    }

    if (frame_count <= 5) {
        std::cout << "✅ Frame " << frame_count << " captured successfully: " << frame_bgr.cols << "x" << frame_bgr.rows << std::endl;
        // Debug: Show camera frame format and sample pixel values
        if (!frame_bgr.empty()) {
            cv::Vec3b center_pixel = frame_bgr.at<cv::Vec3b>(frame_bgr.rows/2, frame_bgr.cols/2);
            cv::Vec3b corner_pixel = frame_bgr.at<cv::Vec3b>(10, 10);
            std::cout << "🔍 CAMERA DEBUG - Frame format: " << frame_bgr.type() << " (CV_8UC3=" << CV_8UC3 << ")" << std::endl;
            std::cout << "🔍 CAMERA DEBUG - Center pixel BGR(" << frame_bgr.rows/2 << "," << frame_bgr.cols/2 << "): ["
                      << (int)center_pixel[0] << "," << (int)center_pixel[1] << "," << (int)center_pixel[2] << "]" << std::endl;
            std::cout << "🔍 CAMERA DEBUG - Corner pixel BGR(10,10): ["
                      << (int)corner_pixel[0] << "," << (int)corner_pixel[1] << "," << (int)corner_pixel[2] << "]" << std::endl;
        }
    }
    return true;
}

void FrameProcessor::UpdateFPSTracking(double& fps, uint64_t& fps_frames, uint32_t& fps_last_ms) {
    fps_frames++;
    uint32_t now_ms = SDL_GetTicks();
    if (now_ms - fps_last_ms >= 500) {
        fps = (double)fps_frames * 1000.0 / (double)(now_ms - fps_last_ms);
        fps_frames = 0;
        fps_last_ms = now_ms;
    }
}

void FrameProcessor::UpdateCameraInfo(FrameProcessingParams& params) {
    params.app_state.fps = params.fps;
    params.app_state.camera_width = params.managers.camera->GetCurrentWidth();
    params.app_state.camera_height = params.managers.camera->GetCurrentHeight();
    params.app_state.camera_fps = params.managers.camera->GetCurrentFPS();
}

void FrameProcessor::UpdateAutoFPS(FrameProcessingParams& params) {
    static float last_camera_fps = -1.0f;
    if (std::abs(params.app_state.camera_fps - last_camera_fps) > 0.1f) {
        params.managers.effects->UpdateTargetFPSFromCamera(params.app_state.camera_fps);
        params.app_state.target_fps = params.managers.effects->GetTargetFPS();
        last_camera_fps = params.app_state.camera_fps;
    }
}

void FrameProcessor::UpdateAutoProcessingScale(FrameProcessingParams& params) {
    if (params.app_state.auto_processing_scale && params.fps > 0.0) {
        params.managers.effects->UpdateAutoProcessingScale(params.fps);
        params.app_state.current_fps = params.managers.effects->GetCurrentFPS();
        params.app_state.fx_adv_scale = params.managers.effects->GetProcessingScale();
    }
}

bool FrameProcessor::SendFrameToMediaPipe(const cv::Mat& frame_bgr, FrameProcessingParams& params) {
    std::unique_ptr<mediapipe::ImageFrame> frame;
    MatToImageFrame(frame_bgr, frame);
    auto ts = mediapipe::Timestamp(params.frame_id++);
    auto st = params.mediapipe_graph->AddPacketToInputStream("input_video", mediapipe::Adopt(frame.release()).At(ts));
    if (!st.ok()) {
        std::cerr << "❌ AddPacket failed: " << st.message() << std::endl;
        return false;
    }
    return true;
}

cv::Mat FrameProcessor::ProcessAndDisplayFrame(
    const cv::Mat& frame_bgr,
    const cv::Mat& last_mask_u8,
    const mediapipe::NormalizedLandmarkList* landmarks_ptr,
    EffectsManager& effects_mgr,
    AppState& app_state,
    bool have_lms) {

    cv::Mat processed_frame = frame_bgr;

    // Apply effects if EffectsManager is available
    if (!last_mask_u8.empty() || have_lms) {
        // Use EffectsManager to process the frame with segmentation mask and face landmarks
        processed_frame = effects_mgr.ProcessFrame(frame_bgr, last_mask_u8, landmarks_ptr);
    }

    // EffectsManager now returns RGB directly, no conversion needed
    cv::Mat display_rgb = processed_frame.clone();

    // Update app state with current frame
    app_state.last_display_rgb = display_rgb.clone();

    return display_rgb;
}

void FrameProcessor::handle_auto_vcam_selection(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {

    if (!app_state.vcam.IsOpen() && app_state.vcam_auto_start) {
        auto vcam_list = camera_mgr.GetVCamList();
        if (!vcam_list.empty()) {
            app_state.ui_vcam_idx = 0;
            app_state.virtual_camera_path = vcam_list[0].path;
            app_state.vcam.Open(vcam_list[0].path, display_rgb.cols, display_rgb.rows);
            std::cout << "📹 Auto-selected virtual camera: " << vcam_list[0].name << " (" << vcam_list[0].path << ")" << std::endl;
        }
    }
}

void FrameProcessor::handle_vcam_resize(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {

    if (app_state.vcam.IsOpen() && (display_rgb.cols != app_state.vcam.Width() || display_rgb.rows != app_state.vcam.Height())) {
        auto vcam_list = camera_mgr.GetVCamList();
        if (app_state.ui_vcam_idx >= 0 && app_state.ui_vcam_idx < (int)vcam_list.size()) {
            app_state.vcam.Open(vcam_list[app_state.ui_vcam_idx].path, display_rgb.cols, display_rgb.rows);
        }
    }
}

void FrameProcessor::handle_frame_output(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {

    cv::Mat display_bgr;
    cv::cvtColor(display_rgb, display_bgr, cv::COLOR_RGB2BGR);

    if (app_state.vcam.IsOpen()) {
        std::cout << "VCam: About to write frame to vcam" << std::endl;
        app_state.vcam.WriteBGR(display_bgr);
    }

    if (camera_mgr.IsPipeWireOutputActive()) {
        camera_mgr.SendFrameToPipeWire(display_bgr);
    }
}

void FrameProcessor::HandleVirtualCameraOutput(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {

    if (display_rgb.empty()) {
        return;
    }

    handle_auto_vcam_selection(camera_mgr, app_state, display_rgb);
    handle_vcam_resize(camera_mgr, app_state, display_rgb);
    handle_frame_output(camera_mgr, app_state, display_rgb);
}

void FrameProcessor::HandleDroppedFiles(
    UIManager& ui_manager,
    AppState& app_state) {

    auto dropped_files = ui_manager.GetDroppedFiles();
    for (const auto& file_path : dropped_files) {
        std::cout << "🖼️  Processing dropped file: " << file_path << std::endl;
        // Load image and set as background
        cv::Mat bg_image;
        std::string resolved_path;
        if (LoadBackgroundImageWithPortal(file_path, bg_image, resolved_path)) {
            app_state.bg_image = bg_image.clone();
            app_state.bg_mode = 2; // Image mode
            // Update the background path for profile persistence
            // Safe string copy with null-termination guarantee
            segmecam::ui_utils::SafeStringCopy(app_state.bg_path_buf, sizeof(app_state.bg_path_buf), resolved_path);
            std::cout << "✅ Background image loaded: " << bg_image.cols << "x" << bg_image.rows
                      << " (auto-switched to Image mode)" << std::endl;
            std::cout << "🔖 Background path saved: " << app_state.bg_path_buf << std::endl;
        } else {
            std::cout << "❌ Failed to load dropped file as image after portal fallback." << std::endl;
        }
    }
}

bool FrameProcessor::ProcessFrame(FrameProcessingParams& params) {
    cv::Mat frame_bgr;
    cv::Mat display_rgb;

    // Process frame capture and initial updates
    if (!ProcessFrameCaptureAndUpdates(params, frame_bgr)) {
        return true; // Continue to next frame
    }

    // Process MediaPipe and effects
    if (!ProcessFrameMediaPipeAndEffects(params, frame_bgr, display_rgb)) {
        return false; // Exit on error
    }

    // Handle UI events and rendering
    return ProcessFrameUIAndRender(params, display_rgb);
}

bool FrameProcessor::ProcessFrameCaptureAndUpdates(FrameProcessingParams& params, cv::Mat& frame_bgr) {
    // Process frame capture
    if (!ProcessFrameCapture(*params.managers.camera, frame_bgr, params.frame_count)) {
        return false; // Continue to next frame
    }

    // Update FPS tracking
    UpdateFPSTracking(params.fps, params.fps_frames, params.fps_last_ms);

    // Update camera information in app state
    UpdateCameraInfo(params);

    // Update auto FPS settings if camera changed
    UpdateAutoFPS(params);

    // Update auto processing scale if enabled
    UpdateAutoProcessingScale(params);

    return true;
}

bool FrameProcessor::ProcessFrameMediaPipeAndEffects(FrameProcessingParams& params, const cv::Mat& frame_bgr, cv::Mat& display_rgb) {
    // Send frame to MediaPipe graph
    if (!SendFrameToMediaPipe(frame_bgr, params)) {
        return false; // Exit on error
    }

    // Process MediaPipe outputs
    MediaPipeOutputData output_data;
    MediaPipeProcessor::ProcessMediaPipeOutputs(output_data, params.mask_poller, params.multi_face_landmarks_poller, params.face_rects_poller,
                           params.app_state, params.frame_count, params.has_landmarks);

    // Sync UI settings to EffectsManager before processing effects
    segmecam::ApplicationRun::SyncSettingsToEffectsManager(*params.managers.effects, params.app_state);

    // Process frame effects and prepare for display
    const mediapipe::NormalizedLandmarkList* landmarks_ptr = (output_data.have_lms) ? &output_data.latest_lms : nullptr;
    display_rgb = ProcessAndDisplayFrame(frame_bgr, output_data.last_mask_u8, landmarks_ptr,
                                        *params.managers.effects, params.app_state, output_data.have_lms);

    // Handle virtual camera output
    HandleVirtualCameraOutput(*params.managers.camera, params.app_state, display_rgb);

    return true;
}

bool FrameProcessor::ProcessFrameUIAndRender(FrameProcessingParams& params, const cv::Mat& display_rgb) {
    // Let UIManager handle events first
    if (!params.ui_manager.ProcessEvents(params.running)) {
        std::cout << "🛑 UIManager ProcessEvents returned false, exiting..." << std::endl;
        return false;
    }

    // Handle any dropped files
    HandleDroppedFiles(params.ui_manager, params.app_state);

    if (!params.running) {
        std::cout << "🛑 Running flag set to false by event handler, exiting..." << std::endl;
        return false;
    }

    // Render complete frame
    RenderUtils::RenderFrame(params.ui_manager, display_rgb, params.window, params.frame_count, params.running);

    return true;
}

} // namespace segmecam