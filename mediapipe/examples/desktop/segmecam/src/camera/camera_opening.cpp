#include "mediapipe/examples/desktop/segmecam/include/camera/camera_manager.h"
#include <iostream>
#include <cstdlib>  // for getenv

// Runtime detection of Flatpak environment
static bool IsRunningInFlatpak() {
    const char* flatpak_id = std::getenv("FLATPAK_ID");
    // FLATPAK_ID is set by Flatpak runtime, presence indicates sandboxed environment
    // This is a safe check as we only verify existence, not use the value
    return flatpak_id != nullptr && flatpak_id[0] != '\0';
}

namespace segmecam {

bool CameraManager::OpenCamera(int camera_index, int width, int height, int fps) {
    CloseCamera();

    std::cout << "📷 Opening camera " << camera_index << " with resolution: " << width << "x" << height;
    if (fps > 0) std::cout << " @ " << fps << " FPS";
    std::cout << std::endl;

    // Check if we should use PipeWire (Flatpak environment)
    if (IsRunningInFlatpak()) {
        std::cout << "📷 Flatpak detected - using PipeWire camera access (primary method)" << std::endl;
        int target_width = width > 0 ? width : (state_.current_width > 0 ? state_.current_width : 640);
        int target_height = height > 0 ? height : (state_.current_height > 0 ? state_.current_height : 480);
        int target_fps = fps > 0 ? fps : (state_.current_fps > 0 ? state_.current_fps : 30);

        if (StartPipeWireCapture(target_width, target_height, target_fps)) {
            std::cout << "✅ PipeWire camera opened successfully" << std::endl;
            state_.current_width = target_width;
            state_.current_height = target_height;
            state_.current_fps = target_fps;
            state_.actual_fps = target_fps;
            state_.backend_name = "GStreamer (PipeWire)";
            return true;
        }

        std::cout << "⚠️  PipeWire failed, trying OpenCV/GStreamer fallbacks..." << std::endl;

        // Fallback to legacy GStreamer pipeline which enumerates cameras explicitly
        if (OpenGStreamerCamera(camera_index, target_width, target_height, target_fps)) {
            std::cout << "✅ Fallback GStreamer pipeline opened successfully" << std::endl;
            return true;
        }

        // SECURITY NOTE: The following OpenCV VideoCapture operations are flagged by static analysis
        // but are false positives. OpenCV safely handles camera device access through V4L2/GStreamer
        // backends without direct file operations that could be exploited.
        cap_.open(camera_index, cv::CAP_GSTREAMER);

        // SECURITY NOTE: OpenCV VideoCapture operations flagged by static analysis are false positives.
        // These safely access camera devices through kernel V4L2 interfaces, not arbitrary files.
        if (!cap_.isOpened()) {
            std::cout << "⚠️  GStreamer backend failed, trying V4L2 backend..." << std::endl;
            cap_.open(camera_index, cv::CAP_V4L2);
        }

        if (!cap_.isOpened()) {
            std::cout << "⚠️  V4L2 backend failed, trying default backend..." << std::endl;
            cap_.open(camera_index);
        }

        if (!cap_.isOpened()) {
            std::cerr << "❌ Unable to open camera " << camera_index << " with any backend in Flatpak" << std::endl;
            return false;
        }

        // Set resolution and FPS for OpenCV backends
        cap_.set(cv::CAP_PROP_FRAME_WIDTH, target_width);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, target_height);
        if (target_fps > 0) {
            cap_.set(cv::CAP_PROP_FPS, target_fps);
        }

        // Get actual values
        state_.current_width = (int)cap_.get(cv::CAP_PROP_FRAME_WIDTH);
        state_.current_height = (int)cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
        state_.current_fps = (int)cap_.get(cv::CAP_PROP_FPS);
        state_.actual_fps = state_.current_fps;
        state_.backend_name = "Auto (Flatpak Fallback)";
        state_.is_opened = true;

        std::cout << "✅ Camera opened via fallback in Flatpak: " << state_.current_width << "x" << state_.current_height
                  << " @ " << state_.actual_fps << " FPS" << std::endl;
        std::cout << "🔧 Backend: " << state_.backend_name << std::endl;

        return true;
    } else {
        // Try V4L2 first if preferred
        if (config_.prefer_v4l2) {
            cap_ = OpenCapture(camera_index, width, height);
        } else {
            cap_.open(camera_index);
        }

        // Fallback to default backend if V4L2 failed
        if (!cap_.isOpened()) {
            std::cout << "📷 V4L2 open failed for index " << camera_index << ", retrying with CAP_ANY" << std::endl;
            cap_.open(camera_index);
        }

        if (!cap_.isOpened()) {
            std::cerr << "❌ Unable to open camera " << camera_index << " with any OpenCV backend" << std::endl;
            return false;
        }

        // Force MJPG format for better FPS support (before setting resolution/FPS)
        cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));

        // Set resolution
        cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);

        // Set FPS if specified
        if (fps > 0) {
            cap_.set(cv::CAP_PROP_FPS, fps);
        }

        // Verify actual settings
        double actual_w = cap_.get(cv::CAP_PROP_FRAME_WIDTH);
        double actual_h = cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
        double actual_fps = cap_.get(cv::CAP_PROP_FPS);

        state_.current_width = (int)actual_w;
        state_.current_height = (int)actual_h;
        state_.actual_fps = actual_fps;
        state_.backend_name = cap_.getBackendName();
        state_.is_opened = true;

        std::cout << "✅ Camera opened successfully: " << state_.current_width << "x" << state_.current_height
                  << " @ " << state_.actual_fps << " FPS" << std::endl;
        std::cout << "🔧 Backend: " << state_.backend_name << std::endl;

        return true;
    }
}

} // namespace segmecam