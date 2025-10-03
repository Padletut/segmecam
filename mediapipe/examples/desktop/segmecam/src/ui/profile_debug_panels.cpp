#include "include/ui/ui_panels.h"
#include "include/ar_filters/filter_object.h"
#include "include/ar_filters/model_loader.h"
#include "include/ar_filters/ar_filter_manager.h"  // Phase 8 Day 2
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
        RenderARFilterControls();  // Phase 8 Day 2: AR filter UI controls
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

// Phase 8 Day 2: AR Filter Controls Implementation
void DebugPanel::RenderARFilterControls() {
    if (!ar_filter_mgr_) {
        return;  // AR Filter Manager not available
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("🎭 AR Filters (Phase 8)");
    ImGui::Separator();
    
    // Enable/Disable toggle
    bool ar_enabled = state_.ar_filters_enabled;
    if (ImGui::Checkbox("Enable AR Filters", &ar_enabled)) {
        state_.ar_filters_enabled = ar_enabled;
        std::cout << "[DebugPanel] AR Filters " 
                  << (ar_enabled ? "enabled" : "disabled") << std::endl;
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable/disable AR filter system with full GPU rendering");
    }
    
    // Get available filters
    auto filters_result = ar_filter_mgr_->GetAvailableFilters();
    if (!filters_result.ok()) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "⚠️  Failed to load filters");
        ImGui::TextWrapped("Error: %s", filters_result.status().message().data());
        return;
    }
    
    const auto& filters = *filters_result;
    
    if (filters.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "⚠️  No filters available");
        ImGui::TextWrapped("Place filter definitions in assets/filters/");
        return;
    }
    
    // Display filter count
    ImGui::Text("Available Filters: %zu", filters.size());
    
    // Filter selection dropdown
    static int selected_filter_idx = 0;  // Start at 0 = "(None)"
    static std::vector<std::string> filter_names;
    static std::vector<const char*> filter_items;
    static std::string last_active_filter_id;
    
    // Rebuild filter list if needed
    if (filter_names.size() != filters.size() + 1) {  // +1 for "(None)"
        filter_names.clear();
        filter_items.clear();
        filter_names.push_back("(None)");
        filter_items.push_back(filter_names.back().c_str());
        
        for (const auto& filter : filters) {
            filter_names.push_back(filter.name + " (" + filter.category + ")");
            filter_items.push_back(filter_names.back().c_str());
        }
        
        selected_filter_idx = 0;  // Reset to "None" when list changes
    }
    
    // Sync dropdown with active filter state
    std::string current_active_id = ar_filter_mgr_->GetActiveFilterId();
    if (current_active_id != last_active_filter_id) {
        // Active filter changed - update dropdown to match
        last_active_filter_id = current_active_id;
        
        if (current_active_id.empty()) {
            selected_filter_idx = 0;  // No active filter -> "(None)"
        } else {
            // Find the filter in the list and update dropdown
            bool found = false;
            for (size_t i = 0; i < filters.size(); ++i) {
                if (filters[i].id == current_active_id) {
                    selected_filter_idx = static_cast<int>(i + 1);  // +1 for "(None)" offset
                    found = true;
                    break;
                }
            }
            if (!found) {
                selected_filter_idx = 0;  // Fallback if filter not found
            }
        }
    }
    
    ImGui::Combo("Select Filter", &selected_filter_idx, 
                 filter_items.data(), 
                 static_cast<int>(filter_items.size()));
    
    // Load/Clear buttons
    ImGui::BeginDisabled(selected_filter_idx <= 0);
    if (ImGui::Button("Load Filter")) {
        if (selected_filter_idx > 0 && selected_filter_idx <= static_cast<int>(filters.size())) {
            const auto& selected_filter = filters[selected_filter_idx - 1];
            auto status = ar_filter_mgr_->LoadFilter(selected_filter.id);
            if (status.ok()) {
                std::cout << "[DebugPanel] ✅ Loaded filter: " 
                          << selected_filter.name << std::endl;
                // Don't reset dropdown - it will sync automatically via GetActiveFilterId()
            } else {
                std::cerr << "[DebugPanel] ❌ Failed to load filter: " 
                          << status.message() << std::endl;
                selected_filter_idx = 0;  // Reset to "(None)" on failure
            }
        }
    }
    ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    bool has_active = ar_filter_mgr_->HasActiveFilter();
    ImGui::BeginDisabled(!has_active);
    if (ImGui::Button("Clear Filter")) {
        auto status = ar_filter_mgr_->UnloadCurrentFilter();
        if (status.ok()) {
            std::cout << "[DebugPanel] Cleared active filter" << std::endl;
            // Don't manually reset - it will sync automatically via GetActiveFilterId()
        } else {
            std::cerr << "[DebugPanel] Failed to clear filter: " 
                      << status.message() << std::endl;
        }
    }
    ImGui::EndDisabled();
    
    // Display current filter status
    if (has_active) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "✅ Active Filter");
        
        // Find and display active filter info
        for (size_t i = 0; i < filters.size(); ++i) {
            // Check if this filter is active by ID comparison
            // (We don't have direct access to active filter ID, so we use HasActiveFilter as indicator)
            if (selected_filter_idx == static_cast<int>(i + 1)) {
                const auto& filter = filters[i];
                ImGui::Text("  Name: %s", filter.name.c_str());
                ImGui::Text("  Category: %s", filter.category.c_str());
                if (!filter.description.empty()) {
                    ImGui::TextWrapped("  %s", filter.description.c_str());
                }
                break;
            }
        }
        
        // Display performance metrics (Phase 8 - New System)
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("🔬 Performance Metrics (Phase 8)");
        auto perf = ar_filter_mgr_->GetPerformanceStats();
        ImGui::Text("  Update Time: %.5f ms", perf.render_time_ms);
        ImGui::Text("  Models: %d, Triangles: %d", 
                    perf.models_rendered_per_frame,
                    perf.triangles_per_frame);
        if (perf.average_fps > 0.0f) {
            ImGui::Text("  Avg FPS: %.1f", perf.average_fps);
        }
        ImGui::Text("  Frames Rendered: %d", perf.frames_rendered);
        
        // Performance comparison hint
        ImGui::Spacing();
        ImGui::TextDisabled("Compare with Phase 3 'AR Filter Presets' above");
        ImGui::TextDisabled("(Old system shows ~0.0003 ms update time)");
    } else {
        ImGui::Text("Status: No active filter");
    }
    
    // Help text
    ImGui::Spacing();
    ImGui::TextDisabled("Tips:");
    ImGui::TextDisabled("- Select a filter from the dropdown");
    ImGui::TextDisabled("- Click 'Load Filter' to activate it");
    ImGui::TextDisabled("- Enable AR Filters to see rendering");
    ImGui::TextDisabled("- Filters require face detection");
}

} // namespace segmecam
