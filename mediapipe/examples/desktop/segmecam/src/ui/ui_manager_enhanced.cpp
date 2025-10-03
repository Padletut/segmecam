#include "include/ui/ui_manager_enhanced.h"
#include "include/ui/ui_panels.h"
#include "include/application/app_state.h"
#include "include/camera/camera_manager.h"
#include "src/config/config_manager.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include <epoxy/gl.h>  // Modern OpenGL function loader
#include <iostream>

namespace segmecam {

UIManager::~UIManager() {
    Shutdown();
}

bool UIManager::Initialize() {
    if (initialized_) return true;
    
    return InitializeSDL() && InitializeOpenGL() && InitializeImGui();
}

bool UIManager::Initialize(SDL_Window* existing_window) {
    if (initialized_) return true;
    
    // Use existing window and GL context instead of creating new ones
    window_ = existing_window;
    gl_context_ = SDL_GL_GetCurrentContext();
    if (!gl_context_) {
        std::fprintf(stderr, "No current GL context found\n");
        return false;
    }
    std::cout << "Using existing GL context" << std::endl;
    
    return InitializeImGui();
}

bool UIManager::InitializeSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    std::cout << "SDL initialized" << std::endl;
    
    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    return true;
}

bool UIManager::InitializeOpenGL() {
    // Get display bounds for fullscreen borderless window
    SDL_DisplayMode display_mode;
    if (SDL_GetCurrentDisplayMode(0, &display_mode) != 0) {
        std::fprintf(stderr, "SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
        return false;
    }
    
    // Create borderless fullscreen window
    window_ = SDL_CreateWindow("SegmeCam", 
                              0, 0,  // Position at top-left
                              display_mode.w, display_mode.h,  // Full display size
                              SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    std::cout << "SDL borderless fullscreen window created (" << display_mode.w << "x" << display_mode.h << ")" << std::endl;
    
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1); // Enable VSync
    std::cout << "GL context ready" << std::endl;
    
    return true;
}

bool UIManager::InitializeImGui() {
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init("#version 330");
    std::cout << "ImGui initialized" << std::endl;
    
    DrawInitialFrame();
    
    initialized_ = true;
    return true;
}

void UIManager::DrawInitialFrame() {
    // Draw an initial frame so the window appears even before first camera frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 100), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Initializing camera and graph...");
    }
    ImGui::End();
    
    ImGui::Render();
    int idw, idh;
    SDL_GL_GetDrawableSize(window_, &idw, &idh);
    glViewport(0, 0, idw, idh);
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
    std::cout << "Initial frame drawn" << std::endl;
}

void UIManager::InitializePanels(AppState& state, 
                                  CameraManager& camera_mgr, 
                                  EffectsManager& effects_mgr, 
                                  ConfigManager* config_mgr,
                                  ar_filters::ARFilterManager* ar_filter_mgr) {
    // Initialize panels with their dependencies
    auto camera_panel = std::make_unique<CameraPanel>(state, camera_mgr, effects_mgr);
    if (config_mgr) {
        camera_panel->SetConfigManager(config_mgr);
        // Update UI to show the default profile that was loaded at startup
        camera_panel->UpdateDefaultProfileDisplay();
    }
    RegisterPanel(std::move(camera_panel));
    
    RegisterPanel(std::make_unique<BackgroundPanel>(state));
    RegisterPanel(std::make_unique<BeautyPanel>(state, effects_mgr));
    
    // Debug and Status panels (Phase 8 Day 2: Pass ar_filter_mgr to DebugPanel)
    auto debug_panel = std::make_unique<DebugPanel>(state);
    if (ar_filter_mgr) {
        debug_panel->SetARFilterManager(ar_filter_mgr);
    }
    RegisterPanel(std::move(debug_panel));
    RegisterPanel(std::make_unique<StatusPanel>(state));
}

void UIManager::RegisterPanel(std::unique_ptr<UIPanel> panel) {
    if (panel) {
        panels_.push_back(std::move(panel));
    }
}

UIPanel* UIManager::FindPanel(const std::string& name) {
    for (auto& panel : panels_) {
        if (panel->GetName() == name) {
            return panel.get();
        }
    }
    return nullptr;
}

bool UIManager::ProcessEvents(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        
        switch (event.type) {
            case SDL_QUIT:
                HandleQuitEvent(running);
                return false;
                
            case SDL_WINDOWEVENT:
                if (HandleWindowEvent(event, running)) {
                    return false;
                }
                break;
                
            case SDL_KEYDOWN:
                if (HandleKeyEvent(event, running)) {
                    return false;
                }
                break;
                
            case SDL_DROPFILE:
                HandleDroppedFile(event.drop.file);
                break;
                
            default:
                break;
        }
    }
    
    return true;
}

void UIManager::HandleDroppedFile(char* dropped_path) {
    std::cout << "📁 File dropped: " << dropped_path << std::endl;
    dropped_files_.push_back(std::string(dropped_path));
    SDL_free(dropped_path);
}

void UIManager::HandleQuitEvent(bool& running) {
    std::cout << "🛑 SDL_QUIT event received" << std::endl;
    running = false;
}

bool UIManager::HandleWindowEvent(const SDL_Event& event, bool& running) {
    if (event.window.event == SDL_WINDOWEVENT_CLOSE && 
        event.window.windowID == SDL_GetWindowID(window_)) {
        std::cout << "🛑 SDL_WINDOWEVENT_CLOSE event received" << std::endl;
        running = false;
        return true;
    }
    return false;
}

bool UIManager::HandleKeyEvent(const SDL_Event& event, bool& running) {
    if (event.key.keysym.sym == SDLK_ESCAPE) {
        std::cout << "🛑 ESC key pressed" << std::endl;
        running = false;
        return true;
    }
    return false;
}

std::vector<std::string> UIManager::GetDroppedFiles() {
    std::vector<std::string> files = dropped_files_;
    dropped_files_.clear();
    return files;
}

void UIManager::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void UIManager::EndFrame() {
    ImGui::Render();
    
    int draw_w, draw_h;
    SDL_GL_GetDrawableSize(window_, &draw_w, &draw_h);
    glViewport(0, 0, draw_w, draw_h);
    
    // Clear with dark background (video shown in preview window instead)
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render ImGui draw data
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}

void UIManager::RenderUI() {
    // Process deferred PipeWire initialization before rendering UI
    auto* camera_panel = static_cast<CameraPanel*>(FindPanel("Camera"));
    if (camera_panel) {
        camera_panel->ProcessDeferredPipeWireInitialization();
    }
    
    RenderVideoPreview();
    
    if (show_main_window_) {
        RenderMainWindow();
    }
    
    if (show_status_overlay_) {
        RenderStatusOverlay();
    }
}

void UIManager::RenderMainWindow() {
    ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);
    
    // Render all panels
    for (auto& panel : panels_) {
        if (panel && panel->IsVisible()) {
            // Skip status panel (rendered as overlay)
            if (panel->GetName() != "Status") {
                panel->Render();
            }
        }
    }
    
    ImGui::End();
}

void UIManager::RenderVideoPreview() {
    if (!show_video_preview_ || tex_ == 0 || tex_w_ <= 0 || tex_h_ <= 0) {
        return;
    }
    
    // Get window size for fullscreen display
    int draw_w, draw_h;
    GetDrawableSize(draw_w, draw_h);
    
    // Create fullscreen borderless window for video
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)draw_w, (float)draw_h));
    
    ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNavFocus;
    
    if (ImGui::Begin("##VideoBackground", nullptr, window_flags)) {
        // Calculate aspect-preserving display size to fill screen
        float aspect = (float)tex_w_ / tex_h_;
        float screen_aspect = (float)draw_w / draw_h;
        
        float display_w, display_h;
        if (screen_aspect > aspect) {
            // Screen wider than video - scale to screen width
            display_w = (float)draw_w;
            display_h = display_w / aspect;
        } else {
            // Screen taller than video - scale to screen height
            display_h = (float)draw_h;
            display_w = display_h * aspect;
        }
        
        // Center the video
        float offset_x = (draw_w - display_w) * 0.5f;
        float offset_y = (draw_h - display_h) * 0.5f;
        ImGui::SetCursorPos(ImVec2(offset_x, offset_y));
        
        ImGui::Image(reinterpret_cast<void*>(tex_), ImVec2(display_w, display_h));
    }
    ImGui::End();
}

void UIManager::RenderStatusOverlay() {
    // Get drawable size for positioning
    int draw_w, draw_h;
    GetDrawableSize(draw_w, draw_h);
    
    // Position status overlay in top-right corner
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::SetNextWindowPos(ImVec2((float)draw_w - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always);
    
    if (ImGui::Begin("Status##overlay", nullptr, 
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | 
                     ImGuiWindowFlags_NoNav)) {
        
        // Render status panel content
        auto* status_panel = FindPanel("Status");
        if (status_panel && status_panel->IsVisible()) {
            status_panel->Render();
        }
    }
    ImGui::End();
}

// Panel management
void UIManager::ShowPanel(const std::string& panel_name, bool show) {
    auto* panel = FindPanel(panel_name);
    if (panel) {
        panel->SetVisible(show);
    }
}

void UIManager::TogglePanel(const std::string& panel_name) {
    auto* panel = FindPanel(panel_name);
    if (panel) {
        panel->SetVisible(!panel->IsVisible());
    }
}

bool UIManager::IsPanelVisible(const std::string& panel_name) const {
    for (const auto& panel : panels_) {
        if (panel->GetName() == panel_name) {
            return panel->IsVisible();
        }
    }
    return false;
}

// Texture management
void UIManager::UploadTexture(const cv::Mat& rgb) {
    if (rgb.empty()) return;
    
    int w = rgb.cols;
    int h = rgb.rows;
    
    if (tex_ == 0) {
        glGenTextures(1, &tex_);
    }
    
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload RGB data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    
    tex_w_ = w;
    tex_h_ = h;
}

// Window management
void UIManager::SetVSync(bool enabled) {
    SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}

void UIManager::GetDrawableSize(int& w, int& h) {
    SDL_GL_GetDrawableSize(window_, &w, &h);
}

void UIManager::Shutdown() {
    if (!initialized_) return;
    
    std::cout << "Shutting down UI Manager..." << std::endl;
    
    // Clear panels
    panels_.clear();
    
    // Clean up texture
    if (tex_ != 0) {
        glDeleteTextures(1, &tex_);
        tex_ = 0;
    }
    
    // Shutdown ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    
    // Shutdown SDL
    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    
    SDL_Quit();
    
    initialized_ = false;
    std::cout << "UI Manager shutdown completed" << std::endl;
}

void UIManager::RenderVideoBackgroundInternal(GLuint video_texture, int video_width, int video_height, int window_width, int window_height) {
    // Calculate aspect-preserving dimensions to fill window
    float video_aspect = (float)video_width / (float)video_height;
    float window_aspect = (float)window_width / (float)window_height;

    float quad_w, quad_h, quad_x, quad_y;
    if (window_aspect > video_aspect) {
        // Window is wider - scale to height, center horizontally
        quad_h = window_height;
        quad_w = quad_h * video_aspect;
        quad_x = (window_width - quad_w) * 0.5f;
        quad_y = 0;
    } else {
        // Window is taller - scale to width, center vertically
        quad_w = window_width;
        quad_h = quad_w / video_aspect;
        quad_x = 0;
        quad_y = (window_height - quad_h) * 0.5f;
    }

    // Set up orthographic projection for fullscreen quad
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, window_height, 0, -1, 1);  // Y-flipped for texture coordinates
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Enable texturing and proper blending
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, video_texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set white color (full brightness)
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Draw fullscreen textured quad
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(quad_x, quad_y);
    glTexCoord2f(1, 0); glVertex2f(quad_x + quad_w, quad_y);
    glTexCoord2f(1, 1); glVertex2f(quad_x + quad_w, quad_y + quad_h);
    glTexCoord2f(0, 1); glVertex2f(quad_x, quad_y + quad_h);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

} // namespace segmecam