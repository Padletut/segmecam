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
#include "absl/status/statusor.h"
#include "absl/status/status.h"

// Forward declarations to avoid circular dependencies
namespace segmecam {
namespace ar_filters {
class TextureManager;
class ModelLoader;
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
    
    // Transform
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f};
    float scale[3] = {1.0f, 1.0f, 1.0f};
    
    // Face landmark attachment (optional)
    bool attach_to_landmarks = false;
    std::vector<int> landmark_indices; // Which landmarks to track
    
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
  
  // Model instance management
  absl::Status CreateModelInstance(const std::string& instance_name, 
                                   const std::string& model_name,
                                   const ModelInstance& config);
  absl::Status CreateModelInstance(const std::string& instance_name, 
                                   const std::string& model_name);
  absl::Status UpdateModelInstance(const std::string& instance_name, 
                                   const ModelInstance& config);
  void RemoveModelInstance(const std::string& instance_name);
  
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