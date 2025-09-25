#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include "include/application/app_state.h"
#include "include/camera/camera_manager.h"
#include "include/camera/cam_enum.h"
#include "src/config/config_manager.h"
#include "include/profile/profile_manager.h"
#include "include/ui/ui_panel_base.h"

namespace segmecam {

// Forward declarations
class EffectsManager;
class ProfileManager;

} // namespace segmecam

#include "include/ui/virtual_camera_panel.h"
#include "include/ui/pipewire_output_panel.h"
#include "include/ui/camera_controls_panel.h"

namespace segmecam {

// Camera selection and controls panel
class CameraPanel : public UIPanel {
public:
    CameraPanel(AppState& state, CameraManager& camera_mgr, class EffectsManager& effects_mgr);
    ~CameraPanel() override = default;
    
    void Render() override;
    
    // Set config manager for profile functionality
    void SetConfigManager(class ConfigManager* config_mgr) { 
        config_mgr_ = config_mgr;
        profile_mgr_.SetConfigManager(config_mgr);
    }
    
    // Update UI to show the currently loaded default profile
    void UpdateDefaultProfileDisplay();
    
    // Process deferred PipeWire initialization
    void ProcessDeferredPipeWireInitialization();

private:
    void SyncWithCameraState();
    void RenderCameraSelection();
    void RenderResolutionSettings();
    void RenderResolutionControls();
    void RenderFPSControls();
    void RenderProfileSection();
    
    // Check if running in Flatpak environment
    bool IsRunningInFlatpak();
    
    AppState& state_;
    CameraManager& camera_mgr_;
    class EffectsManager& effects_mgr_;
    class ConfigManager* config_mgr_ = nullptr;
    ProfileManager profile_mgr_;
    
    // Sub-panels for modular UI
    VirtualCameraPanel virtual_camera_panel_;
    PipeWireOutputPanel pipewire_panel_;
    CameraControlsPanel camera_controls_panel_;
    
    // UI state
    int ui_cam_idx_ = 0;
    int ui_res_idx_ = 0;
    int ui_fps_idx_ = 0;
    int ui_fps_actual_idx_ = 0;
    bool show_resolution_warning_ = false;
    bool show_fps_warning_ = false;
    
    // Resolution and FPS display strings
    std::vector<std::string> res_strings_;
    std::vector<const char*> res_items_;
    std::vector<std::string> fps_strings_;
    std::vector<const char*> fps_items_;
    
    // Profile UI state
    char profile_name_buf_[256] = {0};
    bool show_set_default_ = false;
    int ui_profile_idx_ = 0;
    std::vector<std::string> profile_names_;
    bool profile_loaded_ = false;
    
    // PipeWire deferred initialization
    bool pipewire_initialized_ = false;
};

// Beauty effects panel
class BeautyPanel : public UIPanel {
public:
    BeautyPanel(AppState& state);
    ~BeautyPanel() override = default;
    
    void Render() override;

private:
    void RenderPresets();
    void RenderPerformanceControls();
    void RenderSkinSmoothing();
    void RenderLipEffects();
    void RenderTeethWhitening();
    void RenderSkinSmoothingControls();
    void RenderWrinkleControls();
    void RenderLipControls();
    void RenderTeethControls();
    void RenderBeautyPresets();
    void RenderAdvancedControls();
    void RenderAdvancedSkinControls();
    void RenderLipSliders();
    void RenderLipColorPresets();
    void ApplyLipColorPreset(const char* name, float r, float g, float b);
    void RenderTeethSliders();
    void RenderTeethPresets();
    void ApplyTeethPreset(const char* name, float strength, float margin);
    void RenderTeethTips();
    void ApplyBeautyPreset(int preset_index, const char* preset_name);
    BeautyState CreateBeautyStateFromAppState();
    void CopyBeautyFieldsToBeautyState(BeautyState& bs);
    void CopyWrinkleFieldsToBeautyState(BeautyState& bs);
    void CopyLipFieldsToBeautyState(BeautyState& bs);
    void CopyTeethFieldsToBeautyState(BeautyState& bs);
    void CopyBeautyStateToAppState(const BeautyState& bs);
    void CopyBeautyStateFieldsToAppState(const BeautyState& bs);
    void CopyWrinkleStateToAppState(const BeautyState& bs);
    void CopyLipStateToAppState(const BeautyState& bs);
    void CopyTeethStateToAppState(const BeautyState& bs);
    
    AppState& state_;
    
    // UI state
    int ui_preset_idx_ = 0;
    bool show_advanced_ = false;
};

// Background effects panel
class BackgroundPanel : public UIPanel {
public:
    BackgroundPanel(AppState& state);
    ~BackgroundPanel() override = default;
    
    void Render() override;

private:
    void RenderMaskControls();
    void RenderBackgroundMode();
    void RenderBackgroundModeSelection();
    void RenderBlurControls();
    void RenderImageControls();
    void RenderImageHeader();
    void RenderImagePathControls();
    void RenderImageDisplay();
    void RenderImageTips();
    void RenderSolidColorControls();
    void LoadImageFromPath(const char* path);
    void LoadImageFromClipboard();
    void LoadImageFromPortal();
    void ClearBackgroundImage();
    void RenderImageInfo();
    void RenderImageScaling();
    void RenderImageOpacity();
    void RenderImagePosition();
    void RenderResetPositionButton();
    
    AppState& state_;
    
    // UI state
    int ui_bg_mode_idx_ = 0;
    char bg_image_path_[512] = {0};
    int scale_mode_ = 0;
};

// Profile management panel
class ProfilePanel : public UIPanel {
public:
    ProfilePanel(class ConfigManager* config_mgr);
    ~ProfilePanel() override = default;
    
    void Render() override;
    
    // Update UI to show the currently loaded default profile
    void UpdateDefaultProfileDisplay();

private:
    void RenderProfileManagement();
    void RenderProfileList();
    void RenderProfileActions();
    
    class ConfigManager* config_mgr_;
    
    // UI state
    char profile_name_buf_[256] = {0};
    bool show_set_default_ = false;
    int ui_profile_idx_ = 0;
    std::vector<std::string> profile_names_;
    bool profile_loaded_ = false;
};

// Debug panel for overlay controls and performance stats
class DebugPanel : public UIPanel {
public:
    DebugPanel(AppState& state);
    ~DebugPanel() override = default;
    
    void Render() override;

private:
    void RenderOverlayControls();
    void RenderPerformanceStats();
    void RenderAdvancedSettings();
    void RenderBasicStats();
    void RenderPerformanceOptimization();
    void RenderManualProcessingScale();
    void RenderAutoProcessingScale();
    void RenderAutoProcessingScaleDetails();
    void RenderPerformanceStatus();
    
    AppState& state_;
};

// Status panel for system information and status display
class StatusPanel : public UIPanel {
public:
    StatusPanel(AppState& state);
    ~StatusPanel() override = default;
    
    void Render() override;

private:
    void RenderFPSInfo();
    void RenderSystemInfo();
    void RenderGraphInfo();
    
    AppState& state_;
};

// Status panel - temporarily disabled for compilation

} // namespace segmecam
