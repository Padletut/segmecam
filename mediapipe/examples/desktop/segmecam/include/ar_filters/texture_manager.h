// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// TextureManager - Phase 4 Step 1: Texture Loading and Caching
//
// Handles loading, caching, and GPU management of textures for 3D models.
// Integrates with existing OpenCV infrastructure from RenderManager.

#pragma once

#include <string>
#include <map>
#include <memory>
#include <opencv2/opencv.hpp>
#include "absl/status/statusor.h"
#include "absl/status/status.h"

// Use uint32_t instead of GLuint to avoid OpenGL header dependencies
using GLuint = uint32_t;
using GLenum = uint32_t;

namespace segmecam {
namespace ar_filters {

class TextureManager {
public:
  // Texture data structure
  struct Texture {
    GLuint id;                        // OpenGL texture ID
    int width;                        // Width in pixels
    int height;                       // Height in pixels
    int channels;                     // Number of channels (3=RGB, 4=RGBA)
    bool has_alpha;                   // True if texture has alpha channel
    std::string filepath;             // Original file path
    
    Texture() 
      : id(0), width(0), height(0), channels(0), has_alpha(false) {}
    
    // Check if texture is valid/loaded
    bool IsValid() const { return id != 0 && width > 0 && height > 0; }
  };
  
  TextureManager();
  ~TextureManager();
  
  // Delete copy/move constructors (OpenGL resources are non-copyable)
  TextureManager(const TextureManager&) = delete;
  TextureManager& operator=(const TextureManager&) = delete;
  TextureManager(TextureManager&&) = delete;
  TextureManager& operator=(TextureManager&&) = delete;
  
  // Load texture from file (supports PNG, JPG, BMP, TGA)
  // Returns cached texture if already loaded
  // filepath: Path to image file (absolute or relative)
  absl::StatusOr<Texture> LoadTexture(const std::string& filepath);
  
  // Get cached texture (returns invalid texture if not found)
  // Use this for quick lookups without loading
  Texture GetCachedTexture(const std::string& filepath) const;
  
  // Check if texture is cached
  bool IsTextureCached(const std::string& filepath) const;
  
  // Bind texture to OpenGL texture unit
  // texture: Texture to bind
  // unit: Texture unit (0-31, default 0)
  void BindTexture(const Texture& texture, int unit = 0) const;
  
  // Unbind current texture from specified unit
  void UnbindTexture(int unit = 0) const;
  
  // Unload specific texture and free GPU memory
  void UnloadTexture(const std::string& filepath);
  
  // Unload all textures and free GPU memory
  void UnloadAll();
  
  // Get statistics
  size_t GetCachedTextureCount() const { return texture_cache_.size(); }
  size_t GetTotalGPUMemoryUsed() const;
  
  // Set texture filtering parameters for newly loaded textures
  void SetDefaultFiltering(bool use_linear_filtering, bool generate_mipmaps);
  
private:
  // Texture cache: filepath -> Texture
  std::map<std::string, Texture> texture_cache_;
  
  // Default texture parameters
  bool use_linear_filtering_;   // Use GL_LINEAR vs GL_NEAREST
  bool generate_mipmaps_;       // Generate mipmaps for quality
  
  // Load image from file using OpenCV
  absl::StatusOr<cv::Mat> LoadImageFromFile(const std::string& filepath) const;
  
  // Create OpenGL texture from OpenCV Mat
  absl::StatusOr<Texture> CreateTextureFromMat(const cv::Mat& image, 
                                                const std::string& filepath) const;
  
  // Get OpenGL format from OpenCV format
  struct GLFormat {
    GLenum internal_format;  // Internal format (GL_RGB, GL_RGBA)
    GLenum format;          // Data format (GL_RGB, GL_RGBA, GL_BGR, GL_BGRA)
    GLenum type;            // Data type (GL_UNSIGNED_BYTE)
  };
  
  GLFormat GetGLFormat(const cv::Mat& image) const;
  
  // Normalize file path for consistent caching
  std::string NormalizePath(const std::string& filepath) const;
  
  // Calculate GPU memory usage for a texture
  size_t CalculateTextureMemory(const Texture& texture) const;
};

} // namespace ar_filters
} // namespace segmecam