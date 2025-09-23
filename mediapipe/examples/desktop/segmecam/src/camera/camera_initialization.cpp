#include "mediapipe/examples/desktop/segmecam/include/camera/camera_manager.h"
#include "mediapipe/examples/desktop/segmecam/include/camera/gstreamer_utils.h"
#include <iostream>
#include <cstdlib>  // for getenv

namespace segmecam {

bool CameraManager::TryInitializePipeWire() {
    // Check if running in Flatpak for PipeWire support
    bool use_pipewire = IsRunningInFlatpak();
    if (use_pipewire) {
        std::cout << "📷 Detected Flatpak environment - using PipeWire + Camera Portal" << std::endl;
        // Initialize PipeWire support
        if (!InitializeGStreamer()) {
            std::cout << "⚠️  GStreamer initialization failed, falling back to V4L2" << std::endl;
            return false;
        }
    }
    return use_pipewire;
}

void CameraManager::SetupPipeWireDefaults() {
    // Provide sensible defaults when running under Flatpak
    state_.current_width = config_.default_width > 0 ? config_.default_width : 1280;
    state_.current_height = config_.default_height > 0 ? config_.default_height : 720;
    state_.current_fps = config_.default_fps > 0 ? config_.default_fps : 30;
    state_.current_camera_path = "PipeWire";
    state_.backend_name = "GStreamer (PipeWire)";
}

bool CameraManager::PopulateUIStateForPipeWire() {
    // Populate a virtual camera entry for the UI so resolution/FPS selectors remain functional.
    RefreshCameraList();
    RefreshVCamList();

    if (cam_list_.empty()) {
        return false;
    }

    state_.ui_cam_idx = 0;
    state_.current_camera_path = cam_list_[0].path;

    SelectOptimalResolution();
    SelectOptimalFPS();

    return true;
}

int CameraManager::Initialize(const CameraConfig& config) {
    config_ = config;
    state_ = CameraState{}; // Reset state

    std::cout << "📷 Initializing Camera Manager..." << std::endl;

    // Try PipeWire initialization first (for Flatpak)
    if (TryInitializePipeWire()) {
        SetupPipeWireDefaults();
        
        if (OpenCamera(config_.default_camera_index, state_.current_width, state_.current_height, state_.current_fps)) {
            if (PopulateUIStateForPipeWire()) {
                state_.is_initialized = true;
                std::cout << "✅ Camera Manager initialized for Flatpak/PipeWire" << std::endl;
                return 0;
            }
        }

        std::cout << "⚠️  PipeWire initialization failed, falling back to V4L2" << std::endl;
    }

    // Regular build or fallback - use V4L2 enumeration
    return InitializeV4L2(config);
}

void CameraManager::SelectOptimalResolution() {
    // Select resolution
    if (!cam_list_[0].resolutions.empty()) {
        state_.ui_res_idx = static_cast<int>(cam_list_[0].resolutions.size()) - 1;

        // Try to find matching resolution
        for (size_t i = 0; i < cam_list_[0].resolutions.size(); ++i) {
            if (cam_list_[0].resolutions[i].first == state_.current_width &&
                cam_list_[0].resolutions[i].second == state_.current_height) {
                state_.ui_res_idx = static_cast<int>(i);
                break;
            }
        }

        auto wh = cam_list_[0].resolutions[state_.ui_res_idx];
        state_.current_width = wh.first;
        state_.current_height = wh.second;
    }
}

void CameraManager::SelectOptimalFPS() {
    // Select FPS
    UpdateFPSOptions(state_.current_camera_path, state_.current_width, state_.current_height);
    if (!ui_fps_opts_.empty()) {
        state_.ui_fps_idx = static_cast<int>(ui_fps_opts_.size()) - 1;
        
        // Try to find matching FPS
        for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
            if (ui_fps_opts_[i] == state_.current_fps) {
                state_.ui_fps_idx = static_cast<int>(i);
                break;
            }
        }
        state_.current_fps = ui_fps_opts_[state_.ui_fps_idx];
    }
}

} // namespace segmecam