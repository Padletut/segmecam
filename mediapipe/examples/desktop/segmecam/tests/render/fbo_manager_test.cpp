// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// FBOManager Test - Phase 4 Step 2 Verification
//
// Simple test program to verify FBOManager functionality

#include <iostream>
#include <string>
#include <vector>

// OpenGL context setup (SDL2 + epoxy)
#include <SDL2/SDL.h>
#include <epoxy/gl.h>

// FBOManager
#include "include/render/fbo_manager.h"

using namespace segmecam::render;

class SimpleOpenGLContext {
public:
  SimpleOpenGLContext() : window_(nullptr), context_(nullptr) {}
  
  ~SimpleOpenGLContext() {
    Cleanup();
  }
  
  bool Initialize() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
      return false;
    }
    
    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    // Create window (hidden)
    window_ = SDL_CreateWindow(
        "FBOManager Test",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
    );
    
    if (!window_) {
      std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
      return false;
    }
    
    // Create OpenGL context
    context_ = SDL_GL_CreateContext(window_);
    if (!context_) {
      std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
      return false;
    }
    
    // Make context current
    SDL_GL_MakeCurrent(window_, context_);
    
    // Print OpenGL info
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    
    return true;
  }
  
  void Cleanup() {
    if (context_) {
      SDL_GL_DeleteContext(context_);
      context_ = nullptr;
    }
    
    if (window_) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
    }
    
    SDL_Quit();
  }
  
private:
  SDL_Window* window_;
  SDL_GLContext context_;
};

// Test functions
bool TestBasicFBOCreation() {
  std::cout << "\n=== Test: Basic FBO Creation ===\n";
  
  FBOManager fbo_manager;
  
  // Test creating an FBO
  FBOManager::FBOConfig config;
  config.width = 512;
  config.height = 512;
  config.use_depth_buffer = true;
  config.use_stencil_buffer = false;
  config.use_multisampling = false;
  
  std::cout << "Creating 512x512 FBO with depth buffer...\n";
  auto result = fbo_manager.CreateFBO("test_fbo", config);
  
  if (!result.ok()) {
    std::cout << "❌ Failed to create FBO: " << result.status().message() << std::endl;
    return false;
  }
  
  auto fbo = result.value();
  if (!fbo.IsValid()) {
    std::cout << "❌ Created FBO is not valid" << std::endl;
    return false;
  }
  
  std::cout << "✅ FBO created successfully (ID: " << fbo.framebuffer_id 
            << ", Color: " << fbo.color_texture_id << ")" << std::endl;
  
  // Test getting the FBO
  auto retrieved_fbo = fbo_manager.GetFBO("test_fbo");
  if (!retrieved_fbo.IsValid() || retrieved_fbo.framebuffer_id != fbo.framebuffer_id) {
    std::cout << "❌ Failed to retrieve FBO" << std::endl;
    return false;
  }
  
  std::cout << "✅ FBO retrieval successful\n";
  
  std::cout << "Basic FBO creation test passed!\n";
  return true;
}

bool TestFBOBinding() {
  std::cout << "\n=== Test: FBO Binding ===\n";
  
  FBOManager fbo_manager;
  
  // Create test FBO
  FBOManager::FBOConfig config;
  config.width = 256;
  config.height = 256;
  
  auto result = fbo_manager.CreateFBO("bind_test", config);
  if (!result.ok()) {
    std::cout << "❌ Failed to create test FBO" << std::endl;
    return false;
  }
  
  // Test binding FBO
  bool bind_success = fbo_manager.BindFramebuffer("bind_test");
  if (!bind_success) {
    std::cout << "❌ Failed to bind FBO" << std::endl;
    return false;
  }
  
  std::cout << "✅ FBO binding successful\n";
  
  // Test clearing framebuffer
  fbo_manager.ClearFramebuffer(1.0f, 0.0f, 0.0f, 1.0f, true, false);
  std::cout << "✅ FBO clearing successful\n";
  
  // Test unbinding
  fbo_manager.UnbindFramebuffer();
  std::cout << "✅ FBO unbinding successful\n";
  
  std::cout << "FBO binding test passed!\n";
  return true;
}

bool TestFBOResize() {
  std::cout << "\n=== Test: FBO Resize ===\n";
  
  FBOManager fbo_manager;
  
  // Create test FBO
  FBOManager::FBOConfig config;
  config.width = 128;
  config.height = 128;
  
  auto result = fbo_manager.CreateFBO("resize_test", config);
  if (!result.ok()) {
    std::cout << "❌ Failed to create test FBO" << std::endl;
    return false;
  }
  
  auto original_fbo = result.value();
  std::cout << "Original FBO size: " << original_fbo.width << "x" << original_fbo.height << std::endl;
  
  // Test resizing
  auto resize_status = fbo_manager.ResizeFBO("resize_test", 256, 256);
  if (!resize_status.ok()) {
    std::cout << "❌ Failed to resize FBO: " << resize_status.message() << std::endl;
    return false;
  }
  
  auto resized_fbo = fbo_manager.GetFBO("resize_test");
  if (resized_fbo.width != 256 || resized_fbo.height != 256) {
    std::cout << "❌ FBO resize failed - wrong dimensions" << std::endl;
    return false;
  }
  
  std::cout << "✅ FBO resized successfully to " << resized_fbo.width << "x" << resized_fbo.height << std::endl;
  
  std::cout << "FBO resize test passed!\n";
  return true;
}

bool TestFBOStatistics() {
  std::cout << "\n=== Test: FBO Statistics ===\n";
  
  FBOManager fbo_manager;
  
  // Initial statistics
  std::cout << "Initial FBO count: " << fbo_manager.GetFBOCount() << std::endl;
  std::cout << "Initial GPU memory: " << fbo_manager.GetTotalGPUMemoryUsed() << " bytes" << std::endl;
  
  // Create multiple FBOs
  FBOManager::FBOConfig config;
  config.width = 256;
  config.height = 256;
  config.use_depth_buffer = true;
  
  fbo_manager.CreateFBO("stats_fbo_1", config);
  fbo_manager.CreateFBO("stats_fbo_2", config);
  
  std::cout << "After creating 2 FBOs:" << std::endl;
  std::cout << "FBO count: " << fbo_manager.GetFBOCount() << std::endl;
  std::cout << "GPU memory used: " << fbo_manager.GetTotalGPUMemoryUsed() << " bytes" << std::endl;
  
  // Test color texture access
  GLuint color_texture = fbo_manager.GetColorTexture("stats_fbo_1");
  if (color_texture == 0) {
    std::cout << "❌ Failed to get color texture" << std::endl;
    return false;
  }
  
  std::cout << "✅ Color texture ID: " << color_texture << std::endl;
  
  // Test destroying single FBO
  fbo_manager.DestroyFBO("stats_fbo_1");
  std::cout << "After destroying 1 FBO:" << std::endl;
  std::cout << "FBO count: " << fbo_manager.GetFBOCount() << std::endl;
  
  std::cout << "FBO statistics test passed!\n";
  return true;
}

bool TestFBOParameters() {
  std::cout << "\n=== Test: FBO Parameters ===\n";
  
  FBOManager fbo_manager;
  
  // Test default clear color
  fbo_manager.SetDefaultClearColor(0.5f, 0.7f, 0.2f, 1.0f);
  std::cout << "✅ Set default clear color\n";
  
  // Test viewport functions
  int x, y, width, height;
  fbo_manager.GetCurrentViewport(&x, &y, &width, &height);
  std::cout << "Current viewport: " << x << ", " << y << ", " << width << "x" << height << std::endl;
  
  fbo_manager.SetViewport(0, 0, 512, 512);
  std::cout << "✅ Set viewport to 512x512\n";
  
  // Test framebuffer completeness check
  bool complete = FBOManager::CheckFramebufferComplete();
  std::cout << "Framebuffer complete: " << (complete ? "Yes" : "No") << std::endl;
  
  std::cout << "FBO parameters test passed!\n";
  return true;
}

int main() {
  std::cout << "🧪 SegmeCam FBOManager Test - Phase 4 Step 2\n";
  std::cout << "===============================================\n";
  
  // Initialize OpenGL context
  SimpleOpenGLContext gl_context;
  if (!gl_context.Initialize()) {
    std::cerr << "❌ Failed to initialize OpenGL context" << std::endl;
    return 1;
  }
  
  // Run tests
  std::vector<std::pair<std::string, bool(*)()>> tests = {
    {"Basic FBO Creation", TestBasicFBOCreation},
    {"FBO Binding", TestFBOBinding},
    {"FBO Resize", TestFBOResize},
    {"FBO Statistics", TestFBOStatistics},
    {"FBO Parameters", TestFBOParameters}
  };
  
  int passed = 0;
  int total = tests.size();
  
  for (const auto& [test_name, test_func] : tests) {
    try {
      if (test_func()) {
        passed++;
        std::cout << "✅ " << test_name << " PASSED\n";
      } else {
        std::cout << "❌ " << test_name << " FAILED\n";
      }
    } catch (const std::exception& e) {
      std::cout << "❌ " << test_name << " FAILED with exception: " << e.what() << "\n";
    }
  }
  
  std::cout << "\n===============================================\n";
  std::cout << "📊 Test Results: " << passed << "/" << total << " tests passed\n";
  
  if (passed == total) {
    std::cout << "🎉 All tests passed! FBOManager is ready for integration.\n";
    return 0;
  } else {
    std::cout << "❌ Some tests failed. Check implementation.\n";
    return 1;
  }
}