#include "ui_manager.h"
#include <iostream>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

namespace segmecam {

UIManager::~UIManager() {
  if (initialized_) {
    Shutdown();
  }
}

bool UIManager::Initialize() {
  if (initialized_) return true;

  // SDL2 + OpenGL + ImGui setup (desktop GL core profile)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { 
    std::fprintf(stderr, "SDL error: %s\n", SDL_GetError()); 
    return false; 
  }
  std::cout << "SDL initialized" << std::endl;
  
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  // Request desktop GL 3.3 core
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  
  window_ = SDL_CreateWindow("SegmeCam", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                            1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
  SDL_GL_SetSwapInterval(1);
  std::cout << "GL context ready" << std::endl;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
  ImGui_ImplOpenGL3_Init("#version 330");
  std::cout << "ImGui initialized" << std::endl;

  // Draw an initial frame so the window appears even before first camera frame
  ImGui_ImplOpenGL3_NewFrame(); 
  ImGui_ImplSDL2_NewFrame(); 
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(16,16), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(360,100), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse)) {
    ImGui::Text("Initializing camera and graph...");
  }
  ImGui::End();
  ImGui::Render();
  int idw, idh; 
  SDL_GL_GetDrawableSize(window_, &idw, &idh);
  glViewport(0,0,idw,idh); 
  glClearColor(0.06f,0.06f,0.07f,1.0f); 
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window_);
  std::cout << "Initial frame drawn" << std::endl;

  initialized_ = true;
  return true;
}

bool UIManager::ProcessEvents(bool& running) {
  SDL_Event e; 
  bool background_dropped = false;
  
  while (SDL_PollEvent(&e)) {
    ImGui_ImplSDL2_ProcessEvent(&e);
    if (e.type == SDL_QUIT) {
      running = false;
    }
    // Allow users to drag-and-drop an image file to set custom background
    if (e.type == SDL_DROPFILE) {
      // Note: This returns true to indicate a background was dropped
      // The actual processing should be handled by the caller
      background_dropped = true;
      char* dropped_path = e.drop.file; // SDL allocates; caller must SDL_free
      if (dropped_path) {
        // Store the path somewhere accessible to caller, then free
        SDL_free(dropped_path);
      }
    }
  }
  
  return background_dropped;
}

void UIManager::BeginFrame() {
  ImGui_ImplOpenGL3_NewFrame(); 
  ImGui_ImplSDL2_NewFrame(); 
  ImGui::NewFrame();
}

void UIManager::EndFrame() {
  ImGui::Render();
  int idw, idh; 
  SDL_GL_GetDrawableSize(window_, &idw, &idh);
  glViewport(0,0,idw,idh); 
  glClearColor(0.06f,0.06f,0.07f,1.0f); 
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window_);
}

void UIManager::UploadTexture(const cv::Mat& rgb) {
  // Ensure GL interprets tightly packed RGB rows
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  if (tex_ == 0 || tex_w_ != rgb.cols || tex_h_ != rgb.rows) {
    if (tex_) glDeleteTextures(1, &tex_);
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    tex_w_ = rgb.cols; 
    tex_h_ = rgb.rows;
  } else {
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgb.cols, rgb.rows, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
  }
}

void UIManager::SetVSync(bool enabled) {
  SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}

void UIManager::GetDrawableSize(int& w, int& h) {
  SDL_GL_GetDrawableSize(window_, &w, &h);
}

void UIManager::Shutdown() {
  if (!initialized_) return;
  
  ImGui_ImplOpenGL3_Shutdown(); 
  ImGui_ImplSDL2_Shutdown(); 
  ImGui::DestroyContext();
  if (tex_) glDeleteTextures(1, &tex_);
  if (gl_context_) SDL_GL_DeleteContext(gl_context_); 
  if (window_) SDL_DestroyWindow(window_); 
  SDL_Quit();
  
  initialized_ = false;
  tex_ = 0;
  tex_w_ = 0;
  tex_h_ = 0;
  window_ = nullptr;
  gl_context_ = nullptr;
}

} // namespace segmecam