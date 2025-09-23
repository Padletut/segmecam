#include "include/effects/skin_effects.h"
#include "include/effects/effects_utils.h"
#include <cmath>

// Parameters for bilateral filtering
struct BilateralFilterParams {
    int d;
    double sigmaColor;
    double sigmaSpace;
};

// Helper function to create skin smoothing mask
cv::Mat CreateSkinMask(const cv::Size& frame_size, const FaceRegions& fr) {
  cv::Mat mask(frame_size, CV_8UC1, cv::Scalar(0));
  std::vector<std::vector<cv::Point>> face = {fr.face_oval};
  cv::fillPoly(mask, face, cv::Scalar(220));
  if (!fr.lips_outer.empty()) {
    std::vector<std::vector<cv::Point>> lips = {fr.lips_outer};
    cv::fillPoly(mask, lips, cv::Scalar(0));
  }
  if (!fr.left_eye.empty()) {
    std::vector<std::vector<cv::Point>> e = {fr.left_eye};
    cv::fillPoly(mask, e, cv::Scalar(0));
  }
  if (!fr.right_eye.empty()) {
    std::vector<std::vector<cv::Point>> e = {fr.right_eye};
    cv::fillPoly(mask, e, cv::Scalar(0));
  }
  effects_utils::featherMask(mask, 15);
  return mask;
}

// Helper function to apply bilateral filter and blending for CPU path
void ApplyBilateralFilterCPU(cv::Mat& frame_bgr, const cv::Mat& mask,
                            const BilateralFilterParams& params) {
  cv::Mat smooth;
  cv::bilateralFilter(frame_bgr, smooth, params.d, params.sigmaColor, params.sigmaSpace);

  // Blend only where mask applies (do math in float)
  cv::Mat mask_f;
  mask.convertTo(mask_f, CV_32FC1, 1.0/255.0);
  std::vector<cv::Mat> fch(3), sch(3), of(3);
  cv::split(frame_bgr, fch);
  cv::split(smooth, sch);
  for (int i = 0; i < 3; ++i) {
    cv::Mat f32, s32;
    fch[i].convertTo(f32, CV_32FC1, 1.0/255.0);
    sch[i].convertTo(s32, CV_32FC1, 1.0/255.0);
    of[i] = f32.mul(1.0f - mask_f) + s32.mul(mask_f);
  }
  cv::Mat comp_f;
  cv::merge(of, comp_f);
  comp_f.convertTo(frame_bgr, CV_8UC3, 255.0);
}

// Helper function to apply bilateral filter and blending for OpenCL path
void ApplyBilateralFilterOCL(cv::Mat& frame_bgr, const cv::Mat& mask,
                            const BilateralFilterParams& params) {
  // OpenCL path using Transparent API (UMat).
  cv::UMat src;
  frame_bgr.copyTo(src);
  cv::UMat smooth_u;
  cv::bilateralFilter(src, smooth_u, params.d, params.sigmaColor, params.sigmaSpace);
  cv::UMat mask_u;
  mask.copyTo(mask_u);
  cv::UMat mask_f;
  mask_u.convertTo(mask_f, CV_32FC1, 1.0/255.0);
  std::vector<cv::UMat> fch(3), sch(3), of(3);
  cv::split(src, fch);
  cv::split(smooth_u, sch);
  // Precompute (1 - mask)
  cv::UMat one(mask_f.size(), mask_f.type());
  one.setTo(1.0f);
  cv::UMat inv_mask;
  cv::subtract(one, mask_f, inv_mask);
  for (int i = 0; i < 3; ++i) {
    cv::UMat f32, s32;
    fch[i].convertTo(f32, CV_32FC1, 1.0/255.0);
    sch[i].convertTo(s32, CV_32FC1, 1.0/255.0);
    // out = f*(1-mask) + s*mask (all UMat)
    cv::UMat a, b;
    cv::multiply(f32, inv_mask, a);
    cv::multiply(s32, mask_f, b);
    cv::add(a, b, of[i]);
  }
  cv::UMat comp_f;
  cv::merge(of, comp_f);
  cv::UMat comp_u8;
  comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
  comp_u8.copyTo(frame_bgr);
}

void ApplySkinSmoothingBGR(cv::Mat& frame_bgr,
                           const FaceRegions& fr,
                           float strength,
                           bool use_ocl) {
  strength = std::clamp(strength, 0.0f, 1.0f);
  if (strength <= 0.0f || fr.face_oval.empty()) return;

  // Create skin smoothing mask
  cv::Mat mask = CreateSkinMask(frame_bgr.size(), fr);

  // Bilateral filter strength mapping
  BilateralFilterParams params = {
    9,  // d
    25.0 + 75.0 * strength,  // sigmaColor
    9.0 + 21.0 * strength    // sigmaSpace
  };

  // Apply bilateral filtering based on OpenCL availability
  if (!use_ocl) {
    ApplyBilateralFilterCPU(frame_bgr, mask, params);
  } else {
    ApplyBilateralFilterOCL(frame_bgr, mask, params);
  }
}