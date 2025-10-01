// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Filter Object Definition - Phase 2 Step 5
// Defines FilterObject structure and simple primitive geometry generators

#ifndef SEGMECAM_AR_FILTERS_FILTER_OBJECT_H_
#define SEGMECAM_AR_FILTERS_FILTER_OBJECT_H_

#include <string>
#include <vector>

#include "mediapipe/examples/desktop/segmecam/third_party/glm/glm.hpp"
#include "mediapipe/examples/desktop/segmecam/third_party/glm/gtc/quaternion.hpp"

namespace segmecam {

// Simple filter object for AR attachment testing
// This is a lightweight structure for Phase 2 testing with geometric primitives
// Phase 3+ will extend this with full 3D model loading capabilities
struct FilterObject {
  // Identity
  std::string id;                       // Unique identifier (auto-generated)
  std::string name;                     // Human-readable name (e.g., "Test Cube")
  std::string anchor_name;              // Anchor point to attach to
  
  // Geometry (simple primitives for Phase 2 testing)
  std::vector<glm::vec3> vertices;      // Vertex positions (local space)
  std::vector<unsigned int> indices;    // Triangle indices
  std::vector<glm::vec3> normals;       // Vertex normals (for lighting)
  
  // Transform (world space)
  glm::vec3 position;                   // World position (updated each frame)
  glm::quat rotation;                   // Orientation quaternion (updated each frame)
  glm::vec3 scale;                      // Scale factors (x, y, z)
  
  // Attachment configuration (local space)
  glm::vec3 offset;                     // Offset from anchor point (in anchor local space)
  glm::vec3 local_rotation_euler;       // Local rotation in degrees (pitch, yaw, roll)
  float local_scale;                    // Uniform scale multiplier
  
  // Material properties
  glm::vec4 color;                      // Base color (RGBA)
  float alpha;                          // Transparency (0.0 = transparent, 1.0 = opaque)
  
  // State
  bool visible;                         // Should this filter be rendered?
  bool enabled;                         // Is this filter active?
  
  // Metadata
  float creation_time;                  // Timestamp when filter was created
  int frame_count;                      // Number of frames this filter has been active
  
  // Default constructor
  FilterObject()
      : id(""),
        name("Unnamed"),
        anchor_name(""),
        position(0.0f, 0.0f, 0.0f),
        rotation(1.0f, 0.0f, 0.0f, 0.0f),  // Identity quaternion
        scale(1.0f, 1.0f, 1.0f),
        offset(0.0f, 0.0f, 0.0f),
        local_rotation_euler(0.0f, 0.0f, 0.0f),
        local_scale(1.0f),
        color(1.0f, 1.0f, 1.0f, 1.0f),    // White
        alpha(1.0f),
        visible(true),
        enabled(true),
        creation_time(0.0f),
        frame_count(0) {}
};

// Primitive geometry generators for testing
// These create simple geometric shapes that can be attached to anchor points
// for visual validation of the attachment system

// Create a cube centered at origin
// size: Edge length of the cube
// Returns: FilterObject with cube geometry
FilterObject CreateCube(const std::string& anchor_name, 
                       const std::string& name = "Test Cube",
                       float size = 0.05f);

// Create a sphere centered at origin
// segments: Number of subdivisions (higher = smoother, but more vertices)
// radius: Sphere radius
// Returns: FilterObject with sphere geometry
FilterObject CreateSphere(const std::string& anchor_name,
                         const std::string& name = "Test Sphere", 
                         int segments = 16,
                         float radius = 0.025f);

// Create a cylinder oriented along Y axis
// radius: Cylinder radius
// height: Cylinder height
// segments: Number of subdivisions around circumference
// Returns: FilterObject with cylinder geometry
FilterObject CreateCylinder(const std::string& anchor_name,
                           const std::string& name = "Test Cylinder",
                           float radius = 0.02f, 
                           float height = 0.1f,
                           int segments = 16);

// Create a cone oriented along Y axis (point at top)
// base_radius: Radius of cone base
// height: Cone height
// segments: Number of subdivisions around circumference
// Returns: FilterObject with cone geometry
FilterObject CreateCone(const std::string& anchor_name,
                       const std::string& name = "Test Cone",
                       float base_radius = 0.03f,
                       float height = 0.06f,
                       int segments = 16);

// Helper function to generate unique filter ID
std::string GenerateFilterId();

}  // namespace segmecam

#endif  // SEGMECAM_AR_FILTERS_FILTER_OBJECT_H_
