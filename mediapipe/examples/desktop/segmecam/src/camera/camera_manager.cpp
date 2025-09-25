#include "include/camera/camera_manager.h"
#include "include/camera/gstreamer_utils.h"
#include "include/camera/gstreamer_buffer_utils.h"
#include "include/camera/camera_controls.h"
#include "include/camera/camera_enumeration.h"
#include "include/camera/camera_setup.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace segmecam {

namespace {
constexpr unsigned int kXdpCameraFlagNone = 0;
#include "camera/camera_manager.h"

// Enumerate PipeWire camera nodes using pw-cli
// Implementation moved to camera_pipewire.cpp

// Get PipeWire node ID for a given camera index
// Implementation moved to camera_pipewire.cpp

} // namespace

CameraManager::CameraManager() 
    : camera_controls_(std::make_unique<CameraControls>()),
      camera_enumeration_(std::make_unique<CameraEnumeration>()),
      camera_setup_(std::make_unique<CameraSetup>()) {
    // Constructor - GStreamer will be initialized at runtime if needed
}

CameraManager::~CameraManager() {
    Cleanup();
    CleanupGStreamer();
    if (portal_fd_ >= 0) {
        close(portal_fd_);
        portal_fd_ = -1;
    }
    if (portal_instance_) {
        if (g_object_unref_ptr) {
            g_object_unref_ptr(portal_instance_);
        }
        portal_instance_ = nullptr;
    }
    if (portal_library_handle_) {
        dlclose(portal_library_handle_);
        portal_library_handle_ = nullptr;
    }
}

std::vector<CameraDesc> CameraManager::EnumerateCamerasPortal() {
    return camera_enumeration_->EnumerateCamerasPortal(config_);
}

void CameraManager::LogV4L2InitializationSuccess() {
    std::cout << "✅ Camera Manager initialized successfully!" << std::endl;
    std::cout << "📷 Using camera: " << state_.current_camera_path << std::endl;
    std::cout << "📐 Resolution: " << state_.current_width << "x" << state_.current_height << std::endl;
    std::cout << "🎬 FPS: " << state_.current_fps << std::endl;
    std::cout << "🔧 Backend: " << GetBackendName() << std::endl;
}

int CameraManager::InitializeV4L2(const CameraConfig& config) {
    // Store config
    config_ = config;
    
    // Initialize camera enumeration
    camera_enumeration_->Initialize();
    
    // Enumerate available cameras
    RefreshCameraList();
    RefreshVCamList();
    
    if (cam_list_.empty()) {
        std::cout << "⚠️  No cameras found during enumeration" << std::endl;
        return 1;
    }
    
    // Use CameraSetup for initial camera selection and configuration
    camera_setup_->Initialize(config, cam_list_, state_);
    camera_setup_->SelectInitialCamera(cam_list_, config, state_);
    camera_setup_->SelectInitialResolution(cam_list_, config, state_);
    camera_setup_->SetupCameraPathAndFPS(cam_list_, config, state_, ui_fps_opts_);
    
    // Initialize camera controls
    camera_controls_->Initialize(state_.current_camera_path);
    RefreshControls();
    ApplyDefaultControls();
    
    // Open the camera
    if (!OpenCamera(config.default_camera_index, state_.current_width, state_.current_height, state_.current_fps)) {
        std::cerr << "❌ Failed to open camera " << config.default_camera_index << std::endl;
        return 2;
    }
    
    state_.is_initialized = true;
    LogV4L2InitializationSuccess();
    
    return 0;
}

bool CameraManager::OpenCamera(int camera_index) {
    return OpenCamera(camera_index, state_.current_width, state_.current_height, state_.current_fps);
}

void CameraManager::CloseCamera() {
    if (IsRunningInFlatpak()) {
        StopPipeWireCapture();
    } else {
        if (cap_.isOpened()) {
            cap_.release();
            state_.is_opened = false;
            std::cout << "📷 Camera closed" << std::endl;
        }
    }
}

bool CameraManager::IsOpened() const {
    if (IsRunningInFlatpak()) {
        return state_.is_opened;
    } else {
        return state_.is_opened && cap_.isOpened();
    }
}

void CameraManager::RefreshCameraList() {
    std::cout << "🔍 Enumerating cameras..." << std::endl;
    if (IsRunningInFlatpak()) {
        cam_list_ = camera_enumeration_->EnumerateCamerasPortal(config_);
    } else {
        cam_list_ = camera_enumeration_->EnumerateCameras();
    }
    
    std::cout << "📷 Found " << cam_list_.size() << " camera(s):" << std::endl;
    for (const auto& cam : cam_list_) {
        std::cout << "  • " << cam.name << " (" << cam.path << ") - " 
                  << cam.resolutions.size() << " resolutions" << std::endl;
    }
}

void CameraManager::RefreshVCamList() {
    std::cout << "🔍 Enumerating virtual cameras..." << std::endl;
    vcam_list_ = camera_enumeration_->EnumerateLoopbackDevices();
    
    std::cout << "📹 Found " << vcam_list_.size() << " virtual camera(s):" << std::endl;
    for (const auto& vcam : vcam_list_) {
        std::cout << "  • " << vcam.name << " (" << vcam.path << ")" << std::endl;
    }
}

bool CameraManager::SetCurrentCamera(int ui_cam_idx, int ui_res_idx, int ui_fps_idx) {
    if (ui_cam_idx < 0 || ui_cam_idx >= (int)cam_list_.size()) {
        return false;
    }
    
    const auto& cam = cam_list_[ui_cam_idx];
    if (ui_res_idx < 0 || ui_res_idx >= (int)cam.resolutions.size()) {
        return false;
    }
    
    state_.ui_cam_idx = ui_cam_idx;
    state_.ui_res_idx = ui_res_idx;
    state_.ui_fps_idx = ui_fps_idx;
    
    // Update current settings
    state_.current_camera_path = cam.path;
    auto wh = cam.resolutions[ui_res_idx];
    state_.current_width = wh.first;
    state_.current_height = wh.second;
    
    // Update FPS options for new resolution
    UpdateFPSOptions(state_.current_camera_path, state_.current_width, state_.current_height);
    
    // Validate and set FPS
    if (ui_fps_idx >= 0 && ui_fps_idx < (int)ui_fps_opts_.size()) {
        state_.current_fps = ui_fps_opts_[ui_fps_idx];
    } else if (!ui_fps_opts_.empty()) {
        state_.ui_fps_idx = (int)ui_fps_opts_.size() - 1;
        state_.current_fps = ui_fps_opts_[state_.ui_fps_idx];
    }
    
    // Refresh controls for new camera
    RefreshControls();
    
    return true;
}

const std::vector<std::pair<int,int>>& CameraManager::GetCurrentResolutions() const {
    static std::vector<std::pair<int,int>> empty;
    if (state_.ui_cam_idx >= 0 && state_.ui_cam_idx < (int)cam_list_.size()) {
        return cam_list_[state_.ui_cam_idx].resolutions;
    }
    return empty;
}

bool CameraManager::SetResolution(int width, int height) {
    if (!IsOpened()) return false;
    
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    
    // Verify what was actually set
    double actual_w = cap_.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_h = cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
    
    state_.current_width = (int)actual_w;
    state_.current_height = (int)actual_h;
    
    return (state_.current_width == width && state_.current_height == height);
}

bool CameraManager::SetFPS(int fps) {
    if (!IsOpened() || cam_list_.empty() || state_.ui_cam_idx >= (int)cam_list_.size()) {
        return false;
    }
    
    // Store current settings
    int current_camera_index = cam_list_[state_.ui_cam_idx].index;
    int current_width = state_.current_width;
    int current_height = state_.current_height;
    
    // Close and reopen with new FPS
    CloseCamera();
    bool success = OpenCamera(current_camera_index, current_width, current_height, fps);
    
    if (success) {
        state_.current_fps = fps;
        std::cout << "✅ FPS changed to " << state_.actual_fps << " (requested: " << fps << ")" << std::endl;
    } else {
        std::cerr << "❌ Failed to change FPS to " << fps << std::endl;
        // Try to reopen with original settings
        OpenCamera(current_camera_index, current_width, current_height, state_.current_fps);
    }
    
    return success;
}

void CameraManager::RefreshControls() {
    camera_controls_->RefreshControls(state_.current_camera_path);
}

void CameraManager::ApplyDefaultControls() {
    if (!config_.enable_auto_focus || state_.current_camera_path.empty()) return;
    
    camera_controls_->ApplyDefaultControls(state_.current_camera_path, config_.enable_auto_focus);
}

// Control setter methods
bool CameraManager::SetBrightness(int value) {
    return camera_controls_->SetBrightness(state_.current_camera_path, value);
}

bool CameraManager::SetContrast(int value) {
    return camera_controls_->SetContrast(state_.current_camera_path, value);
}

bool CameraManager::SetSaturation(int value) {
    return camera_controls_->SetSaturation(state_.current_camera_path, value);
}

bool CameraManager::SetGain(int value) {
    return camera_controls_->SetGain(state_.current_camera_path, value);
}

bool CameraManager::SetSharpness(int value) {
    return camera_controls_->SetSharpness(state_.current_camera_path, value);
}

bool CameraManager::SetZoom(int value) {
    return camera_controls_->SetZoom(state_.current_camera_path, value);
}

bool CameraManager::SetFocus(int value) {
    return camera_controls_->SetFocus(state_.current_camera_path, value);
}

bool CameraManager::SetAutoGain(bool enabled) {
    return camera_controls_->SetAutoGain(state_.current_camera_path, enabled);
}

bool CameraManager::SetAutoFocus(bool enabled) {
    return camera_controls_->SetAutoFocus(state_.current_camera_path, enabled);
}

bool CameraManager::SetAutoExposure(bool enabled) {
    return camera_controls_->SetAutoExposure(state_.current_camera_path, enabled);
}

bool CameraManager::SetExposure(int value) {
    return camera_controls_->SetExposure(state_.current_camera_path, value);
}

bool CameraManager::SetWhiteBalance(bool auto_enabled) {
    return camera_controls_->SetWhiteBalance(state_.current_camera_path, auto_enabled);
}

bool CameraManager::SetWhiteBalanceTemperature(int value) {
    return camera_controls_->SetWhiteBalanceTemperature(state_.current_camera_path, value);
}

bool CameraManager::SetBacklightCompensation(int value) {
    return camera_controls_->SetBacklightCompensation(state_.current_camera_path, value);
}

bool CameraManager::SetControl(uint32_t control_id, int value) {
    return camera_controls_->SetControl(state_.current_camera_path, control_id, value);
}

std::string CameraManager::GetBackendName() const {
    return state_.backend_name;
}

double CameraManager::GetActualFPS() const {
    return state_.actual_fps;
}

void CameraManager::UpdatePerformanceStats() {
    if (IsOpened()) {
        state_.actual_fps = cap_.get(cv::CAP_PROP_FPS);
    }
}

void CameraManager::Cleanup() {
    if (!state_.is_initialized) return;
    
    std::cout << "🧹 Cleaning up Camera Manager..." << std::endl;
    
    CloseCamera();
    
    // Reset state
    state_ = CameraState{};
    cam_list_.clear();
    ui_fps_opts_.clear();
    
    std::cout << "✅ Camera Manager cleanup completed" << std::endl;
}

// Private helper methods
cv::VideoCapture CameraManager::OpenCapture(int idx, int w, int h) {
    cv::VideoCapture c(idx, cv::CAP_V4L2);
    if (c.isOpened() && w > 0 && h > 0) {
        // Set MJPG format for higher FPS support (YUYV is limited to 10 FPS at higher resolutions)
        c.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
        c.set(cv::CAP_PROP_FRAME_WIDTH, w);
        c.set(cv::CAP_PROP_FRAME_HEIGHT, h);
        // Set twice to ensure it's applied (some cameras need this)
        c.set(cv::CAP_PROP_FRAME_WIDTH, w);
        c.set(cv::CAP_PROP_FRAME_HEIGHT, h);
    }
    return c;
}

void CameraManager::UpdateFPSOptions(const std::string& cam_path, int width, int height) {
    ui_fps_opts_ = camera_enumeration_->UpdateFPSOptions(cam_path, width, height);
    
    if (!ui_fps_opts_.empty()) {
        std::cout << "🎬 Available FPS options: ";
        for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << ui_fps_opts_[i];
        }
        std::cout << std::endl;
    }
}

// ConvertSampleToBgr implementation moved to camera_gstreamer_conversion.cpp

// PipeWire Output Methods (Flatpak Video Streaming)
bool CameraManager::InitializePipeWireOutput(const std::string& stream_name, int width, int height, int fps) {
    if (pipewire_output_) {
        std::cout << "⚠️  PipeWire output already initialized" << std::endl;
        return true;
    }

    pipewire_output_ = std::make_unique<PipeWireOutput>();
    if (!pipewire_output_->Initialize(stream_name, width, height, fps)) {
        std::cout << "❌ Failed to initialize PipeWire output stream" << std::endl;
        pipewire_output_.reset();
        return false;
    }

    std::cout << "✅ PipeWire output stream initialized: " << stream_name 
              << " (" << width << "x" << height << " @ " << fps << " FPS)" << std::endl;
    return true;
}

void CameraManager::ShutdownPipeWireOutput() {
    if (pipewire_output_) {
        pipewire_output_->Shutdown();
        pipewire_output_.reset();
        std::cout << "🛑 PipeWire output stream shut down" << std::endl;
    }
}

bool CameraManager::SendFrameToPipeWire(const cv::Mat& frame) {
    if (!pipewire_output_ || !pipewire_output_->IsActive()) {
        return false;
    }
  //  std::cout << "📺 PipeWire: Sending frame " << frame.cols << "x" << frame.rows << " to stream" << std::endl;
    return pipewire_output_->SendFrame(frame);
}

bool CameraManager::IsPipeWireOutputActive() const {
    return pipewire_output_ && pipewire_output_->IsActive();
}

const std::string& CameraManager::GetPipeWireStreamName() const {
    static const std::string empty_string;
    return pipewire_output_ ? pipewire_output_->GetStreamName() : empty_string;
}

} // namespace segmecam
