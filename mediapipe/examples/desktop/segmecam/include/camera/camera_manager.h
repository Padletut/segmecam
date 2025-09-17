#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <linux/videodev2.h>
#include "cam_enum.h"

namespace segmecam {

// Configuration for camera system
struct CameraConfig {
    int default_camera_index = 0;
    int default_width = 0;
    int default_height = 0;
    int default_fps = 30;
    bool prefer_v4l2 = true;
    bool enable_auto_focus = true;
    bool enable_auto_gain = true;
    bool enable_auto_exposure = true;
};

// State tracking for camera system
struct CameraState {
    bool is_initialized = false;
    bool is_opened = false;
    std::string current_camera_path;
    int current_width = 0;
    int current_height = 0;
    int current_fps = 0;
    
    // UI state
    int ui_cam_idx = 0;
    int ui_res_idx = 0;
    int ui_fps_idx = 0;
    
    // Camera backend info
    std::string backend_name;
    
    // Performance tracking
    double actual_fps = 0.0;
    int frames_captured = 0;
};

// Camera system manager for initialization, enumeration, capture, and V4L2 controls
class CameraManager {
public:
    CameraManager();
    ~CameraManager();
    
    // Core lifecycle
    int Initialize(const CameraConfig& config);
    void Cleanup();
    
    // Camera operations
    bool OpenCamera(int camera_index);
    bool OpenCamera(int camera_index, int width, int height, int fps = -1);
    void CloseCamera();
    bool IsOpened() const;
    
    // Frame capture
    bool CaptureFrame(cv::Mat& frame);
    
    // Camera enumeration and selection
    const std::vector<CameraDesc>& GetCameraList() const { return cam_list_; }
    void RefreshCameraList();
    bool SetCurrentCamera(int ui_cam_idx, int ui_res_idx, int ui_fps_idx);
    
    // Virtual camera (v4l2loopback) support
    const std::vector<LoopbackDesc>& GetVCamList() const { return vcam_list_; }
    void RefreshVCamList();
    
    // Resolution and FPS management
    const std::vector<std::pair<int,int>>& GetCurrentResolutions() const;
    const std::vector<int>& GetCurrentFPSOptions() const { return ui_fps_opts_; }
    bool SetResolution(int width, int height);
    bool SetFPS(int fps);
    
    // V4L2 camera controls
    void RefreshControls();
    void ApplyDefaultControls();
    
    // Individual control access
    bool SetBrightness(int value);
    bool SetContrast(int value);
    bool SetSaturation(int value);
    bool SetGain(int value);
    bool SetSharpness(int value);
    bool SetZoom(int value);
    bool SetFocus(int value);
    bool SetAutoGain(bool enabled);
    bool SetAutoFocus(bool enabled);
    bool SetAutoExposure(bool enabled);
    bool SetExposure(int value);
    bool SetWhiteBalance(bool auto_enabled);
    bool SetWhiteBalanceTemperature(int value);
    bool SetBacklightCompensation(int value);
    
    // Generic control method for V4L2 controls
    bool SetControl(uint32_t control_id, int value);
    
    // Control ranges (for UI sliders)
    const CtrlRange& GetBrightnessRange() const { return r_brightness_; }
    const CtrlRange& GetContrastRange() const { return r_contrast_; }
    const CtrlRange& GetSaturationRange() const { return r_saturation_; }
    const CtrlRange& GetGainRange() const { return r_gain_; }
    const CtrlRange& GetSharpnessRange() const { return r_sharpness_; }
    const CtrlRange& GetZoomRange() const { return r_zoom_; }
    const CtrlRange& GetFocusRange() const { return r_focus_; }
    const CtrlRange& GetAutoGainRange() const { return r_autogain_; }
    const CtrlRange& GetAutoFocusRange() const { return r_autofocus_; }
    const CtrlRange& GetAutoExposureRange() const { return r_autoexposure_; }
    const CtrlRange& GetExposureRange() const { return r_exposure_abs_; }
    const CtrlRange& GetWhiteBalanceRange() const { return r_awb_; }
    const CtrlRange& GetWhiteBalanceTemperatureRange() const { return r_wb_temp_; }
    const CtrlRange& GetBacklightCompensationRange() const { return r_backlight_; }
    const CtrlRange& GetExposureDynamicFPSRange() const { return r_expo_dynfps_; }
    
    // State access
    const CameraState& GetState() const { return state_; }
    const CameraConfig& GetConfig() const { return config_; }
    
    // UI indices access (for compatibility with existing UI code)
    int GetUICameraIndex() const { return state_.ui_cam_idx; }
    int GetUIResolutionIndex() const { return state_.ui_res_idx; }
    int GetUIFPSIndex() const { return state_.ui_fps_idx; }
    
    // Backend information
    std::string GetBackendName() const;
    double GetActualFPS() const;
    void UpdatePerformanceStats();
    
    // Current camera settings
    int GetCurrentWidth() const { return state_.current_width; }
    int GetCurrentHeight() const { return state_.current_height; }
    int GetCurrentFPS() const { return state_.current_fps; }

private:
    // Configuration and state
    CameraConfig config_;
    CameraState state_;
    
    // Camera enumeration
    std::vector<CameraDesc> cam_list_;
    std::vector<LoopbackDesc> vcam_list_;
    std::vector<int> ui_fps_opts_;
    
    // OpenCV capture
    cv::VideoCapture cap_;
    
    // V4L2 control ranges
    CtrlRange r_brightness_, r_contrast_, r_saturation_, r_gain_;
    CtrlRange r_sharpness_, r_zoom_, r_focus_;
    CtrlRange r_autogain_, r_autofocus_;
    CtrlRange r_autoexposure_, r_exposure_abs_;
    CtrlRange r_awb_, r_wb_temp_, r_backlight_, r_expo_dynfps_;
    
    // Helper methods
    cv::VideoCapture OpenCapture(int idx, int w, int h);
    void QueryCtrl(const std::string& cam_path, uint32_t id, CtrlRange* out);
    bool SetCtrl(const std::string& cam_path, uint32_t id, int32_t value);
    bool GetCtrl(const std::string& cam_path, uint32_t id, int32_t* value);
    void UpdateFPSOptions(const std::string& cam_path, int width, int height);
};

} // namespace segmecam