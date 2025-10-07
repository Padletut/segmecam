// Copyright 2024 SegmeCam
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SEGMECAM_COMPOSITING_UTILS_H_
#define SEGMECAM_COMPOSITING_UTILS_H_

#include <opencv2/opencv.hpp>

namespace segmecam {

/**
 * @brief Core compositing operation that blends foreground and background using a mask
 *
 * This function implements the common compositing pattern used across multiple
 * background replacement and effects functions. It handles the conversion to float,
 * channel splitting, element-wise blending, and final conversion back to uint8.
 *
 * @param frame_bgr Input/output BGR frame (modified in-place)
 * @param background_bgr Background image in BGR format
 * @param mask_f32 Float mask [0,1] where 1 = foreground, 0 = background
 * @param convert_to_rgb If true, convert final result from BGR to RGB
 */
void CompositeWithMask(cv::Mat& frame_bgr, const cv::Mat& background_bgr,
                      const cv::Mat& mask_f32, bool convert_to_rgb = true);

/**
 * @brief Composite frame with solid color background
 *
 * @param frame_bgr Input/output BGR frame (modified in-place)
 * @param background_color Solid background color (BGR)
 * @param mask_f32 Float mask [0,1] where 1 = foreground, 0 = background
 * @param convert_to_rgb If true, convert final result from BGR to RGB
 */
void CompositeWithSolidColor(cv::Mat& frame_bgr, const cv::Scalar& background_color,
                            const cv::Mat& mask_f32, bool convert_to_rgb = true);

/**
 * @brief Composite frame with blurred background
 *
 * @param frame_bgr Input/output BGR frame (modified in-place)
 * @param background_bgr Background image in BGR format
 * @param mask_f32 Float mask [0,1] where 1 = foreground, 0 = background
 * @param blur_kernel_size Size of Gaussian blur kernel (must be odd, > 0)
 * @param convert_to_rgb If true, convert final result from BGR to RGB
 */
void CompositeWithBlurredBackground(cv::Mat& frame_bgr, const cv::Mat& background_bgr,
                                   const cv::Mat& mask_f32, int blur_kernel_size,
                                   bool convert_to_rgb = true);

}  // namespace segmecam

#endif  // SEGMECAM_COMPOSITING_UTILS_H_