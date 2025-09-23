#include "include/camera/camera_controls.h"

#include <iostream>

#ifdef __linux__
#include <linux/videodev2.h>
#endif

// Forward declarations for V4L2 control functions
extern bool QueryCtrl(const std::string& cam_path, uint32_t id, CtrlRange* out);
extern bool SetCtrl(const std::string& cam_path, uint32_t id, int32_t value);
extern bool GetCtrl(const std::string& cam_path, uint32_t id, int32_t* value);

namespace segmecam {

CameraControls::CameraControls() = default;

void CameraControls::Initialize(const std::string& cam_path) {
    // Camera controls initialization - currently no-op
    std::cout << "🎛️  Camera controls initialized for " << cam_path << std::endl;
}

void CameraControls::QueryControl(const std::string& cam_path, uint32_t id, CtrlRange* out) {
    if (!::QueryCtrl(cam_path, id, out)) {
        *out = CtrlRange{}; // Reset to defaults if query fails
    }
}

void CameraControls::RefreshControls(const std::string& cam_path) {
    current_camera_path_ = cam_path;

    std::cout << "🔧 Refreshing camera controls for " << cam_path << std::endl;

    QueryControl(cam_path, V4L2_CID_BRIGHTNESS, &r_brightness_);
    QueryControl(cam_path, V4L2_CID_CONTRAST, &r_contrast_);
    QueryControl(cam_path, V4L2_CID_SATURATION, &r_saturation_);
    QueryControl(cam_path, V4L2_CID_GAIN, &r_gain_);
    QueryControl(cam_path, V4L2_CID_SHARPNESS, &r_sharpness_);
    QueryControl(cam_path, V4L2_CID_ZOOM_ABSOLUTE, &r_zoom_);
    QueryControl(cam_path, V4L2_CID_FOCUS_ABSOLUTE, &r_focus_);
    QueryControl(cam_path, V4L2_CID_AUTOGAIN, &r_autogain_);
    QueryControl(cam_path, V4L2_CID_FOCUS_AUTO, &r_autofocus_);

    // Exposure controls
    QueryControl(cam_path, V4L2_CID_EXPOSURE_AUTO, &r_autoexposure_);
    QueryControl(cam_path, V4L2_CID_EXPOSURE_ABSOLUTE, &r_exposure_abs_);

    // White balance controls
    QueryControl(cam_path, V4L2_CID_AUTO_WHITE_BALANCE, &r_awb_);
    QueryControl(cam_path, V4L2_CID_WHITE_BALANCE_TEMPERATURE, &r_wb_temp_);
    QueryControl(cam_path, V4L2_CID_BACKLIGHT_COMPENSATION, &r_backlight_);
    QueryControl(cam_path, V4L2_CID_EXPOSURE_AUTO_PRIORITY, &r_expo_dynfps_);
}

void CameraControls::ApplyDefaultControls(const std::string& cam_path, bool enable_auto_focus) {
    if (!enable_auto_focus || cam_path.empty()) return;

    // Set auto focus enabled by default if supported
    if (r_autofocus_.available && r_autofocus_.val == 0) {
        if (SetCtrl(cam_path, V4L2_CID_FOCUS_AUTO, 1)) {
            r_autofocus_.val = 1;
            std::cout << "🔧 Enabled auto focus by default" << std::endl;
        }
    }
}

// Control setter methods
bool CameraControls::SetBrightness(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_BRIGHTNESS, value);
}

bool CameraControls::SetContrast(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_CONTRAST, value);
}

bool CameraControls::SetSaturation(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_SATURATION, value);
}

bool CameraControls::SetGain(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_GAIN, value);
}

bool CameraControls::SetSharpness(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_SHARPNESS, value);
}

bool CameraControls::SetZoom(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_ZOOM_ABSOLUTE, value);
}

bool CameraControls::SetFocus(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_FOCUS_ABSOLUTE, value);
}

bool CameraControls::SetAutoGain(const std::string& cam_path, bool enabled) {
    return SetCtrl(cam_path, V4L2_CID_AUTOGAIN, enabled ? 1 : 0);
}

bool CameraControls::SetAutoFocus(const std::string& cam_path, bool enabled) {
    return SetCtrl(cam_path, V4L2_CID_FOCUS_AUTO, enabled ? 1 : 0);
}

bool CameraControls::SetAutoExposure(const std::string& cam_path, bool enabled) {
    return SetCtrl(cam_path, V4L2_CID_EXPOSURE_AUTO, enabled ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL);
}

bool CameraControls::SetExposure(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_EXPOSURE_ABSOLUTE, value);
}

bool CameraControls::SetWhiteBalance(const std::string& cam_path, bool auto_enabled) {
    return SetCtrl(cam_path, V4L2_CID_AUTO_WHITE_BALANCE, auto_enabled ? 1 : 0);
}

bool CameraControls::SetWhiteBalanceTemperature(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_WHITE_BALANCE_TEMPERATURE, value);
}

bool CameraControls::SetBacklightCompensation(const std::string& cam_path, int value) {
    return SetCtrl(cam_path, V4L2_CID_BACKLIGHT_COMPENSATION, value);
}

bool CameraControls::SetControl(const std::string& cam_path, uint32_t control_id, int value) {
    return SetCtrl(cam_path, control_id, value);
}

bool CameraControls::GetControl(const std::string& cam_path, uint32_t id, int32_t* value) {
    return ::GetCtrl(cam_path, id, value);
}

} // namespace segmecam