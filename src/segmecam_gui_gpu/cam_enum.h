#pragma once

#include <string>
#include <vector>
#include <utility>
#include <cstdint>

struct CameraDesc {
  std::string path;   // e.g. /dev/video0
  std::string name;   // human-friendly name (driver/card)
  std::string bus;    // physical bus identifier
  int index = -1;     // numeric index parsed from path
  std::vector<std::pair<int,int>> resolutions; // unique supported WxH
};

// Enumerate /dev/video* devices with V4L2 and collect supported resolutions.
// If V4L2 queries fail for a device, returns it with empty resolutions.
std::vector<CameraDesc> EnumerateCameras();

// Enumerate available discrete FPS values for a camera path at WxH.
std::vector<int> EnumerateFPS(const std::string& cam_path, int width, int height);

struct CtrlRange { int32_t min=0,max=0,step=1,def=0,val=0; bool available=false; };

bool QueryCtrl(const std::string& cam_path, uint32_t id, CtrlRange* out);
bool GetCtrl(const std::string& cam_path, uint32_t id, int32_t* value);
bool SetCtrl(const std::string& cam_path, uint32_t id, int32_t value);
