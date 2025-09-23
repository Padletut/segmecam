#pragma once

#include <string>
#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"

namespace segmecam {

// Simple v4l2loopback writer (YUYV). Linux-only.
class VCam {
public:
  VCam() = default;
  ~VCam();

  bool Open(const std::string& path, int width, int height);
  void Close();
  bool IsOpen() const { return fd_ >= 0; }
  int Width() const { return w_; }
  int Height() const { return h_; }

  // Convert BGR to YUYV and write to the device. Returns true on success.
  bool WriteBGR(const cv::Mat& bgr);

private:
  int fd_ = -1;
  int w_ = 0, h_ = 0;
};

} // namespace segmecam

