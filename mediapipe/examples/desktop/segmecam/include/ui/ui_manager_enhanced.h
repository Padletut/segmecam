#pragma once

#include <SDL.h>
// Note: Don't include SDL_opengl.h when using epoxy - use epoxy/gl.h instead
#include <epoxy/gl.h>  // Modern OpenGL function loader
#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include <memory>
#include <vector>
#include <string>
#include "ui_panels.h"

// Forward declarations
namespace ar_filters {
    class ARFilterManager;
}

namespace segmecam {

class AppState;
class CameraManager;
class ConfigManager;

// Enhanced UI Manager that coordinates all UI panels
class UIManager {
public:
  UIManager() = default;
  ~UIManager();

  // Initialize SDL, OpenGL, and ImGui
  bool Initialize();
  bool Initialize(SDL_Window* existing_window);
  
  // Initialize UI panels with dependencies (Phase 8 Day 2: Added ar_filter_mgr)
  void InitializePanels(AppState& state, 
                        CameraManager& camera_mgr, 
                        class EffectsManager& effects_mgr, 
                        ConfigManager* config_mgr = nullptr,
                        ar_filters::ARFilterManager* ar_filter_mgr = nullptr);
  
  // Event handling
  bool ProcessEvents(bool& running, ar_filters::ARFilterManager* ar_filter_mgr = nullptr);
  
  // Get and clear any dropped file paths
  std::vector<std::string> GetDroppedFiles();
  
  // Frame rendering
  void BeginFrame();
  void EndFrame();
  
  // Main UI rendering
  void RenderUI();
  void RenderMainWindow();
  void RenderVideoPreview();
  void RenderStatusOverlay();
  
  // Panel management
  void ShowPanel(const std::string& panel_name, bool show = true);
  void TogglePanel(const std::string& panel_name);
  bool IsPanelVisible(const std::string& panel_name) const;
  UIPanel* FindPanel(const std::string& name);
  
  // Texture management
  void UploadTexture(const cv::Mat& rgb);
  unsigned int GetTexture() const { return tex_; }
  int GetTextureWidth() const { return tex_w_; }
  int GetTextureHeight() const { return tex_h_; }
  
  // Window management
  SDL_Window* GetWindow() { return window_; }
  void SetVSync(bool enabled);
  void GetDrawableSize(int& w, int& h);
  
  // UI state
  void SetShowMainWindow(bool show) { show_main_window_ = show; }
  void SetShowVideoPreview(bool show) { show_video_preview_ = show; }
  void SetShowStatusOverlay(bool show) { show_status_overlay_ = show; }
  
  // Cleanup
  void Shutdown();

private:
  // SDL/OpenGL/ImGui setup
  bool InitializeSDL();
  bool InitializeOpenGL();
  bool InitializeImGui();
  void DrawInitialFrame();
  
  // Panel management
  void RegisterPanel(std::unique_ptr<UIPanel> panel);
  
  // Dropped file handling
  void HandleDroppedFile(char* dropped_path);
  
  // Event handling helpers
  void HandleQuitEvent(bool& running);
  bool HandleWindowEvent(const SDL_Event& event, bool& running);
  bool HandleKeyEvent(const SDL_Event& event, bool& running, ar_filters::ARFilterManager* ar_filter_mgr);
  
  // Video background rendering
  void RenderVideoBackgroundInternal(GLuint video_texture, int video_width, int video_height, int window_width, int window_height);
  
  // Window state
  SDL_Window* window_ = nullptr;
  SDL_GLContext gl_context_ = nullptr;
  bool initialized_ = false;
  
  // Texture state
  unsigned int tex_ = 0;
  int tex_w_ = 0;
  int tex_h_ = 0;
  
  // UI panels
  std::vector<std::unique_ptr<UIPanel>> panels_;
  
  // UI visibility state
  bool show_main_window_ = true;
  bool show_video_preview_ = true;  // Enabled - shows video in borderless fullscreen window
  bool show_status_overlay_ = true;
  
  // UI layout state
  ImVec2 main_window_pos_ = ImVec2(16, 16);
  ImVec2 main_window_size_ = ImVec2(400, 600);
  ImVec2 preview_window_pos_ = ImVec2(450, 16);
  ImVec2 preview_window_size_ = ImVec2(640, 480);
  
  // Dropped file handling
  std::vector<std::string> dropped_files_;
};

} // namespace segmecam