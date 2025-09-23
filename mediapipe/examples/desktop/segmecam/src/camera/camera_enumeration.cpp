#include "include/camera/camera_enumeration.h"

#include <iostream>
#include <algorithm>

// Forward declarations for camera enumeration functions
extern std::vector<CameraDesc> EnumerateCameras();
extern std::vector<int> EnumerateFPS(const std::string& cam_path, int width, int height);

namespace segmecam {

CameraEnumeration::CameraEnumeration() = default;

void CameraEnumeration::Initialize() {
    // Camera enumeration initialization - currently no-op
    std::cout << "📹 Camera enumeration initialized" << std::endl;
}

std::vector<CameraDesc> CameraEnumeration::EnumerateCameras() const {
    return ::EnumerateCameras();
}

std::vector<CameraDesc> CameraEnumeration::EnumerateCamerasPortal(const CameraConfig& config) const {
    std::vector<CameraDesc> cams;

    CameraDesc cam;
    cam.path = "PipeWire";
    cam.name = "PipeWire Camera";
    cam.index = 0;

    cam.resolutions = {
        {640, 480},
        {800, 600},
        {960, 720},
        {1280, 720},
        {1600, 900},
        {1920, 1080}
    };

    // Sort resolutions by area (smallest first)
    SortResolutionsByArea(cam.resolutions);
    RemoveDuplicateResolutions(cam.resolutions);

    cams.push_back(cam);
    return cams;
}

std::vector<LoopbackDesc> CameraEnumeration::EnumerateLoopbackDevices() const {
    // This would typically enumerate v4l2loopback devices
    // For now, return empty list as this is handled elsewhere
    return {};
}

std::vector<int> CameraEnumeration::EnumerateFPS(const std::string& cam_path, int width, int height) const {
    return ::EnumerateFPS(cam_path, width, height);
}

void CameraEnumeration::AddStandardResolutions(CameraDesc& cam) const {
    // Add common resolutions if not already present
    std::vector<std::pair<int, int>> standard_resolutions = {
        {640, 480}, {800, 600}, {1024, 768}, {1280, 720},
        {1280, 1024}, {1600, 900}, {1920, 1080}
    };

    for (const auto& res : standard_resolutions) {
        if (std::find(cam.resolutions.begin(), cam.resolutions.end(), res) == cam.resolutions.end()) {
            cam.resolutions.push_back(res);
        }
    }
}

void CameraEnumeration::SortResolutionsByArea(std::vector<std::pair<int, int>>& resolutions) const {
    std::sort(resolutions.begin(), resolutions.end(), [](const auto& a, const auto& b) {
        long area_a = static_cast<long>(a.first) * a.second;
        long area_b = static_cast<long>(b.first) * b.second;
        if (area_a == area_b) {
            return a.first < b.first;
        }
        return area_a < area_b;
    });
}

void CameraEnumeration::RemoveDuplicateResolutions(std::vector<std::pair<int, int>>& resolutions) const {
    auto last = std::unique(resolutions.begin(), resolutions.end());
    resolutions.erase(last, resolutions.end());
}

std::vector<int> CameraEnumeration::UpdateFPSOptions(const std::string& cam_path, int width, int height) {
    return EnumerateFPS(cam_path, width, height);
}

} // namespace segmecam