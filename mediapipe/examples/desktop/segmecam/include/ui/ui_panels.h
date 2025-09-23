#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include "include/application/app_state.h"
#include "include/camera/camera_manager.h"
#include "cam_enum.h"
#include "src/config/config_manager.h"

namespace segmecam {

// Base class for all UI panels
class UIPanel {
public:
    UIPanel(const std::string& name) : panel_name_(name) {}
    virtual ~UIPanel() = default;
    
    virtual void Render() = 0;
    
    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }
    const std::string& GetName() const { return panel_name_; }

protected:
    std::string panel_name_;
    bool visible_ = true;
};

// Camera selection and controls panel
class CameraPanel : public UIPanel {
public:
    CameraPanel(AppState& state, CameraManager& camera_mgr, class EffectsManager& effects_mgr);
    ~CameraPanel() override = default;
    
    void Render() override;
    
    // Set config manager for profile functionality
    void SetConfigManager(class ConfigManager* config_mgr) { config_mgr_ = config_mgr; }
    
    // Update UI to show the currently loaded default profile
    void UpdateDefaultProfileDisplay();

private:
    void RenderCameraSelection();
    void RenderResolutionSettings();
    void RenderVirtualCameraControls();
    // Profile section rendering
    void RenderProfileSection();
    void RenderProfileHeader();
    void HandleProfileSelection();
    void ProcessProfileLoadRequest();
    void HandleProfileSaveButton();
    void RenderCameraControls();
    
    // Helper methods for resolution settings
    void RenderResolutionControls();
    void RenderFPSControls();
    
    // Helper methods for virtual camera controls
    void RenderVirtualCameraActive();
    void RenderVirtualCameraInactive();
    void RenderVirtualCameraDeviceSelection();
    void RenderVirtualCameraResolutionInfo();
    void RenderVirtualCameraStartButton();
    void RenderVirtualCameraHelp();
    
    // Helper methods for camera controls
    void GetControlRanges();
    void RenderBasicControls();
    void RenderGainExposureControls();
    void RenderGainControls();
    void RenderExposureControls();
    void RenderBacklightControl();
    void RenderAdditionalControls();
    void RenderWhiteBalanceControls();
    void RenderResetButton();
    
    // Helper methods for exposure auto control
    void DisableAutoExposure();
    void EnableAutoExposure(int current_mode);
    
    // Helper methods for V4L2 controls
    void SliderCtrl(const char* label, CtrlRange& range, uint32_t control_id);
    void CheckboxCtrl(const char* label, CtrlRange& range, uint32_t control_id);
    void CheckboxExposureAuto(const char* label);
    
    // Sync UI state with camera settings
    void SyncWithCameraState();
    
    // Virtual camera device management
    void RefreshVirtualCameraDevices();
    
    AppState& state_;
    CameraManager& camera_mgr_;
    class EffectsManager& effects_mgr_;
    class ConfigManager* config_mgr_ = nullptr;
    
    // UI state
    int ui_cam_idx_ = 0;
    int ui_res_idx_ = 0;
    int ui_fps_idx_ = 0;
    int ui_vcam_idx_ = 0;
    std::vector<int> ui_fps_opts_;
    
    // Profile management UI state  
    int ui_profile_idx_ = -1;
    char profile_name_buf_[128] = {0};
    
    // Profile helper methods
    void LoadProfileIntoState(const std::string& profile_name);
    bool ValidateProfileLoad(const std::string& profile_name);
    void LoadCameraSettings(const ConfigData& config);
    void LoadCameraSelection(const ConfigData& config, bool in_flatpak);
    void LoadResolutionSettings(const ConfigData& config, bool in_flatpak);
    void LoadDisplaySettings(const ConfigData& config);
    void LoadBackgroundSettings(const ConfigData& config);
    void LoadLandmarkSettings(const ConfigData& config);
    void LoadBeautySettings(const ConfigData& config);
    void LoadPerformanceSettings(const ConfigData& config);
    bool SaveStateToProfile(const std::string& profile_name);
    bool ValidateProfileSave(const std::string& profile_name);
    void SaveCameraSettings(ConfigData& config);
    void SaveDisplaySettings(ConfigData& config);
    void SaveBackgroundSettings(ConfigData& config);
    void SaveLandmarkSettings(ConfigData& config);
    void SaveBeautySettings(ConfigData& config);
    void SaveBasicBeautySettings(ConfigData& config);
    void SaveWrinkleSettings(ConfigData& config);
    void SaveLipSettings(ConfigData& config);
    void SaveTeethSettings(ConfigData& config);
    void SavePerformanceSettings(ConfigData& config);
    bool SaveProfileToManager(const std::string& profile_name, const ConfigData& config);
    
    // String storage for combo boxes (to prevent memory corruption)
    std::vector<std::string> res_strings_;
    std::vector<const char*> res_items_;
    std::vector<std::string> fps_strings_;
    std::vector<const char*> fps_items_;
    
    // Virtual camera device enumeration
    std::vector<LoopbackDesc> vcam_devices_;
    std::vector<std::string> vcam_labels_;
    std::vector<const char*> vcam_items_;
    
    // Cached control ranges for camera controls
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
};

// Background and compositing effects panel  
class BackgroundPanel : public UIPanel {
public:
    BackgroundPanel(AppState& state);
    ~BackgroundPanel() override = default;
    
    void Render() override;

private:
    void RenderBackgroundMode();
    void RenderBlurControls();
    void RenderImageControls();
    void RenderImageHeader();
    void RenderImagePathControls();
    void LoadImageFromPath(const char* path);
    void LoadImageFromClipboard();
    void LoadImageFromPortal();
    void ClearBackgroundImage();
    void RenderImageDisplay();
    void RenderImageInfo();
    void RenderImageScaling();
    void RenderImageOpacity();
    void RenderImagePosition();
    void RenderResetPositionButton();
    void RenderImageTips();
    void RenderSolidColorControls();
    void RenderMaskControls();
    
    AppState& state_;
    int scale_mode_ = 1;  // Background image scaling mode (0=Stretch, 1=Fit, 2=Fill, 3=Center, 4=Tile)
};

// Beauty and face effects panel
class BeautyPanel : public UIPanel {
public:
    BeautyPanel(AppState& state);
    ~BeautyPanel() override = default;
    
    void Render() override;

private:
    void RenderPresets();
    void RenderSkinSmoothing();
    void RenderAdvancedSkinControls();
    void RenderWrinkleControls();
    void RenderLipEffects();
    void RenderTeethWhitening();
    void RenderPerformanceControls();
    
    AppState& state_;
};

// Profile management panel
class ProfilePanel : public UIPanel {
public:
    ProfilePanel(AppState& state, CameraManager& camera_mgr);
    ~ProfilePanel() override = default;
    
    void Render() override;
    
    // Set config manager for profile persistence
    void SetConfigManager(ConfigManager* config_mgr) { config_mgr_ = config_mgr; }

private:
    void RenderProfileSelection();
    void RenderProfileActions();
    void HandleProfileLoad();
    void HandleProfileSave();
    void UpdateProfileIndexAfterSave();
    
    void RenderProfileList();
    void RenderProfileCreation();
    void RenderProfileActionButtons();
    void LoadProfileIntoState(const std::string& profile_name);
    bool SaveStateToProfile(const std::string& profile_name);
    
    AppState& state_;
    CameraManager& camera_mgr_;
    ConfigManager* config_mgr_ = nullptr;
    
    // UI state
    char profile_name_buf_[128] = {0};
    int ui_profile_idx_ = -1;
    std::vector<std::string> profile_names_;
    
    // Status tracking
    std::string last_loaded_profile_;
    bool profile_list_dirty_ = true;
};

// Debug and overlay panel
class DebugPanel : public UIPanel {
public:
    DebugPanel(AppState& state);
    ~DebugPanel() override = default;
    
    void Render() override;

private:
    void RenderOverlayControls();
    void RenderDebugVisualization();
    void RenderPerformanceStats();
    void RenderBasicStats();
    void RenderPerformanceOptimization();
    void RenderManualProcessingScale();
    void RenderAutoProcessingScale();
    void RenderAutoProcessingScaleDetails();
    void RenderPerformanceStatus();
    void RenderAdvancedSettings();
    
    AppState& state_;
};

// Status and information panel
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

} // namespace segmecam

// Common UI utilities for profile management
namespace segmecam {
namespace ui_utils {

// Common profile selection UI component
// Returns true if a profile was loaded
bool RenderProfileSelection(class ConfigManager* config_mgr, int& ui_profile_idx, 
                           char* profile_name_buf, size_t buf_size,
                           const std::string& input_label = "Name");

} // namespace ui_utils
} // namespace segmecam