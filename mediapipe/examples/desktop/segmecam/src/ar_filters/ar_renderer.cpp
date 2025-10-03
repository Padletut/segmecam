// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// ARRenderer Implementation - Stub for Phase 4 Step 3

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/ar_renderer.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/model_loader.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/texture_manager.h"
#include "mediapipe/examples/desktop/segmecam/include/ar_filters/filter_asset.h"  // Phase 6
#include "mediapipe/examples/desktop/segmecam/include/render/fbo_manager.h"

#include "absl/log/absl_log.h"
#include "absl/log/log.h"
#include "absl/status/status.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

// OpenGL headers for rendering
#include <epoxy/gl.h>
#include <opencv2/opencv.hpp>
#include <chrono>

namespace segmecam {
namespace ar_filters {

// Constructors
ARRenderer::ARRenderer() : ARRenderer(ARConfig()) {}

ARRenderer::ARRenderer(const ARConfig& config)
    : config_(config), initialized_(false), debug_mode_(false),
      viewport_x_(0), viewport_y_(0),
      viewport_width_(config.render_width),
      viewport_height_(config.render_height),
      last_render_time_ms_(0.0f),
      last_models_rendered_(0),
      last_triangles_rendered_(0) {}

ARRenderer::~ARRenderer() {
  Cleanup();
}

// Initialization
absl::Status ARRenderer::Initialize() {
  if (initialized_) {
    return absl::OkStatus();
  }

  model_loader_ = std::make_unique<ModelLoader>();
  texture_manager_ = std::make_unique<TextureManager>();
  fbo_manager_ = std::make_unique<render::FBOManager>();

  auto setup_status = SetupRenderTarget();
  if (!setup_status.ok()) {
    return setup_status;
  }

  initialized_ = true;
  ABSL_LOG(INFO) << "ARRenderer initialized";
  return absl::OkStatus();
}

void ARRenderer::Cleanup() {
  if (!initialized_) return;
  
  model_instances_.clear();
  fbo_manager_.reset();
  texture_manager_.reset();
  model_loader_.reset();
  
  initialized_ = false;
}

// Model management
absl::Status ARRenderer::LoadModel(const std::string& name, const std::string& model_path) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Not initialized");
  }
  
  auto result = model_loader_->LoadModel(model_path);
  if (!result.ok()) {
    return result.status();
  }
  
  ABSL_LOG(INFO) << "Loaded model: " << name;
  return absl::OkStatus();
}

absl::Status ARRenderer::UnloadModel(const std::string& name) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Not initialized");
  }
  
  // Remove instances using this model
  auto it = model_instances_.begin();
  while (it != model_instances_.end()) {
    if (it->second.model_name == name) {
      it = model_instances_.erase(it);
    } else {
      ++it;
    }
  }
  
  return absl::OkStatus();
}

bool ARRenderer::HasModel(const std::string& name) const {
  for (const auto& pair : model_instances_) {
    if (pair.second.model_name == name) {
      return true;
    }
  }
  return false;
}

// Phase 6: Filter asset integration
absl::Status ARRenderer::LoadFilter(const FilterAsset& filter) {
  if (!initialized_) {
    return absl::FailedPreconditionError("ARRenderer not initialized");
  }
  
  const auto& metadata = filter.GetMetadata();
  const auto& attachments = filter.GetAttachments();
  const auto& materials = filter.GetMaterials();
  
  ABSL_LOG(INFO) << "Loading filter: " << metadata.name << " (" << metadata.id << ")";
  
  // Check if filter already loaded
  if (loaded_filters_.count(metadata.id) > 0) {
    return absl::AlreadyExistsError("Filter already loaded: " + metadata.id);
  }
  
  std::vector<std::string> instance_names;
  
  // Load each attachment as a model instance
  for (const auto& attachment : attachments) {
    // Construct full model path from filter directory
    std::string model_path = filter.GetFilterDirectory() + "/" + attachment.model_path;
    
    // Load the model
    auto load_result = model_loader_->LoadModel(model_path);
    if (!load_result.ok()) {
      ABSL_LOG(WARNING) << "Failed to load model for attachment " 
                        << attachment.id << ": " << load_result.status();
      // Continue loading other attachments
      continue;
    }
    
    // Create model instance
    ModelInstance instance;
    instance.model_name = attachment.id;
    instance.model_path = model_path;
    instance.attachment_anchor = attachment.anchor_name;
    instance.visible = attachment.visible;
    instance.opacity = attachment.opacity;
    instance.attach_to_landmarks = true;
    
    // Set transform from attachment
    instance.position_offset = attachment.offset;
    instance.scale_factor = attachment.scale;
    
    // Convert Euler angles (degrees) to quaternion
    glm::vec3 rotation_rad = glm::radians(attachment.rotation);
    instance.rotation_quat = glm::quat(rotation_rad);
    
    // Store instance
    std::string instance_name = metadata.id + "_" + attachment.id;
    model_instances_[instance_name] = instance;
    instance_names.push_back(instance_name);
    
    ABSL_LOG(INFO) << "  Loaded attachment: " << attachment.id 
                   << " (anchor: " << attachment.anchor_name << ")";
    
    // Load texture if specified
    if (!attachment.texture_path.empty()) {
      std::string texture_path = filter.GetFilterDirectory() + "/" + attachment.texture_path;
      auto tex_result = texture_manager_->LoadTexture(texture_path);
      if (!tex_result.ok()) {
        ABSL_LOG(WARNING) << "Failed to load texture: " << texture_path;
      } else {
        ABSL_LOG(INFO) << "    Loaded texture: " << attachment.texture_path;
      }
    }
  }
  
  // Track loaded filter
  loaded_filters_[metadata.id] = instance_names;
  
  ABSL_LOG(INFO) << "Filter loaded successfully: " << metadata.name 
                 << " (" << instance_names.size() << " attachments)";
  
  return absl::OkStatus();
}

absl::Status ARRenderer::UnloadFilter(const std::string& filter_id) {
  if (!initialized_) {
    return absl::FailedPreconditionError("ARRenderer not initialized");
  }
  
  auto it = loaded_filters_.find(filter_id);
  if (it == loaded_filters_.end()) {
    return absl::NotFoundError("Filter not loaded: " + filter_id);
  }
  
  // Remove all instances for this filter
  for (const auto& instance_name : it->second) {
    model_instances_.erase(instance_name);
    ABSL_LOG(INFO) << "  Unloaded instance: " << instance_name;
  }
  
  loaded_filters_.erase(it);
  
  ABSL_LOG(INFO) << "Filter unloaded: " << filter_id;
  return absl::OkStatus();
}

bool ARRenderer::HasFilter(const std::string& filter_id) const {
  return loaded_filters_.count(filter_id) > 0;
}

// Model instance management
absl::Status ARRenderer::CreateModelInstance(const std::string& instance_name,
                                             const std::string& model_name) {
  ModelInstance instance;
  return CreateModelInstance(instance_name, model_name, instance);
}

absl::Status ARRenderer::CreateModelInstance(const std::string& instance_name,
                                             const std::string& model_name,
                                             const ModelInstance& config) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Not initialized");
  }
  
  if (model_instances_.count(instance_name)) {
    return absl::AlreadyExistsError("Instance exists: " + instance_name);
  }
  
  ModelInstance instance = config;
  instance.model_name = model_name;
  model_instances_[instance_name] = instance;
  
  return absl::OkStatus();
}

absl::Status ARRenderer::UpdateModelInstance(const std::string& instance_name,
                                             const ModelInstance& config) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Not initialized");
  }
  
  auto it = model_instances_.find(instance_name);
  if (it == model_instances_.end()) {
    return absl::NotFoundError("Instance not found: " + instance_name);
  }
  
  std::string saved_model = it->second.model_name;
  it->second = config;
  it->second.model_name = saved_model;
  
  return absl::OkStatus();
}

void ARRenderer::RemoveModelInstance(const std::string& instance_name) {
  model_instances_.erase(instance_name);
}

// Phase 7: Behavior system integration methods
void ARRenderer::SetModelInstanceOffset(const std::string& instance_name, const glm::vec3& offset) {
  auto it = model_instances_.find(instance_name);
  if (it != model_instances_.end()) {
    it->second.position_offset = offset;
    // Recalculate transform matrix
    it->second.transform = glm::translate(glm::mat4(1.0f), offset) * 
                           glm::mat4_cast(it->second.rotation_quat) *
                           glm::scale(glm::mat4(1.0f), it->second.scale_factor);
  }
}

void ARRenderer::SetModelInstanceScale(const std::string& instance_name, float scale) {
  auto it = model_instances_.find(instance_name);
  if (it != model_instances_.end()) {
    it->second.scale_factor = glm::vec3(scale);
    // Recalculate transform matrix
    it->second.transform = glm::translate(glm::mat4(1.0f), it->second.position_offset) * 
                           glm::mat4_cast(it->second.rotation_quat) *
                           glm::scale(glm::mat4(1.0f), it->second.scale_factor);
  }
}

void ARRenderer::SetModelInstanceScaleVec(const std::string& instance_name, const glm::vec3& scale) {
  auto it = model_instances_.find(instance_name);
  if (it != model_instances_.end()) {
    it->second.scale_factor = scale;
    // Recalculate transform matrix
    it->second.transform = glm::translate(glm::mat4(1.0f), it->second.position_offset) * 
                           glm::mat4_cast(it->second.rotation_quat) *
                           glm::scale(glm::mat4(1.0f), it->second.scale_factor);
  }
}

void ARRenderer::SetModelInstanceVisibility(const std::string& instance_name, bool visible) {
  auto it = model_instances_.find(instance_name);
  if (it != model_instances_.end()) {
    it->second.visible = visible;
  }
}

void ARRenderer::SetModelInstanceRotation(const std::string& instance_name, const glm::vec3& rotation) {
  auto it = model_instances_.find(instance_name);
  if (it != model_instances_.end()) {
    // Convert Euler angles to quaternion
    glm::quat quat_x = glm::angleAxis(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat quat_y = glm::angleAxis(rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat quat_z = glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    it->second.rotation_quat = quat_z * quat_y * quat_x;
    
    // Recalculate transform matrix
    it->second.transform = glm::translate(glm::mat4(1.0f), it->second.position_offset) * 
                           glm::mat4_cast(it->second.rotation_quat) *
                           glm::scale(glm::mat4(1.0f), it->second.scale_factor);
  }
}

void ARRenderer::SetModelInstanceColorTint(const std::string& instance_name, const glm::vec3& color) {
  auto it = model_instances_.find(instance_name);
  if (it != model_instances_.end()) {
    // Store color tint for shader use
    // Note: Actual shader integration would apply this in rendering pass
    // For now, we just log it
    LOG(INFO) << "Color tint set for " << instance_name << ": (" 
              << color.r << ", " << color.g << ", " << color.b << ")";
  }
}

void ARRenderer::ResetModelInstanceTransform(const std::string& instance_name) {
  auto it = model_instances_.find(instance_name);
  if (it != model_instances_.end()) {
    it->second.position_offset = glm::vec3(0.0f);
    it->second.scale_factor = glm::vec3(1.0f);
    it->second.rotation_quat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity
    it->second.transform = glm::mat4(1.0f);
    it->second.visible = true;
  }
}

// Face landmarks
absl::Status ARRenderer::AttachModelToLandmarks(const std::string& instance_name,
                                                const std::vector<int>& landmark_indices) {
  auto it = model_instances_.find(instance_name);
  if (it == model_instances_.end()) {
    return absl::NotFoundError("Instance not found");
  }
  
  it->second.attach_to_landmarks = true;
  return absl::OkStatus();
}

absl::Status ARRenderer::UpdateFaceLandmarks(const std::vector<float>& landmarks_3d) {
  current_face_landmarks_ = landmarks_3d;
  return absl::OkStatus();
}

// Rendering
absl::StatusOr<ARRenderer::RenderResult> ARRenderer::RenderToTexture(
    const uint8_t* input_frame, int frame_width, int frame_height) {
  RenderResult result;
  result.success = false;
  result.models_rendered = 0;
  result.triangles_rendered = 0;
  result.render_time_ms = 0.0f;
  
  if (!initialized_) {
    result.error_message = "Not initialized";
    return result;
  }

  // Start timing
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Step 1: Update model transforms from current face landmarks
  UpdateInstanceTransformsFromLandmarks();
  
  // Step 2: Render 3D models to offscreen FBO
  auto render_status = RenderModelInstances();
  if (!render_status.ok()) {
    result.error_message = std::string(render_status.message());
    return result;
  }
  
  // Step 3: Composite AR layer with video background
  auto composite_status = CompositeWithBackground(input_frame, frame_width, frame_height);
  if (!composite_status.ok()) {
    result.error_message = std::string(composite_status.message());
    return result;
  }
  
  // Calculate render time
  auto end_time = std::chrono::high_resolution_clock::now();
  result.render_time_ms = std::chrono::duration<float, std::milli>(
      end_time - start_time).count();
  
  // Update result with rendering statistics
  result.success = true;
  result.output_texture_id = fbo_manager_->GetColorTexture(render_fbo_name_);
  result.models_rendered = last_models_rendered_;
  result.triangles_rendered = last_triangles_rendered_;
  
  // Track performance
  last_render_time_ms_ = result.render_time_ms;
  
  return result;
}

absl::StatusOr<ARRenderer::RenderResult> ARRenderer::RenderModelsOnly() {
  RenderResult result;
  result.success = true;
  return result;
}

// Configuration
void ARRenderer::UpdateConfig(const ARConfig& config) {
  config_ = config;
}

// Statistics
void ARRenderer::GetRenderStatistics(int* total_models, int* visible_instances,
                                     size_t* gpu_memory_used) const {
  if (total_models) {
    std::set<std::string> unique;
    for (const auto& pair : model_instances_) {
      unique.insert(pair.second.model_name);
    }
    *total_models = unique.size();
  }
  
  if (visible_instances) {
    int count = 0;
    for (const auto& pair : model_instances_) {
      if (pair.second.visible) count++;
    }
    *visible_instances = count;
  }
  
  if (gpu_memory_used) {
    *gpu_memory_used = texture_manager_ ? texture_manager_->GetTotalGPUMemoryUsed() : 0;
  }
}

// Viewport
void ARRenderer::SetViewport(int x, int y, int width, int height) {
  viewport_x_ = x;
  viewport_y_ = y;
  viewport_width_ = width;
  viewport_height_ = height;
}

void ARRenderer::GetViewport(int* x, int* y, int* width, int* height) const {
  if (x) *x = viewport_x_;
  if (y) *y = viewport_y_;
  if (width) *width = viewport_width_;
  if (height) *height = viewport_height_;
}

// Private methods
absl::Status ARRenderer::SetupRenderTarget() {
  if (!fbo_manager_) {
    return absl::InternalError("FBO manager not initialized");
  }
  
  render::FBOManager::FBOConfig fbo_config;
  fbo_config.width = config_.render_width;
  fbo_config.height = config_.render_height;
  fbo_config.use_depth_buffer = config_.enable_depth_testing;
  fbo_config.use_multisampling = config_.use_multisampling;
  fbo_config.sample_count = config_.msaa_samples;
  
  auto result = fbo_manager_->CreateFBO("render_target", fbo_config);
  if (!result.ok()) {
    return result.status();
  }
  
  render_fbo_name_ = "render_target";
  return absl::OkStatus();
}

absl::Status ARRenderer::RenderModelInstances() {
  if (render_fbo_name_.empty() || model_instances_.empty()) {
    return absl::OkStatus();  // Nothing to render
  }

  // Bind the offscreen FBO for rendering
  bool bind_success = fbo_manager_->BindFramebuffer(render_fbo_name_);
  if (!bind_success) {
    return absl::InternalError("Failed to bind render FBO: " + render_fbo_name_);
  }

  // Clear with transparent background (important for alpha blending)
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Setup OpenGL state for 3D rendering
  if (config_.enable_depth_testing) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
  }
  
  // Enable alpha blending for transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  // Enable face culling (render only front faces)
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);  // Counter-clockwise winding

  // Setup viewport
  glViewport(0, 0, config_.render_width, config_.render_height);

  // Setup view matrix (identity - we're rendering in screen space)
  glm::mat4 view = glm::mat4(1.0f);
  
  // Setup projection matrix (orthographic for 2D overlay on video)
  // Map [0, width] x [0, height] to normalized device coordinates [-1, 1]
  glm::mat4 projection = glm::ortho(
      0.0f, static_cast<float>(config_.render_width),
      0.0f, static_cast<float>(config_.render_height),
      -100.0f, 100.0f  // Near and far planes
  );

  // Track rendering statistics
  int triangles_rendered = 0;
  int models_rendered = 0;

  // Render each visible model instance
  for (const auto& [instance_name, instance] : model_instances_) {
    if (!instance.visible) {
      continue;
    }

    // Skip if model not loaded
    // TODO: In future, load model on-demand via model_loader_
    // For now, we assume models are pre-loaded
    
    // Calculate MVP matrix for this instance
    glm::mat4 mvp = projection * view * instance.transform;
    
    // TODO Phase 5 Day 2: Actual model rendering
    // This requires integrating with ModelLoader to get VAO/VBO data
    // For now, we track that we would render this instance
    models_rendered++;
    
    // Placeholder: In a full implementation, we would:
    // 1. Get Model data from model_loader_ (VAO, VBO, materials)
    // 2. Bind VAO: glBindVertexArray(mesh.vao)
    // 3. Setup shader uniforms (MVP matrix, opacity, lighting)
    // 4. Bind textures from texture_manager_
    // 5. Draw: glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, 0)
    // 6. Track triangle count
  }

  // Unbind FBO (restore default framebuffer)
  fbo_manager_->UnbindFramebuffer();

  // Update statistics
  last_models_rendered_ = models_rendered;
  last_triangles_rendered_ = triangles_rendered;

  return absl::OkStatus();
}

absl::Status ARRenderer::CompositeWithBackground(const uint8_t* background_frame,
                                                 int frame_width, int frame_height) {
  if (!background_frame) {
    return absl::InvalidArgumentError("Background frame is null");
  }

  if (render_fbo_name_.empty()) {
    return absl::FailedPreconditionError("Render FBO not initialized");
  }

  // Get the rendered AR layer texture from FBO
  GLuint ar_texture_id = fbo_manager_->GetColorTexture(render_fbo_name_);
  if (ar_texture_id == 0) {
    return absl::InternalError("Failed to get AR texture from FBO");
  }

  // Download AR layer from GPU to CPU
  // Create OpenCV mat to hold the AR layer (RGBA format)
  cv::Mat ar_layer(config_.render_height, config_.render_width, CV_8UC4);
  
  // Bind the texture and read pixels
  glBindTexture(GL_TEXTURE_2D, ar_texture_id);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, ar_layer.data);
  glBindTexture(GL_TEXTURE_2D, 0);
  
  // Flip vertically (OpenGL origin is bottom-left, OpenCV is top-left)
  cv::flip(ar_layer, ar_layer, 0);

  // Create OpenCV mat from background frame (assuming RGB format)
  cv::Mat background(frame_height, frame_width, CV_8UC3, 
                     const_cast<uint8_t*>(background_frame));

  // Resize AR layer to match background if needed
  if (ar_layer.cols != frame_width || ar_layer.rows != frame_height) {
    cv::resize(ar_layer, ar_layer, cv::Size(frame_width, frame_height));
  }

  // Convert background to RGBA for alpha blending
  cv::Mat background_rgba;
  cv::cvtColor(background, background_rgba, cv::COLOR_RGB2RGBA);

  // Composite: blend AR layer over background using alpha channel
  // For each pixel: output = bg * (1 - alpha) + fg * alpha
  for (int y = 0; y < frame_height; y++) {
    for (int x = 0; x < frame_width; x++) {
      cv::Vec4b& bg_pixel = background_rgba.at<cv::Vec4b>(y, x);
      const cv::Vec4b& ar_pixel = ar_layer.at<cv::Vec4b>(y, x);
      
      float alpha = ar_pixel[3] / 255.0f;  // Alpha channel (0-1)
      
      // Blend each channel
      for (int c = 0; c < 3; c++) {  // RGB channels only
        bg_pixel[c] = static_cast<uint8_t>(
            bg_pixel[c] * (1.0f - alpha) + ar_pixel[c] * alpha
        );
      }
    }
  }

  // Convert back to RGB for output
  cv::Mat output_rgb;
  cv::cvtColor(background_rgba, output_rgb, cv::COLOR_RGBA2RGB);

  // Copy result back to background_frame buffer
  std::memcpy(const_cast<uint8_t*>(background_frame), 
              output_rgb.data, 
              frame_width * frame_height * 3);

  return absl::OkStatus();
}

// GPU texture readback - Phase 8 Day 3
absl::StatusOr<cv::Mat> ARRenderer::ReadFramebufferToMat(
    const std::string& fbo_name, int width, int height) const {
  
  if (!fbo_manager_) {
    return absl::FailedPreconditionError("FBO manager not initialized");
  }
  
  // Allocate buffer for pixel data (RGBA format)
  std::vector<uint8_t> pixel_buffer(width * height * 4);
  
  // Read pixels from FBO using FBOManager
  auto read_status = fbo_manager_->ReadPixels(
      fbo_name, 
      0, 0,          // x, y offset
      width, height, // read dimensions
      pixel_buffer.data(), 
      pixel_buffer.size());
  
  if (!read_status.ok()) {
    return read_status;
  }
  
  // Create cv::Mat from pixel buffer
  // OpenGL reads bottom-to-top, so we need to flip vertically
  cv::Mat rgba_image(height, width, CV_8UC4, pixel_buffer.data());
  
  // Flip vertically (OpenGL Y-axis is inverted relative to OpenCV)
  cv::Mat rgba_flipped;
  cv::flip(rgba_image, rgba_flipped, 0);  // 0 = flip around x-axis (vertical flip)
  
  // Convert RGBA to BGR (OpenCV standard format)
  cv::Mat bgr_image;
  cv::cvtColor(rgba_flipped, bgr_image, cv::COLOR_RGBA2BGR);
  
  // Return a deep copy since pixel_buffer is temporary
  return bgr_image.clone();
}

void ARRenderer::UpdateInstanceTransformsFromLandmarks() {
  if (current_face_landmarks_.empty() || model_instances_.empty()) {
    return;
  }
  
  // Parse landmarks (468 points x 3 coords = 1404 floats)
  std::vector<cv::Point3f> landmark_points;
  landmark_points.reserve(468);
  
  for (size_t i = 0; i + 2 < current_face_landmarks_.size(); i += 3) {
    landmark_points.emplace_back(
        current_face_landmarks_[i],
        current_face_landmarks_[i + 1],
        current_face_landmarks_[i + 2]
    );
  }
  
  // Update each model instance's transform based on its anchor point
  for (auto& [id, instance] : model_instances_) {
    if (!instance.visible) continue;
    
    // Get anchor position from landmarks
    cv::Point3f anchor_pos = GetAnchorPosition(landmark_points, instance.attachment_anchor);
    
    // Build transform matrix: Translation * Rotation * Scale
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), 
        glm::vec3(anchor_pos.x, anchor_pos.y, anchor_pos.z));
    
    // Apply any user-defined offset
    translation = glm::translate(translation, instance.position_offset);
    
    // Apply rotation (from instance or head pose)
    glm::mat4 rotation = glm::mat4_cast(instance.rotation_quat);
    
    // Apply scale
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), instance.scale_factor);
    
    // Combine: T * R * S
    glm::mat4 new_transform = translation * rotation * scale;
    
    // Smooth transform to reduce jitter (exponential moving average)
    float smoothing = 0.3f;  // Lower = smoother but more lag
    
    // Interpolate each matrix element separately (glm::mix doesn't work on mat4)
    for (int col = 0; col < 4; col++) {
      for (int row = 0; row < 4; row++) {
        instance.transform[col][row] = glm::mix(
            instance.last_transform[col][row],
            new_transform[col][row],
            smoothing);
      }
    }
    instance.last_transform = new_transform;
  }
}

void ARRenderer::CalculateModelTransform(const ModelInstance& instance,
                                        float transform_matrix[16]) const {
  // TODO: Implement transform calculation
}

absl::Status ARRenderer::EnsureResourcesLoaded(const std::string& instance_name) {
  auto it = model_instances_.find(instance_name);
  if (it == model_instances_.end()) {
    return absl::NotFoundError("Instance not found");
  }
  
  return absl::OkStatus();
}

void ARRenderer::CleanupUnusedResources() {
  // TODO: Implement resource cleanup
}

cv::Point3f ARRenderer::GetAnchorPosition(
    const std::vector<cv::Point3f>& landmarks,
    const std::string& anchor_name) const {
  
  if (landmarks.size() < 468) {
    ABSL_LOG(WARNING) << "Insufficient landmarks: " << landmarks.size();
    return cv::Point3f(0, 0, 0);
  }
  
  // Map anchor names to landmark indices based on MediaPipe Face Mesh
  if (anchor_name == "nose_bridge" || anchor_name == "nose") {
    // Average of nose bridge landmarks (6, 197, 195)
    return (landmarks[6] + landmarks[197] + landmarks[195]) / 3.0f;
  }
  else if (anchor_name == "left_ear") {
    // Average of left ear landmarks (234, 127, 162)
    return (landmarks[234] + landmarks[127] + landmarks[162]) / 3.0f;
  }
  else if (anchor_name == "right_ear") {
    // Average of right ear landmarks (454, 356, 389)
    return (landmarks[454] + landmarks[356] + landmarks[389]) / 3.0f;
  }
  else if (anchor_name == "forehead" || anchor_name == "top_head") {
    // Forehead center (landmark 10)
    return landmarks[10];
  }
  else if (anchor_name == "chin") {
    // Chin (landmark 152)
    return landmarks[152];
  }
  else if (anchor_name == "left_eye") {
    // Left eye center (average of 33, 133, 160, 159, 158, 157)
    return (landmarks[33] + landmarks[133] + landmarks[160] + 
            landmarks[159] + landmarks[158] + landmarks[157]) / 6.0f;
  }
  else if (anchor_name == "right_eye") {
    // Right eye center (average of 362, 263, 387, 386, 385, 384)
    return (landmarks[362] + landmarks[263] + landmarks[387] + 
            landmarks[386] + landmarks[385] + landmarks[384]) / 6.0f;
  }
  else if (anchor_name == "mouth") {
    // Mouth center (average of 13, 14, 78, 308)
    return (landmarks[13] + landmarks[14] + landmarks[78] + landmarks[308]) / 4.0f;
  }
  else if (anchor_name == "center" || anchor_name == "face_center") {
    // Face center (average of key central landmarks)
    return (landmarks[1] + landmarks[4] + landmarks[10] + landmarks[152]) / 4.0f;
  }
  else {
    ABSL_LOG(WARNING) << "Unknown anchor: " << anchor_name << ", using face center";
    return (landmarks[1] + landmarks[4] + landmarks[10] + landmarks[152]) / 4.0f;
  }
}

} // namespace ar_filters
} // namespace segmecam
