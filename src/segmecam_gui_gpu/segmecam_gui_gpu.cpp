// SDL2 + OpenGL + ImGui rendering of MediaPipe GPU segmentation (mask on CPU, composite on CPU)

#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <opencv2/opencv.hpp>
#include <cstdio>
#include <memory>
#include <string>
#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/declare.h"
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"
#include "segmecam_composite.h"
#include "segmecam_face_effects.h"
#include "mediapipe/tasks/cc/vision/face_landmarker/face_landmarks_connections.h"

namespace mp = mediapipe;
ABSL_DECLARE_FLAG(std::string, resource_root_dir);

static mp::StatusOr<std::string> LoadTextFile(const std::string& path) {
  std::string contents;
  MP_RETURN_IF_ERROR(mp::file::GetContents(path, &contents));
  return contents;
}

static void MatToImageFrame(const cv::Mat& src_bgr, std::unique_ptr<mp::ImageFrame>& out) {
  cv::Mat rgb;
  cv::cvtColor(src_bgr, rgb, cv::COLOR_BGR2RGB);
  out.reset(new mp::ImageFrame(mp::ImageFormat::SRGB, rgb.cols, rgb.rows, mp::ImageFrame::kDefaultAlignmentBoundary));
  rgb.copyTo(cv::Mat(rgb.rows, rgb.cols, CV_8UC3, out->MutablePixelData(), out->WidthStep()));
}

int main(int argc, char** argv) {
  std::string graph_path = "mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt";  // default: segmentation only
  std::string resource_root_dir = ".";  // MediaPipe repo root or Bazel runfiles
  int cam_index = 0;
  if (argc > 1) graph_path = argv[1];
  if (argc > 2) resource_root_dir = argv[2];
  if (argc > 3) cam_index = std::atoi(argv[3]);

  // If running under Bazel, point MediaPipe resource loader to runfiles.
  const char* rf = std::getenv("RUNFILES_DIR");
  if (rf && *rf) {
    absl::SetFlag(&FLAGS_resource_root_dir, std::string(rf));
  } else if (!resource_root_dir.empty()) {
    absl::SetFlag(&FLAGS_resource_root_dir, resource_root_dir);
  }

  auto cfg_text_or = LoadTextFile(graph_path);
  if (!cfg_text_or.ok()) { std::fprintf(stderr, "Failed to read graph: %s\n", graph_path.c_str()); return 1; }
  mp::CalculatorGraphConfig config = mp::ParseTextProtoOrDie<mp::CalculatorGraphConfig>(cfg_text_or.value());

  mp::CalculatorGraph graph;
  { auto st = graph.Initialize(config); if (!st.ok()) { std::fprintf(stderr, "Initialize failed: %s\n", st.message().data()); return 2; } }
  // Provide GPU resources BEFORE StartRun
  {
    auto or_gpu = mediapipe::GpuResources::Create();
    if (!or_gpu.ok()) { std::fprintf(stderr, "GpuResources::Create failed: %s\n", or_gpu.status().message().data()); return 2; }
    auto st = graph.SetGpuResources(std::move(or_gpu.value()));
    if (!st.ok()) { std::fprintf(stderr, "SetGpuResources failed: %s\n", st.message().data()); return 2; }
  }
  auto mask_poller_or = graph.AddOutputStreamPoller("segmentation_mask_cpu");
  if (!mask_poller_or.ok()) { std::fprintf(stderr, "Graph does not produce 'segmentation_mask_cpu'\n"); return 3; }
  mp::OutputStreamPoller mask_poller = std::move(mask_poller_or.value());
  // Try to attach landmarks poller (optional)
  bool has_landmarks = true;
  std::unique_ptr<mp::OutputStreamPoller> lm_poller;
  std::unique_ptr<mp::OutputStreamPoller> rect_poller;
  {
    auto lm_or = graph.AddOutputStreamPoller("multi_face_landmarks");
    if (!lm_or.ok()) { has_landmarks = false; std::cout << "Landmarks stream not available (graph without face mesh)" << std::endl; }
    else { lm_poller = std::make_unique<mp::OutputStreamPoller>(std::move(lm_or.value())); }
    if (has_landmarks) {
      auto rp = graph.AddOutputStreamPoller("face_rects");
      if (rp.ok()) rect_poller = std::make_unique<mp::OutputStreamPoller>(std::move(rp.value()));
    }
  }
  { auto st = graph.StartRun({}); if (!st.ok()) { std::fprintf(stderr, "StartRun failed: %s\n", st.message().data()); return 4; } }

  // Try V4L2 first, then fallback to default backend.
  cv::VideoCapture cap(cam_index, cv::CAP_V4L2);
  if (!cap.isOpened()) {
    std::cout << "V4L2 open failed for index " << cam_index << ", retrying with CAP_ANY" << std::endl;
    cap.open(cam_index);
  }
  if (!cap.isOpened()) { std::fprintf(stderr, "Unable to open camera %d\n", cam_index); return 5; }
  std::cout << "Camera backend: " << cap.getBackendName() << std::endl;

  // SDL2 + OpenGL + ImGui setup (desktop GL core profile)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { std::fprintf(stderr, "SDL error: %s\n", SDL_GetError()); return 6; }
  std::cout << "SDL initialized" << std::endl;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  // Request desktop GL 3.3 core
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_Window* window = SDL_CreateWindow("SegmeCam", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) { std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return 6; }
  std::cout << "SDL window created" << std::endl;
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (!gl_context) { std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError()); return 6; }
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);
  std::cout << "GL context ready" << std::endl;

  // (GPU resources already configured before StartRun.)

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init("#version 330");
  std::cout << "ImGui initialized" << std::endl;

  // Draw an initial frame so the window appears even before first camera frame
  ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(16,16), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(360,100), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse)) {
    ImGui::Text("Initializing camera and graph...");
  }
  ImGui::End();
  ImGui::Render();
  int idw, idh; SDL_GL_GetDrawableSize(window, &idw, &idh);
  glViewport(0,0,idw,idh); glClearColor(0.06f,0.06f,0.07f,1.0f); glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window);
  std::cout << "Initial frame drawn" << std::endl;

  GLuint tex = 0; int tex_w = 0, tex_h = 0;
  auto upload = [&](const cv::Mat& rgb){
    // Ensure GL interprets tightly packed RGB rows
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (tex == 0 || tex_w != rgb.cols || tex_h != rgb.rows) {
      if (tex) glDeleteTextures(1, &tex);
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
      tex_w = rgb.cols; tex_h = rgb.rows;
    } else {
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgb.cols, rgb.rows, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    }
  };

  bool show_mask = false; int blur_strength = 15; float feather_px = 2.0f; int64_t frame_id = 0;
  bool dbg_composite_rgb = false; // composite in RGB space to test channel order
  // Background mode: 0=None, 1=Blur, 2=Image, 3=Solid Color
  int bg_mode = 1;
  cv::Mat bg_image; // background image (BGR)
  static char bg_path_buf[512] = {0};
  float solid_color[3] = {0.0f, 0.0f, 0.0f}; // RGB 0..1
  cv::Mat last_mask_u8;  // cache latest mask to avoid blocking
  cv::Mat last_display_rgb;
  // Beauty controls
  bool fx_skin = false; float fx_skin_strength = 0.4f;
  bool fx_skin_adv = true; float fx_skin_amount = 0.5f; float fx_skin_radius = 6.0f; float fx_skin_tex = 0.35f; float fx_skin_edge = 12.0f;
  bool fx_skin_wrinkle = true; float fx_skin_smile_boost = 0.6f; float fx_skin_squint_boost = 0.5f; float fx_skin_forehead_boost = 0.8f; float fx_skin_wrinkle_gain = 1.5f;
  bool dbg_wrinkle_mask = false; // visualize wrinkle mask overlay
  bool dbg_wrinkle_stats = true; // show wrinkle stats text when visualizing
  bool fx_wrinkle_suppress_lower = true; float fx_wrinkle_lower_ratio = 0.45f;
  bool fx_wrinkle_ignore_glasses = true; float fx_wrinkle_glasses_margin = 12.0f;
  float fx_wrinkle_keep_ratio = 0.35f; // top fraction kept (higher = more sensitive)
  bool fx_wrinkle_custom_scales = true; float fx_wrinkle_min_px = 2.0f; float fx_wrinkle_max_px = 8.0f;
  bool fx_wrinkle_preview = false; // show only wrinkle attenuation (no base smoothing)
  bool fx_wrinkle_use_skin_gate = false; float fx_wrinkle_mask_gain = 2.0f; float fx_wrinkle_baseline = 0.5f;
  float fx_wrinkle_neg_cap = 0.9f; // max negative-detail attenuation
  bool fx_lipstick = false; float fx_lip_alpha = 0.5f; float fx_lip_color[3] = {0.8f, 0.1f, 0.3f}; float fx_lip_feather = 6.0f; float fx_lip_light = 0.0f; float fx_lip_band = 4.0f;
  bool fx_teeth = false; float fx_teeth_strength = 0.5f; float fx_teeth_margin = 3.0f;
  bool show_landmarks = false; // draw facial landmarks overlay
  bool show_mesh = false; bool show_mesh_dense = false; // Studio-like grid
  bool lm_roi_mode = false; bool lm_apply_rot = true;
  bool lm_flip_x = false, lm_flip_y = false, lm_swap_xy = false; // landmark coordinate tweaks
  // FPS
  double fps = 0.0; uint64_t fps_frames = 0; uint32_t fps_last_ms = SDL_GetTicks();
  uint32_t dbg_last_ms = SDL_GetTicks();

  bool first_frame_log = false, first_mask_log = false, first_mask_info = false;
  bool running = true;
  while (running) {
    SDL_Event e; while (SDL_PollEvent(&e)) { ImGui_ImplSDL2_ProcessEvent(&e); if (e.type == SDL_QUIT) running = false; }

    // Grab frame first, then build UI
    cv::Mat frame_bgr; if (!cap.read(frame_bgr) || frame_bgr.empty()) {
      // Try fallback backend once
      static bool retried = false;
      if (!retried) { cap.release(); cap.open(cam_index); retried = true; continue; }
      break;
    }
    if (!first_frame_log) { std::cout << "Captured frame: " << frame_bgr.cols << "x" << frame_bgr.rows << std::endl; first_frame_log = true; }
    // FPS update
    fps_frames++; uint32_t now_ms = SDL_GetTicks();
    if (now_ms - fps_last_ms >= 500) { fps = (double)fps_frames * 1000.0 / (double)(now_ms - fps_last_ms); fps_frames = 0; fps_last_ms = now_ms; }
    // Send to graph
    {
      std::unique_ptr<mp::ImageFrame> frame; MatToImageFrame(frame_bgr, frame);
      auto ts = mp::Timestamp(frame_id++);
      auto st = graph.AddPacketToInputStream("input_video", mp::Adopt(frame.release()).At(ts));
      if (!st.ok()) { std::fprintf(stderr, "AddPacket failed: %s\n", st.message().data()); break; }
    }

    // Poll latest mask (non-blocking): drain any queued packet
    mp::Packet pkt;
    int drain = 0;
    while (mask_poller.QueueSize() > 0 && mask_poller.Next(&pkt)) {
      const auto& mask = pkt.Get<mp::ImageFrame>();
      last_mask_u8 = DecodeMaskToU8(mask, &first_mask_info);
      drain++;
      if (!first_mask_log) { std::cout << "Received first mask packet" << std::endl; first_mask_log = true; }
    }

    // Poll latest landmarks (non-blocking)
    mediapipe::NormalizedLandmarkList latest_lms;
    bool have_lms = false;
    std::vector<mediapipe::NormalizedRect> latest_rects;
    if (has_landmarks && lm_poller) {
      mp::Packet lp;
      while (lm_poller->QueueSize() > 0 && lm_poller->Next(&lp)) {
        // Expecting vector<NormalizedLandmarkList>
        const auto& v = lp.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
        if (!v.empty()) { latest_lms = v[0]; have_lms = true; }
      }
      if (rect_poller) {
        mp::Packet rp;
        while (rect_poller->QueueSize() > 0 && rect_poller->Next(&rp)) {
          latest_rects = rp.Get<std::vector<mediapipe::NormalizedRect>>();
        }
      }
    }

    auto transform_lms_with_rect = [&](const mediapipe::NormalizedLandmarkList& in,
                                       const mediapipe::NormalizedRect& r,
                                       int W, int H, bool apply_rot) {
      mediapipe::NormalizedLandmarkList out = in;
      const float cx = r.x_center();
      const float cy = r.y_center();
      const float rw = r.width();
      const float rh = r.height();
      const float ang = apply_rot ? r.rotation() : 0.0f;
      const float ca = std::cos(ang), sa = std::sin(ang);
      for (int i=0;i<out.landmark_size();++i) {
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
    };

    // Compose display frame
    cv::Mat display_rgb;
    // Ensure mask matches frame size
    cv::Mat mask8_resized;
    if (!last_mask_u8.empty() && (last_mask_u8.cols != frame_bgr.cols || last_mask_u8.rows != frame_bgr.rows)) {
      cv::resize(last_mask_u8, mask8_resized, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
    } else if (!last_mask_u8.empty()) {
      mask8_resized = last_mask_u8;
    }

    // Apply face effects on the foreground before background compositing
    if (have_lms) {
      mediapipe::NormalizedLandmarkList used_lms = latest_lms;
      if (lm_roi_mode && !latest_rects.empty()) {
        used_lms = transform_lms_with_rect(latest_lms, latest_rects[0], frame_bgr.cols, frame_bgr.rows, lm_apply_rot);
      }
      FaceRegions fr; if (ExtractFaceRegions(used_lms, frame_bgr.size(), &fr, lm_flip_x, lm_flip_y, lm_swap_xy)) {
        // Teeth first to avoid overriding lips later
        if (fx_teeth)     ApplyTeethWhitenBGR(frame_bgr, fr, fx_teeth_strength, fx_teeth_margin);
        if (fx_skin) {
          if (fx_skin_adv) {
            const mediapipe::NormalizedLandmarkList* lmsp = (has_landmarks && have_lms && fx_skin_wrinkle) ? &used_lms : nullptr;
            float sboost = fx_skin_wrinkle ? fx_skin_smile_boost : 0.0f;
            float qboost = fx_skin_wrinkle ? fx_skin_squint_boost : 0.0f;
            ApplySkinSmoothingAdvBGR(frame_bgr, fr, fx_skin_amount, fx_skin_radius, fx_skin_tex, fx_skin_edge, lmsp, sboost, qboost, fx_skin_forehead_boost, fx_skin_wrinkle_gain,
                                     fx_wrinkle_suppress_lower, fx_wrinkle_lower_ratio, fx_wrinkle_ignore_glasses, fx_wrinkle_glasses_margin,
                                     fx_wrinkle_keep_ratio,
                                     fx_wrinkle_custom_scales ? fx_wrinkle_min_px : -1.0f,
                                     fx_wrinkle_custom_scales ? fx_wrinkle_max_px : -1.0f,
                                     10.0f,
                                     fx_wrinkle_preview,
                                     fx_wrinkle_baseline,
                                     fx_wrinkle_use_skin_gate,
                                     fx_wrinkle_mask_gain,
                                     fx_wrinkle_neg_cap);
          } else {
            ApplySkinSmoothingBGR(frame_bgr, fr, fx_skin_strength);
          }
        }
        if (fx_lipstick)  ApplyLipRefinerBGR(frame_bgr, fr,
                     cv::Scalar((int)(fx_lip_color[2]*255.0f),(int)(fx_lip_color[1]*255.0f),(int)(fx_lip_color[0]*255.0f)),
                     fx_lip_alpha, fx_lip_feather, fx_lip_light, fx_lip_band,
                     used_lms, frame_bgr.size());
      }
    }

    if (show_mask && !mask8_resized.empty()) {
      display_rgb = VisualizeMaskRGB(mask8_resized);
    } else if (bg_mode == 0) {
      // No background effect; show camera
      cv::cvtColor(frame_bgr, display_rgb, cv::COLOR_BGR2RGB);
    } else if (bg_mode == 1 && !mask8_resized.empty()) {
      display_rgb = CompositeBlurBackgroundBGR(frame_bgr, mask8_resized, blur_strength, feather_px);
      // Debug: print means once per second
      uint32_t now_dbg = SDL_GetTicks();
      if (now_dbg - dbg_last_ms > 1000) {
        cv::Scalar m_frame = cv::mean(frame_bgr);
        cv::Mat dbg_bgr; cv::cvtColor(display_rgb, dbg_bgr, cv::COLOR_RGB2BGR);
        cv::Scalar m_comp  = cv::mean(dbg_bgr);
        std::cout << "mean(BGR) frame=" << m_frame << " comp=" << m_comp << std::endl;
        dbg_last_ms = now_dbg;
      }
    } else if (bg_mode == 2 && !last_mask_u8.empty()) {
      // Background image replace (fill background with image)
      if (!bg_image.empty()) {
        cv::Mat bg_resized; cv::resize(bg_image, bg_resized, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
        display_rgb = CompositeImageBackgroundBGR(frame_bgr, mask8_resized, bg_resized);
      } else {
        // No image loaded, fall back to camera
        cv::cvtColor(frame_bgr, display_rgb, cv::COLOR_BGR2RGB);
      }
    } else if (bg_mode == 3 && !last_mask_u8.empty()) {
      // Solid color background
      cv::Scalar bgr((int)(solid_color[2]*255.0f), (int)(solid_color[1]*255.0f), (int)(solid_color[0]*255.0f));
      display_rgb = CompositeSolidBackgroundBGR(frame_bgr, mask8_resized, bgr);
    } else {
      // No mask yet; show camera
      cv::cvtColor(frame_bgr, display_rgb, cv::COLOR_BGR2RGB);
    }

    // Optional: draw face landmarks overlay (on display_rgb)
    if (show_landmarks && have_lms) {
      // Convert to BGR for correct OpenCV color ordering while drawing
      cv::Mat dbg_bgr; cv::cvtColor(display_rgb, dbg_bgr, cv::COLOR_RGB2BGR);
      // Draw points
      int W = dbg_bgr.cols, H = dbg_bgr.rows;
      mediapipe::NormalizedLandmarkList draw_lms = latest_lms;
      if (lm_roi_mode && !latest_rects.empty()) draw_lms = transform_lms_with_rect(latest_lms, latest_rects[0], W, H, lm_apply_rot);
      const int n = draw_lms.landmark_size();
      for (int i = 0; i < n; ++i) {
        const auto& p = draw_lms.landmark(i);
        float nx = p.x(); float ny = p.y();
        if (lm_swap_xy) { std::swap(nx, ny); }
        if (lm_flip_x) nx = 1.0f - nx;
        if (lm_flip_y) ny = 1.0f - ny;
        int x = std::max(0, std::min(W - 1, (int)std::round(nx * W)));
        int y = std::max(0, std::min(H - 1, (int)std::round(ny * H)));
        cv::circle(dbg_bgr, cv::Point(x, y), 1, cv::Scalar(0, 255, 0), cv::FILLED, cv::LINE_AA);
      }
      // Draw lip connections (Studio-like) for clearer outlines
      auto draw_conn = [&](int a, int b, const cv::Scalar& col){
        const auto& pa = draw_lms.landmark(a);
        const auto& pb = draw_lms.landmark(b);
        auto tx = [&](float nx, float ny){ if (lm_swap_xy) std::swap(nx,ny); if (lm_flip_x) nx=1.0f-nx; if (lm_flip_y) ny=1.0f-ny; return cv::Point((int)std::round(nx*W),(int)std::round(ny*H)); };
        cv::line(dbg_bgr, tx(pa.x(),pa.y()), tx(pb.x(),pb.y()), col, 1, cv::LINE_AA);
      };
      using Conn = ::mediapipe::tasks::vision::face_landmarker::FaceLandmarksConnections;
      for (const auto& e : Conn::kFaceLandmarksLips) draw_conn(e[0], e[1], cv::Scalar(0,128,255));
      if (show_mesh) {
        // Face oval and eyes/brows connectors
        for (const auto& e : Conn::kFaceLandmarksFaceOval) draw_conn(e[0], e[1], cv::Scalar(255,200,0));
        for (const auto& e : Conn::kFaceLandmarksLeftEye) draw_conn(e[0], e[1], cv::Scalar(80,200,255));
        for (const auto& e : Conn::kFaceLandmarksRightEye) draw_conn(e[0], e[1], cv::Scalar(80,200,255));
        for (const auto& e : Conn::kFaceLandmarksLeftEyeBrow) draw_conn(e[0], e[1], cv::Scalar(180,180,255));
        for (const auto& e : Conn::kFaceLandmarksRightEyeBrow) draw_conn(e[0], e[1], cv::Scalar(180,180,255));
        // Optional dense tessellation (heavier)
        if (show_mesh_dense) {
          for (const auto& e : Conn::kFaceLandmarksTesselation) draw_conn(e[0], e[1], cv::Scalar(120,120,120));
        }
      }
      // Optionally draw lip and face ovals for clarity
      FaceRegions fr;
      if (ExtractFaceRegions(draw_lms, dbg_bgr.size(), &fr, lm_flip_x, lm_flip_y, lm_swap_xy)) {
        if (!fr.face_oval.empty()) {
          const std::vector<std::vector<cv::Point>> polys = {fr.face_oval};
          cv::polylines(dbg_bgr, polys, true, cv::Scalar(255, 200, 0), 1, cv::LINE_AA);
        }
        if (!fr.lips_outer.empty()) {
          const std::vector<std::vector<cv::Point>> polys = {fr.lips_outer};
          cv::polylines(dbg_bgr, polys, true, cv::Scalar(0, 128, 255), 1, cv::LINE_AA);
        }
        if (!latest_rects.empty()) {
          const auto& r = latest_rects[0];
          int cx = (int)std::round(r.x_center() * W);
          int cy = (int)std::round(r.y_center() * H);
          int hw = (int)std::round(0.5f * r.width() * W);
          int hh = (int)std::round(0.5f * r.height() * H);
          cv::RotatedRect rr(cv::Point2f((float)cx,(float)cy), cv::Size2f((float)hw*2,(float)hh*2), r.rotation() * 180.0f / (float)M_PI);
          cv::Point2f verts[4]; rr.points(verts);
          for (int i=0;i<4;++i) cv::line(dbg_bgr, verts[i], verts[(i+1)%4], cv::Scalar(255,0,0), 1, cv::LINE_AA);
        }
      }
      cv::cvtColor(dbg_bgr, display_rgb, cv::COLOR_BGR2RGB);
    }

    // Optional: overlay wrinkle mask heatmap for debugging/inspection (small inset)
    if (dbg_wrinkle_mask && have_lms) {
      mediapipe::NormalizedLandmarkList used_lms = latest_lms;
      if (lm_roi_mode && !latest_rects.empty()) {
        used_lms = transform_lms_with_rect(latest_lms, latest_rects[0], frame_bgr.cols, frame_bgr.rows, lm_apply_rot);
      }
      FaceRegions fr_dbg; if (ExtractFaceRegions(used_lms, frame_bgr.size(), &fr_dbg, lm_flip_x, lm_flip_y, lm_swap_xy)) {
        float s_min = std::max(1.5f, fx_skin_radius * 0.5f);
        float s_max = std::max(3.0f, fx_skin_radius * 1.25f);
        if (fx_wrinkle_custom_scales) { s_min = fx_wrinkle_min_px; s_max = fx_wrinkle_max_px; }
        cv::Mat wr = BuildWrinkleLineMask(frame_bgr, fr_dbg, s_min, s_max,
                                          fx_wrinkle_suppress_lower, fx_wrinkle_lower_ratio,
                                          fx_wrinkle_ignore_glasses, fx_wrinkle_glasses_margin,
                                          fx_wrinkle_keep_ratio,
                                          fx_wrinkle_use_skin_gate,
                                          fx_wrinkle_mask_gain); // CV_32F 0..1
        cv::Mat wr_scaled; cv::multiply(wr, cv::Scalar(1.6f), wr_scaled); wr_scaled = cv::min(wr_scaled, 1.0f);
        cv::Mat wr8; wr_scaled.convertTo(wr8, CV_8U, 255.0);
        cv::Mat heat; cv::applyColorMap(wr8, heat, cv::COLORMAP_TURBO); // BGR
        // Convert display to BGR for drawing
        cv::Mat disp_bgr; cv::cvtColor(display_rgb, disp_bgr, cv::COLOR_RGB2BGR);
        // Draw as small inset (bottom-left), keeping aspect
        int inset_w = std::max(128, frame_bgr.cols / 4);
        int inset_h = std::max(96, frame_bgr.rows / 4);
        cv::Mat heat_small; cv::resize(heat, heat_small, cv::Size(inset_w, inset_h), 0, 0, cv::INTER_AREA);
        // Add thin border
        cv::Rect roi(10, disp_bgr.rows - inset_h - 10, inset_w, inset_h);
        roi.x = std::max(0, std::min(roi.x, disp_bgr.cols - roi.width));
        roi.y = std::max(0, std::min(roi.y, disp_bgr.rows - roi.height));
        cv::rectangle(disp_bgr, cv::Rect(roi.x-1, roi.y-1, roi.width+2, roi.height+2), cv::Scalar(30,30,30), 1, cv::LINE_AA);
        // Light alpha for readability
        cv::Mat dst = disp_bgr(roi);
        cv::addWeighted(dst, 0.65, heat_small, 0.35, 0.0, dst);
        if (dbg_wrinkle_stats) {
          // Build a face ROI mask to compute stats
          cv::Mat face_mask(frame_bgr.size(), CV_8U, cv::Scalar(0));
          if (!fr_dbg.face_oval.empty()) cv::fillPoly(face_mask, std::vector<std::vector<cv::Point>>{fr_dbg.face_oval}, cv::Scalar(255));
          if (!fr_dbg.lips_outer.empty()) cv::fillPoly(face_mask, std::vector<std::vector<cv::Point>>{fr_dbg.lips_outer}, cv::Scalar(0));
          if (!fr_dbg.left_eye.empty())   cv::fillPoly(face_mask, std::vector<std::vector<cv::Point>>{fr_dbg.left_eye},   cv::Scalar(0));
          if (!fr_dbg.right_eye.empty())  cv::fillPoly(face_mask, std::vector<std::vector<cv::Point>>{fr_dbg.right_eye},  cv::Scalar(0));
          cv::Scalar meanWr = cv::mean(wr, face_mask);
          double minv, maxv; cv::minMaxLoc(wr, &minv, &maxv, nullptr, nullptr, face_mask);
          char buf[128]; std::snprintf(buf, sizeof(buf), "wr mean: %.3f  max: %.3f", (float)meanWr[0], (float)maxv);
          int tx = roi.x + 6, ty = roi.y + 18;
          cv::putText(disp_bgr, buf, cv::Point(tx, ty), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(10,10,10), 2, cv::LINE_AA);
          cv::putText(disp_bgr, buf, cv::Point(tx, ty), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(240,240,240), 1, cv::LINE_AA);
        }
        cv::cvtColor(disp_bgr, display_rgb, cv::COLOR_BGR2RGB);
      }
    }

    upload(display_rgb);

    // Start new ImGui frame and build UI windows
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
    int dw, dh; SDL_GL_GetDrawableSize(window, &dw, &dh);
    glViewport(0,0,dw,dh); glClearColor(0.06f,0.06f,0.07f,1.0f); glClear(GL_COLOR_BUFFER_BIT);
    if (tex) {
      ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
      ImGui::SetWindowPos(ImVec2(0,0), ImGuiCond_Always); ImGui::SetWindowSize(ImVec2((float)dw,(float)dh), ImGuiCond_Always);
      float aspect = tex_h > 0 ? (float)tex_w/(float)tex_h : 1.0f; float w = (float)dw, h = w/aspect; if (h > dh) { h = (float)dh; w = h*aspect; }
      ImVec2 center((float)dw*0.5f, (float)dh*0.5f); ImVec2 size(w,h); ImVec2 pos(center.x - size.x*0.5f, center.y - size.y*0.5f);
      ImGui::SetCursorPos(ImVec2(pos.x,pos.y));
      ImGui::Image((ImTextureID)(intptr_t)tex, size, ImVec2(0,0), ImVec2(1,1));
      ImGui::End();
    } else {
      // Show explicit message while waiting for first frame/texture
      ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
      ImGui::SetWindowPos(ImVec2(0,0), ImGuiCond_Always); ImGui::SetWindowSize(ImVec2((float)dw,(float)dh), ImGuiCond_Always);
      ImGui::SetCursorPos(ImVec2(20, 20));
      ImGui::Text("Waiting for camera frame... (tex=0)");
      ImGui::End();
    }
    // Controls (allow user to move/resize; set defaults only once)
    ImGui::SetNextWindowPos(ImVec2(16,16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 240), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse)) {
      ImGui::Checkbox("Show Segmentation (mask)", &show_mask);
      const char* modes[] = {"None", "Blur", "Image", "Solid Color"};
      ImGui::Combo("Background", &bg_mode, modes, IM_ARRAYSIZE(modes));
      if (bg_mode == 1) {
        ImGui::SliderInt("Blur Strength", &blur_strength, 1, 31);
        if ((blur_strength % 2) == 0) blur_strength++;
      } else if (bg_mode == 2) {
        ImGui::InputText("Image Path", bg_path_buf, sizeof(bg_path_buf));
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
          cv::Mat img = cv::imread(bg_path_buf, cv::IMREAD_COLOR);
          if (!img.empty()) {
            bg_image = img; // BGR kept
          }
        }
      } else if (bg_mode == 3) {
        ImGui::ColorEdit3("Color", solid_color);
      }
      ImGui::TextDisabled("GPU graph; CPU composite");
      // Beauty effects UI
      if (!has_landmarks) {
        ImGui::TextDisabled("Face landmarks not available (use combined graph)");
      } else {
        ImGui::Separator();
        ImGui::Text("Beauty Effects");
        ImGui::Checkbox("Show Face Landmarks", &show_landmarks);
        if (show_landmarks) {
          ImGui::Checkbox("LM ROI coords", &lm_roi_mode); ImGui::SameLine();
          ImGui::Checkbox("Apply rotation", &lm_apply_rot);
          ImGui::Checkbox("Show grid", &show_mesh); ImGui::SameLine();
          ImGui::Checkbox("Dense", &show_mesh_dense);
        }
        if (show_landmarks) {
          ImGui::Checkbox("Flip X", &lm_flip_x); ImGui::SameLine();
          ImGui::Checkbox("Flip Y", &lm_flip_y); ImGui::SameLine();
          ImGui::Checkbox("Swap XY", &lm_swap_xy);
        }
        ImGui::Checkbox("Skin smoothing", &fx_skin);
        if (fx_skin) {
          ImGui::SameLine(); ImGui::Checkbox("Advanced", &fx_skin_adv);
          if (fx_skin_adv) {
            ImGui::SliderFloat("Amount##skin", &fx_skin_amount, 0.0f, 1.0f);
            ImGui::SliderFloat("Radius (px)", &fx_skin_radius, 1.0f, 20.0f);
            ImGui::SliderFloat("Texture keep (0..1)", &fx_skin_tex, 0.05f, 1.0f);
            ImGui::SliderFloat("Edge feather (px)", &fx_skin_edge, 2.0f, 40.0f);
            ImGui::Checkbox("Wrinkle-aware", &fx_skin_wrinkle);
            if (fx_skin_wrinkle) {
              ImGui::SliderFloat("Smile boost", &fx_skin_smile_boost, 0.0f, 1.0f);
              ImGui::SliderFloat("Squint boost", &fx_skin_squint_boost, 0.0f, 1.0f);
              ImGui::SliderFloat("Wrinkle gain", &fx_skin_wrinkle_gain, 0.0f, 8.0f);
              ImGui::Checkbox("Show wrinkle mask", &dbg_wrinkle_mask); ImGui::SameLine();
              ImGui::Checkbox("Show stats", &dbg_wrinkle_stats);
              ImGui::SliderFloat("Forehead boost", &fx_skin_forehead_boost, 0.0f, 2.0f);
              ImGui::Checkbox("Suppress chin/stubble", &fx_wrinkle_suppress_lower);
              if (fx_wrinkle_suppress_lower) {
                ImGui::SliderFloat("Lower-face ratio", &fx_wrinkle_lower_ratio, 0.25f, 0.65f);
              }
              ImGui::Checkbox("Ignore glasses", &fx_wrinkle_ignore_glasses); ImGui::SameLine();
              ImGui::SliderFloat("Glasses margin (px)", &fx_wrinkle_glasses_margin, 0.0f, 30.0f);
              ImGui::SliderFloat("Wrinkle sensitivity", &fx_wrinkle_keep_ratio, 0.05f, 10.60f);
              ImGui::Checkbox("Wrinkle-only preview", &fx_wrinkle_preview);
              ImGui::Checkbox("Custom line width", &fx_wrinkle_custom_scales);
              if (fx_wrinkle_custom_scales) {
                ImGui::SliderFloat("Min width (px)", &fx_wrinkle_min_px, 1.0f, 10.0f);
                ImGui::SliderFloat("Max width (px)", &fx_wrinkle_max_px, 2.0f, 16.0f);
                if (fx_wrinkle_max_px < fx_wrinkle_min_px) fx_wrinkle_max_px = fx_wrinkle_min_px;
              }
              ImGui::Checkbox("Skin gate (YCbCr)", &fx_wrinkle_use_skin_gate); ImGui::SameLine();
              ImGui::SliderFloat("Mask gain", &fx_wrinkle_mask_gain, 0.5f, 3.0f);
              ImGui::SliderFloat("Baseline boost", &fx_wrinkle_baseline, 0.0f, 1.0f);
              ImGui::SliderFloat("Neg atten cap", &fx_wrinkle_neg_cap, 0.6f, 1.0f);
            }
          } else {
            ImGui::SliderFloat("Strength", &fx_skin_strength, 0.0f, 1.0f);
          }
        }
        ImGui::Checkbox("Lip refiner", &fx_lipstick);
        ImGui::ColorEdit3("Lip color", fx_lip_color);
        ImGui::SliderFloat("Amount##lip", &fx_lip_alpha, 0.0f, 1.0f);
        ImGui::SliderFloat("Feather (px)", &fx_lip_feather, 0.0f, 20.0f);
        ImGui::SliderFloat("Band grow (px)", &fx_lip_band, 0.0f, 12.0f);
        ImGui::SliderFloat("Lightness", &fx_lip_light, -1.0f, 1.0f);
        ImGui::Checkbox("Teeth whitening", &fx_teeth); ImGui::SameLine(); ImGui::SliderFloat("Amount##teeth", &fx_teeth_strength, 0.0f, 1.0f);
        ImGui::SliderFloat("Avoid lips (px)", &fx_teeth_margin, 0.0f, 12.0f);
      }
    }
    ImGui::End();
    // Status overlay top-right
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::SetNextWindowPos(ImVec2((float)dw - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1,0));
    if (ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs)) {
      ImGui::Text("fps: %.1f", fps);
      ImGui::Text("tex: %dx%d", tex_w, tex_h);
      ImGui::Text("mask: %s", last_mask_u8.empty()?"no":"yes");
    }
    ImGui::End();
    // Render
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext();
  if (tex) glDeleteTextures(1, &tex);
  SDL_GL_DeleteContext(gl_context); SDL_DestroyWindow(window); SDL_Quit();

  { auto st = graph.CloseInputStream("input_video"); if (!st.ok()) std::fprintf(stderr, "CloseInputStream failed: %s\n", st.message().data()); }
  { auto st = graph.WaitUntilDone(); if (!st.ok()) std::fprintf(stderr, "WaitUntilDone failed: %s\n", st.message().data()); }
  return 0;
}
