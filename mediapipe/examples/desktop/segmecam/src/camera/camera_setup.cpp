#include "include/camera/camera_setup.h"
#include "include/camera/camera_manager.h"

#include <iostream>
#include <algorithm>

namespace segmecam {

CameraSetup::CameraSetup() = default;

void CameraSetup::Initialize(const CameraConfig& config, const std::vector<CameraDesc>& cameras, CameraState& state) {
    // Camera setup initialization - currently no-op
    std::cout << "⚙️  Camera setup initialized" << std::endl;
}

void CameraSetup::SelectInitialCamera(const std::vector<CameraDesc>& cameras, const CameraConfig& config, CameraState& state) {
    // Find the requested camera index in the enumerated list
    for (size_t i = 0; i < cameras.size(); ++i) {
        if (cameras[i].index == config.default_camera_index) {
            state.ui_cam_idx = (int)i;
            break;
        }
    }
}

void CameraSetup::SelectInitialResolution(const std::vector<CameraDesc>& cameras, const CameraConfig& config, CameraState& state) {
    // Set initial resolution from available cameras
    if (!cameras.empty() && !cameras[state.ui_cam_idx].resolutions.empty()) {
        auto resolutions = cameras[state.ui_cam_idx].resolutions;

        // Try to find matching resolution or use the largest available
        int best_res_idx = FindBestResolutionIndex(resolutions, config.default_width, config.default_height);

        state.ui_res_idx = best_res_idx;
        auto wh = resolutions[best_res_idx];
        state.current_width = wh.first;
        state.current_height = wh.second;
    }
}

void CameraSetup::SetupCameraPathAndFPS(const std::vector<CameraDesc>& cameras, const CameraConfig& config, CameraState& state, std::vector<int>& fps_options) {
    // Setup camera path and FPS options
    if (!cameras.empty()) {
        state.current_camera_path = cameras[state.ui_cam_idx].path;
        UpdateFPSOptions(state.current_camera_path, state.current_width, state.current_height, fps_options);

        // Find best FPS option
        if (!fps_options.empty()) {
            int best_fps_idx = FindBestFPSIndex(fps_options, config.default_fps);
            state.ui_fps_idx = best_fps_idx;
            state.current_fps = fps_options[best_fps_idx];
        }
    }
}

void CameraSetup::UpdateFPSOptions(const std::string& cam_path, int width, int height, std::vector<int>& fps_options) {
    if (cam_path == "PipeWire" || cam_path == "pipewire") {
        fps_options = {15, 24, 30, 45, 60};
        return;
    }

    // This would call the external EnumerateFPS function
    // For now, we'll set some defaults
    fps_options = {15, 24, 30};
}

void CameraSetup::LogInitializationSuccess(const CameraState& state) {
    std::cout << "✅ Camera Manager initialized successfully!" << std::endl;
    std::cout << "📷 Using camera: " << state.current_camera_path << std::endl;
    std::cout << "📐 Resolution: " << state.current_width << "x" << state.current_height << std::endl;
    std::cout << "🎬 FPS: " << state.current_fps << std::endl;
    std::cout << "🔧 Backend: " << state.backend_name << std::endl;
}

int CameraSetup::FindBestResolutionIndex(const std::vector<std::pair<int, int>>& resolutions, int width, int height) const {
    // Try to find exact match first
    for (size_t i = 0; i < resolutions.size(); ++i) {
        if (resolutions[i].first == width && resolutions[i].second == height) {
            return (int)i;
        }
    }

    // Default to largest resolution
    return (int)resolutions.size() - 1;
}

int CameraSetup::FindBestFPSIndex(const std::vector<int>& fps_options, int target_fps) const {
    // Try to find exact match first
    for (size_t i = 0; i < fps_options.size(); ++i) {
        if (fps_options[i] == target_fps) {
            return (int)i;
        }
    }

    // Default to highest FPS
    return (int)fps_options.size() - 1;
}

} // namespace segmecam