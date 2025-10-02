# Phase 3: 3D Model Loading System - Implementation Plan

## Overview

**Goal**: Add 3D model loading support (OBJ/GLTF) **alongside existing primitive system**

**Status**: 🔜 **NOT STARTED**  
**Estimated Complexity**: ⚙️⚙️⚙️ (3/5 - Moderate)  
**Estimated Duration**: 1 week  
**Dependencies**: Phase 2 Complete (FilterObject, AttachmentController, Presets)

> ⚠️ **IMPORTANT**: This phase **extends** FilterObject with 3D model support!
>
> **What Phase 3 Adds**:
>
> - ✅ **Keep Phase 2 primitives** (CreateCube, CreateCylinder, CreateCone, CreateSphere)
> - ✅ **Add model loading** (CreateFromModel for OBJ/GLTF files)
> - ✅ **Both coexist** - Primitives for simple filters, Models for complex ones!
>
> **Why Keep Phase 2 Primitives**:
>
> - 🚀 **Performance**: 0.1μs switching, super lightweight
> - 🐛 **Debugging**: Easy visualization of anchor points and transforms
> - 🔧 **Prototyping**: Quick filter creation without 3D modeling
> - 🔙 **Fallback**: If model loading fails or assets missing
> - 📚 **Educational**: Show how the AR system works
> - 🎨 **Customization**: Users can create simple filters without Blender

> 🚨 **ARCHITECTURAL INDEPENDENCE**:
>
> - **NO dependencies** on EffectsManager or beauty effects code
> - **NO imports** from beauty/background effect modules
> - **Direct MediaPipe access** for face data (no intermediaries)
> - **Separate OpenGL context** and GPU resource management
> - Must work perfectly even if beauty effects are disabled/broken

---

## Implementation Steps

### Step 1: Choose Model Loading Approach (Day 1)

**Decision**: Choose between Assimp library or custom OBJ parser

#### Option A: Assimp Library (Recommended)

**Pros**:

- ✅ Supports 50+ formats (OBJ, GLTF, FBX, STL, etc.)
- ✅ Battle-tested, widely used
- ✅ Handles complex features (materials, textures, animations)
- ✅ Active maintenance and community
- ✅ MIT/BSD license (Apache 2.0 compatible)

**Cons**:

- ❌ Large dependency (~2MB compiled)
- ❌ Bazel integration complexity
- ❌ Potential version conflicts with system libraries

**Bazel Integration**:

```python
# Add to MODULE.bazel
bazel_dep(name = "assimp", version = "5.3.1")

# Or add to WORKSPACE (if not using bzlmod)
http_archive(
    name = "assimp",
    urls = ["https://github.com/assimp/assimp/archive/refs/tags/v5.3.1.tar.gz"],
    strip_prefix = "assimp-5.3.1",
    sha256 = "...",
    build_file = "@//third_party:assimp.BUILD",
)
```

**Usage Example**:

```cpp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

Assimp::Importer importer;
const aiScene* scene = importer.ReadFile(filepath,
    aiProcess_Triangulate |
    aiProcess_FlipUVs |
    aiProcess_GenNormals);
```

#### Option B: Custom OBJ Parser (Lightweight)

**Pros**:

- ✅ No external dependencies
- ✅ Lightweight (~300-400 lines)
- ✅ Full control over implementation
- ✅ Easy to debug and optimize
- ✅ No Bazel complexity

**Cons**:

- ❌ Only supports OBJ format
- ❌ Need to implement MTL material parsing
- ❌ No GLTF/FBX support (would need separate parsers)
- ❌ Less robust error handling

**Implementation Complexity**:

- Parse vertices: `v x y z` (~30 lines)
- Parse normals: `vn x y z` (~20 lines)
- Parse UVs: `vt u v` (~20 lines)
- Parse faces: `f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3` (~80 lines)
- Parse MTL materials: `newmtl`, `Ka`, `Kd`, `Ks`, `map_Kd` (~100 lines)
- Error handling and validation (~100 lines)

**Recommended Decision**: 🎯 **Start with custom OBJ parser**

- Phase 3 goal is "OBJ format initially"
- Can add Assimp later if needed (Phase 4 or beyond)
- Keeps dependencies minimal
- Faster to implement and test

---

### Step 2: Create ModelLoader Class (Day 1-2)

**Files to Create**:

- `include/ar_filters/model_loader.h`
- `src/ar_filters/model_loader.cpp`
- `tests/ar_filters/model_loader_test.cpp`

**Class Structure**:

```cpp
// include/ar_filters/model_loader.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <GL/gl.h>
#include <opencv2/core.hpp>
#include "absl/status/statusor.h"

namespace segmecam {
namespace ar_filters {

struct Material {
  cv::Vec3f ambient;        // Ka - ambient color
  cv::Vec3f diffuse;        // Kd - diffuse color
  cv::Vec3f specular;       // Ks - specular color
  float shininess;          // Ns - specular exponent
  float transparency;       // d or Tr - transparency
  std::string texture_path; // map_Kd - diffuse texture
  GLuint texture_id;        // OpenGL texture ID (0 if not loaded)
};

struct Mesh {
  GLuint vao;              // Vertex Array Object
  GLuint vbo;              // Vertex Buffer Object (interleaved vertices)
  GLuint ebo;              // Element Buffer Object (indices)
  size_t index_count;      // Number of indices to draw
  std::string material_name; // Material name from OBJ file
  
  // Bounding box for this mesh
  cv::Vec3f bounds_min;
  cv::Vec3f bounds_max;
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
};

class ModelLoader {
public:
  ModelLoader() = default;
  ~ModelLoader() = default;

  // Load OBJ file and create OpenGL buffers
  absl::StatusOr<Model> LoadOBJ(const std::string& filepath);
  
  // Unload model and free GPU resources
  void UnloadModel(Model& model);
  
  // Load MTL material file
  absl::StatusOr<std::map<std::string, Material>> 
    LoadMTL(const std::string& filepath);

private:
  // OBJ parsing helpers
  struct OBJData {
    std::vector<cv::Vec3f> positions;
    std::vector<cv::Vec3f> normals;
    std::vector<cv::Vec2f> uvs;
    
    struct Face {
      std::vector<int> position_indices;
      std::vector<int> normal_indices;
      std::vector<int> uv_indices;
      std::string material_name;
    };
    std::vector<Face> faces;
  };
  
  absl::StatusOr<OBJData> ParseOBJ(const std::string& filepath);
  absl::Status CreateOpenGLBuffers(const OBJData& obj_data, 
                                    Model& model);
  
  // Vertex data for OpenGL (interleaved)
  struct Vertex {
    cv::Vec3f position;
    cv::Vec3f normal;
    cv::Vec2f uv;
  };
};

} // namespace ar_filters
} // namespace segmecam
```

**Implementation Details**:

```cpp
// src/ar_filters/model_loader.cpp
#include "ar_filters/model_loader.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "absl/strings/str_split.h"
#include "absl/strings/numbers.h"

namespace segmecam {
namespace ar_filters {

absl::StatusOr<ModelLoader::OBJData> 
ModelLoader::ParseOBJ(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return absl::NotFoundError(
      absl::StrCat("Failed to open OBJ file: ", filepath));
  }
  
  OBJData data;
  std::string current_material;
  std::string line;
  int line_number = 0;
  
  while (std::getline(file, line)) {
    line_number++;
    
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') continue;
    
    std::istringstream iss(line);
    std::string prefix;
    iss >> prefix;
    
    if (prefix == "v") {
      // Vertex position: v x y z [w]
      cv::Vec3f pos;
      iss >> pos[0] >> pos[1] >> pos[2];
      data.positions.push_back(pos);
      
    } else if (prefix == "vn") {
      // Vertex normal: vn x y z
      cv::Vec3f normal;
      iss >> normal[0] >> normal[1] >> normal[2];
      data.normals.push_back(normal);
      
    } else if (prefix == "vt") {
      // Texture coordinate: vt u v [w]
      cv::Vec2f uv;
      iss >> uv[0] >> uv[1];
      data.uvs.push_back(uv);
      
    } else if (prefix == "f") {
      // Face: f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3 [v4/vt4/vn4]
      OBJData::Face face;
      face.material_name = current_material;
      
      std::string vertex_str;
      while (iss >> vertex_str) {
        // Parse v/vt/vn or v//vn or v/vt or v
        std::vector<std::string> indices = 
          absl::StrSplit(vertex_str, '/');
        
        if (indices.size() >= 1) {
          int v_idx;
          if (absl::SimpleAtoi(indices[0], &v_idx)) {
            // OBJ indices are 1-based, convert to 0-based
            face.position_indices.push_back(v_idx - 1);
          }
        }
        
        if (indices.size() >= 2 && !indices[1].empty()) {
          int vt_idx;
          if (absl::SimpleAtoi(indices[1], &vt_idx)) {
            face.uv_indices.push_back(vt_idx - 1);
          }
        }
        
        if (indices.size() >= 3 && !indices[2].empty()) {
          int vn_idx;
          if (absl::SimpleAtoi(indices[2], &vn_idx)) {
            face.normal_indices.push_back(vn_idx - 1);
          }
        }
      }
      
      // Triangulate if face has more than 3 vertices (quad -> 2 triangles)
      if (face.position_indices.size() >= 3) {
        data.faces.push_back(face);
      }
      
    } else if (prefix == "usemtl") {
      // Material usage: usemtl material_name
      iss >> current_material;
      
    } else if (prefix == "mtllib") {
      // Material library reference (handled separately)
      // Just note it for now, we'll load MTL in LoadOBJ
    }
  }
  
  return data;
}

absl::Status ModelLoader::CreateOpenGLBuffers(
    const OBJData& obj_data, Model& model) {
  
  // Group faces by material to create separate meshes
  std::map<std::string, std::vector<OBJData::Face>> faces_by_material;
  for (const auto& face : obj_data.faces) {
    faces_by_material[face.material_name].push_back(face);
  }
  
  // Create a mesh for each material
  for (const auto& [material_name, faces] : faces_by_material) {
    Mesh mesh;
    mesh.material_name = material_name;
    
    // Convert faces to indexed vertex buffer
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::unordered_map<std::string, GLuint> vertex_cache;
    
    for (const auto& face : faces) {
      // Triangulate: support both triangles and quads
      size_t vertex_count = face.position_indices.size();
      
      // Create triangles (fan triangulation for quads+)
      for (size_t i = 1; i < vertex_count - 1; ++i) {
        for (size_t j : {0, i, i + 1}) {
          Vertex vertex;
          
          // Position
          int pos_idx = face.position_indices[j];
          if (pos_idx >= 0 && pos_idx < obj_data.positions.size()) {
            vertex.position = obj_data.positions[pos_idx];
          }
          
          // Normal
          if (j < face.normal_indices.size()) {
            int norm_idx = face.normal_indices[j];
            if (norm_idx >= 0 && norm_idx < obj_data.normals.size()) {
              vertex.normal = obj_data.normals[norm_idx];
            }
          }
          
          // UV
          if (j < face.uv_indices.size()) {
            int uv_idx = face.uv_indices[j];
            if (uv_idx >= 0 && uv_idx < obj_data.uvs.size()) {
              vertex.uv = obj_data.uvs[uv_idx];
            }
          }
          
          // Check if vertex already exists (deduplication)
          std::string vertex_key = absl::StrCat(
            vertex.position[0], ",", vertex.position[1], ",", vertex.position[2], ",",
            vertex.normal[0], ",", vertex.normal[1], ",", vertex.normal[2], ",",
            vertex.uv[0], ",", vertex.uv[1]
          );
          
          auto it = vertex_cache.find(vertex_key);
          if (it != vertex_cache.end()) {
            indices.push_back(it->second);
          } else {
            GLuint new_index = vertices.size();
            vertices.push_back(vertex);
            indices.push_back(new_index);
            vertex_cache[vertex_key] = new_index;
          }
        }
      }
    }
    
    // Calculate bounding box for this mesh
    mesh.bounds_min = cv::Vec3f(FLT_MAX, FLT_MAX, FLT_MAX);
    mesh.bounds_max = cv::Vec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& vertex : vertices) {
      mesh.bounds_min[0] = std::min(mesh.bounds_min[0], vertex.position[0]);
      mesh.bounds_min[1] = std::min(mesh.bounds_min[1], vertex.position[1]);
      mesh.bounds_min[2] = std::min(mesh.bounds_min[2], vertex.position[2]);
      mesh.bounds_max[0] = std::max(mesh.bounds_max[0], vertex.position[0]);
      mesh.bounds_max[1] = std::max(mesh.bounds_max[1], vertex.position[1]);
      mesh.bounds_max[2] = std::max(mesh.bounds_max[2], vertex.position[2]);
    }
    
    // Create OpenGL buffers
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    
    glBindVertexArray(mesh.vao);
    
    // Upload vertex data (interleaved)
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, 
                 vertices.size() * sizeof(Vertex),
                 vertices.data(), 
                 GL_STATIC_DRAW);
    
    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(GLuint),
                 indices.data(),
                 GL_STATIC_DRAW);
    
    // Set vertex attribute pointers
    // Position (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, position));
    
    // Normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    
    // UV (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));
    
    glBindVertexArray(0);
    
    mesh.index_count = indices.size();
    model.meshes.push_back(mesh);
    
    // Update overall model statistics
    model.vertex_count += vertices.size();
    model.triangle_count += indices.size() / 3;
  }
  
  // Calculate overall bounding box
  if (!model.meshes.empty()) {
    model.bounds_min = model.meshes[0].bounds_min;
    model.bounds_max = model.meshes[0].bounds_max;
    
    for (size_t i = 1; i < model.meshes.size(); ++i) {
      model.bounds_min[0] = std::min(model.bounds_min[0], 
                                      model.meshes[i].bounds_min[0]);
      model.bounds_min[1] = std::min(model.bounds_min[1], 
                                      model.meshes[i].bounds_min[1]);
      model.bounds_min[2] = std::min(model.bounds_min[2], 
                                      model.meshes[i].bounds_min[2]);
      model.bounds_max[0] = std::max(model.bounds_max[0], 
                                      model.meshes[i].bounds_max[0]);
      model.bounds_max[1] = std::max(model.bounds_max[1], 
                                      model.meshes[i].bounds_max[1]);
      model.bounds_max[2] = std::max(model.bounds_max[2], 
                                      model.meshes[i].bounds_max[2]);
    }
  }
  
  return absl::OkStatus();
}

absl::StatusOr<Model> ModelLoader::LoadOBJ(const std::string& filepath) {
  // Parse OBJ file
  auto obj_data_or = ParseOBJ(filepath);
  if (!obj_data_or.ok()) {
    return obj_data_or.status();
  }
  OBJData obj_data = *obj_data_or;
  
  // Create model
  Model model;
  model.name = filepath.substr(filepath.find_last_of("/\\") + 1);
  
  // Create OpenGL buffers
  auto status = CreateOpenGLBuffers(obj_data, model);
  if (!status.ok()) {
    return status;
  }
  
  // Load MTL file if referenced
  // (Look for .mtl file with same name as .obj)
  std::string mtl_path = filepath.substr(0, filepath.find_last_of('.')) + ".mtl";
  auto materials_or = LoadMTL(mtl_path);
  if (materials_or.ok()) {
    model.materials = *materials_or;
  }
  // If MTL fails, continue without materials (not critical)
  
  return model;
}

void ModelLoader::UnloadModel(Model& model) {
  for (auto& mesh : model.meshes) {
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(1, &mesh.vbo);
    glDeleteBuffers(1, &mesh.ebo);
  }
  
  // Delete textures
  for (auto& [name, material] : model.materials) {
    if (material.texture_id != 0) {
      glDeleteTextures(1, &material.texture_id);
    }
  }
  
  model.meshes.clear();
  model.materials.clear();
}

absl::StatusOr<std::map<std::string, Material>> 
ModelLoader::LoadMTL(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return absl::NotFoundError(
      absl::StrCat("Failed to open MTL file: ", filepath));
  }
  
  std::map<std::string, Material> materials;
  std::string current_material;
  std::string line;
  
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    
    std::istringstream iss(line);
    std::string prefix;
    iss >> prefix;
    
    if (prefix == "newmtl") {
      // New material definition
      iss >> current_material;
      materials[current_material] = Material();
      
    } else if (prefix == "Ka" && !current_material.empty()) {
      // Ambient color
      auto& mat = materials[current_material];
      iss >> mat.ambient[0] >> mat.ambient[1] >> mat.ambient[2];
      
    } else if (prefix == "Kd" && !current_material.empty()) {
      // Diffuse color
      auto& mat = materials[current_material];
      iss >> mat.diffuse[0] >> mat.diffuse[1] >> mat.diffuse[2];
      
    } else if (prefix == "Ks" && !current_material.empty()) {
      // Specular color
      auto& mat = materials[current_material];
      iss >> mat.specular[0] >> mat.specular[1] >> mat.specular[2];
      
    } else if (prefix == "Ns" && !current_material.empty()) {
      // Shininess
      auto& mat = materials[current_material];
      iss >> mat.shininess;
      
    } else if ((prefix == "d" || prefix == "Tr") && !current_material.empty()) {
      // Transparency
      auto& mat = materials[current_material];
      iss >> mat.transparency;
      
    } else if (prefix == "map_Kd" && !current_material.empty()) {
      // Diffuse texture
      auto& mat = materials[current_material];
      iss >> mat.texture_path;
      // Texture loading will be handled separately
    }
  }
  
  return materials;
}

} // namespace ar_filters
} // namespace segmecam
```

---

### Step 3: Extend FilterObject for 3D Models (Day 2-3)

**Update FilterObject Class**:

```cpp
// include/ar_filters/filter_object.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/core.hpp>
#include <GL/gl.h>
#include "ar_filters/model_loader.h"

namespace segmecam {
namespace ar_filters {

class FilterObject {
public:
  enum class Type {
    PRIMITIVE,  // Phase 2: Cube, Cylinder, Cone, Sphere
    MODEL_3D    // Phase 3: Loaded OBJ/GLTF models
  };
  
  // Phase 2 methods (KEEP - still work perfectly!)
  static FilterObject CreateCube(const std::string& anchor_name, 
                                  const std::string& name,
                                  const cv::Vec3f& size,
                                  const cv::Vec4f& color = cv::Vec4f(1,1,1,1));
  
  static FilterObject CreateCylinder(const std::string& anchor_name,
                                      const std::string& name,
                                      float radius, float height,
                                      int segments = 32,
                                      const cv::Vec4f& color = cv::Vec4f(1,1,1,1));
  
  static FilterObject CreateCone(const std::string& anchor_name,
                                  const std::string& name,
                                  float radius, float height,
                                  int segments = 32,
                                  const cv::Vec4f& color = cv::Vec4f(1,1,1,1));
  
  static FilterObject CreateSphere(const std::string& anchor_name,
                                    const std::string& name,
                                    float radius,
                                    int stacks = 16, int slices = 32,
                                    const cv::Vec4f& color = cv::Vec4f(1,1,1,1));
  
  // Phase 3 methods (NEW!)
  static FilterObject CreateFromModel(const std::string& anchor_name,
                                       const std::string& name,
                                       const std::string& model_path);
  
  // Rendering
  void Render() const;
  
  // Getters
  Type GetType() const { return type_; }
  const std::string& GetName() const { return name_; }
  const std::string& GetAnchorName() const { return anchor_name_; }
  bool IsVisible() const { return visible_; }
  
  // Setters
  void SetVisible(bool visible) { visible_ = visible; }
  void SetTransform(const cv::Matx44f& transform) { transform_ = transform; }
  void SetColor(const cv::Vec4f& color) { color_ = color; }
  void SetScale(const cv::Vec3f& scale) { scale_ = scale; }
  
private:
  FilterObject() = default;
  
  Type type_;
  std::string name_;
  std::string anchor_name_;
  bool visible_ = true;
  
  // Transform
  cv::Matx44f transform_ = cv::Matx44f::eye();
  cv::Vec3f scale_ = cv::Vec3f(1.0f, 1.0f, 1.0f);
  cv::Vec4f color_ = cv::Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
  
  // Primitive data (Phase 2)
  std::vector<cv::Vec3f> vertices_;
  std::vector<cv::Vec3f> normals_;
  std::vector<unsigned int> indices_;
  GLuint vao_ = 0;
  GLuint vbo_ = 0;
  GLuint ebo_ = 0;
  
  // Model data (Phase 3)
  std::shared_ptr<ModelLoader::Model> model_;
  
  // Rendering helpers
  void RenderPrimitive() const;
  void RenderModel() const;
  void SetupPrimitiveBuffers();
};

} // namespace ar_filters
} // namespace segmecam
```

**Implementation**:

```cpp
// src/ar_filters/filter_object.cpp

FilterObject FilterObject::CreateFromModel(
    const std::string& anchor_name,
    const std::string& name,
    const std::string& model_path) {
  
  FilterObject obj;
  obj.type_ = Type::MODEL_3D;
  obj.name_ = name;
  obj.anchor_name_ = anchor_name;
  
  // Load model
  ModelLoader loader;
  auto model_or = loader.LoadOBJ(model_path);
  if (!model_or.ok()) {
    LOG(ERROR) << "Failed to load model: " << model_or.status();
    // Return empty object (will not render)
    return obj;
  }
  
  obj.model_ = std::make_shared<ModelLoader::Model>(*model_or);
  return obj;
}

void FilterObject::Render() const {
  if (!visible_) return;
  
  // Apply transform
  glPushMatrix();
  glMultMatrixf(transform_.val);
  
  // Apply scale
  glScalef(scale_[0], scale_[1], scale_[2]);
  
  // Render based on type
  if (type_ == Type::PRIMITIVE) {
    RenderPrimitive();
  } else if (type_ == Type::MODEL_3D) {
    RenderModel();
  }
  
  glPopMatrix();
}

void FilterObject::RenderModel() const {
  if (!model_) return;
  
  // Render each mesh in the model
  for (const auto& mesh : model_->meshes) {
    // Apply material if available
    auto it = model_->materials.find(mesh.material_name);
    if (it != model_->materials.end()) {
      const Material& mat = it->second;
      
      // Set material properties
      glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat.ambient.val);
      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat.diffuse.val);
      glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat.specular.val);
      glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat.shininess);
      
      // Bind texture if available
      if (mat.texture_id != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, mat.texture_id);
      }
    } else {
      // Use FilterObject's color as fallback
      glColor4fv(color_.val);
    }
    
    // Render mesh
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Disable texture
    glDisable(GL_TEXTURE_2D);
  }
}
```

---

### Step 4: Create Test Assets (Day 3)

**Asset Structure**:

```
mediapipe/examples/desktop/segmecam/assets/
└── ar_filters/
    ├── models/
    │   ├── simple_glasses.obj     # Simple test model
    │   ├── simple_glasses.mtl
    │   ├── top_hat.obj            # Another test model
    │   └── top_hat.mtl
    └── README.md                   # Asset documentation
```

**Simple Test Model (Glasses)**:

```obj
# simple_glasses.obj - Two cylinder lenses connected by a bridge
mtllib simple_glasses.mtl

# Left lens vertices (cylinder)
v -0.05 0 -0.03
v -0.05 0 0.03
# ... (20 vertices for cylinder)

# Right lens vertices
v 0.05 0 -0.03
v 0.05 0 0.03
# ... (20 vertices for cylinder)

# Bridge vertices
v -0.01 0 -0.005
v -0.01 0 0.005
v 0.01 0 0.005
v 0.01 0 -0.005

# Normals
vn 0 1 0
vn 0 -1 0
# ...

# UVs
vt 0.0 0.0
vt 1.0 0.0
# ...

# Faces (triangles)
usemtl glass_material
f 1/1/1 2/2/1 3/3/1
f 3/3/1 2/2/1 4/4/1
# ...
```

**Material File**:

```mtl
# simple_glasses.mtl
newmtl glass_material
Ka 0.1 0.1 0.1    # Ambient
Kd 0.2 0.2 0.2    # Diffuse (dark glass)
Ks 0.9 0.9 0.9    # Specular (shiny)
Ns 100.0          # Shininess
d 0.5             # Transparency (50%)
```

**Alternative**: Export from Blender

1. Create simple glasses model in Blender
2. Export as OBJ (File → Export → Wavefront (.obj))
3. Enable: Write Materials, Triangulate Faces, Write Normals
4. Copy to `assets/ar_filters/models/`

---

### Step 5: Update AttachmentController (Day 4)

**No Changes Needed!** AttachmentController already handles any FilterObject type through the `Render()` interface.

**Verify Integration**:

```cpp
// AttachmentController automatically works with models!
auto realistic_glasses = FilterObject::CreateFromModel(
    "nose_bridge", 
    "realistic_sunglasses",
    "assets/ar_filters/models/simple_glasses.obj"
);

attachment_controller.AttachFilter(realistic_glasses);
// Model renders at nose_bridge anchor point automatically!
```

---

### Step 6: Create Unit Tests (Day 4-5)

**Test File**: `tests/ar_filters/model_loader_test.cpp`

```cpp
#include "ar_filters/model_loader.h"
#include <gtest/gtest.h>

namespace segmecam {
namespace ar_filters {
namespace test {

TEST(ModelLoaderTest, LoadSimpleOBJ) {
  ModelLoader loader;
  auto model_or = loader.LoadOBJ("assets/ar_filters/models/simple_glasses.obj");
  
  ASSERT_TRUE(model_or.ok()) << model_or.status();
  
  const Model& model = *model_or;
  EXPECT_GT(model.meshes.size(), 0);
  EXPECT_GT(model.vertex_count, 0);
  EXPECT_GT(model.triangle_count, 0);
  
  // Check bounding box
  EXPECT_LT(model.bounds_min[0], model.bounds_max[0]);
  EXPECT_LT(model.bounds_min[1], model.bounds_max[1]);
  EXPECT_LT(model.bounds_min[2], model.bounds_max[2]);
}

TEST(ModelLoaderTest, LoadNonexistentFile) {
  ModelLoader loader;
  auto model_or = loader.LoadOBJ("nonexistent.obj");
  
  EXPECT_FALSE(model_or.ok());
  EXPECT_EQ(model_or.status().code(), absl::StatusCode::kNotFound);
}

TEST(ModelLoaderTest, UnloadModel) {
  ModelLoader loader;
  auto model_or = loader.LoadOBJ("assets/ar_filters/models/simple_glasses.obj");
  ASSERT_TRUE(model_or.ok());
  
  Model model = *model_or;
  EXPECT_GT(model.meshes.size(), 0);
  
  // Unload should not crash
  loader.UnloadModel(model);
  EXPECT_EQ(model.meshes.size(), 0);
}

TEST(ModelLoaderTest, LoadMTL) {
  ModelLoader loader;
  auto materials_or = loader.LoadMTL("assets/ar_filters/models/simple_glasses.mtl");
  
  ASSERT_TRUE(materials_or.ok());
  
  const auto& materials = *materials_or;
  EXPECT_GT(materials.size(), 0);
  
  // Check glass_material exists
  auto it = materials.find("glass_material");
  ASSERT_NE(it, materials.end());
  
  const Material& mat = it->second;
  EXPECT_GT(mat.shininess, 0.0f);
}

TEST(FilterObjectTest, CreateFromModel) {
  auto glasses = FilterObject::CreateFromModel(
      "nose_bridge",
      "test_glasses",
      "assets/ar_filters/models/simple_glasses.obj"
  );
  
  EXPECT_EQ(glasses.GetType(), FilterObject::Type::MODEL_3D);
  EXPECT_EQ(glasses.GetName(), "test_glasses");
  EXPECT_EQ(glasses.GetAnchorName(), "nose_bridge");
  EXPECT_TRUE(glasses.IsVisible());
}

TEST(FilterObjectTest, PrimitivesStillWork) {
  // Ensure Phase 2 primitives still work!
  auto cube = FilterObject::CreateCube("nose_bridge", "test_cube",
                                        cv::Vec3f(0.1, 0.1, 0.1));
  
  EXPECT_EQ(cube.GetType(), FilterObject::Type::PRIMITIVE);
  EXPECT_EQ(cube.GetName(), "test_cube");
}

} // namespace test
} // namespace ar_filters
} // namespace segmecam
```

**Run Tests**:

```bash
bazel test -c opt //mediapipe/examples/desktop/segmecam:model_loader_test
```

---

### Step 7: Integration Testing (Day 5)

**Test Application**: Create `test-3d-models.cpp`

```cpp
// Test 3D model loading alongside primitives
#include "ar_filters/filter_object.h"
#include "ar_filters/attachment_controller.h"

int main() {
  // Test 1: Load 3D model
  auto model_glasses = FilterObject::CreateFromModel(
      "nose_bridge",
      "realistic_glasses",
      "assets/ar_filters/models/simple_glasses.obj"
  );
  
  // Test 2: Phase 2 primitives still work
  auto primitive_glasses = FilterObject::CreateCylinder(
      "nose_bridge", "simple_lens", 0.03f, 0.005f, 32
  );
  
  // Test 3: Both can be attached
  AttachmentController controller;
  controller.AttachFilter(model_glasses);
  controller.AttachFilter(primitive_glasses);
  
  // Test 4: Switching between them
  model_glasses.SetVisible(true);
  primitive_glasses.SetVisible(false);
  
  // Render loop
  for (int i = 0; i < 100; ++i) {
    controller.Update(face_mesh);
    controller.Render();
  }
  
  return 0;
}
```

---

### Step 8: Update BUILD File (Day 5)

```python
# mediapipe/examples/desktop/segmecam/BUILD

cc_library(
    name = "model_loader",
    srcs = ["src/ar_filters/model_loader.cpp"],
    hdrs = ["include/ar_filters/model_loader.h"],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "//third_party:opencv",
    ],
    copts = ["-std=c++17"],
)

cc_library(
    name = "filter_object",
    srcs = ["src/ar_filters/filter_object.cpp"],
    hdrs = ["include/ar_filters/filter_object.h"],
    deps = [
        ":model_loader",  # NEW dependency
        "@com_google_absl//absl/status:statusor",
        "//third_party:opencv",
    ],
    copts = ["-std=c++17"],
)

cc_test(
    name = "model_loader_test",
    srcs = ["tests/ar_filters/model_loader_test.cpp"],
    deps = [
        ":model_loader",
        ":filter_object",
        "@com_google_googletest//:gtest_main",
    ],
    data = [
        "//assets/ar_filters/models:simple_glasses.obj",
        "//assets/ar_filters/models:simple_glasses.mtl",
    ],
)
```

---

## Testing Criteria

### Functional Tests

- [ ] **OBJ Parsing**
  - [ ] Loads vertices, normals, UVs correctly
  - [ ] Handles faces (triangles and quads)
  - [ ] Supports multiple objects in one file
  - [ ] Parses MTL material files
  - [ ] Handles missing data gracefully (no normals/UVs)

- [ ] **OpenGL Buffer Creation**
  - [ ] Creates VAO, VBO, EBO correctly
  - [ ] Interleaved vertex data (position, normal, UV)
  - [ ] Indexed rendering works
  - [ ] Bounding box calculated correctly
  - [ ] Multiple meshes per model supported

- [ ] **FilterObject Integration**
  - [ ] CreateFromModel() works
  - [ ] Renders models correctly
  - [ ] Primitives still work (backward compatibility)
  - [ ] Both types coexist without conflicts
  - [ ] Transform/scale/color applied correctly

- [ ] **AttachmentController**
  - [ ] Attaches model-based filters
  - [ ] Positions models at anchor points
  - [ ] Head tracking works with models
  - [ ] Switching between primitive/model filters works

### Performance Tests

- [ ] **Loading Performance**
  - [ ] OBJ file parsing < 50ms for simple models
  - [ ] OpenGL buffer creation < 10ms
  - [ ] Total load time < 100ms for typical model

- [ ] **Rendering Performance**
  - [ ] Model rendering maintains 30 FPS
  - [ ] No frame drops with 1-2 models active
  - [ ] Memory usage reasonable (< 50MB per model)

### Visual Tests

- [ ] **Model Rendering**
  - [ ] Models render at correct position
  - [ ] Lighting/shading looks correct
  - [ ] Materials applied properly
  - [ ] No visual glitches or z-fighting

- [ ] **Primitive Compatibility**
  - [ ] Primitives render identically to Phase 2
  - [ ] No visual regression in primitive filters
  - [ ] Classic Glasses preset still works
  - [ ] Party Hat preset still works

---

## Success Criteria

### Phase 3 Complete When

1. ✅ **ModelLoader Implemented**
   - OBJ file parsing works
   - MTL material loading works
   - OpenGL buffer creation works
   - Unit tests pass (90%+ coverage)

2. ✅ **FilterObject Extended**
   - CreateFromModel() method added
   - MODEL_3D type supported
   - Rendering works for both primitive and model types
   - All Phase 2 primitives still work identically

3. ✅ **Test Assets Created**
   - At least 2 test OBJ models (glasses, hat)
   - MTL files with materials
   - Models render correctly in app

4. ✅ **Integration Verified**
   - AttachmentController works with models (no code changes needed)
   - Head tracking positions models correctly
   - Can switch between primitive and model filters
   - FilterPresets work with models

5. ✅ **Performance Validated**
   - Model loading < 100ms
   - Rendering maintains 30 FPS
   - Memory usage acceptable

6. ✅ **Documentation Updated**
   - ModelLoader API documented
   - Asset format documented
   - Examples added to README

---

## Implementation Checklist

### Day 1: Planning & Setup

- [ ] Decide on Assimp vs custom OBJ parser (recommend Assimp)
- [ ] Create ModelLoader header file
- [ ] Define Model, Mesh, Material structures
- [ ] Set up test assets directory structure

### Day 2: ModelLoader Implementation

- [ ] Implement ParseOBJ() method
- [ ] Implement LoadMTL() method
- [ ] Implement CreateOpenGLBuffers() method
- [ ] Implement LoadOBJ() main method
- [ ] Implement UnloadModel() method

### Day 3: FilterObject Extension

- [ ] Add MODEL_3D type to FilterObject enum
- [ ] Add model_ member variable
- [ ] Implement CreateFromModel() static method
- [ ] Implement RenderModel() method
- [ ] Update Render() to handle both types
- [ ] Create simple test OBJ files (glasses, hat)

### Day 4: Testing

- [ ] Write unit tests for ModelLoader
- [ ] Write unit tests for FilterObject model support
- [ ] Test primitive backward compatibility
- [ ] Test AttachmentController integration
- [ ] Create test application

### Day 5: Integration & Documentation

- [ ] Run full integration tests
- [ ] Verify performance (loading, rendering)
- [ ] Update BUILD file
- [ ] Update documentation
- [ ] Create asset README
- [ ] Commit Phase 3 code

---

## Future Enhancements (Post-Phase 3)

### Phase 4 Could Add

- **GLTF Support**: More modern format with animations
- **Assimp Integration**: Support 50+ formats
- **Texture Loading**: From PNG/JPEG files
- **Normal Mapping**: Better lighting detail
- **PBR Materials**: Physically-based rendering
- **Model Caching**: Avoid reloading same models
- **Asset Bundles**: Package multiple models together

### Phase 3 Scope (Keep Minimal)

- ✅ OBJ format only
- ✅ Basic materials (Kd, Ks, Ka)
- ✅ Simple textures (optional)
- ✅ Static models (no animation)
- ✅ Single-file loading
- ✅ CPU-side parsing (GPU upload)

---

## Risk Mitigation

### Risk 1: OBJ Parser Bugs

**Mitigation**:

- Start with simple test models
- Add comprehensive unit tests
- Validate against known-good parsers

### Risk 2: OpenGL Buffer Management

**Mitigation**:

- Use RAII wrappers for OpenGL resources
- Implement proper UnloadModel()
- Test memory leaks with valgrind

### Risk 3: Performance Degradation

**Mitigation**:

- Profile model loading time
- Benchmark rendering with 1-5 models
- Optimize if >10ms per model

### Risk 4: Breaking Phase 2 Primitives

**Mitigation**:

- Run all Phase 2 tests before/after
- Test Classic Glasses preset explicitly
- Verify 0.1μs switching performance maintained

---

## Notes

- **Independence Reminder**: No EffectsManager dependencies! AR filters are completely separate from beauty effects.
- **Backward Compatibility**: All Phase 2 primitives must work identically after Phase 3
- **Minimal Scope**: Focus on OBJ only, no animations, simple materials
- **Testing First**: Write tests before/during implementation, not after
- **Asset Quality**: Start with simple models, add complexity later

---

## Estimated Effort

| Task | Estimated Time |
|------|----------------|
| Planning & design decisions | 4 hours |
| ModelLoader implementation | 16 hours |
| FilterObject extension | 8 hours |
| Test asset creation | 4 hours |
| Unit tests | 8 hours |
| Integration testing | 8 hours |
| Documentation | 4 hours |
| **Total** | **52 hours (~1 week)** |

---

## Completion Definition

Phase 3 is **COMPLETE** when:

1. ✅ All tests pass (unit + integration)
2. ✅ At least 2 test OBJ models load and render
3. ✅ Phase 2 primitives still work identically
4. ✅ Performance targets met (< 100ms load, 30 FPS render)
5. ✅ Documentation updated
6. ✅ Code committed and tagged as `phase-3-complete`

**Ready for Phase 4**: Texture Management & Material System
