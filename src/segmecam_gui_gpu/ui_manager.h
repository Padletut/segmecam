#pragma once

#include <SDL.h>
#include <SDL_opengl.h>
#include <opencv2/opencv.hpp>

namespace segmecam {

class UIManager {
public:
  UIManager() = default;
  ~UIManager();

  // Initialize SDL, OpenGL, and ImGui
  bool Initialize();
  
  // Event handling
  bool ProcessEvents(bool& running);
  
  // Frame rendering
  void BeginFrame();
  void EndFrame();
  
  // Texture management
  void UploadTexture(const cv::Mat& rgb);
  unsigned int GetTexture() const { return tex_; }
  int GetTextureWidth() const { return tex_w_; }
  int GetTextureHeight() const { return tex_h_; }
  
  // Window management
  SDL_Window* GetWindow() { return window_; }
  void SetVSync(bool enabled);
  void GetDrawableSize(int& w, int& h);
  
  // Cleanup
  void Shutdown();

private:
  SDL_Window* window_ = nullptr;
  SDL_GLContext gl_context_ = nullptr;
  unsigned int tex_ = 0;
  int tex_w_ = 0;
  int tex_h_ = 0;
  bool initialized_ = false;
};

} // namespace segmecam