#pragma once

#include <string>
#include <vector>
#include <memory>
#include "mediapipe/framework/port/opencv_core_inc.h"

namespace segmecam {

// Forward declaration
struct ConfigData;

/**
 * ConfigManager - Phase 7: Configuration System
 * 
 * Manages application configuration, settings persistence, and profile management.
 * Extracted from monolithic main() to provide centralized configuration handling.
 * 
 * Key Responsibilities:
 * - Profile directory management and creation
 * - Settings save/load using OpenCV FileStorage YAML format
 * - Default profile management and persistence
 * - Configuration validation and error handling
 * - Settings migration and backward compatibility
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // Profile directory management
    std::string GetProfileDir() const;
    bool EnsureProfileDirExists() const;
    
    // Profile enumeration
    std::vector<std::string> ListProfiles() const;
    bool ProfileExists(const std::string& name) const;
    
    // Profile save/load operations
    bool SaveProfile(const std::string& name, const ConfigData& config) const;
    bool LoadProfile(const std::string& name, ConfigData& config) const;
    bool DeleteProfile(const std::string& name) const;
    
    // Default profile management
    bool SetDefaultProfile(const std::string& name) const;
    bool GetDefaultProfile(std::string& name) const;
    bool LoadDefaultProfile(ConfigData& config) const;
    
    // Configuration validation
    bool ValidateConfig(const ConfigData& config) const;
    
    // Utility functions
    std::string GetDefaultProfilePath() const;
    bool IsValidProfileName(const std::string& name) const;

private:
    // Internal helper functions
    cv::FileStorage OpenProfileForRead(const std::string& path) const;
    cv::FileStorage OpenProfileForWrite(const std::string& path) const;
    bool WriteConfigToStorage(cv::FileStorage& fs, const ConfigData& config) const;
    bool ReadConfigFromStorage(cv::FileStorage& fs, ConfigData& config) const;
    
    // Helper functions for type-safe reading
    int ReadInt(const cv::FileNode& node, int defaultValue) const;
    float ReadFloat(const cv::FileNode& node, float defaultValue) const;
    std::string ReadString(const cv::FileNode& node, const std::string& defaultValue) const;
    void ReadColorArray(const cv::FileNode& node, float* color, const float* defaultColor) const;

    std::string profile_dir_;
    std::string default_profile_path_;
    
    // Configuration file extension
    static constexpr const char* PROFILE_EXTENSION = ".yml";
    static constexpr const char* DEFAULT_PROFILE_FILENAME = "default_profile.txt";
};

/**
 * ConfigData - Complete application configuration structure
 * 
 * Contains all settings that can be saved/loaded to/from profiles.
 * Organized by logical groups for maintainability.
 */
struct ConfigData {
    // Camera settings
    struct CameraConfig {
        std::string cam_path;
        int res_w = 0;
        int res_h = 0;
        int fps_value = 0;
        int ui_cam_idx = -1;
        int ui_res_idx = -1;
        int ui_fps_idx = -1;
    } camera;
    
    // Display settings
    struct DisplayConfig {
        bool vsync_on = true;
        bool show_mask = false;
        bool show_landmarks = false;
        bool show_mesh = false;
        bool show_mesh_dense = false;
    } display;
    
    // Background settings
    struct BackgroundConfig {
        int bg_mode = 0;  // 0=None, 1=Blur, 2=Image, 3=Color
        int blur_strength = 25;
        float feather_px = 2.0f;
        float solid_color[3] = {0.0f, 0.0f, 0.0f};
        std::string bg_path;
    } background;
    
    // Landmark settings
    struct LandmarkConfig {
        bool lm_roi_mode = false;
        bool lm_apply_rot = true;
        bool lm_flip_x = false;
        bool lm_flip_y = false;
        bool lm_swap_xy = false;
    } landmarks;
    
    // Beauty effects settings
    struct BeautyConfig {
        // Core skin smoothing
        bool fx_skin = false;
        bool fx_skin_adv = true;
        float fx_skin_strength = 0.4f;
        float fx_skin_amount = 0.5f;
        float fx_skin_radius = 6.0f;
        float fx_skin_tex = 0.35f;
        float fx_skin_edge = 12.0f;
        
        // Advanced parameters
        float fx_adv_scale = 0.8f;  // Changed default to optimized value
        float fx_adv_detail_preserve = 0.18f;
        
        // Auto processing scale
        bool auto_processing_scale = false;
        float target_fps = 14.5f;
        
        // Wrinkle-aware settings
        bool fx_skin_wrinkle = true;
        float fx_skin_smile_boost = 0.5f;
        float fx_skin_squint_boost = 0.5f;
        float fx_skin_forehead_boost = 0.8f;
        float fx_skin_wrinkle_gain = 1.5f;
        
        // Wrinkle processing controls
        bool fx_wrinkle_suppress_lower = true;
        float fx_wrinkle_lower_ratio = 0.45f;
        bool fx_wrinkle_ignore_glasses = true;
        float fx_wrinkle_glasses_margin = 12.0f;
        float fx_wrinkle_keep_ratio = 0.35f;
        bool fx_wrinkle_custom_scales = true;
        float fx_wrinkle_min_px = 2.0f;
        float fx_wrinkle_max_px = 8.0f;
        bool fx_wrinkle_use_skin_gate = false;
        float fx_wrinkle_mask_gain = 2.0f;
        float fx_wrinkle_baseline = 0.5f;
        float fx_wrinkle_neg_cap = 0.9f;
        bool fx_wrinkle_preview = false;
        
        // Lipstick settings
        bool fx_lipstick = false;
        float fx_lip_alpha = 0.5f;
        float fx_lip_feather = 6.0f;
        float fx_lip_light = 0.0f;
        float fx_lip_band = 4.0f;
        float fx_lip_color[3] = {0.8f, 0.1f, 0.3f};
        
        // Teeth whitening settings
        bool fx_teeth = false;
        float fx_teeth_strength = 0.5f;
        float fx_teeth_margin = 3.0f;
    } beauty;
    
    // Performance settings
    struct PerformanceConfig {
        bool use_opencl = true; // Enable by default if available
    } performance;
    
    // Debug settings
    struct DebugConfig {
        // No debug settings currently needed
    } debug;
    
    // Reset to default values
    void Reset();
    
    // Apply preset to beauty settings (integrates with existing presets.h)
    void ApplyBeautyPreset(int preset_index);
};

} // namespace segmecam