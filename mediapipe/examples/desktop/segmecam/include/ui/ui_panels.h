#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include "app_state.h"
#include "include/camera/camera_manager.h"
#include "cam_enum.h"

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
    void RenderProfileSection();
    void RenderCameraControls();
    
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
    bool SaveStateToProfile(const std::string& profile_name);
    
    // String storage for combo boxes (to prevent memory corruption)
    std::vector<std::string> res_strings_;
    std::vector<const char*> res_items_;
    std::vector<std::string> fps_strings_;
    std::vector<const char*> fps_items_;
    
    // Virtual camera device enumeration
    std::vector<LoopbackDesc> vcam_devices_;
    std::vector<std::string> vcam_labels_;
    std::vector<const char*> vcam_items_;
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
    void RenderSolidColorControls();
    void RenderMaskControls();
    
    AppState& state_;
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
    void RenderProfileList();
    void RenderProfileCreation();
    void RenderProfileActions();
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