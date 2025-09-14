#include "app_loop.h"
#include <iostream>
#include <chrono>
#include <SDL.h>
#include "imgui.h"
#include "segmecam_composite.h"
#include "segmecam_face_effects.h"

namespace segmecam {

static void MatToImageFrame(const cv::Mat& src_bgr, std::unique_ptr<mp::ImageFrame>& out) {
  cv::Mat rgb;
  cv::cvtColor(src_bgr, rgb, cv::COLOR_BGR2RGB);
  out.reset(new mp::ImageFrame(mp::ImageFormat::SRGB, rgb.cols, rgb.rows, mp::ImageFrame::kDefaultAlignmentBoundary));
  rgb.copyTo(cv::Mat(rgb.rows, rgb.cols, CV_8UC3, out->MutablePixelData(), out->WidthStep()));
}

AppLoop::AppLoop(AppState& state, UIManager& ui, CameraManager& camera, MediaPipeManager& mediapipe)
  : state_(state), ui_(ui), camera_(camera), mediapipe_(mediapipe) {
}

void AppLoop::Run() {
  while (state_.running) {
    HandleEvents();
    ProcessFrame();
    RenderUI();
    ui_.EndFrame();
  }
}

void AppLoop::HandleEvents() {
  bool background_dropped = ui_.ProcessEvents(state_.running);
  
  // Handle background image drop (simplified)
  if (background_dropped) {
    // Note: In a full implementation, you'd handle the dropped file path
    // For now, just log that a drop occurred
    std::cout << "Background image dropped (handling not implemented in simplified version)" << std::endl;
  }
}

void AppLoop::ProcessFrame() {
  // Grab frame from camera
  cv::Mat frame_bgr;
  if (!camera_.GetCapture().read(frame_bgr) || frame_bgr.empty()) {
    // Try fallback backend once
    static bool retried = false;
    if (!retried) { 
      camera_.GetCapture().release(); 
      camera_.GetCapture().open(0); // Simplified - use camera 0
      retried = true; 
      return; 
    }
    state_.running = false;
    return;
  }
  
  if (!state_.first_frame_log) { 
    std::cout << "Captured frame: " << frame_bgr.cols << "x" << frame_bgr.rows << std::endl; 
    state_.first_frame_log = true; 
  }
  
  UpdateFPS();
  SendFrameToGraph(frame_bgr);
  PollMaskData();
  PollLandmarkData();
  
  // Apply face effects if we have landmarks
  if (have_landmarks_) {
    ApplyFaceEffects(frame_bgr, latest_landmarks_);
  }
  
  // Compose display frame
  cv::Mat display_rgb;
  CompositeBackground(display_rgb, frame_bgr, state_.last_mask_u8);
  
  // Upload to GPU texture
  if (!display_rgb.empty()) {
    ui_.UploadTexture(display_rgb);
    state_.last_display_rgb = display_rgb;
  }
}

void AppLoop::SendFrameToGraph(const cv::Mat& frame_bgr) {
  std::unique_ptr<mp::ImageFrame> frame;
  MatToImageFrame(frame_bgr, frame);
  mediapipe_.SendFrame(std::move(frame), state_.frame_id++);
}

void AppLoop::PollMaskData() {
  mp::Packet pkt;
  int drain = 0;
  auto* mask_poller = mediapipe_.GetMaskPoller();
  
  while (mask_poller && mask_poller->QueueSize() > 0 && mask_poller->Next(&pkt)) {
    const auto& mask = pkt.Get<mp::ImageFrame>();
    state_.last_mask_u8 = DecodeMaskToU8(mask, &state_.first_mask_info);
    drain++;
    if (!state_.first_mask_log) { 
      std::cout << "Received first mask packet" << std::endl; 
      state_.first_mask_log = true; 
    }
  }
}

void AppLoop::PollLandmarkData() {
  have_landmarks_ = false;
  if (!mediapipe_.HasLandmarks()) return;
  
  auto* lm_poller = mediapipe_.GetLandmarksPoller();
  auto* rect_poller = mediapipe_.GetRectPoller();
  
  if (lm_poller) {
    mp::Packet lp;
    while (lm_poller->QueueSize() > 0 && lm_poller->Next(&lp)) {
      const auto& v = lp.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
      if (!v.empty()) { 
        latest_landmarks_ = v[0]; 
        have_landmarks_ = true; 
      }
    }
  }
  
  if (rect_poller) {
    mp::Packet rp;
    while (rect_poller->QueueSize() > 0 && rect_poller->Next(&rp)) {
      latest_rects_ = rp.Get<std::vector<mediapipe::NormalizedRect>>();
    }
  }
}

void AppLoop::ApplyFaceEffects(cv::Mat& frame_bgr, const mediapipe::NormalizedLandmarkList& landmarks) {
  mediapipe::NormalizedLandmarkList used_lms = landmarks;
  if (state_.lm_roi_mode && !latest_rects_.empty()) {
    used_lms = TransformLandmarksWithRect(landmarks, latest_rects_[0], 
                                         frame_bgr.cols, frame_bgr.rows, state_.lm_apply_rot);
  }
  
  FaceRegions fr; 
  if (ExtractFaceRegions(used_lms, frame_bgr.size(), &fr, 
                        state_.lm_flip_x, state_.lm_flip_y, state_.lm_swap_xy)) {
    // Apply effects in order
    if (state_.fx_teeth) {
      ApplyTeethWhitenBGR(frame_bgr, fr, state_.fx_teeth_strength, state_.fx_teeth_margin);
    }
    if (state_.fx_skin && state_.fx_skin_adv) {
      // Simplified skin smoothing - full implementation would be more complex
      ApplySkinSmoothingAdvancedBGR(frame_bgr, fr, state_.fx_skin_amount, state_.fx_skin_radius, 
                                   state_.fx_skin_tex, state_.fx_skin_edge, state_.fx_adv_scale, 
                                   state_.fx_adv_detail_preserve, state_.use_opencl);
    }
    if (state_.fx_lipstick) {
      ApplyLipstickBGR(frame_bgr, fr, state_.fx_lip_alpha, state_.fx_lip_color, 
                      state_.fx_lip_feather, state_.fx_lip_light, state_.fx_lip_band);
    }
  }
}

void AppLoop::CompositeBackground(cv::Mat& display_rgb, const cv::Mat& frame_bgr, const cv::Mat& mask) {
  // Simplified background compositing
  cv::Mat mask_resized;
  if (!mask.empty() && (mask.cols != frame_bgr.cols || mask.rows != frame_bgr.rows)) {
    cv::resize(mask, mask_resized, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  } else if (!mask.empty()) {
    mask_resized = mask;
  }
  
  if (state_.bg_mode == 0 || mask_resized.empty()) {
    // No background replacement
    cv::cvtColor(frame_bgr, display_rgb, cv::COLOR_BGR2RGB);
  } else {
    // Apply background effect
    cv::Mat background;
    switch (state_.bg_mode) {
      case 1: // Blur
        cv::GaussianBlur(frame_bgr, background, cv::Size(state_.blur_strength*2+1, state_.blur_strength*2+1), 0);
        break;
      case 2: // Image
        if (!state_.bg_image.empty()) {
          cv::resize(state_.bg_image, background, frame_bgr.size());
        } else {
          background = frame_bgr.clone();
        }
        break;
      case 3: // Solid color
        background = cv::Mat(frame_bgr.size(), CV_8UC3, 
                           cv::Scalar(state_.solid_color[2]*255, state_.solid_color[1]*255, state_.solid_color[0]*255));
        break;
      default:
        background = frame_bgr.clone();
        break;
    }
    
    // Composite using mask
    cv::Mat result_bgr;
    CompositeWithMask(frame_bgr, background, mask_resized, result_bgr, state_.feather_px);
    cv::cvtColor(result_bgr, display_rgb, cv::COLOR_BGR2RGB);
  }
}

void AppLoop::RenderUI() {
  ui_.BeginFrame();
  
  // Main window
  ImGui::SetNextWindowPos(ImVec2(16,16), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(400,300), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse)) {
    RenderMainControls();
    RenderCameraControls();
    RenderEffectsControls();
    RenderProfileControls();
  }
  ImGui::End();
  
  // Show camera feed
  if (ui_.GetTexture() && ui_.GetTextureWidth() > 0 && ui_.GetTextureHeight() > 0) {
    ImGui::SetNextWindowPos(ImVec2(450,16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640,480), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Camera Feed", nullptr, ImGuiWindowFlags_NoCollapse)) {
      ImVec2 size = ImGui::GetContentRegionAvail();
      float aspect = (float)ui_.GetTextureWidth() / ui_.GetTextureHeight();
      float display_w = size.x;
      float display_h = display_w / aspect;
      if (display_h > size.y) {
        display_h = size.y;
        display_w = display_h * aspect;
      }
      ImGui::Image((void*)(intptr_t)ui_.GetTexture(), ImVec2(display_w, display_h));
    }
    ImGui::End();
  }
}

void AppLoop::RenderMainControls() {
  ImGui::Text("FPS: %.1f", state_.fps);
  ImGui::Checkbox("Show Mask", &state_.show_mask);
  ImGui::Checkbox("VSync", &state_.vsync_on);
  if (ImGui::IsItemEdited()) {
    ui_.SetVSync(state_.vsync_on);
  }
}

void AppLoop::RenderCameraControls() {
  if (ImGui::CollapsingHeader("Camera")) {
    const auto& cam_list = camera_.GetCameraList();
    if (!cam_list.empty()) {
      int current_cam = camera_.GetCurrentCamIndex();
      if (ImGui::Combo("Camera", &current_cam, [](void* data, int idx, const char** out_text) {
        auto* cams = (const std::vector<CameraDesc>*)data;
        if (idx >= 0 && idx < (int)cams->size()) {
          *out_text = (*cams)[idx].name.c_str();
          return true;
        }
        return false;
      }, (void*)&cam_list, (int)cam_list.size())) {
        // Handle camera change (simplified)
        std::cout << "Camera changed to index " << current_cam << std::endl;
      }
    }
  }
}

void AppLoop::RenderEffectsControls() {
  if (ImGui::CollapsingHeader("Effects")) {
    ImGui::Text("Background");
    ImGui::RadioButton("None", &state_.bg_mode, 0); ImGui::SameLine();
    ImGui::RadioButton("Blur", &state_.bg_mode, 1); ImGui::SameLine();
    ImGui::RadioButton("Image", &state_.bg_mode, 2); ImGui::SameLine();
    ImGui::RadioButton("Color", &state_.bg_mode, 3);
    
    if (state_.bg_mode == 1) {
      ImGui::SliderInt("Blur Strength", &state_.blur_strength, 1, 50);
    }
    if (state_.bg_mode == 3) {
      ImGui::ColorEdit3("Background Color", state_.solid_color);
    }
    
    ImGui::SliderFloat("Feather", &state_.feather_px, 0.0f, 10.0f);
    
    ImGui::Separator();
    ImGui::Text("Face Effects");
    ImGui::Checkbox("Skin Smoothing", &state_.fx_skin);
    if (state_.fx_skin) {
      ImGui::SliderFloat("Strength", &state_.fx_skin_strength, 0.0f, 1.0f);
    }
    ImGui::Checkbox("Teeth Whitening", &state_.fx_teeth);
    if (state_.fx_teeth) {
      ImGui::SliderFloat("Teeth Strength", &state_.fx_teeth_strength, 0.0f, 1.0f);
    }
    ImGui::Checkbox("Lipstick", &state_.fx_lipstick);
    if (state_.fx_lipstick) {
      ImGui::SliderFloat("Lip Alpha", &state_.fx_lip_alpha, 0.0f, 1.0f);
      ImGui::ColorEdit3("Lip Color", state_.fx_lip_color);
    }
  }
}

void AppLoop::RenderProfileControls() {
  if (ImGui::CollapsingHeader("Profiles")) {
    auto profiles = camera_.ListProfiles();
    if (!profiles.empty()) {
      int current_profile = state_.ui_profile_idx;
      if (ImGui::Combo("Profile", &current_profile, [](void* data, int idx, const char** out_text) {
        auto* prof = (const std::vector<std::string>*)data;
        if (idx >= 0 && idx < (int)prof->size()) {
          *out_text = (*prof)[idx].c_str();
          return true;
        }
        return false;
      }, (void*)&profiles, (int)profiles.size())) {
        state_.ui_profile_idx = current_profile;
        if (current_profile >= 0 && current_profile < (int)profiles.size()) {
          // Load profile
          camera_.LoadProfile(profiles[current_profile], [this](const cv::FileNode& root) {
            state_.LoadFromProfile(root);
          });
        }
      }
    }
    
    ImGui::InputText("Profile Name", state_.profile_name_buf, sizeof(state_.profile_name_buf));
    if (ImGui::Button("Save Profile")) {
      if (strlen(state_.profile_name_buf) > 0) {
        camera_.SaveProfile(state_.profile_name_buf, [this](cv::FileStorage& fs) {
          state_.SaveToProfile(fs);
        });
      }
    }
  }
}

void AppLoop::UpdateFPS() {
  state_.fps_frames++;
  uint32_t now_ms = SDL_GetTicks();
  if (now_ms - state_.fps_last_ms >= 500) { 
    state_.fps = (double)state_.fps_frames * 1000.0 / (double)(now_ms - state_.fps_last_ms); 
    state_.fps_frames = 0; 
    state_.fps_last_ms = now_ms; 
  }
}

mediapipe::NormalizedLandmarkList AppLoop::TransformLandmarksWithRect(
    const mediapipe::NormalizedLandmarkList& in,
    const mediapipe::NormalizedRect& r,
    int W, int H, bool apply_rot) {
  mediapipe::NormalizedLandmarkList out = in;
  const float cx = r.x_center();
  const float cy = r.y_center();
  const float rw = r.width();
  const float rh = r.height();
  const float ang = apply_rot ? r.rotation() : 0.0f;
  const float ca = std::cos(ang), sa = std::sin(ang);
  
  for (int i = 0; i < out.landmark_size(); ++i) {
    auto* p = out.mutable_landmark(i);
    float rx = p->x();
    float ry = p->y();
    float ox = (rx - 0.5f) * rw * W;
    float oy = (ry - 0.5f) * rh * H;
    float rotx = ca*ox - sa*oy;
    float roty = sa*ox + ca*oy;
    p->set_x(cx + rotx / W);
    p->set_y(cy + roty / H);
  }
  return out;
}

} // namespace segmecam