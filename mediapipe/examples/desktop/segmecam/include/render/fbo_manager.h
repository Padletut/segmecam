// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// FBOManager - Phase 4 Step 2: Framebuffer Object Management
//
// Handles offscreen rendering to framebuffer objects for compositing
// 3D models with video frames. Provides render-to-texture functionality.

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include "absl/status/statusor.h"
#include "absl/status/status.h"

// Use uint32_t instead of OpenGL types to avoid header dependencies
using GLuint = uint32_t;
using GLenum = uint32_t;

namespace segmecam {
namespace render {

class FBOManager {
public:
  // Framebuffer configuration
  struct FBOConfig {
    int width;                    // Framebuffer width in pixels
    int height;                   // Framebuffer height in pixels
    bool use_depth_buffer;        // Whether to attach depth buffer
    bool use_stencil_buffer;      // Whether to attach stencil buffer
    bool use_multisampling;       // Whether to use MSAA
    int sample_count;             // MSAA sample count (if enabled)
    
    FBOConfig()
      : width(1920), height(1080),
        use_depth_buffer(true),
        use_stencil_buffer(false),
        use_multisampling(false),
        sample_count(4) {}
  };
  
  // Framebuffer object data
  struct FBO {
    GLuint framebuffer_id;        // OpenGL framebuffer object ID
    GLuint color_texture_id;      // Color attachment texture ID
    GLuint depth_buffer_id;       // Depth buffer ID (if enabled)
    GLuint stencil_buffer_id;     // Stencil buffer ID (if enabled)
    int width;                    // Framebuffer width
    int height;                   // Framebuffer height
    bool is_multisampled;         // Whether this FBO uses MSAA
    
    FBO() 
      : framebuffer_id(0), color_texture_id(0), 
        depth_buffer_id(0), stencil_buffer_id(0),
        width(0), height(0), is_multisampled(false) {}
    
    // Check if FBO is valid/created
    bool IsValid() const { return framebuffer_id != 0 && width > 0 && height > 0; }
  };
  
  FBOManager();
  ~FBOManager();
  
  // Delete copy/move constructors (OpenGL resources are non-copyable)
  FBOManager(const FBOManager&) = delete;
  FBOManager& operator=(const FBOManager&) = delete;
  FBOManager(FBOManager&&) = delete;
  FBOManager& operator=(FBOManager&&) = delete;
  
  // Create framebuffer object with specified configuration
  // name: Unique identifier for this FBO
  // config: Framebuffer configuration
  absl::StatusOr<FBO> CreateFBO(const std::string& name, const FBOConfig& config);
  
  // Get cached FBO by name
  // Returns invalid FBO if not found
  FBO GetFBO(const std::string& name) const;
  
  // Check if FBO exists
  bool HasFBO(const std::string& name) const;
  
  // Bind framebuffer for rendering
  // fbo: Framebuffer to bind (use invalid FBO to bind default framebuffer)
  void BindFramebuffer(const FBO& fbo) const;
  
  // Bind framebuffer by name
  bool BindFramebuffer(const std::string& name) const;
  
  // Unbind current framebuffer (bind default framebuffer)
  void UnbindFramebuffer() const;
  
  // Clear framebuffer contents
  // color_r, color_g, color_b, color_a: Clear color (0.0-1.0)
  // clear_depth: Whether to clear depth buffer
  // clear_stencil: Whether to clear stencil buffer
  void ClearFramebuffer(float color_r = 0.0f, float color_g = 0.0f, 
                       float color_b = 0.0f, float color_a = 0.0f,
                       bool clear_depth = true, bool clear_stencil = false) const;
  
  // Resize existing FBO
  absl::Status ResizeFBO(const std::string& name, int new_width, int new_height);
  
  // Get color texture from FBO for reading/binding
  GLuint GetColorTexture(const std::string& name) const;
  
  // Copy FBO contents to another FBO or default framebuffer
  // Useful for resolving multisampled FBOs
  absl::Status BlitFramebuffer(const std::string& source_name,
                              const std::string& dest_name = "",
                              bool copy_color = true,
                              bool copy_depth = false) const;
  
  // Read pixels from FBO color attachment
  // Returns raw pixel data (caller must provide buffer)
  absl::Status ReadPixels(const std::string& name, 
                         int x, int y, int width, int height,
                         void* pixel_buffer, size_t buffer_size) const;
  
  // Destroy specific FBO and free GPU resources
  void DestroyFBO(const std::string& name);
  
  // Destroy all FBOs and free GPU resources
  void DestroyAll();
  
  // Get FBO statistics
  size_t GetFBOCount() const { return fbo_cache_.size(); }
  size_t GetTotalGPUMemoryUsed() const;
  
  // Set default clear color for all FBOs
  void SetDefaultClearColor(float r, float g, float b, float a);
  
  // Check OpenGL framebuffer completeness
  static bool CheckFramebufferComplete();
  
  // Get current viewport size
  void GetCurrentViewport(int* x, int* y, int* width, int* height) const;
  
  // Set viewport for rendering
  void SetViewport(int x, int y, int width, int height) const;
  
private:
  // FBO cache: name -> FBO
  std::map<std::string, FBO> fbo_cache_;
  
  // Default clear color
  float default_clear_color_[4];  // RGBA
  
  // Create OpenGL framebuffer object
  absl::StatusOr<FBO> CreateFramebufferObject(const FBOConfig& config) const;
  
  // Create color texture attachment
  absl::StatusOr<GLuint> CreateColorTexture(int width, int height, 
                                           bool multisampled, int samples) const;
  
  // Create depth buffer attachment
  absl::StatusOr<GLuint> CreateDepthBuffer(int width, int height,
                                          bool multisampled, int samples) const;
  
  // Create stencil buffer attachment  
  absl::StatusOr<GLuint> CreateStencilBuffer(int width, int height,
                                            bool multisampled, int samples) const;
  
  // Destroy OpenGL resources for FBO
  void DestroyFBOResources(const FBO& fbo) const;
  
  // Calculate GPU memory usage for an FBO
  size_t CalculateFBOMemory(const FBO& fbo) const;
  
  // Get OpenGL format info for attachments
  struct GLAttachmentFormat {
    GLenum internal_format;  // Internal format (GL_RGBA8, GL_DEPTH_COMPONENT24, etc.)
    GLenum format;          // Data format (GL_RGBA, GL_DEPTH_COMPONENT, etc.)
    GLenum type;            // Data type (GL_UNSIGNED_BYTE, GL_FLOAT, etc.)
  };
  
  GLAttachmentFormat GetColorFormat() const;
  GLAttachmentFormat GetDepthFormat() const;
  GLAttachmentFormat GetStencilFormat() const;
};

} // namespace render
} // namespace segmecam