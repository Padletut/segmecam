#include "mediapipe/examples/desktop/segmecam/include/camera/camera_manager.h"
#include <iostream>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include <cstdlib>  // for getenv

// Runtime detection of Flatpak environment
static bool IsRunningInFlatpak() {
    const char* flatpak_id = std::getenv("FLATPAK_ID");
    // FLATPAK_ID is set by Flatpak runtime, presence indicates sandboxed environment
    // This is a safe check as we only verify existence, not use the value
    return flatpak_id != nullptr && flatpak_id[0] != '\0';
}

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

    if (IsRunningInFlatpak()) {
        // Check if using direct GStreamer capture
        if (gst_camera_active_) {
            return CaptureGStreamerFrame(frame);
        }

        // For PipeWire, wait briefly for a frame if one is not yet ready
        std::unique_lock<std::mutex> lock(frame_mutex_);
        static int wait_log_count = 0;

        if (!frame_ready_) {
            if (wait_log_count < 10) {
                std::cout << "⏳ CaptureFrame waiting for PipeWire sample..." << std::endl;
            }

            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(800);
            bool signaled = false;
            while (!frame_ready_ && state_.is_opened) {
                signaled = frame_ready_cv_.wait_until(lock, deadline, [this]() {
                    return frame_ready_ || !state_.is_opened;
                });
                if (frame_ready_ || !state_.is_opened || std::chrono::steady_clock::now() >= deadline) {
                    break;
                }
            }

            if (wait_log_count < 10) {
                std::cout << "⏱️  CaptureFrame wait finished (signaled=" << std::boolalpha << signaled
                          << ", frame_ready=" << frame_ready_ << ", opened=" << state_.is_opened << ")" << std::endl;
                wait_log_count++;
            }
        }

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
    } else {
        // SECURITY NOTE: Validate frame dimensions to prevent buffer overflow issues
        // CWE-120/CWE-20: Check buffer boundaries and input validation
        bool success = cap_.read(frame); // NOLINT - OpenCV manages internal buffers safely
        if (success && !frame.empty() && frame.cols > 0 && frame.rows > 0) {
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

            // Additional validation: ensure frame has valid channels (1-4 for typical formats)
            if (frame.channels() < 1 || frame.channels() > 4) {
                std::cerr << "❌ Invalid frame channels: " << frame.channels()
                          << " (expected 1-4 channels)" << std::endl;
                return false;
            }

            state_.frames_captured++;
            return true;
        }
        return false;
    }
}

} // namespace segmecam