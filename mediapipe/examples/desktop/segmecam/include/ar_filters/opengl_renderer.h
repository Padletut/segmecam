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
#include "ar_filters/midas_depth_estimator.h"  // Phase 8 Day 4: MiDaS depth estimation

// For OpenCV types (face landmarks)
#include <opencv2/core.hpp>

// GLM for transform calculations
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Forward declarations - no OpenGL headers in this public interface
namespace segmecam {
namespace ar_filters {
  struct Model;  // Already defined in model_loader.h but forward declare for clarity
  class TextureManager;  // For texture loading and caching
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
  bool flip_z;                       // Flip Z-axis to fix inside-out models
  
  // Cached rendering data (managed internally)
  const Model* model_ptr;            // Pointer to loaded model (non-owning)
  glm::mat4 cached_transform;        // Cached model matrix
  bool transform_dirty;              // Flag to recalculate transform
  
  ModelInstance()
      : offset(0.0f),
        rotation_euler(0.0f),
        scale(1.0f),
        visible(true),
        flip_z(false),
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
  bool first_frame;  // Skip smoothing on first frame
  
  TransformCache()
      : previous_mvp(1.0f),
        smoothing_alpha(0.3f),
        first_frame(true) {}
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
    // Render to an existing FBO with attached texture
  bool RenderToFBO(unsigned int fbo_id, unsigned int texture_id, int width, int height);
  
  // **NEW: Render to external texture (creates temporary FBO)**
  // Convenience method for rendering AR filters onto an existing video texture
  bool RenderToExternalTexture(unsigned int texture_id, int width, int height);

  // Model loading and instance management

  // **NEW: Model Instance Management (Phase 2 Step 2.2)**
  // Load a 3D model from file and cache it
  // Returns model ID on success, empty string on failure
  std::string LoadModel(const std::string& path);
  
  // Override the texture path for a loaded model
  // Returns true on success, false on failure
  bool SetModelTexture(const std::string& model_id, const std::string& texture_path);
  
  // Override the opacity map path for a loaded model
  bool SetModelOpacityMap(const std::string& model_id, const std::string& opacity_map_path);
  
  // Override the emissive map path for a loaded model
  bool SetModelEmissiveMap(const std::string& model_id, const std::string& emissive_map_path);
  
  // Create a new model instance attached to a face anchor
  // Returns instance ID on success, empty string on failure
  std::string CreateInstance(
      const std::string& model_id,
      const std::string& anchor,
      const glm::vec3& offset = glm::vec3(0.0f),
      const glm::vec3& rotation = glm::vec3(0.0f),
      const glm::vec3& scale = glm::vec3(1.0f),
      bool flip_z = false
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
  
  // **NEW: Supersampling control**
  // Enable high-resolution rendering (always render at fixed resolution regardless of camera)
  void EnableSupersampling(bool enable, int width = 1920, int height = 1080);
  bool IsSupersampling() const { return use_supersampling_; }
  
  // **NEW: Fullscreen quad rendering for compositing**
  void InitializeFullscreenQuad();
  void RenderFullscreenQuad();

  // **NEW: Face Landmark Integration (Phase 2 Step 2.3)**
  // Update face landmarks for anchor point calculation
  void UpdateFaceLandmarks(const std::vector<cv::Point3f>& landmarks);
  
  // **NEW: Phase 8 Day 4 - Update face landmarks WITH current frame for MiDaS depth**
  void UpdateFaceLandmarksWithFrame(const std::vector<cv::Point3f>& landmarks, 
                                     const cv::Mat& frame_rgb);
  
  // **NEW: Phase 8 Day 4 - MiDaS depth calibration**
  // Recalibrate MiDaS depth estimation at a known distance (in meters)
  // Call this when you're at a specific distance from the camera (e.g., exactly 1.0m)
  void RecalibrateMiDasDepth(float known_distance_meters);
  
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

  // **NEW: Debug Visualization (Finding #8)**
  // Enable/disable debug anchor visualization
  void SetDebugAnchorsEnabled(bool enabled);
  bool IsDebugAnchorsEnabled() const { return debug_anchors_enabled_; }
  
  // **NEW: Crown Anchor Adjustment (Finding #8)**
  // Adjust crown anchor offset multiplier (default 0.4 = 40% above forehead)
  void SetCrownOffsetMultiplier(float multiplier);
  float GetCrownOffsetMultiplier() const { return crown_offset_multiplier_; }
  
  // Adjust crown anchor depth offset (forward/backward, default 0.0)
  void SetCrownDepthOffset(float offset);
  float GetCrownDepthOffset() const { return crown_depth_offset_; }

  // Global scale for anchor-derived Z offset (reduces how far models sit off the face)
  // 1.0 = full landmark Z influence, 0.0 = glued to face plane. Default tuned lower for overlays.
  void SetAnchorZScale(float s) { anchor_z_scale_ = s; }
  float GetAnchorZScale() const { return anchor_z_scale_; }

  // A small constant bias (meters) towards the camera to reduce perceived gap from the face
  // Applied to non-crown anchors; positive values move models closer to the camera (less negative Z)
  void SetAnchorZBiasMeters(float m) { anchor_z_bias_meters_ = m; }
  float GetAnchorZBiasMeters() const { return anchor_z_bias_meters_; }

  // Keep model size proportional to face width across depth changes
  void SetScaleWithFaceWidth(bool enabled) { scale_with_face_width_ = enabled; }
  bool GetScaleWithFaceWidth() const { return scale_with_face_width_; }

  // Optional offset along face normal (meters). Positive moves toward camera along the face's normal.
  void SetFaceNormalOffset(float m) { face_normal_offset_m_ = m; }
  float GetFaceNormalOffset() const { return face_normal_offset_m_; }

  // Blend factor that pulls model Z toward the face plane to keep a stable face-relative distance.
  // Range [0..1]: 0 = no pull (original), 1 = glued to face depth. Default is moderate pull.
  void SetAnchorZFaceLerp(float t) { anchor_z_face_lerp_ = std::max(0.0f, std::min(1.0f, t)); }
  float GetAnchorZFaceLerp() const { return anchor_z_face_lerp_; }
  
  // Render debug markers for anchor points
  void RenderDebugAnchors(const HeadPose& head_pose);

  // Cleanup OpenGL resources
  void Cleanup();

 private:
  bool initialized_;
  int viewport_width_;
  int viewport_height_;

  // OpenGL resources (stored as opaque pointers to avoid header pollution)
  std::unique_ptr<render::ShaderProgram> shader_;          // 3D model shader
  std::unique_ptr<render::ShaderProgram> texture_shader_;  // 2D texture shader for compositing
  std::unique_ptr<TextureManager> texture_manager_;        // Texture loading and caching
  
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
  cv::Mat current_frame_rgb_;                                    // Current RGB frame for MiDaS (Phase 8 Day 4)
  
  // **NEW: MiDaS depth estimation (Phase 8 Day 4)**
  std::unique_ptr<MidasDepthEstimator> depth_estimator_;         // MiDaS neural depth estimator
  float midas_calibration_depth_;                                 // Calibration: known depth in meters (default 1.0m)
  float midas_calibration_inverse_;                               // Calibration: inverse depth at known distance
  bool midas_calibrated_;                                         // Calibration status
  
  // **NEW: Transform smoothing**
  std::map<std::string, TransformCache> transform_caches_;       // Per-instance smoothing
  HeadPose current_head_pose_;                                   // Cached head pose
  bool head_pose_valid_;                                         // Head pose validity flag
  
  // **NEW: Debug visualization**
  bool debug_anchors_enabled_;                                   // Show anchor debug markers
  float crown_offset_multiplier_;                                 // Crown anchor Y-offset (default 0.4)
  float crown_depth_offset_;                                      // Crown anchor Z-offset (default 0.0)
  float anchor_z_scale_;                                          // Scales anchor Z offset globally (default 0.25)
  float anchor_z_bias_meters_;                                    // Constant Z bias towards camera (default 0.02m)
  bool  scale_with_face_width_;                                   // Maintain size relative to face width
  float face_width_baseline_m_;                                   // Baseline face width measured in meters
  float face_normal_offset_m_;                                    // Offset along face normal (meters)
  float anchor_z_face_lerp_;                                      // Lerp toward face depth (default 0.5)
  
  // **NEW: High-resolution rendering (supersampling)**
  uint32_t supersample_fbo_;                                     // Framebuffer for high-res rendering
  uint32_t supersample_texture_;                                 // Color texture (RGBA)
  uint32_t supersample_depth_;                                   // Depth renderbuffer
  int supersample_width_;                                        // Supersampling resolution width (default 1920)
  int supersample_height_;                                       // Supersampling resolution height (default 1080)
  bool use_supersampling_;                                       // Enable/disable supersampling
  
  // **NEW: Fullscreen quad for compositing**
  uint32_t quad_vao_;                                            // Vertex Array Object for fullscreen quad
  uint32_t quad_vbo_;                                            // Vertex Buffer Object for fullscreen quad

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
