#include "include/ui/ui_panels.h"
#include "include/ui/ui_utils.h"
#include "include/camera/camera_manager.h"
#include "include/effects/effects_manager.h"
#include "src/config/config_manager.h"
#include "include/camera/cam_enum.h"
#include "include/profile/profile_manager.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include <functional>

namespace segmecam {

// Camera Panel Implementation
CameraPanel::CameraPanel(AppState& state, CameraManager& camera_mgr, EffectsManager& effects_mgr)
    : UIPanel("Camera"), state_(state), camera_mgr_(camera_mgr), effects_mgr_(effects_mgr), 
      profile_mgr_(state, camera_mgr, effects_mgr),
      virtual_camera_panel_(state),
      pipewire_panel_(state, camera_mgr),
      camera_controls_panel_(camera_mgr) {
    SyncWithCameraState();
}

void CameraPanel::SyncWithCameraState() {
    // Sync UI indices with current camera settings from CameraManager
    ui_cam_idx_ = camera_mgr_.GetUICameraIndex();
    ui_res_idx_ = camera_mgr_.GetUIResolutionIndex();
    ui_fps_idx_ = camera_mgr_.GetUIFPSIndex();
}

void CameraPanel::Render() {
    if (!visible_) return;
    
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderCameraSelection();
        RenderResolutionSettings();
        virtual_camera_panel_.Render();
        profile_mgr_.RenderProfileSection();
        camera_controls_panel_.Render();
        pipewire_panel_.Render();
    }
}

void CameraPanel::RenderCameraSelection() {
    ImGui::Text("Camera Selection");
    ImGui::Separator();
    
    // Get camera list from manager
    const auto& cam_list = camera_mgr_.GetCameraList();
    
    // Create display list for combo
    std::vector<const char*> items;
    for (const auto& cam : cam_list) {
        items.push_back(cam.name.c_str());
    }
    
    if (!items.empty()) {
        int current_cam = camera_mgr_.GetUICameraIndex();
        if (ImGui::Combo("Camera", &ui_cam_idx_, items.data(), (int)items.size())) {
            if (ui_cam_idx_ != current_cam) {
                // Use placeholder resolution and fps for now
                camera_mgr_.SetCurrentCamera(ui_cam_idx_, 0, 0);
                std::cout << "Camera changed to: " << items[ui_cam_idx_] << std::endl;
            }
        }
    } else {
        ImGui::Text("No cameras detected");
        if (ImGui::Button("Refresh")) {
            camera_mgr_.RefreshCameraList();
        }
    }
}

void CameraPanel::RenderResolutionSettings() {
    ImGui::Text("Resolution & FPS");
    ImGui::Separator();
    
    RenderResolutionControls();
    RenderFPSControls();
}

void CameraPanel::RenderResolutionControls() {
    // Get available resolutions from camera manager
    const auto& res_list = camera_mgr_.GetCurrentResolutions();
    
    // Create display list for resolutions using member variables
    res_strings_.clear();
    res_items_.clear();
    for (const auto& res : res_list) {
        res_strings_.push_back(std::to_string(res.first) + "x" + std::to_string(res.second));
        res_items_.push_back(res_strings_.back().c_str());
    }
    
    if (!res_items_.empty()) {
        if (ImGui::Combo("Resolution", &ui_res_idx_, res_items_.data(), (int)res_items_.size())) {
            if (ui_res_idx_ >= 0 && ui_res_idx_ < (int)res_list.size()) {
                const auto& selected_res = res_list[ui_res_idx_];
                camera_mgr_.SetResolution(selected_res.first, selected_res.second);
                std::cout << "Resolution changed to: " << selected_res.first << "x" << selected_res.second << std::endl;
            }
        }
    }
}

void CameraPanel::RenderFPSControls() {
    // FPS options using member variables
    const auto& fps_list = camera_mgr_.GetCurrentFPSOptions();
    fps_strings_.clear();
    fps_items_.clear();
    for (int fps : fps_list) {
        fps_strings_.push_back(std::to_string(fps) + " FPS");
        fps_items_.push_back(fps_strings_.back().c_str());
    }
    
    if (!fps_items_.empty()) {
        if (ImGui::Combo("FPS", &ui_fps_idx_, fps_items_.data(), (int)fps_items_.size())) {
            if (ui_fps_idx_ >= 0 && ui_fps_idx_ < (int)fps_list.size()) {
                camera_mgr_.SetFPS(fps_list[ui_fps_idx_]);
                std::cout << "FPS changed to: " << fps_list[ui_fps_idx_] << std::endl;
            }
        }
    }
}

void CameraPanel::RenderProfileSection() {
    profile_mgr_.RenderProfileSection();
}

void CameraPanel::UpdateDefaultProfileDisplay() {
    profile_mgr_.UpdateDefaultProfileDisplay();
}

void CameraPanel::ProcessDeferredPipeWireInitialization() {
    pipewire_panel_.ProcessDeferredPipeWireInitialization();
}

} // namespace segmecam
