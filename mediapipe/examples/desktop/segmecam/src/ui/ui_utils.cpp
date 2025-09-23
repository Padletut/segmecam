#include "include/ui/ui_utils.h"
#include "include/ui/ui_panels.h"
#include "src/config/config_manager.h"
#include <cstring>
#include <algorithm>
#include <iostream>

namespace segmecam {
namespace ui_utils {

// Common profile selection UI component
// Returns true if a profile was loaded
bool RenderProfileSelection(class ConfigManager* config_mgr, int& ui_profile_idx,
                           char* profile_name_buf, size_t buf_size,
                           const std::string& input_label,
                           bool show_set_default) {
    bool profile_loaded = false;

    // List existing profiles (if ConfigManager is available)
    if (config_mgr) {
        auto profile_names = config_mgr->ListProfiles();
        if (profile_names.empty()) {
            ImGui::TextDisabled("No profiles yet");
        } else {
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

            if (ImGui::Button("Load##prof") && ui_profile_idx >= 0) {
                profile_loaded = true;
            }
        }

        ImGui::InputText(input_label.c_str(), profile_name_buf, buf_size);

        if (ImGui::Button("Save##prof")) {
            if (strnlen(profile_name_buf, buf_size) > 0) {
                // This will be handled by the caller since they need access to SaveStateToProfile
                std::cout << "Profile save requested: " << profile_name_buf << std::endl;
            }
        }

        if (show_set_default) {
            ImGui::SameLine();
            if (ImGui::Button("Set Default##prof") && strnlen(profile_name_buf, buf_size) > 0) {
                config_mgr->SetDefaultProfile(profile_name_buf);
                std::cout << "Set default profile: " << profile_name_buf << std::endl;
            }
        }
    } else {
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
    }

    return profile_loaded;
}

} // namespace ui_utils
} // namespace segmecam