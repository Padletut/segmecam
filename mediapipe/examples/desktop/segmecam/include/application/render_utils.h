#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include <memory>
#include <opencv2/opencv.hpp>
#include <SDL.h>
#include <epoxy/gl.h>  // Modern OpenGL function loader
#include "application/manager_coordination.h"
#include "include/application/app_state.h"

// Forward declarations
namespace segmecam {
    class UIManager;
}

namespace segmecam {

/**
 * Utility class for rendering operations in SegmeCam
 */
class RenderUtils {
public:
    /**
     * Create an OpenGL texture from an OpenCV RGB image
     * @param display_rgb The RGB image to create texture from
     * @return OpenGL texture ID, or 0 if creation failed
     */
    static GLuint CreateVideoTexture(const cv::Mat& display_rgb);

    /**
     * Render video feed as fullscreen background with aspect ratio preservation
     * @param display_rgb The RGB image to render
     * @param window_width Window width in pixels
     * @param window_height Window height in pixels
     */
    static void RenderVideoBackground(const cv::Mat& display_rgb, int window_width, int window_height);

    /**
     * Render comprehensive UI using UIManager Enhanced
     * @param managers Reference to manager coordination structure
     * @param app_state Current application state
     * @param ui_manager UI manager instance
     * @param running Reference to running flag (can be set to false to exit)
     */
    static void RenderComprehensiveUI(ManagerCoordination::Managers& managers,
                                     const AppState& app_state,
                                     UIManager& ui_manager,
                                     bool& running);

    /**
     * Render a complete frame including video background and UI
     * @param ui_manager UI manager instance
     * @param display_rgb The RGB image to render as background
     * @param window SDL window handle
     * @param frame_count Current frame count for debugging
     * @param running Reference to running flag
     */
    static void RenderFrame(UIManager& ui_manager,
                           const cv::Mat& display_rgb,
                           SDL_Window* window,
                           int frame_count,
                           bool& running);
};

} // namespace segmecam

#endif // RENDER_UTILS_H