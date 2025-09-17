#include "include/ui/ui_panels.h"
#include <iostream>
#include <cstring>
#include <SDL2/SDL.h>

namespace segmecam {

// Background Panel Implementation
BackgroundPanel::BackgroundPanel(AppState& state)
    : UIPanel("Background"), state_(state) {
}

void BackgroundPanel::Render() {
    if (!visible_) return;
    
    if (ImGui::CollapsingHeader("Background", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderMaskControls();
        RenderBackgroundMode();
        
        switch (state_.bg_mode) {
            case 1:
                RenderBlurControls();
                break;
            case 2:
                RenderImageControls();
                break;
            case 3:
                RenderSolidColorControls();
                break;
            default:
                break;
        }
    }
}

void BackgroundPanel::RenderMaskControls() {
    ImGui::Checkbox("Show Segmentation (mask)", &state_.show_mask);
    ImGui::TextDisabled("GPU graph; CPU composite");
}

void BackgroundPanel::RenderBackgroundMode() {
    ImGui::Text("Background Mode");
    
    const char* modes[] = {"None", "Blur", "Image", "Solid Color"};
    ImGui::Combo("Mode", &state_.bg_mode, modes, IM_ARRAYSIZE(modes));
    
    // Alternative radio button layout
    ImGui::Text("Quick Select:");
    ImGui::RadioButton("None", &state_.bg_mode, 0); ImGui::SameLine();
    ImGui::RadioButton("Blur", &state_.bg_mode, 1); ImGui::SameLine();
    ImGui::RadioButton("Image", &state_.bg_mode, 2); ImGui::SameLine();
    ImGui::RadioButton("Color", &state_.bg_mode, 3);
}

void BackgroundPanel::RenderBlurControls() {
    ImGui::Separator();
    ImGui::Text("Blur Settings");
    
    ImGui::SliderInt("Blur Strength", &state_.blur_strength, 1, 61);
    // Ensure odd kernel size for proper blur
    if ((state_.blur_strength % 2) == 0) {
        state_.blur_strength++;
    }
    
    ImGui::SliderFloat("Feather (px)", &state_.feather_px, 0.0f, 20.0f);
    ImGui::SameLine();
    if (ImGui::Button("?")) {
        ImGui::SetTooltip("Edge softening to reduce harsh mask edges");
    }
    
    // Blur quality settings
    static int blur_quality = 1; // 0=Fast, 1=Normal, 2=High
    const char* quality_items[] = {"Fast", "Normal", "High Quality"};
    if (ImGui::Combo("Quality", &blur_quality, quality_items, IM_ARRAYSIZE(quality_items))) {
        std::cout << "Blur quality changed to: " << quality_items[blur_quality] << std::endl;
    }
    
    // Performance tip
    ImGui::Separator();
    ImGui::TextDisabled("Performance tips:");
    ImGui::TextDisabled("• Lower blur strength = faster processing");
    ImGui::TextDisabled("• Fast quality for real-time use");
    ImGui::TextDisabled("• High quality for recordings");
    
    if (state_.blur_strength > 30) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "⚠ High blur may impact performance");
    }
}

void BackgroundPanel::RenderImageControls() {
    ImGui::Separator();
    ImGui::Text("Image Background");
    
    ImGui::InputText("Image Path", state_.bg_path_buf, sizeof(state_.bg_path_buf));
    
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        cv::Mat img = cv::imread(state_.bg_path_buf, cv::IMREAD_COLOR);
        if (!img.empty()) {
            state_.bg_image = img;
            std::cout << "Loaded background image: " << state_.bg_path_buf 
                      << " (" << img.cols << "x" << img.rows << ")" << std::endl;
        } else {
            std::cerr << "Failed to load background image: " << state_.bg_path_buf << std::endl;
        }
    }
    
    // Paste from clipboard
    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
        char* clip = SDL_GetClipboardText();
        if (clip && *clip) {
            std::snprintf(state_.bg_path_buf, sizeof(state_.bg_path_buf), "%s", clip);
            cv::Mat img = cv::imread(state_.bg_path_buf, cv::IMREAD_COLOR);
            if (!img.empty()) {
                state_.bg_image = img;
                std::cout << "Loaded background image from clipboard: " << state_.bg_path_buf 
                          << " (" << img.cols << "x" << img.rows << ")" << std::endl;
            }
        }
        if (clip) SDL_free(clip);
    }
    
    // File browser button (Linux specific)
#ifdef __linux__
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
        // Use zenity for file selection on Linux
        FILE* fp = popen("zenity --file-selection --file-filter='Images (*.jpg,*.jpeg,*.png,*.bmp) | *.jpg *.jpeg *.png *.bmp' --title='Select background image' 2>/dev/null", "r");
        if (fp) {
            char out[1024] = {0};
            if (fgets(out, sizeof(out), fp)) {
                // Strip trailing newline
                size_t n = strlen(out);
                while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r')) {
                    out[--n] = '\0';
                }
                if (n > 0) {
                    std::snprintf(state_.bg_path_buf, sizeof(state_.bg_path_buf), "%s", out);
                    cv::Mat img = cv::imread(state_.bg_path_buf, cv::IMREAD_COLOR);
                    if (!img.empty()) {
                        state_.bg_image = img;
                        std::cout << "Loaded background image: " << state_.bg_path_buf 
                                  << " (" << img.cols << "x" << img.rows << ")" << std::endl;
                    }
                }
            }
            pclose(fp);
        }
    }
#endif
    
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        state_.bg_image.release();
        state_.bg_path_buf[0] = '\0';
        std::cout << "Cleared background image" << std::endl;
    }
    
    // Display current image info and scaling options
    if (!state_.bg_image.empty()) {
        ImGui::Separator();
        ImGui::Text("Current Image: %dx%d", state_.bg_image.cols, state_.bg_image.rows);
        
        // Image scaling mode
        const char* scale_modes[] = {"Stretch", "Fit", "Fill", "Center", "Tile"};
        static int scale_mode = 1; // Default to "Fit"
        if (ImGui::Combo("Scaling", &scale_mode, scale_modes, IM_ARRAYSIZE(scale_modes))) {
            std::cout << "Background scaling changed to: " << scale_modes[scale_mode] << std::endl;
        }
        
        // Image opacity
        static float bg_opacity = 1.0f;
        if (ImGui::SliderFloat("Opacity", &bg_opacity, 0.0f, 1.0f)) {
            // TODO: Apply opacity to background image compositing
        }
        
        // Image position adjustment (for center mode)
        if (scale_mode == 3) { // Center mode
            static float offset_x = 0.0f;
            static float offset_y = 0.0f;
            ImGui::SliderFloat("Offset X", &offset_x, -1.0f, 1.0f);
            ImGui::SliderFloat("Offset Y", &offset_y, -1.0f, 1.0f);
        }
        
        if (ImGui::Button("Reset Position")) {
            // Reset any position offsets
        }
    } else {
        ImGui::TextDisabled("No image loaded");
    }
    
    ImGui::Separator();
    ImGui::TextDisabled("Tips:");
    ImGui::TextDisabled("• Drag & drop an image file onto the window");
    ImGui::TextDisabled("• Copy image path to clipboard and use Paste");
    ImGui::TextDisabled("• Supports JPG, PNG, BMP formats");
}

void BackgroundPanel::RenderSolidColorControls() {
    ImGui::Separator();
    ImGui::Text("Solid Color Background");
    
    ImGui::ColorEdit3("Color", state_.solid_color);
    
    // Color presets
    ImGui::Text("Presets:");
    
    if (ImGui::Button("Black")) {
        state_.solid_color[0] = 0.0f;
        state_.solid_color[1] = 0.0f;
        state_.solid_color[2] = 0.0f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("White")) {
        state_.solid_color[0] = 1.0f;
        state_.solid_color[1] = 1.0f;
        state_.solid_color[2] = 1.0f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Green")) {
        state_.solid_color[0] = 0.0f;
        state_.solid_color[1] = 1.0f;
        state_.solid_color[2] = 0.0f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Blue")) {
        state_.solid_color[0] = 0.0f;
        state_.solid_color[1] = 0.0f;
        state_.solid_color[2] = 1.0f;
    }
}

} // namespace segmecam