// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// OpenGL 3D Renderer Implementation - Phase 3 Step 8
// 
// CRITICAL: This file uses epoxy/gl.h and must NEVER include MediaPipe headers
// Keep this compilation unit completely separate from MediaPipe's GL setup

#include "include/ar_filters/opengl_renderer.h"
#include "include/ar_filters/model_loader.h"
#include "include/render/shader_program.h"

// OpenGL and math libraries (isolated from MediaPipe)
#include <epoxy/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
      viewport_height_(480) {
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
  camera_position_[2] = 0.0f;
  
  // Initialize matrices to identity
  for (int i = 0; i < 16; i++) {
    projection_matrix_[i] = (i % 5 == 0) ? 1.0f : 0.0f; // Identity matrix
    view_matrix_[i] = (i % 5 == 0) ? 1.0f : 0.0f;
  }
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

  // Setup default projection matrix
  UpdateProjectionMatrix(viewport_width_, viewport_height_);

  // Test shader activation
  shader_->Use();
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    ABSL_LOG(ERROR) << "OpenGL error after shader activation: " << error;
    return false;
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

  // Create perspective projection matrix
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
  
  // Create view matrix (camera looking down -Z axis)
  glm::mat4 view = glm::lookAt(
      glm::vec3(camera_position_[0], camera_position_[1], camera_position_[2]),
      glm::vec3(0.0f, 0.0f, -1.0f),  // Look at
      glm::vec3(0.0f, 1.0f, 0.0f)    // Up vector
  );

  // Copy to internal storage
  std::memcpy(projection_matrix_, glm::value_ptr(projection), 16 * sizeof(float));
  std::memcpy(view_matrix_, glm::value_ptr(view), 16 * sizeof(float));

  ABSL_LOG(INFO) << "Projection matrix updated for " << width << "x" << height;
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
  glFrontFace(GL_CCW);
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
  
  shader_.reset();
  
  initialized_ = false;
}

} // namespace ar_filters
} // namespace segmecam
