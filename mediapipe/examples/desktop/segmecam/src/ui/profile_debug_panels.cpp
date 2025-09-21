#include "include/ui/ui_panels.h"
#include "include/camera/camera_manager.h"
#include "src/config/config_manager.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace segmecam {

// Profile Panel Implementation
ProfilePanel::ProfilePanel(AppState& state, CameraManager& camera_mgr)
    : UIPanel("Profiles"), state_(state), camera_mgr_(camera_mgr) {
}

void ProfilePanel::Render() {
    if (!visible_) return;
    
    ImGui::Text("Profile");
    
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
        
        ImGui::InputText("Name", profile_name_buf_, sizeof(profile_name_buf_));
        
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
        ImGui::InputText("Name", profile_name_buf_, sizeof(profile_name_buf_));
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

void ProfilePanel::RenderProfileList() {
    ImGui::Text("Available Profiles");
    ImGui::Separator();
    
    // Simple static profile list for now
    static std::vector<std::string> profiles = {"Default", "Beauty", "Natural", "Stream"};
    
    for (int i = 0; i < (int)profiles.size(); i++) {
        if (ImGui::Selectable(profiles[i].c_str())) {
            LoadProfileIntoState(profiles[i]);
        }
    }
}

void ProfilePanel::RenderProfileCreation() {
    ImGui::Text("Create New Profile");
    ImGui::Separator();
    
    ImGui::InputText("Profile Name", profile_name_buf_, sizeof(profile_name_buf_));
    
    if (ImGui::Button("Save Current Settings")) {
        if (strlen(profile_name_buf_) > 0) {
            if (SaveStateToProfile(profile_name_buf_)) {
                std::cout << "Profile saved: " << profile_name_buf_ << std::endl;
                profile_name_buf_[0] = '\0';
            }
        }
    }
}

void ProfilePanel::RenderProfileActions() {
    ImGui::Text("Profile Actions");
    ImGui::Separator();
    
    if (ImGui::Button("Load Default")) {
        LoadProfileIntoState("Default");
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset All")) {
        // Reset to defaults
        state_.fx_skin = false;
        state_.fx_skin_strength = 0.4f;
        state_.fx_lipstick = false;
        state_.fx_teeth = false;
    }
    
    ImGui::TextDisabled("Profile system is currently simplified");
    ImGui::TextDisabled("Full YAML persistence will be integrated later");
}

void ProfilePanel::LoadProfileIntoState(const std::string& profile_name) {
    std::cout << "Loading profile: " << profile_name << std::endl;
    
    // Simple preset loading for now
    if (profile_name == "Natural") {
        state_.fx_skin = true;
        state_.fx_skin_strength = 0.3f;
        state_.fx_skin_wrinkle = true;
    } else if (profile_name == "Beauty") {
        state_.fx_skin = true;
        state_.fx_skin_strength = 0.6f;
        state_.fx_skin_wrinkle = true;
        state_.fx_lipstick = true;
        state_.fx_lip_alpha = 0.3f;
    } else if (profile_name == "Stream") {
        state_.bg_mode = 1; // Blur
        state_.fx_skin = true;
        state_.fx_skin_strength = 0.4f;
    }
    
    last_loaded_profile_ = profile_name;
}

bool ProfilePanel::SaveStateToProfile(const std::string& profile_name) {
    std::cout << "Saving profile: " << profile_name << std::endl;
    
    // TODO: Implement real YAML persistence with ConfigManager
    if (config_mgr_) {
        // Future: Use config_mgr_->SaveProfile(profile_name, state_to_config(state_));
    }
    
    return true;
}

// Debug Panel Implementation
DebugPanel::DebugPanel(AppState& state)
    : UIPanel("Debug"), state_(state) {
}

void DebugPanel::Render() {
    if (!visible_) return;
    
    if (ImGui::CollapsingHeader("Debug Controls")) {
        RenderOverlayControls();
        RenderDebugVisualization();
        RenderPerformanceStats();
        RenderAdvancedSettings();
    }
}

void DebugPanel::RenderOverlayControls() {
    ImGui::Text("Debug Overlays");
    ImGui::Separator();
    
    ImGui::Checkbox("Show Face Landmarks", &state_.show_landmarks);
    ImGui::Checkbox("Show Segmentation Mask", &state_.show_mask);
    ImGui::Checkbox("Show Face Mesh", &state_.show_mesh);
}

void DebugPanel::RenderDebugVisualization() {
    ImGui::Text("Debug Visualization");
    ImGui::Separator();
    
    ImGui::Checkbox("Composite RGB debug", &state_.dbg_composite_rgb);
}

void DebugPanel::RenderPerformanceStats() {
    ImGui::Text("Performance Statistics");
    ImGui::Separator();
    
    ImGui::Text("FPS: %.1f", state_.fps);
    ImGui::Text("Frame ID: %lld", (long long)state_.frame_id);
    
    if (state_.perf_log && state_.perf_sum_frames > 0) {
        ImGui::Text("Avg Frame Time: %.2f ms", state_.perf_sum_frame_ms / state_.perf_sum_frames);
        ImGui::Text("Avg Smooth Time: %.2f ms", state_.perf_sum_smooth_ms / state_.perf_sum_frames);
        ImGui::Text("Avg Background Time: %.2f ms", state_.perf_sum_bg_ms / state_.perf_sum_frames);
    }
    
    ImGui::Spacing();
    ImGui::Text("Performance Optimization");
    ImGui::Separator();
    
    // Manual processing scale
    ImGui::SliderFloat("Processing scale", &state_.fx_adv_scale, 0.4f, 1.0f);
    ImGui::TextDisabled("Reduces image size for faster processing");
    
    // Detail preserve (only shown when processing scale is reduced)
    if (state_.fx_adv_scale < 0.999f) {
        ImGui::SliderFloat("Detail preserve", &state_.fx_adv_detail_preserve, 0.0f, 0.5f);
        ImGui::TextDisabled("Preserves fine details when processing at reduced scale");
    }
    
    // Auto processing scale
    ImGui::Checkbox("Auto processing scale", &state_.auto_processing_scale);
    if (state_.auto_processing_scale) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ May cause shaky appearance");
    }
    ImGui::TextDisabled("Automatically adjusts scale to maintain target FPS");
    
    if (state_.auto_processing_scale) {
        ImGui::Indent();
        
        // Target FPS (read-only, auto-calculated)
        ImGui::Text("Target: %.1f fps", state_.target_fps);
        ImGui::SameLine();
        ImGui::TextDisabled("(auto-detected from camera)");
        
        // Show current status with better formatting
        ImGui::Text("Current: %.1f fps, Scale: %.2f", state_.current_fps, state_.fx_adv_scale);
        if (state_.current_fps > 0.0f) {
            float fps_diff = state_.target_fps - state_.current_fps;
            if (std::abs(fps_diff) > 0.5f) {
                ImGui::SameLine();
                if (fps_diff > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(slow)");
                } else {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(fast)");
                }
            } else {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(optimal)");
            }
        }
        ImGui::Unindent();
    }
}

void DebugPanel::RenderAdvancedSettings() {
    ImGui::Text("Advanced Settings");
    ImGui::Separator();
    
    // OpenCL status (read-only) - not an editable checkbox!
    if (state_.opencl_available) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ OpenCL: Available");
    } else {
        ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "⚠️  OpenCL: Not Available");
    }
    
    ImGui::Checkbox("Performance Logging", &state_.perf_log);
    ImGui::Checkbox("VSync", &state_.vsync_on);
}

// Status Panel Implementation
StatusPanel::StatusPanel(AppState& state)
    : UIPanel("Status"), state_(state) {
}

void StatusPanel::Render() {
    if (!visible_) return;
    
    // Status content - rendered by UIManager overlay, not as a separate window
    RenderFPSInfo();
    RenderSystemInfo();
    RenderGraphInfo();
}

void StatusPanel::RenderFPSInfo() {
    ImGui::Text("FPS: %.1f", state_.fps);
    if (state_.camera_width > 0 && state_.camera_height > 0) {
        ImGui::Text("Cam: %dx%d@%d", state_.camera_width, state_.camera_height, state_.camera_fps);
    } else {
        ImGui::Text("Cam: Not initialized");
    }
    if (!state_.camera_status_message.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.2f, 1.0f), "%s", state_.camera_status_message.c_str());
    }
}

void StatusPanel::RenderSystemInfo() {
    if (state_.opencl_available) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "OpenCL: Available");
    } else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "OpenCL: Unavailable");
    }
    
    if (state_.vcam.IsOpen()) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "VCam: Active");
    } else {
        ImGui::Text("VCam: Inactive");
    }
}

void StatusPanel::RenderGraphInfo() {
    // Removed App: Running status as it's not very useful
    // The fact that the UI is updating indicates the app is running
}

} // namespace segmecam
