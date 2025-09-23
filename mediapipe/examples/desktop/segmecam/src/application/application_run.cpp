#include "application/application_run.h"
#include "application/manager_coordination.h"
#include "application/frame_processor.h"
#include "application/application_sync.h"
#include "include/ui/ui_manager_enhanced.h"
#include "app_state.h"
#include <memory>
#include <iostream>

namespace segmecam {

bool ApplicationRun::VerifyRequiredManagers(const ManagerCoordination::Managers& managers) {
    if (!managers.camera) {
        std::cerr << "❌ Camera manager not available!" << std::endl;
        return false;
    }
    if (!managers.effects) {
        std::cerr << "❌ Effects manager not available!" << std::endl;
        return false;
    }
    if (!managers.config) {
        std::cerr << "❌ Config manager not available!" << std::endl;
        return false;
    }
    if (!managers.ui) {
        std::cerr << "❌ UI manager not available!" << std::endl;
        return false;
    }
    return true;
}

int ApplicationRun::ExecuteMainLoop(
    ManagerCoordination::Managers& managers,
    std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    SDL_Window* window,
    AppState& app_state) {

    std::cout << "🚀 ApplicationRun::ExecuteMainLoop - Starting main application loop!" << std::endl;

    // Verify all required managers are available
    if (!VerifyRequiredManagers(managers)) {
        return 1;
    }

    // Initialize frame processing state variables
    int64_t frame_id = 0;
    double fps = 0.0;
    uint64_t fps_frames = 0;
    uint32_t fps_last_ms = SDL_GetTicks();
    int frame_count = 0;
    bool running = true;
    bool has_landmarks = (multi_face_landmarks_poller != nullptr);

    // Initialize frame processing state and parameters
    FrameProcessingParams params = {
        managers, mediapipe_graph, mask_poller, multi_face_landmarks_poller,
        face_rects_poller, window, app_state, *managers.ui,
        frame_id, fps, fps_frames, fps_last_ms, frame_count, running, has_landmarks
    };

    std::cout << "🎬 Starting main event loop..." << std::endl;

    // Main application loop
    while (params.running) {
        // Process SDL events and UI
        if (!managers.ui->ProcessEvents(params.running)) {
            std::cout << "🛑 UI requested exit" << std::endl;
            break;
        }

        // Process one complete frame
        if (!FrameProcessor::ProcessFrame(params)) {
            std::cout << "⚠️  Frame processing failed, continuing..." << std::endl;
        }

        params.frame_count++;
    }

    std::cout << "👋 Application loop ended gracefully. Processed " << params.frame_count << " frames." << std::endl;
    return 0;
}

void ApplicationRun::SyncStatusFromEffectsManager(const EffectsManager& effects_manager, AppState& app_state) {
    ApplicationSync::SyncStatusFromEffectsManager(effects_manager, app_state);
}

void ApplicationRun::SyncSettingsToEffectsManager(EffectsManager& effects_manager, const AppState& app_state) {
    ApplicationSync::SyncSettingsToEffectsManager(effects_manager, app_state);
}

} // namespace segmecam
