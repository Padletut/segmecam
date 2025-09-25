#include "include/camera/vcam.h"
#include "include/camera/gstreamer_buffer_utils.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <vector>
#include <iostream>
#include <cstring>

namespace segmecam {

VCam::~VCam() { Close(); }

bool VCam::Open(const std::string& path, int width, int height) {
  Close();
  std::cout << "VCam: Attempting to open " << path << " with " << width << "x" << height << std::endl;
  int fd = ::open(path.c_str(), O_RDWR);
  if (fd < 0) {
    std::cerr << "VCam: Failed to open " << path << ": " << strerror(errno) << std::endl;
    return false;
  }
  std::cout << "VCam: Successfully opened " << path << ", fd=" << fd << std::endl;
  v4l2_format fmt{}; fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width = width; fmt.fmt.pix.height = height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; fmt.fmt.pix.field = V4L2_FIELD_NONE;
  fmt.fmt.pix.bytesperline = width * 2; fmt.fmt.pix.sizeimage = width * height * 2;
  if (ioctl(fd, VIDIOC_S_FMT, &fmt) != 0) {
    std::cerr << "VCam: Failed to set format: " << strerror(errno) << std::endl;
    ::close(fd); return false;
  }
  std::cout << "VCam: Format set successfully, fd=" << fd << ", w=" << width << ", h=" << height << std::endl;
  fd_ = fd; w_ = width; h_ = height; return true;
}

void VCam::Close() {
  if (fd_ >= 0) { ::close(fd_); fd_ = -1; w_ = h_ = 0; }
}

bool VCam::WriteBGR(const cv::Mat& bgr) {
  if (fd_ < 0 || bgr.empty()) {
    std::cerr << "VCam: WriteBGR failed - fd=" << fd_ << ", empty=" << bgr.empty() << std::endl;
    return false;
  }
  if (bgr.cols != w_ || bgr.rows != h_) {
    std::cerr << "VCam: WriteBGR failed - size mismatch: " << bgr.cols << "x" << bgr.rows << " vs " << w_ << "x" << h_ << std::endl;
    return false;
  }
  std::vector<uint8_t> yuyv; yuyv.resize((size_t)w_ * (size_t)h_ * 2u);
  BGRToYUY2(bgr, yuyv.data());
  ssize_t need = (ssize_t)yuyv.size();
  ssize_t wr = ::write(fd_, yuyv.data(), need);
  if (wr != need) {
    std::cerr << "VCam: Write failed - wrote " << wr << " of " << need << " bytes: " << strerror(errno) << std::endl;
    return false;
  }
  return true;
}

} // namespace segmecam

