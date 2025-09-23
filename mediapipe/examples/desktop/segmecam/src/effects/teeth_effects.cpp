#include "include/effects/teeth_effects.h"
#include "include/effects/effects_utils.h"
#include <cmath>

void ApplyTeethWhitenBGR(cv::Mat& frame_bgr,
                         const FaceRegions& fr,
                         float strength,
                         float shrink_px) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  if (strength <= 0.0f || fr.lips_inner.empty()) return;
  cv::Mat mask(frame_bgr.size(), CV_8UC1, cv::Scalar(0));
  cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{fr.lips_inner}, cv::Scalar(255));
  // Erode to avoid lips bleed into whitening
  if (shrink_px > 0.5f) {
    int k = std::max(1, (int)std::round(shrink_px));
    cv::Mat ker = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    cv::erode(mask, mask, ker);
  }
  effects_utils::featherMask(mask, 5);
  // Convert to LAB and nudge b* toward blue (reduce yellow), slight L* increase.
  cv::Mat lab; cv::cvtColor(frame_bgr, lab, cv::COLOR_BGR2Lab);
  std::vector<cv::Mat> ch; cv::split(lab, ch);
  // ch[0]=L 0..255, ch[1]=a 0..255, ch[2]=b 0..255 (128 is neutral)
  // Move b toward 128 by factor, and slightly increase L.
  const float k = 0.35f * strength;
  const float kL = 0.15f * strength;
  ch[2].forEach<uchar>([&](uchar &pix, const int pos[]) {
    int y = pos[0], x = pos[1];
    if (mask.at<uchar>(y, x) == 0) return;
    float b = pix;
    b = 128.0f + (b - 128.0f) * (1.0f - k);
    pix = (uchar)std::clamp(b, 0.0f, 255.0f);
  });
  ch[0].forEach<uchar>([&](uchar &pix, const int pos[]) {
    int y = pos[0], x = pos[1];
    if (mask.at<uchar>(y, x) == 0) return;
    float L = pix * (1.0f + kL);
    pix = (uchar)std::clamp(L, 0.0f, 255.0f);
  });
  cv::merge(ch, lab);
  cv::cvtColor(lab, frame_bgr, cv::COLOR_Lab2BGR);
}