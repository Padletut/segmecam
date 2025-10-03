#ifndef SEGMECAM_RENDER_MANAGER_H
#define SEGMECAM_RENDER_MANAGER_H

#include <string>
#include <memory>

#include <SDL.h>
// Note: Don't include SDL_opengl.h when using epoxy - epoxy provides all OpenGL functions
#include <epoxy/gl.h>  // Modern OpenGL function loader
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "mediapipe/framework/port/opencv_core_inc.h"

// Forward declarations
namespace segmecam {
namespace render {
    class ShaderProgram;
}
namespace ar_filters {
    struct Model;
    struct Material;
}
}

namespace segmecam {

/**
 * Configuration for render system initialization
 */
struct RenderConfig {
    std::string window_title = "SegmeCam";
    int window_width = 1280;
    int window_height = 720;
    bool vsync_enabled = true;
    bool allow_highdpi = true;
    bool resizable = true;
    
    // OpenGL settings
    int gl_major_version = 3;
    int gl_minor_version = 3;
    int depth_size = 24;
    int stencil_size = 8;
};

/**
 * Render system state and runtime information
 */
struct RenderState {
    bool is_initialized = false;
    bool window_should_close = false;
    int drawable_width = 0;
    int drawable_height = 0;
    bool vsync_enabled = true;
    
    // Texture state
    unsigned int current_texture = 0;
    int texture_width = 0;
    int texture_height = 0;
    
    // Performance tracking
    float fps = 0.0f;
    uint32_t last_frame_time = 0;
};

/**
 * Render Manager class
 * Handles SDL2 window management, OpenGL context, Dear ImGui setup,
 * texture management, and frame presentation
 */
class RenderManager {
public:
    RenderManager();
    ~RenderManager();

    /**
     * Initialize the rendering system with given configuration
     * @param config Render configuration
     * @return 0 on success, error code on failure
     */
    int Initialize(const RenderConfig& config);

    /**
     * Process SDL events (window close, resize, etc.)
     * @param should_quit Set to true if application should quit
     * @return 0 on success, error code on failure
     */
    int ProcessEvents(bool& should_quit);

    /**
     * Begin a new rendering frame
     * Sets up ImGui new frame and prepares for UI rendering
     */
    void BeginFrame();

    /**
     * Complete frame rendering and present to screen
     * Renders ImGui draw data and swaps buffers
     */
    void EndFrame();

    /**
     * Upload image data to GPU texture for display
     * @param rgb_image RGB image data (CV_8UC3)
     */
    void UploadTexture(const cv::Mat& rgb_image);

    /**
     * Render background image/texture behind UI elements
     * @param show_preview Whether to show the camera preview
     */
    void RenderBackground(bool show_preview = true);

    /**
     * Set VSync enabled/disabled
     * @param enabled True to enable VSync
     */
    void SetVSync(bool enabled);

    /**
     * Get the current render state (read-only)
     */
    const RenderState& GetState() const { return state_; }

    /**
     * Get SDL window handle for advanced operations
     */
    SDL_Window* GetWindow() { return window_; }

    /**
     * Get current texture ID for ImGui rendering
     */
    unsigned int GetTextureID() const { return state_.current_texture; }

    /**
     * Get current texture dimensions
     */
    void GetTextureDimensions(int& width, int& height) const {
        width = state_.texture_width;
        height = state_.texture_height;
    }

    /**
     * Clean shutdown of all render resources
     */
    void Cleanup();

    // ===== 3D RENDERING API (Phase 3 Step 8) =====
    
    /**
     * Initialize 3D rendering system (shaders, OpenGL state)
     * @return true on success, false on failure
     */
    bool Initialize3DRendering();
    
    /**
     * Update projection matrix (call on window resize)
     * @param width Window width in pixels
     * @param height Window height in pixels
     */
    void UpdateProjectionMatrix(int width, int height);
    
    /**
     * Render a 3D model with given transformation and material
     * @param model Model data (meshes with VAO/VBO/EBO)
     * @param model_matrix Transformation matrix (position, rotation, scale)
     * @param material Material properties (ambient, diffuse, specular, etc.)
     */
    void Render3DModel(const ar_filters::Model& model,
                      const glm::mat4& model_matrix,
                      const ar_filters::Material& material);
    
    /**
     * Get current projection matrix (for external use)
     */
    const glm::mat4& GetProjectionMatrix() const { return projection_matrix_; }
    
    /**
     * Get current view matrix (for external use)
     */
    const glm::mat4& GetViewMatrix() const { return view_matrix_; }

private:
    RenderConfig config_;
    RenderState state_;
    
    // SDL/OpenGL resources
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    
    // 3D rendering resources (Phase 3 Step 8)
    std::unique_ptr<render::ShaderProgram> model_shader_;
    glm::mat4 projection_matrix_;
    glm::mat4 view_matrix_;
    
    // Lighting parameters
    glm::vec3 light_direction_;
    glm::vec3 light_color_;
    glm::vec3 ambient_color_;
    glm::vec3 camera_position_;
    
    // Internal helper functions
    int SetupSDL();
    int SetupOpenGL();
    int SetupImGui();
    void UpdatePerformanceStats();
    void CleanupResources();
};

} // namespace segmecam

#endif // SEGMECAM_RENDER_MANAGER_H