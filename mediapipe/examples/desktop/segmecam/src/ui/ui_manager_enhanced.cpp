#include "include/ui/ui_manager_enhanced.h"
#include "include/ui/ui_panels.h"
#include "app_state.h"
#include "include/camera/camera_manager.h"
#include "src/config/config_manager.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include <GL/gl.h>
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
    window_ = SDL_CreateWindow("SegmeCam", 
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              1280, 720, 
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    std::cout << "SDL window created" << std::endl;
    
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

void UIManager::InitializePanels(AppState& state, CameraManager& camera_mgr, EffectsManager& effects_mgr, ConfigManager* config_mgr) {
    // Initialize panels with their dependencies
    auto camera_panel = std::make_unique<CameraPanel>(state, camera_mgr, effects_mgr);
    if (config_mgr) {
        camera_panel->SetConfigManager(config_mgr);
        // Update UI to show the default profile that was loaded at startup
        camera_panel->UpdateDefaultProfileDisplay();
    }
    RegisterPanel(std::move(camera_panel));
    
    RegisterPanel(std::make_unique<BackgroundPanel>(state));
    RegisterPanel(std::make_unique<BeautyPanel>(state));
    
    // Note: Profile functionality moved to Camera Panel to be after Virtual Webcam section
    // ProfilePanel removed to avoid duplication
    
    RegisterPanel(std::make_unique<DebugPanel>(state));
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
        
        if (event.type == SDL_QUIT) {
            std::cout << "🛑 SDL_QUIT event received" << std::endl;
            running = false;
            return false;
        }
        
        // Handle window events
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE && 
                event.window.windowID == SDL_GetWindowID(window_)) {
                std::cout << "🛑 SDL_WINDOWEVENT_CLOSE event received" << std::endl;
                running = false;
                return false;
            }
        }
        
        // Handle keyboard shortcuts
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                std::cout << "🛑 ESC key pressed" << std::endl;
                running = false;
                return false;
            }
        }
        
        // Handle drag-and-drop files
        if (event.type == SDL_DROPFILE) {
            char* dropped_path = event.drop.file;
            std::cout << "📁 File dropped: " << dropped_path << std::endl;
            dropped_files_.push_back(std::string(dropped_path));
            SDL_free(dropped_path);
        }
    }
    
    return true;
}

std::vector<std::string> UIManager::GetDroppedFiles() {
    std::vector<std::string> files = std::move(dropped_files_);
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
    
    // Don't clear again - main loop already cleared with black background
    // Render ImGui draw data on top of video background
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}

void UIManager::RenderUI() {
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
    
    // Set default position and size for video preview
    ImGui::SetNextWindowPos(preview_window_pos_, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(preview_window_size_, ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Camera Feed", &show_video_preview_, ImGuiWindowFlags_NoCollapse)) {
        // Update position and size from user interaction
        preview_window_pos_ = ImGui::GetWindowPos();
        preview_window_size_ = ImGui::GetWindowSize();
        
        // Calculate aspect-preserving display size
        ImVec2 content_size = ImGui::GetContentRegionAvail();
        float aspect = (float)tex_w_ / tex_h_;
        float display_w = content_size.x;
        float display_h = display_w / aspect;
        
        if (display_h > content_size.y) {
            display_h = content_size.y;
            display_w = display_h * aspect;
        }
        
        // Center the image
        ImVec2 cursor_pos = ImGui::GetCursorPos();
        ImVec2 center_offset((content_size.x - display_w) * 0.5f, (content_size.y - display_h) * 0.5f);
        ImGui::SetCursorPos(ImVec2(cursor_pos.x + center_offset.x, cursor_pos.y + center_offset.y));
        
        ImGui::Image((void*)(intptr_t)tex_, ImVec2(display_w, display_h));
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

} // namespace segmecam