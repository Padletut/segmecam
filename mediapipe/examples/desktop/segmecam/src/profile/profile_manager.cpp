#include "include/profile/profile_manager.h"
#include <iostream>
#include <cstring>    
#include <algorithm>
#include <filesystem>

namespace segmecam {

ProfileManager::ProfileManager(AppState& state, CameraManager& camera_mgr, class EffectsManager& effects_mgr)
    : state_(state), camera_mgr_(camera_mgr), effects_mgr_(effects_mgr) {
}

void ProfileManager::RenderProfileSection() {
    RenderProfileHeader();
    HandleProfileSelection();
    ImGui::Separator();
}

void ProfileManager::RenderProfileHeader() {
    ImGui::Spacing();
    ImGui::Text("Profile Management");
    ImGui::Separator();
}

void ProfileManager::HandleProfileSelection() {
    // Use common profile selection UI
    if (ui_utils::RenderProfileSelection(config_mgr_, ui_profile_idx_, profile_name_buf_,
                                       sizeof(profile_name_buf_), "Profile Name##camera_panel", true)) {
        ProcessProfileLoadRequest();
    }
}

void ProfileManager::ProcessProfileLoadRequest() {
    // Profile load was requested
    auto profile_names = config_mgr_->ListProfiles();
    if (ui_profile_idx_ >= 0 && ui_profile_idx_ < (int)profile_names.size()) {
        LoadProfileIntoState(profile_names[ui_profile_idx_]);
        // Update name buffer to match loaded profile
        std::string profile_name = profile_names[ui_profile_idx_];
        segmecam::ui_utils::SafeStringCopy(profile_name_buf_, sizeof(profile_name_buf_), profile_name);
    }
}

void ProfileManager::LoadProfileIntoState(const std::string& profile_name) {
    
    if (!ValidateProfileLoad(profile_name)) {
        std::cerr << "Profile validation failed" << std::endl;
        return;
    }

    ConfigData config;

    std::cerr << "Calling config_mgr_->LoadProfile" << std::endl;
    if (!config_mgr_->LoadProfile(profile_name, config)) {
        std::cerr << "Failed to load profile: " << profile_name << std::endl;
        return;
    }

    LoadCameraSettings(config);  
    LoadDisplaySettings(config);
    LoadBackgroundSettings(config);
    LoadLandmarkSettings(config);
    LoadBeautySettings(config);
    LoadPerformanceSettings(config);

    std::cout << "Profile loaded successfully: " << profile_name << std::endl;
}

bool ProfileManager::ValidateProfileLoad(const std::string& profile_name) {
    if (!config_mgr_) {
        std::cout << "Config manager not available for loading profile" << std::endl;
        return false;
    }

    if (profile_name.empty()) {
        std::cout << "Profile name cannot be empty" << std::endl;
        return false;
    }

    return true;
}

void ProfileManager::LoadCameraSettings(const ConfigData& config) {
    // Apply loaded settings to state

    // Camera settings - apply both actual camera changes and UI state
    const bool in_flatpak = IsFlatpakEnvironment();

    if (config.camera.ui_cam_idx >= 0) {
        LoadCameraSelection(config, in_flatpak);
        LoadResolutionSettings(config, in_flatpak);

        // Update AppState camera dimensions for consistency
        const auto& camera_state = camera_mgr_.GetState();
        state_.camera_width = camera_state.current_width;
        state_.camera_height = camera_state.current_height;
        state_.camera_fps = camera_state.current_fps;
    }
}

void ProfileManager::LoadCameraSelection(const ConfigData& config, bool in_flatpak) {
    if (in_flatpak) {
        // In Flatpak, we can't change cameras but we can store the preference
        std::cout << "Profile loaded in Flatpak: camera settings retained (PipeWire session unchanged)." << std::endl;
    } else if (config.camera.ui_cam_idx != -1) {
        // Apply camera change with UI indices
        camera_mgr_.SetCurrentCamera(config.camera.ui_cam_idx,
                                   config.camera.ui_res_idx >= 0 ? config.camera.ui_res_idx : 0,
                                   config.camera.ui_fps_idx >= 0 ? config.camera.ui_fps_idx : 0);
        std::cout << "Profile loaded: Camera changed to index " << config.camera.ui_cam_idx
                  << " with resolution index " << config.camera.ui_res_idx
                  << " and FPS index " << config.camera.ui_fps_idx << std::endl;
    }
}

void ProfileManager::LoadResolutionSettings(const ConfigData& config, bool in_flatpak) {
    if (!in_flatpak) {
        // Same camera, but possibly different resolution/FPS using actual values
        // Only apply if camera is currently open to avoid conflicts
        if (camera_mgr_.IsOpened() && config.camera.res_w > 0 && config.camera.res_h > 0) {
            camera_mgr_.SetResolution(config.camera.res_w, config.camera.res_h);
            std::cout << "Profile loaded: Resolution changed to "
                      << config.camera.res_w << "x" << config.camera.res_h << std::endl;
        }
        if (camera_mgr_.IsOpened() && config.camera.fps_value > 0) {
            camera_mgr_.SetFPS(config.camera.fps_value);
            std::cout << "Profile loaded: FPS changed to " << config.camera.fps_value << std::endl;
        }
    }
}

void ProfileManager::LoadDisplaySettings(const ConfigData& config) {
    // Display settings
    state_.vsync_on = config.display.vsync_on;
    state_.show_mask = config.display.show_mask;
    state_.show_landmarks = config.display.show_landmarks;
    state_.show_mesh = config.display.show_mesh;
    state_.show_mesh_dense = config.display.show_mesh_dense;
    state_.show_facemask = config.display.show_facemask;
    state_.show_wrinkle_segmentation = config.display.show_wrinkle_segmentation;
    state_.show_wrinkle_inpaint = config.display.show_wrinkle_inpaint;
}

void ProfileManager::LoadBackgroundSettings(const ConfigData& config) {
    // Background settings
    state_.bg_mode = config.background.bg_mode;
    state_.blur_strength = config.background.blur_strength;
    state_.feather_px = config.background.feather_px;
    segmecam::ui_utils::SafeStringCopy(state_.bg_path_buf, sizeof(state_.bg_path_buf), config.background.bg_path);
    state_.solid_color[0] = config.background.solid_color[0];
    state_.solid_color[1] = config.background.solid_color[1];
    state_.solid_color[2] = config.background.solid_color[2];

    // Note: Background image loading is deferred to avoid memory issues during profile loading
    // The UI will load the image when needed (similar to old monolithic version)
}

void ProfileManager::LoadLandmarkSettings(const ConfigData& config) {
    // Landmark settings
    state_.lm_roi_mode = config.landmarks.lm_roi_mode;
    state_.lm_apply_rot = config.landmarks.lm_apply_rot;
    state_.lm_flip_x = config.landmarks.lm_flip_x;
    state_.lm_flip_y = config.landmarks.lm_flip_y;
    state_.lm_swap_xy = config.landmarks.lm_swap_xy;
}

void ProfileManager::LoadBeautySettings(const ConfigData& config) {
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
    state_.fx_skin_smile_wrinkle_gain = config.beauty.fx_skin_smile_wrinkle_gain;
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
}

void ProfileManager::LoadPerformanceSettings(const ConfigData& config) {
    // Performance settings
    state_.use_opencl = config.performance.use_opencl;

    // Debug settings (currently none)
}

bool ProfileManager::SaveStateToProfile(const std::string& profile_name) {
    if (!ValidateProfileSave(profile_name)) {
        return false;
    }

    ConfigData config;
    SaveCameraSettings(config);
    SaveDisplaySettings(config);
    SaveBackgroundSettings(config);
    SaveLandmarkSettings(config);
    SaveBeautySettings(config);
    SavePerformanceSettings(config);

    return SaveProfileToManager(profile_name, config);
}

bool ProfileManager::ValidateProfileSave(const std::string& profile_name) {
    if (!config_mgr_) {
        std::cout << "Config manager not available for saving profile" << std::endl;
        return false;
    }

    if (profile_name.empty()) {
        std::cout << "Profile name cannot be empty" << std::endl;
        return false;
    }

    return true;
}

void ProfileManager::SaveCameraSettings(ConfigData& config) {
    // Camera settings - save both UI indices and actual camera state
    const auto& camera_state = camera_mgr_.GetState();
    config.camera.ui_cam_idx = -1; // Will be set by CameraPanel
    config.camera.ui_res_idx = -1; // Will be set by CameraPanel
    config.camera.ui_fps_idx = -1; // Will be set by CameraPanel
    config.camera.res_w = camera_state.current_width;
    config.camera.res_h = camera_state.current_height;
    config.camera.fps_value = camera_state.current_fps;
}

void ProfileManager::SaveDisplaySettings(ConfigData& config) {
    // Display settings
    config.display.vsync_on = state_.vsync_on;
    config.display.show_mask = state_.show_mask;
    config.display.show_landmarks = state_.show_landmarks;
    config.display.show_mesh = state_.show_mesh;
    config.display.show_mesh_dense = state_.show_mesh_dense;
    config.display.show_facemask = state_.show_facemask;
    config.display.show_wrinkle_segmentation = state_.show_wrinkle_segmentation;
    config.display.show_wrinkle_inpaint = state_.show_wrinkle_inpaint;
}

void ProfileManager::SaveBackgroundSettings(ConfigData& config) {
    // Background settings
    config.background.bg_mode = state_.bg_mode;
    config.background.blur_strength = state_.blur_strength;
    config.background.feather_px = state_.feather_px;
    config.background.bg_path = std::string(state_.bg_path_buf);
    config.background.solid_color[0] = state_.solid_color[0];
    config.background.solid_color[1] = state_.solid_color[1];
    config.background.solid_color[2] = state_.solid_color[2];
}

void ProfileManager::SaveLandmarkSettings(ConfigData& config) {
    // Landmark settings
    config.landmarks.lm_roi_mode = state_.lm_roi_mode;
    config.landmarks.lm_apply_rot = state_.lm_apply_rot;
    config.landmarks.lm_flip_x = state_.lm_flip_x;
    config.landmarks.lm_flip_y = state_.lm_flip_y;
    config.landmarks.lm_swap_xy = state_.lm_swap_xy;
}

void ProfileManager::SaveBeautySettings(ConfigData& config) {
    SaveBasicBeautySettings(config);
    SaveWrinkleSettings(config);
    SaveLipSettings(config);
    SaveTeethSettings(config);
}

void ProfileManager::SaveBasicBeautySettings(ConfigData& config) {
    // Basic beauty settings
    config.beauty.fx_skin = state_.fx_skin;
    config.beauty.fx_skin_adv = state_.fx_skin_adv;
    config.beauty.fx_skin_strength = state_.fx_skin_strength;
    config.beauty.fx_skin_amount = state_.fx_skin_amount;
    config.beauty.fx_skin_radius = state_.fx_skin_radius;
    config.beauty.fx_skin_tex = state_.fx_skin_tex;
    config.beauty.fx_skin_edge = state_.fx_skin_edge;
    config.beauty.fx_adv_scale = state_.fx_adv_scale;
    config.beauty.fx_adv_detail_preserve = state_.fx_adv_detail_preserve;
}

void ProfileManager::SaveWrinkleSettings(ConfigData& config) {
    // Wrinkle settings
    config.beauty.fx_skin_wrinkle = state_.fx_skin_wrinkle;
    config.beauty.fx_skin_smile_boost = state_.fx_skin_smile_boost;
    config.beauty.fx_skin_squint_boost = state_.fx_skin_squint_boost;
    config.beauty.fx_skin_forehead_boost = state_.fx_skin_forehead_boost;
    config.beauty.fx_skin_wrinkle_gain = state_.fx_skin_wrinkle_gain;
    config.beauty.fx_skin_smile_wrinkle_gain = state_.fx_skin_smile_wrinkle_gain;
    config.beauty.fx_wrinkle_suppress_lower = state_.fx_wrinkle_suppress_lower;
    config.beauty.fx_wrinkle_lower_ratio = state_.fx_wrinkle_lower_ratio;
    config.beauty.fx_wrinkle_ignore_glasses = state_.fx_wrinkle_ignore_glasses;
    config.beauty.fx_wrinkle_glasses_margin = state_.fx_wrinkle_glasses_margin;
    config.beauty.fx_wrinkle_keep_ratio = state_.fx_wrinkle_keep_ratio;
    config.beauty.fx_wrinkle_custom_scales = state_.fx_wrinkle_custom_scales;
    config.beauty.fx_wrinkle_min_px = state_.fx_wrinkle_min_px;
    config.beauty.fx_wrinkle_max_px = state_.fx_wrinkle_max_px;
    config.beauty.fx_wrinkle_use_skin_gate = state_.fx_wrinkle_use_skin_gate;
    config.beauty.fx_wrinkle_mask_gain = config.beauty.fx_wrinkle_mask_gain;
    config.beauty.fx_wrinkle_baseline = state_.fx_wrinkle_baseline;
    config.beauty.fx_wrinkle_neg_cap = state_.fx_wrinkle_neg_cap;
    config.beauty.fx_wrinkle_preview = state_.fx_wrinkle_preview;
}

void ProfileManager::SaveLipSettings(ConfigData& config) {
    // Lip effects
    config.beauty.fx_lipstick = state_.fx_lipstick;
    config.beauty.fx_lip_alpha = state_.fx_lip_alpha;
    config.beauty.fx_lip_feather = state_.fx_lip_feather;
    config.beauty.fx_lip_light = state_.fx_lip_light;
    config.beauty.fx_lip_band = state_.fx_lip_band;
    config.beauty.fx_lip_color[0] = state_.fx_lip_color[0];
    config.beauty.fx_lip_color[1] = state_.fx_lip_color[1];
    config.beauty.fx_lip_color[2] = state_.fx_lip_color[2];
}

void ProfileManager::SaveTeethSettings(ConfigData& config) {
    // Teeth whitening
    config.beauty.fx_teeth = state_.fx_teeth;
    config.beauty.fx_teeth_strength = state_.fx_teeth_strength;
    config.beauty.fx_teeth_margin = state_.fx_teeth_margin;
}

void ProfileManager::SavePerformanceSettings(ConfigData& config) {
    // Performance settings
    config.performance.use_opencl = state_.use_opencl;
}

bool ProfileManager::SaveProfileToManager(const std::string& profile_name, const ConfigData& config) {
    // Save using ConfigManager
    bool success = config_mgr_->SaveProfile(profile_name, config);
    if (success) {
        std::cout << "Profile saved successfully: " << profile_name << std::endl;
    } else {
        std::cout << "Failed to save profile: " << profile_name << std::endl;
    }

    return success;
}

void ProfileManager::UpdateDefaultProfileDisplay() {
    if (!config_mgr_) return;

    std::string default_profile;
    if (config_mgr_->GetDefaultProfile(default_profile) && !default_profile.empty()) {
        segmecam::ui_utils::SafeStringCopy(profile_name_buf_, sizeof(profile_name_buf_), default_profile);

        auto profile_names = config_mgr_->ListProfiles();
        auto it = std::find(profile_names.begin(), profile_names.end(), default_profile);
        ui_profile_idx_ = (it == profile_names.end()) ? -1 : (int)std::distance(profile_names.begin(), it);
    }
}

bool ProfileManager::IsFlatpakEnvironment() const {
    return std::getenv("FLATPAK_ID") != nullptr;
}

} // namespace segmecam
