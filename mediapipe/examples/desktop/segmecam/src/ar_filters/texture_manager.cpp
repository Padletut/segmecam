// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// TextureManager Implementation - Phase 4 Step 1
//
// CRITICAL: This file uses epoxy/gl.h and must NEVER include MediaPipe headers
// Keep this compilation unit completely separate from MediaPipe's GL setup

#include "include/ar_filters/texture_manager.h"

// OpenGL (isolated from MediaPipe)
#include <epoxy/gl.h>

// Standard libraries
#include <filesystem>
#include <algorithm>
#include <iostream>

// Logging
#include "absl/log/log.h"
#include "absl/log/absl_log.h"

namespace segmecam {
namespace ar_filters {

TextureManager::TextureManager() 
  : use_linear_filtering_(true),
    generate_mipmaps_(true) {
  ABSL_LOG(INFO) << "TextureManager initialized";
}

TextureManager::~TextureManager() {
  UnloadAll();
  ABSL_LOG(INFO) << "TextureManager destroyed";
}

absl::StatusOr<TextureManager::Texture> TextureManager::LoadTexture(const std::string& filepath) {
  if (filepath.empty()) {
    return absl::InvalidArgumentError("Texture filepath cannot be empty");
  }
  
  // Normalize path for consistent caching
  std::string normalized_path = NormalizePath(filepath);
  
  // Check if already cached
  auto cache_it = texture_cache_.find(normalized_path);
  if (cache_it != texture_cache_.end()) {
    ABSL_LOG(INFO) << "Using cached texture: " << normalized_path;
    return cache_it->second;
  }
  
  ABSL_LOG(INFO) << "Loading new texture: " << normalized_path;
  
  // Load image from file
  auto image_result = LoadImageFromFile(normalized_path);
  if (!image_result.ok()) {
    return image_result.status();
  }
  
  // Create OpenGL texture
  auto texture_result = CreateTextureFromMat(image_result.value(), normalized_path);
  if (!texture_result.ok()) {
    return texture_result.status();
  }
  
  // Cache the texture
  texture_cache_[normalized_path] = texture_result.value();
  
  ABSL_LOG(INFO) << "Texture loaded successfully: " << normalized_path 
                 << " (ID: " << texture_result.value().id 
                 << ", Size: " << texture_result.value().width << "x" << texture_result.value().height
                 << ", Channels: " << texture_result.value().channels << ")";
  
  return texture_result.value();
}

TextureManager::Texture TextureManager::GetCachedTexture(const std::string& filepath) const {
  std::string normalized_path = NormalizePath(filepath);
  auto cache_it = texture_cache_.find(normalized_path);
  
  if (cache_it != texture_cache_.end()) {
    return cache_it->second;
  }
  
  // Return invalid texture if not found
  return Texture();
}

bool TextureManager::IsTextureCached(const std::string& filepath) const {
  std::string normalized_path = NormalizePath(filepath);
  return texture_cache_.find(normalized_path) != texture_cache_.end();
}

void TextureManager::BindTexture(const Texture& texture, int unit) const {
  if (!texture.IsValid()) {
    ABSL_LOG(WARNING) << "Attempting to bind invalid texture";
    return;
  }
  
  if (unit < 0 || unit > 31) {
    ABSL_LOG(WARNING) << "Invalid texture unit: " << unit << " (valid range: 0-31)";
    return;
  }
  
  // Activate texture unit and bind texture
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, texture.id);
}

void TextureManager::UnbindTexture(int unit) const {
  if (unit < 0 || unit > 31) {
    ABSL_LOG(WARNING) << "Invalid texture unit: " << unit << " (valid range: 0-31)";
    return;
  }
  
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void TextureManager::UnloadTexture(const std::string& filepath) {
  std::string normalized_path = NormalizePath(filepath);
  auto cache_it = texture_cache_.find(normalized_path);
  
  if (cache_it != texture_cache_.end()) {
    const Texture& texture = cache_it->second;
    
    if (texture.IsValid()) {
      glDeleteTextures(1, &texture.id);
      ABSL_LOG(INFO) << "Unloaded texture: " << normalized_path << " (ID: " << texture.id << ")";
    }
    
    texture_cache_.erase(cache_it);
  }
}

void TextureManager::UnloadAll() {
  for (auto& [filepath, texture] : texture_cache_) {
    if (texture.IsValid()) {
      glDeleteTextures(1, &texture.id);
    }
  }
  
  size_t unloaded_count = texture_cache_.size();
  texture_cache_.clear();
  
  if (unloaded_count > 0) {
    ABSL_LOG(INFO) << "Unloaded all textures (" << unloaded_count << " textures)";
  }
}

size_t TextureManager::GetTotalGPUMemoryUsed() const {
  size_t total_memory = 0;
  for (const auto& [filepath, texture] : texture_cache_) {
    total_memory += CalculateTextureMemory(texture);
  }
  return total_memory;
}

void TextureManager::SetDefaultFiltering(bool use_linear_filtering, bool generate_mipmaps) {
  use_linear_filtering_ = use_linear_filtering;
  generate_mipmaps_ = generate_mipmaps;
  
  ABSL_LOG(INFO) << "Texture filtering updated: linear=" << use_linear_filtering_ 
                 << ", mipmaps=" << generate_mipmaps_;
}

absl::StatusOr<cv::Mat> TextureManager::LoadImageFromFile(const std::string& filepath) const {
  // Check if file exists
  if (!std::filesystem::exists(filepath)) {
    return absl::NotFoundError("Texture file not found: " + filepath);
  }
  
  // Load image using OpenCV (supports PNG, JPG, BMP, TGA, etc.)
  cv::Mat image = cv::imread(filepath, cv::IMREAD_UNCHANGED);
  
  if (image.empty()) {
    return absl::InvalidArgumentError("Failed to load image: " + filepath + 
                                     " (unsupported format or corrupted file)");
  }
  
  // Convert BGR to RGB for OpenGL (OpenCV loads as BGR by default)
  cv::Mat rgb_image;
  if (image.channels() == 3) {
    cv::cvtColor(image, rgb_image, cv::COLOR_BGR2RGB);
  } else if (image.channels() == 4) {
    cv::cvtColor(image, rgb_image, cv::COLOR_BGRA2RGBA);
  } else if (image.channels() == 1) {
    // Convert grayscale to RGB
    cv::cvtColor(image, rgb_image, cv::COLOR_GRAY2RGB);
  } else {
    return absl::InvalidArgumentError("Unsupported image format: " + filepath + 
                                     " (channels: " + std::to_string(image.channels()) + ")");
  }
  
  return rgb_image;
}

absl::StatusOr<TextureManager::Texture> TextureManager::CreateTextureFromMat(
    const cv::Mat& image, const std::string& filepath) const {
  
  if (image.empty()) {
    return absl::InvalidArgumentError("Cannot create texture from empty image");
  }
  
  // Get OpenGL format information
  GLFormat gl_format = GetGLFormat(image);
  
  // Generate OpenGL texture
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  if (texture_id == 0) {
    return absl::InternalError("Failed to generate OpenGL texture");
  }
  
  // Bind and configure texture
  glBindTexture(GL_TEXTURE_2D, texture_id);
  
  // Set texture parameters
  if (use_linear_filtering_) {
    if (generate_mipmaps_) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      // Bias towards higher resolution mipmaps for sharper close-up textures
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.5f);
    } else {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  
  // Set wrapping mode
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  
  // Enable anisotropic filtering for much better quality at angles
  GLfloat max_anisotropy;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
  if (max_anisotropy > 1.0f) {
    // Use maximum anisotropic filtering (typically 16x)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropy);
    std::cout << "Anisotropic filtering enabled: " << max_anisotropy << "x" << std::endl;
  }
  
  // Upload texture data with high quality settings
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, gl_format.internal_format, 
               image.cols, image.rows, 0, 
               gl_format.format, gl_format.type, image.data);
  
  // Generate mipmaps if requested
  if (generate_mipmaps_) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  
  // Check for OpenGL errors
  GLenum gl_error = glGetError();
  if (gl_error != GL_NO_ERROR) {
    glDeleteTextures(1, &texture_id);
    return absl::InternalError("OpenGL error while creating texture: " + std::to_string(gl_error));
  }
  
  // Unbind texture
  glBindTexture(GL_TEXTURE_2D, 0);
  
  // Create texture object
  Texture texture;
  texture.id = texture_id;
  texture.width = image.cols;
  texture.height = image.rows;
  texture.channels = image.channels();
  texture.has_alpha = (image.channels() == 4);
  texture.filepath = filepath;
  
  return texture;
}

TextureManager::GLFormat TextureManager::GetGLFormat(const cv::Mat& image) const {
  GLFormat format;
  
  switch (image.channels()) {
    case 1: // Grayscale
      format.internal_format = GL_RGB;
      format.format = GL_RGB;
      format.type = GL_UNSIGNED_BYTE;
      break;
      
    case 3: // RGB
      format.internal_format = GL_RGB;
      format.format = GL_RGB;
      format.type = GL_UNSIGNED_BYTE;
      break;
      
    case 4: // RGBA
      format.internal_format = GL_RGBA;
      format.format = GL_RGBA;
      format.type = GL_UNSIGNED_BYTE;
      break;
      
    default:
      // Fallback to RGB
      format.internal_format = GL_RGB;
      format.format = GL_RGB;
      format.type = GL_UNSIGNED_BYTE;
      break;
  }
  
  return format;
}

std::string TextureManager::NormalizePath(const std::string& filepath) const {
  try {
    // Convert to absolute path and normalize
    std::filesystem::path path(filepath);
    return std::filesystem::canonical(path).string();
  } catch (const std::filesystem::filesystem_error&) {
    // If canonical fails (file doesn't exist), just normalize without resolving
    std::filesystem::path path(filepath);
    return path.lexically_normal().string();
  }
}

size_t TextureManager::CalculateTextureMemory(const Texture& texture) const {
  if (!texture.IsValid()) {
    return 0;
  }
  
  // Calculate memory usage: width * height * channels * bytes_per_channel
  size_t pixel_count = texture.width * texture.height;
  size_t bytes_per_pixel = texture.channels; // Assuming 8-bit per channel
  size_t base_memory = pixel_count * bytes_per_pixel;
  
  // Add mipmap memory if mipmaps are generated (approximately 33% more)
  if (generate_mipmaps_) {
    base_memory += base_memory / 3;
  }
  
  return base_memory;
}

} // namespace ar_filters
} // namespace segmecam