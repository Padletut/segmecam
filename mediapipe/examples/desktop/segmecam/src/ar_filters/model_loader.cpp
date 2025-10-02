// Copyright 2024 SegmeCam Contributors
// Licensed under the Apache License, Version 2.0

#include "mediapipe/examples/desktop/segmecam/include/ar_filters/model_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// OpenGL headers - use glcorearb.h for modern OpenGL 3.0+ functions
#define GL_GLEXT_PROTOTYPES  // Enable function prototypes
#include <GL/gl.h>
#include <GL/glext.h>  // For VAO/VBO functions (OpenGL 3.0+)

#include "absl/strings/str_cat.h"
#include "absl/log/log.h"

namespace segmecam {
namespace ar_filters {

absl::StatusOr<Model> ModelLoader::LoadModel(const std::string& filepath) {
  // Create Assimp importer
  Assimp::Importer importer;
  
  // Load model with common post-processing flags
  const aiScene* scene = importer.ReadFile(filepath,
      aiProcess_Triangulate |           // Convert polygons to triangles
      aiProcess_FlipUVs |               // Flip UVs vertically (OpenGL convention)
      aiProcess_GenNormals |            // Generate normals if missing
      aiProcess_CalcTangentSpace |      // Calculate tangents for normal mapping
      aiProcess_JoinIdenticalVertices | // Optimize by joining identical vertices
      aiProcess_SortByPType |           // Separate meshes by primitive type
      aiProcess_OptimizeMeshes |        // Reduce number of meshes
      aiProcess_ValidateDataStructure   // Validate loaded data
  );
  
  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to load model: ", filepath, " - ", 
                     importer.GetErrorString()));
  }
  
  // Create model structure
  Model model;
  model.name = filepath.substr(filepath.find_last_of("/\\") + 1);
  
  // Process materials first
  auto material_status = ProcessMaterials(scene, model);
  if (!material_status.ok()) {
    return material_status;
  }
  
  // Process scene graph recursively
  auto node_status = ProcessNode(scene->mRootNode, scene, model);
  if (!node_status.ok()) {
    return node_status;
  }
  
  // Calculate overall bounding box
  if (!model.meshes.empty()) {
    model.bounds_min = model.meshes[0].bounds_min;
    model.bounds_max = model.meshes[0].bounds_max;
    
    for (size_t i = 1; i < model.meshes.size(); ++i) {
      model.bounds_min[0] = std::min(model.bounds_min[0], model.meshes[i].bounds_min[0]);
      model.bounds_min[1] = std::min(model.bounds_min[1], model.meshes[i].bounds_min[1]);
      model.bounds_min[2] = std::min(model.bounds_min[2], model.meshes[i].bounds_min[2]);
      model.bounds_max[0] = std::max(model.bounds_max[0], model.meshes[i].bounds_max[0]);
      model.bounds_max[1] = std::max(model.bounds_max[1], model.meshes[i].bounds_max[1]);
      model.bounds_max[2] = std::max(model.bounds_max[2], model.meshes[i].bounds_max[2]);
    }
  }
  
  LOG(INFO) << "Loaded model: " << model.name 
            << " (" << model.meshes.size() << " meshes, "
            << model.vertex_count << " vertices, "
            << model.triangle_count << " triangles)";
  
  return model;
}

absl::Status ModelLoader::ProcessNode(aiNode* node, const aiScene* scene, Model& model) {
  // Process all meshes in this node
  for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
    aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
    auto status = ProcessMesh(mesh, scene, model);
    if (!status.ok()) {
      return status;
    }
  }
  
  // Process all child nodes recursively
  for (unsigned int i = 0; i < node->mNumChildren; ++i) {
    auto status = ProcessNode(node->mChildren[i], scene, model);
    if (!status.ok()) {
      return status;
    }
  }
  
  return absl::OkStatus();
}

absl::Status ModelLoader::ProcessMesh(aiMesh* ai_mesh, const aiScene* scene, Model& model) {
  Mesh mesh;
  
  // Create OpenGL buffers from Assimp mesh
  auto status = CreateOpenGLBuffers(ai_mesh, mesh);
  if (!status.ok()) {
    return status;
  }
  
  // Get material name
  if (ai_mesh->mMaterialIndex >= 0 && ai_mesh->mMaterialIndex < scene->mNumMaterials) {
    aiMaterial* material = scene->mMaterials[ai_mesh->mMaterialIndex];
    aiString name;
    if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
      mesh.material_name = name.C_Str();
    }
  }
  
  // Update model statistics
  model.vertex_count += ai_mesh->mNumVertices;
  model.triangle_count += ai_mesh->mNumFaces;
  
  model.meshes.push_back(mesh);
  return absl::OkStatus();
}

absl::Status ModelLoader::ProcessMaterials(const aiScene* scene, Model& model) {
  for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
    aiMaterial* ai_mat = scene->mMaterials[i];
    
    aiString name;
    ai_mat->Get(AI_MATKEY_NAME, name);
    std::string mat_name = name.C_Str();
    
    Material material;
    
    // Ambient color
    aiColor3D ambient;
    if (ai_mat->Get(AI_MATKEY_COLOR_AMBIENT, ambient) == AI_SUCCESS) {
      material.ambient = cv::Vec3f(ambient.r, ambient.g, ambient.b);
    }
    
    // Diffuse color
    aiColor3D diffuse;
    if (ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
      material.diffuse = cv::Vec3f(diffuse.r, diffuse.g, diffuse.b);
    }
    
    // Specular color
    aiColor3D specular;
    if (ai_mat->Get(AI_MATKEY_COLOR_SPECULAR, specular) == AI_SUCCESS) {
      material.specular = cv::Vec3f(specular.r, specular.g, specular.b);
    }
    
    // Shininess
    float shininess;
    if (ai_mat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
      material.shininess = shininess;
    }
    
    // Transparency/Opacity
    float opacity;
    if (ai_mat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
      material.transparency = opacity;
    }
    
    // Diffuse texture
    if (ai_mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
      aiString texture_path;
      if (ai_mat->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == AI_SUCCESS) {
        material.texture_path = texture_path.C_Str();
        // Texture loading will be handled separately (Phase 4+)
      }
    }
    
    model.materials[mat_name] = material;
  }
  
  return absl::OkStatus();
}

absl::Status ModelLoader::CreateOpenGLBuffers(aiMesh* ai_mesh, Mesh& mesh) {
  if (!ai_mesh->HasPositions()) {
    return absl::InvalidArgumentError("Mesh has no positions");
  }
  
  // Build interleaved vertex buffer
  std::vector<Vertex> vertices;
  vertices.reserve(ai_mesh->mNumVertices);
  
  mesh.bounds_min = cv::Vec3f(FLT_MAX, FLT_MAX, FLT_MAX);
  mesh.bounds_max = cv::Vec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  
  for (unsigned int i = 0; i < ai_mesh->mNumVertices; ++i) {
    Vertex vertex;
    
    // Position
    vertex.position[0] = ai_mesh->mVertices[i].x;
    vertex.position[1] = ai_mesh->mVertices[i].y;
    vertex.position[2] = ai_mesh->mVertices[i].z;
    
    // Update bounding box
    mesh.bounds_min[0] = std::min(mesh.bounds_min[0], vertex.position[0]);
    mesh.bounds_min[1] = std::min(mesh.bounds_min[1], vertex.position[1]);
    mesh.bounds_min[2] = std::min(mesh.bounds_min[2], vertex.position[2]);
    mesh.bounds_max[0] = std::max(mesh.bounds_max[0], vertex.position[0]);
    mesh.bounds_max[1] = std::max(mesh.bounds_max[1], vertex.position[1]);
    mesh.bounds_max[2] = std::max(mesh.bounds_max[2], vertex.position[2]);
    
    // Normal
    if (ai_mesh->HasNormals()) {
      vertex.normal[0] = ai_mesh->mNormals[i].x;
      vertex.normal[1] = ai_mesh->mNormals[i].y;
      vertex.normal[2] = ai_mesh->mNormals[i].z;
    }
    
    // Texture coordinates (use first UV channel)
    if (ai_mesh->HasTextureCoords(0)) {
      vertex.uv[0] = ai_mesh->mTextureCoords[0][i].x;
      vertex.uv[1] = ai_mesh->mTextureCoords[0][i].y;
    }
    
    vertices.push_back(vertex);
  }
  
  // Build index buffer
  std::vector<GLuint> indices;
  indices.reserve(ai_mesh->mNumFaces * 3);
  
  for (unsigned int i = 0; i < ai_mesh->mNumFaces; ++i) {
    const aiFace& face = ai_mesh->mFaces[i];
    
    // Assimp triangulated faces for us (aiProcess_Triangulate)
    if (face.mNumIndices == 3) {
      indices.push_back(face.mIndices[0]);
      indices.push_back(face.mIndices[1]);
      indices.push_back(face.mIndices[2]);
    }
  }
  
  if (indices.empty()) {
    return absl::InvalidArgumentError("Mesh has no triangles");
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
  
  return absl::OkStatus();
}

void ModelLoader::UnloadModel(Model& model) {
  for (auto& mesh : model.meshes) {
    if (mesh.vao != 0) glDeleteVertexArrays(1, &mesh.vao);
    if (mesh.vbo != 0) glDeleteBuffers(1, &mesh.vbo);
    if (mesh.ebo != 0) glDeleteBuffers(1, &mesh.ebo);
  }
  
  // Delete textures
  for (auto& [name, material] : model.materials) {
    if (material.texture_id != 0) {
      glDeleteTextures(1, &material.texture_id);
    }
  }
  
  model.meshes.clear();
  model.materials.clear();
  model.vertex_count = 0;
  model.triangle_count = 0;
}

} // namespace ar_filters
} // namespace segmecam
