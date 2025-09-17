#include "include/render/render_manager.h"

#include <iostream>
#include <cstdio>
#include <GL/gl.h>

namespace segmecam {

RenderManager::RenderManager() {
    // Initialize frame timing
    state_.last_frame_time = SDL_GetTicks();
}

RenderManager::~RenderManager() {
    Cleanup();
}

int RenderManager::Initialize(const RenderConfig& config) {
    config_ = config;
    state_ = RenderState{}; // Reset state
    
    std::cout << "🎨 Initializing Render Manager..." << std::endl;
    
    if (int result = SetupSDL(); result != 0) {
        return result;
    }
    
    if (int result = SetupOpenGL(); result != 0) {
        return result;
    }
    
    if (int result = SetupImGui(); result != 0) {
        return result;
    }
    
    // Get initial drawable size
    SDL_GL_GetDrawableSize(window_, &state_.drawable_width, &state_.drawable_height);
    state_.vsync_enabled = config_.vsync_enabled;
    state_.is_initialized = true;
    
    // Draw initial frame so window appears immediately
    BeginFrame();
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 100), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Initializing camera and MediaPipe...");
    }
    ImGui::End();
    EndFrame();
    
    std::cout << "✅ Render Manager initialized successfully!" << std::endl;
    return 0;
}

int RenderManager::ProcessEvents(bool& should_quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        
        if (event.type == SDL_QUIT) {
            should_quit = true;
            state_.window_should_close = true;
        }
        
        // Handle window resize
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            SDL_GL_GetDrawableSize(window_, &state_.drawable_width, &state_.drawable_height);
        }
    }
    
    return 0;
}

void RenderManager::BeginFrame() {
    if (!state_.is_initialized) return;
    
    UpdatePerformanceStats();
    
    // Start new ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void RenderManager::EndFrame() {
    if (!state_.is_initialized) return;
    
    // Prepare for rendering
    ImGui::Render();
    
    // Update drawable size and set viewport
    SDL_GL_GetDrawableSize(window_, &state_.drawable_width, &state_.drawable_height);
    glViewport(0, 0, state_.drawable_width, state_.drawable_height);
    
    // Clear with dark background
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render background texture if available (before UI)
    RenderBackground(state_.current_texture != 0);
    
    // Render ImGui
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Present frame
    SDL_GL_SwapWindow(window_);
}

void RenderManager::UploadTexture(const cv::Mat& rgb_image) {
    if (!state_.is_initialized || rgb_image.empty()) return;
    
    // Ensure proper format (RGB, 8-bit)
    if (rgb_image.type() != CV_8UC3) {
        std::cerr << "⚠️  RenderManager: Expected CV_8UC3 format for texture upload" << std::endl;
        return;
    }
    
    // Set up OpenGL pixel storage
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    
    // Create or update texture
    if (state_.current_texture == 0 || 
        state_.texture_width != rgb_image.cols || 
        state_.texture_height != rgb_image.rows) {
        
        // Delete old texture if exists
        if (state_.current_texture != 0) {
            glDeleteTextures(1, &state_.current_texture);
        }
        
        // Create new texture
        glGenTextures(1, &state_.current_texture);
        glBindTexture(GL_TEXTURE_2D, state_.current_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        state_.texture_width = rgb_image.cols;
        state_.texture_height = rgb_image.rows;
        
        std::cout << "🖼️  Created texture: " << state_.texture_width << "x" << state_.texture_height << std::endl;
    } else {
        glBindTexture(GL_TEXTURE_2D, state_.current_texture);
    }
    
    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb_image.cols, rgb_image.rows, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_image.data);
}

void RenderManager::RenderBackground(bool show_preview) {
    if (!show_preview || state_.current_texture == 0) return;
    
    // Calculate aspect-preserving size for background display
    float aspect = state_.texture_height > 0 ? 
        (float)state_.texture_width / (float)state_.texture_height : 1.0f;
    
    float display_w = (float)state_.drawable_width;
    float display_h = display_w / aspect;
    
    if (display_h > state_.drawable_height) {
        display_h = (float)state_.drawable_height;
        display_w = display_h * aspect;
    }
    
    // Center the image
    ImVec2 center((float)state_.drawable_width * 0.5f, (float)state_.drawable_height * 0.5f);
    ImVec2 size(display_w, display_h);
    ImVec2 pos(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
    
    // Draw texture as background using ImGui background draw list
    ImVec2 viewport_pos = ImGui::GetMainViewport()->Pos;
    ImVec2 p0(viewport_pos.x + pos.x, viewport_pos.y + pos.y);
    ImVec2 p1(viewport_pos.x + pos.x + size.x, viewport_pos.y + pos.y + size.y);
    
    ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
    bg_draw_list->AddImage((ImTextureID)(intptr_t)state_.current_texture, p0, p1, 
                          ImVec2(0, 0), ImVec2(1, 1));
}

void RenderManager::SetVSync(bool enabled) {
    if (!state_.is_initialized) return;
    
    SDL_GL_SetSwapInterval(enabled ? 1 : 0);
    state_.vsync_enabled = enabled;
    std::cout << "🔄 VSync " << (enabled ? "enabled" : "disabled") << std::endl;
}

void RenderManager::Cleanup() {
    if (!state_.is_initialized) return;
    
    std::cout << "🧹 Cleaning up Render Manager..." << std::endl;
    
    CleanupResources();
    
    state_ = RenderState{}; // Reset state
    window_ = nullptr;
    gl_context_ = nullptr;
    
    std::cout << "✅ Render Manager cleanup completed" << std::endl;
}

int RenderManager::SetupSDL() {
    std::cout << "🚀 Setting up SDL2..." << std::endl;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "❌ SDL Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config_.gl_major_version);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config_.gl_minor_version);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, config_.depth_size);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, config_.stencil_size);
    
    // Create window
    int window_flags = SDL_WINDOW_OPENGL;
    if (config_.resizable) window_flags |= SDL_WINDOW_RESIZABLE;
    if (config_.allow_highdpi) window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    
    window_ = SDL_CreateWindow(
        config_.window_title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config_.window_width, config_.window_height,
        window_flags
    );
    
    if (!window_) {
        std::fprintf(stderr, "❌ SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 2;
    }
    
    std::cout << "✅ SDL2 window created: " << config_.window_width << "x" << config_.window_height << std::endl;
    return 0;
}

int RenderManager::SetupOpenGL() {
    std::cout << "🚀 Setting up OpenGL context..." << std::endl;
    
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::fprintf(stderr, "❌ SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(config_.vsync_enabled ? 1 : 0);
    
    std::cout << "✅ OpenGL context ready" << std::endl;
    return 0;
}

int RenderManager::SetupImGui() {
    std::cout << "🚀 Setting up Dear ImGui..." << std::endl;
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    if (!ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_)) {
        std::fprintf(stderr, "❌ ImGui SDL2 init failed\n");
        return 1;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::fprintf(stderr, "❌ ImGui OpenGL3 init failed\n");
        return 2;
    }
    
    std::cout << "✅ Dear ImGui initialized" << std::endl;
    return 0;
}

void RenderManager::UpdatePerformanceStats() {
    uint32_t current_time = SDL_GetTicks();
    uint32_t delta_time = current_time - state_.last_frame_time;
    
    if (delta_time > 0) {
        state_.fps = 1000.0f / delta_time;
    }
    
    state_.last_frame_time = current_time;
}

void RenderManager::CleanupResources() {
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    
    // Cleanup texture
    if (state_.current_texture != 0) {
        glDeleteTextures(1, &state_.current_texture);
    }
    
    // Cleanup OpenGL context
    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
    }
    
    // Cleanup window
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    
    // Cleanup SDL
    SDL_Quit();
}

} // namespace segmecam