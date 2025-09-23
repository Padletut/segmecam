#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_utils.h"
// cppcheck-suppress missingInclude
#include <iostream>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include <cstdlib>  // for getenv

namespace segmecam {

bool CameraManager::CaptureFrame(cv::Mat& frame) {
    if (!IsOpened()) {
        static int not_open_log = 0;
        if (not_open_log < 5) {
            std::cout << "⚠️  CaptureFrame invoked while camera not opened (flatpak="
                      << std::boolalpha << IsRunningInFlatpak() << ")" << std::endl;
            not_open_log++;
        }
        return false;
    }

    return IsRunningInFlatpak() ? CaptureFrameFlatpak(frame) : CaptureFrameNative(frame);
}

bool CameraManager::CaptureFrameFlatpak(cv::Mat& frame) {
    // If using V4L2 source, pull frames directly from appsink
    if (using_v4l2_source_ && gst_appsink_ && gst_app_sink_pull_sample) {
        return CaptureV4L2Frame(frame);
    }

    // Check if using direct GStreamer capture
    if (gst_camera_active_) {
        return CaptureGStreamerFrame(frame);
    }

    // For PipeWire, wait briefly for a frame if one is not yet ready
    if (!WaitForPipeWireFrame()) {
        return false;
    }

    return ValidateAndCopyFrame(frame);
}

void CameraManager::LogWaitStart() {
    static int wait_log_count = 0;
    if (wait_log_count < 10) {
        std::cout << "⏳ CaptureFrame waiting for PipeWire sample..." << std::endl;
        wait_log_count++;
    }
}

bool CameraManager::ShouldContinueWaiting() {
    return !frame_ready_ && state_.is_opened;
}

bool CameraManager::IsTimeoutExpired(const std::chrono::steady_clock::time_point& deadline) {
    return std::chrono::steady_clock::now() >= deadline;
}

bool CameraManager::WaitForPipeWireFrame() {
    std::unique_lock<std::mutex> lock(frame_mutex_);

    if (!frame_ready_) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(800);

        LogWaitStart();

        while (ShouldContinueWaiting()) {
            frame_ready_cv_.wait_until(lock, deadline, [this]() {
                return frame_ready_ || !state_.is_opened;
            });
            if (frame_ready_ || !state_.is_opened || IsTimeoutExpired(deadline)) {
                break;
            }
        }

    }

    return frame_ready_;
}

bool CameraManager::ValidateAndCopyFrame(cv::Mat& frame) {
    if (!frame_ready_ || current_frame_.empty()) {
        static int empty_log = 0;
        if (empty_log < 10) {
            std::cout << "⚠️  CaptureFrame returning empty frame (ready=" << std::boolalpha << frame_ready_
                      << ", empty=" << current_frame_.empty() << ")" << std::endl;
            empty_log++;
        }
        return false;
    }

    current_frame_.copyTo(frame);
    frame_ready_ = false;
    state_.frames_captured++;
    return true;
}

bool CameraManager::CaptureFrameNative(cv::Mat& frame) {
    // SECURITY NOTE: Validate frame dimensions to prevent buffer overflow issues
    // CWE-120/CWE-20: Check buffer boundaries and input validation
    bool success = cap_.read(frame);
    if (success && !frame.empty() && frame.cols > 0 && frame.rows > 0) {
        return PerformFrameValidation(frame);
    }
    return false;
}

bool CameraManager::PerformFrameValidation(const cv::Mat& frame) {
    // Validate reasonable frame dimensions to prevent memory exhaustion
    // Maximum reasonable dimensions for camera frames (8K resolution limit)
    const int MAX_FRAME_WIDTH = 7680;  // 8K width
    const int MAX_FRAME_HEIGHT = 4320; // 8K height
    const int MAX_FRAME_PIXELS = MAX_FRAME_WIDTH * MAX_FRAME_HEIGHT;

    if (frame.cols > MAX_FRAME_WIDTH || frame.rows > MAX_FRAME_HEIGHT ||
        (frame.cols * frame.rows) > MAX_FRAME_PIXELS) {
        std::cerr << "❌ Invalid frame dimensions: " << frame.cols << "x" << frame.rows
                  << " (max allowed: " << MAX_FRAME_WIDTH << "x" << MAX_FRAME_HEIGHT << ")" << std::endl;
        return false;
    }

    return ValidateFrameChannels(frame);
}

bool CameraManager::ValidateFrameChannels(const cv::Mat& frame) {
    // Additional validation: ensure frame has valid channels (1-4 for typical formats)
    if (frame.channels() < 1 || frame.channels() > 4) {
        std::cerr << "❌ Invalid frame channels: " << frame.channels()
                  << " (expected 1-4 channels)" << std::endl;
        return false;
    }

    state_.frames_captured++;
    return true;
}

bool CameraManager::CaptureV4L2Frame(cv::Mat& frame) {
    if (!gst_appsink_ || !gst_app_sink_pull_sample) {
        std::cerr << "❌ V4L2 capture not properly initialized" << std::endl;
        return false;
    }

    // Check pipeline state
    if (pipeline_ && gst_element_get_state) {
        int current_state, pending_state;
        gst_element_get_state(pipeline_, &current_state, &pending_state, GST_CLOCK_TIME_NONE);
        std::cout << "🔍 V4L2 pipeline state: current=" << current_state << ", pending=" << pending_state << std::endl;
        if (current_state != GST_STATE_PLAYING) {
            std::cout << "⚠️  V4L2 pipeline not in PLAYING state" << std::endl;
            return false;
        }
    } else {
        std::cout << "⚠️  Cannot check pipeline state (no pipeline or function not loaded)" << std::endl;
    }

    // Pull sample from appsink
    GstSample* sample = reinterpret_cast<GstSample*>(gst_app_sink_pull_sample(gst_appsink_));
    if (!sample) {
        static int pull_fail_count = 0;
        if (pull_fail_count < 10) {
            std::cout << "⚠️  Failed to pull V4L2 sample from appsink" << std::endl;
            pull_fail_count++;
        }
        return false;
    }

    // Extract buffer from sample
    GstBuffer* buffer = reinterpret_cast<GstBuffer*>(gst_sample_get_buffer(sample));
    if (!buffer) {
        std::cerr << "❌ Failed to get buffer from V4L2 sample" << std::endl;
        gst_sample_unref(sample);
        return false;
    }

    // Map buffer for reading
    GstMapInfo map_info;
    if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ)) {
        std::cerr << "❌ Failed to map V4L2 buffer" << std::endl;
        gst_sample_unref(sample);
        return false;
    }

    // Get frame dimensions from caps
    GstCaps* caps = reinterpret_cast<GstCaps*>(gst_sample_get_caps(sample));
    if (caps) {
        GstStructure* structure = gst_caps_get_structure(caps, 0);
        if (structure) {
            int width, height;
            if (gst_structure_get_int(structure, "width", &width) &&
                gst_structure_get_int(structure, "height", &height)) {

                // Create OpenCV Mat from buffer data
                // Assuming BGR format as specified in pipeline
                frame = cv::Mat(height, width, CV_8UC3, map_info.data, map_info.size / height);

                if (!frame.empty()) {
                    // Clone the frame to ensure we have our own copy before unmapping
                    frame = frame.clone();
                    gst_buffer_unmap(buffer, &map_info);
                    gst_sample_unref(sample);
                    state_.frames_captured++;
                    return PerformFrameValidation(frame);
                }
            }
        }
    }

    gst_buffer_unmap(buffer, &map_info);
    gst_sample_unref(sample);
    return false;
}

} // namespace segmecam