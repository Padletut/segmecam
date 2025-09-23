#ifndef APPLICATION_SYNC_H
#define APPLICATION_SYNC_H

#include "include/application/app_state.h"

// Forward declarations
namespace segmecam {
    class EffectsManager;
}

namespace segmecam {

/**
 * Utility class for synchronizing application state between managers
 */
class ApplicationSync {
public:
    /**
     * Sync settings from AppState to EffectsManager
     * Transfers all beauty, background, and processing settings to the effects manager
     * @param effects_manager Effects manager instance to update
     * @param app_state Application state containing current settings
     */
    static void SyncSettingsToEffectsManager(EffectsManager& effects_manager, const AppState& app_state);

    /**
     * Sync status from EffectsManager back to AppState
     * Updates application state with status information from effects manager (e.g., OpenCL availability)
     * @param effects_manager Effects manager instance to read from
     * @param app_state Application state to update
     */
    static void SyncStatusFromEffectsManager(const EffectsManager& effects_manager, AppState& app_state);
};

} // namespace segmecam

#endif // APPLICATION_SYNC_H