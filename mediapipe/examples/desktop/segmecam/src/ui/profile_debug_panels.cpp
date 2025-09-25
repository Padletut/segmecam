#include "include/ui/ui_panels.h"
#include <iostream>

namespace segmecam {

namespace {
} // anonymous namespace

// Debug Panel Implementation
DebugPanel::DebugPanel(AppState& state)
    : UIPanel("Debug"), state_(state) {
}

void DebugPanel::Render() {
    if (!visible_) return;
    
    if (ImGui::CollapsingHeader("Debug Controls")) {
        RenderOverlayControls();
        RenderPerformanceStats();
        RenderAdvancedSettings();
    }
}

void DebugPanel::RenderOverlayControls() {
    ImGui::Text("Debug Overlays");
    ImGui::Separator();
    
    ImGui::Checkbox("Show Face Landmarks", &state_.show_landmarks);
    ImGui::Checkbox("Show Segmentation Mask", &state_.show_mask);
    
    // Mesh visualization
    ImGui::Checkbox("Show Face Mesh", &state_.show_mesh);
    if (state_.show_mesh) {
        ImGui::SameLine();
        ImGui::Checkbox("Dense", &state_.show_mesh_dense);
        ImGui::SameLine();
        if (ImGui::Button("?##mesh_dense")) {
            ImGui::SetTooltip("Show all 468 landmarks vs reduced set");
        }
    }
}


void DebugPanel::RenderPerformanceStats() {
    RenderBasicStats();
    RenderPerformanceOptimization();
}

void DebugPanel::RenderBasicStats() {
    ImGui::Text("Performance Statistics");
    ImGui::Separator();
    
    ImGui::Text("FPS: %.1f", state_.fps);
    ImGui::Text("Frame ID: %lld", (long long)state_.frame_id);
}

void DebugPanel::RenderPerformanceOptimization() {
    ImGui::Spacing();
    ImGui::Text("Performance Optimization");
    ImGui::Separator();
    
    RenderManualProcessingScale();
    RenderAutoProcessingScale();
}

void DebugPanel::RenderManualProcessingScale() {
    // Manual processing scale
    ImGui::SliderFloat("Processing scale", &state_.fx_adv_scale, 0.4f, 1.0f);
    ImGui::TextDisabled("Reduces image size for faster processing");
    
    // Detail preserve (only shown when processing scale is reduced)
    if (state_.fx_adv_scale < 0.999f) {
        ImGui::SliderFloat("Detail preserve", &state_.fx_adv_detail_preserve, 0.0f, 0.5f);
        ImGui::TextDisabled("Preserves fine details when processing at reduced scale");
    }
}

void DebugPanel::RenderAutoProcessingScale() {
    // Auto processing scale
    ImGui::Checkbox("Auto processing scale", &state_.auto_processing_scale);
    if (state_.auto_processing_scale) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ May cause shaky appearance");
    }
    ImGui::TextDisabled("Automatically adjusts scale to maintain target FPS");
    
    if (state_.auto_processing_scale) {
        RenderAutoProcessingScaleDetails();
    }
}

void DebugPanel::RenderAutoProcessingScaleDetails() {
    ImGui::Indent();
    
    // Target FPS (read-only, auto-calculated)
    ImGui::Text("Target: %.1f fps", state_.target_fps);
    ImGui::SameLine();
    ImGui::TextDisabled("(auto-detected from camera)");
    
    // Show current status with better formatting
    ImGui::Text("Current: %.1f fps, Scale: %.2f", state_.current_fps, state_.fx_adv_scale);
    if (state_.current_fps > 0.0f) {
        RenderPerformanceStatus();
    }
    ImGui::Unindent();
}

void DebugPanel::RenderPerformanceStatus() {
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

void DebugPanel::RenderAdvancedSettings() {
    ImGui::Text("Advanced Settings");
    ImGui::Separator();
    
    // OpenCL status (read-only) - not an editable checkbox!
    if (state_.opencl_available) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ OpenCL: Available");
    } else {
        ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "⚠️  OpenCL: Not Available");
    }
    
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
    
    if (state_.pipewire_output_active) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "PipeWire: Active");
    } else {
        ImGui::Text("PipeWire: Inactive");
    }
}

void StatusPanel::RenderGraphInfo() {
    // Removed App: Running status as it's not very useful
    // The fact that the UI is updating indicates the app is running
}

} // namespace segmecam
