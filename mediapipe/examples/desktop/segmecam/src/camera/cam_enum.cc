#include "include/camera/cam_enum.h"

#include <algorithm>
#include <set>
#include <filesystem>
#include <cmath>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>

namespace fs = std::filesystem;

static int parse_index(const std::string& path) {
  // naive: find trailing digits
  int idx = -1; int mul = 1; for (int i=(int)path.size()-1;i>=0;--i){
    if (path[i]<'0'||path[i]>'9') break; idx = (idx<0?0:idx) + (path[i]-'0')*mul; mul*=10; }
  return idx;
}

static void add_unique(std::vector<std::pair<int,int>>& v, int w, int h) {
  if (w<=0||h<=0) return; if (w>7680||h>4320) return; // sanity
  for (auto& p: v) if (p.first==w && p.second==h) return;
  v.emplace_back(w,h);
}

static void add_common_resolutions_in_range(const v4l2_frmsize_stepwise& sw, std::vector<std::pair<int,int>>& out) {
  const int common[][2]={{320,240},{640,360},{640,480},{800,600},{960,540},{1024,576},{1280,720},{1280,800},{1600,900},{1920,1080},{2560,1440},{3840,2160}};
  for (auto c: common) {
    int w=c[0], h=c[1]; if (w>=sw.min_width&&w<=sw.max_width&&h>=sw.min_height&&h<=sw.max_height) add_unique(out,w,h);
  }
}

static void add_stepwise_resolutions(const v4l2_frmsize_stepwise& sw, std::vector<std::pair<int,int>>& out) {
  int minW = sw.min_width, maxW = sw.max_width, stepW = std::max(1u, sw.step_width);
  int minH = sw.min_height, maxH = sw.max_height, stepH = std::max(1u, sw.step_height);
  for (int w=minW; w<=maxW; w+=std::max(stepW, (maxW-minW)/3)) {
    for (int h=minH; h<=maxH; h+=std::max(stepH, (maxH-minH)/3)) add_unique(out,w,h);
  }
}

static void enum_discrete_framesizes(int fd, uint32_t pixfmt, std::vector<std::pair<int,int>>& out) {
  v4l2_frmsizeenum fse{}; fse.pixel_format = pixfmt;
  for (fse.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fse) == 0; ++fse.index) {
    if (fse.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
      add_unique(out, fse.discrete.width, fse.discrete.height);
    }
  }
}

static void enum_stepwise_framesizes(int fd, uint32_t pixfmt, std::vector<std::pair<int,int>>& out) {
  v4l2_frmsizeenum fse{}; fse.pixel_format = pixfmt;
  for (fse.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fse) == 0; ++fse.index) {
    if (fse.type == V4L2_FRMSIZE_TYPE_STEPWISE || fse.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
      add_common_resolutions_in_range(fse.stepwise, out);
      add_stepwise_resolutions(fse.stepwise, out);
    }
  }
}

static void enum_framesizes(int fd, uint32_t pixfmt, std::vector<std::pair<int,int>>& out) {
  enum_discrete_framesizes(fd, pixfmt, out);
  enum_stepwise_framesizes(fd, pixfmt, out);
}

static bool is_valid_camera_device(const std::string& path) {
  return path.find("/dev/video") != std::string::npos;
}

static bool query_device_capabilities(int fd, CameraDesc& cd) {
  v4l2_capability cap{};
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) != 0) {
    cd.name = "Video Device";
    return false;
  }
  
  cd.name = reinterpret_cast<const char*>(cap.card);
  cd.bus  = reinterpret_cast<const char*>(cap.bus_info);
  
  uint32_t caps = (cap.device_caps != 0) ? cap.device_caps : cap.capabilities;
  bool is_capture = (caps & V4L2_CAP_VIDEO_CAPTURE) || (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);
  bool is_output  = (caps & V4L2_CAP_VIDEO_OUTPUT) || (caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE);
  
  return is_capture && !is_output;
}

static void enumerate_device_formats_and_resolutions(int fd, CameraDesc& cd) {
  std::set<uint32_t> fmts = {V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_NV12};
  
  // Query driver-supported formats
  v4l2_fmtdesc fm{}; 
  fm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  for (fm.index = 0; ioctl(fd, VIDIOC_ENUM_FMT, &fm) == 0; ++fm.index) {
    fmts.insert(fm.pixelformat);
  }
  
  for (auto f : fmts) {
    enum_framesizes(fd, f, cd.resolutions);
  }
  
  // Sort by area then width
  std::sort(cd.resolutions.begin(), cd.resolutions.end(), 
    [](auto& a, auto& b){ 
      long aa = (long)a.first * a.second, bb = (long)b.first * b.second; 
      if (aa != bb) return aa < bb; 
      return a.first < b.first;
    });
}

static std::vector<CameraDesc> deduplicate_cameras_by_bus(const std::vector<CameraDesc>& cams) {
  std::vector<CameraDesc> unique;
  std::set<std::string> seen_bus;
  
  for (const auto& c : cams) {
    if (!c.bus.empty()) {
      if (seen_bus.count(c.bus)) continue;
      seen_bus.insert(c.bus);
    }
    unique.push_back(c);
  }
  
  return unique;
}

static CameraDesc process_camera_device(const std::string& device_path) {
  CameraDesc cd; 
  cd.path = device_path; 
  cd.index = parse_index(device_path);
  
  int fd = ::open(device_path.c_str(), O_RDWR | O_NONBLOCK);
  if (fd >= 0) {
    if (query_device_capabilities(fd, cd)) {
      enumerate_device_formats_and_resolutions(fd, cd);
    } else {
      ::close(fd);
      cd.name = "(invalid device)";
      return cd;
    }
    ::close(fd);
  } else {
    cd.name = "(unavailable)";
  }
  
  return cd;
}

static std::vector<std::string> find_camera_devices() {
  std::vector<std::string> devices;
  std::error_code ec;
  
  for (const auto& entry : fs::directory_iterator("/dev", ec)) {
    if (ec) break;
    const auto p = entry.path();
    if (!fs::is_character_file(p, ec)) continue;
    
    const std::string sp = p.string();
    if (is_valid_camera_device(sp)) {
      devices.push_back(sp);
    }
  }
  
  return devices;
}

std::vector<CameraDesc> EnumerateCameras() {
  std::vector<CameraDesc> cams;
  
  for (const auto& device_path : find_camera_devices()) {
    CameraDesc cd; 
    cd.path = device_path; 
    cd.index = parse_index(device_path);
    
    int fd = ::open(device_path.c_str(), O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
      v4l2_capability cap{};
      if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        cd.name = reinterpret_cast<const char*>(cap.card);
        cd.bus  = reinterpret_cast<const char*>(cap.bus_info);
        uint32_t caps = (cap.device_caps != 0) ? cap.device_caps : cap.capabilities;
        bool is_capture = (caps & V4L2_CAP_VIDEO_CAPTURE) || (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE);
        bool is_output  = (caps & V4L2_CAP_VIDEO_OUTPUT) || (caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE);
        if (is_capture && !is_output) {
          enumerate_device_formats_and_resolutions(fd, cd);
        } else {
          cd.name = "(invalid device)";
        }
      } else {
        cd.name = "Video Device";
      }
      ::close(fd);
    } else {
      cd.name = "(unavailable)";
    }
    
    cams.push_back(std::move(cd));
  }
  
  // Stable sort by index
  std::sort(cams.begin(), cams.end(), 
    [](const CameraDesc& a, const CameraDesc& b){ return a.index < b.index; });
  
  return deduplicate_cameras_by_bus(cams);
}

static void add_discrete_fps(const v4l2_frmivalenum& fie, std::vector<int>& out) {
  if (fie.discrete.numerator > 0) {
    int fps = (int)std::round((double)fie.discrete.denominator / (double)fie.discrete.numerator);
    if (fps > 0 && fps <= 240 && std::find(out.begin(), out.end(), fps) == out.end()) {
      out.push_back(fps);
    }
  }
}

static void add_common_fps(std::vector<int>& out) {
  int common[] = {15, 24, 25, 30, 50, 60, 90, 120};
  for (int fps : common) {
    if (std::find(out.begin(), out.end(), fps) == out.end()) {
      out.push_back(fps);
    }
  }
}

static std::vector<int> enum_fps_fd(int fd, int w, int h, uint32_t pixfmt) {
  std::vector<int> out;
  v4l2_frmivalenum fie{}; fie.pixel_format = pixfmt; fie.width = w; fie.height = h;
  for (fie.index=0; ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fie)==0; ++fie.index) {
    if (fie.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
      add_discrete_fps(fie, out);
    } else {
      add_common_fps(out);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<int> EnumerateFPS(const std::string& cam_path, int width, int height) {
  std::vector<int> fps;
  int fd = ::open(cam_path.c_str(), O_RDWR | O_NONBLOCK);
  if (fd < 0) return fps;
  std::set<int> uniq;
  for (uint32_t fmt : {V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_NV12}) {
    auto v = enum_fps_fd(fd, width, height, fmt);
    for (int f : v) uniq.insert(f);
  }
  ::close(fd);
  fps.assign(uniq.begin(), uniq.end());
  std::sort(fps.begin(), fps.end());
  return fps;
}

static bool query_ctrl_fd(int fd, uint32_t id, CtrlRange* out) {
  if (!out) return false; *out = CtrlRange();
  v4l2_queryctrl qc{}; qc.id = id;
  if (ioctl(fd, VIDIOC_QUERYCTRL, &qc) != 0 || (qc.flags & V4L2_CTRL_FLAG_DISABLED)) return false;
  out->min = qc.minimum; out->max = qc.maximum; out->step = std::max<int32_t>(1, qc.step); out->def = qc.default_value; out->available = true;
  v4l2_control c{}; c.id = id; if (ioctl(fd, VIDIOC_G_CTRL, &c)==0) out->val = c.value; else out->val = out->def;
  return true;
}

bool QueryCtrl(const std::string& cam_path, uint32_t id, CtrlRange* out) {
  int fd = ::open(cam_path.c_str(), O_RDWR | O_NONBLOCK);
  if (fd < 0) return false;
  bool ok = query_ctrl_fd(fd, id, out);
  ::close(fd);
  return ok;
}

bool GetCtrl(const std::string& cam_path, uint32_t id, int32_t* value) {
  int fd = ::open(cam_path.c_str(), O_RDWR | O_NONBLOCK);
  if (fd < 0) return false;
  v4l2_control c{}; c.id = id; bool ok = (ioctl(fd, VIDIOC_G_CTRL, &c)==0); if (ok && value) *value = c.value; ::close(fd); return ok;
}

bool SetCtrl(const std::string& cam_path, uint32_t id, int32_t value) {
  int fd = ::open(cam_path.c_str(), O_RDWR | O_NONBLOCK);
  if (fd < 0) return false;
  v4l2_control c{}; c.id = id; c.value = value; bool ok = (ioctl(fd, VIDIOC_S_CTRL, &c)==0);
  ::close(fd);
  return ok;
}

static bool is_video_device(const std::string& path) {
  return path.find("/dev/video") != std::string::npos;
}

static bool is_output_device(int fd) {
  v4l2_capability cap{};
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) != 0) return false;
  uint32_t caps = (cap.device_caps != 0) ? cap.device_caps : cap.capabilities;
  return (caps & V4L2_CAP_VIDEO_OUTPUT) || (caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE);
}

static LoopbackDesc create_loopback_desc(const std::string& path, int fd) {
  LoopbackDesc d;
  d.path = path;
  d.index = parse_index(path);
  
  v4l2_capability cap{};
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
    d.name = reinterpret_cast<const char*>(cap.card);
  }
  return d;
}

std::vector<LoopbackDesc> EnumerateLoopbackDevices() {
  std::vector<LoopbackDesc> out;
  std::error_code ec;
  
  for (const auto& entry : fs::directory_iterator("/dev", ec)) {
    if (ec) break;
    
    const auto p = entry.path();
    if (!fs::is_character_file(p, ec)) continue;
    
    const std::string sp = p.string();
    if (!is_video_device(sp)) continue;
    
    int fd = ::open(sp.c_str(), O_RDWR | O_NONBLOCK);
    if (fd >= 0) {
      if (is_output_device(fd)) {
        out.push_back(create_loopback_desc(sp, fd));
      }
      ::close(fd);
    }
  }
  
  std::sort(out.begin(), out.end(), [](const LoopbackDesc& a, const LoopbackDesc& b){ return a.index < b.index; });
  return out;
}
