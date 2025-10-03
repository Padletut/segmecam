// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// OpenGL 3D Renderer - Phase 3 Step 8
// Isolated OpenGL rendering module - NO MediaPipe dependencies
// Uses epoxy/gl.h for modern OpenGL 3.3+ support

#ifndef SEGMECAM_AR_FILTERS_OPENGL_RENDERER_H_
#define SEGMECAM_AR_FILTERS_OPENGL_RENDERER_H_

#include <memory>
#include <string>
#include <vector>

// Include Material for complete type definition
#include "ar_filters/model_loader.h"

// Forward declarations - no OpenGL headers in this public interface
namespace segmecam {
namespace ar_filters {
  struct Model;  // Already defined in model_loader.h but forward declare for clarity
}
namespace render {
  class ShaderProgram;
}
}

namespace segmecam {
namespace ar_filters {

// Render command for 3D model rendering
// Plain C-style struct with no OpenGL types - safe to use anywhere
struct RenderCommand {
  const Model* model;           // Model to render (non-owning pointer)
  float model_matrix[16];       // 4x4 model transformation matrix (column-major)
  Material material;            // Material properties (copied, not referenced)
  bool visible;                 // Visibility flag
};

// OpenGL Renderer - Completely isolated from MediaPipe
// This class uses epoxy/gl.h internally and should never be included
// in files that use MediaPipe's gl_base.h
class OpenGLRenderer {
 public:
  OpenGLRenderer();
  ~OpenGLRenderer();

  // Delete copy/move constructors (OpenGL resources are non-copyable)
  OpenGLRenderer(const OpenGLRenderer&) = delete;
  OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;
  OpenGLRenderer(OpenGLRenderer&&) = delete;
  OpenGLRenderer& operator=(OpenGLRenderer&&) = delete;

  // Initialize OpenGL rendering system
  // Must be called with active OpenGL context
  // Returns true on success, false on failure
  bool Initialize();

  // Check if renderer is initialized and ready
  bool IsInitialized() const { return initialized_; }

  // Update projection matrix when viewport changes
  // width, height: Viewport dimensions in pixels
  void UpdateProjectionMatrix(int width, int height);

  // Render a batch of 3D models
  // commands: Vector of render commands
  // Returns number of models successfully rendered
  int RenderModels(const std::vector<RenderCommand>& commands);

  // Clear depth buffer (call before rendering)
  void ClearDepth();

  // Get/set lighting parameters
  void SetLightDirection(float x, float y, float z);
  void SetLightColor(float r, float g, float b);
  void SetAmbientColor(float r, float g, float b);
  void SetCameraPosition(float x, float y, float z);

  // Get current viewport dimensions
  void GetViewportSize(int* width, int* height) const;

  // Cleanup OpenGL resources
  void Cleanup();

 private:
  bool initialized_;
  int viewport_width_;
  int viewport_height_;

  // OpenGL resources (stored as opaque pointers to avoid header pollution)
  std::unique_ptr<render::ShaderProgram> shader_;
  
  // Lighting parameters
  float light_direction_[3];
  float light_color_[3];
  float ambient_color_[3];
  float camera_position_[3];
  
  // Projection and view matrices (stored as float arrays to avoid glm in header)
  float projection_matrix_[16];
  float view_matrix_[16];

  // Internal helper: Render a single model
  void RenderSingleModel(const RenderCommand& command);
  
  // Internal helper: Setup GL state for rendering
  void SetupGLState();
  
  // Internal helper: Restore GL state after rendering
  void RestoreGLState();
};

} // namespace ar_filters
} // namespace segmecam

#endif // SEGMECAM_AR_FILTERS_OPENGL_RENDERER_H_
