#include "include/ui/pipewire_output_panel.h"
#include "include/ui/ui_panels.h"
#include <iostream>

namespace segmecam {

// PipeWire Output Panel Implementation
PipeWireOutputPanel::PipeWireOutputPanel(AppState& state, CameraManager& camera_mgr)
    : UIPanel("Virtual Camera"), state_(state), camera_mgr_(camera_mgr) {
}

void PipeWireOutputPanel::Render() {
    if (!visible_) return;

    ImGui::Text("Virtual Camera Output");
    ImGui::Separator();

    if (camera_mgr_.IsPipeWireOutputActive()) {
        RenderPipeWireOutputActive();
    } else {
        RenderPipeWireOutputInactive();
    }

    RenderPipeWireOutputHelp();
}

void PipeWireOutputPanel::RenderPipeWireOutputActive() {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Active");

    // Show stream name
    const std::string& stream_name = camera_mgr_.GetPipeWireStreamName();
    ImGui::Text("Stream: %s", stream_name.c_str());

    // Show actual resolution info from camera manager
    const auto& camera_state = camera_mgr_.GetState();
    int actual_width = camera_state.current_width > 0 ? camera_state.current_width : 640;
    int actual_height = camera_state.current_height > 0 ? camera_state.current_height : 480;
    int actual_fps = camera_state.current_fps > 0 ? static_cast<int>(camera_state.current_fps) : 30;
    ImGui::Text("Resolution: %dx%d @ %d FPS", actual_width, actual_height, actual_fps);

    if (ImGui::Button("Stop Virtual Camera")) {
        camera_mgr_.ShutdownPipeWireOutput();
        state_.pipewire_output_active = false;
        std::cout << "PipeWire output stopped" << std::endl;
    }
}

void PipeWireOutputPanel::RenderPipeWireOutputInactive() {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: Inactive");

    ImGui::Text("Stream: SegmeCam Virtual Camera");
    // Show actual camera resolution that will be used
    int display_width = state_.camera_width > 0 ? state_.camera_width : 640;
    int display_height = state_.camera_height > 0 ? state_.camera_height : 480;
    int display_fps = state_.camera_fps > 0 ? static_cast<int>(state_.camera_fps) : 30;
    ImGui::Text("Resolution: %dx%d @ %d FPS", display_width, display_height, display_fps);

    if (ImGui::Button("Start Virtual Camera")) {
        // Defer PipeWire initialization to avoid UI thread issues
        // Use the actual camera input resolution, not hardcoded values
        int pipewire_width = state_.camera_width > 0 ? state_.camera_width : 640;
        int pipewire_height = state_.camera_height > 0 ? state_.camera_height : 480;
        int pipewire_fps = state_.camera_fps > 0 ? static_cast<int>(state_.camera_fps) : 30;

        pipewire_start_requested_ = true;
        pipewire_width_ = pipewire_width;
        pipewire_height_ = pipewire_height;
        pipewire_fps_ = pipewire_fps;
        std::cout << "PipeWire output start requested - will initialize on next frame" << std::endl;
        std::cout << "Requested resolution: " << pipewire_width << "x" << pipewire_height << " @ " << pipewire_fps << " FPS" << std::endl;
    }

    ImGui::TextDisabled("Note: Only available in Flatpak environment");
}

void PipeWireOutputPanel::RenderPipeWireOutputHelp() {
    ImGui::Separator();
    ImGui::TextDisabled("PipeWire Output:");
    ImGui::TextDisabled("• Portal-compliant video streaming");
    ImGui::TextDisabled("• Works with modern OBS Studio, Discord, browser apps");
    ImGui::TextDisabled("• Can be bridged to v4l2loopback for broader compatibility");
    ImGui::TextDisabled("• Manual control for testing and troubleshooting");
}

void PipeWireOutputPanel::ProcessDeferredPipeWireInitialization() {
    if (pipewire_start_requested_) {
        pipewire_start_requested_ = false; // Reset flag

        std::cout << "Processing deferred PipeWire initialization..." << std::endl;
        if (camera_mgr_.InitializePipeWireOutput("SegmeCam Virtual Camera", pipewire_width_, pipewire_height_, pipewire_fps_)) {
            state_.pipewire_output_active = true;
            std::cout << "PipeWire output started successfully" << std::endl;
        } else {
            state_.pipewire_output_active = false;
            std::cout << "Failed to start PipeWire output" << std::endl;
        }
    }
}

} // namespace segmecam