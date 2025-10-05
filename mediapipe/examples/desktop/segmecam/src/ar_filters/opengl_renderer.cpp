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
      crown_offset_multiplier_(0.4f) {  // Default 40% above forehead
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
  loaded_models_.clear();
  instances_.clear();
  
  initialized_ = false;
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
  
  // Bind video texture to FBO
  glBindFramebuffer(GL_FRAMEBUFFER, temp_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
  
  // Check FBO status
  GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (fbo_status != GL_FRAMEBUFFER_COMPLETE) {
    ABSL_LOG(ERROR) << "[DEBUG] FBO incomplete! Status: 0x" << std::hex << fbo_status;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
  }
  
  ABSL_LOG(INFO) << "[DEBUG] FBO setup complete, rendering " << instances_.size() << " instances to texture " << texture_id;
  
  // Set viewport
  glViewport(0, 0, width, height);
  
  // DON'T clear - we want to render AR filters ON TOP of existing video
  // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // COMMENTED OUT!
  
  // Enable blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Enable depth testing so filters occlude properly
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  
  // Render all model instances
  int rendered_count = RenderInstances();
  
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

std::string OpenGLRenderer::CreateInstance(
    const std::string& model_id,
    const std::string& anchor,
    const glm::vec3& offset,
    const glm::vec3& rotation,
    const glm::vec3& scale
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
    // Top of head - calculate from forehead with upward offset
    // Use ONLY forehead as base to ensure alignment with forehead anchor
    cv::Point3f forehead = face_landmarks_[10];
    cv::Point3f chin = face_landmarks_[152];
    
    // Calculate face height in normalized coordinates
    float face_height = std::abs(chin.y - forehead.y);
    
    // Crown position: same X/Z as forehead, offset Y upward
    // IMPORTANT: In MediaPipe coordinates, Y increases DOWNWARD (Y=0 is TOP, Y=1 is BOTTOM)
    // So to move UP (above forehead), we SUBTRACT from Y
    // Note: Z-offset (depth adjustment) is applied later in world space, not here
    cv::Point3f crown = forehead;  // Start with forehead position
    crown.y -= face_height * crown_offset_multiplier_;  // SUBTRACT to move UP
    // crown.z is kept same as forehead (depth offset applied in CalculateInstanceTransform)
    
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
  
  // Update camera intrinsics
  float focal_length = static_cast<float>(image_width);  // Simple approximation
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
  if (pnp_count < 3 || pnp_count % 30 == 0) {
    ABSL_LOG(INFO) << "[DEBUG] PnP SUCCESS - tvec: [" << translation_vec.at<double>(0) 
                   << ", " << translation_vec.at<double>(1)
                   << ", " << translation_vec.at<double>(2) << "]";
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
  
  // **DYNAMIC DEPTH ESTIMATION from face size**
  // PnP depth is unreliable (constant), so estimate from face height in screen space
  // Larger face = closer, smaller face = farther (inverse relationship)
  
  float depth;
  glm::vec3 face_offset;
  
  // Calculate face height from landmarks (forehead to chin)
  if (!face_landmarks_.empty() && face_landmarks_.size() > 152) {
    cv::Point3f forehead = face_landmarks_[10];   // Forehead landmark
    cv::Point3f chin = face_landmarks_[152];      // Chin landmark
    float face_height_screen = std::abs(chin.y - forehead.y);  // Normalized 0-1
    
    // Estimate depth from face size (inverse relationship)
    // Reference: at 1.0m distance, face height ≈ 0.20 (20% of screen)
    // At 0.5m (closer), face height ≈ 0.40 (40% of screen)
    // At 2.0m (farther), face height ≈ 0.10 (10% of screen)
    const float reference_face_height = 0.20f;  // Face height at 1.0m distance
    const float reference_depth = 1.0f;          // Reference distance (1 meter)
    
    if (face_height_screen > 0.01f) {  // Avoid division by zero
      depth = (reference_face_height / face_height_screen) * reference_depth;
      depth = std::clamp(depth, 0.3f, 3.0f);  // Clamp to reasonable range (30cm - 3m)
    } else {
      depth = 1.0f;  // Fallback if face too small/not detected
    }
    
    face_offset = glm::vec3(0.0f, 0.0f, 0.0f);  // No offset in fallback mode
    
    static int fallback_count = 0;
    if (fallback_count < 5 || fallback_count % 30 == 0) {
      ABSL_LOG(INFO) << "[DYNAMIC DEPTH] face_height=" << face_height_screen 
                     << " estimated_depth=" << depth << "m";
    }
    fallback_count++;
  } else {
    // No landmarks available - use fixed depth
    depth = 1.0f;
    face_offset = glm::vec3(0.0f, 0.0f, 0.0f);
  }
  
  // Calculate world position from normalized screen coordinates
  // Must account for perspective projection with 45° FOV
  const float fov_degrees = 45.0f;
  const float tan_half_fov = std::tan(glm::radians(fov_degrees / 2.0f));
  const float aspect = static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_);
  
  // DEBUG: Log anchor calculation with higher frequency for up/down testing
  static int debug_log_count = 0;
  if (debug_log_count < 20 || debug_log_count % 10 == 0) {  // Much more frequent logging
    float world_y_before_offset = (0.5f - anchor.y) * 2.0f * depth * tan_half_fov;
    ABSL_LOG(INFO) << "[ANCHOR " << instance.anchor_name << "] "
                   << "landmark_y=" << anchor.y 
                   << " world_y=" << world_y_before_offset
                   << " face_offset_y=" << face_offset.y
                   << " depth=" << depth
                   << " use_pnp=false (dynamic depth)";
  }
  debug_log_count++;
  
  // Apply 2x scale multiplier for more responsive tracking
  const float scale_multiplier = 2.0f;
  
  glm::vec3 world_position = glm::vec3(
      (anchor.x - 0.5f) * 2.0f * depth * tan_half_fov * aspect * scale_multiplier,  // X with FOV correction + scale
      (anchor.y - 0.5f) * 2.0f * depth * tan_half_fov * scale_multiplier,            // Y same direction as screen + scale
      depth                                                                           // Z = estimated depth
  );
  
  // Apply crown depth offset in world space (meters)
  // This allows W/S keys to move crown forward/backward
  if (instance.anchor_name == "head_crown") {
    // Calculate face height for scaling the offset
    if (!face_landmarks_.empty() && face_landmarks_.size() > 152) {
      cv::Point3f forehead = face_landmarks_[10];
      cv::Point3f chin = face_landmarks_[152];
      float face_height = std::abs(chin.y - forehead.y);
      
      // Apply depth offset: positive = forward (closer), negative = backward (farther)
      // Scale by face height and depth to make offset proportional to face size
      // CRITICAL: SUBTRACT offset because OpenGL Z-axis points backward (positive Z = away)
      float depth_offset_world = crown_depth_offset_ * face_height * depth;
      world_position.z -= depth_offset_world;  // Subtract to match intuitive direction
      
      static int depth_offset_log = 0;
      if (depth_offset_log < 10 || depth_offset_log % 30 == 0) {
        ABSL_LOG(INFO) << "[CROWN DEPTH] offset_multiplier=" << crown_depth_offset_ 
                       << " face_height=" << face_height
                       << " depth=" << depth << "m"
                       << " world_offset=" << depth_offset_world << "m"
                       << " final_z=" << world_position.z << "m";
      }
      depth_offset_log++;
    }
  }
  
  // Add face offset ONLY if PnP is valid
  world_position += face_offset;
  
  // DEBUG: Log final world position frequently
  static int position_log_count = 0;
  if (position_log_count < 30 || position_log_count % 10 == 0) {
    ABSL_LOG(INFO) << "[WORLD POS] " << instance.anchor_name 
                   << " final=[" << world_position.x << ", " << world_position.y << ", " << world_position.z << "]";
  }
  position_log_count++;
  
  // DEBUG: Log final position
  if (debug_log_count < 5 || debug_log_count % 60 == 0) {
    ABSL_LOG(INFO) << "[ANCHOR DEBUG] Final world_position=[" 
                   << world_position.x << "," << world_position.y << "," << world_position.z << "]";
  }
  
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
  
  // 1. Translate to final world position
  model = glm::translate(model, world_position);
  
  // 2. Apply head rotation from PnP (try re-enabling for tilt tracking)
  // Use the rotation to make models tilt with head orientation
  static int rotation_log_count = 0;
  if (rotation_log_count < 20 || rotation_log_count % 30 == 0) {
    glm::vec3 euler = glm::eulerAngles(head_pose.rotation);
    ABSL_LOG(INFO) << "[HEAD ROTATION] pitch=" << glm::degrees(euler.x) 
                   << "° yaw=" << glm::degrees(euler.y) 
                   << "° roll=" << glm::degrees(euler.z) << "°";
  }
  rotation_log_count++;
  
  model = model * glm::mat4_cast(head_pose.rotation);
  
  // 3. No hardcoded rotation - use filter.json rotation values instead
  
  // 4. Apply instance rotation (Euler angles from filter.json)
  model = glm::rotate(model, glm::radians(instance.rotation_euler.x), glm::vec3(1,0,0));  // Pitch
  model = glm::rotate(model, glm::radians(instance.rotation_euler.y), glm::vec3(0,1,0));  // Yaw
  model = glm::rotate(model, glm::radians(instance.rotation_euler.z), glm::vec3(0,0,1));  // Roll
  
  // 5. Apply scale
  model = glm::scale(model, instance.scale);
  
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
  
  // Use default material (can be enhanced later)
  Material default_material;
  default_material.ambient[0] = 0.3f;
  default_material.ambient[1] = 0.3f;
  default_material.ambient[2] = 0.3f;
  default_material.diffuse[0] = 0.8f;
  default_material.diffuse[1] = 0.8f;
  default_material.diffuse[2] = 0.8f;
  default_material.specular[0] = 0.5f;
  default_material.specular[1] = 0.5f;
  default_material.specular[2] = 0.5f;
  default_material.shininess = 32.0f;
  default_material.opacity = 1.0f;
  
  shader_->SetVec3("uMaterialAmbient", 
                   default_material.ambient[0],
                   default_material.ambient[1], 
                   default_material.ambient[2]);
  shader_->SetVec3("uMaterialDiffuse",
                   default_material.diffuse[0],
                   default_material.diffuse[1],
                   default_material.diffuse[2]);
  shader_->SetVec3("uMaterialSpecular",
                   default_material.specular[0],
                   default_material.specular[1],
                   default_material.specular[2]);
  shader_->SetFloat("uMaterialShininess", default_material.shininess);
  shader_->SetFloat("uMaterialOpacity", default_material.opacity);
  
  // Render each mesh
  for (const auto& mesh : model->meshes) {
    if (mesh.vao == 0) {
      continue;
    }
    
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
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
  shader_->SetVec3("uLightDirection", light_direction_[0], light_direction_[1], light_direction_[2]);
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
  ABSL_LOG(INFO) << "Crown depth offset set to " << offset << " (" << (offset * 100.0f) << "% " << (offset > 0 ? "backward" : "forward") << ")";
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
  if (face_landmarks_.size() > 10) {
    glColor3f(1.0f, 0.0f, 0.0f);
    glm::vec3 forehead = LandmarkToWorld(face_landmarks_[10]);
    glVertex3f(forehead.x, forehead.y, forehead.z);
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
  if (crown_debug_count < 5) {
    ABSL_LOG(INFO) << "[DEBUG] Crown world: [" << crown_world.x << "," << crown_world.y << "," << crown_world.z << "]";
  }
  
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
