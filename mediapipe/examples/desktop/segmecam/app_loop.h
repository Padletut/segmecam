#pragma once

#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include <memory>
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "app_state.h"
#include "ui_manager.h"
#include "camera_manager.h"
#include "mediapipe_manager.h"

namespace mp = mediapipe;

namespace segmecam {

class AppLoop {
public:
  AppLoop(AppState& state, UIManager& ui, CameraManager& camera, MediaPipeManager& mediapipe);
  ~AppLoop() = default;

  // Main application loop
  void Run();

private:
  // Frame processing
  void ProcessFrame();
  
  // Event handling
  void HandleEvents();
  
  // MediaPipe processing
  void SendFrameToGraph(const cv::Mat& frame_bgr);
  void PollMaskData();
  void PollLandmarkData();
  
  // Face effects processing
  void ApplyFaceEffects(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks);
  
  // Background compositing
  void CompositeBackground(cv::Mat& display_rgb, const cv::Mat& frame_bgr, const cv::Mat& mask);
  
  // UI rendering
  void RenderUI();
  void RenderMainControls();
  void RenderCameraControls();
  void RenderEffectsControls();
  void RenderProfileControls();
  
  // Utility functions
  void UpdateFPS();
  void LogPerformance();
  cv::Mat MatToImageFrame(const cv::Mat& src_bgr, std::unique_ptr<mp::ImageFrame>& out);
  mediapipe::NormalizedLandmarkList TransformLandmarksWithRect(
    const mediapipe::NormalizedLandmarkList& in,
    const mediapipe::NormalizedRect& r,
    int W, int H, bool apply_rot);
  
  AppState& state_;
  UIManager& ui_;
  CameraManager& camera_;
  MediaPipeManager& mediapipe_;
  
  // Latest MediaPipe data
  mediapipe::NormalizedLandmarkList latest_landmarks_;
  bool have_landmarks_ = false;
  std::vector<mediapipe::NormalizedRect> latest_rects_;
};

} // namespace segmecam