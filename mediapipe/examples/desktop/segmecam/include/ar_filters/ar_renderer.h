// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// ARRenderer - Phase 4 Step 3: Integration Layer
//
// Integrates ModelLoader → TextureManager → FBOManager → OpenGLRenderer
// for complete 3D model rendering pipeline with AR compositing

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <opencv2/core.hpp>
#include "absl/status/statusor.h"
#include "absl/status/status.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

// Forward declarations to avoid circular dependencies
namespace segmecam {
namespace ar_filters {
class TextureManager;
class ModelLoader;
class FilterAsset;  // Phase 6: Filter asset definition
}
namespace render {
class FBOManager;
}
}

namespace segmecam {
namespace ar_filters {

/**
 * ARRenderer - Complete 3D model rendering integration for AR filters
 * 
 * This class integrates all Phase 4 components to provide a complete
 * 3D model rendering pipeline:
 * 
 * Pipeline: ModelLoader → TextureManager → FBOManager → OpenGLRenderer
 * 
 * Features:
 * - Load 3D models (.obj files) with materials
 * - Load and cache textures (PNG/JPG/BMP/TGA)
 * - Render models to offscreen framebuffers
 * - Composite rendered models with video frames
 * - Face landmark-based positioning and scaling
 */
class ARRenderer {
public:
  /**
   * Configuration for AR rendering
   */
  struct ARConfig {
    // Rendering dimensions
    int render_width = 1920;
    int render_height = 1080;
    
    // FBO settings
    bool use_multisampling = true;
    int msaa_samples = 4;
    
    // Model scaling and positioning
    float default_scale = 1.0f;
    bool auto_scale_to_face = true;
    
    // Texture filtering
    bool use_anisotropic_filtering = true;
    int max_anisotropy = 16;
    
    // Performance settings
    bool enable_frustum_culling = true;
    bool enable_depth_testing = true;
  };

  /**
   * 3D model instance for AR rendering
   */
  struct ModelInstance {
    std::string model_name;
    std::string model_path;
    
    // Transform (legacy arrays for compatibility)
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f};
    float scale[3] = {1.0f, 1.0f, 1.0f};
    
    // Phase 5: GLM-based transforms for rendering
    glm::vec3 position_offset{0.0f, 0.0f, 0.0f};
    glm::vec3 scale_factor{1.0f, 1.0f, 1.0f};
    glm::quat rotation_quat{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion
    glm::mat4 transform{1.0f};  // Current transform matrix
    glm::mat4 last_transform{1.0f};  // Previous transform for smoothing
    
    // Face landmark attachment
    bool attach_to_landmarks = false;
    std::vector<int> landmark_indices; // Which landmarks to track
    std::string attachment_anchor = "nose_bridge";  // Phase 5: anchor point name
    
    // Visibility
    bool visible = true;
    float opacity = 1.0f;
    
    // Model-specific settings
    bool cast_shadows = false;
    bool receive_shadows = false;
  };

  /**
   * Render result containing the composited frame
   */
  struct RenderResult {
    bool success = false;
    std::string error_message;
    
    // Output texture ID (GPU)
    uint32_t output_texture_id = 0;
    
    // Performance metrics
    float render_time_ms = 0.0f;
    int models_rendered = 0;
    int triangles_rendered = 0;
  };

public:
  // Constructor with optional configuration
  explicit ARRenderer(const ARConfig& config);
  ARRenderer();
  ~ARRenderer();

  // Initialization and cleanup
  absl::Status Initialize();
  void Cleanup();
  bool IsInitialized() const { return initialized_; }

  // Model management
  absl::Status LoadModel(const std::string& name, const std::string& model_path);
  absl::Status UnloadModel(const std::string& name);
  bool HasModel(const std::string& name) const;
  
  // Phase 6: Filter asset integration
  absl::Status LoadFilter(const FilterAsset& filter);
  absl::Status UnloadFilter(const std::string& filter_id);
  bool HasFilter(const std::string& filter_id) const;
  
  // Model instance management
  absl::Status CreateModelInstance(const std::string& instance_name, 
                                   const std::string& model_name,
                                   const ModelInstance& config);
  absl::Status CreateModelInstance(const std::string& instance_name, 
                                   const std::string& model_name);
  absl::Status UpdateModelInstance(const std::string& instance_name, 
                                   const ModelInstance& config);
  void RemoveModelInstance(const std::string& instance_name);
  
  // Phase 7: Behavior system integration
  void SetModelInstanceOffset(const std::string& instance_name, const glm::vec3& offset);
  void SetModelInstanceScale(const std::string& instance_name, float scale);
  void SetModelInstanceScaleVec(const std::string& instance_name, const glm::vec3& scale);
  void SetModelInstanceVisibility(const std::string& instance_name, bool visible);
  void SetModelInstanceRotation(const std::string& instance_name, const glm::vec3& rotation);
  void SetModelInstanceColorTint(const std::string& instance_name, const glm::vec3& color);
  void ResetModelInstanceTransform(const std::string& instance_name);
  
  // Face landmark integration
  absl::Status AttachModelToLandmarks(const std::string& instance_name,
                                      const std::vector<int>& landmark_indices);
  absl::Status UpdateFaceLandmarks(const std::vector<float>& landmarks_3d);
  
  // Rendering pipeline
  absl::StatusOr<RenderResult> RenderToTexture(
      const uint8_t* input_frame, int frame_width, int frame_height);
  
  absl::StatusOr<RenderResult> RenderModelsOnly();
  
  // Configuration
  void UpdateConfig(const ARConfig& config);
  const ARConfig& GetConfig() const { return config_; }
  
  // Statistics and debugging
  void GetRenderStatistics(int* total_models, int* visible_instances, 
                          size_t* gpu_memory_used) const;
  void SetDebugMode(bool enabled) { debug_mode_ = enabled; }
  
  // Viewport management
  void SetViewport(int x, int y, int width, int height);
  void GetViewport(int* x, int* y, int* width, int* height) const;
  
  // GPU texture readback - Phase 8 Day 3
  absl::StatusOr<cv::Mat> ReadFramebufferToMat(const std::string& fbo_name,
                                                 int width, int height) const;

private:
  // Internal rendering methods
  absl::Status SetupRenderTarget();
  absl::Status RenderModelInstances();
  absl::Status CompositeWithBackground(const uint8_t* background_frame,
                                       int frame_width, int frame_height);
  
  // Face landmark processing
  void UpdateInstanceTransformsFromLandmarks();
  void CalculateModelTransform(const ModelInstance& instance,
                              float transform_matrix[16]) const;
  cv::Point3f GetAnchorPosition(const std::vector<cv::Point3f>& landmarks,
                                const std::string& anchor_name) const;
  
  // Resource management
  absl::Status EnsureResourcesLoaded(const std::string& instance_name);
  void CleanupUnusedResources();

private:
  ARConfig config_;
  bool initialized_;
  bool debug_mode_;
  
  // Component managers
  std::unique_ptr<ModelLoader> model_loader_;
  std::unique_ptr<TextureManager> texture_manager_;
  std::unique_ptr<render::FBOManager> fbo_manager_;
  
  // Model and instance management
  std::map<std::string, ModelInstance> model_instances_;
  std::vector<float> current_face_landmarks_;
  
  // Phase 6: Filter tracking
  std::map<std::string, std::vector<std::string>> loaded_filters_; // filter_id -> instance_names
  
  // Rendering state
  std::string render_fbo_name_;
  int viewport_x_, viewport_y_, viewport_width_, viewport_height_;
  
  // Performance tracking
  mutable float last_render_time_ms_;
  mutable int last_models_rendered_;
  mutable int last_triangles_rendered_;
};

} // namespace ar_filters
} // namespace segmecam