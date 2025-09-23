#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_utils.h"
#include <iostream>
#include <cstdlib>  // for getenv

namespace segmecam {

// Helper method to log camera opening attempt
void CameraManager::LogCameraOpening(int camera_index, int width, int height, int fps) {
    std::cout << "📷 Opening camera " << camera_index << " with resolution: " << width << "x" << height;
    if (fps > 0) std::cout << " @ " << fps << " FPS";
    std::cout << std::endl;
}

// Helper method to prepare camera parameters with defaults
CameraOpeningParams CameraManager::PrepareCameraParameters(int camera_index, int width, int height, int fps) {
    CameraOpeningParams params;
    params.camera_index = camera_index;
    params.width = width > 0 ? width : (state_.current_width > 0 ? state_.current_width : 640);
    params.height = height > 0 ? height : (state_.current_height > 0 ? state_.current_height : 480);
    params.fps = fps > 0 ? fps : (state_.current_fps > 0 ? state_.current_fps : 30);
    return params;
}

// Helper method to open camera in Flatpak environment
bool CameraManager::OpenCameraInFlatpak(const CameraOpeningParams& params) {
    std::cout << "📷 Flatpak detected - using PipeWire camera access (primary method)" << std::endl;

    // Try PipeWire first
    if (TryOpenPipeWireCapture(params.width, params.height, params.fps)) {
        return true;
    }

    // Try OpenCV fallbacks
    return TryOpenOpenCVFallback(params.camera_index, params.width, params.height, params.fps);
}

// Helper method to open camera natively (non-Flatpak)
bool CameraManager::OpenCameraNatively(const CameraOpeningParams& params) {
    return TryOpenNativeCamera(params.camera_index, params.width, params.height, params.fps);
}

bool CameraManager::OpenCamera(int camera_index, int width, int height, int fps) {
    CloseCamera();
    LogCameraOpening(camera_index, width, height, fps);

    CameraOpeningParams params = PrepareCameraParameters(camera_index, width, height, fps);

    if (IsRunningInFlatpak()) {
        return OpenCameraInFlatpak(params);
    } else {
        return OpenCameraNatively(params);
    }
}

// Helper method to try opening PipeWire camera capture
bool CameraManager::TryOpenPipeWireCapture(int width, int height, int fps) {
    if (StartPipeWireCapture(width, height, fps)) {
        std::cout << "✅ PipeWire camera opened successfully" << std::endl;
        UpdateCameraStateFromCapture("GStreamer (PipeWire)");
        return true;
    }
    return false;
}

// Helper method to try OpenCV fallback for Flatpak
bool CameraManager::TryOpenOpenCVFallback(int camera_index, int width, int height, int fps) {
    std::cout << "⚠️  PipeWire failed, trying OpenCV/GStreamer fallbacks..." << std::endl;

    // Fallback to legacy GStreamer pipeline which enumerates cameras explicitly
    if (OpenGStreamerCamera(camera_index, width, height, fps)) {
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

    ConfigureCameraProperties(width, height, fps);
    UpdateCameraStateFromCapture("Auto (Flatpak Fallback)");

    std::cout << "✅ Camera opened via fallback in Flatpak: " << state_.current_width << "x" << state_.current_height
              << " @ " << state_.actual_fps << " FPS" << std::endl;
    std::cout << "🔧 Backend: " << state_.backend_name << std::endl;

    return true;
}

// Helper method to try opening native camera (non-Flatpak)
bool CameraManager::TryOpenNativeCamera(int camera_index, int width, int height, int fps) {
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

    ConfigureCameraProperties(width, height, fps);
    UpdateCameraStateFromCapture(cap_.getBackendName());

    std::cout << "✅ Camera opened successfully: " << state_.current_width << "x" << state_.current_height
              << " @ " << state_.actual_fps << " FPS" << std::endl;
    std::cout << "🔧 Backend: " << state_.backend_name << std::endl;

    return true;
}

// Helper method to configure camera properties
void CameraManager::ConfigureCameraProperties(int width, int height, int fps) {
    // Force MJPG format for better FPS support (before setting resolution/FPS)
    cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));

    // Set resolution
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    // Set FPS if specified
    if (fps > 0) {
        cap_.set(cv::CAP_PROP_FPS, fps);
    }
}

// Helper method to update camera state from capture
void CameraManager::UpdateCameraStateFromCapture(const std::string& backend_name) {
    // Get actual values
    double actual_w = cap_.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_h = cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
    double actual_fps = cap_.get(cv::CAP_PROP_FPS);

    state_.current_width = (int)actual_w;
    state_.current_height = (int)actual_h;
    state_.actual_fps = actual_fps;
    state_.backend_name = backend_name;
    state_.is_opened = true;
}

} // namespace segmecam