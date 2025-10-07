#pragma once

#include <vector>
#include <string>
#include "camera_enumeration.h"

namespace segmecam {

// Forward declarations
struct CameraConfig;
struct CameraState;

// Camera setup manager - handles initial camera selection and configuration
class CameraSetup {
public:
    CameraSetup();
    ~CameraSetup() = default;

    // Initialization
    void Initialize(const CameraConfig& config, const std::vector<CameraDesc>& cameras, CameraState& state);

    // Initial camera selection
    void SelectInitialCamera(const std::vector<CameraDesc>& cameras, const CameraConfig& config, CameraState& state);
    void SelectInitialResolution(const std::vector<CameraDesc>& cameras, const CameraConfig& config, CameraState& state);
    void SetupCameraPathAndFPS(const std::vector<CameraDesc>& cameras, const CameraConfig& config, CameraState& state, std::vector<int>& fps_options);

    // FPS options management
    void UpdateFPSOptions(const std::string& cam_path, int width, int height, std::vector<int>& fps_options);

    // Logging
    void LogInitializationSuccess(const CameraState& state);

private:
    // Helper methods
    int FindBestResolutionIndex(const std::vector<std::pair<int, int>>& resolutions, int width, int height) const;
    int FindBestFPSIndex(const std::vector<int>& fps_options, int target_fps) const;
};

} // namespace segmecam