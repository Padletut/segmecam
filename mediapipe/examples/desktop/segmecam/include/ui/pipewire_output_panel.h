#pragma once

#include "imgui.h"
#include <string>
#include "include/application/app_state.h"
#include "include/camera/camera_manager.h"
#include "include/ui/ui_panel_base.h"

namespace segmecam {

// PipeWire output controls panel
class PipeWireOutputPanel : public UIPanel {
public:
    PipeWireOutputPanel(AppState& state, CameraManager& camera_mgr);
    ~PipeWireOutputPanel() override = default;

    void Render() override;

    // Process deferred PipeWire initialization
    void ProcessDeferredPipeWireInitialization();

private:
    void RenderPipeWireOutputActive();
    void RenderPipeWireOutputInactive();
    void RenderPipeWireOutputHelp();

    AppState& state_;
    CameraManager& camera_mgr_;

    // PipeWire output UI state
    bool pipewire_start_requested_ = false;
    int pipewire_width_ = 640;
    int pipewire_height_ = 480;
    int pipewire_fps_ = 30;
};

} // namespace segmecam