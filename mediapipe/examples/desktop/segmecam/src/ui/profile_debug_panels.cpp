#include "include/ui/ui_panels.h"
#include "include/ar_filters/filter_object.h"
#include "include/ar_filters/model_loader.h"
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
    ImGui::Checkbox("Show Facemask", &state_.show_facemask);
    ImGui::Checkbox("Show Wrinkle Segmentation", &state_.show_wrinkle_segmentation);

    // Mesh visualization
    ImGui::Checkbox("Show Face Mesh", &state_.show_mesh);
    if (state_.show_mesh) {
        ImGui::SameLine();
        ImGui::Checkbox("Dense", &state_.show_mesh_dense);
        ImGui::SameLine();
        ImGui::Button("?##mesh_dense");
    }
    
    // Anchor point visualization (Phase 2 Step 3)
    ImGui::Checkbox("Show Anchor Points", &state_.show_anchors);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Display 7 key attachment points for AR filters");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // AR Filter Test Demo (Phase 2 Step 5.5)
    ImGui::Text("AR Filter Test Demo");
    ImGui::Separator();
    
    bool test_changed = ImGui::Checkbox("Enable Filter Test", &state_.show_filter_test);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Creates 7 colored geometric primitives attached to face anchors");
    }
    
    if (test_changed) {
        if (state_.show_filter_test && !state_.filter_test_demo.IsInitialized()) {
            // Initialize test on first enable
            state_.filter_test_demo.Initialize(state_.attachment_controller, state_.ar_filters_enabled);
            std::cout << "[DebugPanel] AR Filter test demo initialized" << std::endl;
        }
        state_.filter_test_demo.SetActive(state_.show_filter_test, state_.ar_filters_enabled);
    }
    
    // Show test statistics if active
    if (state_.show_filter_test && state_.filter_test_demo.IsInitialized()) {
        ImGui::Indent();
        auto stats = state_.attachment_controller.GetStatistics();
        ImGui::Text("Filters: %zu/%zu visible", 
                   stats.visible_filters, 
                   stats.total_filters);
        ImGui::Text("Update: %.2f ms", stats.average_update_time_ms);
        
        if (ImGui::Button("Print Stats")) {
            state_.filter_test_demo.PrintStatistics(state_.attachment_controller);
        }
        ImGui::Unindent();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // AR Filter Presets (Phase 2 Step 6)
    ImGui::Text("AR Filter Presets");
    ImGui::Separator();
    
    // Preset selection dropdown
    static int selected_preset = 0;
    const char* preset_items[] = {
        "None",
        "Classic Glasses",
        "Party Hat",
        "Face Mask"
    };
    
    if (ImGui::Combo("Filter Preset", &selected_preset, preset_items, IM_ARRAYSIZE(preset_items))) {
        FilterPreset preset = static_cast<FilterPreset>(selected_preset);
        state_.filter_preset_manager.ApplyPreset(preset, state_.attachment_controller, state_.ar_filters_enabled);
        std::cout << "[DebugPanel] Applied preset: " 
                  << state_.filter_preset_manager.GetPresetName(preset) << std::endl;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("?##preset_help")) {}
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("For Test Demo (7 Filters), use 'Enable Filter Test' checkbox above");
    }
    
    // Show preset info if one is active
    if (state_.filter_preset_manager.IsPresetActive()) {
        ImGui::Indent();
        FilterPreset current = state_.filter_preset_manager.GetCurrentPreset();
        ImGui::Text("Active: %s", state_.filter_preset_manager.GetPresetName(current).c_str());
        ImGui::Text("Filters: %d", state_.filter_preset_manager.GetPresetFilterCount(current));
        
        auto stats = state_.attachment_controller.GetStatistics();
        ImGui::Text("Visible: %zu/%zu", stats.visible_filters, stats.total_filters);
        ImGui::Text("Update: %.5f ms", stats.average_update_time_ms);
        
        if (ImGui::Button("Clear Preset")) {
            state_.filter_preset_manager.ClearCurrentPreset(state_.attachment_controller, state_.ar_filters_enabled);
            selected_preset = 0;  // Reset dropdown to "None"
            std::cout << "[DebugPanel] Cleared filter preset" << std::endl;
        }
        ImGui::Unindent();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // 3D Model Loading (Phase 3 Step 7)
    ImGui::Text("3D Model Loading (Phase 3)");
    ImGui::Separator();
    
    // Model file path input
    static char model_path[512] = "assets/ar_filters/models/simple_cube.obj";
    ImGui::InputText("Model Path", model_path, sizeof(model_path));
    ImGui::TextDisabled("Path relative to workspace or absolute path");
    
    // Anchor point selection
    static int model_anchor = 0;
    const char* anchor_items[] = {
        "nose_bridge",
        "forehead", 
        "chin",
        "left_cheek",
        "right_cheek",
        "left_eye",
        "right_eye"
    };
    ImGui::Combo("Anchor Point", &model_anchor, anchor_items, IM_ARRAYSIZE(anchor_items));
    
    // Model scale control
    static float model_scale = 1.0f;
    ImGui::SliderFloat("Model Scale", &model_scale, 0.1f, 5.0f);
    
    // Load button
    if (ImGui::Button("Load 3D Model")) {
        std::string anchor_name = anchor_items[model_anchor];
        std::cout << "[DebugPanel] Loading 3D model: " << model_path 
                  << " at anchor: " << anchor_name << std::endl;
        
        // Create FilterObject from model
        FilterObject model_filter = CreateFromModel(
            anchor_name,
            std::string("Model: ") + model_path,
            std::string(model_path)
        );
        
        if (model_filter.model != nullptr && !model_filter.model->meshes.empty()) {
            model_filter.local_scale = model_scale;
            model_filter.offset = {0.0f, 0.0f, 0.0f};
            model_filter.enabled = true;
            model_filter.visible = true;
            
            // Enable AR filters and attach
            state_.ar_filters_enabled = true;
            auto filter_id = state_.attachment_controller.AttachFilter(model_filter);
            
            std::cout << "[DebugPanel] Model loaded successfully! Filter ID: " 
                      << filter_id << std::endl;
            std::cout << "[DebugPanel] Meshes: " << model_filter.model->meshes.size()
                      << ", Materials: " << model_filter.model->materials.size() << std::endl;
        } else {
            std::cout << "[DebugPanel] ERROR: Failed to load model from: " 
                      << model_path << std::endl;
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear All Models")) {
        state_.attachment_controller.DetachAll();
        state_.ar_filters_enabled = false;
        std::cout << "[DebugPanel] Cleared all models" << std::endl;
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
