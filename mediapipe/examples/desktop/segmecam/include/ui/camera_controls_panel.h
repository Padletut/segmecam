#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <map>
#include "include/camera/camera_manager.h"
#include "include/ui/ui_panel_base.h"

namespace segmecam {

// Camera controls (V4L2) panel
class CameraControlsPanel : public UIPanel {
public:
    CameraControlsPanel(CameraManager& camera_mgr);
    ~CameraControlsPanel() override = default;

    void Render() override;

private:
    void RenderCameraControls();
    void SliderCtrl(const std::string& label, int32_t ctrl_id, int32_t& value, const CtrlRange& range);
    void CheckboxCtrl(const std::string& label, int32_t ctrl_id, int32_t& value);
    void RenderExposureAutoControls();
    void UpdateControlRanges();
    void RenderGainExposureControls();
    void RenderAdditionalControls();
    void RenderWhiteBalanceControls();
    void RenderResetButton();
    void RenderGainControls();
    void RenderExposureControls();
    void RenderBacklightControl();
    void SliderCtrl(const char* label, CtrlRange& range, uint32_t control_id);
    void CheckboxCtrl(const char* label, CtrlRange& range, uint32_t control_id);
    void CheckboxExposureAuto(const char* label);
    void DisableAutoExposure();
    void EnableAutoExposure(int current_mode);
    
    CameraManager& camera_mgr_;
    
    // Control ranges (cached for performance)
    CtrlRange r_brightness_, r_contrast_, r_saturation_, r_gain_, r_sharpness_;
    CtrlRange r_zoom_, r_focus_, r_autogain_, r_autofocus_, r_autoexposure_;
    CtrlRange r_exposure_abs_, r_awb_, r_wb_temp_, r_backlight_, r_expo_dynfps_;
    
    // Cached control ranges for performance
    std::map<int32_t, CtrlRange> control_ranges_;
    bool ranges_cached_ = false;
};

} // namespace segmecam
