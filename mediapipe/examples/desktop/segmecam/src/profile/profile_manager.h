#pragma once

#include "include/application/app_state.h"
#include "include/camera/camera_manager.h"
#include "include/effects/effects_manager.h"
#include "src/config/config_manager.h"
#include <string>
#include <iostream>

namespace segmecam {

// Profile management functionality extracted from CameraPanel
class ProfileManager {
public:
    ProfileManager(AppState& state, CameraManager& camera_mgr, class EffectsManager& effects_mgr);
    
    // Set config manager (called by CameraPanel)
    void SetConfigManager(class ConfigManager* config_mgr) { config_mgr_ = config_mgr; }
    
    // Profile data operations
    bool LoadProfileIntoState(const std::string& profile_name);
    bool SaveStateToProfile(const std::string& profile_name);
    
    // Update default profile display (for UI integration)
    void UpdateDefaultProfileDisplay();
    
private:
    // Validation methods
    bool ValidateProfileLoad(const std::string& profile_name);
    bool ValidateProfileSave(const std::string& profile_name);
    
    // Loading methods
    void LoadCameraSettings(const ConfigData& config);
    void LoadCameraSelection(const ConfigData& config, bool in_flatpak);
    void LoadResolutionSettings(const ConfigData& config, bool in_flatpak);
    void LoadDisplaySettings(const ConfigData& config);
    void LoadBackgroundSettings(const ConfigData& config);
    void LoadLandmarkSettings(const ConfigData& config);
    void LoadBeautySettings(const ConfigData& config);
    void LoadPerformanceSettings(const ConfigData& config);
    
    // Saving methods
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
    
    // Utility methods
    bool IsFlatpakEnvironment() const;
    
    // Member variables
    AppState& state_;
    CameraManager& camera_mgr_;
    class EffectsManager& effects_mgr_;
    class ConfigManager* config_mgr_ = nullptr;
};

} // namespace segmecam