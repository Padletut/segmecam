#include "include/ui/ui_utils.h"
#include "include/ui/ui_panels.h"
#include "src/config/config_manager.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <functional>

namespace segmecam {
namespace ui_utils {

// Safe string copy helper that ensures null termination
void SafeStringCopy(char* dest, size_t dest_size, const std::string& src) {
    if (dest_size == 0) return;
    
    size_t copy_len = std::min(src.length(), dest_size - 1);
    std::memcpy(dest, src.c_str(), copy_len);
    dest[copy_len] = '\0';
}

// Common profile selection UI component
// Returns true if a profile was loaded
bool RenderProfileSelection(class ConfigManager* config_mgr, int& ui_profile_idx,
                           char* profile_name_buf, size_t buf_size,
                           const std::string& input_label,
                           bool show_set_default) {
    if (!config_mgr) {
        return RenderProfileSelectionDisabled(input_label, profile_name_buf, buf_size, show_set_default);
    }
    
    return RenderProfileSelectionEnabled(config_mgr, ui_profile_idx, profile_name_buf, buf_size, input_label, show_set_default);
}

bool RenderProfileSelectionEnabled(class ConfigManager* config_mgr, int& ui_profile_idx,
                                  char* profile_name_buf, size_t buf_size,
                                  const std::string& input_label, bool show_set_default) {
    bool profile_loaded = false;
    
    auto profile_names = config_mgr->ListProfiles();
    if (profile_names.empty()) {
        ImGui::TextDisabled("No profiles yet");
    } else {
        profile_loaded = RenderProfileCombo(profile_names, ui_profile_idx);
    }
    
    RenderProfileInputAndButtons(config_mgr, profile_name_buf, buf_size, input_label, show_set_default);
    
    return profile_loaded;
}

bool RenderProfileCombo(const std::vector<std::string>& profile_names, int& ui_profile_idx) {
    std::vector<const char*> items;
    for (const auto& name : profile_names) {
        items.push_back(name.c_str());
    }
    
    // Ensure ui_profile_idx is valid
    if (ui_profile_idx < 0 || ui_profile_idx >= (int)profile_names.size()) {
        ui_profile_idx = 0;
    }
    
    ImGui::Combo("Select", &ui_profile_idx, items.data(), (int)items.size());
    ImGui::SameLine();
    
    return ImGui::Button("Load##prof") && ui_profile_idx >= 0;
}

void RenderProfileInputAndButtons(class ConfigManager* config_mgr, char* profile_name_buf, 
                                 size_t buf_size, const std::string& input_label, bool show_set_default) {
    ImGui::InputText(input_label.c_str(), profile_name_buf, buf_size);
    
    if (ImGui::Button("Save##prof")) {
        if (strnlen(profile_name_buf, buf_size) > 0) {
            std::cout << "Profile save requested: " << profile_name_buf << std::endl;
        }
    }
    
    if (show_set_default) {
        ImGui::SameLine();
        RenderSetDefaultButton(config_mgr, profile_name_buf, buf_size);
    }
}

void RenderSetDefaultButton(class ConfigManager* config_mgr, char* profile_name_buf, size_t buf_size) {
    if (ImGui::Button("Set Default##prof") && strnlen(profile_name_buf, buf_size) > 0) {
        config_mgr->SetDefaultProfile(profile_name_buf);
        std::cout << "Set default profile: " << profile_name_buf << std::endl;
    }
}

bool RenderProfileSelectionDisabled(const std::string& input_label, char* profile_name_buf, 
                                   size_t buf_size, bool show_set_default) {
    ImGui::TextDisabled("Profile system not available");
    ImGui::InputText((input_label + "##disabled").c_str(), profile_name_buf, buf_size);
    
    if (ImGui::Button("Save##prof")) {
        ImGui::TextDisabled("Config manager not initialized");
    }
    
    if (show_set_default) {
        ImGui::SameLine();
        if (ImGui::Button("Set Default##prof")) {
            ImGui::TextDisabled("Config manager not initialized");
        }
    }
    
    return false;
}

bool HandleProfileSaveButton(class ConfigManager* config_mgr, int& ui_profile_idx,
                            char* profile_name_buf, size_t buf_size,
                            std::function<bool(const std::string&)> save_callback) {
    if (!config_mgr || !ImGui::Button("Save##prof")) {
        return false;
    }
    
    if (strnlen(profile_name_buf, buf_size) == 0) {
        return false;
    }
    
    if (!save_callback(profile_name_buf)) {
        return false;
    }
    
    std::cout << "Profile saved: " << profile_name_buf << std::endl;
    
    // Update profile index after successful save
    auto profile_names = config_mgr->ListProfiles();
    auto it = std::find(profile_names.begin(), profile_names.end(), profile_name_buf);
    ui_profile_idx = (it == profile_names.end()) ? -1 : (int)std::distance(profile_names.begin(), it);
    
    return true;
}

} // namespace ui_utils
} // namespace segmecam