#include "mediapipe_manager.h"
#include <iostream>
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"

namespace segmecam {

static mp::StatusOr<std::string> LoadTextFile(const std::string& path) {
  std::string contents;
  MP_RETURN_IF_ERROR(mp::file::GetContents(path, &contents));
  return contents;
}

bool MediaPipeManager::Initialize(const std::string& graph_path) {
  if (initialized_) return true;

  auto cfg_text_or = LoadTextFile(graph_path);
  if (!cfg_text_or.ok()) { 
    std::fprintf(stderr, "Failed to read graph: %s\n", graph_path.c_str()); 
    return false; 
  }
  
  mp::CalculatorGraphConfig config = mp::ParseTextProtoOrDie<mp::CalculatorGraphConfig>(cfg_text_or.value());

  auto st = graph_.Initialize(config); 
  if (!st.ok()) { 
    std::fprintf(stderr, "Initialize failed: %s\n", st.message().data()); 
    return false; 
  }
  
  // Provide GPU resources BEFORE StartRun
  auto or_gpu = mediapipe::GpuResources::Create();
  if (!or_gpu.ok()) { 
    std::fprintf(stderr, "GpuResources::Create failed: %s\n", or_gpu.status().message().data()); 
    return false; 
  }
  st = graph_.SetGpuResources(std::move(or_gpu.value()));
  if (!st.ok()) { 
    std::fprintf(stderr, "SetGpuResources failed: %s\n", st.message().data()); 
    return false; 
  }

  // Setup output stream pollers
  auto mask_poller_or = graph_.AddOutputStreamPoller("segmentation_mask_cpu");
  if (!mask_poller_or.ok()) { 
    std::fprintf(stderr, "Graph does not produce 'segmentation_mask_cpu'\n"); 
    return false; 
  }
  mask_poller_ = std::move(mask_poller_or.value());

  // Try to attach landmarks poller (optional)
  auto lm_or = graph_.AddOutputStreamPoller("multi_face_landmarks");
  if (!lm_or.ok()) { 
    has_landmarks_ = false; 
    std::cout << "Landmarks stream not available (graph without face mesh)" << std::endl; 
  } else { 
    landmarks_poller_ = std::make_unique<mp::OutputStreamPoller>(std::move(lm_or.value())); 
    has_landmarks_ = true;
    
    auto rp = graph_.AddOutputStreamPoller("face_rects");
    if (rp.ok()) {
      rect_poller_ = std::make_unique<mp::OutputStreamPoller>(std::move(rp.value()));
    }
  }

  initialized_ = true;
  return true;
}

bool MediaPipeManager::Start() {
  if (!initialized_) return false;
  if (started_) return true;

  auto st = graph_.StartRun({});
  if (!st.ok()) { 
    std::fprintf(stderr, "StartRun failed: %s\n", st.message().data()); 
    return false; 
  }
  
  started_ = true;
  return true;
}

void MediaPipeManager::Stop() {
  started_ = false;
}

bool MediaPipeManager::SendFrame(std::unique_ptr<mp::ImageFrame> frame, int64_t frame_id) {
  if (!started_) return false;
  
  auto ts = mp::Timestamp(frame_id);
  auto st = graph_.AddPacketToInputStream("input_video", mp::Adopt(frame.release()).At(ts));
  if (!st.ok()) { 
    std::fprintf(stderr, "AddPacket failed: %s\n", st.message().data()); 
    return false; 
  }
  return true;
}

void MediaPipeManager::CloseInputStream() {
  auto st = graph_.CloseInputStream("input_video"); 
  if (!st.ok()) {
    std::fprintf(stderr, "CloseInputStream failed: %s\n", st.message().data());
  }
}

void MediaPipeManager::WaitUntilDone() {
  auto st = graph_.WaitUntilDone(); 
  if (!st.ok()) {
    std::fprintf(stderr, "WaitUntilDone failed: %s\n", st.message().data());
  }
}

} // namespace segmecam