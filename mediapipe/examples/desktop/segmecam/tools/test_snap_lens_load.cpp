// Copyright 2024 SegmeCam Contributors
// Licensed under the Apache License, Version 2.0

#include <iostream>
#include <fstream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

void PrintSceneInfo(const aiScene* scene) {
  std::cout << "\n=== Scene Information ===\n";
  std::cout << "Meshes: " << scene->mNumMeshes << "\n";
  std::cout << "Materials: " << scene->mNumMaterials << "\n";
  std::cout << "Textures: " << scene->mNumTextures << "\n";
  std::cout << "Animations: " << scene->mNumAnimations << "\n";
  std::cout << "Cameras: " << scene->mNumCameras << "\n";
  std::cout << "Lights: " << scene->mNumLights << "\n";
  
  if (scene->mNumMeshes > 0) {
    std::cout << "\n=== Mesh Details ===\n";
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
      aiMesh* mesh = scene->mMeshes[i];
      std::cout << "Mesh " << i << ": " << mesh->mName.C_Str() << "\n";
      std::cout << "  Vertices: " << mesh->mNumVertices << "\n";
      std::cout << "  Faces: " << mesh->mNumFaces << "\n";
      std::cout << "  Has Normals: " << (mesh->HasNormals() ? "Yes" : "No") << "\n";
      std::cout << "  Has Texture Coords: " << (mesh->HasTextureCoords(0) ? "Yes" : "No") << "\n";
      std::cout << "  Material Index: " << mesh->mMaterialIndex << "\n";
    }
  }
  
  if (scene->mNumMaterials > 0) {
    std::cout << "\n=== Material Details ===\n";
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
      aiMaterial* mat = scene->mMaterials[i];
      aiString name;
      mat->Get(AI_MATKEY_NAME, name);
      std::cout << "Material " << i << ": " << name.C_Str() << "\n";
      
      // Check for textures
      unsigned int diffuse_count = mat->GetTextureCount(aiTextureType_DIFFUSE);
      unsigned int specular_count = mat->GetTextureCount(aiTextureType_SPECULAR);
      unsigned int normal_count = mat->GetTextureCount(aiTextureType_NORMALS);
      
      std::cout << "  Diffuse textures: " << diffuse_count << "\n";
      std::cout << "  Specular textures: " << specular_count << "\n";
      std::cout << "  Normal textures: " << normal_count << "\n";
      
      // Print texture paths
      for (unsigned int j = 0; j < diffuse_count; j++) {
        aiString path;
        mat->GetTexture(aiTextureType_DIFFUSE, j, &path);
        std::cout << "    Diffuse[" << j << "]: " << path.C_Str() << "\n";
      }
      for (unsigned int j = 0; j < specular_count; j++) {
        aiString path;
        mat->GetTexture(aiTextureType_SPECULAR, j, &path);
        std::cout << "    Specular[" << j << "]: " << path.C_Str() << "\n";
      }
    }
  }
  
  if (scene->mNumTextures > 0) {
    std::cout << "\n=== Embedded Textures ===\n";
    for (unsigned int i = 0; i < scene->mNumTextures; i++) {
      aiTexture* tex = scene->mTextures[i];
      std::cout << "Texture " << i << ": " << tex->mFilename.C_Str() << "\n";
      std::cout << "  Width: " << tex->mWidth << "\n";
      std::cout << "  Height: " << tex->mHeight << "\n";
      std::cout << "  Format: " << tex->achFormatHint << "\n";
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path-to-scene-file>\n";
    std::cerr << "Example: " << argv[0] << " assets/filters/snap-lens/scene.scn\n";
    return 1;
  }
  
  std::string file_path = argv[1];
  
  // Check if file exists
  std::ifstream file(file_path);
  if (!file.good()) {
    std::cerr << "Error: File not found: " << file_path << "\n";
    return 1;
  }
  file.close();
  
  std::cout << "Testing Assimp load of: " << file_path << "\n";
  std::cout << "File size: " << std::ifstream(file_path, std::ios::binary | std::ios::ate).tellg() << " bytes\n";
  
  // Create Assimp importer
  Assimp::Importer importer;
  
  // Try loading with various flags
  const aiScene* scene = importer.ReadFile(file_path,
    aiProcess_Triangulate |
    aiProcess_GenSmoothNormals |
    aiProcess_FlipUVs |
    aiProcess_JoinIdenticalVertices
  );
  
  if (!scene) {
    std::cerr << "\n❌ Assimp FAILED to load file!\n";
    std::cerr << "Error: " << importer.GetErrorString() << "\n";
    
    // Try without any flags
    std::cout << "\nRetrying with no processing flags...\n";
    scene = importer.ReadFile(file_path, 0);
    
    if (!scene) {
      std::cerr << "❌ Still failed: " << importer.GetErrorString() << "\n";
      
      // List supported formats
      std::cout << "\n=== Supported Assimp Formats ===\n";
      std::string formats;
      importer.GetExtensionList(formats);
      std::cout << formats << "\n";
      
      return 1;
    }
  }
  
  std::cout << "\n✅ Assimp successfully loaded the file!\n";
  PrintSceneInfo(scene);
  
  // Calculate compatibility score
  int score = 0;
  int max_score = 100;
  
  if (scene->mNumMeshes > 0) {
    score += 30;
    std::cout << "\n✅ Has 3D meshes (+30 points)\n";
  } else {
    std::cout << "\n⚠️  No 3D meshes found (0 points)\n";
  }
  
  if (scene->mNumMaterials > 0) {
    score += 20;
    std::cout << "✅ Has materials (+20 points)\n";
  } else {
    std::cout << "⚠️  No materials found (0 points)\n";
  }
  
  if (scene->mNumTextures > 0) {
    score += 20;
    std::cout << "✅ Has embedded textures (+20 points)\n";
  }
  
  // Check for texture references in materials
  bool has_texture_refs = false;
  for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
    if (scene->mMaterials[i]->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
      has_texture_refs = true;
      break;
    }
  }
  if (has_texture_refs) {
    score += 15;
    std::cout << "✅ Has texture references in materials (+15 points)\n";
  }
  
  if (scene->mNumAnimations > 0) {
    score += 15;
    std::cout << "✅ Has animations (+15 points)\n";
  } else {
    std::cout << "⚠️  No animations found (0 points)\n";
  }
  
  std::cout << "\n=== Compatibility Score ===\n";
  std::cout << "Score: " << score << "/" << max_score << " (" << (score * 100 / max_score) << "%)\n";
  
  if (score >= 70) {
    std::cout << "✅ HIGH compatibility - Can likely import directly into SegmeCam!\n";
  } else if (score >= 40) {
    std::cout << "⚠️  MEDIUM compatibility - May need some conversion/adaptation\n";
  } else {
    std::cout << "❌ LOW compatibility - Significant conversion required\n";
  }
  
  return 0;
}
