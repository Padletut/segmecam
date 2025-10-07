#include "include/ui/virtual_camera_panel.h"
#include "include/ui/ui_panels.h"
#include <iostream>
#include <filesystem>

namespace segmecam {

// Virtual Camera Panel Implementation
VirtualCameraPanel::VirtualCameraPanel(AppState& state)
    : UIPanel("Virtual Camera"), state_(state) {
    RefreshVirtualCameraDevices();
}

void VirtualCameraPanel::RefreshVirtualCameraDevices() {
    vcam_devices_.clear();
    vcam_labels_.clear();
    vcam_items_.clear();

    // Get available loopback devices
    auto devices = EnumerateLoopbackDevices();

    for (const auto& device : devices) {
        vcam_devices_.push_back(device);
        vcam_labels_.push_back(device.name + " (" + device.path + ")");
        vcam_items_.push_back(vcam_labels_.back().c_str());
    }

    // If we have devices, try to match the current virtual camera path
    if (!vcam_devices_.empty()) {
        ui_vcam_idx_ = 0; // Default to first device
        for (size_t i = 0; i < vcam_devices_.size(); ++i) {
            if (vcam_devices_[i].path == state_.virtual_camera_path) {
                ui_vcam_idx_ = static_cast<int>(i);
                break;
            }
        }
    } else {
        ui_vcam_idx_ = -1;
    }
}

void VirtualCameraPanel::Render() {
    if (!visible_) return;

    ImGui::Text("Virtual Camera Output");
    ImGui::Separator();

    // Check if virtual camera is active
    bool vcam_active = state_.vcam.IsOpen();

    if (vcam_active) {
        RenderVirtualCameraActive();
    } else {
        RenderVirtualCameraInactive();
    }

    RenderVirtualCameraHelp();
}

void VirtualCameraPanel::RenderVirtualCameraActive() {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Active (%dx%d)", state_.vcam.Width(), state_.vcam.Height());

    if (ImGui::Button("Stop Virtual Camera")) {
        state_.vcam.Close();
        std::cout << "Virtual camera stopped" << std::endl;
    }
}

void VirtualCameraPanel::RenderVirtualCameraInactive() {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: Inactive");

    // Virtual camera device selection dropdown
    if (ImGui::Button("Refresh Devices")) {
        RefreshVirtualCameraDevices();
    }
    ImGui::SameLine();

    if (vcam_devices_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No v4l2loopback devices found");
    } else {
        RenderVirtualCameraDeviceSelection();
        RenderVirtualCameraResolutionInfo();
        RenderVirtualCameraStartButton();
    }
}

void VirtualCameraPanel::RenderVirtualCameraDeviceSelection() {
    if (ImGui::Combo("Device", &ui_vcam_idx_, vcam_items_.data(), static_cast<int>(vcam_items_.size()))) {
        // Update the state with selected device path
        if (ui_vcam_idx_ >= 0 && ui_vcam_idx_ < static_cast<int>(vcam_devices_.size())) {
            state_.virtual_camera_path = vcam_devices_[ui_vcam_idx_].path;
        }
    }
}

void VirtualCameraPanel::RenderVirtualCameraResolutionInfo() {
    // Resolution info (display only - matches input camera)
    ImGui::Text("Resolution: %dx%d (matches input camera)",
               state_.camera_width > 0 ? state_.camera_width : 640,
               state_.camera_height > 0 ? state_.camera_height : 480);
}

void VirtualCameraPanel::RenderVirtualCameraStartButton() {
    if (ImGui::Button("Start Virtual Camera")) {
        if (!vcam_devices_.empty() && ui_vcam_idx_ >= 0 && ui_vcam_idx_ < static_cast<int>(vcam_devices_.size())) {
            const std::string& device_path = vcam_devices_[ui_vcam_idx_].path;
            // Use input camera resolution, fallback to 640x480 if not available
            int vcam_width = state_.camera_width > 0 ? state_.camera_width : 640;
            int vcam_height = state_.camera_height > 0 ? state_.camera_height : 480;

            if (state_.vcam.Open(device_path.c_str(), vcam_width, vcam_height)) {
                std::cout << "Virtual camera started on " << device_path
                         << " at " << vcam_width << "x" << vcam_height << std::endl;
            } else {
                std::cout << "Failed to start virtual camera on " << device_path << std::endl;
            }
        } else {
            std::cout << "No virtual camera device selected" << std::endl;
        }
    }
}

void VirtualCameraPanel::RenderVirtualCameraHelp() {
    // Virtual camera help
    ImGui::Separator();
    ImGui::TextDisabled("Usage:");
    ImGui::TextDisabled("• Install v4l2loopback kernel module");
    ImGui::TextDisabled("• Use in video calls (Zoom, Teams, etc.)");
    ImGui::TextDisabled("• Refresh to detect new devices");

    // Clear separation before next section
    ImGui::Spacing();
    ImGui::Separator();
}

} // namespace segmecam