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

#include "include/compositing_utils.h"

#include <algorithm>
#include <vector>

namespace segmecam {

// Helper functions for CompositeWithMask refactoring
static cv::Mat prepare_mask(const cv::Mat& mask_f32);
static void resize_inputs(const cv::Size& target_size, const cv::Mat& background_bgr,
                         const cv::Mat& mask_single, cv::Mat& bg_resized, cv::Mat& mask_resized);
static void prepare_for_blending(const cv::Mat& frame_bgr, const cv::Mat& bg_resized,
                                cv::Mat& frame_f, cv::Mat& bg_f);
static void broadcast_mask_channels(const cv::Mat& mask_clamped, const cv::Mat& mask_inv,
                                   const cv::Mat& frame_f, cv::Mat& mask_3ch, cv::Mat& mask_inv_3ch);
static cv::Mat perform_blending(const cv::Mat& frame_f, const cv::Mat& bg_f,
                               const cv::Mat& mask_3ch, const cv::Mat& mask_inv_3ch);
static void finalize_result(cv::Mat& frame_bgr, const cv::Mat& result_f, bool convert_to_rgb);

void CompositeWithMask(cv::Mat& frame_bgr, const cv::Mat& background_bgr,
                      const cv::Mat& mask_f32, bool convert_to_rgb) {
    if (frame_bgr.empty() || background_bgr.empty() || mask_f32.empty()) {
        return;
    }

    // Prepare mask (extract single channel and clamp to [0,1])
    cv::Mat mask_prepared = prepare_mask(mask_f32);

    // Resize inputs to match frame size
    cv::Size target_size = frame_bgr.size();
    cv::Mat bg_resized, mask_resized;
    resize_inputs(target_size, background_bgr, mask_prepared, bg_resized, mask_resized);

    // Convert to float for blending
    cv::Mat frame_f, bg_f;
    prepare_for_blending(frame_bgr, bg_resized, frame_f, bg_f);

    // Create inverted mask for background
    cv::Mat mask_inv;
    cv::subtract(1.0f, mask_resized, mask_inv);

    // Broadcast mask channels for proper multiplication with BGR images
    cv::Mat mask_3ch, mask_inv_3ch;
    broadcast_mask_channels(mask_resized, mask_inv, frame_f, mask_3ch, mask_inv_3ch);

    // Perform blending: result = frame * mask + background * (1 - mask)
    cv::Mat result_f;
    try {
        result_f = perform_blending(frame_f, bg_f, mask_3ch, mask_inv_3ch);
    } catch (const cv::Exception& e) {
        std::cout << "❌ OpenCV Exception in CompositeWithMask: " << e.what() << std::endl;
        return;
    }

    // Convert back to uint8 and handle color space conversion
    finalize_result(frame_bgr, result_f, convert_to_rgb);
}

// Helper function to prepare mask (extract single channel and clamp to [0,1])
static cv::Mat prepare_mask(const cv::Mat& mask_f32) {
    // Ensure mask is single-channel
    cv::Mat mask_single;
    if (mask_f32.channels() > 1) {
        std::vector<cv::Mat> channels;
        cv::split(mask_f32, channels);
        mask_single = channels[0].clone();
    } else {
        mask_single = mask_f32;
    }

    // Ensure mask is in [0,1] range
    cv::Mat mask_clamped;
    cv::max(mask_single, 0.0f, mask_clamped);
    cv::min(mask_clamped, 1.0f, mask_clamped);
    return mask_clamped;
}

// Helper function to resize inputs to match target size
static void resize_inputs(const cv::Size& target_size, const cv::Mat& background_bgr,
                         const cv::Mat& mask_single, cv::Mat& bg_resized, cv::Mat& mask_resized) {
    if (background_bgr.size() != target_size) {
        cv::resize(background_bgr, bg_resized, target_size, 0, 0, cv::INTER_LINEAR);
    } else {
        bg_resized = background_bgr;
    }

    if (mask_single.size() != target_size) {
        cv::resize(mask_single, mask_resized, target_size, 0, 0, cv::INTER_LINEAR);
    } else {
        mask_resized = mask_single;
    }
}

// Helper function to convert inputs to float for blending
static void prepare_for_blending(const cv::Mat& frame_bgr, const cv::Mat& bg_resized,
                                cv::Mat& frame_f, cv::Mat& bg_f) {
    frame_bgr.convertTo(frame_f, CV_32F, 1.0/255.0);
    bg_resized.convertTo(bg_f, CV_32F, 1.0/255.0);
}

// Helper function to broadcast mask channels for proper multiplication with BGR images
static void broadcast_mask_channels(const cv::Mat& mask_clamped, const cv::Mat& mask_inv,
                                   const cv::Mat& frame_f, cv::Mat& mask_3ch, cv::Mat& mask_inv_3ch) {
    if (mask_clamped.channels() == 1 && frame_f.channels() == 3) {
        std::vector<cv::Mat> mask_channels(3, mask_clamped);
        cv::merge(mask_channels, mask_3ch);
        std::vector<cv::Mat> mask_inv_channels(3, mask_inv);
        cv::merge(mask_inv_channels, mask_inv_3ch);
    } else {
        mask_3ch = mask_clamped;
        mask_inv_3ch = mask_inv;
    }
}

// Helper function to perform the blending operation
static cv::Mat perform_blending(const cv::Mat& frame_f, const cv::Mat& bg_f,
                               const cv::Mat& mask_3ch, const cv::Mat& mask_inv_3ch) {
    cv::Mat frame_masked, bg_masked, result_f;
    cv::multiply(frame_f, mask_3ch, frame_masked);
    cv::multiply(bg_f, mask_inv_3ch, bg_masked);
    cv::add(frame_masked, bg_masked, result_f);
    return result_f;
}

// Helper function to finalize result (convert back to uint8 and handle color space)
static void finalize_result(cv::Mat& frame_bgr, const cv::Mat& result_f, bool convert_to_rgb) {
    // Convert back to uint8
    result_f.convertTo(frame_bgr, CV_8U, 255.0);

    // Convert BGR to RGB if requested
    if (convert_to_rgb) {
        cv::cvtColor(frame_bgr, frame_bgr, cv::COLOR_BGR2RGB);
    }
}

void CompositeWithSolidColor(cv::Mat& frame_bgr, const cv::Scalar& background_color,
                            const cv::Mat& mask_f32, bool convert_to_rgb) {
    if (frame_bgr.empty() || mask_f32.empty()) {
        return;
    }

    cv::Size target_size = frame_bgr.size();
    cv::Mat mask_resized;
    if (mask_f32.size() != target_size) {
        cv::resize(mask_f32, mask_resized, target_size, 0, 0, cv::INTER_LINEAR);
    } else {
        mask_resized = mask_f32;
    }

    // Convert to float for blending
    cv::Mat frame_f;
    frame_bgr.convertTo(frame_f, CV_32F, 1.0/255.0);

    // Create solid color background
    cv::Mat bg_f(target_size, CV_32FC3, background_color);
    bg_f *= 1.0f / 255.0f;  // Normalize to [0,1]

    // Create inverted mask for background
    cv::Mat mask_inv = 1.0f - mask_resized;

    // Broadcast masks to 3 channels for proper arithmetic with BGR images
    cv::Mat mask_3ch, mask_inv_3ch;
    if (mask_resized.channels() == 1 && frame_f.channels() == 3) {
        std::vector<cv::Mat> mask_channels(3, mask_resized);
        cv::merge(mask_channels, mask_3ch);
        std::vector<cv::Mat> mask_inv_channels(3, mask_inv);
        cv::merge(mask_inv_channels, mask_inv_3ch);
    } else {
        mask_3ch = mask_resized;
        mask_inv_3ch = mask_inv;
    }

    // Blend: result = frame * mask + background * (1 - mask)
    cv::Mat result_f;
    cv::Mat frame_masked, bg_masked;
    cv::multiply(frame_f, mask_3ch, frame_masked);
    cv::multiply(bg_f, mask_inv_3ch, bg_masked);
    cv::add(frame_masked, bg_masked, result_f);

    // Convert back to uint8
    result_f.convertTo(frame_bgr, CV_8U, 255.0);

    // Convert BGR to RGB if requested
    if (convert_to_rgb) {
        cv::cvtColor(frame_bgr, frame_bgr, cv::COLOR_BGR2RGB);
    }
}

void CompositeWithBlurredBackground(cv::Mat& frame_bgr, const cv::Mat& background_bgr,
                                   const cv::Mat& mask_f32, int blur_kernel_size,
                                   bool convert_to_rgb) {
    if (frame_bgr.empty() || background_bgr.empty() || mask_f32.empty()) {
        return;
    }

    // Ensure blur kernel size is valid
    blur_kernel_size = std::max(1, blur_kernel_size);
    if (blur_kernel_size % 2 == 0) {
        blur_kernel_size++;  // Must be odd
    }

    cv::Size target_size = frame_bgr.size();
    cv::Mat bg_resized, mask_resized;
    if (background_bgr.size() != target_size) {
        cv::resize(background_bgr, bg_resized, target_size, 0, 0, cv::INTER_LINEAR);
    } else {
        bg_resized = background_bgr;
    }

    if (mask_f32.size() != target_size) {
        cv::resize(mask_f32, mask_resized, target_size, 0, 0, cv::INTER_LINEAR);
    } else {
        mask_resized = mask_f32;
    }

    // Apply blur to background
    cv::Mat bg_blurred;
    cv::GaussianBlur(bg_resized, bg_blurred, cv::Size(blur_kernel_size, blur_kernel_size), 0);

    // Use the standard compositing function with blurred background
    CompositeWithMask(frame_bgr, bg_blurred, mask_resized, convert_to_rgb);
}

}  // namespace segmecam