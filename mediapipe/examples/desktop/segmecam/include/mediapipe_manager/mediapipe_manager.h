#ifndef SEGMECAM_MEDIAPIPE_MANAGER_H
#define SEGMECAM_MEDIAPIPE_MANAGER_H

#include <string>
#include <memory>

#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/port/opencv_core_inc.h"
#include "gpu_detector.h"

namespace segmecam {

/**
 * Configuration for MediaPipe graph initialization
 */
struct MediaPipeConfig {
    std::string graph_path;
    std::string resource_root_dir = ".";
    bool use_gpu = false;
    bool force_cpu = false;
    GPUCapabilities gpu_capabilities;
};

/**
 * MediaPipe processing state and runtime information
 */
struct MediaPipeState {
    bool is_initialized = false;
    bool is_running = false;
    std::string loaded_graph_path;
    bool gpu_resources_available = false;
};

/**
 * MediaPipe Manager class
 * Handles all MediaPipe-specific operations including graph loading,
 * initialization, GPU resource management, and processing pipeline
 */
class MediaPipeManager {
public:
    MediaPipeManager();
    ~MediaPipeManager();

    /**
     * Initialize MediaPipe with the given configuration
     * @param config MediaPipe configuration including graph path and GPU settings
     * @return 0 on success, error code on failure
     */
    int Initialize(const MediaPipeConfig& config);

    /**
     * Setup output stream pollers before starting the graph
     * @param use_face_landmarks Whether to setup face landmarks pollers
     * @return 0 on success, error code on failure
     */
    int SetupOutputPollers(bool use_face_landmarks);

    /**
     * Start the MediaPipe graph processing
     * @return 0 on success, error code on failure
     */
    int Start();

    /**
     * Stop the MediaPipe graph processing
     * @return 0 on success, error code on failure
     */
    int Stop();

    /**
     * Clean shutdown of MediaPipe resources
     */
    void Cleanup();

    /**
     * Process a frame through the MediaPipe graph
     * @param input_frame The input image frame
     * @param timestamp_us Timestamp in microseconds
     * @return Status of the processing operation
     */
    mediapipe::Status ProcessFrame(const cv::Mat& input_frame, int64_t timestamp_us);

    /**
     * Get the current MediaPipe state (read-only)
     */
    const MediaPipeState& GetState() const { return state_; }

    /**
     * Get the MediaPipe calculator graph (for advanced operations)
     */
    mediapipe::CalculatorGraph* GetGraph() { return graph_.get(); }

    /**
     * Check if MediaPipe is ready for processing
     */
    bool IsReady() const { return state_.is_initialized && state_.is_running; }

private:
    MediaPipeConfig config_;
    MediaPipeState state_;
    std::unique_ptr<mediapipe::CalculatorGraph> graph_;

    // Internal initialization steps
    int LoadGraphConfiguration();
    int SetupGPUResources();
    int InitializeGraph();

    // Helper functions
    mediapipe::StatusOr<std::string> LoadTextFile(const std::string& path);
};

} // namespace segmecam

#endif // SEGMECAM_MEDIAPIPE_MANAGER_H