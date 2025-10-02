// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Filter Object Definition - Phase 2 Step 5, Phase 3 Step 3
// Defines FilterObject structure with primitive geometry and 3D model support

#ifndef SEGMECAM_AR_FILTERS_FILTER_OBJECT_H_
#define SEGMECAM_AR_FILTERS_FILTER_OBJECT_H_

#include <string>
#include <vector>
#include <memory>
#include <memory>
#include <opencv2/core.hpp>
#include <GL/gl.h>
#include <GL/gl.h>

namespace segmecam {

// Forward declarations
namespace ar_filters {
  struct Model;
}

// Filter object for AR attachments
// Supports both simple primitives (Phase 2) and 3D models (Phase 3)
// Can switch between types seamlessly for testing and production use
struct FilterObject {
  // Type of filter object
  enum class Type {
    PRIMITIVE,  // Phase 2: Cube, Cylinder, Cone, Sphere
    MODEL_3D    // Phase 3: Loaded OBJ/GLTF/FBX models
  };
  
  Type type;                            // Current object type
  // Identity
  std::string id;                       // Unique identifier (auto-generated)
  std::string name;                     // Human-readable name (e.g., "Test Cube")
  std::string anchor_name;              // Anchor point to attach to
  
  // Geometry (simple primitives for Phase 2 testing)
  std::vector<cv::Vec3f> vertices;      // Vertex positions (local space) - PRIMITIVE only
  std::vector<unsigned int> indices;    // Triangle indices - PRIMITIVE only
  std::vector<cv::Vec3f> normals;       // Vertex normals (for lighting) - PRIMITIVE only
  
  // OpenGL buffers (for both primitives and models)
  GLuint vao;                           // Vertex Array Object (PRIMITIVE only)
  GLuint vbo;                           // Vertex Buffer Object (PRIMITIVE only)
  GLuint ebo;                           // Element Buffer Object (PRIMITIVE only)
  
  // 3D Model data (Phase 3 - MODEL_3D type only)
  std::shared_ptr<ar_filters::Model> model;  // Loaded 3D model (nullptr for primitives)
  
  // Transform (world space)
  cv::Vec3f position;                   // World position (updated each frame)
  cv::Vec4f rotation;                   // Orientation quaternion (w,x,y,z) (updated each frame)
  cv::Vec3f scale;                      // Scale factors (x, y, z)
  
  // Attachment configuration (local space)
  cv::Vec3f offset;                     // Offset from anchor point (in anchor local space)
  cv::Vec3f local_rotation_euler;       // Local rotation in degrees (pitch, yaw, roll)
  float local_scale;                    // Uniform scale multiplier
  
  // Material properties
  cv::Vec4f color;                      // Base color (RGBA)
  float alpha;                          // Transparency (0.0 = transparent, 1.0 = opaque)
  
  // State
  bool visible;                         // Should this filter be rendered?
  bool enabled;                         // Is this filter active?
  
  // Metadata
  float creation_time;                  // Timestamp when filter was created
  int frame_count;                      // Number of frames this filter has been active
  
  // Default constructor
  FilterObject()
      : type(Type::PRIMITIVE),
        id(""),
        name("Unnamed"),
        anchor_name(""),
        vao(0),
        vbo(0),
        ebo(0),
        model(nullptr),
        position(0.0f, 0.0f, 0.0f),
        rotation(1.0f, 0.0f, 0.0f, 0.0f),  // Identity quaternion (w,x,y,z)
        scale(1.0f, 1.0f, 1.0f),
        offset(0.0f, 0.0f, 0.0f),
        local_rotation_euler(0.0f, 0.0f, 0.0f),
        local_scale(1.0f),
        color(1.0f, 1.0f, 1.0f, 1.0f),    // White (R,G,B,A)
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

// Phase 3: Create from 3D model file
// Supports 50+ formats via Assimp: OBJ, GLTF, FBX, STL, Collada, 3DS, Blender, etc.
// model_path: Path to 3D model file (absolute or relative)
// anchor_name: Anchor point to attach to (e.g., "nose_bridge", "forehead")
// name: Human-readable name for the filter
// Returns: FilterObject with loaded 3D model (type = MODEL_3D)
FilterObject CreateFromModel(const std::string& anchor_name,
                            const std::string& name,
                            const std::string& model_path);

// Helper function to generate unique filter ID
std::string GenerateFilterId();

}  // namespace segmecam

#endif  // SEGMECAM_AR_FILTERS_FILTER_OBJECT_H_
