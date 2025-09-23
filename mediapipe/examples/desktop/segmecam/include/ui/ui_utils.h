#pragma once

#include "src/config/config_manager.h"
#include <string>

namespace segmecam {
namespace ui_utils {

// Common profile selection UI component
// Returns true if a profile was loaded
bool RenderProfileSelection(class ConfigManager* config_mgr, int& ui_profile_idx,
                           char* profile_name_buf, size_t buf_size,
                           const std::string& input_label,
                           bool show_set_default = false);

} // namespace ui_utils
} // namespace segmecam