#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct VideoResolution {
    int width = 0;
    int height = 0;
    std::vector<double> fps; // frames per second
};

struct VideoFormat {
    uint32_t fourcc = 0;           // V4L2 pixel format
    std::string description;       // Human-readable description
    std::vector<VideoResolution> resolutions;
};

struct VideoDevice {
    std::string path;              // e.g., /dev/video0
    std::string name;              // card/driver name
    std::string bus_info;          // bus info to deduplicate
    std::vector<VideoFormat> formats;
};

// Enumerate video capture devices and their formats/resolutions via V4L2.
std::vector<VideoDevice> enumerate_video_devices();

// Utility to make FOURCC into string.
std::string fourcc_to_string(uint32_t fourcc);
