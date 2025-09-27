#ifndef APPLICATION_RUN_H
#define APPLICATION_RUN_H

#include <memory>
#include <opencv2/opencv.hpp>
#include <SDL.h>
#include <GL/gl.h>
#include "application/manager_coordination.h"
#include "include/application/app_state.h"

// Include module headers for struct definitions
#include "include/ui/ui_manager_enhanced.h"
#include "application/frame_processor.h"
#include "application/mediapipe_processor.h"

// Forward declarations
namespace segmecam {
    class UIManager;
    class EffectsManager;
}

// MediaPipe includes for complete types
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "mediapipe/framework/output_stream_poller.h"

namespace segmecam {

/**
 * Application main loop module - handles the core application execution loop
 * 
 * This module follows the modular architecture pattern and provides the main
 * application loop functionality including frame processing, UI rendering,
 * and event handling.
 */
class ApplicationRun {
public:
    /**
     * Execute the main application event loop
     * 
     * @param managers Manager coordination structure
     * @param mediapipe_graph MediaPipe calculator graph
     * @param mask_poller Segmentation mask output poller
     * @param multi_face_landmarks_poller Optional face landmarks poller
     * @param face_rects_poller Optional face rects poller
     * @param face_blendshapes_poller Optional face blendshapes poller
     * @param window SDL window for rendering
     * @param app_state Application state for shared data
     * @return Exit code (0 for success, non-zero for error)
     */
    static int ExecuteMainLoop(
        ManagerCoordination::Managers& managers,
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
        std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
        std::unique_ptr<mediapipe::OutputStreamPoller>& face_blendshapes_poller,
        SDL_Window* window,
        AppState& app_state
    );    /**
     * Sync status FROM EffectsManager back TO app_state (e.g., OpenCL availability)
     */
    static void SyncStatusFromEffectsManager(const EffectsManager& effects_manager, AppState& app_state);

    /**
     * Sync app_state settings to EffectsManager
     */
    static void SyncSettingsToEffectsManager(EffectsManager& effects_manager, const AppState& app_state);

private:
    /**
     * Verify that all required managers are available
     * @param managers Manager coordination structure to verify
     * @return true if all managers are available, false otherwise
     */
    static bool VerifyRequiredManagers(const ManagerCoordination::Managers& managers);

};

} // namespace segmecam

#endif // APPLICATION_RUN_H