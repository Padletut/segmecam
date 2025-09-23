#pragma once

#include <string>
#include <cstdint>
#include "include/camera/cam_enum.h"

namespace segmecam {

// Camera controls manager - handles V4L2 camera control operations
class CameraControls {
public:
    CameraControls();
    ~CameraControls() = default;

    // Initialization
    void Initialize(const std::string& cam_path);

    // Control range queries
    void QueryControl(const std::string& cam_path, uint32_t id, CtrlRange* out);
    void RefreshControls(const std::string& cam_path);

    // Control setters
    bool SetBrightness(const std::string& cam_path, int value);
    bool SetContrast(const std::string& cam_path, int value);
    bool SetSaturation(const std::string& cam_path, int value);
    bool SetGain(const std::string& cam_path, int value);
    bool SetSharpness(const std::string& cam_path, int value);
    bool SetZoom(const std::string& cam_path, int value);
    bool SetFocus(const std::string& cam_path, int value);
    bool SetAutoGain(const std::string& cam_path, bool enabled);
    bool SetAutoFocus(const std::string& cam_path, bool enabled);
    bool SetAutoExposure(const std::string& cam_path, bool enabled);
    bool SetExposure(const std::string& cam_path, int value);
    bool SetWhiteBalance(const std::string& cam_path, bool auto_enabled);
    bool SetWhiteBalanceTemperature(const std::string& cam_path, int value);
    bool SetBacklightCompensation(const std::string& cam_path, int value);
    bool SetControl(const std::string& cam_path, uint32_t control_id, int value);

    // Control getters
    bool GetControl(const std::string& cam_path, uint32_t id, int32_t* value);

    // Apply default control settings
    void ApplyDefaultControls(const std::string& cam_path, bool enable_auto_focus);

    // Control range accessors
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
    const CtrlRange& GetExposureAbsRange() const { return r_exposure_abs_; }
    const CtrlRange& GetAutoWhiteBalanceRange() const { return r_awb_; }
    const CtrlRange& GetWhiteBalanceTempRange() const { return r_wb_temp_; }
    const CtrlRange& GetBacklightRange() const { return r_backlight_; }
    const CtrlRange& GetExposureDynamicFPSRange() const { return r_expo_dynfps_; }

private:
    // Cached control ranges
    CtrlRange r_brightness_;
    CtrlRange r_contrast_;
    CtrlRange r_saturation_;
    CtrlRange r_gain_;
    CtrlRange r_sharpness_;
    CtrlRange r_zoom_;
    CtrlRange r_focus_;
    CtrlRange r_autogain_;
    CtrlRange r_autofocus_;
    CtrlRange r_autoexposure_;
    CtrlRange r_exposure_abs_;
    CtrlRange r_awb_;
    CtrlRange r_wb_temp_;
    CtrlRange r_backlight_;
    CtrlRange r_expo_dynfps_;

    // Current camera path for control operations
    std::string current_camera_path_;
};

} // namespace segmecam