#pragma once

#include "src/config/config_manager.h"
#include <string>
#include <vector>
#include <cstring>

namespace segmecam {
namespace ui_utils {

// Safe string copy helper that ensures null termination
void SafeStringCopy(char* dest, size_t dest_size, const std::string& src);

// Common profile selection UI component
// Returns true if a profile was loaded
bool RenderProfileSelection(class ConfigManager* config_mgr, int& ui_profile_idx,
                           char* profile_name_buf, size_t buf_size,
                           const std::string& input_label,
                           bool show_set_default = false);

// Helper functions for profile selection
bool RenderProfileSelectionEnabled(class ConfigManager* config_mgr, int& ui_profile_idx,
                                  char* profile_name_buf, size_t buf_size,
                                  const std::string& input_label, bool show_set_default);

bool RenderProfileCombo(const std::vector<std::string>& profile_names, int& ui_profile_idx);

void RenderProfileInputAndButtons(class ConfigManager* config_mgr, char* profile_name_buf, 
                                 size_t buf_size, const std::string& input_label, bool show_set_default);

void RenderSetDefaultButton(class ConfigManager* config_mgr, char* profile_name_buf, size_t buf_size);

bool RenderProfileSelectionDisabled(const std::string& input_label, char* profile_name_buf, 
                                   size_t buf_size, bool show_set_default);

// Common profile save button handler
// Returns true if a profile was saved
bool HandleProfileSaveButton(class ConfigManager* config_mgr, int& ui_profile_idx,
                            char* profile_name_buf, size_t buf_size,
                            std::function<bool(const std::string&)> save_callback);

} // namespace ui_utils
} // namespace segmecam