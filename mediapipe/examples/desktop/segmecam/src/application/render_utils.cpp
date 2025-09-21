#include "mediapipe/examples/desktop/segmecam/include/application/render_utils.h"
#include "include/ui/ui_manager_enhanced.h"
#include "mediapipe/examples/desktop/segmecam/app_state.h"

// Include ImGui for GUI
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h"
#include "third_party/imgui/backends/imgui_impl_opengl3.h"

// Include OpenCV for image processing
#include <opencv2/opencv.hpp>

// Include SDL for window management
#include <SDL.h>

// Include OpenGL
#include <GL/gl.h>

#include <iostream>

namespace segmecam {

GLuint RenderUtils::CreateVideoTexture(const cv::Mat& display_rgb) {
    GLuint video_texture = 0;
    if (!display_rgb.empty()) {
        // Debug output for texture upload
        static int texture_debug_count = 0;
        texture_debug_count++;
        if (texture_debug_count <= 3) {
            cv::Vec3b center_pixel = display_rgb.at<cv::Vec3b>(display_rgb.rows/2, display_rgb.cols/2);
            std::cout << "🔍 TEXTURE UPLOAD " << texture_debug_count << " - format: " << display_rgb.type()
                      << " center pixel RGB: [" << (int)center_pixel[0] << "," << (int)center_pixel[1] << "," << (int)center_pixel[2] << "]" << std::endl;
        }

        // Ensure GL interprets tightly packed RGB rows (like original)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glGenTextures(1, &video_texture);
        glBindTexture(GL_TEXTURE_2D, video_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, display_rgb.cols, display_rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, display_rgb.data);
    }
    return video_texture;
}

void RenderUtils::RenderVideoBackground(const cv::Mat& display_rgb, int window_width, int window_height) {
    // Calculate aspect-preserving dimensions to fill window
    float video_aspect = (float)display_rgb.cols / (float)display_rgb.rows;
    float window_aspect = (float)window_width / (float)window_height;

    float quad_w, quad_h, quad_x, quad_y;
    if (window_aspect > video_aspect) {
        // Window is wider - scale to height, center horizontally
        quad_h = window_height;
        quad_w = quad_h * video_aspect;
        quad_x = (window_width - quad_w) * 0.5f;
        quad_y = 0;
    } else {
        // Window is taller - scale to width, center vertically
        quad_w = window_width;
        quad_h = quad_w / video_aspect;
        quad_x = 0;
        quad_y = (window_height - quad_h) * 0.5f;
    }

    // Set up orthographic projection for fullscreen quad (like original)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, window_height, 0, -1, 1);  // Y-flipped for texture coordinates
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Enable texturing and proper blending
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set white color (full brightness)
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Draw fullscreen textured quad (matching original coordinate system)
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(quad_x, quad_y);
    glTexCoord2f(1, 0); glVertex2f(quad_x + quad_w, quad_y);
    glTexCoord2f(1, 1); glVertex2f(quad_x + quad_w, quad_y + quad_h);
    glTexCoord2f(0, 1); glVertex2f(quad_x, quad_y + quad_h);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void RenderUtils::RenderComprehensiveUI(ManagerCoordination::Managers& managers,
                                       const AppState& app_state,
                                       UIManager& ui_manager,
                                       bool& running) {
    // Use UIManager Enhanced for comprehensive panels
    ui_manager.ProcessEvents(running);
    ui_manager.BeginFrame();
    ui_manager.RenderUI();
    ui_manager.EndFrame();
}

void RenderUtils::RenderFrame(UIManager& ui_manager,
                             const cv::Mat& display_rgb,
                             SDL_Window* window,
                             int frame_count,
                             bool& running) {
    // Get window size for rendering
    int dw, dh;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);

    // Clear screen with dark background
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Create OpenGL texture from processed frame for background display
    GLuint video_texture = CreateVideoTexture(display_rgb);

    // Render camera feed as fullscreen background
    if (video_texture && !display_rgb.empty()) {
        glBindTexture(GL_TEXTURE_2D, video_texture);
        RenderVideoBackground(display_rgb, dw, dh);
    }

    // Begin ImGui frame and render UI panels on top
    ui_manager.BeginFrame();
    ui_manager.RenderUI();
    ui_manager.EndFrame();

    if (frame_count <= 5) {
        std::cout << "✅ UI rendered successfully for frame " << frame_count << std::endl;
    }

    // Clean up texture
    if (video_texture) {
        glDeleteTextures(1, &video_texture);
    }
}

} // namespace segmecam