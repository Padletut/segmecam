#include "include/application/application_initialization.h"

// Include extracted modules
#include "include/application/mediapipe_setup.h"

// Include ImGui for GUI initialization
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h" 
#include "third_party/imgui/backends/imgui_impl_opengl3.h"

// Include SDL for OpenGL context
#include <SDL_opengl.h>

#include <iostream>

namespace segmecam {

int ApplicationInitialization::InitializeMediaPipe(
    const ApplicationConfig& config,
    const GPUSetupState& gpu_setup_state,
    std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller
) {
    // CRITICAL: Setup MediaPipe BEFORE SDL (matches original code order)
    std::string graph_path = MediaPipeSetup::SelectGraphPath(config, gpu_setup_state.gpu_caps);
    if (MediaPipeSetup::InitializeGraph(mediapipe_graph, graph_path, gpu_setup_state.gpu_caps, config) != 0) {
        std::cerr << "❌ MediaPipe setup failed" << std::endl;
        return -2;
    }
    
    // Setup basic output stream poller (required before StartRun to prevent deadlock)
    std::cout << "📡 Setting up MediaPipe output stream pollers..." << std::endl;
    auto mask_poller_or = mediapipe_graph->AddOutputStreamPoller("segmentation_mask_cpu");
    if (!mask_poller_or.ok()) {
        std::cout << "⚠️  'segmentation_mask_cpu' not found, trying 'segmentation_mask'..." << std::endl;
        // Try alternative stream name for CPU-only graphs
        mask_poller_or = mediapipe_graph->AddOutputStreamPoller("segmentation_mask");
        if (!mask_poller_or.ok()) {
            std::cerr << "❌ Failed to setup mask output poller: " << mask_poller_or.status().message() << std::endl;
            return -3;
        }
    }
    mask_poller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(mask_poller_or.value()));
    std::cout << "✅ Segmentation mask poller ready" << std::endl;
    
    // Setup face landmarks pollers if face mode enabled (BEFORE StartRun)
    bool use_face = (config.graph_path.find("face") != std::string::npos);
    if (use_face) {
        std::cout << "👤 Setting up face landmarks pollers..." << std::endl;
        
        // Setup multi_face_landmarks poller
        auto multi_face_landmarks_poller_or = mediapipe_graph->AddOutputStreamPoller("multi_face_landmarks");
        if (!multi_face_landmarks_poller_or.ok()) {
            std::cerr << "❌ Failed to setup multi_face_landmarks poller: " << multi_face_landmarks_poller_or.status().message() << std::endl;
            return -5;
        }
        multi_face_landmarks_poller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(multi_face_landmarks_poller_or.value()));
        
        // Setup face_rects poller (optional - may not exist in all face graphs)
        auto face_rects_poller_or = mediapipe_graph->AddOutputStreamPoller("face_rects");
        if (!face_rects_poller_or.ok()) {
            std::cout << "ℹ️  face_rects stream not available in this graph (this is normal for some face graphs)" << std::endl;
            face_rects_poller = nullptr;
        } else {
            face_rects_poller = std::make_unique<mediapipe::OutputStreamPoller>(std::move(face_rects_poller_or.value()));
            std::cout << "✅ Face rects poller attached successfully" << std::endl;
        }
        
        std::cout << "✅ Face landmarks pollers ready" << std::endl;
    } else {
        // Set to null if not using face landmarks
        multi_face_landmarks_poller = nullptr;
        face_rects_poller = nullptr;
    }
    
    // Now safe to start the MediaPipe graph
    std::cout << "🚀 Starting MediaPipe graph..." << std::endl;
    if (MediaPipeSetup::StartGraph(mediapipe_graph) != 0) {
        std::cerr << "❌ MediaPipe graph start failed" << std::endl;
        return -4;
    }
    std::cout << "✅ MediaPipe graph running" << std::endl;
    
    return 0;
}

int ApplicationInitialization::InitializeSDLAndOpenGL(
    SDL_Window*& window,
    SDL_GLContext& gl_context
) {
    // NOW setup SDL and OpenGL context AFTER MediaPipe (matches original)
    std::cout << "🖥️ Initializing SDL and OpenGL context..." << std::endl;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { 
        std::cerr << "❌ SDL error: " << SDL_GetError() << std::endl;
        return -5; 
    }
    
    // Setup OpenGL attributes (matching original - compatibility mode for immediate mode rendering)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    // Use compatibility profile to allow immediate mode rendering (glBegin/glEnd)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    window = SDL_CreateWindow("SegmeCam", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) { 
        std::cerr << "❌ SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return -6; 
    }
    
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) { 
        std::cerr << "❌ SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        return -7; 
    }
    
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);
    std::cout << "✅ SDL and OpenGL context ready" << std::endl;
    
    return 0;
}

int ApplicationInitialization::InitializeImGui(SDL_Window* window, SDL_GLContext gl_context) {
    // Initialize enhanced ImGui for Phase 8 (UI Manager integration will be refined)
    std::cout << "🎨 Initializing enhanced ImGui..." << std::endl;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");
    std::cout << "✅ Enhanced ImGui initialized" << std::endl;
    
    return 0;
}

int ApplicationInitialization::InitializeManagers(
    ManagerCoordination::Managers& managers,
    AppState& app_state
) {
    // Setup all Phase 1-7 managers using extracted coordination module
    if (!ManagerCoordination::SetupManagers(managers, app_state)) {
        std::cerr << "❌ Manager setup failed" << std::endl;
        return -8;
    }
    
    // Load default profile's background image (needs to be done after all managers are initialized)
    ManagerCoordination::LoadDefaultProfileBackgroundImage(managers, app_state);
    
    return 0;
}

int ApplicationInitialization::InitializeApplication(
    const ApplicationConfig& config,
    ManagerCoordination::Managers& managers,
    AppState& app_state,
    std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    SDL_Window*& window,
    SDL_GLContext& gl_context,
    GPUSetupState& gpu_setup_state
) {
    std::cout << "🚀 Initializing SegmeCam Application..." << std::endl;
    
    // Setup GPU detection using extracted module
    gpu_setup_state = GPUSetup::DetectAndSetupGPU();
    
    // Initialize MediaPipe system
    int mediapipe_result = InitializeMediaPipe(config, gpu_setup_state, mediapipe_graph, mask_poller, multi_face_landmarks_poller, face_rects_poller);
    if (mediapipe_result != 0) {
        return mediapipe_result;
    }
    
    // Initialize SDL and OpenGL
    int sdl_result = InitializeSDLAndOpenGL(window, gl_context);
    if (sdl_result != 0) {
        return sdl_result;
    }
    
    // Initialize ImGui
    int imgui_result = InitializeImGui(window, gl_context);
    if (imgui_result != 0) {
        return imgui_result;
    }
    
    // Initialize managers
    int managers_result = InitializeManagers(managers, app_state);
    if (managers_result != 0) {
        return managers_result;
    }
    
    std::cout << "✅ SegmeCam Application initialized successfully!" << std::endl;
    return 0;
}

} // namespace segmecam