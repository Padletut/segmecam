// Copyright 2025 SegmeCam Contributors
// SPDX-License-Identifier: Apache-2.0
//
// ARRenderer Test - Phase 4 Step 3 Verification
//
// Test program to verify ARRenderer integration functionality

#include <iostream>
#include <string>
#include <vector>

// OpenGL context setup (SDL2 + epoxy)
#include <SDL2/SDL.h>
#include <epoxy/gl.h>

// ARRenderer
#include "include/ar_filters/ar_renderer.h"

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
        "ARRenderer Test",
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
bool TestARRendererCreation() {
  std::cout << "\n=== Test: ARRenderer Creation ===\n";
  
  ARRenderer::ARConfig config;
  config.render_width = 1280;
  config.render_height = 720;
  config.use_multisampling = true;
  config.msaa_samples = 4;
  
  auto ar_renderer = std::make_unique<ARRenderer>(config);
  
  if (!ar_renderer) {
    std::cout << "❌ Failed to create ARRenderer" << std::endl;
    return false;
  }
  
  std::cout << "✅ ARRenderer created successfully" << std::endl;
  
  // Test configuration
  const auto& retrieved_config = ar_renderer->GetConfig();
  if (retrieved_config.render_width != config.render_width ||
      retrieved_config.render_height != config.render_height) {
    std::cout << "❌ Configuration mismatch" << std::endl;
    return false;
  }
  
  std::cout << "✅ Configuration matches: " << retrieved_config.render_width 
            << "x" << retrieved_config.render_height << std::endl;
  
  std::cout << "ARRenderer creation test passed!\n";
  return true;
}

bool TestARRendererInitialization() {
  std::cout << "\n=== Test: ARRenderer Initialization ===\n";
  
  ARRenderer::ARConfig config;
  config.render_width = 512;
  config.render_height = 512;
  
  ARRenderer ar_renderer(config);
  
  // Test initialization
  auto init_status = ar_renderer.Initialize();
  if (!init_status.ok()) {
    std::cout << "❌ ARRenderer initialization failed: " << init_status.message() << std::endl;
    return false;
  }
  
  if (!ar_renderer.IsInitialized()) {
    std::cout << "❌ ARRenderer reports not initialized" << std::endl;
    return false;
  }
  
  std::cout << "✅ ARRenderer initialized successfully" << std::endl;
  
  // Test viewport
  int x, y, width, height;
  ar_renderer.GetViewport(&x, &y, &width, &height);
  std::cout << "✅ Viewport: " << x << ", " << y << ", " << width << "x" << height << std::endl;
  
  std::cout << "ARRenderer initialization test passed!\n";
  return true;
}

bool TestModelInstanceManagement() {
  std::cout << "\n=== Test: Model Instance Management ===\n";
  
  ARRenderer ar_renderer;
  auto init_status = ar_renderer.Initialize();
  if (!init_status.ok()) {
    std::cout << "❌ Failed to initialize ARRenderer" << std::endl;
    return false;
  }
  
  // Create model instance
  ARRenderer::ModelInstance instance_config;
  instance_config.position[0] = 1.0f;
  instance_config.position[1] = 2.0f;
  instance_config.position[2] = 3.0f;
  instance_config.visible = true;
  instance_config.opacity = 0.8f;
  
  auto create_status = ar_renderer.CreateModelInstance("test_glasses", "glasses_model", instance_config);
  if (!create_status.ok()) {
    std::cout << "❌ Failed to create model instance: " << create_status.message() << std::endl;
    return false;
  }
  
  std::cout << "✅ Model instance created successfully" << std::endl;
  
  // Update model instance
  instance_config.opacity = 0.5f;
  auto update_status = ar_renderer.UpdateModelInstance("test_glasses", instance_config);
  if (!update_status.ok()) {
    std::cout << "❌ Failed to update model instance: " << update_status.message() << std::endl;
    return false;
  }
  
  std::cout << "✅ Model instance updated successfully" << std::endl;
  
  // Remove model instance
  ar_renderer.RemoveModelInstance("test_glasses");
  std::cout << "✅ Model instance removed successfully" << std::endl;
  
  std::cout << "Model instance management test passed!\n";
  return true;
}

bool TestFaceLandmarkIntegration() {
  std::cout << "\n=== Test: Face Landmark Integration ===\n";
  
  ARRenderer ar_renderer;
  auto init_status = ar_renderer.Initialize();
  if (!init_status.ok()) {
    std::cout << "❌ Failed to initialize ARRenderer" << std::endl;
    return false;
  }
  
  // Create model instance for landmark attachment
  ar_renderer.CreateModelInstance("landmark_glasses", "glasses_model");
  
  // Attach to landmarks
  std::vector<int> landmark_indices = {1, 2, 17, 18}; // Example nose bridge landmarks
  auto attach_status = ar_renderer.AttachModelToLandmarks("landmark_glasses", landmark_indices);
  if (!attach_status.ok()) {
    std::cout << "❌ Failed to attach to landmarks: " << attach_status.message() << std::endl;
    return false;
  }
  
  std::cout << "✅ Model attached to landmarks successfully" << std::endl;
  
  // Update face landmarks
  std::vector<float> face_landmarks = {
    0.1f, 0.2f, 0.3f,  // Landmark 0
    0.4f, 0.5f, 0.6f,  // Landmark 1
    0.7f, 0.8f, 0.9f,  // Landmark 2
    // ... more landmarks
  };
  
  auto landmark_status = ar_renderer.UpdateFaceLandmarks(face_landmarks);
  if (!landmark_status.ok()) {
    std::cout << "❌ Failed to update landmarks: " << landmark_status.message() << std::endl;
    return false;
  }
  
  std::cout << "✅ Face landmarks updated successfully" << std::endl;
  
  std::cout << "Face landmark integration test passed!\n";
  return true;
}

bool TestRenderStatistics() {
  std::cout << "\n=== Test: Render Statistics ===\n";
  
  ARRenderer ar_renderer;
  auto init_status = ar_renderer.Initialize();
  if (!init_status.ok()) {
    std::cout << "❌ Failed to initialize ARRenderer" << std::endl;
    return false;
  }
  
  // Get initial statistics
  int total_models, visible_instances;
  size_t gpu_memory_used;
  ar_renderer.GetRenderStatistics(&total_models, &visible_instances, &gpu_memory_used);
  
  std::cout << "Initial stats:" << std::endl;
  std::cout << "  Total models: " << total_models << std::endl;
  std::cout << "  Visible instances: " << visible_instances << std::endl;
  std::cout << "  GPU memory used: " << gpu_memory_used << " bytes" << std::endl;
  
  // Create some instances
  ar_renderer.CreateModelInstance("stats_test_1", "model1");
  ar_renderer.CreateModelInstance("stats_test_2", "model2");
  
  ARRenderer::ModelInstance hidden_instance;
  hidden_instance.visible = false;
  ar_renderer.CreateModelInstance("stats_test_hidden", "model3", hidden_instance);
  
  // Get updated statistics
  ar_renderer.GetRenderStatistics(&total_models, &visible_instances, &gpu_memory_used);
  
  std::cout << "After creating instances:" << std::endl;
  std::cout << "  Total models: " << total_models << std::endl;
  std::cout << "  Visible instances: " << visible_instances << std::endl;
  std::cout << "  GPU memory used: " << gpu_memory_used << " bytes" << std::endl;
  
  if (visible_instances != 2) {
    std::cout << "❌ Expected 2 visible instances, got " << visible_instances << std::endl;
    return false;
  }
  
  std::cout << "✅ Statistics reporting correctly" << std::endl;
  
  std::cout << "Render statistics test passed!\n";
  return true;
}

int main() {
  std::cout << "🧪 SegmeCam ARRenderer Test - Phase 4 Step 3\n";
  std::cout << "===============================================\n";
  
  // Initialize OpenGL context
  SimpleOpenGLContext gl_context;
  if (!gl_context.Initialize()) {
    std::cerr << "❌ Failed to initialize OpenGL context" << std::endl;
    return 1;
  }
  
  // Run tests
  std::vector<std::pair<std::string, bool(*)()>> tests = {
    {"ARRenderer Creation", TestARRendererCreation},
    {"ARRenderer Initialization", TestARRendererInitialization},
    {"Model Instance Management", TestModelInstanceManagement},
    {"Face Landmark Integration", TestFaceLandmarkIntegration},
    {"Render Statistics", TestRenderStatistics}
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
    std::cout << "🎉 All tests passed! ARRenderer integration layer is ready.\n";
    return 0;
  } else {
    std::cout << "❌ Some tests failed. Check implementation.\n";
    return 1;
  }
}