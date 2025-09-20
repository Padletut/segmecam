#include "config_manager.h"
#include "presets.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cstdlib>

namespace segmecam {

ConfigManager::ConfigManager() {
    profile_dir_ = GetProfileDir();
    default_profile_path_ = profile_dir_ + "/" + DEFAULT_PROFILE_FILENAME;
    EnsureProfileDirExists();
}

ConfigManager::~ConfigManager() = default;

std::string ConfigManager::GetProfileDir() const {
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config && *xdg_config) {
        return std::string(xdg_config) + "/segmecam";
    }

    const char* home = std::getenv("HOME");
    std::string dir = (home && *home) ? 
        std::string(home) + "/.config/segmecam" : 
        std::string("./.segmecam");
    return dir;
}

bool ConfigManager::EnsureProfileDirExists() const {
    std::error_code ec;
    std::filesystem::create_directories(profile_dir_, ec);
    return !ec;
}

std::vector<std::string> ConfigManager::ListProfiles() const {
    std::vector<std::string> names;
    std::error_code ec;
    
    for (const auto& entry : std::filesystem::directory_iterator(profile_dir_, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        
        auto path = entry.path();
        if (path.extension() == PROFILE_EXTENSION) {
            names.push_back(path.stem().string());
        }
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

bool ConfigManager::ProfileExists(const std::string& name) const {
    if (!IsValidProfileName(name)) return false;
    std::string path = profile_dir_ + "/" + name + PROFILE_EXTENSION;
    return std::filesystem::exists(path);
}

bool ConfigManager::SaveProfile(const std::string& name, const ConfigData& config) const {
    if (!IsValidProfileName(name)) {
        std::cerr << "ConfigManager: Invalid profile name: " << name << std::endl;
        return false;
    }
    
    if (!ValidateConfig(config)) {
        std::cerr << "ConfigManager: Invalid configuration data" << std::endl;
        return false;
    }
    
    EnsureProfileDirExists();
    std::string path = profile_dir_ + "/" + name + PROFILE_EXTENSION;
    
    cv::FileStorage fs = OpenProfileForWrite(path);
    if (!fs.isOpened()) {
        std::cerr << "ConfigManager: Failed to open profile for writing: " << path << std::endl;
        return false;
    }
    
    bool success = WriteConfigToStorage(fs, config);
    fs.release();
    
    if (success) {
        std::cout << "ConfigManager: Saved profile '" << name << "' to " << path << std::endl;
    }
    
    return success;
}

bool ConfigManager::LoadProfile(const std::string& name, ConfigData& config) const {
    if (!IsValidProfileName(name)) {
        std::cerr << "ConfigManager: Invalid profile name: " << name << std::endl;
        return false;
    }
    
    std::string path = profile_dir_ + "/" + name + PROFILE_EXTENSION;
    
    cv::FileStorage fs = OpenProfileForRead(path);
    if (!fs.isOpened()) {
        std::cerr << "ConfigManager: Failed to open profile for reading: " << path << std::endl;
        return false;
    }
    
    cv::FileNode root = fs.root();
    if (root.empty() || !root.isMap()) {
        std::cerr << "ConfigManager: Invalid or empty profile file: " << path << std::endl;
        fs.release();
        return false;
    }
    
    bool success = ReadConfigFromStorage(fs, config);
    fs.release();
    
    if (success) {
        std::cout << "ConfigManager: Loaded profile '" << name << "' from " << path << std::endl;
    }
    
    return success;
}

bool ConfigManager::DeleteProfile(const std::string& name) const {
    if (!IsValidProfileName(name)) return false;
    
    std::string path = profile_dir_ + "/" + name + PROFILE_EXTENSION;
    std::error_code ec;
    bool removed = std::filesystem::remove(path, ec);
    
    if (removed && !ec) {
        std::cout << "ConfigManager: Deleted profile '" << name << "'" << std::endl;
    }
    
    return removed && !ec;
}

bool ConfigManager::SetDefaultProfile(const std::string& name) const {
    if (!ProfileExists(name)) {
        std::cerr << "ConfigManager: Cannot set non-existent profile as default: " << name << std::endl;
        return false;
    }
    
    std::ofstream fout(default_profile_path_);
    if (!fout) {
        std::cerr << "ConfigManager: Failed to write default profile file: " << default_profile_path_ << std::endl;
        return false;
    }
    
    fout << name << std::endl;
    fout.close();
    
    std::cout << "ConfigManager: Set default profile to '" << name << "'" << std::endl;
    return true;
}

bool ConfigManager::GetDefaultProfile(std::string& name) const {
    std::ifstream fin(default_profile_path_);
    if (!fin) return false;
    
    std::getline(fin, name);
    fin.close();
    
    // Validate that the default profile still exists
    if (!name.empty() && !ProfileExists(name)) {
        std::cerr << "ConfigManager: Default profile '" << name << "' no longer exists" << std::endl;
        return false;
    }
    
    return !name.empty();
}

bool ConfigManager::LoadDefaultProfile(ConfigData& config) const {
    std::string default_name;
    if (!GetDefaultProfile(default_name)) {
        return false;
    }
    
    return LoadProfile(default_name, config);
}

bool ConfigManager::ValidateConfig(const ConfigData& config) const {
    // Basic validation - can be extended as needed
    if (config.camera.res_w < 0 || config.camera.res_h < 0) return false;
    if (config.camera.fps_value < 0) return false;
    if (config.background.bg_mode < 0 || config.background.bg_mode > 3) return false;
    if (config.background.blur_strength < 1) return false;
    if (config.background.feather_px < 0.0f) return false;
    
    // Validate color arrays are in valid range [0.0, 1.0]
    for (int i = 0; i < 3; i++) {
        if (config.background.solid_color[i] < 0.0f || config.background.solid_color[i] > 1.0f) return false;
        if (config.beauty.fx_lip_color[i] < 0.0f || config.beauty.fx_lip_color[i] > 1.0f) return false;
    }
    
    return true;
}

std::string ConfigManager::GetDefaultProfilePath() const {
    return default_profile_path_;
}

bool ConfigManager::IsValidProfileName(const std::string& name) const {
    if (name.empty() || name.length() > 100) return false;
    
    // Check for invalid characters
    for (char c : name) {
        if (!std::isalnum(c) && c != '_' && c != '-' && c != ' ') {
            return false;
        }
    }
    
    return true;
}

// Private implementation methods

cv::FileStorage ConfigManager::OpenProfileForRead(const std::string& path) const {
    return cv::FileStorage(path, cv::FileStorage::READ);
}

cv::FileStorage ConfigManager::OpenProfileForWrite(const std::string& path) const {
    return cv::FileStorage(path, cv::FileStorage::WRITE);
}

bool ConfigManager::WriteConfigToStorage(cv::FileStorage& fs, const ConfigData& config) const {
    try {
        // Camera settings
        fs << "cam_path" << config.camera.cam_path;
        fs << "res_w" << config.camera.res_w;
        fs << "res_h" << config.camera.res_h;
        fs << "fps_value" << config.camera.fps_value;
        fs << "ui_cam_idx" << config.camera.ui_cam_idx;
        fs << "ui_res_idx" << config.camera.ui_res_idx;
        fs << "ui_fps_idx" << config.camera.ui_fps_idx;
        
        // Display settings
        fs << "vsync_on" << (int)config.display.vsync_on;
        fs << "show_mask" << (int)config.display.show_mask;
        fs << "show_landmarks" << (int)config.display.show_landmarks;
        fs << "show_mesh" << (int)config.display.show_mesh;
        fs << "show_mesh_dense" << (int)config.display.show_mesh_dense;
        
        // Background settings
        fs << "bg_mode" << config.background.bg_mode;
        fs << "blur_strength" << config.background.blur_strength;
        fs << "feather_px" << config.background.feather_px;
        fs << "solid_color" << "[" << config.background.solid_color[0] 
           << config.background.solid_color[1] << config.background.solid_color[2] << "]";
        fs << "bg_path" << config.background.bg_path;
        
        // Landmark settings
        fs << "lm_roi_mode" << (int)config.landmarks.lm_roi_mode;
        fs << "lm_apply_rot" << (int)config.landmarks.lm_apply_rot;
        fs << "lm_flip_x" << (int)config.landmarks.lm_flip_x;
        fs << "lm_flip_y" << (int)config.landmarks.lm_flip_y;
        fs << "lm_swap_xy" << (int)config.landmarks.lm_swap_xy;
        
        // Beauty effects settings
        fs << "fx_skin" << (int)config.beauty.fx_skin;
        fs << "fx_skin_adv" << (int)config.beauty.fx_skin_adv;
        fs << "fx_skin_strength" << config.beauty.fx_skin_strength;
        fs << "fx_skin_amount" << config.beauty.fx_skin_amount;
        fs << "fx_skin_radius" << config.beauty.fx_skin_radius;
        fs << "fx_skin_tex" << config.beauty.fx_skin_tex;
        fs << "fx_skin_edge" << config.beauty.fx_skin_edge;
        fs << "fx_adv_scale" << config.beauty.fx_adv_scale;
        fs << "fx_adv_detail_preserve" << config.beauty.fx_adv_detail_preserve;
        
        // Auto processing scale
        fs << "auto_processing_scale" << (int)config.beauty.auto_processing_scale;
        fs << "target_fps" << config.beauty.target_fps;
        
        // Wrinkle-aware settings
        fs << "fx_skin_wrinkle" << (int)config.beauty.fx_skin_wrinkle;
        fs << "fx_skin_smile_boost" << config.beauty.fx_skin_smile_boost;
        fs << "fx_skin_squint_boost" << config.beauty.fx_skin_squint_boost;
        fs << "fx_skin_forehead_boost" << config.beauty.fx_skin_forehead_boost;
        fs << "fx_skin_wrinkle_gain" << config.beauty.fx_skin_wrinkle_gain;
        
        // Wrinkle processing controls
        fs << "fx_wrinkle_suppress_lower" << (int)config.beauty.fx_wrinkle_suppress_lower;
        fs << "fx_wrinkle_lower_ratio" << config.beauty.fx_wrinkle_lower_ratio;
        fs << "fx_wrinkle_ignore_glasses" << (int)config.beauty.fx_wrinkle_ignore_glasses;
        fs << "fx_wrinkle_glasses_margin" << config.beauty.fx_wrinkle_glasses_margin;
        fs << "fx_wrinkle_keep_ratio" << config.beauty.fx_wrinkle_keep_ratio;
        fs << "fx_wrinkle_custom_scales" << (int)config.beauty.fx_wrinkle_custom_scales;
        fs << "fx_wrinkle_min_px" << config.beauty.fx_wrinkle_min_px;
        fs << "fx_wrinkle_max_px" << config.beauty.fx_wrinkle_max_px;
        fs << "fx_wrinkle_use_skin_gate" << (int)config.beauty.fx_wrinkle_use_skin_gate;
        fs << "fx_wrinkle_mask_gain" << config.beauty.fx_wrinkle_mask_gain;
        fs << "fx_wrinkle_baseline" << config.beauty.fx_wrinkle_baseline;
        fs << "fx_wrinkle_neg_cap" << config.beauty.fx_wrinkle_neg_cap;
        fs << "fx_wrinkle_preview" << (int)config.beauty.fx_wrinkle_preview;
        
        // Lipstick settings
        fs << "fx_lipstick" << (int)config.beauty.fx_lipstick;
        fs << "fx_lip_alpha" << config.beauty.fx_lip_alpha;
        fs << "fx_lip_feather" << config.beauty.fx_lip_feather;
        fs << "fx_lip_light" << config.beauty.fx_lip_light;
        fs << "fx_lip_band" << config.beauty.fx_lip_band;
        fs << "fx_lip_color" << "[" << config.beauty.fx_lip_color[0] 
           << config.beauty.fx_lip_color[1] << config.beauty.fx_lip_color[2] << "]";
        
        // Teeth whitening settings
        fs << "fx_teeth" << (int)config.beauty.fx_teeth;
        fs << "fx_teeth_strength" << config.beauty.fx_teeth_strength;
        fs << "fx_teeth_margin" << config.beauty.fx_teeth_margin;
        
        // Performance settings
        fs << "use_opencl" << (int)config.performance.use_opencl;
        
        // Debug settings (currently none)
        
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "ConfigManager: OpenCV error writing config: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "ConfigManager: Error writing config: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::ReadConfigFromStorage(cv::FileStorage& fs, ConfigData& config) const {
    try {
        cv::FileNode root = fs.root();
        
        // Camera settings
        config.camera.cam_path = ReadString(root["cam_path"], "");
        config.camera.res_w = ReadInt(root["res_w"], 0);
        config.camera.res_h = ReadInt(root["res_h"], 0);
        config.camera.fps_value = ReadInt(root["fps_value"], 0);
        config.camera.ui_cam_idx = ReadInt(root["ui_cam_idx"], -1);
        config.camera.ui_res_idx = ReadInt(root["ui_res_idx"], -1);
        config.camera.ui_fps_idx = ReadInt(root["ui_fps_idx"], -1);
        
        // Display settings
        config.display.vsync_on = ReadInt(root["vsync_on"], 1) != 0;
        config.display.show_mask = ReadInt(root["show_mask"], 0) != 0;
        config.display.show_landmarks = ReadInt(root["show_landmarks"], 0) != 0;
        config.display.show_mesh = ReadInt(root["show_mesh"], 0) != 0;
        config.display.show_mesh_dense = ReadInt(root["show_mesh_dense"], 0) != 0;
        
        // Background settings
        config.background.bg_mode = ReadInt(root["bg_mode"], 0);
        config.background.blur_strength = ReadInt(root["blur_strength"], 25);
        config.background.feather_px = ReadFloat(root["feather_px"], 2.0f);
        ReadColorArray(root["solid_color"], config.background.solid_color, 
                      (const float[]){0.0f, 0.0f, 0.0f});
        config.background.bg_path = ReadString(root["bg_path"], "");
        
        // Landmark settings
        config.landmarks.lm_roi_mode = ReadInt(root["lm_roi_mode"], 0) != 0;
        config.landmarks.lm_apply_rot = ReadInt(root["lm_apply_rot"], 1) != 0;
        config.landmarks.lm_flip_x = ReadInt(root["lm_flip_x"], 0) != 0;
        config.landmarks.lm_flip_y = ReadInt(root["lm_flip_y"], 0) != 0;
        config.landmarks.lm_swap_xy = ReadInt(root["lm_swap_xy"], 0) != 0;
        
        // Beauty effects settings
        config.beauty.fx_skin = ReadInt(root["fx_skin"], 0) != 0;
        config.beauty.fx_skin_adv = ReadInt(root["fx_skin_adv"], 1) != 0;
        config.beauty.fx_skin_strength = ReadFloat(root["fx_skin_strength"], 0.4f);
        config.beauty.fx_skin_amount = ReadFloat(root["fx_skin_amount"], 0.5f);
        config.beauty.fx_skin_radius = ReadFloat(root["fx_skin_radius"], 6.0f);
        config.beauty.fx_skin_tex = ReadFloat(root["fx_skin_tex"], 0.35f);
        config.beauty.fx_skin_edge = ReadFloat(root["fx_skin_edge"], 12.0f);
        config.beauty.fx_adv_scale = ReadFloat(root["fx_adv_scale"], 0.8f);
        config.beauty.fx_adv_detail_preserve = ReadFloat(root["fx_adv_detail_preserve"], 0.18f);
        
        // Auto processing scale settings
        config.beauty.auto_processing_scale = ReadInt(root["auto_processing_scale"], 1) != 0; // Default enabled
        config.beauty.target_fps = ReadFloat(root["target_fps"], 14.5f);
        
        // Migration: Ensure auto processing scale is enabled for better performance
        // This helps migrate profiles saved before auto-scale was enabled by default
        if (!config.beauty.auto_processing_scale) {
            std::cout << "ConfigManager: Migrating profile to enable auto processing scale by default" << std::endl;
            config.beauty.auto_processing_scale = true;
        }
        
        // Wrinkle-aware settings
        config.beauty.fx_skin_wrinkle = ReadInt(root["fx_skin_wrinkle"], 1) != 0;
        config.beauty.fx_skin_smile_boost = ReadFloat(root["fx_skin_smile_boost"], 0.5f);
        config.beauty.fx_skin_squint_boost = ReadFloat(root["fx_skin_squint_boost"], 0.5f);
        config.beauty.fx_skin_forehead_boost = ReadFloat(root["fx_skin_forehead_boost"], 0.8f);
        config.beauty.fx_skin_wrinkle_gain = ReadFloat(root["fx_skin_wrinkle_gain"], 1.5f);
        
        // Wrinkle processing controls
        config.beauty.fx_wrinkle_suppress_lower = ReadInt(root["fx_wrinkle_suppress_lower"], 1) != 0;
        config.beauty.fx_wrinkle_lower_ratio = ReadFloat(root["fx_wrinkle_lower_ratio"], 0.45f);
        config.beauty.fx_wrinkle_ignore_glasses = ReadInt(root["fx_wrinkle_ignore_glasses"], 1) != 0;
        config.beauty.fx_wrinkle_glasses_margin = ReadFloat(root["fx_wrinkle_glasses_margin"], 12.0f);
        config.beauty.fx_wrinkle_keep_ratio = ReadFloat(root["fx_wrinkle_keep_ratio"], 0.35f);
        config.beauty.fx_wrinkle_custom_scales = ReadInt(root["fx_wrinkle_custom_scales"], 1) != 0;
        config.beauty.fx_wrinkle_min_px = ReadFloat(root["fx_wrinkle_min_px"], 2.0f);
        config.beauty.fx_wrinkle_max_px = ReadFloat(root["fx_wrinkle_max_px"], 8.0f);
        config.beauty.fx_wrinkle_use_skin_gate = ReadInt(root["fx_wrinkle_use_skin_gate"], 0) != 0;
        config.beauty.fx_wrinkle_mask_gain = ReadFloat(root["fx_wrinkle_mask_gain"], 2.0f);
        config.beauty.fx_wrinkle_baseline = ReadFloat(root["fx_wrinkle_baseline"], 0.5f);
        config.beauty.fx_wrinkle_neg_cap = ReadFloat(root["fx_wrinkle_neg_cap"], 0.9f);
        config.beauty.fx_wrinkle_preview = ReadInt(root["fx_wrinkle_preview"], 0) != 0;
        
        // Lipstick settings
        config.beauty.fx_lipstick = ReadInt(root["fx_lipstick"], 0) != 0;
        config.beauty.fx_lip_alpha = ReadFloat(root["fx_lip_alpha"], 0.5f);
        config.beauty.fx_lip_feather = ReadFloat(root["fx_lip_feather"], 6.0f);
        config.beauty.fx_lip_light = ReadFloat(root["fx_lip_light"], 0.0f);
        config.beauty.fx_lip_band = ReadFloat(root["fx_lip_band"], 4.0f);
        ReadColorArray(root["fx_lip_color"], config.beauty.fx_lip_color, 
                      (const float[]){0.8f, 0.1f, 0.3f});
        
        // Teeth whitening settings
        config.beauty.fx_teeth = ReadInt(root["fx_teeth"], 0) != 0;
        config.beauty.fx_teeth_strength = ReadFloat(root["fx_teeth_strength"], 0.5f);
        config.beauty.fx_teeth_margin = ReadFloat(root["fx_teeth_margin"], 3.0f);
        
        // Performance settings
        config.performance.use_opencl = ReadInt(root["use_opencl"], 1) != 0; // Default to enabled
        
        // Debug settings (currently none)
        
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "ConfigManager: OpenCV error reading config: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "ConfigManager: Error reading config: " << e.what() << std::endl;
        return false;
    }
}

// Type-safe helper functions

int ConfigManager::ReadInt(const cv::FileNode& node, int defaultValue) const {
    return node.empty() ? defaultValue : (int)node;
}

float ConfigManager::ReadFloat(const cv::FileNode& node, float defaultValue) const {
    return node.empty() ? defaultValue : (float)node;
}

std::string ConfigManager::ReadString(const cv::FileNode& node, const std::string& defaultValue) const {
    if (node.empty()) return defaultValue;
    std::string result;
    node >> result;
    return result;
}

void ConfigManager::ReadColorArray(const cv::FileNode& node, float* color, const float* defaultColor) const {
    if (!node.empty() && node.isSeq()) {
        int i = 0;
        for (auto it = node.begin(); it != node.end() && i < 3; ++it, ++i) {
            color[i] = (float)((double)*it);
        }
    } else {
        // Use default color
        for (int i = 0; i < 3; i++) {
            color[i] = defaultColor[i];
        }
    }
}

// ConfigData implementation

void ConfigData::Reset() {
    *this = ConfigData{};  // Reset to default-constructed state
}

void ConfigData::ApplyBeautyPreset(int preset_index) {
    // Convert ConfigData beauty settings to BeautyState format
    BeautyState state;
    
    // Copy current beauty settings to BeautyState
    state.bg_mode = background.bg_mode;
    state.blur_strength = background.blur_strength;
    state.feather_px = background.feather_px;
    state.show_mask = display.show_mask;
    
    state.fx_skin = beauty.fx_skin;
    state.fx_skin_adv = beauty.fx_skin_adv;
    state.fx_skin_amount = beauty.fx_skin_amount;
    state.fx_skin_radius = beauty.fx_skin_radius;
    state.fx_skin_tex = beauty.fx_skin_tex;
    state.fx_skin_edge = beauty.fx_skin_edge;
    state.fx_skin_wrinkle = beauty.fx_skin_wrinkle;
    state.fx_skin_smile_boost = beauty.fx_skin_smile_boost;
    state.fx_skin_squint_boost = beauty.fx_skin_squint_boost;
    state.fx_skin_forehead_boost = beauty.fx_skin_forehead_boost;
    state.fx_skin_wrinkle_gain = beauty.fx_skin_wrinkle_gain;
    state.fx_wrinkle_suppress_lower = beauty.fx_wrinkle_suppress_lower;
    state.fx_wrinkle_lower_ratio = beauty.fx_wrinkle_lower_ratio;
    state.fx_wrinkle_ignore_glasses = beauty.fx_wrinkle_ignore_glasses;
    state.fx_wrinkle_glasses_margin = beauty.fx_wrinkle_glasses_margin;
    state.fx_wrinkle_keep_ratio = beauty.fx_wrinkle_keep_ratio;
    state.fx_wrinkle_custom_scales = beauty.fx_wrinkle_custom_scales;
    state.fx_wrinkle_min_px = beauty.fx_wrinkle_min_px;
    state.fx_wrinkle_max_px = beauty.fx_wrinkle_max_px;
    state.fx_wrinkle_use_skin_gate = beauty.fx_wrinkle_use_skin_gate;
    state.fx_wrinkle_mask_gain = beauty.fx_wrinkle_mask_gain;
    state.fx_wrinkle_baseline = beauty.fx_wrinkle_baseline;
    state.fx_wrinkle_neg_cap = beauty.fx_wrinkle_neg_cap;
    state.fx_wrinkle_preview = beauty.fx_wrinkle_preview;
    state.fx_adv_scale = beauty.fx_adv_scale;
    state.fx_adv_detail_preserve = beauty.fx_adv_detail_preserve;
    
    // Auto processing scale
    state.auto_processing_scale = beauty.auto_processing_scale;
    state.target_fps = beauty.target_fps;
    state.fx_adv_detail_preserve = beauty.fx_adv_detail_preserve;
    state.fx_lipstick = beauty.fx_lipstick;
    state.fx_lip_alpha = beauty.fx_lip_alpha;
    state.fx_lip_feather = beauty.fx_lip_feather;
    state.fx_lip_light = beauty.fx_lip_light;
    state.fx_lip_band = beauty.fx_lip_band;
    for (int i = 0; i < 3; i++) {
        state.fx_lip_color[i] = beauty.fx_lip_color[i];
    }
    state.fx_teeth = beauty.fx_teeth;
    state.fx_teeth_strength = beauty.fx_teeth_strength;
    state.fx_teeth_margin = beauty.fx_teeth_margin;
    
    // Apply preset using existing function
    ApplyPreset(preset_index, state);
    
    // Copy back to ConfigData
    background.bg_mode = state.bg_mode;
    background.blur_strength = state.blur_strength;
    background.feather_px = state.feather_px;
    display.show_mask = state.show_mask;
    
    beauty.fx_skin = state.fx_skin;
    beauty.fx_skin_adv = state.fx_skin_adv;
    beauty.fx_skin_amount = state.fx_skin_amount;
    beauty.fx_skin_radius = state.fx_skin_radius;
    beauty.fx_skin_tex = state.fx_skin_tex;
    beauty.fx_skin_edge = state.fx_skin_edge;
    beauty.fx_skin_wrinkle = state.fx_skin_wrinkle;
    beauty.fx_skin_smile_boost = state.fx_skin_smile_boost;
    beauty.fx_skin_squint_boost = state.fx_skin_squint_boost;
    beauty.fx_skin_forehead_boost = state.fx_skin_forehead_boost;
    beauty.fx_skin_wrinkle_gain = state.fx_skin_wrinkle_gain;
    beauty.fx_wrinkle_suppress_lower = state.fx_wrinkle_suppress_lower;
    beauty.fx_wrinkle_lower_ratio = state.fx_wrinkle_lower_ratio;
    beauty.fx_wrinkle_ignore_glasses = state.fx_wrinkle_ignore_glasses;
    beauty.fx_wrinkle_glasses_margin = state.fx_wrinkle_glasses_margin;
    beauty.fx_wrinkle_keep_ratio = state.fx_wrinkle_keep_ratio;
    beauty.fx_wrinkle_custom_scales = state.fx_wrinkle_custom_scales;
    beauty.fx_wrinkle_min_px = state.fx_wrinkle_min_px;
    beauty.fx_wrinkle_max_px = state.fx_wrinkle_max_px;
    beauty.fx_wrinkle_use_skin_gate = state.fx_wrinkle_use_skin_gate;
    beauty.fx_wrinkle_mask_gain = state.fx_wrinkle_mask_gain;
    beauty.fx_wrinkle_baseline = state.fx_wrinkle_baseline;
    beauty.fx_wrinkle_neg_cap = state.fx_wrinkle_neg_cap;
    beauty.fx_wrinkle_preview = state.fx_wrinkle_preview;
    beauty.fx_adv_scale = state.fx_adv_scale;
    beauty.fx_adv_detail_preserve = state.fx_adv_detail_preserve;
    
    // Auto processing scale  
    beauty.auto_processing_scale = state.auto_processing_scale;
    beauty.target_fps = state.target_fps;
    beauty.fx_lipstick = state.fx_lipstick;
    beauty.fx_lip_alpha = state.fx_lip_alpha;
    beauty.fx_lip_feather = state.fx_lip_feather;
    beauty.fx_lip_light = state.fx_lip_light;
    beauty.fx_lip_band = state.fx_lip_band;
    for (int i = 0; i < 3; i++) {
        beauty.fx_lip_color[i] = state.fx_lip_color[i];
    }
    beauty.fx_teeth = state.fx_teeth;
    beauty.fx_teeth_strength = state.fx_teeth_strength;
    beauty.fx_teeth_margin = state.fx_teeth_margin;
}

} // namespace segmecam
