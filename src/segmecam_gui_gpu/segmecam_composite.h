#pragma once

#include <opencv2/opencv.hpp>
#include "mediapipe/framework/formats/image_frame.h"

// Decode a MediaPipe mask (ImageFrame) into 8‑bit single‑channel mask in 0..255.
// Logs channel selection for 4xU8 (e.g., SRGBA) once when log_once is provided.
cv::Mat DecodeMaskToU8(const mediapipe::ImageFrame& mask,
                       bool* logged_once = nullptr);

// Resize mask to match a given frame size (linear).
cv::Mat ResizeMaskToFrame(const cv::Mat& mask_u8, const cv::Size& frame_size);

// Visualize mask as RGB image for UI.
cv::Mat VisualizeMaskRGB(const cv::Mat& mask_u8);

// Background blur compositing without foreground bleed (normalized masked blur).
// Returns RGB (8UC3).
cv::Mat CompositeBlurBackgroundBGR(const cv::Mat& frame_bgr,
                                   const cv::Mat& mask_u8,
                                   int blur_strength,
                                   float feather_px);

// Optional accelerated/background-scaled path. If use_ocl=true and OpenCV has OpenCL,
// uses UMat for heavy ops. If scale < 1.0, computes blurred background at reduced res
// and upsamples for compositing (keeps normalized blend to avoid halos).
cv::Mat CompositeBlurBackgroundBGR_Accel(const cv::Mat& frame_bgr,
                                         const cv::Mat& mask_u8,
                                         int blur_strength,
                                         float feather_px,
                                         bool use_ocl,
                                         float scale);

// Image background composite. bg_bgr must be same size or will be resized.
cv::Mat CompositeImageBackgroundBGR(const cv::Mat& frame_bgr,
                                    const cv::Mat& mask_u8,
                                    const cv::Mat& bg_bgr);

// Solid color background composite.
cv::Mat CompositeSolidBackgroundBGR(const cv::Mat& frame_bgr,
                                    const cv::Mat& mask_u8,
                                    const cv::Scalar& bgr);
