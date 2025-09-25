#ifndef SEGMECAM_PIPEWIRE_FRAME_CONVERTERS_H_
#define SEGMECAM_PIPEWIRE_FRAME_CONVERTERS_H_

#include <opencv2/opencv.hpp>
#include <pipewire/pipewire.h>

namespace segmecam {

// Convert frames to different PipeWire buffer formats
bool convert_yuy2_frame(const cv::Mat& frame, pw_buffer* buffer, int process_count);
bool convert_bgr_frame(const cv::Mat& frame, pw_buffer* buffer);
bool convert_rgb_frame(const cv::Mat& frame, pw_buffer* buffer);
bool convert_bgrx_frame(const cv::Mat& frame, pw_buffer* buffer);
bool convert_rgbx_frame(const cv::Mat& frame, pw_buffer* buffer);

} // namespace segmecam

#endif // SEGMECAM_PIPEWIRE_FRAME_CONVERTERS_H_