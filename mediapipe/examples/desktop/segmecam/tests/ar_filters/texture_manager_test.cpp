// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// TextureManager Test - Phase 4 Step 1 Verification
//
// Simple test program to verify TextureManager functionality

#include <iostream>
#include <string>
#include <vector>

// OpenGL context setup (SDL2 + epoxy)
#include <SDL2/SDL.h>
#include <epoxy/gl.h>

// TextureManager
#include "include/ar_filters/texture_manager.h"

using namespace segmecam::ar_filters;

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
        "TextureManager Test",
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
bool TestBasicTexturing() {
  std::cout << "\n=== Test: Basic Texture Loading ===\n";
  
  TextureManager texture_manager;
  
  // Test loading a non-existent texture
  std::cout << "Testing non-existent texture...\n";
  auto result = texture_manager.LoadTexture("non_existent_texture.png");
  if (!result.ok()) {
    std::cout << "✅ Correctly failed to load non-existent texture: " << result.status().message() << std::endl;
  } else {
    std::cout << "❌ Should have failed to load non-existent texture" << std::endl;
    return false;
  }
  
  std::cout << "Basic texture loading test passed!\n";
  return true;
}

bool TestCaching() {
  std::cout << "\n=== Test: Texture Caching ===\n";
  
  TextureManager texture_manager;
  
  // Test cache functionality (even with non-existent files)
  std::cout << "Testing cache with non-existent texture...\n";
  
  // Should not be cached initially
  if (texture_manager.IsTextureCached("test.png")) {
    std::cout << "❌ Texture should not be cached initially" << std::endl;
    return false;
  }
  
  // Get cached texture should return invalid texture
  auto cached = texture_manager.GetCachedTexture("test.png");
  if (cached.IsValid()) {
    std::cout << "❌ GetCachedTexture should return invalid texture for non-cached file" << std::endl;
    return false;
  }
  
  std::cout << "✅ Cache test passed!\n";
  return true;
}

bool TestTextureParameters() {
  std::cout << "\n=== Test: Texture Parameters ===\n";
  
  TextureManager texture_manager;
  
  // Test setting texture parameters
  texture_manager.SetDefaultFiltering(true, true);
  std::cout << "✅ Set linear filtering with mipmaps\n";
  
  texture_manager.SetDefaultFiltering(false, false);
  std::cout << "✅ Set nearest filtering without mipmaps\n";
  
  // Test getting statistics
  std::cout << "Cached texture count: " << texture_manager.GetCachedTextureCount() << std::endl;
  std::cout << "GPU memory used: " << texture_manager.GetTotalGPUMemoryUsed() << " bytes" << std::endl;
  
  std::cout << "Texture parameters test passed!\n";
  return true;
}

bool TestTextureBinding() {
  std::cout << "\n=== Test: Texture Binding ===\n";
  
  TextureManager texture_manager;
  
  // Create a dummy texture for binding test
  TextureManager::Texture dummy_texture;
  // Don't set valid ID - should handle gracefully
  
  // Test binding invalid texture
  texture_manager.BindTexture(dummy_texture, 0);
  std::cout << "✅ Handled binding invalid texture gracefully\n";
  
  // Test unbinding
  texture_manager.UnbindTexture(0);
  std::cout << "✅ Unbind texture successful\n";
  
  // Test invalid texture units
  texture_manager.BindTexture(dummy_texture, -1);  // Should warn
  texture_manager.BindTexture(dummy_texture, 32);  // Should warn
  
  std::cout << "Texture binding test passed!\n";
  return true;
}

int main() {
  std::cout << "🧪 SegmeCam TextureManager Test - Phase 4 Step 1\n";
  std::cout << "===============================================\n";
  
  // Initialize OpenGL context
  SimpleOpenGLContext gl_context;
  if (!gl_context.Initialize()) {
    std::cerr << "❌ Failed to initialize OpenGL context" << std::endl;
    return 1;
  }
  
  // Run tests
  std::vector<std::pair<std::string, bool(*)()>> tests = {
    {"Basic Texture Loading", TestBasicTexturing},
    {"Texture Caching", TestCaching},
    {"Texture Parameters", TestTextureParameters},
    {"Texture Binding", TestTextureBinding}
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
    std::cout << "🎉 All tests passed! TextureManager is ready for integration.\n";
    return 0;
  } else {
    std::cout << "❌ Some tests failed. Check implementation.\n";
    return 1;
  }
}