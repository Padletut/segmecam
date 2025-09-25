#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include "include/application/app_state.h"
#include "include/camera/cam_enum.h"
#include "include/ui/ui_panel_base.h"

namespace segmecam {

// Virtual camera controls panel
class VirtualCameraPanel : public UIPanel {
public:
    VirtualCameraPanel(AppState& state);
    ~VirtualCameraPanel() override = default;

    void Render() override;

private:
    void RenderVirtualCameraActive();
    void RenderVirtualCameraInactive();
    void RefreshVirtualCameraDevices();
    void RenderVirtualCameraDeviceSelection();
    void RenderVirtualCameraResolutionInfo();
    void RenderVirtualCameraStartButton();
    void RenderVirtualCameraHelp();
    
    AppState& state_;
    
    // Virtual camera state
    std::vector<LoopbackDesc> vcam_devices_;
    std::vector<std::string> vcam_labels_;
    std::vector<const char*> vcam_items_;
    int ui_vcam_idx_ = 0;
};

} // namespace segmecam
