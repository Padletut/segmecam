#include "include/ui/ui_panels.h"
#include "include/camera/camera_manager.h"
#include "include/effects/effects_manager.h"
#include "src/config/config_manager.h"
#include "cam_enum.h"
#include "vcam.h"
#include <iostream>
#include <algorithm>
#include <cstring>

#ifdef __linux__
#include <linux/videodev2.h>
#endif

namespace segmecam {

// Camera Panel Implementation
CameraPanel::CameraPanel(AppState& state, CameraManager& camera_mgr, EffectsManager& effects_mgr)
    : UIPanel("Camera"), state_(state), camera_mgr_(camera_mgr), effects_mgr_(effects_mgr) {
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
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Active (%dx%d)", state_.vcam.Width(), state_.vcam.Height());
        
        if (ImGui::Button("Stop Virtual Camera")) {
            state_.vcam.Close();
            std::cout << "Virtual camera stopped" << std::endl;
        }
    } else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: Inactive");
        
        // Virtual camera device selection dropdown
        if (ImGui::Button("Refresh Devices")) {
            RefreshVirtualCameraDevices();
        }
        ImGui::SameLine();
        
        if (vcam_devices_.empty()) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No v4l2loopback devices found");
        } else {
            if (ImGui::Combo("Device", &ui_vcam_idx_, vcam_items_.data(), static_cast<int>(vcam_items_.size()))) {
                // Update the state with selected device path
                if (ui_vcam_idx_ >= 0 && ui_vcam_idx_ < static_cast<int>(vcam_devices_.size())) {
                    state_.virtual_camera_path = vcam_devices_[ui_vcam_idx_].path;
                }
            }
        }
        
        // Resolution info (display only - matches input camera)
        ImGui::Text("Resolution: %dx%d (matches input camera)", 
                   state_.camera_width > 0 ? state_.camera_width : 640,
                   state_.camera_height > 0 ? state_.camera_height : 480);
        
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
    
    // Get control ranges from camera manager
    auto& r_brightness = const_cast<CtrlRange&>(camera_mgr_.GetBrightnessRange());
    auto& r_contrast = const_cast<CtrlRange&>(camera_mgr_.GetContrastRange());
    auto& r_saturation = const_cast<CtrlRange&>(camera_mgr_.GetSaturationRange());
    auto& r_gain = const_cast<CtrlRange&>(camera_mgr_.GetGainRange());
    auto& r_sharpness = const_cast<CtrlRange&>(camera_mgr_.GetSharpnessRange());
    auto& r_zoom = const_cast<CtrlRange&>(camera_mgr_.GetZoomRange());
    auto& r_focus = const_cast<CtrlRange&>(camera_mgr_.GetFocusRange());
    auto& r_autogain = const_cast<CtrlRange&>(camera_mgr_.GetAutoGainRange());
    auto& r_autofocus = const_cast<CtrlRange&>(camera_mgr_.GetAutoFocusRange());
    auto& r_autoexposure = const_cast<CtrlRange&>(camera_mgr_.GetAutoExposureRange());
    auto& r_exposure_abs = const_cast<CtrlRange&>(camera_mgr_.GetExposureRange());
    auto& r_awb = const_cast<CtrlRange&>(camera_mgr_.GetWhiteBalanceRange());
    auto& r_wb_temp = const_cast<CtrlRange&>(camera_mgr_.GetWhiteBalanceTemperatureRange());
    auto& r_backlight = const_cast<CtrlRange&>(camera_mgr_.GetBacklightCompensationRange());
    auto& r_expo_dynfps = const_cast<CtrlRange&>(camera_mgr_.GetExposureDynamicFPSRange());
    
    // Render basic controls
    SliderCtrl("Brightness", r_brightness, V4L2_CID_BRIGHTNESS);
    SliderCtrl("Contrast", r_contrast, V4L2_CID_CONTRAST);
    SliderCtrl("Saturation", r_saturation, V4L2_CID_SATURATION);
    
    // Auto gain control
    if (r_autogain.available) {
        CheckboxCtrl("Auto gain", r_autogain, V4L2_CID_AUTOGAIN);
    } else if (r_autoexposure.available) {
        // Fallback label when AUTOGAIN not provided by driver
        CheckboxExposureAuto("Auto exposure");
    }
    
    // Hide Gain/Exposure sliders completely when Auto Exposure is ON
    bool ae_on_global = (r_autoexposure.available && r_autoexposure.val != V4L2_EXPOSURE_MANUAL);
    if (!ae_on_global) {
        // Gain: disable only if explicit AUTOGAIN is enabled
        if (r_autogain.available && r_autogain.val) ImGui::BeginDisabled();
        SliderCtrl("Gain", r_gain, V4L2_CID_GAIN);
        if (r_autogain.available && r_autogain.val) ImGui::EndDisabled();
    }
    
    // Exposure controls and helpers
    if (r_autoexposure.available) {
        bool ae_on = (r_autoexposure.val != V4L2_EXPOSURE_MANUAL);
        if (r_exposure_abs.available && !ae_on) {
            SliderCtrl("Exposure", r_exposure_abs, V4L2_CID_EXPOSURE_ABSOLUTE);
        }
        if (r_expo_dynfps.available) {
            CheckboxCtrl("Exposure dynamic framerate", r_expo_dynfps, V4L2_CID_EXPOSURE_AUTO_PRIORITY);
        }
    }
    
    // Backlight compensation
    if (r_backlight.available) {
        if (r_backlight.min == 0 && r_backlight.max == 1 && r_backlight.step == 1) {
            CheckboxCtrl("Backlight compensation", r_backlight, V4L2_CID_BACKLIGHT_COMPENSATION);
        } else {
            SliderCtrl("Backlight compensation", r_backlight, V4L2_CID_BACKLIGHT_COMPENSATION);
        }
    }
    
    // Additional controls
    SliderCtrl("Sharpness", r_sharpness, V4L2_CID_SHARPNESS);
    SliderCtrl("Zoom", r_zoom, V4L2_CID_ZOOM_ABSOLUTE);
    
    // Focus controls (disable manual focus when auto focus is enabled)
    CheckboxCtrl("Auto focus", r_autofocus, V4L2_CID_FOCUS_AUTO);
    if (r_autofocus.val) ImGui::BeginDisabled();
    SliderCtrl("Focus", r_focus, V4L2_CID_FOCUS_ABSOLUTE);
    if (r_autofocus.val) ImGui::EndDisabled();
    
    // White balance (AWB + temperature)
    if (r_awb.available) {
        CheckboxCtrl("Auto white balance", r_awb, V4L2_CID_AUTO_WHITE_BALANCE);
        if (r_wb_temp.available) {
            if (r_awb.val) ImGui::BeginDisabled();
            SliderCtrl("White balance (temp)", r_wb_temp, V4L2_CID_WHITE_BALANCE_TEMPERATURE);
            if (r_awb.val) ImGui::EndDisabled();
        }
    }
    
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
        camera_mgr_.SetControl(control_id, range.val);
    }
}

void CameraPanel::CheckboxCtrl(const char* label, CtrlRange& range, uint32_t control_id) {
    if (!range.available) return;
    
    bool v = (range.val != 0);
    if (ImGui::Checkbox(label, &v)) {
        range.val = v ? 1 : 0;
        camera_mgr_.SetControl(control_id, range.val);
    }
}

void CameraPanel::CheckboxExposureAuto(const char* label) {
    const auto& r_autoexposure = camera_mgr_.GetAutoExposureRange();
    if (!r_autoexposure.available) return;
    
    // Treat any non-manual as enabled
    int mode = r_autoexposure.val;
    bool enabled = (mode != V4L2_EXPOSURE_MANUAL);
    
    if (ImGui::Checkbox(label, &enabled)) {
        auto& range = const_cast<CtrlRange&>(r_autoexposure);
        
        if (!enabled) {
            // Turn OFF -> MANUAL
            if (camera_mgr_.SetControl(V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL)) {
                range.val = V4L2_EXPOSURE_MANUAL;
            } else {
                std::cout << "Failed to set EXPOSURE_AUTO to MANUAL" << std::endl;
            }
        } else {
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
                if (mode != V4L2_EXPOSURE_MANUAL) {
                    range.val = mode;
                } else {
                    std::cout << "Failed to enable auto exposure: no supported mode accepted" << std::endl;
                }
            }
        }
    }
}

void CameraPanel::RenderProfileSection() {
    ImGui::Spacing();
    ImGui::Text("Profile Management");
    ImGui::Separator();
    
    // List existing profiles (if ConfigManager is available)
    if (config_mgr_) {
        auto profile_names = config_mgr_->ListProfiles();
        if (profile_names.empty()) {
            ImGui::TextDisabled("No profiles yet");
        } else {
            std::vector<const char*> items;
            for (const auto& name : profile_names) {
                items.push_back(name.c_str());
            }
            
            // Ensure ui_profile_idx_ is valid
            if (ui_profile_idx_ < 0 || ui_profile_idx_ >= (int)profile_names.size()) {
                ui_profile_idx_ = 0;
            }
            
            ImGui::Combo("Select", &ui_profile_idx_, items.data(), (int)items.size());
            ImGui::SameLine();
            
            if (ImGui::Button("Load##prof") && ui_profile_idx_ >= 0) {
                LoadProfileIntoState(profile_names[ui_profile_idx_]);
                // Update name buffer to match loaded profile
                strncpy(profile_name_buf_, profile_names[ui_profile_idx_].c_str(), sizeof(profile_name_buf_) - 1);
                profile_name_buf_[sizeof(profile_name_buf_) - 1] = '\0';
            }
        }
        
        ImGui::InputText("Profile Name##camera_panel", profile_name_buf_, sizeof(profile_name_buf_));
        
        if (ImGui::Button("Save##prof")) {
            if (strlen(profile_name_buf_) > 0) {
                if (SaveStateToProfile(profile_name_buf_)) {
                    std::cout << "Profile saved: " << profile_name_buf_ << std::endl;
                    // Update profile index after successful save
                    auto profile_names = config_mgr_->ListProfiles();
                    auto it = std::find(profile_names.begin(), profile_names.end(), profile_name_buf_);
                    ui_profile_idx_ = (it == profile_names.end()) ? -1 : (int)std::distance(profile_names.begin(), it);
                }
            }
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Set Default##prof") && strlen(profile_name_buf_) > 0) {
            config_mgr_->SetDefaultProfile(profile_name_buf_);
            std::cout << "Set default profile: " << profile_name_buf_ << std::endl;
        }
    } else {
        ImGui::TextDisabled("Profile system not available");
        ImGui::InputText("Profile Name##camera_panel_disabled", profile_name_buf_, sizeof(profile_name_buf_));
        if (ImGui::Button("Save##prof")) {
            ImGui::TextDisabled("Config manager not initialized");
        }
        ImGui::SameLine();
        if (ImGui::Button("Set Default##prof")) {
            ImGui::TextDisabled("Config manager not initialized");
        }
    }
    
    ImGui::Separator();
}

void CameraPanel::LoadProfileIntoState(const std::string& profile_name) {
    if (!config_mgr_) {
        std::cout << "Config manager not available for loading profile" << std::endl;
        return;
    }
    
    if (profile_name.empty()) {
        std::cout << "Profile name cannot be empty" << std::endl;
        return;
    }
    
    // Load profile using ConfigManager
    ConfigData config;
    bool success = config_mgr_->LoadProfile(profile_name, config);
    if (!success) {
        std::cout << "Failed to load profile: " << profile_name << std::endl;
        return;
    }
    
    // Apply loaded settings to state
    
    // Camera settings - apply both actual camera changes and UI state
    if (config.camera.ui_cam_idx >= 0) {
        // Set camera if different from current
        if (config.camera.ui_cam_idx != ui_cam_idx_) {
            ui_cam_idx_ = config.camera.ui_cam_idx;
            ui_res_idx_ = config.camera.ui_res_idx >= 0 ? config.camera.ui_res_idx : 0;
            ui_fps_idx_ = config.camera.ui_fps_idx >= 0 ? config.camera.ui_fps_idx : 0;
            
            // Apply camera change with UI indices
            camera_mgr_.SetCurrentCamera(ui_cam_idx_, ui_res_idx_, ui_fps_idx_);
            std::cout << "Profile loaded: Camera changed to index " << ui_cam_idx_ 
                      << " with resolution index " << ui_res_idx_ 
                      << " and FPS index " << ui_fps_idx_ << std::endl;
        } else {
            // Same camera, but possibly different resolution/FPS using actual values
            if (config.camera.res_w > 0 && config.camera.res_h > 0) {
                camera_mgr_.SetResolution(config.camera.res_w, config.camera.res_h);
                std::cout << "Profile loaded: Resolution changed to " 
                          << config.camera.res_w << "x" << config.camera.res_h << std::endl;
            }
            if (config.camera.fps_value > 0) {
                camera_mgr_.SetFPS(config.camera.fps_value);
                std::cout << "Profile loaded: FPS changed to " << config.camera.fps_value << std::endl;
            }
            
            // Update UI indices to match
            ui_res_idx_ = config.camera.ui_res_idx >= 0 ? config.camera.ui_res_idx : ui_res_idx_;
            ui_fps_idx_ = config.camera.ui_fps_idx >= 0 ? config.camera.ui_fps_idx : ui_fps_idx_;
        }
        
        // Update AppState camera dimensions for consistency
        const auto& camera_state = camera_mgr_.GetState();
        state_.camera_width = camera_state.current_width;
        state_.camera_height = camera_state.current_height;
        state_.camera_fps = camera_state.current_fps;
    }
    
    // Display settings
    state_.vsync_on = config.display.vsync_on;
    state_.show_mask = config.display.show_mask;
    state_.show_landmarks = config.display.show_landmarks;
    state_.show_mesh = config.display.show_mesh;
    state_.show_mesh_dense = config.display.show_mesh_dense;
    
    // Background settings
    state_.bg_mode = config.background.bg_mode;
    state_.blur_strength = config.background.blur_strength;
    state_.feather_px = config.background.feather_px;
    strncpy(state_.bg_path_buf, config.background.bg_path.c_str(), sizeof(state_.bg_path_buf) - 1);
    state_.bg_path_buf[sizeof(state_.bg_path_buf) - 1] = '\0';
    state_.solid_color[0] = config.background.solid_color[0];
    state_.solid_color[1] = config.background.solid_color[1];
    state_.solid_color[2] = config.background.solid_color[2];
    
    // Load background image if path is provided
    if (!config.background.bg_path.empty() && state_.bg_mode == 2) { // 2 = background image mode
        std::cout << "Loading background image from profile: " << config.background.bg_path << std::endl;
        effects_mgr_.SetBackgroundImageFromPath(config.background.bg_path);
    }
    
    // Landmark settings
    state_.lm_roi_mode = config.landmarks.lm_roi_mode;
    state_.lm_apply_rot = config.landmarks.lm_apply_rot;
    state_.lm_flip_x = config.landmarks.lm_flip_x;
    state_.lm_flip_y = config.landmarks.lm_flip_y;
    state_.lm_swap_xy = config.landmarks.lm_swap_xy;
    
    // Beauty settings - copy all beauty parameters
    state_.fx_skin = config.beauty.fx_skin;
    state_.fx_skin_adv = config.beauty.fx_skin_adv;
    state_.fx_skin_strength = config.beauty.fx_skin_strength;
    state_.fx_skin_amount = config.beauty.fx_skin_amount;
    state_.fx_skin_radius = config.beauty.fx_skin_radius;
    state_.fx_skin_tex = config.beauty.fx_skin_tex;
    state_.fx_skin_edge = config.beauty.fx_skin_edge;
    state_.fx_adv_scale = config.beauty.fx_adv_scale;
    state_.fx_adv_detail_preserve = config.beauty.fx_adv_detail_preserve;
    
    // Wrinkle settings
    state_.fx_skin_wrinkle = config.beauty.fx_skin_wrinkle;
    state_.fx_skin_smile_boost = config.beauty.fx_skin_smile_boost;
    state_.fx_skin_squint_boost = config.beauty.fx_skin_squint_boost;
    state_.fx_skin_forehead_boost = config.beauty.fx_skin_forehead_boost;
    state_.fx_skin_wrinkle_gain = config.beauty.fx_skin_wrinkle_gain;
    state_.fx_wrinkle_suppress_lower = config.beauty.fx_wrinkle_suppress_lower;
    state_.fx_wrinkle_lower_ratio = config.beauty.fx_wrinkle_lower_ratio;
    state_.fx_wrinkle_ignore_glasses = config.beauty.fx_wrinkle_ignore_glasses;
    state_.fx_wrinkle_glasses_margin = config.beauty.fx_wrinkle_glasses_margin;
    state_.fx_wrinkle_keep_ratio = config.beauty.fx_wrinkle_keep_ratio;
    state_.fx_wrinkle_custom_scales = config.beauty.fx_wrinkle_custom_scales;
    state_.fx_wrinkle_min_px = config.beauty.fx_wrinkle_min_px;
    state_.fx_wrinkle_max_px = config.beauty.fx_wrinkle_max_px;
    state_.fx_wrinkle_use_skin_gate = config.beauty.fx_wrinkle_use_skin_gate;
    state_.fx_wrinkle_mask_gain = config.beauty.fx_wrinkle_mask_gain;
    state_.fx_wrinkle_baseline = config.beauty.fx_wrinkle_baseline;
    state_.fx_wrinkle_neg_cap = config.beauty.fx_wrinkle_neg_cap;
    state_.fx_wrinkle_preview = config.beauty.fx_wrinkle_preview;
    
    // Lip effects
    state_.fx_lipstick = config.beauty.fx_lipstick;
    state_.fx_lip_alpha = config.beauty.fx_lip_alpha;
    state_.fx_lip_feather = config.beauty.fx_lip_feather;
    state_.fx_lip_light = config.beauty.fx_lip_light;
    state_.fx_lip_band = config.beauty.fx_lip_band;
    state_.fx_lip_color[0] = config.beauty.fx_lip_color[0];
    state_.fx_lip_color[1] = config.beauty.fx_lip_color[1];
    state_.fx_lip_color[2] = config.beauty.fx_lip_color[2];
    
    // Teeth whitening
    state_.fx_teeth = config.beauty.fx_teeth;
    state_.fx_teeth_strength = config.beauty.fx_teeth_strength;
    state_.fx_teeth_margin = config.beauty.fx_teeth_margin;
    
    // Performance settings
    state_.use_opencl = config.performance.use_opencl;
    
    // Debug settings (currently none)
    
    std::cout << "Profile loaded successfully: " << profile_name << std::endl;
}

bool CameraPanel::SaveStateToProfile(const std::string& profile_name) {
    if (!config_mgr_) {
        std::cout << "Config manager not available for saving profile" << std::endl;
        return false;
    }
    
    if (profile_name.empty()) {
        std::cout << "Profile name cannot be empty" << std::endl;
        return false;
    }
    
    // Create ConfigData from current state
    ConfigData config;
    
    // Camera settings - save both UI indices and actual camera state
    const auto& camera_state = camera_mgr_.GetState();
    config.camera.ui_cam_idx = ui_cam_idx_;
    config.camera.ui_res_idx = ui_res_idx_;
    config.camera.ui_fps_idx = ui_fps_idx_;
    config.camera.res_w = camera_state.current_width;
    config.camera.res_h = camera_state.current_height;
    config.camera.fps_value = camera_state.current_fps;
    
    // Display settings
    config.display.vsync_on = state_.vsync_on;
    config.display.show_mask = state_.show_mask;
    config.display.show_landmarks = state_.show_landmarks;
    config.display.show_mesh = state_.show_mesh;
    config.display.show_mesh_dense = state_.show_mesh_dense;
    
    // Background settings
    config.background.bg_mode = state_.bg_mode;
    config.background.blur_strength = state_.blur_strength;
    config.background.feather_px = state_.feather_px;
    config.background.bg_path = std::string(state_.bg_path_buf);
    config.background.solid_color[0] = state_.solid_color[0];
    config.background.solid_color[1] = state_.solid_color[1];
    config.background.solid_color[2] = state_.solid_color[2];
    
    // Landmark settings
    config.landmarks.lm_roi_mode = state_.lm_roi_mode;
    config.landmarks.lm_apply_rot = state_.lm_apply_rot;
    config.landmarks.lm_flip_x = state_.lm_flip_x;
    config.landmarks.lm_flip_y = state_.lm_flip_y;
    config.landmarks.lm_swap_xy = state_.lm_swap_xy;
    
    // Beauty settings - copy all beauty parameters
    config.beauty.fx_skin = state_.fx_skin;
    config.beauty.fx_skin_adv = state_.fx_skin_adv;
    config.beauty.fx_skin_strength = state_.fx_skin_strength;
    config.beauty.fx_skin_amount = state_.fx_skin_amount;
    config.beauty.fx_skin_radius = state_.fx_skin_radius;
    config.beauty.fx_skin_tex = state_.fx_skin_tex;
    config.beauty.fx_skin_edge = state_.fx_skin_edge;
    config.beauty.fx_adv_scale = state_.fx_adv_scale;
    config.beauty.fx_adv_detail_preserve = state_.fx_adv_detail_preserve;
    
    // Wrinkle settings
    config.beauty.fx_skin_wrinkle = state_.fx_skin_wrinkle;
    config.beauty.fx_skin_smile_boost = state_.fx_skin_smile_boost;
    config.beauty.fx_skin_squint_boost = state_.fx_skin_squint_boost;
    config.beauty.fx_skin_forehead_boost = state_.fx_skin_forehead_boost;
    config.beauty.fx_skin_wrinkle_gain = state_.fx_skin_wrinkle_gain;
    config.beauty.fx_wrinkle_suppress_lower = state_.fx_wrinkle_suppress_lower;
    config.beauty.fx_wrinkle_lower_ratio = state_.fx_wrinkle_lower_ratio;
    config.beauty.fx_wrinkle_ignore_glasses = state_.fx_wrinkle_ignore_glasses;
    config.beauty.fx_wrinkle_glasses_margin = state_.fx_wrinkle_glasses_margin;
    config.beauty.fx_wrinkle_keep_ratio = state_.fx_wrinkle_keep_ratio;
    config.beauty.fx_wrinkle_custom_scales = state_.fx_wrinkle_custom_scales;
    config.beauty.fx_wrinkle_min_px = state_.fx_wrinkle_min_px;
    config.beauty.fx_wrinkle_max_px = state_.fx_wrinkle_max_px;
    config.beauty.fx_wrinkle_use_skin_gate = state_.fx_wrinkle_use_skin_gate;
    config.beauty.fx_wrinkle_mask_gain = state_.fx_wrinkle_mask_gain;
    config.beauty.fx_wrinkle_baseline = state_.fx_wrinkle_baseline;
    config.beauty.fx_wrinkle_neg_cap = state_.fx_wrinkle_neg_cap;
    config.beauty.fx_wrinkle_preview = state_.fx_wrinkle_preview;
    
    // Lip effects
    config.beauty.fx_lipstick = state_.fx_lipstick;
    config.beauty.fx_lip_alpha = state_.fx_lip_alpha;
    config.beauty.fx_lip_feather = state_.fx_lip_feather;
    config.beauty.fx_lip_light = state_.fx_lip_light;
    config.beauty.fx_lip_band = state_.fx_lip_band;
    config.beauty.fx_lip_color[0] = state_.fx_lip_color[0];
    config.beauty.fx_lip_color[1] = state_.fx_lip_color[1];
    config.beauty.fx_lip_color[2] = state_.fx_lip_color[2];
    
    // Teeth whitening
    config.beauty.fx_teeth = state_.fx_teeth;
    config.beauty.fx_teeth_strength = state_.fx_teeth_strength;
    config.beauty.fx_teeth_margin = state_.fx_teeth_margin;
    
    // Performance settings
    config.performance.use_opencl = state_.use_opencl;
    
    // Debug settings
    // Debug settings (currently none)
    
    // Save using ConfigManager
    bool success = config_mgr_->SaveProfile(profile_name, config);
    if (success) {
        std::cout << "Profile saved successfully: " << profile_name << std::endl;
    } else {
        std::cout << "Failed to save profile: " << profile_name << std::endl;
    }
    
    return success;
}

void CameraPanel::UpdateDefaultProfileDisplay() {
    if (!config_mgr_) return;
    
    // Get the default profile name
    std::string default_profile;
    if (config_mgr_->GetDefaultProfile(default_profile) && !default_profile.empty()) {
        // Update the profile name buffer to show the default profile
        strncpy(profile_name_buf_, default_profile.c_str(), sizeof(profile_name_buf_) - 1);
        profile_name_buf_[sizeof(profile_name_buf_) - 1] = '\0';
        
        // Update the dropdown index to match the default profile
        auto profile_names = config_mgr_->ListProfiles();
        auto it = std::find(profile_names.begin(), profile_names.end(), default_profile);
        ui_profile_idx_ = (it == profile_names.end()) ? -1 : (int)std::distance(profile_names.begin(), it);
        
        std::cout << "UI updated to show loaded default profile: " << default_profile << std::endl;
    }
}

} // namespace segmecam