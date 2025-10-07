#include "include/application/application_cleanup.h"
#include "include/application/manager_coordination.h"

// Include MediaPipe for graph cleanup
#include "mediapipe/framework/calculator_graph.h"

// Include ImGui for cleanup
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h" 
#include "third_party/imgui/backends/imgui_impl_opengl3.h"

namespace segmecam {

void ApplicationCleanup::PerformCleanup(
    ManagerCoordination::Managers& managers,
    std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
    SDL_GLContext& gl_context,
    SDL_Window*& window
) {
    std::cout << "🧹 Cleaning up application resources..." << std::endl;
    
    // Cleanup in reverse order of initialization to handle dependencies properly
    
    // 1. Shutdown managers first (they may depend on MediaPipe/SDL)
    CleanupManagers(managers);
    
    // 2. Cleanup MediaPipe graph
    CleanupMediaPipe(mediapipe_graph);
    
    // 3. Shutdown ImGui rendering
    CleanupImGui();
    
    // 4. Cleanup SDL and OpenGL resources last
    CleanupSDL(gl_context, window);
    
    std::cout << "✅ Application cleanup completed" << std::endl;
}

void ApplicationCleanup::CleanupManagers(ManagerCoordination::Managers& managers) {
    // Use the coordination module to properly shutdown all managers
    ManagerCoordination::ShutdownManagers(managers);
}

void ApplicationCleanup::CleanupMediaPipe(std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph) {
    if (mediapipe_graph) {
        std::cout << "🔧 Shutting down MediaPipe graph..." << std::endl;
        
        // Close input stream first
        auto status = mediapipe_graph->CloseInputStream("input_video");
        if (!status.ok()) {
            std::cerr << "Warning: Failed to close input stream: " << status.message() << std::endl;
        }
        
        // Wait for graph to finish processing
        status = mediapipe_graph->WaitUntilDone();
        if (!status.ok()) {
            std::cerr << "Warning: Graph shutdown error: " << status.message() << std::endl;
        }
        
        std::cout << "✅ MediaPipe graph shutdown completed" << std::endl;
    }
}

void ApplicationCleanup::CleanupImGui() {
    std::cout << "🎨 Shutting down ImGui..." << std::endl;

    // Only shutdown if ImGui context exists (init may have failed earlier)
    if (ImGui::GetCurrentContext() != nullptr) {
        // Shutdown ImGui in proper order
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        std::cout << "✅ Enhanced ImGui cleaned up" << std::endl;
    } else {
        std::cout << "ℹ️  ImGui not initialized; skipping shutdown" << std::endl;
    }
}

void ApplicationCleanup::CleanupSDL(SDL_GLContext& gl_context, SDL_Window*& window) {
    std::cout << "🖥️ Shutting down SDL and OpenGL..." << std::endl;
    
    // Clean up OpenGL context
    if (gl_context) {
        SDL_GL_DeleteContext(gl_context);
        gl_context = nullptr;
        std::cout << "✅ OpenGL context released" << std::endl;
    }
    
    // Clean up SDL window
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
        std::cout << "✅ SDL window destroyed" << std::endl;
    }
    
    // Quit SDL subsystems
    SDL_Quit();
    std::cout << "✅ SDL quit" << std::endl;
}

} // namespace segmecam
