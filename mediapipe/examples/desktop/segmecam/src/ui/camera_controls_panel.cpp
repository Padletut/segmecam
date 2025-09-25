#include "include/ui/camera_controls_panel.h"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <functional>

#ifdef __linux__
#include <linux/videodev2.h>
#endif

namespace segmecam {

// Camera Controls Panel Implementation
CameraControlsPanel::CameraControlsPanel(CameraManager& camera_mgr)
    : UIPanel("Camera Controls"), camera_mgr_(camera_mgr) {
}

void CameraControlsPanel::Render() {
    if (!visible_) return;

    ImGui::Text("Camera Controls (V4L2)");
    ImGui::Separator();

    if (!camera_mgr_.IsOpened()) {
        ImGui::Text("No camera opened");
        return;
    }

    UpdateControlRanges();
    RenderCameraControls();
    RenderGainExposureControls();
    RenderAdditionalControls();
    RenderWhiteBalanceControls();
    RenderResetButton();
}

void CameraControlsPanel::UpdateControlRanges() {
    // Get control ranges from camera manager
    r_brightness_ = const_cast<CtrlRange&>(camera_mgr_.GetBrightnessRange());
    r_contrast_ = const_cast<CtrlRange&>(camera_mgr_.GetContrastRange());
    r_saturation_ = const_cast<CtrlRange&>(camera_mgr_.GetSaturationRange());
    r_gain_ = const_cast<CtrlRange&>(camera_mgr_.GetGainRange());
    r_sharpness_ = const_cast<CtrlRange&>(camera_mgr_.GetSharpnessRange());
    r_zoom_ = const_cast<CtrlRange&>(camera_mgr_.GetZoomRange());
    r_focus_ = const_cast<CtrlRange&>(camera_mgr_.GetFocusRange());
    r_autogain_ = const_cast<CtrlRange&>(camera_mgr_.GetAutoGainRange());
    r_autofocus_ = const_cast<CtrlRange&>(camera_mgr_.GetAutoFocusRange());
    r_autoexposure_ = const_cast<CtrlRange&>(camera_mgr_.GetAutoExposureRange());
    r_exposure_abs_ = const_cast<CtrlRange&>(camera_mgr_.GetExposureRange());
    r_awb_ = const_cast<CtrlRange&>(camera_mgr_.GetWhiteBalanceRange());
    r_wb_temp_ = const_cast<CtrlRange&>(camera_mgr_.GetWhiteBalanceTemperatureRange());
    r_backlight_ = const_cast<CtrlRange&>(camera_mgr_.GetBacklightCompensationRange());
    r_expo_dynfps_ = const_cast<CtrlRange&>(camera_mgr_.GetExposureDynamicFPSRange());
}

void CameraControlsPanel::RenderCameraControls() {
    // Render basic controls
    SliderCtrl("Brightness", r_brightness_, V4L2_CID_BRIGHTNESS);
    SliderCtrl("Contrast", r_contrast_, V4L2_CID_CONTRAST);
    SliderCtrl("Saturation", r_saturation_, V4L2_CID_SATURATION);
}

void CameraControlsPanel::RenderGainExposureControls() {
    // Auto gain control
    if (r_autogain_.available) {
        CheckboxCtrl("Auto gain", r_autogain_, V4L2_CID_AUTOGAIN);
    } else if (r_autoexposure_.available) {
        // Fallback label when AUTOGAIN not provided by driver
        CheckboxExposureAuto("Auto exposure");
    }

    // Hide Gain/Exposure sliders completely when Auto Exposure is ON
    bool ae_on_global = (r_autoexposure_.available && r_autoexposure_.val != V4L2_EXPOSURE_MANUAL);
    if (!ae_on_global) {
        RenderGainControls();
    }

    RenderExposureControls();
    RenderBacklightControl();
}

void CameraControlsPanel::RenderGainControls() {
    // Gain: disable only if explicit AUTOGAIN is enabled
    if (r_autogain_.available && r_autogain_.val) ImGui::BeginDisabled();
    SliderCtrl("Gain", r_gain_, V4L2_CID_GAIN);
    if (r_autogain_.available && r_autogain_.val) ImGui::EndDisabled();
}

void CameraControlsPanel::RenderExposureControls() {
    // Exposure controls and helpers
    if (r_autoexposure_.available) {
        bool ae_on = (r_autoexposure_.val != V4L2_EXPOSURE_MANUAL);
        if (r_exposure_abs_.available && !ae_on) {
            SliderCtrl("Exposure", r_exposure_abs_, V4L2_CID_EXPOSURE_ABSOLUTE);
        }
        if (r_expo_dynfps_.available) {
            CheckboxCtrl("Exposure dynamic framerate", r_expo_dynfps_, V4L2_CID_EXPOSURE_AUTO_PRIORITY);
        }
    }
}

void CameraControlsPanel::RenderBacklightControl() {
    // Backlight compensation
    if (r_backlight_.available) {
        if (r_backlight_.min == 0 && r_backlight_.max == 1 && r_backlight_.step == 1) {
            CheckboxCtrl("Backlight compensation", r_backlight_, V4L2_CID_BACKLIGHT_COMPENSATION);
        } else {
            SliderCtrl("Backlight compensation", r_backlight_, V4L2_CID_BACKLIGHT_COMPENSATION);
        }
    }
}

void CameraControlsPanel::RenderAdditionalControls() {
    // Additional controls
    SliderCtrl("Sharpness", r_sharpness_, V4L2_CID_SHARPNESS);
    SliderCtrl("Zoom", r_zoom_, V4L2_CID_ZOOM_ABSOLUTE);

    // Focus controls (disable manual focus when auto focus is enabled)
    CheckboxCtrl("Auto focus", r_autofocus_, V4L2_CID_FOCUS_AUTO);
    if (r_autofocus_.val) ImGui::BeginDisabled();
    SliderCtrl("Focus", r_focus_, V4L2_CID_FOCUS_ABSOLUTE);
    if (r_autofocus_.val) ImGui::EndDisabled();
}

void CameraControlsPanel::RenderWhiteBalanceControls() {
    // White balance (AWB + temperature)
    if (r_awb_.available) {
        CheckboxCtrl("Auto white balance", r_awb_, V4L2_CID_AUTO_WHITE_BALANCE);
        if (r_wb_temp_.available) {
            if (r_awb_.val) ImGui::BeginDisabled();
            SliderCtrl("White balance (temp)", r_wb_temp_, V4L2_CID_WHITE_BALANCE_TEMPERATURE);
            if (r_awb_.val) ImGui::EndDisabled();
        }
    }
}

void CameraControlsPanel::RenderResetButton() {
    if (ImGui::Button("Reset to Defaults")) {
        camera_mgr_.ApplyDefaultControls();
    }
}

void CameraControlsPanel::SliderCtrl(const char* label, CtrlRange& range, uint32_t control_id) {
    if (!range.available) return;

    int v = range.val;
    int minv = range.min;
    int maxv = range.max;
    int step = std::max(1, range.step);

    if (ImGui::SliderInt(label, &v, minv, maxv)) {
        // round to step
        int rs = minv + ((v - minv) / step) * step;
        range.val = rs;

        // Use lookup table to call appropriate setter
        static const std::unordered_map<uint32_t, std::function<void(CameraManager&, int)>> setters = {
            {V4L2_CID_BRIGHTNESS, [](CameraManager& mgr, int val) { mgr.SetBrightness(val); }},
            {V4L2_CID_CONTRAST, [](CameraManager& mgr, int val) { mgr.SetContrast(val); }},
            {V4L2_CID_SATURATION, [](CameraManager& mgr, int val) { mgr.SetSaturation(val); }},
            {V4L2_CID_GAIN, [](CameraManager& mgr, int val) { mgr.SetGain(val); }},
            {V4L2_CID_SHARPNESS, [](CameraManager& mgr, int val) { mgr.SetSharpness(val); }},
            {V4L2_CID_ZOOM_ABSOLUTE, [](CameraManager& mgr, int val) { mgr.SetZoom(val); }},
            {V4L2_CID_FOCUS_ABSOLUTE, [](CameraManager& mgr, int val) { mgr.SetFocus(val); }},
            {V4L2_CID_EXPOSURE_ABSOLUTE, [](CameraManager& mgr, int val) { mgr.SetExposure(val); }},
            {V4L2_CID_WHITE_BALANCE_TEMPERATURE, [](CameraManager& mgr, int val) { mgr.SetWhiteBalanceTemperature(val); }},
            {V4L2_CID_BACKLIGHT_COMPENSATION, [](CameraManager& mgr, int val) { mgr.SetBacklightCompensation(val); }}
        };

        auto it = setters.find(control_id);
        if (it != setters.end()) {
            it->second(camera_mgr_, range.val);
        } else {
            camera_mgr_.SetControl(control_id, range.val);
        }
    }
}

void CameraControlsPanel::CheckboxCtrl(const char* label, CtrlRange& range, uint32_t control_id) {
    if (!range.available) return;

    bool v = (range.val != 0);
    if (ImGui::Checkbox(label, &v)) {
        range.val = v ? 1 : 0;

        // Use lookup table for boolean controls
        static const std::unordered_map<uint32_t, std::function<void(CameraManager&, bool)>> bool_setters = {
            {V4L2_CID_AUTOGAIN, [](CameraManager& mgr, bool val) { mgr.SetAutoGain(val); }},
            {V4L2_CID_FOCUS_AUTO, [](CameraManager& mgr, bool val) { mgr.SetAutoFocus(val); }},
            {V4L2_CID_AUTO_WHITE_BALANCE, [](CameraManager& mgr, bool val) { mgr.SetWhiteBalance(val); }}
        };

        auto it = bool_setters.find(control_id);
        if (it != bool_setters.end()) {
            it->second(camera_mgr_, v);
        } else if (control_id == V4L2_CID_BACKLIGHT_COMPENSATION &&
                   range.min == 0 && range.max == 1 && range.step == 1) {
            // Special case: backlight can be either checkbox or slider
            camera_mgr_.SetBacklightCompensation(range.val);
        } else {
            camera_mgr_.SetControl(control_id, range.val);
        }
    }
}

void CameraControlsPanel::CheckboxExposureAuto(const char* label) {
    const auto& r_autoexposure = camera_mgr_.GetAutoExposureRange();
    if (!r_autoexposure.available) return;

    // Treat any non-manual as enabled
    int mode = r_autoexposure.val;
    bool enabled = (mode != V4L2_EXPOSURE_MANUAL);

    if (ImGui::Checkbox(label, &enabled)) {
        if (!enabled) {
            DisableAutoExposure();
        } else {
            EnableAutoExposure(mode);
        }
    }
}

void CameraControlsPanel::DisableAutoExposure() {
    const auto& r_autoexposure = camera_mgr_.GetAutoExposureRange();
    auto& range = const_cast<CtrlRange&>(r_autoexposure);

    // Turn OFF -> MANUAL
    if (camera_mgr_.SetControl(V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL)) {
        range.val = V4L2_EXPOSURE_MANUAL;
    } else {
        std::cout << "Failed to set EXPOSURE_AUTO to MANUAL" << std::endl;
    }
}

void CameraControlsPanel::EnableAutoExposure(int current_mode) {
    const auto& r_autoexposure = camera_mgr_.GetAutoExposureRange();
    auto& range = const_cast<CtrlRange&>(r_autoexposure);

    // Turn ON: try supported non-manual modes in order of likelihood of success for UVC cams
    const int candidates[] = {
        (int)V4L2_EXPOSURE_APERTURE_PRIORITY,
        (int)V4L2_EXPOSURE_AUTO,
        (int)V4L2_EXPOSURE_SHUTTER_PRIORITY
    };

    bool ok = false;
    for (int c : candidates) {
        if (c < r_autoexposure.min || c > r_autoexposure.max || c == (int)V4L2_EXPOSURE_MANUAL) continue;
        if (camera_mgr_.SetControl(V4L2_CID_EXPOSURE_AUTO, c)) {
            range.val = c;
            ok = true;
            break;
        }
    }

    if (!ok) {
        // Last resort: try leaving as-is if it was already some auto mode
        if (current_mode != V4L2_EXPOSURE_MANUAL) {
            range.val = current_mode;
        } else {
            std::cout << "Failed to enable auto exposure: no supported mode accepted" << std::endl;
        }
    }
}

} // namespace segmecam