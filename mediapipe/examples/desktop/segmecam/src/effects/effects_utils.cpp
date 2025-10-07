#include "include/effects/effects_utils.h"
#include <cmath>

namespace effects_utils {

void featherMask(cv::Mat& mask, int ksize) {
  if (ksize <= 1) return;
  int k = (ksize | 1);
  cv::GaussianBlur(mask, mask, cv::Size(k, k), 0);
}

} // namespace effects_utils