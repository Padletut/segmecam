#include "include/camera/camera_manager.h"

#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace segmecam {

CameraManager::CameraManager() {
    // Initialize with default values
}

CameraManager::~CameraManager() {
    Cleanup();
}

int CameraManager::Initialize(const CameraConfig& config) {
    config_ = config;
    state_ = CameraState{}; // Reset state
    
    std::cout << "📷 Initializing Camera Manager..." << std::endl;
    
    // Enumerate available cameras
    RefreshCameraList();
    RefreshVCamList();
    
    if (cam_list_.empty()) {
        std::cout << "⚠️  No cameras found during enumeration" << std::endl;
        return 1;
    }
    
    // Find the requested camera index in the enumerated list
    for (size_t i = 0; i < cam_list_.size(); ++i) {
        if (cam_list_[i].index == config_.default_camera_index) {
            state_.ui_cam_idx = (int)i;
            break;
        }
    }
    
    // Set initial resolution from available cameras
    if (!cam_list_.empty() && !cam_list_[state_.ui_cam_idx].resolutions.empty()) {
        auto resolutions = cam_list_[state_.ui_cam_idx].resolutions;
        
        // Try to find matching resolution or use the largest available
        int best_res_idx = (int)resolutions.size() - 1; // Default to largest
        
        if (config_.default_width > 0 && config_.default_height > 0) {
            for (size_t i = 0; i < resolutions.size(); ++i) {
                if (resolutions[i].first == config_.default_width && 
                    resolutions[i].second == config_.default_height) {
                    best_res_idx = (int)i;
                    break;
                }
            }
        }
        
        state_.ui_res_idx = best_res_idx;
        auto wh = resolutions[best_res_idx];
        state_.current_width = wh.first;
        state_.current_height = wh.second;
    }
    
    // Setup camera path and FPS options
    if (!cam_list_.empty()) {
        state_.current_camera_path = cam_list_[state_.ui_cam_idx].path;
        UpdateFPSOptions(state_.current_camera_path, state_.current_width, state_.current_height);
        
        // Find best FPS option
        if (!ui_fps_opts_.empty()) {
            state_.ui_fps_idx = (int)ui_fps_opts_.size() - 1; // Default to highest
            
            if (config_.default_fps > 0) {
                for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
                    if (ui_fps_opts_[i] == config_.default_fps) {
                        state_.ui_fps_idx = (int)i;
                        break;
                    }
                }
            }
            
            state_.current_fps = ui_fps_opts_[state_.ui_fps_idx];
        }
    }
    
    // Initialize camera controls
    RefreshControls();
    ApplyDefaultControls();
    
    // Open the camera
    if (!OpenCamera(config_.default_camera_index, state_.current_width, state_.current_height, state_.current_fps)) {
        std::cerr << "❌ Failed to open camera " << config_.default_camera_index << std::endl;
        return 2;
    }
    
    state_.is_initialized = true;
    std::cout << "✅ Camera Manager initialized successfully!" << std::endl;
    std::cout << "📷 Using camera: " << state_.current_camera_path << std::endl;
    std::cout << "📐 Resolution: " << state_.current_width << "x" << state_.current_height << std::endl;
    std::cout << "🎬 FPS: " << state_.current_fps << std::endl;
    std::cout << "🔧 Backend: " << GetBackendName() << std::endl;
    
    return 0;
}

bool CameraManager::OpenCamera(int camera_index) {
    return OpenCamera(camera_index, state_.current_width, state_.current_height, state_.current_fps);
}

bool CameraManager::OpenCamera(int camera_index, int width, int height, int fps) {
    CloseCamera();
    
    std::cout << "📷 Opening camera " << camera_index << " with resolution: " << width << "x" << height;
    if (fps > 0) std::cout << " @ " << fps << " FPS";
    std::cout << std::endl;
    
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
        std::cerr << "❌ Unable to open camera " << camera_index << std::endl;
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

void CameraManager::CloseCamera() {
    if (cap_.isOpened()) {
        cap_.release();
        state_.is_opened = false;
        std::cout << "📷 Camera closed" << std::endl;
    }
}

bool CameraManager::IsOpened() const {
    return state_.is_opened && cap_.isOpened();
}

bool CameraManager::CaptureFrame(cv::Mat& frame) {
    if (!IsOpened()) {
        return false;
    }
    
    bool success = cap_.read(frame);
    if (success) {
        state_.frames_captured++;
    }
    
    return success;
}

void CameraManager::RefreshCameraList() {
    std::cout << "🔍 Enumerating cameras..." << std::endl;
    cam_list_ = EnumerateCameras();
    
    std::cout << "📷 Found " << cam_list_.size() << " camera(s):" << std::endl;
    for (const auto& cam : cam_list_) {
        std::cout << "  • " << cam.name << " (" << cam.path << ") - " 
                  << cam.resolutions.size() << " resolutions" << std::endl;
    }
}

void CameraManager::RefreshVCamList() {
    std::cout << "🔍 Enumerating virtual cameras..." << std::endl;
    vcam_list_ = EnumerateLoopbackDevices();
    
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
    if (state_.current_camera_path.empty()) return;
    
    std::cout << "🔧 Refreshing camera controls for " << state_.current_camera_path << std::endl;
    
    QueryCtrl(state_.current_camera_path, V4L2_CID_BRIGHTNESS, &r_brightness_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_CONTRAST, &r_contrast_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_SATURATION, &r_saturation_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_GAIN, &r_gain_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_SHARPNESS, &r_sharpness_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_ZOOM_ABSOLUTE, &r_zoom_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_FOCUS_ABSOLUTE, &r_focus_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_AUTOGAIN, &r_autogain_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_FOCUS_AUTO, &r_autofocus_);
    
    // Exposure controls
    QueryCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_AUTO, &r_autoexposure_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_ABSOLUTE, &r_exposure_abs_);
    
    // White balance controls
    QueryCtrl(state_.current_camera_path, V4L2_CID_AUTO_WHITE_BALANCE, &r_awb_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_WHITE_BALANCE_TEMPERATURE, &r_wb_temp_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_BACKLIGHT_COMPENSATION, &r_backlight_);
    QueryCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_AUTO_PRIORITY, &r_expo_dynfps_);
}

void CameraManager::ApplyDefaultControls() {
    if (!config_.enable_auto_focus || state_.current_camera_path.empty()) return;
    
    // Set auto focus enabled by default if supported
    if (r_autofocus_.available && r_autofocus_.val == 0) {
        if (SetCtrl(state_.current_camera_path, V4L2_CID_FOCUS_AUTO, 1)) {
            r_autofocus_.val = 1;
            std::cout << "🔧 Enabled auto focus by default" << std::endl;
        }
    }
}

// Control setter methods
bool CameraManager::SetBrightness(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_BRIGHTNESS, value);
}

bool CameraManager::SetContrast(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_CONTRAST, value);
}

bool CameraManager::SetSaturation(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_SATURATION, value);
}

bool CameraManager::SetGain(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_GAIN, value);
}

bool CameraManager::SetSharpness(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_SHARPNESS, value);
}

bool CameraManager::SetZoom(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_ZOOM_ABSOLUTE, value);
}

bool CameraManager::SetFocus(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_FOCUS_ABSOLUTE, value);
}

bool CameraManager::SetAutoGain(bool enabled) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_AUTOGAIN, enabled ? 1 : 0);
}

bool CameraManager::SetAutoFocus(bool enabled) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_FOCUS_AUTO, enabled ? 1 : 0);
}

bool CameraManager::SetAutoExposure(bool enabled) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_AUTO, enabled ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL);
}

bool CameraManager::SetExposure(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_EXPOSURE_ABSOLUTE, value);
}

bool CameraManager::SetWhiteBalance(bool auto_enabled) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_AUTO_WHITE_BALANCE, auto_enabled ? 1 : 0);
}

bool CameraManager::SetWhiteBalanceTemperature(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_WHITE_BALANCE_TEMPERATURE, value);
}

bool CameraManager::SetBacklightCompensation(int value) {
    return SetCtrl(state_.current_camera_path, V4L2_CID_BACKLIGHT_COMPENSATION, value);
}

bool CameraManager::SetControl(uint32_t control_id, int value) {
    return SetCtrl(state_.current_camera_path, control_id, value);
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

void CameraManager::QueryCtrl(const std::string& cam_path, uint32_t id, CtrlRange* out) {
    if (!::QueryCtrl(cam_path, id, out)) {
        *out = CtrlRange{}; // Reset to defaults if query fails
    }
}

bool CameraManager::SetCtrl(const std::string& cam_path, uint32_t id, int32_t value) {
    bool success = ::SetCtrl(cam_path, id, value);
    if (success) {
        // Update the cached value in the appropriate range
        // This is a simplified approach - in a full implementation you'd want
        // to identify which control was set and update its cached val
        RefreshControls();
    }
    return success;
}

bool CameraManager::GetCtrl(const std::string& cam_path, uint32_t id, int32_t* value) {
    return ::GetCtrl(cam_path, id, value);
}

void CameraManager::UpdateFPSOptions(const std::string& cam_path, int width, int height) {
    ui_fps_opts_ = EnumerateFPS(cam_path, width, height);
    
    if (!ui_fps_opts_.empty()) {
        std::cout << "🎬 Available FPS options: ";
        for (size_t i = 0; i < ui_fps_opts_.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << ui_fps_opts_[i];
        }
        std::cout << std::endl;
    }
}

} // namespace segmecam