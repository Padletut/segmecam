#pragma once

#include <memory>
#include <string>
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/port/status.h"

namespace mp = mediapipe;

namespace segmecam {

class MediaPipeManager {
public:
  MediaPipeManager() = default;
  ~MediaPipeManager() = default;

  // Initialize the graph from config file
  bool Initialize(const std::string& graph_path);
  
  // Start the graph processing
  bool Start();
  
  // Stop and cleanup the graph
  void Stop();
  
  // Get pollers for output streams
  mp::OutputStreamPoller* GetMaskPoller() { return &mask_poller_; }
  mp::OutputStreamPoller* GetLandmarksPoller() { return landmarks_poller_.get(); }
  mp::OutputStreamPoller* GetRectPoller() { return rect_poller_.get(); }
  
  // Send frame to the graph
  bool SendFrame(std::unique_ptr<mp::ImageFrame> frame, int64_t frame_id);
  
  // Check if landmarks are available
  bool HasLandmarks() const { return has_landmarks_; }
  
  // Cleanup
  void CloseInputStream();
  void WaitUntilDone();

private:
  mp::CalculatorGraph graph_;
  mp::OutputStreamPoller mask_poller_;
  std::unique_ptr<mp::OutputStreamPoller> landmarks_poller_;
  std::unique_ptr<mp::OutputStreamPoller> rect_poller_;
  bool has_landmarks_ = false;
  bool initialized_ = false;
  bool started_ = false;
};

} // namespace segmecam