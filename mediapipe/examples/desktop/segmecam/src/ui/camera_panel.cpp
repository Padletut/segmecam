#include "include/ui/ui_panels.h"
#include "include/ui/ui_utils.h"
#include "include/camera/camera_manager.h"
#include "include/effects/effects_manager.h"
#include "src/config/config_manager.h"
#include "include/camera/cam_enum.h"
#include "include/camera/vcam.h"
#include "include/profile/profile_manager.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include <functional>
#include "include/effects/effects_manager.h"
#include "src/config/config_manager.h"
#include "include/camera/cam_enum.h"
#include "include/camera/vcam.h"

#ifdef __linux__
#include <linux/videodev2.h>
#endif

namespace segmecam {

namespace {
bool IsFlatpakEnvironment() {
    if (std::getenv("FLATPAK_ID")) {
        return true;
    }
    return std::filesystem::exists("/.flatpak-info");
}

} // namespace

// Camera Panel Implementation
CameraPanel::CameraPanel(AppState& state, CameraManager& camera_mgr, EffectsManager& effects_mgr)
    : UIPanel("Camera"), state_(state), camera_mgr_(camera_mgr), effects_mgr_(effects_mgr), 
      profile_mgr_(state, camera_mgr, effects_mgr) {
    SyncWithCameraState();
    RefreshVirtualCameraDevices();
}

void CameraPanel::SyncWithCameraState() {
    // Sync UI indices with current camera settings from CameraManager
    ui_cam_idx_ = camera_mgr_.GetUICameraIndex();
    ui_res_idx_ = camera_mgr_.GetUIResolutionIndex();
    ui_fps_idx_ = camera_mgr_.GetUIFPSIndex();
}

void CameraPanel::RefreshVirtualCameraDevices() {
    vcam_devices_.clear();
    vcam_labels_.clear();
    vcam_items_.clear();
    
    // Get available loopback devices
    auto devices = EnumerateLoopbackDevices();
    
    for (const auto& device : devices) {
        vcam_devices_.push_back(device);
        vcam_labels_.push_back(device.name + " (" + device.path + ")");
        vcam_items_.push_back(vcam_labels_.back().c_str());
    }
    
    // If we have devices, try to match the current virtual camera path
    if (!vcam_devices_.empty()) {
        ui_vcam_idx_ = 0; // Default to first device
        for (size_t i = 0; i < vcam_devices_.size(); ++i) {
            if (vcam_devices_[i].path == state_.virtual_camera_path) {
                ui_vcam_idx_ = static_cast<int>(i);
                break;
            }
        }
    } else {
        ui_vcam_idx_ = -1;
    }
}

void CameraPanel::Render() {
    if (!visible_) return;
    
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderCameraSelection();
        RenderResolutionSettings();
        RenderVirtualCameraControls();
        RenderProfileSection();
        RenderCameraControls();
    }
}

void CameraPanel::RenderCameraSelection() {
    ImGui::Text("Camera Selection");
    ImGui::Separator();
    
    // Get camera list from manager
    const auto& cam_list = camera_mgr_.GetCameraList();
    
    // Create display list for combo
    std::vector<const char*> items;
    for (const auto& cam : cam_list) {
        items.push_back(cam.name.c_str());
    }
    
    if (!items.empty()) {
        int current_cam = camera_mgr_.GetUICameraIndex();
        if (ImGui::Combo("Camera", &ui_cam_idx_, items.data(), (int)items.size())) {
            if (ui_cam_idx_ != current_cam) {
                // Use placeholder resolution and fps for now
                camera_mgr_.SetCurrentCamera(ui_cam_idx_, 0, 0);
                std::cout << "Camera changed to: " << items[ui_cam_idx_] << std::endl;
            }
        }
    } else {
        ImGui::Text("No cameras detected");
        if (ImGui::Button("Refresh")) {
            camera_mgr_.RefreshCameraList();
        }
    }
}

void CameraPanel::RenderResolutionSettings() {
    ImGui::Text("Resolution & FPS");
    ImGui::Separator();
    
    RenderResolutionControls();
    RenderFPSControls();
}

void CameraPanel::RenderResolutionControls() {
    // Get available resolutions from camera manager
    const auto& res_list = camera_mgr_.GetCurrentResolutions();
    
    // Create display list for resolutions using member variables
    res_strings_.clear();
    res_items_.clear();
    for (const auto& res : res_list) {
        res_strings_.push_back(std::to_string(res.first) + "x" + std::to_string(res.second));
        res_items_.push_back(res_strings_.back().c_str());
    }
    
    if (!res_items_.empty()) {
        if (ImGui::Combo("Resolution", &ui_res_idx_, res_items_.data(), (int)res_items_.size())) {
            if (ui_res_idx_ >= 0 && ui_res_idx_ < (int)res_list.size()) {
                const auto& selected_res = res_list[ui_res_idx_];
                camera_mgr_.SetResolution(selected_res.first, selected_res.second);
                std::cout << "Resolution changed to: " << selected_res.first << "x" << selected_res.second << std::endl;
            }
        }
    }
}

void CameraPanel::RenderFPSControls() {
    // FPS options using member variables
    const auto& fps_list = camera_mgr_.GetCurrentFPSOptions();
    fps_strings_.clear();
    fps_items_.clear();
    for (int fps : fps_list) {
        fps_strings_.push_back(std::to_string(fps) + " FPS");
        fps_items_.push_back(fps_strings_.back().c_str());
    }
    
    if (!fps_items_.empty()) {
        if (ImGui::Combo("FPS", &ui_fps_idx_, fps_items_.data(), (int)fps_items_.size())) {
            if (ui_fps_idx_ >= 0 && ui_fps_idx_ < (int)fps_list.size()) {
                camera_mgr_.SetFPS(fps_list[ui_fps_idx_]);
                std::cout << "FPS changed to: " << fps_list[ui_fps_idx_] << std::endl;
            }
        }
    }
}

void CameraPanel::RenderVirtualCameraControls() {
    ImGui::Text("Virtual Camera Output");
    ImGui::Separator();
    
    // Check if virtual camera is active
    bool vcam_active = state_.vcam.IsOpen();
    
    if (vcam_active) {
        RenderVirtualCameraActive();
    } else {
        RenderVirtualCameraInactive();
    }
    
    RenderVirtualCameraHelp();
}

void CameraPanel::RenderVirtualCameraActive() {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Active (%dx%d)", state_.vcam.Width(), state_.vcam.Height());
    
    if (ImGui::Button("Stop Virtual Camera")) {
        state_.vcam.Close();
        std::cout << "Virtual camera stopped" << std::endl;
    }
}

void CameraPanel::RenderVirtualCameraInactive() {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: Inactive");
    
    // Virtual camera device selection dropdown
    if (ImGui::Button("Refresh Devices")) {
        RefreshVirtualCameraDevices();
    }
    ImGui::SameLine();
    
    if (vcam_devices_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No v4l2loopback devices found");
    } else {
        RenderVirtualCameraDeviceSelection();
        RenderVirtualCameraResolutionInfo();
        RenderVirtualCameraStartButton();
    }
}

void CameraPanel::RenderVirtualCameraDeviceSelection() {
    if (ImGui::Combo("Device", &ui_vcam_idx_, vcam_items_.data(), static_cast<int>(vcam_items_.size()))) {
        // Update the state with selected device path
        if (ui_vcam_idx_ >= 0 && ui_vcam_idx_ < static_cast<int>(vcam_devices_.size())) {
            state_.virtual_camera_path = vcam_devices_[ui_vcam_idx_].path;
        }
    }
}

void CameraPanel::RenderVirtualCameraResolutionInfo() {
    // Resolution info (display only - matches input camera)
    ImGui::Text("Resolution: %dx%d (matches input camera)", 
               state_.camera_width > 0 ? state_.camera_width : 640,
               state_.camera_height > 0 ? state_.camera_height : 480);
}

void CameraPanel::RenderVirtualCameraStartButton() {
    if (ImGui::Button("Start Virtual Camera")) {
        if (!vcam_devices_.empty() && ui_vcam_idx_ >= 0 && ui_vcam_idx_ < static_cast<int>(vcam_devices_.size())) {
            const std::string& device_path = vcam_devices_[ui_vcam_idx_].path;
            // Use input camera resolution, fallback to 640x480 if not available
            int vcam_width = state_.camera_width > 0 ? state_.camera_width : 640;
            int vcam_height = state_.camera_height > 0 ? state_.camera_height : 480;
            
            if (state_.vcam.Open(device_path.c_str(), vcam_width, vcam_height)) {
                std::cout << "Virtual camera started on " << device_path 
                         << " at " << vcam_width << "x" << vcam_height << std::endl;
            } else {
                std::cout << "Failed to start virtual camera on " << device_path << std::endl;
            }
        } else {
            std::cout << "No virtual camera device selected" << std::endl;
        }
    }
}

void CameraPanel::RenderVirtualCameraHelp() {
    // Virtual camera help
    ImGui::Separator();
    ImGui::TextDisabled("Usage:");
    ImGui::TextDisabled("• Install v4l2loopback kernel module");
    ImGui::TextDisabled("• Use in video calls (Zoom, Teams, etc.)");
    ImGui::TextDisabled("• Refresh to detect new devices");
    
    // Clear separation before Profile section
    ImGui::Spacing();
    ImGui::Separator();
}

void CameraPanel::RenderCameraControls() {
    ImGui::Text("Camera Controls (V4L2)");
    ImGui::Separator();
    
    if (!camera_mgr_.IsOpened()) {
        ImGui::Text("No camera opened");
        return;
    }
    
    GetControlRanges();
    RenderBasicControls();
    RenderGainExposureControls();
    RenderAdditionalControls();
    RenderWhiteBalanceControls();
    RenderResetButton();
    
    // PipeWire Output Controls
    ImGui::Spacing();
    ImGui::Separator();
    RenderPipeWireOutputSection();
}

void CameraPanel::RenderPipeWireOutputSection() {
    ImGui::Text("PipeWire Output");
    ImGui::Separator();
    
    if (camera_mgr_.IsPipeWireOutputActive()) {
        RenderPipeWireOutputActive();
    } else {
        RenderPipeWireOutputInactive();
    }
    
    RenderPipeWireOutputHelp();
}

void CameraPanel::RenderPipeWireOutputActive() {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Active");
    
    // Show stream name
    const std::string& stream_name = camera_mgr_.GetPipeWireStreamName();
    ImGui::Text("Stream: %s", stream_name.c_str());
    
    // Show actual resolution info from camera manager
    const auto& camera_state = camera_mgr_.GetState();
    int actual_width = camera_state.current_width > 0 ? camera_state.current_width : 640;
    int actual_height = camera_state.current_height > 0 ? camera_state.current_height : 480;
    int actual_fps = camera_state.current_fps > 0 ? static_cast<int>(camera_state.current_fps) : 30;
    ImGui::Text("Resolution: %dx%d @ %d FPS", actual_width, actual_height, actual_fps);
    
    if (ImGui::Button("Stop PipeWire Output")) {
        camera_mgr_.ShutdownPipeWireOutput();
        std::cout << "PipeWire output stopped" << std::endl;
    }
}

void CameraPanel::RenderPipeWireOutputInactive() {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: Inactive");
    
    ImGui::Text("Stream: SegmeCam Virtual Camera");
    // Show actual camera resolution that will be used
    int display_width = state_.camera_width > 0 ? state_.camera_width : 640;
    int display_height = state_.camera_height > 0 ? state_.camera_height : 480;
    int display_fps = state_.camera_fps > 0 ? static_cast<int>(state_.camera_fps) : 30;
    ImGui::Text("Resolution: %dx%d @ %d FPS", display_width, display_height, display_fps);
    
    if (ImGui::Button("Start PipeWire Output")) {
        // Defer PipeWire initialization to avoid UI thread issues
        // Use the actual camera input resolution, not hardcoded values
        int pipewire_width = state_.camera_width > 0 ? state_.camera_width : 640;
        int pipewire_height = state_.camera_height > 0 ? state_.camera_height : 480;
        int pipewire_fps = state_.camera_fps > 0 ? static_cast<int>(state_.camera_fps) : 30;
        
        pipewire_start_requested_ = true;
        pipewire_width_ = pipewire_width;
        pipewire_height_ = pipewire_height;
        pipewire_fps_ = pipewire_fps;
        std::cout << "PipeWire output start requested - will initialize on next frame" << std::endl;
        std::cout << "Requested resolution: " << pipewire_width << "x" << pipewire_height << " @ " << pipewire_fps << " FPS" << std::endl;
    }
    
    ImGui::TextDisabled("Note: Only available in Flatpak environment");
}

void CameraPanel::RenderPipeWireOutputHelp() {
    ImGui::Separator();
    ImGui::TextDisabled("PipeWire Output:");
    ImGui::TextDisabled("• Portal-compliant video streaming");
    ImGui::TextDisabled("• Works with OBS, Discord, browser apps");
    ImGui::TextDisabled("• No --device=all permission required");
    ImGui::TextDisabled("• Manual control for testing and troubleshooting");
}

void CameraPanel::ProcessDeferredPipeWireInitialization() {
    if (pipewire_start_requested_) {
        pipewire_start_requested_ = false; // Reset flag
        
        std::cout << "Processing deferred PipeWire initialization..." << std::endl;
        if (camera_mgr_.InitializePipeWireOutput("SegmeCam Virtual Camera", pipewire_width_, pipewire_height_, pipewire_fps_)) {
            std::cout << "PipeWire output started successfully" << std::endl;
        } else {
            std::cout << "Failed to start PipeWire output" << std::endl;
        }
    }
}

void CameraPanel::GetControlRanges() {
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

void CameraPanel::RenderBasicControls() {
    // Render basic controls
    SliderCtrl("Brightness", r_brightness_, V4L2_CID_BRIGHTNESS);
    SliderCtrl("Contrast", r_contrast_, V4L2_CID_CONTRAST);
    SliderCtrl("Saturation", r_saturation_, V4L2_CID_SATURATION);
}

void CameraPanel::RenderGainExposureControls() {
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

void CameraPanel::RenderGainControls() {
    // Gain: disable only if explicit AUTOGAIN is enabled
    if (r_autogain_.available && r_autogain_.val) ImGui::BeginDisabled();
    SliderCtrl("Gain", r_gain_, V4L2_CID_GAIN);
    if (r_autogain_.available && r_autogain_.val) ImGui::EndDisabled();
}

void CameraPanel::RenderExposureControls() {
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

void CameraPanel::RenderBacklightControl() {
    // Backlight compensation
    if (r_backlight_.available) {
        if (r_backlight_.min == 0 && r_backlight_.max == 1 && r_backlight_.step == 1) {
            CheckboxCtrl("Backlight compensation", r_backlight_, V4L2_CID_BACKLIGHT_COMPENSATION);
        } else {
            SliderCtrl("Backlight compensation", r_backlight_, V4L2_CID_BACKLIGHT_COMPENSATION);
        }
    }
}

void CameraPanel::RenderAdditionalControls() {
    // Additional controls
    SliderCtrl("Sharpness", r_sharpness_, V4L2_CID_SHARPNESS);
    SliderCtrl("Zoom", r_zoom_, V4L2_CID_ZOOM_ABSOLUTE);
    
    // Focus controls (disable manual focus when auto focus is enabled)
    CheckboxCtrl("Auto focus", r_autofocus_, V4L2_CID_FOCUS_AUTO);
    if (r_autofocus_.val) ImGui::BeginDisabled();
    SliderCtrl("Focus", r_focus_, V4L2_CID_FOCUS_ABSOLUTE);
    if (r_autofocus_.val) ImGui::EndDisabled();
}

void CameraPanel::RenderWhiteBalanceControls() {
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

void CameraPanel::RenderResetButton() {
    if (ImGui::Button("Reset to Defaults")) {
        camera_mgr_.ApplyDefaultControls();
    }
}

void CameraPanel::SliderCtrl(const char* label, CtrlRange& range, uint32_t control_id) {
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

void CameraPanel::CheckboxCtrl(const char* label, CtrlRange& range, uint32_t control_id) {
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

void CameraPanel::CheckboxExposureAuto(const char* label) {
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

void CameraPanel::DisableAutoExposure() {
    const auto& r_autoexposure = camera_mgr_.GetAutoExposureRange();
    auto& range = const_cast<CtrlRange&>(r_autoexposure);
    
    // Turn OFF -> MANUAL
    if (camera_mgr_.SetControl(V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL)) {
        range.val = V4L2_EXPOSURE_MANUAL;
    } else {
        std::cout << "Failed to set EXPOSURE_AUTO to MANUAL" << std::endl;
    }
}

void CameraPanel::EnableAutoExposure(int current_mode) {
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

void CameraPanel::RenderProfileSection() {
    profile_mgr_.RenderProfileSection();
}

void CameraPanel::UpdateDefaultProfileDisplay() {
    profile_mgr_.UpdateDefaultProfileDisplay();
}

} // namespace segmecam
