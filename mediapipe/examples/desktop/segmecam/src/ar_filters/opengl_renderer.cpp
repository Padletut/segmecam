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
      head_pose_valid_(false) {
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
  ModelLoader loader;
  auto model_result = loader.LoadModel(path);
  if (!model_result.ok()) {
    ABSL_LOG(ERROR) << "Failed to load model: " << path << " - " << model_result.status();
    return "";
  }
  
  // Cache model
  auto model = std::make_unique<Model>(std::move(*model_result));
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
  
  // Calculate face scale from eye distance
  if (landmarks.size() > 263) {
    cv::Point3f left_eye = landmarks[33];
    cv::Point3f right_eye = landmarks[263];
    float eye_distance = cv::norm(left_eye - right_eye);
    face_scale_factor_ = eye_distance * 10.0f;  // Scale to reasonable size
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
    return pose;
  }
  
  // Convert rotation vector to matrix
  cv::Mat rotation_mat;
  cv::Rodrigues(rotation_vec, rotation_mat);
  
  // Extract Euler angles (ZYX convention)
  double sy = std::sqrt(rotation_mat.at<double>(0,0) * rotation_mat.at<double>(0,0) +
                        rotation_mat.at<double>(1,0) * rotation_mat.at<double>(1,0));
  
  pose.euler_angles.x = std::atan2(rotation_mat.at<double>(2,1), rotation_mat.at<double>(2,2));  // Pitch
  pose.euler_angles.y = std::atan2(-rotation_mat.at<double>(2,0), sy);  // Yaw
  pose.euler_angles.z = std::atan2(rotation_mat.at<double>(1,0), rotation_mat.at<double>(0,0));  // Roll
  
  // Convert to quaternion
  pose.rotation = glm::quat(glm::vec3(pose.euler_angles.x, pose.euler_angles.y, pose.euler_angles.z));
  
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
  // Get anchor position
  cv::Point3f anchor = GetAnchorPosition(instance.anchor_name);
  
  // Convert anchor to world space (normalized coordinates to pixels)
  glm::vec3 anchor_world = glm::vec3(
      anchor.x * viewport_width_,
      anchor.y * viewport_height_,
      anchor.z * face_scale_factor_
  );
  
  // Apply head rotation to offset
  glm::vec3 rotated_offset = head_pose.rotation * instance.offset;
  
  // Build model matrix
  glm::mat4 model = glm::mat4(1.0f);
  
  // 1. Translate to anchor position
  model = glm::translate(model, anchor_world + rotated_offset);
  
  // 2. Apply head rotation
  model = model * glm::mat4_cast(head_pose.rotation);
  
  // 3. Apply instance rotation (Euler angles)
  model = glm::rotate(model, glm::radians(instance.rotation_euler.x), glm::vec3(1,0,0));  // Pitch
  model = glm::rotate(model, glm::radians(instance.rotation_euler.y), glm::vec3(0,1,0));  // Yaw
  model = glm::rotate(model, glm::radians(instance.rotation_euler.z), glm::vec3(0,0,1));  // Roll
  
  // 4. Apply scale
  model = glm::scale(model, instance.scale);
  
  return model;
}

void OpenGLRenderer::RenderModelInstance(const ModelInstance& instance, const HeadPose& head_pose) {
  if (!instance.visible || !instance.model_ptr) {
    return;
  }
  
  const Model* model = instance.model_ptr;
  if (model->meshes.empty()) {
    return;
  }
  
  // Calculate model matrix
  glm::mat4 model_matrix = CalculateInstanceTransform(instance, head_pose);
  
  // Apply transform smoothing
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
  
  if (instances_.empty()) {
    return 0;
  }
  
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
      head_pose.rotation = glm::quat(glm::vec3(0.0f, 0.0f, head_pose.euler_angles.z));
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
  for (const auto& pair : instances_) {
    const ModelInstance& instance = pair.second;
    if (instance.visible) {
      RenderModelInstance(instance, head_pose);
      rendered_count++;
    }
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

} // namespace ar_filters
} // namespace segmecam
