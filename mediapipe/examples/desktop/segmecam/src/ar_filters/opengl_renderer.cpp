// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// OpenGL 3D Renderer Implementation - Phase 3 Step 8
// 
// CRITICAL: This file uses epoxy/gl.h and must NEVER include MediaPipe headers
// Keep this compilation unit completely separate from MediaPipe's GL setup

#include "include/ar_filters/opengl_renderer.h"
#include "include/ar_filters/model_loader.h"
#include "include/ar_filters/texture_manager.h"
#include "include/ar_filters/midas_depth_estimator.h"  // Phase 8 Day 4: MiDaS depth
#include "include/render/shader_program.h"

// OpenGL and math libraries (isolated from MediaPipe)
#include <epoxy/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// OpenCV for face landmarks and PnP
#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>

// Logging
#include "absl/log/log.h"
#include "absl/log/absl_log.h"

#include <iostream>
#include <cstring>

namespace segmecam {
namespace ar_filters {

OpenGLRenderer::OpenGLRenderer()
    : initialized_(false),
      viewport_width_(640),
      viewport_height_(480),
      next_instance_id_(1),
      face_scale_factor_(1.0f),
      head_pose_valid_(false),
      debug_anchors_enabled_(true),     // Enable debug by default
      crown_offset_multiplier_(0.4f),   // Default 40% above forehead
      crown_depth_offset_(0.0f),        // Default no depth offset (INITIALIZE THIS!)
      supersample_fbo_(0),
      supersample_texture_(0),
      supersample_depth_(0),
      supersample_width_(1920),
      supersample_height_(1080),
      use_supersampling_(true),         // Enable supersampling with 2D texture shader compositing
      quad_vao_(0),
      quad_vbo_(0) {
  // Initialize lighting to reasonable defaults
  light_direction_[0] = 0.0f;
  light_direction_[1] = 0.0f;
  light_direction_[2] = -1.0f;
  
  light_color_[0] = 1.0f;
  light_color_[1] = 1.0f;
  light_color_[2] = 1.0f;
  
  ambient_color_[0] = 0.3f;
  ambient_color_[1] = 0.3f;
  ambient_color_[2] = 0.3f;
  
  camera_position_[0] = 0.0f;
  camera_position_[1] = 0.0f;
  camera_position_[2] = 3.0f;  // 3 units back for perspective
  
  // Initialize matrices to identity
  for (int i = 0; i < 16; i++) {
    projection_matrix_[i] = (i % 5 == 0) ? 1.0f : 0.0f; // Identity matrix
    view_matrix_[i] = (i % 5 == 0) ? 1.0f : 0.0f;
  }
  
  // Initialize canonical face model for PnP
  InitializeCanonicalModel();
  
  // Initialize texture manager (will be created in Initialize())
  texture_manager_ = nullptr;

  // Default: reduce landmark-derived Z influence so overlays sit closer to face
  anchor_z_scale_ = 0.25f; // 25% of computed world face width contribution
  anchor_z_bias_meters_ = -0.4f; // Pull models ~2cm towards camera by default
  anchor_z_face_lerp_ = 0.6f; // 60% toward face depth to stabilize distance

  // Face-relative scaling and offsets
  scale_with_face_width_ = true;
  face_width_baseline_m_ = 0.0f;
  face_normal_offset_m_ = 0.0f;
}

OpenGLRenderer::~OpenGLRenderer() {
  Cleanup();
}

bool OpenGLRenderer::Initialize() {
  if (initialized_) {
    ABSL_LOG(WARNING) << "OpenGLRenderer already initialized";
    return true;
  }

  ABSL_LOG(INFO) << "Initializing OpenGL 3D Renderer...";

  // Create shader program
  shader_ = std::make_unique<render::ShaderProgram>();
  
  // Load shaders from files
  const std::string vertex_path = "mediapipe/examples/desktop/segmecam/shaders/model_vertex.glsl";
  const std::string fragment_path = "mediapipe/examples/desktop/segmecam/shaders/model_fragment.glsl";
  
  if (!shader_->LoadFromFiles(vertex_path, fragment_path)) {
    ABSL_LOG(ERROR) << "Failed to load shaders from: " << vertex_path << " and " << fragment_path;
    return false;
  }

  ABSL_LOG(INFO) << "Shaders loaded successfully";

  // Load 2D texture shader for compositing
  texture_shader_ = std::make_unique<render::ShaderProgram>();
  const std::string texture_vertex_path = "mediapipe/examples/desktop/segmecam/shaders/texture_vertex.glsl";
  const std::string texture_fragment_path = "mediapipe/examples/desktop/segmecam/shaders/texture_fragment.glsl";
  
  if (!texture_shader_->LoadFromFiles(texture_vertex_path, texture_fragment_path)) {
    ABSL_LOG(ERROR) << "Failed to load texture shader from: " << texture_vertex_path << " and " << texture_fragment_path;
    return false;
  }
  
  ABSL_LOG(INFO) << "2D texture shader loaded successfully";

  // Initialize texture manager
  texture_manager_ = std::make_unique<TextureManager>();
  texture_manager_->SetDefaultFiltering(true, false);  // Linear filtering, NO mipmaps for max quality
  ABSL_LOG(INFO) << "TextureManager initialized (high quality mode)";  

  // Initialize MiDaS depth estimator (Phase 8 Day 4)
  depth_estimator_ = std::make_unique<MidasDepthEstimator>();
  if (depth_estimator_->Initialize()) {
    ABSL_LOG(INFO) << "✅ MiDaS depth estimator initialized successfully";
    midas_calibration_depth_ = 1.0f;   // Default: 1.0 meter calibration distance
    midas_calibration_inverse_ = 0.5f;  // Will be calibrated on first frame
    midas_calibrated_ = false;
  } else {
    ABSL_LOG(WARNING) << "⚠️  MiDaS depth estimator not available, falling back to face size estimation";
    depth_estimator_ = nullptr;
  }

  // Setup default projection matrix
  UpdateProjectionMatrix(viewport_width_, viewport_height_);

  // Test shader activation
  shader_->Use();
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    ABSL_LOG(ERROR) << "OpenGL error after shader activation: " << error;
    return false;
  }

  // Initialize fullscreen quad for compositing
  InitializeFullscreenQuad();

  // Initialize supersampling framebuffer
  if (use_supersampling_) {
    EnableSupersampling(true, supersample_width_, supersample_height_);
  }

  initialized_ = true;
  ABSL_LOG(INFO) << "OpenGL 3D Renderer initialized successfully";
  return true;
}

void OpenGLRenderer::UpdateProjectionMatrix(int width, int height) {
  if (width <= 0 || height <= 0) {
    ABSL_LOG(WARNING) << "Invalid viewport dimensions: " << width << "x" << height;
    return;
  }

  viewport_width_ = width;
  viewport_height_ = height;
  
  ABSL_LOG(INFO) << "📐 Viewport updated to: " << viewport_width_ << "x" << viewport_height_;

  // Perspective projection (matches 3D world position math based on depth and FOV)
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

  // Identity view (camera at origin looking down -Z). Our world positions are computed in this space.
  glm::mat4 view = glm::mat4(1.0f);

  // Copy to internal storage
  std::memcpy(projection_matrix_, glm::value_ptr(projection), 16 * sizeof(float));
  std::memcpy(view_matrix_, glm::value_ptr(view), 16 * sizeof(float));

  ABSL_LOG(INFO) << "Projection matrix updated for " << width << "x" << height << " (perspective, identity view)";
}

void OpenGLRenderer::SetupGLState() {
  // Enable depth testing for 3D rendering
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  
  // Enable alpha blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Enable face culling (back-face culling)
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);  // Default to counter-clockwise winding (standard for most 3D models)
  
  // Enable multisampling for smoother edges (anti-aliasing)
  glEnable(GL_MULTISAMPLE);
  
  // Enable line smoothing for better edge quality
  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  
  // Enable polygon smoothing hint
  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
  
  ABSL_LOG(INFO) << "OpenGL state configured with anti-aliasing";
}

void OpenGLRenderer::RestoreGLState() {
  // Restore default GL state
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
}

void OpenGLRenderer::ClearDepth() {
  glClear(GL_DEPTH_BUFFER_BIT);
}

int OpenGLRenderer::RenderModels(const std::vector<RenderCommand>& commands) {
  if (!initialized_) {
    ABSL_LOG(ERROR) << "OpenGLRenderer not initialized";
    return 0;
  }

  if (commands.empty()) {
    return 0;
  }

  // Setup GL state
  SetupGLState();

  // Activate shader
  shader_->Use();

  // Set global uniforms (same for all models)
  glm::mat4 projection, view;
  std::memcpy(glm::value_ptr(projection), projection_matrix_, 16 * sizeof(float));
  std::memcpy(glm::value_ptr(view), view_matrix_, 16 * sizeof(float));
  
  shader_->SetMat4("uProjection", glm::value_ptr(projection));
  shader_->SetMat4("uView", glm::value_ptr(view));
  shader_->SetVec3("uCameraPos", camera_position_[0], camera_position_[1], camera_position_[2]);
  shader_->SetVec3("uLightDirection", light_direction_[0], light_direction_[1], light_direction_[2]);
  shader_->SetVec3("uLightColor", light_color_[0], light_color_[1], light_color_[2]);
  shader_->SetVec3("uAmbientColor", ambient_color_[0], ambient_color_[1], ambient_color_[2]);

  // Render each model
  int rendered_count = 0;
  for (const auto& command : commands) {
    if (!command.visible || !command.model) {
      continue;
    }

    RenderSingleModel(command);
    rendered_count++;
  }

  // Check for OpenGL errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    ABSL_LOG(ERROR) << "OpenGL error during rendering: " << error;
  }

  // Restore GL state
  RestoreGLState();

  return rendered_count;
}

void OpenGLRenderer::RenderSingleModel(const RenderCommand& command) {
  const Model* model = command.model;
  if (!model || model->meshes.empty()) {
    return;
  }

  // Convert model matrix from float array to glm::mat4
  glm::mat4 model_matrix;
  std::memcpy(glm::value_ptr(model_matrix), command.model_matrix, 16 * sizeof(float));

  // Calculate normal matrix (inverse transpose for non-uniform scaling)
  glm::mat3 normal_matrix = glm::mat3(glm::transpose(glm::inverse(model_matrix)));

  // Set per-model uniforms
  shader_->SetMat4("uModel", glm::value_ptr(model_matrix));
  shader_->SetMat3("uNormalMatrix", glm::value_ptr(normal_matrix));

  // Set material uniforms
  shader_->SetVec3("uMaterialAmbient", 
                   command.material.ambient[0],
                   command.material.ambient[1], 
                   command.material.ambient[2]);
  shader_->SetVec3("uMaterialDiffuse",
                   command.material.diffuse[0],
                   command.material.diffuse[1],
                   command.material.diffuse[2]);
  shader_->SetVec3("uMaterialSpecular",
                   command.material.specular[0],
                   command.material.specular[1],
                   command.material.specular[2]);
  shader_->SetFloat("uMaterialShininess", command.material.shininess);
  shader_->SetFloat("uMaterialOpacity", command.material.opacity);

  // Handle texture loading and binding
  bool has_texture = false;
  if (!command.material.texture_path.empty() && texture_manager_) {
    // Try to load texture if not already loaded
    auto texture_result = texture_manager_->LoadTexture(command.material.texture_path);
    if (texture_result.ok()) {
      const auto& texture = texture_result.value();
      if (texture.IsValid()) {
        // Bind texture to texture unit 0
        texture_manager_->BindTexture(texture, 0);
        has_texture = true;
        ABSL_LOG(INFO) << "Bound texture: " << command.material.texture_path 
                       << " (" << texture.width << "x" << texture.height << ")";
      }
    } else {
      ABSL_LOG(WARNING) << "Failed to load texture: " << command.material.texture_path 
                        << " - " << texture_result.status().message();
    }
  }
  
  // Set texture uniforms
  shader_->SetBool("uHasTexture", has_texture);
  shader_->SetInt("uTexture", 0);  // Texture unit 0

  // Render each mesh
  for (const auto& mesh : model->meshes) {
    if (mesh.vao == 0) {
      ABSL_LOG(WARNING) << "Mesh has invalid VAO";
      continue;
    }

    // Bind VAO and draw
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
  }
}

void OpenGLRenderer::SetLightDirection(float x, float y, float z) {
  light_direction_[0] = x;
  light_direction_[1] = y;
  light_direction_[2] = z;
}

void OpenGLRenderer::SetLightColor(float r, float g, float b) {
  light_color_[0] = r;
  light_color_[1] = g;
  light_color_[2] = b;
}

void OpenGLRenderer::SetAmbientColor(float r, float g, float b) {
  ambient_color_[0] = r;
  ambient_color_[1] = g;
  ambient_color_[2] = b;
}

void OpenGLRenderer::SetCameraPosition(float x, float y, float z) {
  camera_position_[0] = x;
  camera_position_[1] = y;
  camera_position_[2] = z;
}

void OpenGLRenderer::GetViewportSize(int* width, int* height) const {
  if (width) *width = viewport_width_;
  if (height) *height = viewport_height_;
}

void OpenGLRenderer::Cleanup() {
  if (!initialized_) {
    return;
  }

  ABSL_LOG(INFO) << "Cleaning up OpenGL 3D Renderer...";
  
  // Clean up fullscreen quad
  if (quad_vao_ != 0) {
    glDeleteVertexArrays(1, &quad_vao_);
    quad_vao_ = 0;
  }
  if (quad_vbo_ != 0) {
    glDeleteBuffers(1, &quad_vbo_);
    quad_vbo_ = 0;
  }
  
  // Clean up supersampling framebuffer if it exists
  if (supersample_fbo_ != 0) {
    glDeleteFramebuffers(1, &supersample_fbo_);
    supersample_fbo_ = 0;
  }
  if (supersample_texture_ != 0) {
    glDeleteTextures(1, &supersample_texture_);
    supersample_texture_ = 0;
  }
  if (supersample_depth_ != 0) {
    glDeleteRenderbuffers(1, &supersample_depth_);
    supersample_depth_ = 0;
  }
  
  shader_.reset();
  texture_shader_.reset();
  loaded_models_.clear();
  instances_.clear();
  
  initialized_ = false;
}

void OpenGLRenderer::EnableSupersampling(bool enable, int width, int height) {
  use_supersampling_ = enable;
  supersample_width_ = width;
  supersample_height_ = height;

  // Clean up existing framebuffer if it exists
  if (supersample_fbo_ != 0) {
    glDeleteFramebuffers(1, &supersample_fbo_);
    supersample_fbo_ = 0;
  }
  if (supersample_texture_ != 0) {
    glDeleteTextures(1, &supersample_texture_);
    supersample_texture_ = 0;
  }
  if (supersample_depth_ != 0) {
    glDeleteRenderbuffers(1, &supersample_depth_);
    supersample_depth_ = 0;
  }

  if (!enable) {
    ABSL_LOG(INFO) << "🔍 Supersampling disabled";
    return;
  }

  // Create framebuffer
  glGenFramebuffers(1, &supersample_fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, supersample_fbo_);

  // Create color texture
  glGenTextures(1, &supersample_texture_);
  glBindTexture(GL_TEXTURE_2D, supersample_texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, supersample_texture_, 0);

  // Create depth renderbuffer
  glGenRenderbuffers(1, &supersample_depth_);
  glBindRenderbuffer(GL_RENDERBUFFER, supersample_depth_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, supersample_depth_);

  // Check framebuffer completeness
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    ABSL_LOG(ERROR) << "❌ Supersampling framebuffer incomplete: 0x" << std::hex << status;
    glDeleteFramebuffers(1, &supersample_fbo_);
    glDeleteTextures(1, &supersample_texture_);
    glDeleteRenderbuffers(1, &supersample_depth_);
    supersample_fbo_ = supersample_texture_ = supersample_depth_ = 0;
    use_supersampling_ = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  ABSL_LOG(INFO) << "✨ Supersampling enabled: " << width << "x" << height << " → viewport resolution";
}

void OpenGLRenderer::InitializeFullscreenQuad() {
  // Fullscreen quad vertices: position (x, y) and texture coords (u, v)
  float quad_vertices[] = {
    // positions   // texcoords
    -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
    -1.0f, -1.0f,  0.0f, 0.0f,  // bottom-left
     1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right
    
    -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
     1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right
     1.0f,  1.0f,  1.0f, 1.0f   // top-right
  };

  glGenVertexArrays(1, &quad_vao_);
  glGenBuffers(1, &quad_vbo_);
  
  glBindVertexArray(quad_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
  
  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  
  // Texture coord attribute
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  
  glBindVertexArray(0);
  
  ABSL_LOG(INFO) << "Fullscreen quad initialized for compositing";
}

void OpenGLRenderer::RenderFullscreenQuad() {
  if (quad_vao_ == 0) {
    ABSL_LOG(ERROR) << "Fullscreen quad not initialized";
    return;
  }
  
  if (!texture_shader_) {
    ABSL_LOG(ERROR) << "Texture shader not initialized";
    return;
  }
  
  // Use the 2D texture shader
  texture_shader_->Use();
  
  // Set texture uniform (texture is already bound to GL_TEXTURE0)
  texture_shader_->SetInt("uTexture", 0);
  
  // Render the quad
  glBindVertexArray(quad_vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

// ============================================================================
// PHASE 2: New Feature Implementations
// ============================================================================

// ----------------------------------------------------------------------------
// Step 2.1: FBO Rendering
// ----------------------------------------------------------------------------

bool OpenGLRenderer::RenderToFBO(unsigned int fbo_id, unsigned int texture_id, 
                                  int width, int height) {
  if (!initialized_) {
    ABSL_LOG(ERROR) << "OpenGLRenderer not initialized";
    return false;
  }

  // Bind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
  
  // Set viewport
  glViewport(0, 0, width, height);
  
  // Clear with transparent background
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Render all model instances
  int rendered_count = RenderInstances();
  
  // Unbind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  // Check for errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    ABSL_LOG(ERROR) << "OpenGL error during FBO rendering: " << error;
    return false;
  }
  
  return rendered_count > 0;
}

bool OpenGLRenderer::RenderToExternalTexture(unsigned int texture_id, int width, int height) {
  if (!initialized_) {
    ABSL_LOG(ERROR) << "OpenGLRenderer not initialized";
    return false;
  }

  // Create temporary FBO if needed (static to persist across calls)
  static GLuint temp_fbo = 0;
  if (temp_fbo == 0) {
    glGenFramebuffers(1, &temp_fbo);
    ABSL_LOG(INFO) << "[DEBUG] Created temporary FBO: " << temp_fbo;
  }
  
  // Determine target FBO and viewport size based on supersampling
  GLuint target_fbo = temp_fbo;
  int render_width = width;
  int render_height = height;
  
  if (use_supersampling_ && supersample_fbo_ != 0) {
    // Render to high-res supersampling FBO first
    target_fbo = supersample_fbo_;
    render_width = supersample_width_;
    render_height = supersample_height_;
    ABSL_LOG(INFO) << "🔍 Supersampling: rendering at " << render_width << "x" << render_height 
                   << " → " << width << "x" << height;
  }
  
  // Bind target framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
  
  // For supersampling FBO, attachment is already configured
  // For temp_fbo, attach the video texture
  if (!use_supersampling_ || supersample_fbo_ == 0) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
  }
  
  // Check FBO status
  GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (fbo_status != GL_FRAMEBUFFER_COMPLETE) {
    ABSL_LOG(ERROR) << "[DEBUG] FBO incomplete! Status: 0x" << std::hex << fbo_status;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
  }
  
  ABSL_LOG(INFO) << "[DEBUG] FBO setup complete, rendering " << instances_.size() << " instances";
  
  // Set viewport to render resolution
  glViewport(0, 0, render_width, render_height);
  
  // If supersampling, temporarily update projection matrix for high-res viewport
  float saved_projection[16];
  int saved_viewport_width = viewport_width_;
  int saved_viewport_height = viewport_height_;
  
  if (use_supersampling_ && supersample_fbo_ != 0) {
    std::memcpy(saved_projection, projection_matrix_, 16 * sizeof(float));
    ABSL_LOG(INFO) << "💾 Saved projection matrix for viewport " << viewport_width_ << "x" << viewport_height_;
    
    // Manually update projection matrix WITHOUT changing viewport_width_/height_
    // This prevents aspect ratio calculations from using the wrong resolution
    float aspect = static_cast<float>(render_width) / static_cast<float>(render_height);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    std::memcpy(projection_matrix_, glm::value_ptr(projection), 16 * sizeof(float));
    
    // Temporarily set viewport dimensions for any calculations during rendering
    viewport_width_ = render_width;
    viewport_height_ = render_height;
    
    ABSL_LOG(INFO) << "🔄 Updated projection matrix to supersampling resolution: " << render_width << "x" << render_height;
    
    // STRATEGY: Render AR filters at high-res with transparent background
    // Then downsample and composite onto original camera video
    // This avoids upscaling the camera feed which would introduce blur
    
    // Bind supersample FBO for rendering
    glBindFramebuffer(GL_FRAMEBUFFER, supersample_fbo_);
    
    // Clear to transparent black (alpha = 0) - AR filters will be rendered on transparent background
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Restore normal clear color for future operations
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    ABSL_LOG(INFO) << "🎨 Rendering AR at " << render_width << "x" << render_height 
                   << " on transparent background";
  }
  
  // DON'T clear when not supersampling - we want to render AR filters ON TOP of existing video
  // (When supersampling, we already cleared to transparent above)
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Enable depth testing so filters occlude properly
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  
  // Render all model instances
  int rendered_count = RenderInstances();
  
  // Restore original projection matrix and viewport if supersampling
  if (use_supersampling_ && supersample_fbo_ != 0) {
    std::memcpy(projection_matrix_, saved_projection, 16 * sizeof(float));
    viewport_width_ = saved_viewport_width;
    viewport_height_ = saved_viewport_height;
    ABSL_LOG(INFO) << "🔄 Restored projection matrix to " << viewport_width_ << "x" << viewport_height_;
  }
  
  // If supersampling, composite high-res AR onto original camera video
  if (use_supersampling_ && supersample_fbo_ != 0) {
    // Bind temp_fbo with video texture for rendering
    glBindFramebuffer(GL_FRAMEBUFFER, temp_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
    
    // Set viewport to final output resolution
    glViewport(0, 0, width, height);
    
    // Disable depth test for 2D composite
    glDisable(GL_DEPTH_TEST);
    
    // Enable blending with premultiplied alpha
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    // Bind the high-res AR texture from supersample FBO
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, supersample_texture_);
    
    // Render fullscreen quad with the AR texture
    // This will blend the downsampled AR onto the camera video
    RenderFullscreenQuad();
    
    // Restore GL state
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    
    ABSL_LOG(INFO) << "✨ Composited high-res AR " << render_width << "x" << render_height 
                   << " → " << width << "x" << height;
  }
  
  // Unbind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  // Check for errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    ABSL_LOG(ERROR) << "OpenGL error during external texture rendering: " << error;
    return false;
  }
  
  ABSL_LOG(INFO) << "[DEBUG] Rendered " << rendered_count << " instances to external texture";
  
  return rendered_count > 0;
}

// ----------------------------------------------------------------------------
// Step 2.2: Model Instance Management
// ----------------------------------------------------------------------------

std::string OpenGLRenderer::LoadModel(const std::string& path) {
  // Check if already loaded
  auto it = loaded_models_.find(path);
  if (it != loaded_models_.end()) {
    ABSL_LOG(INFO) << "Model already loaded: " << path;
    return path;  // Return path as model ID
  }
  
  // Load model using ModelLoader
  ABSL_LOG(INFO) << "[DEBUG] LoadModel called: " << path;
  ModelLoader loader;
  auto model_result = loader.LoadModel(path);
  if (!model_result.ok()) {
    ABSL_LOG(ERROR) << "Failed to load model: " << path << " - " << model_result.status();
    return "";
  }
  
  // Cache model
  auto model = std::make_unique<Model>(std::move(*model_result));
  ABSL_LOG(INFO) << "[DEBUG] Model loaded with " << model->meshes.size() << " meshes";
  for (size_t i = 0; i < model->meshes.size(); i++) {
    ABSL_LOG(INFO) << "[DEBUG]   Mesh " << i << ": " 
                   << model->meshes[i].index_count << " indices, VAO=" 
                   << model->meshes[i].vao;
  }
  loaded_models_[path] = std::move(model);
  ABSL_LOG(INFO) << "Model loaded successfully: " << path;
  
  return path;  // Return path as model ID
}

bool OpenGLRenderer::SetModelTexture(const std::string& model_id, const std::string& texture_path) {
  // Check if model exists
  auto model_it = loaded_models_.find(model_id);
  if (model_it == loaded_models_.end()) {
    ABSL_LOG(ERROR) << "Model not found: " << model_id;
    return false;
  }
  
  Model* model = model_it->second.get();
  
  // Override texture path for all materials in the model
  for (auto& material_pair : model->materials) {
    Material& material = material_pair.second;
    material.texture_path = texture_path;
    material.texture_id = 0;  // Reset texture ID to force reload
    ABSL_LOG(INFO) << "Overridden texture for material '" << material_pair.first 
                   << "' to: " << texture_path;
  }
  
  return true;
}

bool OpenGLRenderer::SetModelOpacityMap(const std::string& model_id, const std::string& opacity_map_path) {
  auto model_it = loaded_models_.find(model_id);
  if (model_it == loaded_models_.end()) {
    ABSL_LOG(ERROR) << "Model not found: " << model_id;
    return false;
  }
  
  Model* model = model_it->second.get();
  for (auto& material_pair : model->materials) {
    material_pair.second.opacity_map_path = opacity_map_path;
    ABSL_LOG(INFO) << "Overridden opacity map for material '" << material_pair.first 
                   << "' to: " << opacity_map_path;
  }
  return true;
}

bool OpenGLRenderer::SetModelEmissiveMap(const std::string& model_id, const std::string& emissive_map_path) {
  auto model_it = loaded_models_.find(model_id);
  if (model_it == loaded_models_.end()) {
    ABSL_LOG(ERROR) << "Model not found: " << model_id;
    return false;
  }
  
  Model* model = model_it->second.get();
  for (auto& material_pair : model->materials) {
    material_pair.second.emissive_map_path = emissive_map_path;
    ABSL_LOG(INFO) << "Overridden emissive map for material '" << material_pair.first 
                   << "' to: " << emissive_map_path;
  }
  return true;
}

std::string OpenGLRenderer::CreateInstance(
    const std::string& model_id,
    const std::string& anchor,
    const glm::vec3& offset,
    const glm::vec3& rotation,
    const glm::vec3& scale,
    bool flip_z
) {
  // Check if model exists
  auto model_it = loaded_models_.find(model_id);
  if (model_it == loaded_models_.end()) {
    ABSL_LOG(ERROR) << "Model not found: " << model_id;
    return "";
  }
  
  // Generate unique instance ID
  std::string instance_id = "instance_" + std::to_string(next_instance_id_++);
  
  // Create instance
  ModelInstance instance;
  instance.id = instance_id;
  instance.model_path = model_id;
  instance.anchor_name = anchor;
  instance.offset = offset;
  instance.rotation_euler = rotation;
  instance.scale = scale;
  instance.flip_z = flip_z;
  instance.visible = true;
  instance.model_ptr = model_it->second.get();
  instance.transform_dirty = true;
  
  // Store instance
  instances_[instance_id] = instance;
  
  ABSL_LOG(INFO) << "Created model instance: " << instance_id 
                 << " (model=" << model_id << ", anchor=" << anchor << ")";
  
  return instance_id;
}

void OpenGLRenderer::SetInstanceVisible(const std::string& instance_id, bool visible) {
  auto it = instances_.find(instance_id);
  if (it != instances_.end()) {
    it->second.visible = visible;
  }
}

void OpenGLRenderer::SetInstanceOffset(const std::string& instance_id, const glm::vec3& offset) {
  auto it = instances_.find(instance_id);
  if (it != instances_.end()) {
    it->second.offset = offset;
    it->second.transform_dirty = true;
  }
}

void OpenGLRenderer::SetInstanceRotation(const std::string& instance_id, const glm::vec3& rotation) {
  auto it = instances_.find(instance_id);
  if (it != instances_.end()) {
    it->second.rotation_euler = rotation;
    it->second.transform_dirty = true;
  }
}

void OpenGLRenderer::SetInstanceScale(const std::string& instance_id, const glm::vec3& scale) {
  auto it = instances_.find(instance_id);
  if (it != instances_.end()) {
    it->second.scale = scale;
    it->second.transform_dirty = true;
  }
}

void OpenGLRenderer::RemoveInstance(const std::string& instance_id) {
  instances_.erase(instance_id);
  transform_caches_.erase(instance_id);
  ABSL_LOG(INFO) << "Removed model instance: " << instance_id;
}

void OpenGLRenderer::ClearAllInstances() {
  instances_.clear();
  transform_caches_.clear();
  ABSL_LOG(INFO) << "Cleared all model instances";
}

// ----------------------------------------------------------------------------
// Step 2.3: Face Landmark Integration
// ----------------------------------------------------------------------------

void OpenGLRenderer::UpdateFaceLandmarks(const std::vector<cv::Point3f>& landmarks) {
  face_landmarks_ = landmarks;
  ABSL_LOG(INFO) << "[DEBUG] UpdateFaceLandmarks called with " << landmarks.size() << " points";
  
  // Calculate face scale from eye distance
  if (landmarks.size() > 263) {
    cv::Point3f left_eye = landmarks[33];
    cv::Point3f right_eye = landmarks[263];
    float eye_distance = cv::norm(left_eye - right_eye);
    face_scale_factor_ = eye_distance * 10.0f;  // Scale to reasonable size
    ABSL_LOG(INFO) << "[DEBUG] Face scale factor: " << face_scale_factor_;
  }
}

// Phase 8 Day 4: Update face landmarks WITH current frame for MiDaS depth
void OpenGLRenderer::UpdateFaceLandmarksWithFrame(const std::vector<cv::Point3f>& landmarks,
                                                   const cv::Mat& frame_rgb) {
  // Update landmarks first
  UpdateFaceLandmarks(landmarks);
  
  // Store current frame for MiDaS depth estimation
  if (!frame_rgb.empty()) {
    current_frame_rgb_ = frame_rgb.clone();
    ABSL_LOG(INFO) << "[MIDAS] Frame stored for depth estimation: " 
                   << frame_rgb.cols << "x" << frame_rgb.rows;
  }
}

void OpenGLRenderer::RecalibrateMiDasDepth(float known_distance_meters) {
  if (!depth_estimator_ || !depth_estimator_->IsLoaded()) {
    ABSL_LOG(WARNING) << "[MIDAS CALIBRATION] Depth estimator not available";
    return;
  }
  
  if (current_frame_rgb_.empty() || face_landmarks_.empty()) {
    ABSL_LOG(WARNING) << "[MIDAS CALIBRATION] No frame or landmarks available";
    return;
  }
  
  // Get nose tip position for depth sampling
  cv::Point3f nose_tip = face_landmarks_[1];  // Nose tip landmark
  
  // Estimate inverse depth at current position
  float inverse_depth = depth_estimator_->EstimateFaceDepth(
      current_frame_rgb_, 
      nose_tip.x,  // Normalized 0-1
      nose_tip.y,  // Normalized 0-1
      15  // Sample radius in pixels
  );
  
  if (inverse_depth > 0.0f) {
    midas_calibration_inverse_ = inverse_depth;
    midas_calibration_depth_ = known_distance_meters;
    midas_calibrated_ = true;
    
    ABSL_LOG(INFO) << "[MIDAS CALIBRATION] ✅ Calibrated at " << known_distance_meters << "m "
                   << "(inverse=" << inverse_depth << ")";
  } else {
    ABSL_LOG(ERROR) << "[MIDAS CALIBRATION] ❌ Failed to estimate depth";
  }
}

cv::Point3f OpenGLRenderer::GetAnchorPosition(const std::string& anchor_name) const {
  if (face_landmarks_.empty()) {
    return cv::Point3f(0.0f, 0.0f, 0.0f);
  }
  
  // Implement 9 anchor points based on MediaPipe face landmarks
  if (anchor_name == "nose_bridge") {
    // Average of landmarks 6, 197, 195
    return (face_landmarks_[6] + face_landmarks_[197] + face_landmarks_[195]) / 3.0f;
  }
  else if (anchor_name == "forehead") {
    // Landmark 10
    return face_landmarks_[10];
  }
  else if (anchor_name == "head_crown") {
    // Top of head - apply offset with vertical position compensation
    cv::Point3f crown = face_landmarks_[10];  // Start with forehead
    cv::Point3f forehead_original = face_landmarks_[10];  // Keep original for clamping
    
    // Calculate how high in frame we are (0 = top, 1 = bottom in normalized space)
    // When forehead.y is small (high in frame), we need MORE offset
    float vertical_position = crown.y;  // 0.0 = top of frame, 1.0 = bottom
    
    // Increase offset when higher in frame (smaller Y value)
    // This compensates for perspective distortion at extreme positions
    float position_compensation = 1.0f + (0.5f - vertical_position) * 2.0f;  // 2x at top, 1x at center, 0x at bottom
    position_compensation = std::max(0.5f, std::min(3.0f, position_compensation));  // Clamp 0.5x to 3x
    
    // Apply offset with compensation
    const float BASE_OFFSET = 0.05f;  // 5% base offset
    float offset = BASE_OFFSET * crown_offset_multiplier_ * position_compensation;
    crown.y -= offset;
    
    // CRITICAL: Ensure crown never goes below (higher Y value than) forehead
    // In normalized space: Y=0 is top, Y=1 is bottom, so crown.y must be <= forehead.y
    if (crown.y > forehead_original.y) {
      crown.y = forehead_original.y;
    }
    
    return crown;
  }
  else if (anchor_name == "left_ear") {
    // Average of 234, 127, 162
    return (face_landmarks_[234] + face_landmarks_[127] + face_landmarks_[162]) / 3.0f;
  }
  else if (anchor_name == "right_ear") {
    // Average of 454, 356, 389
    return (face_landmarks_[454] + face_landmarks_[356] + face_landmarks_[389]) / 3.0f;
  }
  else if (anchor_name == "left_temple") {
    // Temple point near left ear for glasses attachment
    // MATCHES TransformCalculator: landmarks 139, 127, 162
    return (face_landmarks_[139] + face_landmarks_[127] + face_landmarks_[162]) / 3.0f;
  }
  else if (anchor_name == "right_temple") {
    // Temple point near right ear for glasses attachment
    // MATCHES TransformCalculator: landmarks 368, 356, 389
    return (face_landmarks_[368] + face_landmarks_[356] + face_landmarks_[389]) / 3.0f;
  }
  else if (anchor_name == "chin") {
    // Landmark 152
    return face_landmarks_[152];
  }
  else if (anchor_name == "left_eye") {
    // Average of 33, 133, 160, 159, 158, 157
    return (face_landmarks_[33] + face_landmarks_[133] + face_landmarks_[160] +
            face_landmarks_[159] + face_landmarks_[158] + face_landmarks_[157]) / 6.0f;
  }
  else if (anchor_name == "right_eye") {
    // Average of 362, 263, 387, 386, 385, 384
    return (face_landmarks_[362] + face_landmarks_[263] + face_landmarks_[387] +
            face_landmarks_[386] + face_landmarks_[385] + face_landmarks_[384]) / 6.0f;
  }
  else if (anchor_name == "mouth") {
    // Average of 61, 291
    return (face_landmarks_[61] + face_landmarks_[291]) / 2.0f;
  }
  else if (anchor_name == "center") {
    // Midpoint between eyes
    cv::Point3f left_eye = (face_landmarks_[33] + face_landmarks_[133]) / 2.0f;
    cv::Point3f right_eye = (face_landmarks_[362] + face_landmarks_[263]) / 2.0f;
    return (left_eye + right_eye) / 2.0f;
  }
  
  ABSL_LOG(WARNING) << "Unknown anchor name: " << anchor_name;
  return cv::Point3f(0.0f, 0.0f, 0.0f);
}

// ----------------------------------------------------------------------------
// Step 2.4: Transform Smoothing
// ----------------------------------------------------------------------------

glm::mat4 OpenGLRenderer::GetSmoothedTransform(const glm::mat4& current, TransformCache& cache) {
  // On first frame, just use current transform without smoothing
  if (cache.first_frame) {
    cache.previous_mvp = current;
    cache.first_frame = false;
    return current;
  }
  
  // Element-wise linear interpolation for matrix smoothing
  glm::mat4 result;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      result[i][j] = cache.previous_mvp[i][j] * (1.0f - cache.smoothing_alpha) + 
                     current[i][j] * cache.smoothing_alpha;
    }
  }
  cache.previous_mvp = result;
  return result;
}

// ----------------------------------------------------------------------------
// Phase 3: Head Pose Tracking (PnP Algorithm)
// ----------------------------------------------------------------------------

void OpenGLRenderer::InitializeCanonicalModel() {
  // MediaPipe canonical face landmarks (6 key points for PnP)
  // These are in millimeters relative to nose tip
  canonical_face_model_ = {
    cv::Point3f(0.0f,    0.0f,    0.0f),     // Nose tip (landmark 1)
    cv::Point3f(0.0f,   -63.6f,   -12.5f),   // Chin (landmark 152)
    cv::Point3f(-43.3f,  32.7f,   -26.0f),   // Left eye left corner (landmark 33)
    cv::Point3f(43.3f,   32.7f,   -26.0f),   // Right eye right corner (landmark 263)
    cv::Point3f(-28.9f, -28.9f,   -24.1f),   // Left mouth corner (landmark 61)
    cv::Point3f(28.9f,  -28.9f,   -24.1f)    // Right mouth corner (landmark 291)
  };
  
  // Initialize camera matrix (will be updated with actual dimensions)
  camera_matrix_ = cv::Mat::eye(3, 3, CV_64F);
  
  // No lens distortion (assumed for webcam)
  dist_coeffs_ = cv::Mat::zeros(4, 1, CV_64F);
}

HeadPose OpenGLRenderer::CalculateHeadPose(int image_width, int image_height) {
  HeadPose pose;
  
  if (face_landmarks_.empty() || face_landmarks_.size() < 468) {
    pose.confidence = 0.0f;
    head_pose_valid_ = false;
    return pose;
  }
  
  // Update camera intrinsics with better focal length estimation
  // Typical webcam horizontal FOV is ~60-70 degrees
  // Formula: focal_length = (image_width / 2) / tan(horizontal_fov / 2)
  const float horizontal_fov_degrees = 63.0f;  // Typical webcam FOV (matching FaceGeometry)
  const float horizontal_fov_radians = horizontal_fov_degrees * M_PI / 180.0f;
  float focal_length = (static_cast<float>(image_width) / 2.0f) / std::tan(horizontal_fov_radians / 2.0f);
  
  cv::Point2f center(image_width / 2.0f, image_height / 2.0f);
  camera_matrix_.at<double>(0, 0) = focal_length;
  camera_matrix_.at<double>(1, 1) = focal_length;
  camera_matrix_.at<double>(0, 2) = center.x;
  camera_matrix_.at<double>(1, 2) = center.y;
  
  // Select same 6 landmarks from detected points
  std::vector<cv::Point2f> image_points = {
    cv::Point2f(face_landmarks_[1].x * image_width, face_landmarks_[1].y * image_height),
    cv::Point2f(face_landmarks_[152].x * image_width, face_landmarks_[152].y * image_height),
    cv::Point2f(face_landmarks_[33].x * image_width, face_landmarks_[33].y * image_height),
    cv::Point2f(face_landmarks_[263].x * image_width, face_landmarks_[263].y * image_height),
    cv::Point2f(face_landmarks_[61].x * image_width, face_landmarks_[61].y * image_height),
    cv::Point2f(face_landmarks_[291].x * image_width, face_landmarks_[291].y * image_height)
  };
  
  // Solve PnP
  cv::Mat rotation_vec, translation_vec;
  bool success = cv::solvePnP(
      canonical_face_model_,
      image_points,
      camera_matrix_,
      dist_coeffs_,
      rotation_vec,
      translation_vec,
      false,
      cv::SOLVEPNP_ITERATIVE
  );
  
  if (!success) {
    pose.confidence = 0.0f;
    head_pose_valid_ = false;
    ABSL_LOG(WARNING) << "[DEBUG] PnP solve FAILED - using fallback";
    return pose;
  }
  
  // **CRITICAL: Validate PnP output before using it**
  // PnP can succeed but return garbage when landmarks are unreliable
  double tx = translation_vec.at<double>(0);  // X: horizontal position
  double ty = translation_vec.at<double>(1);  // Y: vertical position
  double tz = translation_vec.at<double>(2);  // Z: depth
  
  // Validate all axes - face should be within reasonable bounds
  bool depth_valid = (tz > 300.0 && tz < 3000.0);        // Depth: 30cm - 3m
  bool horizontal_valid = (std::abs(tx) < 2000.0);       // Horizontal: ±2m from center
  bool vertical_valid = (std::abs(ty) < 2000.0);         // Vertical: ±2m from center
  
  bool pnp_valid = depth_valid && horizontal_valid && vertical_valid;
  
  if (!pnp_valid) {
    static int invalid_count = 0;
    if (invalid_count < 5 || invalid_count % 30 == 0) {
      ABSL_LOG(WARNING) << "[PnP VALIDATION FAILED] tvec=[" << tx << ", " << ty << ", " << tz << "] "
                        << "(depth_ok=" << depth_valid << ", horiz_ok=" << horizontal_valid 
                        << ", vert_ok=" << vertical_valid << ") - using fallback";
    }
    invalid_count++;
    pose.confidence = 0.0f;
    head_pose_valid_ = false;
    return pose;
  }
  
  static int pnp_count = 0;
  if (pnp_count < 10 || pnp_count % 30 == 0) {
    ABSL_LOG(INFO) << "[PNP SUCCESS] tvec: [" << translation_vec.at<double>(0) 
                   << ", " << translation_vec.at<double>(1)
                   << ", " << translation_vec.at<double>(2) << "]"
                   << " (depth=" << (translation_vec.at<double>(2) / 1000.0) << "m)";
  }
  pnp_count++;
  
  // Convert rotation vector to matrix
  cv::Mat rotation_mat;
  cv::Rodrigues(rotation_vec, rotation_mat);
  
  // Extract Euler angles (ZYX convention)
  double sy = std::sqrt(rotation_mat.at<double>(0,0) * rotation_mat.at<double>(0,0) +
                        rotation_mat.at<double>(1,0) * rotation_mat.at<double>(1,0));
  
  pose.euler_angles.x = std::atan2(rotation_mat.at<double>(2,1), rotation_mat.at<double>(2,2));  // Pitch
  pose.euler_angles.y = std::atan2(-rotation_mat.at<double>(2,0), sy);  // Yaw
  pose.euler_angles.z = std::atan2(rotation_mat.at<double>(1,0), rotation_mat.at<double>(0,0));  // Roll
  
  // Convert rotation matrix to quaternion (CORRECT METHOD)
  // Use glm::mat3 constructor from OpenCV rotation matrix
  glm::mat3 glm_rotation_mat(
      rotation_mat.at<double>(0,0), rotation_mat.at<double>(0,1), rotation_mat.at<double>(0,2),
      rotation_mat.at<double>(1,0), rotation_mat.at<double>(1,1), rotation_mat.at<double>(1,2),
      rotation_mat.at<double>(2,0), rotation_mat.at<double>(2,1), rotation_mat.at<double>(2,2)
  );
  pose.rotation = glm::quat_cast(glm_rotation_mat);
  
  // Translation
  pose.translation = glm::vec3(
      translation_vec.at<double>(0),
      translation_vec.at<double>(1),
      translation_vec.at<double>(2)
  );
  
  pose.confidence = 1.0f;
  current_head_pose_ = pose;
  head_pose_valid_ = true;
  
  return pose;
}

// ----------------------------------------------------------------------------
// Instance Rendering with Head Pose
// ----------------------------------------------------------------------------

glm::mat4 OpenGLRenderer::CalculateInstanceTransform(const ModelInstance& instance, 
                                                      const HeadPose& head_pose) {
  // Get anchor position in normalized coordinates [0-1]
  cv::Point3f anchor = GetAnchorPosition(instance.anchor_name);
  
  // **MIDAS DEPTH ESTIMATION - Neural depth estimation with PnP rotation**
  // Phase 8 Day 4: Use MiDaS trained model for accurate monocular depth
  // Falls back to face size heuristic if MiDaS not available
  
  float depth;
  glm::vec3 face_offset;
  
  if (depth_estimator_ && depth_estimator_->IsLoaded() && !face_landmarks_.empty() && !current_frame_rgb_.empty()) {
    // **USE MIDAS NEURAL DEPTH ESTIMATION!**
    cv::Point3f nose_tip = face_landmarks_[1];  // Nose tip landmark
    
    // Estimate depth from MiDaS
    float midas_inverse_depth = depth_estimator_->EstimateFaceDepth(
        current_frame_rgb_, 
        nose_tip.x,  // Normalized 0-1
        nose_tip.y,  // Normalized 0-1
        15  // Sample radius in pixels
    );
    
    if (midas_inverse_depth > 0.0f) {
      // Convert inverse depth to metric depth
      // First frame: auto-calibrate assuming 1.0m distance
      if (!midas_calibrated_) {
        midas_calibration_inverse_ = midas_inverse_depth;
        midas_calibration_depth_ = 1.0f;  // Assume 1.0m on first frame
        midas_calibrated_ = true;
        ABSL_LOG(INFO) << "[MIDAS] Auto-calibrated: inverse=" << midas_calibration_inverse_ 
                       << " at depth=1.0m";
      }
      
      // Convert to metric depth using calibration
      depth = depth_estimator_->InverseDepthToMeters(
          midas_inverse_depth,
          midas_calibration_depth_,
          midas_calibration_inverse_
      );
      
      // Clamp to reasonable range
      depth = std::clamp(depth, 0.3f, 3.0f);
      
      face_offset = glm::vec3(0.0f, 0.0f, 0.0f);
      
      // Log MiDaS depth periodically
      static int midas_log_count = 0;
      if (midas_log_count < 10 || midas_log_count % 30 == 0) {
        auto perf = depth_estimator_->GetPerformanceStats();
        ABSL_LOG(INFO) << "[MIDAS DEPTH] inverse=" << midas_inverse_depth 
                       << ", metric=" << depth << "m"
                       << ", inference=" << perf.average_inference_time_ms << "ms"
                       << ", provider=" << (perf.using_cuda ? "CUDA" : "CPU");
      }
      midas_log_count++;
    } else {
      // MiDaS failed, fall back to face width
      ABSL_LOG(WARNING) << "[MIDAS] Depth estimation failed, using face width fallback";
      depth = 1.0f;
      face_offset = glm::vec3(0.0f, 0.0f, 0.0f);
    }
    
  } else if (depth_estimator_ && depth_estimator_->IsLoaded() && current_frame_rgb_.empty()) {
    // MiDaS available but no frame yet - fallback to face width
    if (face_landmarks_.size() > 263) {
      cv::Point3f left_eye = face_landmarks_[33];
      cv::Point3f right_eye = face_landmarks_[263];
      float face_width_screen = std::abs(right_eye.x - left_eye.x);
      
      const float reference_face_width = 0.10f;
      const float reference_depth = 1.0f;
      
      if (face_width_screen > 0.01f) {
        depth = (reference_face_width / face_width_screen) * reference_depth;
        depth = std::clamp(depth, 0.3f, 3.0f);
      } else {
        depth = 1.0f;
      }
    } else {
      depth = 1.0f;
    }
    
    face_offset = glm::vec3(0.0f, 0.0f, 0.0f);
    
    static int no_frame_count = 0;
    if (no_frame_count < 5) {
      ABSL_LOG(WARNING) << "[MIDAS] No frame available yet, using face width fallback";
    }
    no_frame_count++;
    
  } else if (!face_landmarks_.empty() && face_landmarks_.size() > 263) {
    // Fallback: Face width estimation (original hybrid method)
    cv::Point3f left_eye = face_landmarks_[33];
    cv::Point3f right_eye = face_landmarks_[263];
    float face_width_screen = std::abs(right_eye.x - left_eye.x);  // Normalized 0-1
    
    // Use PnP depth as baseline, but scale it by face size
    float pnp_depth = 0.891f;  // PnP baseline (will be constant)
    if (head_pose.confidence > 0.5f && head_pose_valid_) {
      pnp_depth = head_pose.translation.z / 1000.0f;  // mm to meters
    }
    
    // Scale depth inversely with face width
    // Reference: eye distance of ~0.10 (10% of screen) = 1.0m depth
    const float reference_face_width = 0.10f;  // Eye distance at 1.0m
    const float reference_depth = 1.0f;
    
    if (face_width_screen > 0.01f) {
      // Depth is inversely proportional to apparent face size
      depth = (reference_face_width / face_width_screen) * reference_depth;
      depth = std::clamp(depth, 0.3f, 3.0f);
    } else {
      depth = pnp_depth;  // Fallback to raw PnP
    }
    
    face_offset = glm::vec3(0.0f, 0.0f, 0.0f);
    
    static int depth_count = 0;
    if (depth_count < 10 || depth_count % 30 == 0) {
      ABSL_LOG(INFO) << "[HYBRID DEPTH] face_width=" << face_width_screen 
                     << ", pnp_baseline=" << pnp_depth << "m"
                     << ", scaled_depth=" << depth << "m";
    }
    depth_count++;
  } else if (!face_landmarks_.empty() && face_landmarks_.size() > 152) {
    // Fallback to face height if eye landmarks not available
    cv::Point3f forehead = face_landmarks_[10];
    cv::Point3f chin = face_landmarks_[152];
    float face_height_screen = std::abs(chin.y - forehead.y);
    
    // Estimate depth from face size (inverse relationship)
    // CALIBRATION NOTE: reference_face_height depends on your camera FOV and typical distance
    // Adjust this value to match your setup:
    // - If filters appear TOO CLOSE: increase reference_face_height (e.g., 0.25 or 0.30)
    // - If filters appear TOO FAR: decrease reference_face_height (e.g., 0.15 or 0.18)
    // Based on user logs: face_height ≈ 0.27 at normal sitting distance
    const float reference_face_height = 0.27f;  // Face height at 1.0m distance (calibrated from logs)
    const float reference_depth = 1.0f;          // Reference distance (1 meter)
    
    if (face_height_screen > 0.01f) {  // Avoid division by zero
      depth = (reference_face_height / face_height_screen) * reference_depth;
      depth = std::clamp(depth, 0.3f, 3.0f);  // Clamp to reasonable range (30cm - 3m)
    } else {
      depth = 1.0f;  // Fallback if face too small/not detected
    }
    
    face_offset = glm::vec3(0.0f, 0.0f, 0.0f);
    
    static int depth_count = 0;
    if (depth_count < 10 || depth_count % 30 == 0) {
      ABSL_LOG(INFO) << "[FACE HEIGHT DEPTH] face_height=" << face_height_screen 
                     << " depth=" << depth << "m (PnP rotation only, depth from face size)";
    }
    depth_count++;
  } else {
    // No landmarks available - use fixed depth
    depth = 1.0f;
    face_offset = glm::vec3(0.0f, 0.0f, 0.0f);
    ABSL_LOG(WARNING) << "[FIXED DEPTH] No landmarks - using 1.0m default";
  }
  
  // Calculate world position from normalized screen coordinates using perspective math
  const float fov_degrees = 45.0f;
  const float tan_half_fov = std::tan(glm::radians(fov_degrees / 2.0f));
  const float aspect = static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_);

  // Calculate face center in normalized space
  cv::Point3f face_center(0.5f, 0.5f, 0.0f);
  if (!face_landmarks_.empty() && face_landmarks_.size() > 263) {
    cv::Point3f left_eye = face_landmarks_[33];
    cv::Point3f right_eye = face_landmarks_[263];
    face_center.x = (left_eye.x + right_eye.x) / 2.0f;
    face_center.y = (left_eye.y + right_eye.y) / 2.0f;
    face_center.z = (left_eye.z + right_eye.z) / 2.0f;
  }

  // Face center in world (meters) at current depth
  glm::vec3 face_center_world = glm::vec3(
      (face_center.x - 0.5f) * 2.0f * depth * tan_half_fov * aspect,
      (face_center.y - 0.5f) * 2.0f * depth * tan_half_fov,
      -depth
  );

  // Anchor offset from face center in normalized space → world space
  // For Z, scale by world face width at current depth to avoid exploding with distance
  float face_width_screen = 0.0f;
  if (!face_landmarks_.empty() && face_landmarks_.size() > 263) {
    cv::Point3f left_eye = face_landmarks_[33];
    cv::Point3f right_eye = face_landmarks_[263];
    face_width_screen = std::abs(right_eye.x - left_eye.x);
  }
  float world_face_width_for_z = face_width_screen * 2.0f * depth * tan_half_fov * aspect;  // meters
  float z_offset_world = (anchor.z - face_center.z) * world_face_width_for_z * anchor_z_scale_;

  // Depth-based Z lock: fade out landmark-derived Z as user moves farther from camera.
  // Rationale: landmark Z can get noisy with distance. Blending to 0 keeps overlays on the face plane,
  // preventing perspective inversions that make them look larger when moving away.
  {
    const float z_lock_near_m = 0.45f;  // start fading beyond ~45 cm
    const float z_lock_far_m  = 1.20f;  // fully locked by ~1.2 m
    float t = (depth - z_lock_near_m) / (z_lock_far_m - z_lock_near_m);
    float keep = 1.0f - std::clamp(t, 0.0f, 1.0f); // 1 near, 0 far
    z_offset_world *= keep;
  }

  glm::vec3 anchor_offset_world = glm::vec3(
      (anchor.x - face_center.x) * 2.0f * depth * tan_half_fov * aspect,
      (anchor.y - face_center.y) * 2.0f * depth * tan_half_fov,
      z_offset_world
  );

  // Final world position = face center + offset
  glm::vec3 world_position = face_center_world + anchor_offset_world;

  // For non-crown anchors, keep Z close to the face depth to avoid distance growth with camera motion.
  if (instance.anchor_name != "head_crown") {
    // Blend between computed anchor Z and the face plane depth (face_center_world.z == -depth)
    float z_face = face_center_world.z; // equals -depth
    world_position.z = (1.0f - anchor_z_face_lerp_) * world_position.z + anchor_z_face_lerp_ * z_face;
    // Then apply the signed bias (user may set negative to push into face plane or positive toward camera)
    world_position.z = world_position.z + anchor_z_bias_meters_;
  }

  const float reference_depth_for_scaling = 1.0f;
  float actual_face_width = face_width_screen * 2.0f * reference_depth_for_scaling * tan_half_fov * aspect;
  
  // DEBUG: Log face center, anchor offset, and final world position
  static int anchor_depth_log = 0;
  static float prev_logged_depth = 0.0f;
  float depth_change = std::abs(depth - prev_logged_depth);
  
  if (anchor_depth_log < 20 || anchor_depth_log % 30 == 0 || depth_change > 0.05f) {
    ABSL_LOG(INFO) << "[FACE→WORLD] face_center_2d=[" << face_center.x << "," << face_center.y << "] "
                   << "face_center_world=[" << face_center_world.x << "," << face_center_world.y << "," << face_center_world.z << "] "
                   << "anchor_offset_world=[" << anchor_offset_world.x << "," << anchor_offset_world.y << "," << anchor_offset_world.z << "] "
                   << "world_pos=[" << world_position.x << "," << world_position.y << "," << world_position.z << "]m "
                   << "depth=" << depth << "m";
    prev_logged_depth = depth;
  }
  anchor_depth_log++;
  
  // Apply crown offset - ONLY DEPTH (Z) in world space
  // Vertical (Y) is handled in normalized space in GetAnchorPosition
  if (instance.anchor_name == "head_crown") {
    if (!face_landmarks_.empty() && face_landmarks_.size() > 152) {
      // Store original Z for proportional calculations
      float original_z = world_position.z;
      
      // Apply depth offset PROPORTIONAL to current depth
      // This prevents perspective distortion when moving closer/farther
      float depth_ratio = crown_depth_offset_ * 0.1f;  // 10% per unit
      float depth_offset_meters = original_z * depth_ratio;
      
      // IMPORTANT: Prevent crown from going behind camera or too close to near plane
      // Near plane is at 0.1m, so keep crown at least 0.15m away
      const float MIN_Z_DISTANCE = 0.15f;
      float new_z = original_z - depth_offset_meters;
      if (new_z < MIN_Z_DISTANCE) {
        depth_offset_meters = original_z - MIN_Z_DISTANCE;
        new_z = MIN_Z_DISTANCE;
      }
      world_position.z = new_z;
      
      static int crown_offset_log = 0;
      if (crown_offset_log < 10 || crown_offset_log % 30 == 0) {
        ABSL_LOG(INFO) << "[CROWN NORMALIZED-Y + PROP-Z] "
                       << "orig_z=" << original_z << "m "
                       << "z_offset=" << depth_offset_meters << "m "
                       << "world_pos=[" << world_position.x << "," << world_position.y << "," << world_position.z << "]m";
      }
      crown_offset_log++;
    }
  }
  
  // Add face offset ONLY if PnP is valid
  static int face_offset_log = 0;
  if (face_offset_log < 30 || face_offset_log % 10 == 0) {
    ABSL_LOG(INFO) << "[FACE OFFSET] face_offset=[" << face_offset.x << "," << face_offset.y << "," << face_offset.z << "]";
  }
  face_offset_log++;
  world_position += face_offset;
  
  // DEBUG: Log final world position frequently
  static int position_log_count = 0;
  if (position_log_count < 30 || position_log_count % 10 == 0) {
    ABSL_LOG(INFO) << "[WORLD POS] " << instance.anchor_name 
                   << " final=[" << world_position.x << ", " << world_position.y << ", " << world_position.z << "]";
  }
  position_log_count++;
  
  // Apply instance offset (NOT rotated - just raw offset)
  // DEBUG: Log before and after instance offset
  static int offset_log_count = 0;
  if (offset_log_count < 20 || offset_log_count % 30 == 0) {
    ABSL_LOG(INFO) << "[INSTANCE OFFSET] " << instance.anchor_name 
                   << " world_pos_before=[" << world_position.x << ", " << world_position.y << ", " << world_position.z << "]"
                   << " instance.offset=[" << instance.offset.x << ", " << instance.offset.y << ", " << instance.offset.z << "]";
  }
  world_position += instance.offset;
  if (offset_log_count < 20 || offset_log_count % 30 == 0) {
    ABSL_LOG(INFO) << "[INSTANCE OFFSET] " << instance.anchor_name 
                   << " world_pos_after=[" << world_position.x << ", " << world_position.y << ", " << world_position.z << "]";
  }
  offset_log_count++;
  
  // Build model matrix
  glm::mat4 model = glm::mat4(1.0f);
  
  // CRITICAL FIX: Matrix transformation order for proper face attachment
  // 
  // The problem: MediaPipe landmarks give us 2D screen coordinates that change when
  // you rotate your head. When you roll right, your nose_bridge moves left on screen.
  // But in 3D world space, the nose should stay attached to your face!
  //
  // Solution: We need to work in FACE-LOCAL space, not world space directly.
  // 1. Convert screen anchor to world position (accounts for perspective)
  // 2. Calculate head rotation from landmarks
  // 3. The position is already in world space at the face location
  // 4. Apply rotation to orient the model with the head
  //
  // The key: world_position is derived from screen coords, which already moves
  // to follow the face. We just need to rotate the MODEL, not the position.
  
  // Calculate head rotation FIRST (before building matrix)
  glm::quat head_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity by default
  
  if (!face_landmarks_.empty() && face_landmarks_.size() > 454) {
    // Use eye corners and nose to calculate head tilt (roll) and pitch
    cv::Point3f left_eye = face_landmarks_[33];   // Left eye outer corner
    cv::Point3f right_eye = face_landmarks_[263]; // Right eye outer corner
    cv::Point3f nose_tip = face_landmarks_[1];    // Nose tip
    cv::Point3f forehead = face_landmarks_[10];   // Forehead
    cv::Point3f chin = face_landmarks_[152];      // Chin
    
    // Calculate ROLL (tilt left/right) from eye line angle
    float eye_delta_y = right_eye.y - left_eye.y;  // Y difference between eyes
    float eye_delta_x = right_eye.x - left_eye.x;  // X difference between eyes
    float roll_angle = std::atan2(eye_delta_y, eye_delta_x) * 0.5f;  // Radians * 0.5 = half sensitivity (INVERTED)
    
    // Calculate PITCH (tilt forward/backward) from face vertical alignment
    // When looking down: nose moves down relative to forehead
    // When looking up: nose moves up relative to forehead
    float face_center_y = (forehead.y + chin.y) / 2.0f;
    float nose_offset = nose_tip.y - face_center_y;
    float face_height = std::abs(chin.y - forehead.y);
    float pitch_ratio = nose_offset / (face_height * 0.5f);  // -1 to +1 range
    float pitch_angle = -pitch_ratio * 0.3f;  // Scale to ~±17 degrees max (INVERTED with negative sign)
    
    // Calculate YAW (turn left/right) from nose horizontal position
    float face_center_x = (left_eye.x + right_eye.x) / 2.0f;
    float nose_offset_x = nose_tip.x - face_center_x;
    float face_width = std::abs(right_eye.x - left_eye.x);
    float yaw_ratio = nose_offset_x / (face_width * 0.5f);
    float yaw_angle = yaw_ratio * 0.25f;  // Scale to ~±14 degrees max (reduced from 0.5)
    
    // Create rotation quaternion from Euler angles (apply in order: yaw, pitch, roll)
    glm::quat yaw_quat = glm::angleAxis(yaw_angle, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat pitch_quat = glm::angleAxis(pitch_angle, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat roll_quat = glm::angleAxis(roll_angle, glm::vec3(0.0f, 0.0f, 1.0f));
    head_rotation = yaw_quat * pitch_quat * roll_quat;
    
    static int rotation_log_count = 0;
    if (rotation_log_count < 20 || rotation_log_count % 30 == 0) {
      ABSL_LOG(INFO) << "[LANDMARK ROTATION] "
                     << "pitch=" << glm::degrees(pitch_angle) << "° "
                     << "yaw=" << glm::degrees(yaw_angle) << "° "
                     << "roll=" << glm::degrees(roll_angle) << "°";
    }
    rotation_log_count++;
  }
  
  // SIMPLIFIED APPROACH: Use anchor position directly, only rotate the model
  // The anchor position from MediaPipe tracks the face feature on screen.
  // We convert it to pixel space, then just rotate the MODEL to match head tilt.
  
  // Build the model matrix:
  // 1. Translate to anchor's pixel position
  model = glm::translate(model, world_position);
  
  // DEBUG: Log transform components
  static int transform_log = 0;
  if (transform_log < 30 || transform_log % 10 == 0) {
    ABSL_LOG(INFO) << "[TRANSFORM] " << instance.anchor_name 
                   << " world_pos=[" << world_position.x << "," << world_position.y << "," << world_position.z << "] "
                   << " head_rot=[" << glm::degrees(glm::eulerAngles(head_rotation).x) << "," 
                   << glm::degrees(glm::eulerAngles(head_rotation).y) << "," 
                   << glm::degrees(glm::eulerAngles(head_rotation).z) << "]°";
  }
  transform_log++;
  
  // 2. Apply head rotation (to orient the model with the head)
  model = model * glm::mat4_cast(head_rotation);

  // 3. Apply instance rotation (filter-specific orientation)
  model = glm::rotate(model, glm::radians(instance.rotation_euler.x), glm::vec3(1,0,0));  // Pitch
  model = glm::rotate(model, glm::radians(instance.rotation_euler.y), glm::vec3(0,1,0));  // Yaw
  model = glm::rotate(model, glm::radians(instance.rotation_euler.z), glm::vec3(0,0,1));  // Roll

  
  // 4. Apply scale
  
  glm::vec3 final_scale = instance.scale;
  if (instance.flip_z) {
    final_scale.z = -final_scale.z;
  }

  // Keep model size proportional to face width across depth changes
  if (scale_with_face_width_) {
    // face_width_screen was computed earlier (normalized [0..1] width)
    if (face_width_screen > 0.0f) {
      const float tan_half_fov_local = std::tan(glm::radians(fov_degrees / 2.0f));
      float face_width_m_now = face_width_screen * 2.0f * depth * tan_half_fov_local * aspect;
      if (!(face_width_baseline_m_ > 0.0f) || !std::isfinite(face_width_baseline_m_)) {
        face_width_baseline_m_ = face_width_m_now;
      }
      if (face_width_baseline_m_ > 0.0f && std::isfinite(face_width_m_now)) {
        float scale_ratio = face_width_m_now / face_width_baseline_m_;
        if (!std::isfinite(scale_ratio)) scale_ratio = 1.0f;
        // Clamp to a safe range to avoid popping
        scale_ratio = std::max(0.5f, std::min(2.0f, scale_ratio));
        final_scale *= scale_ratio;
      }
    }
  }

  // Safety: guard against legacy JSONs that include large negative Z offsets (meters)
  if (std::abs(instance.offset.z) > 0.2f) { // > 20 cm is suspicious for face overlays
    static int once = 0;
    if (once < 3) {
      ABSL_LOG(WARNING) << "[AR] Large instance.offset.z detected (" << instance.offset.z
                        << ") — clamping to ±0.2m to avoid distance blow-up.";
      once++;
    }
    // Clamp but don't mutate instance; apply as translation mitigation
    float clampedZ = std::max(std::min(instance.offset.z, 0.2f), -0.2f);
    // Adjust model translation instead of modifying world_position pre-rotation
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, clampedZ - instance.offset.z));
  }
  
  static int scale_log_count = 0;
  if (scale_log_count < 10 || scale_log_count % 30 == 0) {
    ABSL_LOG(INFO) << "[FACE-SPACE SCALE] depth=" << depth << "m, "
                   << "face_width_screen=" << face_width_screen << ", "
                   << "actual_face_width=" << actual_face_width << "m, "
                   << "scale_with_face_width=" << (scale_with_face_width_ ? 1 : 0) << ", "
                   << "baseline_face_width_m=" << face_width_baseline_m_ << ", "
                   << "final_scale=[" << final_scale.x << "," << final_scale.y << "," << final_scale.z << "]";
  }
  scale_log_count++;
  
  model = glm::scale(model, final_scale);
  
  // Optional: small offset along approximate face normal (camera-view -Z)
  if (std::abs(face_normal_offset_m_) > 1e-5f) {
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, -face_normal_offset_m_));
  }
  
  return model;
}

void OpenGLRenderer::RenderModelInstance(const ModelInstance& instance, const HeadPose& head_pose) {
  ABSL_LOG(INFO) << "[DEBUG] RenderModelInstance called: id=" << instance.id 
                 << " anchor=" << instance.anchor_name 
                 << " visible=" << instance.visible;
  
  if (!instance.visible || !instance.model_ptr) {
    ABSL_LOG(WARNING) << "[DEBUG] Instance not visible or has no model";
    return;
  }
  
  const Model* model = instance.model_ptr;
  if (model->meshes.empty()) {
    ABSL_LOG(WARNING) << "[DEBUG] Model has no meshes";
    return;
  }
  
  // Calculate model matrix
  glm::mat4 model_matrix = CalculateInstanceTransform(instance, head_pose);
  
  // Apply transform smoothing (with first-frame detection)
  auto& cache = transform_caches_[instance.id];
  model_matrix = GetSmoothedTransform(model_matrix, cache);
  
  // Calculate normal matrix
  glm::mat3 normal_matrix = glm::mat3(glm::transpose(glm::inverse(model_matrix)));
  
  // Set uniforms
  shader_->SetMat4("uModel", glm::value_ptr(model_matrix));
  shader_->SetMat3("uNormalMatrix", glm::value_ptr(normal_matrix));
  
  // Get the actual material from the model instead of using default
  Material material_to_use;
  if (!model->materials.empty()) {
    // Use the first material (most models have one material)
    material_to_use = model->materials.begin()->second;
    ABSL_LOG(INFO) << "[DEBUG] Using model material: " << model->materials.begin()->first;
  } else {
    // Fallback to default material
    material_to_use.ambient[0] = 0.3f;
    material_to_use.ambient[1] = 0.3f;
    material_to_use.ambient[2] = 0.3f;
    material_to_use.diffuse[0] = 0.8f;
    material_to_use.diffuse[1] = 0.8f;
    material_to_use.diffuse[2] = 0.8f;
    material_to_use.specular[0] = 0.5f;
    material_to_use.specular[1] = 0.5f;
    material_to_use.specular[2] = 0.5f;
    material_to_use.shininess = 32.0f;
    material_to_use.opacity = 1.0f;
    ABSL_LOG(INFO) << "[DEBUG] Using default material (no materials in model)";
  }
  
  // Set material uniforms
  shader_->SetVec3("uMaterialAmbient", 
                   material_to_use.ambient[0],
                   material_to_use.ambient[1], 
                   material_to_use.ambient[2]);
  shader_->SetVec3("uMaterialDiffuse",
                   material_to_use.diffuse[0],
                   material_to_use.diffuse[1],
                   material_to_use.diffuse[2]);
  shader_->SetVec3("uMaterialSpecular",
                   material_to_use.specular[0],
                   material_to_use.specular[1],
                   material_to_use.specular[2]);
  shader_->SetFloat("uMaterialShininess", material_to_use.shininess);
  shader_->SetFloat("uMaterialOpacity", material_to_use.opacity);

  // **NEW: Handle texture loading and binding**
  bool has_texture = false;
  bool has_opacity_map = false;
  bool has_emissive_map = false;
  
  if (!material_to_use.texture_path.empty() && texture_manager_) {
    // Try to load texture if not already loaded
    auto texture_result = texture_manager_->LoadTexture(material_to_use.texture_path);
    if (texture_result.ok()) {
      const auto& texture = texture_result.value();
      if (texture.IsValid()) {
        // Bind texture to texture unit 0
        texture_manager_->BindTexture(texture, 0);
        has_texture = true;
        ABSL_LOG(INFO) << "Bound texture: " << material_to_use.texture_path 
                       << " (" << texture.width << "x" << texture.height << ")";
      }
    } else {
      ABSL_LOG(WARNING) << "Failed to load texture: " << material_to_use.texture_path 
                        << " - " << texture_result.status().message();
    }
  }
  
  // Load and bind opacity map if available
  if (!material_to_use.opacity_map_path.empty() && texture_manager_) {
    auto opacity_result = texture_manager_->LoadTexture(material_to_use.opacity_map_path);
    if (opacity_result.ok()) {
      const auto& opacity_map = opacity_result.value();
      if (opacity_map.IsValid()) {
        texture_manager_->BindTexture(opacity_map, 1);  // Texture unit 1
        has_opacity_map = true;
        ABSL_LOG(INFO) << "Bound opacity map: " << material_to_use.opacity_map_path;
      }
    }
  }
  
  // Load and bind emissive map if available
  if (!material_to_use.emissive_map_path.empty() && texture_manager_) {
    auto emissive_result = texture_manager_->LoadTexture(material_to_use.emissive_map_path);
    if (emissive_result.ok()) {
      const auto& emissive_map = emissive_result.value();
      if (emissive_map.IsValid()) {
        texture_manager_->BindTexture(emissive_map, 2);  // Texture unit 2
        has_emissive_map = true;
        ABSL_LOG(INFO) << "Bound emissive map: " << material_to_use.emissive_map_path;
      }
    }
  }
  
  // Set texture uniforms
  shader_->SetBool("uHasTexture", has_texture);
  shader_->SetInt("uTexture", 0);  // Texture unit 0
  shader_->SetBool("uHasOpacityMap", has_opacity_map);
  shader_->SetInt("uOpacityMap", 1);  // Texture unit 1
  shader_->SetBool("uHasEmissiveMap", has_emissive_map);
  shader_->SetInt("uEmissiveMap", 2);  // Texture unit 2
  
  // Handle face winding for flipped models
  // When Z-scale is negative (flip_z), triangle winding order is inverted
  if (instance.flip_z) {
    glFrontFace(GL_CW);  // Switch to clockwise for flipped geometry
  }
  
  // Render each mesh
  for (const auto& mesh : model->meshes) {
    if (mesh.vao == 0) {
      continue;
    }
    
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
  }
  
  // Restore face winding if it was changed
  if (instance.flip_z) {
    glFrontFace(GL_CCW);  // Restore to default counter-clockwise
  }
}

int OpenGLRenderer::RenderInstances() {
  if (!initialized_) {
    ABSL_LOG(ERROR) << "OpenGLRenderer not initialized";
    return 0;
  }
  
  ABSL_LOG(INFO) << "[DEBUG] RenderInstances called: " << instances_.size() << " total instances";
  
  if (instances_.empty()) {
    ABSL_LOG(WARNING) << "[DEBUG] No instances to render";
    return 0;
  }
  
  // Count visible instances
  int visible_count = 0;
  for (const auto& pair : instances_) {
    if (pair.second.visible) visible_count++;
  }
  ABSL_LOG(INFO) << "[DEBUG] Visible instances: " << visible_count;
  
  // Calculate head pose if landmarks available
  HeadPose head_pose;
  if (head_pose_valid_) {
    head_pose = current_head_pose_;
  } else {
    head_pose = CalculateHeadPose(viewport_width_, viewport_height_);
  }
  
  // Fallback to roll-only if PnP fails
  if (head_pose.confidence < 0.5f && !face_landmarks_.empty()) {
    // Calculate roll from eye line
    if (face_landmarks_.size() > 263) {
      cv::Point3f left_eye = face_landmarks_[33];
      cv::Point3f right_eye = face_landmarks_[263];
      cv::Point3f eye_vector = right_eye - left_eye;
      head_pose.euler_angles.x = 0.0f;  // No pitch
      head_pose.euler_angles.y = 0.0f;  // No yaw
      head_pose.euler_angles.z = std::atan2(eye_vector.y, eye_vector.x);  // Roll only
      // Use glm::angleAxis for single-axis rotation (CORRECT METHOD)
      head_pose.rotation = glm::angleAxis(static_cast<float>(head_pose.euler_angles.z), glm::vec3(0.0f, 0.0f, 1.0f));
      head_pose.confidence = 0.5f;
    }
  }
  
  // Setup GL state
  SetupGLState();
  
  // Activate shader
  shader_->Use();
  
  // Set global uniforms
  glm::mat4 projection, view;
  std::memcpy(glm::value_ptr(projection), projection_matrix_, 16 * sizeof(float));
  std::memcpy(glm::value_ptr(view), view_matrix_, 16 * sizeof(float));
  
  shader_->SetMat4("uProjection", glm::value_ptr(projection));
  shader_->SetMat4("uView", glm::value_ptr(view));
  shader_->SetVec3("uCameraPos", camera_position_[0], camera_position_[1], camera_position_[2]);
  shader_->SetVec3("uLightDir", light_direction_[0], light_direction_[1], light_direction_[2]);  // ← FIX: Match shader uniform name
  shader_->SetVec3("uLightColor", light_color_[0], light_color_[1], light_color_[2]);
  shader_->SetVec3("uAmbientColor", ambient_color_[0], ambient_color_[1], ambient_color_[2]);
  
  // Render each instance
  int rendered_count = 0;
  static int render_cycle = 0;
  for (const auto& pair : instances_) {
    const ModelInstance& instance = pair.second;
    if (instance.visible) {
      // Log anchor position for first few frames
      if (render_cycle < 3) {
        cv::Point3f anchor_pos = GetAnchorPosition(instance.anchor_name);
        ABSL_LOG(INFO) << "[DEBUG] Instance '" << instance.id << "' anchor=" << instance.anchor_name
                       << " pos=[" << anchor_pos.x << ", " << anchor_pos.y << ", " << anchor_pos.z << "]";
      }
      RenderModelInstance(instance, head_pose);
      rendered_count++;
    }
  }
  render_cycle++;
  
  // **NEW: Render debug anchors (Finding #8)**
  if (debug_anchors_enabled_) {
    // CRITICAL: Unbind shader program before debug rendering
    // Shaders override glColor* calls with their own lighting calculations
    glUseProgram(0);  // Switch to fixed-function pipeline
    RenderDebugAnchors(head_pose);
  }
  
  // Check for errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    ABSL_LOG(ERROR) << "OpenGL error during instance rendering: " << error;
  }
  
  // Restore GL state
  RestoreGLState();
  
  return rendered_count;
}

// **NEW: Debug Visualization Implementation (Finding #8)**

void OpenGLRenderer::SetDebugAnchorsEnabled(bool enabled) {
  debug_anchors_enabled_ = enabled;
  ABSL_LOG(INFO) << "Debug anchor visualization " << (enabled ? "enabled" : "disabled");
}

void OpenGLRenderer::SetCrownOffsetMultiplier(float multiplier) {
  crown_offset_multiplier_ = multiplier;
  ABSL_LOG(INFO) << "Crown offset multiplier set to " << multiplier << " (" << (multiplier * 100.0f) << "% above forehead)";
}

void OpenGLRenderer::SetCrownDepthOffset(float offset) {
  crown_depth_offset_ = offset;
  ABSL_LOG(INFO) << "Crown depth offset set to " << offset << " (" << (offset * 200.0f) << "% " << (offset > 0 ? "backward" : "forward") << ")";
}

void OpenGLRenderer::RenderDebugAnchors(const HeadPose& head_pose) {
  if (!debug_anchors_enabled_ || face_landmarks_.empty()) {
    return;
  }
  
  // Use immediate mode OpenGL for simple debug rendering (GL_LINES, GL_POINTS)
  // Save current OpenGL state
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  
  // Disable everything that could interfere with colors
  glDisable(GL_DEPTH_TEST);  // Always visible on top
  glDisable(GL_LIGHTING);    // No lighting calculations
  glDisable(GL_TEXTURE_2D);  // No textures
  glDisable(GL_BLEND);       // No blending
  glDisable(GL_FOG);         // No fog
  
  // Use vertex colors directly
  glShadeModel(GL_FLAT);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glm::mat4 projection;
  std::memcpy(glm::value_ptr(projection), projection_matrix_, 16 * sizeof(float));
  glLoadMatrixf(glm::value_ptr(projection));
  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glm::mat4 view;
  std::memcpy(glm::value_ptr(view), view_matrix_, 16 * sizeof(float));
  glLoadMatrixf(glm::value_ptr(view));
  
  // **DYNAMIC DEPTH ESTIMATION - Same as main rendering**
  // Calculate depth from face size for consistent debug visualization
  float depth;
  glm::vec3 face_offset;
  
  // Calculate face height from landmarks (forehead to chin)
  if (!face_landmarks_.empty() && face_landmarks_.size() > 152) {
    cv::Point3f forehead = face_landmarks_[10];   // Forehead landmark
    cv::Point3f chin = face_landmarks_[152];      // Chin landmark
    float face_height_screen = std::abs(chin.y - forehead.y);  // Normalized 0-1
    
    // Estimate depth from face size (inverse relationship)
    const float reference_face_height = 0.20f;  // Face height at 1.0m distance
    const float reference_depth = 1.0f;          // Reference distance (1 meter)
    
    if (face_height_screen > 0.01f) {  // Avoid division by zero
      depth = (reference_face_height / face_height_screen) * reference_depth;
      depth = std::clamp(depth, 0.3f, 3.0f);  // Clamp to reasonable range (30cm - 3m)
    } else {
      depth = 1.0f;  // Fallback if face too small/not detected
    }
    
    face_offset = glm::vec3(0.0f, 0.0f, 0.0f);  // No offset
  } else {
    // No landmarks available - use fixed depth
    depth = 1.0f;
    face_offset = glm::vec3(0.0f, 0.0f, 0.0f);
  }
  
  // Helper lambda to convert normalized landmark to world space
  // Uses same fallback positioning as main anchor calculation
  // Must account for perspective projection with 45° FOV
  const float fov_degrees = 45.0f;
  const float tan_half_fov = std::tan(glm::radians(fov_degrees / 2.0f));
  const float aspect = static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_);
  
  // Apply 2x scale multiplier to match main rendering
  const float scale_multiplier = 2.0f;
  
  auto LandmarkToWorld = [&](const cv::Point3f& landmark) -> glm::vec3 {
    glm::vec3 pos = glm::vec3(
        (landmark.x - 0.5f) * 2.0f * depth * tan_half_fov * aspect * scale_multiplier,
        (landmark.y - 0.5f) * 2.0f * depth * tan_half_fov * scale_multiplier,  // Y same direction as screen
        depth
    );
    return pos + face_offset;  // Add face offset only if PnP valid
  };
  
  // Draw key landmarks
  glPointSize(8.0f);
  glBegin(GL_POINTS);
  
  // Forehead (landmark 10) - RED
  glm::vec3 forehead_world;
  if (face_landmarks_.size() > 10) {
    glColor3f(1.0f, 0.0f, 0.0f);
    forehead_world = LandmarkToWorld(face_landmarks_[10]);
    glVertex3f(forehead_world.x, forehead_world.y, forehead_world.z);
  }
  
  // Chin (landmark 152) - BLUE
  if (face_landmarks_.size() > 152) {
    glColor3f(0.0f, 0.0f, 1.0f);
    glm::vec3 chin = LandmarkToWorld(face_landmarks_[152]);
    glVertex3f(chin.x, chin.y, chin.z);
  }
  
  // Left eye (landmark 33) - CYAN
  if (face_landmarks_.size() > 33) {
    glColor3f(0.0f, 1.0f, 1.0f);
    glm::vec3 left_eye = LandmarkToWorld(face_landmarks_[33]);
    glVertex3f(left_eye.x, left_eye.y, left_eye.z);
  }
  
  // Right eye (landmark 263) - MAGENTA
  if (face_landmarks_.size() > 263) {
    glColor3f(1.0f, 0.0f, 1.0f);
    glm::vec3 right_eye = LandmarkToWorld(face_landmarks_[263]);
    glVertex3f(right_eye.x, right_eye.y, right_eye.z);
  }
  
  glEnd();
  
  // Draw head_crown anchor - BIG GREEN MARKER
  cv::Point3f crown_anchor = GetAnchorPosition("head_crown");
  
  // Debug: Log crown anchor position
  static int crown_debug_count = 0;
  if (crown_debug_count < 5) {
    ABSL_LOG(INFO) << "[DEBUG] Crown anchor: [" << crown_anchor.x << "," << crown_anchor.y << "," << crown_anchor.z << "]";
    crown_debug_count++;
  }
  
  glm::vec3 crown_world = LandmarkToWorld(crown_anchor);
  
  // Debug: Log world position
  static int world_debug_count = 0;
  if (world_debug_count < 10 || world_debug_count % 60 == 0) {
    ABSL_LOG(INFO) << "[DEBUG] Crown world: [" << crown_world.x << "," << crown_world.y << "," << crown_world.z << "]";
    if (face_landmarks_.size() > 10) {
      ABSL_LOG(INFO) << "[DEBUG] Forehead world: [" << forehead_world.x << "," << forehead_world.y << "," << forehead_world.z << "]";
      ABSL_LOG(INFO) << "[WORLD ALIGNMENT] X diff = " << (crown_world.x - forehead_world.x) << " (should be ~0!)";
    }
  }
  world_debug_count++;
  
  // Make sure point rendering is enabled and size is set
  glEnable(GL_POINT_SMOOTH);  // Smooth points
  
  // Draw MULTIPLE markers in different colors at crown position
  // If lighting interferes, at least ONE color should be visible
  
  // 1. HUGE GREEN marker
  glPointSize(35.0f);
  glBegin(GL_POINTS);
  glColor3f(0.0f, 1.0f, 0.0f);  // Pure green
  glVertex3f(crown_world.x, crown_world.y, crown_world.z);
  glEnd();
  
  // 2. CYAN marker (slightly offset)
  glPointSize(30.0f);
  glBegin(GL_POINTS);
  glColor3f(0.0f, 1.0f, 1.0f);  // Cyan
  glVertex3f(crown_world.x + 0.01f, crown_world.y, crown_world.z);
  glEnd();
  
  // 3. YELLOW marker (slightly offset)
  glPointSize(30.0f);
  glBegin(GL_POINTS);
  glColor3f(1.0f, 1.0f, 0.0f);  // Yellow
  glVertex3f(crown_world.x - 0.01f, crown_world.y, crown_world.z);
  glEnd();
  
  // 4. WHITE marker (center - always visible)
  glPointSize(25.0f);
  glBegin(GL_POINTS);
  glColor3f(1.0f, 1.0f, 1.0f);  // Pure white
  glVertex3f(crown_world.x, crown_world.y + 0.01f, crown_world.z);
  glEnd();
  
  // Draw CROSS pattern at crown for maximum visibility
  glLineWidth(5.0f);
  glBegin(GL_LINES);
  
  // Horizontal line - MAGENTA
  glColor3f(1.0f, 0.0f, 1.0f);
  glVertex3f(crown_world.x - 0.05f, crown_world.y, crown_world.z);
  glVertex3f(crown_world.x + 0.05f, crown_world.y, crown_world.z);
  
  // Vertical line - CYAN
  glColor3f(0.0f, 1.0f, 1.0f);
  glVertex3f(crown_world.x, crown_world.y - 0.05f, crown_world.z);
  glVertex3f(crown_world.x, crown_world.y + 0.05f, crown_world.z);
  
  glEnd();
  
  // Draw line from forehead to crown
  if (face_landmarks_.size() > 10) {
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 1.0f, 0.0f);  // YELLOW
    glm::vec3 forehead = LandmarkToWorld(face_landmarks_[10]);
    glVertex3f(forehead.x, forehead.y, forehead.z);
    glVertex3f(crown_world.x, crown_world.y, crown_world.z);
    glEnd();
  }
  
  // Draw coordinate axes at head center
  glm::vec3 head_center(
      head_pose.translation.x / 1000.0f,
      head_pose.translation.y / 1000.0f,
      depth
  );
  
  glLineWidth(2.0f);
  glBegin(GL_LINES);
  
  // X-axis - RED
  glColor3f(1.0f, 0.0f, 0.0f);
  glVertex3f(head_center.x, head_center.y, head_center.z);
  glVertex3f(head_center.x + 0.1f, head_center.y, head_center.z);
  
  // Y-axis - GREEN
  glColor3f(0.0f, 1.0f, 0.0f);
  glVertex3f(head_center.x, head_center.y, head_center.z);
  glVertex3f(head_center.x, head_center.y + 0.1f, head_center.z);
  
  // Z-axis - BLUE
  glColor3f(0.0f, 0.0f, 1.0f);
  glVertex3f(head_center.x, head_center.y, head_center.z);
  glVertex3f(head_center.x, head_center.y, head_center.z + 0.1f);
  
  glEnd();
  
  // Restore previous OpenGL state
  glPopAttrib();
}

} // namespace ar_filters
} // namespace segmecam
