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
#include <map>

// Include Material for complete type definition
#include "ar_filters/model_loader.h"

// For OpenCV types (face landmarks)
#include <opencv2/core.hpp>

// GLM for transform calculations
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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

// Model instance configuration for AR filter rendering
struct ModelInstance {
  std::string id;                    // Unique instance identifier
  std::string model_path;            // Path to 3D model file
  std::string anchor_name;           // Face anchor point (nose_bridge, forehead, etc.)
  glm::vec3 offset;                  // Position offset from anchor
  glm::vec3 rotation_euler;          // Rotation in degrees (pitch, yaw, roll)
  glm::vec3 scale;                   // Scale factor
  bool visible;                      // Visibility flag
  
  // Cached rendering data (managed internally)
  const Model* model_ptr;            // Pointer to loaded model (non-owning)
  glm::mat4 cached_transform;        // Cached model matrix
  bool transform_dirty;              // Flag to recalculate transform
  
  ModelInstance()
      : offset(0.0f),
        rotation_euler(0.0f),
        scale(1.0f),
        visible(true),
        model_ptr(nullptr),
        cached_transform(1.0f),
        transform_dirty(true) {}
};

// Head pose tracking result from PnP algorithm
struct HeadPose {
  glm::vec3 euler_angles;            // Pitch, yaw, roll in radians
  glm::quat rotation;                // Quaternion representation
  glm::vec3 translation;             // Head position in camera space
  float confidence;                  // 0-1 tracking confidence
  
  HeadPose()
      : euler_angles(0.0f),
        rotation(1.0f, 0.0f, 0.0f, 0.0f),
        translation(0.0f),
        confidence(0.0f) {}
};

// Transform cache for smoothing
struct TransformCache {
  glm::mat4 previous_mvp;
  float smoothing_alpha;
  
  TransformCache()
      : previous_mvp(1.0f),
        smoothing_alpha(0.3f) {}
};

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

  // **NEW: FBO Rendering (Phase 2 Step 2.1)**
  // Render to framebuffer object for compositing
  bool RenderToFBO(unsigned int fbo_id, unsigned int texture_id, int width, int height);

  // **NEW: Model Instance Management (Phase 2 Step 2.2)**
  // Load a 3D model from file and cache it
  // Returns model ID on success, empty string on failure
  std::string LoadModel(const std::string& path);
  
  // Create a new model instance attached to a face anchor
  // Returns instance ID on success, empty string on failure
  std::string CreateInstance(
      const std::string& model_id,
      const std::string& anchor,
      const glm::vec3& offset = glm::vec3(0.0f),
      const glm::vec3& rotation = glm::vec3(0.0f),
      const glm::vec3& scale = glm::vec3(1.0f)
  );
  
  // Update instance properties
  void SetInstanceVisible(const std::string& instance_id, bool visible);
  void SetInstanceOffset(const std::string& instance_id, const glm::vec3& offset);
  void SetInstanceRotation(const std::string& instance_id, const glm::vec3& rotation);
  void SetInstanceScale(const std::string& instance_id, const glm::vec3& scale);
  
  // Remove an instance
  void RemoveInstance(const std::string& instance_id);
  
  // Clear all instances
  void ClearAllInstances();

  // **NEW: Face Landmark Integration (Phase 2 Step 2.3)**
  // Update face landmarks for anchor point calculation
  void UpdateFaceLandmarks(const std::vector<cv::Point3f>& landmarks);
  
  // Get anchor position from face landmarks
  cv::Point3f GetAnchorPosition(const std::string& anchor_name) const;

  // **NEW: Head Pose Tracking (Phase 3)**
  // Calculate head pose from face landmarks using PnP
  HeadPose CalculateHeadPose(int image_width, int image_height);

  // Render all model instances with head pose tracking
  int RenderInstances();

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

  // **NEW: Model instance management**
  std::map<std::string, std::unique_ptr<Model>> loaded_models_;  // Model cache
  std::map<std::string, ModelInstance> instances_;               // Active instances
  int next_instance_id_;                                         // ID counter
  
  // **NEW: Face landmark tracking**
  std::vector<cv::Point3f> face_landmarks_;                      // Current face landmarks
  std::vector<cv::Point3f> canonical_face_model_;                // 3D canonical face model for PnP
  float face_scale_factor_;                                      // Calculated from eye distance
  cv::Mat camera_matrix_;                                        // Camera intrinsics
  cv::Mat dist_coeffs_;                                          // Distortion coefficients
  
  // **NEW: Transform smoothing**
  std::map<std::string, TransformCache> transform_caches_;       // Per-instance smoothing
  HeadPose current_head_pose_;                                   // Cached head pose
  bool head_pose_valid_;                                         // Head pose validity flag

  // Internal helper: Render a single model
  void RenderSingleModel(const RenderCommand& command);
  
  // Internal helper: Setup GL state for rendering
  void SetupGLState();
  
  // Internal helper: Restore GL state after rendering
  void RestoreGLState();
  
  // **NEW: Internal helpers for Phase 2**
  // Initialize canonical face model for PnP
  void InitializeCanonicalModel();
  
  // Calculate model matrix for an instance
  glm::mat4 CalculateInstanceTransform(const ModelInstance& instance, const HeadPose& head_pose);
  
  // Apply transform smoothing
  glm::mat4 GetSmoothedTransform(const glm::mat4& current, TransformCache& cache);
  
  // Render a single model instance
  void RenderModelInstance(const ModelInstance& instance, const HeadPose& head_pose);
};

} // namespace ar_filters
} // namespace segmecam

#endif // SEGMECAM_AR_FILTERS_OPENGL_RENDERER_H_
