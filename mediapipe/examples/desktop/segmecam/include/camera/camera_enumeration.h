#pragma once

#include <vector>
#include <string>
#include "cam_enum.h"

namespace segmecam {

// Forward declarations
struct CameraConfig;

// Virtual camera description
struct VCamDesc {
    std::string path;
    std::string name;
};

// Camera enumeration manager - handles camera discovery and enumeration
class CameraEnumeration {
public:
    CameraEnumeration();
    ~CameraEnumeration() = default;

    // Initialization
    void Initialize();

    // Camera enumeration
    std::vector<CameraDesc> EnumerateCameras() const;
    std::vector<CameraDesc> EnumerateCamerasPortal(const CameraConfig& config) const;
    std::vector<LoopbackDesc> EnumerateLoopbackDevices() const;

    // FPS enumeration for specific camera/resolution
    std::vector<int> EnumerateFPS(const std::string& cam_path, int width, int height) const;
    std::vector<int> UpdateFPSOptions(const std::string& cam_path, int width, int height);

private:
    // Helper methods for camera enumeration
    void AddStandardResolutions(CameraDesc& cam) const;
    void SortResolutionsByArea(std::vector<std::pair<int, int>>& resolutions) const;
    void RemoveDuplicateResolutions(std::vector<std::pair<int, int>>& resolutions) const;
};

} // namespace segmecam