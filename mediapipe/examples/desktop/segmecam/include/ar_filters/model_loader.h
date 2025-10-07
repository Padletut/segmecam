// Copyright 2024 SegmeCam Contributors
// Licensed under the Apache License, Version 2.0

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <opencv2/core.hpp>
#include "absl/status/statusor.h"

// NOTE: Do NOT include OpenGL headers here! This file must be includable
// from both MediaPipe code (which uses GLES2) and OpenGL renderer code (which uses epoxy)
// Use uint32_t instead of GLuint to avoid header dependencies

// Forward declare Assimp types to avoid header pollution
struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;

namespace segmecam {
namespace ar_filters {

struct Material {
  cv::Vec3f ambient;        // Ka - ambient color
  cv::Vec3f diffuse;        // Kd - diffuse color
  cv::Vec3f specular;       // Ks - specular color
  float shininess;          // Ns - specular exponent
  float opacity;            // d or Tr - opacity (1.0 = opaque, 0.0 = transparent)
  std::string texture_path; // map_Kd - diffuse texture
  std::string opacity_map_path;  // map_d - opacity/alpha map
  std::string emissive_map_path; // map_Ke - emissive/glow map
  uint32_t texture_id;      // OpenGL texture ID (0 if not loaded) - uint32_t instead of GLuint
  
  Material() 
    : ambient(0.2f, 0.2f, 0.2f),
      diffuse(0.8f, 0.8f, 0.8f),
      specular(1.0f, 1.0f, 1.0f),
      shininess(32.0f),
      opacity(1.0f),
      texture_id(0) {}
};

struct Mesh {
  uint32_t vao;            // Vertex Array Object - uint32_t instead of GLuint
  uint32_t vbo;            // Vertex Buffer Object (interleaved vertices)
  uint32_t ebo;            // Element Buffer Object (indices)
  size_t index_count;      // Number of indices to draw
  std::string material_name; // Material name from model file
  
  // Bounding box for this mesh
  cv::Vec3f bounds_min;
  cv::Vec3f bounds_max;
  
  Mesh() 
    : vao(0), vbo(0), ebo(0), 
      index_count(0),
      bounds_min(FLT_MAX, FLT_MAX, FLT_MAX),
      bounds_max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
};

struct Model {
  std::vector<Mesh> meshes;
  std::map<std::string, Material> materials;
  
  // Overall bounding box
  cv::Vec3f bounds_min;
  cv::Vec3f bounds_max;
  
  // Metadata
  std::string name;
  size_t vertex_count;
  size_t triangle_count;
  
  Model()
    : bounds_min(FLT_MAX, FLT_MAX, FLT_MAX),
      bounds_max(-FLT_MAX, -FLT_MAX, -FLT_MAX),
      vertex_count(0),
      triangle_count(0) {}
};

class ModelLoader {
public:
  ModelLoader() = default;
  ~ModelLoader() = default;

  // Load 3D model (supports OBJ, GLTF, FBX, STL, and 50+ formats via Assimp)
  absl::StatusOr<Model> LoadModel(const std::string& filepath);
  
  // Unload model and free GPU resources
  void UnloadModel(Model& model);

private:
  // Process Assimp scene into our Model structure
  absl::Status ProcessNode(aiNode* node, const aiScene* scene, Model& model);
  absl::Status ProcessMesh(aiMesh* mesh, const aiScene* scene, Model& model);
  absl::Status ProcessMaterials(const aiScene* scene, Model& model);
  
  // Create OpenGL buffers from mesh data
  absl::Status CreateOpenGLBuffers(aiMesh* mesh, Mesh& out_mesh);
  
  // Vertex data for OpenGL (interleaved)
  struct Vertex {
    cv::Vec3f position;
    cv::Vec3f normal;
    cv::Vec2f uv;
    
    Vertex() 
      : position(0, 0, 0),
        normal(0, 1, 0),
        uv(0, 0) {}
  };
};

} // namespace ar_filters
} // namespace segmecam
