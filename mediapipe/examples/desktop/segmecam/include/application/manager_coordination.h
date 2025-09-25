#ifndef MANAGER_COORDINATION_H
#define MANAGER_COORDINATION_H

#include <memory>
#include <iostream>
#include "src/config/config_manager.h"  // Include for complete type

// Include UI manager for complete type definition
#include "ui/ui_manager_enhanced.h"

// Forward declare segmecam::AppState to avoid circular dependencies  
namespace segmecam {
    struct AppState;
}

// Forward declarations to avoid circular dependencies
namespace segmecam {
    class CameraManager;
    class MediaPipeManager;
    class RenderManager;
    class EffectsManager;
    class ConfigManager;
}

class ManagerCoordination {
public:
    struct Managers {
        // Essential managers for modular architecture
        std::unique_ptr<segmecam::ConfigManager> config;
        std::unique_ptr<segmecam::CameraManager> camera;
        std::unique_ptr<segmecam::EffectsManager> effects;
        std::unique_ptr<segmecam::UIManager> ui;
        
        // TODO: Add other managers when their dependencies are resolved
        // std::unique_ptr<segmecam::MediaPipeManager> mediapipe;
        // std::unique_ptr<segmecam::RenderManager> render;
        
        // Default constructor
        Managers() = default;
        
        // Move constructor and assignment
        Managers(Managers&&) = default;
        Managers& operator=(Managers&&) = default;
        
        // Delete copy constructor and assignment
        Managers(const Managers&) = delete;
        Managers& operator=(const Managers&) = delete;
    };
    
    static bool SetupManagers(Managers& managers, segmecam::AppState& app_state);
    static void ShutdownManagers(Managers& managers);
    static bool ValidateManagers(const Managers& managers);
    
    // Post-initialization step to load default profile's background image
    static void LoadDefaultProfileBackgroundImage(Managers& managers, segmecam::AppState& app_state);
    
private:
    static bool InitializeConfigManager(Managers& managers, segmecam::AppState& app_state, segmecam::ConfigData& out_config_data);
    static bool InitializeCameraManager(Managers& managers, segmecam::AppState& app_state, const segmecam::ConfigData& config_data);
    static bool InitializeEffectsManager(Managers& managers, segmecam::AppState& app_state);
    static bool InitializeUIManager(Managers& managers, segmecam::AppState& app_state);
    
    // Helper methods for ConfigManager initialization to reduce complexity
    static bool CreateConfigManager(Managers& managers);
    static segmecam::ConfigData LoadDefaultProfile(Managers& managers, segmecam::AppState& app_state);
    static void ApplyProfileSettingsToAppState(segmecam::AppState& app_state, const segmecam::ConfigData& config_data);
    
    // Helper methods for profile loading to reduce complexity
    static void ApplyDisplaySettingsFromProfile(segmecam::AppState& app_state, const segmecam::ConfigData& config_data);
    static void ApplyBackgroundSettingsFromProfile(segmecam::AppState& app_state, const segmecam::ConfigData& config_data);
    static void ApplyBeautySettingsFromProfile(segmecam::AppState& app_state, const segmecam::ConfigData& config_data);
    static void ApplyPerformanceSettingsFromProfile(segmecam::AppState& app_state, const segmecam::ConfigData& config_data);
    static void ApplyCameraSettingsFromProfile(segmecam::AppState& app_state, const segmecam::ConfigData& config_data);
    static void ApplyCameraControlsFromProfile(Managers& managers, const segmecam::ConfigData& config_data);
    
    // Helper method to reduce cyclomatic complexity
    template<typename SetterFunc>
    static void ApplyCameraControlIfValid(int value, SetterFunc setter) {
        if (value >= 0) {
            setter(value);
        }
    }
};

#endif // MANAGER_COORDINATION_H