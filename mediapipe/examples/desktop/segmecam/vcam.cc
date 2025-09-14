#include "vcam.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <vector>

namespace segmecam {

VCam::~VCam() { Close(); }

bool VCam::Open(const std::string& path, int width, int height) {
  Close();
  int fd = ::open(path.c_str(), O_RDWR);
  if (fd < 0) return false;
  v4l2_format fmt{}; fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width = width; fmt.fmt.pix.height = height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; fmt.fmt.pix.field = V4L2_FIELD_NONE;
  fmt.fmt.pix.bytesperline = width * 2; fmt.fmt.pix.sizeimage = width * height * 2;
  if (ioctl(fd, VIDIOC_S_FMT, &fmt) != 0) { ::close(fd); return false; }
  fd_ = fd; w_ = width; h_ = height; return true;
}

void VCam::Close() {
  if (fd_ >= 0) { ::close(fd_); fd_ = -1; w_ = h_ = 0; }
}

static inline uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

bool VCam::WriteBGR(const cv::Mat& bgr) {
  if (fd_ < 0 || bgr.empty()) return false;
  if (bgr.cols != w_ || bgr.rows != h_) return false;
  std::vector<uint8_t> yuyv; yuyv.resize((size_t)w_ * (size_t)h_ * 2u);
  const int W = w_, H = h_;
  const uint8_t* p = bgr.data; int stride = (int)bgr.step; uint8_t* o = yuyv.data();
  for (int y=0; y<H; ++y) {
    const uint8_t* row = p + y * stride;
    for (int x=0; x<W; x+=2) {
      int b0=row[x*3+0], g0=row[x*3+1], r0=row[x*3+2];
      int b1=row[(x+1)*3+0], g1=row[(x+1)*3+1], r1=row[(x+1)*3+2];
      int Y0 = ( 66*r0 +129*g0 + 25*b0 +128)>>8; Y0 += 16;
      int Y1 = ( 66*r1 +129*g1 + 25*b1 +128)>>8; Y1 += 16;
      int U  = (-38*r0 - 74*g0 +112*b0 +128)>>8; U += 128;
      int V  = (112*r0 - 94*g0 - 18*b0 +128)>>8; V += 128;
      int U1 = (-38*r1 - 74*g1 +112*b1 +128)>>8; U1 += 128;
      int V1 = (112*r1 - 94*g1 - 18*b1 +128)>>8; V1 += 128;
      U = (U + U1) >> 1; V = (V + V1) >> 1;
      *o++ = clamp8(Y0); *o++ = clamp8(U); *o++ = clamp8(Y1); *o++ = clamp8(V);
    }
  }
  ssize_t need = (ssize_t)yuyv.size();
  ssize_t wr = ::write(fd_, yuyv.data(), need);
  return wr == need;
}

} // namespace segmecam

