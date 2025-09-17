#ifndef APPLICATION_CLEANUP_H
#define APPLICATION_CLEANUP_H

#include <memory>
#include <iostream>
#include <SDL.h>
#include "application/manager_coordination.h"

// Forward declarations to avoid circular dependencies
namespace mediapipe {
class CalculatorGraph;
}

namespace segmecam {

/**
 * Application cleanup module - handles graceful shutdown of all application subsystems
 * 
 * This module follows the modular architecture pattern and provides centralized
 * cleanup for MediaPipe, SDL, ImGui, and manager resources.
 */
class ApplicationCleanup {
public:
    /**
     * Perform complete application cleanup in proper order
     * @param managers Reference to manager coordination structure
     * @param mediapipe_graph MediaPipe graph to shutdown
     * @param gl_context SDL OpenGL context to cleanup
     * @param window SDL window to destroy
     */
    static void PerformCleanup(
        ManagerCoordination::Managers& managers,
        std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
        SDL_GLContext& gl_context,
        SDL_Window*& window
    );

private:
    /**
     * Shutdown all managers through coordination module
     */
    static void CleanupManagers(ManagerCoordination::Managers& managers);
    
    /**
     * Cleanup MediaPipe graph and resources
     */
    static void CleanupMediaPipe(std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph);
    
    /**
     * Shutdown ImGui rendering system
     */
    static void CleanupImGui();
    
    /**
     * Cleanup SDL OpenGL context and window
     */
    static void CleanupSDL(SDL_GLContext& gl_context, SDL_Window*& window);
};

} // namespace segmecam

#endif // APPLICATION_CLEANUP_H