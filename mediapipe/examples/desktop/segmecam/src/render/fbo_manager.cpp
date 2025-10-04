// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// FBOManager Implementation - Phase 4 Step 2
//
// CRITICAL: This file uses epoxy/gl.h and must NEVER include MediaPipe headers
// Keep this compilation unit completely separate from MediaPipe's GL setup

#include "include/render/fbo_manager.h"

// OpenGL (isolated from MediaPipe)
#include <epoxy/gl.h>

// Standard libraries
#include <algorithm>
#include <iostream>
#include <cstring>

// Logging
#include "absl/log/log.h"
#include "absl/log/absl_log.h"

namespace segmecam {
namespace render {

FBOManager::FBOManager() {
  // Initialize default clear color to transparent black
  default_clear_color_[0] = 0.0f; // R
  default_clear_color_[1] = 0.0f; // G
  default_clear_color_[2] = 0.0f; // B
  default_clear_color_[3] = 0.0f; // A (transparent)
  
  ABSL_LOG(INFO) << "FBOManager initialized";
}

FBOManager::~FBOManager() {
  DestroyAll();
  ABSL_LOG(INFO) << "FBOManager destroyed";
}

absl::StatusOr<FBOManager::FBO> FBOManager::CreateFBO(const std::string& name, 
                                                     const FBOConfig& config) {
  if (name.empty()) {
    return absl::InvalidArgumentError("FBO name cannot be empty");
  }
  
  if (config.width <= 0 || config.height <= 0) {
    return absl::InvalidArgumentError("FBO dimensions must be positive");
  }
  
  // Check if FBO already exists
  if (HasFBO(name)) {
    ABSL_LOG(WARNING) << "FBO already exists: " << name << ", destroying old one";
    DestroyFBO(name);
  }
  
  ABSL_LOG(INFO) << "Creating FBO: " << name 
                 << " (" << config.width << "x" << config.height << ")";
  
  // Create the framebuffer object
  auto fbo_result = CreateFramebufferObject(config);
  if (!fbo_result.ok()) {
    return fbo_result.status();
  }
  
  FBO fbo = fbo_result.value();
  
  // Cache the FBO
  fbo_cache_[name] = fbo;
  
  ABSL_LOG(INFO) << "FBO created successfully: " << name 
                 << " (ID: " << fbo.framebuffer_id 
                 << ", Color: " << fbo.color_texture_id << ")";
  
  return fbo;
}

FBOManager::FBO FBOManager::GetFBO(const std::string& name) const {
  auto cache_it = fbo_cache_.find(name);
  if (cache_it != fbo_cache_.end()) {
    return cache_it->second;
  }
  
  // Return invalid FBO if not found
  return FBO();
}

bool FBOManager::HasFBO(const std::string& name) const {
  return fbo_cache_.find(name) != fbo_cache_.end();
}

void FBOManager::BindFramebuffer(const FBO& fbo) const {
  if (!fbo.IsValid()) {
    // Bind default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return;
  }
  
  glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer_id);
  
  // Set viewport to match FBO size
  glViewport(0, 0, fbo.width, fbo.height);
}

bool FBOManager::BindFramebuffer(const std::string& name) const {
  FBO fbo = GetFBO(name);
  if (!fbo.IsValid()) {
    ABSL_LOG(WARNING) << "Attempting to bind non-existent FBO: " << name;
    return false;
  }
  
  BindFramebuffer(fbo);
  return true;
}

void FBOManager::UnbindFramebuffer() const {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBOManager::ClearFramebuffer(float color_r, float color_g, float color_b, float color_a,
                                 bool clear_depth, bool clear_stencil) const {
  // Set clear color
  glClearColor(color_r, color_g, color_b, color_a);
  
  // Determine what to clear
  GLbitfield clear_mask = GL_COLOR_BUFFER_BIT;
  
  if (clear_depth) {
    clear_mask |= GL_DEPTH_BUFFER_BIT;
    glClearDepth(1.0f);
  }
  
  if (clear_stencil) {
    clear_mask |= GL_STENCIL_BUFFER_BIT;
    glClearStencil(0);
  }
  
  glClear(clear_mask);
}

absl::Status FBOManager::ResizeFBO(const std::string& name, int new_width, int new_height) {
  if (new_width <= 0 || new_height <= 0) {
    return absl::InvalidArgumentError("FBO dimensions must be positive");
  }
  
  auto cache_it = fbo_cache_.find(name);
  if (cache_it == fbo_cache_.end()) {
    return absl::NotFoundError("FBO not found: " + name);
  }
  
  FBO& fbo = cache_it->second;
  
  if (fbo.width == new_width && fbo.height == new_height) {
    // No resize needed
    return absl::OkStatus();
  }
  
  ABSL_LOG(INFO) << "Resizing FBO: " << name 
                 << " from " << fbo.width << "x" << fbo.height
                 << " to " << new_width << "x" << new_height;
  
  // Store old configuration
  FBOConfig config;
  config.width = new_width;
  config.height = new_height;
  config.use_depth_buffer = (fbo.depth_buffer_id != 0);
  config.use_stencil_buffer = (fbo.stencil_buffer_id != 0);
  config.use_multisampling = fbo.is_multisampled;
  config.sample_count = 4; // Default sample count
  
  // Destroy old FBO
  DestroyFBOResources(fbo);
  
  // Create new FBO with new size
  auto new_fbo_result = CreateFramebufferObject(config);
  if (!new_fbo_result.ok()) {
    // Remove from cache if creation failed
    fbo_cache_.erase(cache_it);
    return new_fbo_result.status();
  }
  
  // Update cached FBO
  fbo = new_fbo_result.value();
  
  ABSL_LOG(INFO) << "FBO resized successfully: " << name;
  return absl::OkStatus();
}

GLuint FBOManager::GetColorTexture(const std::string& name) const {
  FBO fbo = GetFBO(name);
  return fbo.IsValid() ? fbo.color_texture_id : 0;
}

absl::Status FBOManager::BlitFramebuffer(const std::string& source_name,
                                        const std::string& dest_name,
                                        bool copy_color, bool copy_depth) const {
  // Get source FBO
  FBO source_fbo = GetFBO(source_name);
  if (!source_fbo.IsValid()) {
    return absl::NotFoundError("Source FBO not found: " + source_name);
  }
  
  // Get destination FBO (empty name means default framebuffer)
  GLuint dest_fbo_id = 0;
  int dest_width = 0, dest_height = 0;
  bool dest_is_multisampled = false;
  
  if (!dest_name.empty()) {
    FBO dest_fbo = GetFBO(dest_name);
    if (!dest_fbo.IsValid()) {
      return absl::NotFoundError("Destination FBO not found: " + dest_name);
    }
    dest_fbo_id = dest_fbo.framebuffer_id;
    dest_width = dest_fbo.width;
    dest_height = dest_fbo.height;
    dest_is_multisampled = dest_fbo.is_multisampled;
  } else {
    // Default framebuffer - get current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    dest_width = viewport[2];
    dest_height = viewport[3];
  }
  
  // Validate MSAA resolve constraints
  if (source_fbo.is_multisampled && !dest_is_multisampled) {
    // When resolving MSAA, dimensions must match exactly
    if (source_fbo.width != dest_width || source_fbo.height != dest_height) {
      return absl::InvalidArgumentError(
          "MSAA resolve requires matching dimensions: source=" + 
          std::to_string(source_fbo.width) + "x" + std::to_string(source_fbo.height) +
          " dest=" + std::to_string(dest_width) + "x" + std::to_string(dest_height));
    }
    // Cannot copy depth buffer when resolving MSAA
    if (copy_depth) {
      return absl::InvalidArgumentError(
          "Cannot copy depth buffer when resolving MSAA FBO");
    }
  }
  
  // Clear any previous OpenGL errors (with safety limit to prevent infinite loops)
  int error_clear_count = 0;
  const int MAX_ERROR_CLEAR = 100;
  while (glGetError() != GL_NO_ERROR && error_clear_count++ < MAX_ERROR_CLEAR) {}
  
  if (error_clear_count >= MAX_ERROR_CLEAR) {
    ABSL_LOG(WARNING) << "Too many pending GL errors, may indicate GL state corruption";
  }
  
  ABSL_LOG(INFO) << "BlitFramebuffer: " << source_name << " (" << source_fbo.width << "x" << source_fbo.height 
                 << ", MSAA=" << source_fbo.is_multisampled << ") -> " << dest_name 
                 << " (" << dest_width << "x" << dest_height << ")";
  
  // Unbind any active shader program (can interfere with blits)
  glUseProgram(0);
  ABSL_LOG(INFO) << "  Unbound shader programs";
  
  // Bind source and destination framebuffers
  glBindFramebuffer(GL_READ_FRAMEBUFFER, source_fbo.framebuffer_id);
  GLenum error1 = glGetError();
  if (error1 != GL_NO_ERROR) {
    return absl::InternalError("Error binding read framebuffer: " + std::to_string(error1));
  }
  ABSL_LOG(INFO) << "  Bound read framebuffer: " << source_fbo.framebuffer_id;
  
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest_fbo_id);
  GLenum error2 = glGetError();
  if (error2 != GL_NO_ERROR) {
    return absl::InternalError("Error binding draw framebuffer: " + std::to_string(error2));
  }
  ABSL_LOG(INFO) << "  Bound draw framebuffer: " << dest_fbo_id;
  
  // Verify framebuffer completeness
  GLenum read_status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
  if (read_status != GL_FRAMEBUFFER_COMPLETE) {
    return absl::InternalError("Read framebuffer incomplete: " + std::to_string(read_status));
  }
  ABSL_LOG(INFO) << "  Read framebuffer complete";
  
  GLenum draw_status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
  if (draw_status != GL_FRAMEBUFFER_COMPLETE) {
    return absl::InternalError("Draw framebuffer incomplete: " + std::to_string(draw_status));
  }
  ABSL_LOG(INFO) << "  Draw framebuffer complete";
  
  // Determine what to blit
  GLbitfield blit_mask = 0;
  if (copy_color) blit_mask |= GL_COLOR_BUFFER_BIT;
  if (copy_depth) blit_mask |= GL_DEPTH_BUFFER_BIT;
  
  // Determine filter mode
  // When resolving multisampled FBOs, GL_NEAREST must be used (GL spec requirement)
  // GL_LINEAR is only valid when both FBOs have the same sample count
  GLenum filter = (source_fbo.is_multisampled && !dest_is_multisampled) ? GL_NEAREST : GL_LINEAR;
  
  ABSL_LOG(INFO) << "  Calling glBlitFramebuffer with filter=" << (filter == GL_NEAREST ? "GL_NEAREST" : "GL_LINEAR");
  
  // Perform the blit
  glBlitFramebuffer(0, 0, source_fbo.width, source_fbo.height,
                   0, 0, dest_width, dest_height,
                   blit_mask, filter);
  
  ABSL_LOG(INFO) << "  glBlitFramebuffer completed, checking errors...";
  
  // Check for OpenGL errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    return absl::InternalError("OpenGL error during framebuffer blit: " + std::to_string(error));
  }
  
  ABSL_LOG(INFO) << "  Blit successful!";
  
  return absl::OkStatus();
}

absl::Status FBOManager::ReadPixels(const std::string& name, 
                                   int x, int y, int width, int height,
                                   void* pixel_buffer, size_t buffer_size) const {
  ABSL_LOG(INFO) << "ReadPixels: " << name << " region=(" << x << "," << y << "," << width << "x" << height << ") buffer_size=" << buffer_size;
  
  FBO fbo = GetFBO(name);
  if (!fbo.IsValid()) {
    return absl::NotFoundError("FBO not found: " + name);
  }
  
  // Validate read region
  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > fbo.width || y + height > fbo.height) {
    return absl::InvalidArgumentError("Invalid read region");
  }
  
  // Calculate required buffer size (assuming RGBA, 8-bit per channel)
  size_t required_size = width * height * 4;
  if (buffer_size < required_size) {
    return absl::InvalidArgumentError("Buffer too small for pixel data");
  }
  
  ABSL_LOG(INFO) << "  Binding FBO " << fbo.framebuffer_id << " for reading...";
  // Bind FBO for reading
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.framebuffer_id);
  
  ABSL_LOG(INFO) << "  Calling glReadPixels (" << (required_size / 1024 / 1024) << " MB)...";
  // Read pixels
  glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixel_buffer);
  ABSL_LOG(INFO) << "  glReadPixels completed";
  
  // Check for OpenGL errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    return absl::InternalError("OpenGL error during pixel read: " + std::to_string(error));
  }
  
  return absl::OkStatus();
}

void FBOManager::DestroyFBO(const std::string& name) {
  auto cache_it = fbo_cache_.find(name);
  if (cache_it != fbo_cache_.end()) {
    const FBO& fbo = cache_it->second;
    
    if (fbo.IsValid()) {
      DestroyFBOResources(fbo);
      ABSL_LOG(INFO) << "Destroyed FBO: " << name << " (ID: " << fbo.framebuffer_id << ")";
    }
    
    fbo_cache_.erase(cache_it);
  }
}

void FBOManager::DestroyAll() {
  for (auto& [name, fbo] : fbo_cache_) {
    if (fbo.IsValid()) {
      DestroyFBOResources(fbo);
    }
  }
  
  size_t destroyed_count = fbo_cache_.size();
  fbo_cache_.clear();
  
  if (destroyed_count > 0) {
    ABSL_LOG(INFO) << "Destroyed all FBOs (" << destroyed_count << " FBOs)";
  }
}

size_t FBOManager::GetTotalGPUMemoryUsed() const {
  size_t total_memory = 0;
  for (const auto& [name, fbo] : fbo_cache_) {
    total_memory += CalculateFBOMemory(fbo);
  }
  return total_memory;
}

void FBOManager::SetDefaultClearColor(float r, float g, float b, float a) {
  default_clear_color_[0] = r;
  default_clear_color_[1] = g;
  default_clear_color_[2] = b;
  default_clear_color_[3] = a;
  
  ABSL_LOG(INFO) << "Default clear color updated: (" << r << ", " << g << ", " << b << ", " << a << ")";
}

bool FBOManager::CheckFramebufferComplete() {
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  return status == GL_FRAMEBUFFER_COMPLETE;
}

void FBOManager::GetCurrentViewport(int* x, int* y, int* width, int* height) const {
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  
  if (x) *x = viewport[0];
  if (y) *y = viewport[1];
  if (width) *width = viewport[2];
  if (height) *height = viewport[3];
}

void FBOManager::SetViewport(int x, int y, int width, int height) const {
  glViewport(x, y, width, height);
}

absl::StatusOr<FBOManager::FBO> FBOManager::CreateFramebufferObject(const FBOConfig& config) const {
  FBO fbo;
  fbo.width = config.width;
  fbo.height = config.height;
  fbo.is_multisampled = config.use_multisampling;
  
  // Generate framebuffer
  glGenFramebuffers(1, &fbo.framebuffer_id);
  if (fbo.framebuffer_id == 0) {
    return absl::InternalError("Failed to generate OpenGL framebuffer");
  }
  
  // Bind framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer_id);
  
  // Create color texture
  auto color_result = CreateColorTexture(config.width, config.height, 
                                        config.use_multisampling, config.sample_count);
  if (!color_result.ok()) {
    glDeleteFramebuffers(1, &fbo.framebuffer_id);
    return color_result.status();
  }
  
  fbo.color_texture_id = color_result.value();
  
  // Attach color texture
  if (config.use_multisampling) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                          GL_TEXTURE_2D_MULTISAMPLE, fbo.color_texture_id, 0);
  } else {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                          GL_TEXTURE_2D, fbo.color_texture_id, 0);
  }
  
  // Create depth buffer if requested
  if (config.use_depth_buffer) {
    auto depth_result = CreateDepthBuffer(config.width, config.height,
                                         config.use_multisampling, config.sample_count);
    if (!depth_result.ok()) {
      DestroyFBOResources(fbo);
      return depth_result.status();
    }
    
    fbo.depth_buffer_id = depth_result.value();
    
    // Attach depth buffer
    if (config.use_multisampling) {
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
                               GL_RENDERBUFFER, fbo.depth_buffer_id);
    } else {
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_RENDERBUFFER, fbo.depth_buffer_id);
    }
  }
  
  // Create stencil buffer if requested
  if (config.use_stencil_buffer) {
    auto stencil_result = CreateStencilBuffer(config.width, config.height,
                                             config.use_multisampling, config.sample_count);
    if (!stencil_result.ok()) {
      DestroyFBOResources(fbo);
      return stencil_result.status();
    }
    
    fbo.stencil_buffer_id = stencil_result.value();
    
    // Attach stencil buffer
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                             GL_RENDERBUFFER, fbo.stencil_buffer_id);
  }
  
  // Check framebuffer completeness
  if (!CheckFramebufferComplete()) {
    DestroyFBOResources(fbo);
    return absl::InternalError("Framebuffer is not complete");
  }
  
  // Unbind framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  return fbo;
}

absl::StatusOr<GLuint> FBOManager::CreateColorTexture(int width, int height, 
                                                     bool multisampled, int samples) const {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  if (texture_id == 0) {
    return absl::InternalError("Failed to generate color texture");
  }
  
  GLAttachmentFormat format = GetColorFormat();
  
  if (multisampled) {
    // Create multisampled texture
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture_id);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format.internal_format,
                           width, height, GL_TRUE);
  } else {
    // Create regular texture
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, format.internal_format, width, height, 0,
                format.format, format.type, nullptr);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  
  // Check for OpenGL errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    glDeleteTextures(1, &texture_id);
    return absl::InternalError("OpenGL error creating color texture: " + std::to_string(error));
  }
  
  return texture_id;
}

absl::StatusOr<GLuint> FBOManager::CreateDepthBuffer(int width, int height,
                                                    bool multisampled, int samples) const {
  GLuint buffer_id;
  glGenRenderbuffers(1, &buffer_id);
  if (buffer_id == 0) {
    return absl::InternalError("Failed to generate depth buffer");
  }
  
  glBindRenderbuffer(GL_RENDERBUFFER, buffer_id);
  
  GLAttachmentFormat format = GetDepthFormat();
  
  if (multisampled) {
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format.internal_format,
                                    width, height);
  } else {
    glRenderbufferStorage(GL_RENDERBUFFER, format.internal_format, width, height);
  }
  
  // Check for OpenGL errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    glDeleteRenderbuffers(1, &buffer_id);
    return absl::InternalError("OpenGL error creating depth buffer: " + std::to_string(error));
  }
  
  return buffer_id;
}

absl::StatusOr<GLuint> FBOManager::CreateStencilBuffer(int width, int height,
                                                      bool multisampled, int samples) const {
  GLuint buffer_id;
  glGenRenderbuffers(1, &buffer_id);
  if (buffer_id == 0) {
    return absl::InternalError("Failed to generate stencil buffer");
  }
  
  glBindRenderbuffer(GL_RENDERBUFFER, buffer_id);
  
  GLAttachmentFormat format = GetStencilFormat();
  
  if (multisampled) {
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format.internal_format,
                                    width, height);
  } else {
    glRenderbufferStorage(GL_RENDERBUFFER, format.internal_format, width, height);
  }
  
  // Check for OpenGL errors
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    glDeleteRenderbuffers(1, &buffer_id);
    return absl::InternalError("OpenGL error creating stencil buffer: " + std::to_string(error));
  }
  
  return buffer_id;
}

void FBOManager::DestroyFBOResources(const FBO& fbo) const {
  if (fbo.color_texture_id != 0) {
    glDeleteTextures(1, &fbo.color_texture_id);
  }
  
  if (fbo.depth_buffer_id != 0) {
    glDeleteRenderbuffers(1, &fbo.depth_buffer_id);
  }
  
  if (fbo.stencil_buffer_id != 0) {
    glDeleteRenderbuffers(1, &fbo.stencil_buffer_id);
  }
  
  if (fbo.framebuffer_id != 0) {
    glDeleteFramebuffers(1, &fbo.framebuffer_id);
  }
}

size_t FBOManager::CalculateFBOMemory(const FBO& fbo) const {
  if (!fbo.IsValid()) {
    return 0;
  }
  
  size_t memory = 0;
  
  // Color texture memory (4 bytes per pixel for RGBA8)
  memory += fbo.width * fbo.height * 4;
  
  // Depth buffer memory (4 bytes per pixel for 24-bit depth)
  if (fbo.depth_buffer_id != 0) {
    memory += fbo.width * fbo.height * 4;
  }
  
  // Stencil buffer memory (1 byte per pixel)
  if (fbo.stencil_buffer_id != 0) {
    memory += fbo.width * fbo.height * 1;
  }
  
  // Multiply by sample count for multisampling
  if (fbo.is_multisampled) {
    memory *= 4; // Assume 4x MSAA
  }
  
  return memory;
}

FBOManager::GLAttachmentFormat FBOManager::GetColorFormat() const {
  GLAttachmentFormat format;
  format.internal_format = GL_RGBA8;
  format.format = GL_RGBA;
  format.type = GL_UNSIGNED_BYTE;
  return format;
}

FBOManager::GLAttachmentFormat FBOManager::GetDepthFormat() const {
  GLAttachmentFormat format;
  format.internal_format = GL_DEPTH_COMPONENT24;
  format.format = GL_DEPTH_COMPONENT;
  format.type = GL_FLOAT;
  return format;
}

FBOManager::GLAttachmentFormat FBOManager::GetStencilFormat() const {
  GLAttachmentFormat format;
  format.internal_format = GL_STENCIL_INDEX8;
  format.format = GL_STENCIL_INDEX;
  format.type = GL_UNSIGNED_BYTE;
  return format;
}

} // namespace render
} // namespace segmecam