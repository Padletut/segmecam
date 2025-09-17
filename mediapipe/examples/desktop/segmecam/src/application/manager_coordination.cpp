#include "include/application/manager_coordination.h"
#include "include/application/application_run.h"
#include "src/config/config_manager.h"
#include "include/camera/camera_manager.h"
#include "include/effects/effects_manager.h"
#include "app_state.h"
#include <iostream>
#include <cstring>

bool ManagerCoordination::SetupManagers(Managers& managers, segmecam::AppState& app_state) {
    std::cout << "Initializing essential managers..." << std::endl;
    
    // Initialize config manager
    if (!InitializeConfigManager(managers, app_state)) {
        std::cerr << "Error: Failed to initialize ConfigManager" << std::endl;
        return false;
    }
    
    // Initialize camera manager
    if (!InitializeCameraManager(managers, app_state)) {
        std::cerr << "Error: Failed to initialize CameraManager" << std::endl;
        return false;
    }
    
    // Initialize effects manager
    if (!InitializeEffectsManager(managers, app_state)) {
        std::cerr << "Error: Failed to initialize EffectsManager" << std::endl;
        return false;
    }
    
    std::cout << "Essential managers initialized successfully" << std::endl;
    return true;
}

void ManagerCoordination::ShutdownManagers(Managers& managers) {
    std::cout << "Shutting down managers..." << std::endl;
    
    // Shutdown in reverse order to handle dependencies
    if (managers.effects) {
        managers.effects->Cleanup();
        managers.effects.reset();
    }
    
    if (managers.camera) {
        managers.camera->Cleanup();
        managers.camera.reset();
    }
    
    managers.config.reset();
    
    std::cout << "All managers shut down" << std::endl;
}

bool ManagerCoordination::ValidateManagers(const Managers& managers) {
    if (!managers.config) {
        std::cerr << "ConfigManager not initialized" << std::endl;
        return false;
    }
    
    if (!managers.camera) {
        std::cerr << "CameraManager not initialized" << std::endl;
        return false;
    }
    
    if (!managers.effects) {
        std::cerr << "EffectsManager not initialized" << std::endl;
        return false;
    }
    
    return true;
}

bool ManagerCoordination::InitializeConfigManager(Managers& managers, segmecam::AppState& app_state) {
    try {
        managers.config = std::make_unique<segmecam::ConfigManager>();
        
        // ConfigManager doesn't have an Initialize method, it's ready to use
        std::cout << "ConfigManager created successfully" << std::endl;
        
        // Try to get default profile
        std::string default_profile;
        if (managers.config->GetDefaultProfile(default_profile) && !default_profile.empty()) {
            std::cout << "Loading default profile: " << default_profile << std::endl;
            // Load the default profile using ConfigData and apply basic settings
            segmecam::ConfigData config_data;
            if (managers.config->LoadProfile(default_profile, config_data)) {
                // Apply basic display settings from the profile
                app_state.vsync_on = config_data.display.vsync_on;
                app_state.show_mask = config_data.display.show_mask;
                app_state.show_landmarks = config_data.display.show_landmarks;
                app_state.bg_mode = config_data.background.bg_mode;
                app_state.blur_strength = config_data.background.blur_strength;
                // Apply background path from profile to preserve it for future saves
                strncpy(app_state.bg_path_buf, config_data.background.bg_path.c_str(), sizeof(app_state.bg_path_buf) - 1);
                app_state.bg_path_buf[sizeof(app_state.bg_path_buf) - 1] = '\0';
                
                // Apply beauty effects settings from profile
                app_state.fx_skin = config_data.beauty.fx_skin;
                app_state.fx_skin_adv = config_data.beauty.fx_skin_adv;
                app_state.fx_skin_strength = config_data.beauty.fx_skin_strength;
                app_state.fx_skin_amount = config_data.beauty.fx_skin_amount;
                app_state.fx_skin_radius = config_data.beauty.fx_skin_radius;
                app_state.fx_skin_tex = config_data.beauty.fx_skin_tex;
                app_state.fx_skin_edge = config_data.beauty.fx_skin_edge;
                app_state.fx_adv_scale = config_data.beauty.fx_adv_scale;
                app_state.fx_adv_detail_preserve = config_data.beauty.fx_adv_detail_preserve;
                
                // Wrinkle settings
                app_state.fx_skin_wrinkle = config_data.beauty.fx_skin_wrinkle;
                app_state.fx_skin_smile_boost = config_data.beauty.fx_skin_smile_boost;
                app_state.fx_skin_squint_boost = config_data.beauty.fx_skin_squint_boost;
                app_state.fx_skin_forehead_boost = config_data.beauty.fx_skin_forehead_boost;
                app_state.fx_skin_wrinkle_gain = config_data.beauty.fx_skin_wrinkle_gain;
                app_state.fx_wrinkle_suppress_lower = config_data.beauty.fx_wrinkle_suppress_lower;
                app_state.fx_wrinkle_lower_ratio = config_data.beauty.fx_wrinkle_lower_ratio;
                app_state.fx_wrinkle_ignore_glasses = config_data.beauty.fx_wrinkle_ignore_glasses;
                app_state.fx_wrinkle_glasses_margin = config_data.beauty.fx_wrinkle_glasses_margin;
                app_state.fx_wrinkle_keep_ratio = config_data.beauty.fx_wrinkle_keep_ratio;
                app_state.fx_wrinkle_custom_scales = config_data.beauty.fx_wrinkle_custom_scales;
                app_state.fx_wrinkle_min_px = config_data.beauty.fx_wrinkle_min_px;
                app_state.fx_wrinkle_max_px = config_data.beauty.fx_wrinkle_max_px;
                app_state.fx_wrinkle_use_skin_gate = config_data.beauty.fx_wrinkle_use_skin_gate;
                app_state.fx_wrinkle_mask_gain = config_data.beauty.fx_wrinkle_mask_gain;
                app_state.fx_wrinkle_baseline = config_data.beauty.fx_wrinkle_baseline;
                app_state.fx_wrinkle_neg_cap = config_data.beauty.fx_wrinkle_neg_cap;
                app_state.fx_wrinkle_preview = config_data.beauty.fx_wrinkle_preview;
                
                // Lip effects
                app_state.fx_lipstick = config_data.beauty.fx_lipstick;
                app_state.fx_lip_alpha = config_data.beauty.fx_lip_alpha;
                app_state.fx_lip_feather = config_data.beauty.fx_lip_feather;
                app_state.fx_lip_light = config_data.beauty.fx_lip_light;
                app_state.fx_lip_band = config_data.beauty.fx_lip_band;
                app_state.fx_lip_color[0] = config_data.beauty.fx_lip_color[0];
                app_state.fx_lip_color[1] = config_data.beauty.fx_lip_color[1];
                app_state.fx_lip_color[2] = config_data.beauty.fx_lip_color[2];
                
                // Teeth whitening
                app_state.fx_teeth = config_data.beauty.fx_teeth;
                app_state.fx_teeth_strength = config_data.beauty.fx_teeth_strength;
                app_state.fx_teeth_margin = config_data.beauty.fx_teeth_margin;
                
                // Performance settings
                app_state.use_opencl = config_data.performance.use_opencl;
                
                // Debug settings (currently none)
                
                // Store camera settings from profile in app_state for use during camera initialization
                // These will be used by InitializeCameraManager if valid
                if (config_data.camera.res_w > 0 && config_data.camera.res_h > 0) {
                    app_state.camera_width = config_data.camera.res_w;
                    app_state.camera_height = config_data.camera.res_h;
                }
                if (config_data.camera.fps_value > 0) {
                    app_state.camera_fps = config_data.camera.fps_value;
                }
                
                // Note: Full beauty effects will be loaded by UI panels
                std::cout << "Default profile loaded successfully: " << default_profile << std::endl;
            } else {
                std::cout << "Failed to load default profile: " << default_profile << std::endl;
            }
        } else {
            std::cout << "No default profile found" << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception initializing ConfigManager: " << e.what() << std::endl;
        return false;
    }
}

bool ManagerCoordination::InitializeCameraManager(Managers& managers, segmecam::AppState& app_state) {
    try {
        managers.camera = std::make_unique<segmecam::CameraManager>();
        
        // Use camera settings from loaded profile if available, otherwise use defaults
        segmecam::CameraConfig camera_config;
        camera_config.default_camera_index = 0;  // Use camera 0 like original
        
        // Check if profile provided specific camera settings
        if (app_state.camera_width > 0 && app_state.camera_height > 0) {
            camera_config.default_width = app_state.camera_width;
            camera_config.default_height = app_state.camera_height;
            std::cout << "Using camera resolution from profile: " << app_state.camera_width << "x" << app_state.camera_height << std::endl;
        } else {
            camera_config.default_width = 1280;      // Match original default width
            camera_config.default_height = 720;      // Match original default height
        }
        
        if (app_state.camera_fps > 0) {
            camera_config.default_fps = app_state.camera_fps;
            std::cout << "Using camera FPS from profile: " << app_state.camera_fps << std::endl;
        } else {
            camera_config.default_fps = 30;          // Standard FPS
        }
        
        int result = managers.camera->Initialize(camera_config);
        if (result != 0) {
            std::cerr << "CameraManager initialization failed with code: " << result << std::endl;
            return false;
        }
        
        std::cout << "CameraManager initialized successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception initializing CameraManager: " << e.what() << std::endl;
        return false;
    }
}

bool ManagerCoordination::InitializeEffectsManager(Managers& managers, segmecam::AppState& app_state) {
    try {
        managers.effects = std::make_unique<segmecam::EffectsManager>();
        
        // Initialize effects with default configuration
        segmecam::EffectsConfig effects_config;
        effects_config.enable_opencl = true;  // Enable OpenCL by default if available
        effects_config.enable_face_effects = true;
        effects_config.enable_background_effects = true;
        effects_config.default_processing_scale = 0.8f; // Match app_state default
        effects_config.enable_performance_logging = false;
        
        int result = managers.effects->Initialize(effects_config);
        if (result != 0) {
            std::cerr << "EffectsManager initialization failed with code: " << result << std::endl;
            return false;
        }
        
        // Sync profile settings to EffectsManager after initialization
        segmecam::ApplicationRun::SyncSettingsToEffectsManager(*managers.effects, app_state);
        
        // Sync status from EffectsManager back to app_state (e.g., OpenCL availability)
        segmecam::ApplicationRun::SyncStatusFromEffectsManager(*managers.effects, app_state);
        
        std::cout << "EffectsManager initialized successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception initializing EffectsManager: " << e.what() << std::endl;
        return false;
    }
}

void ManagerCoordination::LoadDefaultProfileBackgroundImage(Managers& managers, segmecam::AppState& app_state) {
    if (!managers.config || !managers.effects) {
        std::cout << "Config or Effects manager not available for default profile background loading" << std::endl;
        return;
    }
    
    // Check if there's a default profile set
    std::string default_profile;
    if (!managers.config->GetDefaultProfile(default_profile) || default_profile.empty()) {
        std::cout << "No default profile set for background image loading" << std::endl;
        return;
    }
    
    // Load the default profile configuration
    segmecam::ConfigData config_data;
    if (!managers.config->LoadProfile(default_profile, config_data)) {
        std::cout << "Failed to load default profile for background image: " << default_profile << std::endl;
        return;
    }
    
    // Load background image if path is provided and background mode is image
    if (!config_data.background.bg_path.empty() && config_data.background.bg_mode == 2) { // 2 = background image mode
        std::cout << "Loading default profile background image: " << config_data.background.bg_path << std::endl;
        managers.effects->SetBackgroundImageFromPath(config_data.background.bg_path);
    } else {
        std::cout << "Default profile has no background image to load (mode: " << config_data.background.bg_mode << ", path: '" << config_data.background.bg_path << "')" << std::endl;
    }
}