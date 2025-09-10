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
#include <fstream>
#include <algorithm>
#include <filesystem>

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
#include "cam_enum.h"
#include <linux/videodev2.h>
#include "mediapipe/tasks/cc/vision/face_landmarker/face_landmarks_connections.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <opencv2/core/ocl.hpp>
#include <chrono>

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
  std::vector<CameraDesc> cam_list = EnumerateCameras();
  int ui_cam_idx = 0; // index into cam_list
  int ui_res_idx = 0; // index into resolutions of selected cam
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
  // Defer StartRun was previously moved; restored to earlier start for stability.

  // Try V4L2 first, then fallback to default backend.
  if (!cam_list.empty()) {
    for (size_t i=0;i<cam_list.size();++i) if (cam_list[i].index == cam_index) { ui_cam_idx = (int)i; break; }
  }
  auto open_capture = [&](int idx, int w, int h){
    cv::VideoCapture c(idx, cv::CAP_V4L2);
    if (c.isOpened() && w>0 && h>0) {
      c.set(cv::CAP_PROP_FRAME_WIDTH, w);
      c.set(cv::CAP_PROP_FRAME_HEIGHT, h);
      c.set(cv::CAP_PROP_FRAME_WIDTH, w);
      c.set(cv::CAP_PROP_FRAME_HEIGHT, h);
    }
    return c;
  };
  int init_w = 0, init_h = 0;
  if (!cam_list.empty() && !cam_list[ui_cam_idx].resolutions.empty()) {
    auto wh = cam_list[ui_cam_idx].resolutions.back();
    init_w = wh.first; init_h = wh.second;
  }
  cv::VideoCapture cap = open_capture(cam_index, init_w, init_h);
  // Profiles (saved settings)
  namespace fs = std::filesystem;
  auto get_profile_dir = [](){
    const char* home = std::getenv("HOME");
    std::string dir = (home && *home) ? std::string(home) + "/.config/segmecam" : std::string("./.segmecam");
    fs::create_directories(dir);
    return dir;
  };
  std::string profile_dir = get_profile_dir();
  auto default_profile_path = profile_dir + "/default_profile.txt";
  auto list_profiles = [&](){
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(profile_dir, ec)) {
      if (ec) break; if (!e.is_regular_file()) continue;
      auto p = e.path(); if (p.extension() == ".yml" || p.extension() == ".yaml") names.push_back(p.stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
  };
  std::vector<std::string> profile_names = list_profiles();
  int ui_profile_idx = profile_names.empty()? -1 : 0;
  static char profile_name_buf[128] = {0};
  // Enumerate v4l2loopback outputs (virtual cams)
  std::vector<LoopbackDesc> vcam_list = EnumerateLoopbackDevices();
  int ui_vcam_idx = 0; bool vcam_running = false; int vcam_fd = -1; int vcam_w = 0, vcam_h = 0;
  std::vector<int> ui_fps_opts; int ui_fps_idx = 0;
  std::string current_cam_path = (!cam_list.empty()?cam_list[ui_cam_idx].path:std::string());
  if (!current_cam_path.empty() && init_w>0 && init_h>0) {
    ui_fps_opts = EnumerateFPS(current_cam_path, init_w, init_h);
    if (!ui_fps_opts.empty()) ui_fps_idx = (int)ui_fps_opts.size()-1;
  }
  // Camera control ranges
  CtrlRange r_brightness, r_contrast, r_saturation, r_gain, r_sharpness, r_zoom, r_focus;
  CtrlRange r_autogain, r_autofocus;
  // Some cameras don't expose AUTOGAIN; use exposure auto/absolute instead
  CtrlRange r_autoexposure, r_exposure_abs;
  // Additional helpful controls seen in guvcview
  CtrlRange r_awb, r_wb_temp, r_backlight, r_expo_dynfps;
  auto refresh_ctrls = [&]() {
    if (current_cam_path.empty()) return;
    QueryCtrl(current_cam_path, V4L2_CID_BRIGHTNESS, &r_brightness);
    QueryCtrl(current_cam_path, V4L2_CID_CONTRAST, &r_contrast);
    QueryCtrl(current_cam_path, V4L2_CID_SATURATION, &r_saturation);
    QueryCtrl(current_cam_path, V4L2_CID_GAIN, &r_gain);
    QueryCtrl(current_cam_path, V4L2_CID_SHARPNESS, &r_sharpness);
    QueryCtrl(current_cam_path, V4L2_CID_ZOOM_ABSOLUTE, &r_zoom);
    QueryCtrl(current_cam_path, V4L2_CID_FOCUS_ABSOLUTE, &r_focus);
    QueryCtrl(current_cam_path, V4L2_CID_AUTOGAIN, &r_autogain);
    QueryCtrl(current_cam_path, V4L2_CID_FOCUS_AUTO, &r_autofocus);
    // Exposure controls (fallback when AUTOGAIN is not available)
    QueryCtrl(current_cam_path, V4L2_CID_EXPOSURE_AUTO, &r_autoexposure);
    QueryCtrl(current_cam_path, V4L2_CID_EXPOSURE_ABSOLUTE, &r_exposure_abs);
    // White balance / backlight / dynamic fps (exposure priority)
    QueryCtrl(current_cam_path, V4L2_CID_AUTO_WHITE_BALANCE, &r_awb);
    QueryCtrl(current_cam_path, V4L2_CID_WHITE_BALANCE_TEMPERATURE, &r_wb_temp);
    QueryCtrl(current_cam_path, V4L2_CID_BACKLIGHT_COMPENSATION, &r_backlight);
    QueryCtrl(current_cam_path, V4L2_CID_EXPOSURE_AUTO_PRIORITY, &r_expo_dynfps);
  };
  auto apply_default_ctrls = [&]() {
    // Set auto focus enabled by default if supported
    if (!current_cam_path.empty() && r_autofocus.available && r_autofocus.val == 0) {
      if (SetCtrl(current_cam_path, V4L2_CID_FOCUS_AUTO, 1)) {
        r_autofocus.val = 1;
      }
    }
  };
  refresh_ctrls();
  apply_default_ctrls();
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
  bool vsync_on = true; // allow toggling to diagnose frame pacing caps
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
  // Now start the Mediapipe graph (start earlier to match stable behavior)
  // (Moved back before SDL init to previous timing)

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

  bool show_mask = false; int blur_strength = 25; float feather_px = 2.0f; int64_t frame_id = 0;
  bool dbg_composite_rgb = false; // composite in RGB space to test channel order
  // OpenCL acceleration (Transparent API)
  bool use_opencl = false; bool opencl_available = cv::ocl::haveOpenCL();
  // Perf logging (terminal)
  bool perf_log = false; int perf_log_interval_ms = 5000; // 5s default
  uint32_t perf_last_log_ms = SDL_GetTicks();
  double perf_sum_frame_ms = 0.0, perf_sum_smooth_ms = 0.0, perf_sum_bg_ms = 0.0; uint32_t perf_sum_frames = 0;
  bool perf_logged_caps = false;
  // Background mode: 0=None, 1=Blur, 2=Image, 3=Solid Color
  int bg_mode = 0;
  cv::Mat bg_image; // background image (BGR)
  static char bg_path_buf[512] = {0};
  float solid_color[3] = {0.0f, 0.0f, 0.0f}; // RGB 0..1
  cv::Mat last_mask_u8;  // cache latest mask to avoid blocking
  cv::Mat last_display_rgb;
  // Beauty controls
  bool fx_skin = false; float fx_skin_strength = 0.4f;
  bool fx_skin_adv = true; float fx_skin_amount = 0.5f; float fx_skin_radius = 6.0f; float fx_skin_tex = 0.35f; float fx_skin_edge = 12.0f;
  // Advanced processing scale (downscale ROI before processing to save CPU). 1.0 = full res.
  float fx_adv_scale = 1.0f; // 0.5..1.0
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

  auto close_vcam = [&](){ if (vcam_fd >= 0) { ::close(vcam_fd); vcam_fd = -1; vcam_running=false; } };
  auto open_vcam = [&](const std::string& path, int W, int H){
    close_vcam();
    int fd = ::open(path.c_str(), O_RDWR);
    if (fd < 0) { std::perror("open vcam"); return false; }
    v4l2_format fmt{}; fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT; fmt.fmt.pix.width = W; fmt.fmt.pix.height = H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = W * 2; fmt.fmt.pix.sizeimage = W * H * 2;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) != 0) { std::perror("VIDIOC_S_FMT vcam"); ::close(fd); return false; }
    vcam_fd = fd; vcam_w = W; vcam_h = H; vcam_running = true; return true;
  };
  auto bgr_to_yuyv = [&](const cv::Mat& bgr, std::vector<uint8_t>& out){
    int W = bgr.cols, H = bgr.rows; out.resize(W*H*2);
    const uint8_t* p = bgr.data; int s = (int)bgr.step; uint8_t* o = out.data();
    auto clamp = [](int v){ return (uint8_t)std::min(255,std::max(0,v)); };
    for (int y=0;y<H;++y){
      const uint8_t* row = p + y*s;
      for (int x=0;x<W; x+=2){
        int b0=row[x*3+0], g0=row[x*3+1], r0=row[x*3+2];
        int b1=row[(x+1)*3+0], g1=row[(x+1)*3+1], r1=row[(x+1)*3+2];
        int Y0 = ( 66*r0 +129*g0 + 25*b0 +128)>>8; Y0 += 16;
        int Y1 = ( 66*r1 +129*g1 + 25*b1 +128)>>8; Y1 += 16;
        int U  = (-38*r0 - 74*g0 +112*b0 +128)>>8; U += 128;
        int V  = (112*r0 - 94*g0 - 18*b0 +128)>>8; V += 128;
        // Average U,V over pair for simplicity
        int U1 = (-38*r1 - 74*g1 +112*b1 +128)>>8; U1 += 128;
        int V1 = (112*r1 - 94*g1 - 18*b1 +128)>>8; V1 += 128;
        U = (U+U1)>>1; V = (V+V1)>>1;
        *o++ = clamp(Y0); *o++ = clamp(U); *o++ = clamp(Y1); *o++ = clamp(V);
      }
    }
  };

  // Profile save/load helpers (use OpenCV FileStorage YAML)
  auto save_profile = [&](const std::string& name){
    if (name.empty()) return false; fs::create_directories(profile_dir);
    std::string path = profile_dir + "/" + name + ".yml";
    cv::FileStorage fsw(path, cv::FileStorage::WRITE);
    if (!fsw.isOpened()) return false;
    // Camera info
    int saved_w = 0, saved_h = 0, saved_fps = 0;
    if (!cam_list.empty() && ui_cam_idx >= 0 && ui_cam_idx < (int)cam_list.size()) {
      const auto& rlist = cam_list[ui_cam_idx].resolutions;
      if (!rlist.empty() && ui_res_idx >= 0 && ui_res_idx < (int)rlist.size()) { saved_w = rlist[ui_res_idx].first; saved_h = rlist[ui_res_idx].second; }
    }
    if (!ui_fps_opts.empty() && ui_fps_idx >= 0 && ui_fps_idx < (int)ui_fps_opts.size()) saved_fps = ui_fps_opts[ui_fps_idx];
    fsw << "cam_path" << current_cam_path;
    fsw << "res_w" << saved_w << "res_h" << saved_h;
    fsw << "fps_value" << saved_fps;
    // UI indices (best-effort)
    fsw << "ui_cam_idx" << ui_cam_idx << "ui_res_idx" << ui_res_idx << "ui_fps_idx" << ui_fps_idx;
    fsw << "vsync_on" << (int)vsync_on;
    fsw << "show_mask" << (int)show_mask << "bg_mode" << bg_mode << "blur_strength" << blur_strength << "feather_px" << feather_px;
    fsw << "solid_color" << "[" << solid_color[0] << solid_color[1] << solid_color[2] << "]";
    fsw << "bg_path" << bg_path_buf;
    fsw << "show_landmarks" << (int)show_landmarks << "lm_roi_mode" << (int)lm_roi_mode << "lm_apply_rot" << (int)lm_apply_rot
        << "lm_flip_x" << (int)lm_flip_x << "lm_flip_y" << (int)lm_flip_y << "lm_swap_xy" << (int)lm_swap_xy;
    fsw << "fx_skin" << (int)fx_skin << "fx_skin_adv" << (int)fx_skin_adv << "fx_skin_strength" << fx_skin_strength
        << "fx_skin_amount" << fx_skin_amount << "fx_skin_radius" << fx_skin_radius << "fx_skin_tex" << fx_skin_tex << "fx_skin_edge" << fx_skin_edge
        << "fx_adv_scale" << fx_adv_scale;
    fsw << "fx_skin_wrinkle" << (int)fx_skin_wrinkle << "fx_skin_smile_boost" << fx_skin_smile_boost << "fx_skin_squint_boost" << fx_skin_squint_boost
        << "fx_skin_forehead_boost" << fx_skin_forehead_boost << "fx_skin_wrinkle_gain" << fx_skin_wrinkle_gain
        << "dbg_wrinkle_mask" << (int)dbg_wrinkle_mask << "dbg_wrinkle_stats" << (int)dbg_wrinkle_stats
        << "fx_wrinkle_suppress_lower" << (int)fx_wrinkle_suppress_lower << "fx_wrinkle_lower_ratio" << fx_wrinkle_lower_ratio
        << "fx_wrinkle_ignore_glasses" << (int)fx_wrinkle_ignore_glasses << "fx_wrinkle_glasses_margin" << fx_wrinkle_glasses_margin
        << "fx_wrinkle_keep_ratio" << fx_wrinkle_keep_ratio << "fx_wrinkle_custom_scales" << (int)fx_wrinkle_custom_scales
        << "fx_wrinkle_min_px" << fx_wrinkle_min_px << "fx_wrinkle_max_px" << fx_wrinkle_max_px
        << "fx_wrinkle_use_skin_gate" << (int)fx_wrinkle_use_skin_gate << "fx_wrinkle_mask_gain" << fx_wrinkle_mask_gain
        << "fx_wrinkle_baseline" << fx_wrinkle_baseline << "fx_wrinkle_neg_cap" << fx_wrinkle_neg_cap
        << "fx_wrinkle_preview" << (int)fx_wrinkle_preview;
    fsw << "fx_lipstick" << (int)fx_lipstick << "fx_lip_alpha" << fx_lip_alpha << "fx_lip_feather" << fx_lip_feather << "fx_lip_light" << fx_lip_light << "fx_lip_band" << fx_lip_band;
    fsw << "fx_lip_color" << "[" << fx_lip_color[0] << fx_lip_color[1] << fx_lip_color[2] << "]";
    fsw << "fx_teeth" << (int)fx_teeth << "fx_teeth_strength" << fx_teeth_strength << "fx_teeth_margin" << fx_teeth_margin;
    fsw.release();
    return true;
  };
  auto read_int = [](const cv::FileNode& n, int def){ return n.empty()?def:(int)n; };
  auto read_float = [](const cv::FileNode& n, float def){ return n.empty()?def:(float)n; };
  auto load_profile = [&](const std::string& name){
    std::string path = profile_dir + "/" + name + ".yml";
    cv::FileStorage fsr(path, cv::FileStorage::READ);
    if (!fsr.isOpened()) return false;
    cv::FileNode root = fsr.root();
    if (root.empty() || !root.isMap()) return false; // guard malformed/old files
    // Camera selection by path, resolution and fps (robust to device order changes)
    std::string saved_path; if (!root["cam_path"].empty()) root["cam_path"] >> saved_path;
    int saved_w = read_int(root["res_w"], 0);
    int saved_h = read_int(root["res_h"], 0);
    int saved_fps = read_int(root["fps_value"], 0);
    // Choose camera index
    if (!cam_list.empty()) {
      int chosen_idx = -1;
      if (!saved_path.empty()) {
        for (size_t i=0;i<cam_list.size();++i) if (cam_list[i].path == saved_path) { chosen_idx = (int)i; break; }
      }
      if (chosen_idx < 0) chosen_idx = 0; // fallback to first enumerated
      ui_cam_idx = chosen_idx;
      // Choose resolution
      const auto& rlist = cam_list[ui_cam_idx].resolutions;
      if (!rlist.empty()) {
        int ridx = -1;
        if (saved_w>0 && saved_h>0) {
          for (size_t i=0;i<rlist.size();++i) if (rlist[i].first==saved_w && rlist[i].second==saved_h) { ridx=(int)i; break; }
        }
        if (ridx < 0) ridx = (int)rlist.size()-1; // fallback to largest known
        ui_res_idx = ridx;
        // Enumerate fps for this resolution
        current_cam_path = cam_list[ui_cam_idx].path;
        ui_fps_opts = EnumerateFPS(current_cam_path, rlist[ui_res_idx].first, rlist[ui_res_idx].second);
        // Pick fps
        if (!ui_fps_opts.empty()) {
          int fidx = (int)ui_fps_opts.size()-1;
          if (saved_fps > 0) {
            for (size_t i=0;i<ui_fps_opts.size();++i) if (ui_fps_opts[i]==saved_fps) { fidx=(int)i; break; }
          }
          ui_fps_idx = fidx;
        } else {
          ui_fps_idx = 0;
        }
        // Open camera
        int new_idx = cam_list[ui_cam_idx].index;
        auto wh = rlist[ui_res_idx];
        cap.release();
        cap = open_capture(new_idx, wh.first, wh.second);
        if (!cap.isOpened()) cap.open(new_idx);
        if (!ui_fps_opts.empty()) {
          double fpsv = (double)ui_fps_opts[ui_fps_idx]; cap.set(cv::CAP_PROP_FPS, fpsv);
        }
        refresh_ctrls();
        apply_default_ctrls();
      }
    }
    // Legacy indices if present (non-fatal)
    ui_cam_idx = read_int(root["ui_cam_idx"], ui_cam_idx);
    ui_res_idx = read_int(root["ui_res_idx"], ui_res_idx);
    ui_fps_idx = read_int(root["ui_fps_idx"], ui_fps_idx);
    vsync_on = read_int(root["vsync_on"], vsync_on?1:0)!=0; SDL_GL_SetSwapInterval(vsync_on?1:0);
    show_mask = read_int(root["show_mask"], show_mask?1:0)!=0;
    bg_mode = read_int(root["bg_mode"], bg_mode);
    blur_strength = read_int(root["blur_strength"], blur_strength);
    feather_px = read_float(root["feather_px"], feather_px);
    if (!root["solid_color"].empty() && root["solid_color"].isSeq()) {
      auto sc = root["solid_color"]; int i=0; for (auto it=sc.begin(); it!=sc.end() && i<3; ++it,++i) solid_color[i] = (float)((double)*it);
    }
    if (!root["bg_path"].empty()) { std::string s; root["bg_path"] >> s; std::snprintf(bg_path_buf,sizeof(bg_path_buf),"%s", s.c_str()); }
    show_landmarks = read_int(root["show_landmarks"], show_landmarks?1:0)!=0;
    lm_roi_mode = read_int(root["lm_roi_mode"], lm_roi_mode?1:0)!=0;
    lm_apply_rot = read_int(root["lm_apply_rot"], lm_apply_rot?1:0)!=0;
    lm_flip_x = read_int(root["lm_flip_x"], lm_flip_x?1:0)!=0;
    lm_flip_y = read_int(root["lm_flip_y"], lm_flip_y?1:0)!=0;
    lm_swap_xy = read_int(root["lm_swap_xy"], lm_swap_xy?1:0)!=0;
    fx_skin = read_int(root["fx_skin"], fx_skin?1:0)!=0; fx_skin_adv = read_int(root["fx_skin_adv"], fx_skin_adv?1:0)!=0;
    fx_skin_strength = read_float(root["fx_skin_strength"], fx_skin_strength);
    fx_skin_amount = read_float(root["fx_skin_amount"], fx_skin_amount);
    fx_skin_radius = read_float(root["fx_skin_radius"], fx_skin_radius);
    fx_skin_tex = read_float(root["fx_skin_tex"], fx_skin_tex);
    fx_skin_edge = read_float(root["fx_skin_edge"], fx_skin_edge);
    fx_adv_scale = read_float(root["fx_adv_scale"], fx_adv_scale);
    fx_skin_wrinkle = read_int(root["fx_skin_wrinkle"], fx_skin_wrinkle?1:0)!=0;
    fx_skin_smile_boost = read_float(root["fx_skin_smile_boost"], fx_skin_smile_boost);
    fx_skin_squint_boost = read_float(root["fx_skin_squint_boost"], fx_skin_squint_boost);
    fx_skin_forehead_boost = read_float(root["fx_skin_forehead_boost"], fx_skin_forehead_boost);
    fx_skin_wrinkle_gain = read_float(root["fx_skin_wrinkle_gain"], fx_skin_wrinkle_gain);
    dbg_wrinkle_mask = read_int(root["dbg_wrinkle_mask"], dbg_wrinkle_mask?1:0)!=0;
    dbg_wrinkle_stats = read_int(root["dbg_wrinkle_stats"], dbg_wrinkle_stats?1:0)!=0;
    fx_wrinkle_suppress_lower = read_int(root["fx_wrinkle_suppress_lower"], fx_wrinkle_suppress_lower?1:0)!=0;
    fx_wrinkle_lower_ratio = read_float(root["fx_wrinkle_lower_ratio"], fx_wrinkle_lower_ratio);
    fx_wrinkle_ignore_glasses = read_int(root["fx_wrinkle_ignore_glasses"], fx_wrinkle_ignore_glasses?1:0)!=0;
    fx_wrinkle_glasses_margin = read_float(root["fx_wrinkle_glasses_margin"], fx_wrinkle_glasses_margin);
    fx_wrinkle_keep_ratio = read_float(root["fx_wrinkle_keep_ratio"], fx_wrinkle_keep_ratio);
    fx_wrinkle_custom_scales = read_int(root["fx_wrinkle_custom_scales"], fx_wrinkle_custom_scales?1:0)!=0;
    fx_wrinkle_min_px = read_float(root["fx_wrinkle_min_px"], fx_wrinkle_min_px);
    fx_wrinkle_max_px = read_float(root["fx_wrinkle_max_px"], fx_wrinkle_max_px);
    fx_wrinkle_use_skin_gate = read_int(root["fx_wrinkle_use_skin_gate"], fx_wrinkle_use_skin_gate?1:0)!=0;
    fx_wrinkle_mask_gain = read_float(root["fx_wrinkle_mask_gain"], fx_wrinkle_mask_gain);
    fx_wrinkle_baseline = read_float(root["fx_wrinkle_baseline"], fx_wrinkle_baseline);
    fx_wrinkle_neg_cap = read_float(root["fx_wrinkle_neg_cap"], fx_wrinkle_neg_cap);
    fx_wrinkle_preview = read_int(root["fx_wrinkle_preview"], fx_wrinkle_preview?1:0)!=0;
    fx_lipstick = read_int(root["fx_lipstick"], fx_lipstick?1:0)!=0;
    fx_lip_alpha = read_float(root["fx_lip_alpha"], fx_lip_alpha);
    fx_lip_feather = read_float(root["fx_lip_feather"], fx_lip_feather);
    fx_lip_light = read_float(root["fx_lip_light"], fx_lip_light);
    fx_lip_band = read_float(root["fx_lip_band"], fx_lip_band);
    if (!root["fx_lip_color"].empty() && root["fx_lip_color"].isSeq()) { auto c = root["fx_lip_color"]; int i=0; for (auto it=c.begin(); it!=c.end() && i<3; ++it,++i) fx_lip_color[i] = (float)((double)*it); }
    fx_teeth = read_int(root["fx_teeth"], fx_teeth?1:0)!=0;
    fx_teeth_strength = read_float(root["fx_teeth_strength"], fx_teeth_strength);
    fx_teeth_margin = read_float(root["fx_teeth_margin"], fx_teeth_margin);
    return true;
  };
  // Load default profile if present
  {
    std::ifstream fin(default_profile_path); if (fin) { std::string defname; std::getline(fin, defname); if (!defname.empty()) { load_profile(defname); profile_names = list_profiles(); auto it = std::find(profile_names.begin(), profile_names.end(), defname); ui_profile_idx = (it==profile_names.end()? -1 : (int)std::distance(profile_names.begin(), it)); std::snprintf(profile_name_buf, sizeof(profile_name_buf), "%s", defname.c_str()); }}
  }
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
    auto t_frame_start = std::chrono::steady_clock::now();
    double t_smooth_ms = 0.0, t_bg_ms = 0.0;
    if (have_lms) {
      mediapipe::NormalizedLandmarkList used_lms = latest_lms;
      if (lm_roi_mode && !latest_rects.empty()) {
        used_lms = transform_lms_with_rect(latest_lms, latest_rects[0], frame_bgr.cols, frame_bgr.rows, lm_apply_rot);
      }
      FaceRegions fr; if (ExtractFaceRegions(used_lms, frame_bgr.size(), &fr, lm_flip_x, lm_flip_y, lm_swap_xy)) {
        // Teeth first to avoid overriding lips later
        if (fx_teeth)     ApplyTeethWhitenBGR(frame_bgr, fr, fx_teeth_strength, fx_teeth_margin);
        if (fx_skin) {
          auto t0 = std::chrono::steady_clock::now();
          if (fx_skin_adv) {
            const mediapipe::NormalizedLandmarkList* lmsp = (has_landmarks && have_lms && fx_skin_wrinkle) ? &used_lms : nullptr;
            float sboost = fx_skin_wrinkle ? fx_skin_smile_boost : 0.0f;
            float qboost = fx_skin_wrinkle ? fx_skin_squint_boost : 0.0f;
            if (fx_adv_scale < 0.999f) {
              // Process a padded face ROI at reduced scale, then upsample and paste back
              cv::Rect face_bb = cv::boundingRect(fr.face_oval);
              int pad = std::max(8, (int)std::round(fx_skin_edge + fx_skin_radius * 2.0f));
              cv::Rect roi(face_bb.x - pad, face_bb.y - pad, face_bb.width + 2*pad, face_bb.height + 2*pad);
              roi &= cv::Rect(0,0, frame_bgr.cols, frame_bgr.rows);
              if (roi.width >= 8 && roi.height >= 8) {
                // Build FaceRegions in ROI coordinates, then scale
                auto shift_poly = [&](const std::vector<cv::Point>& poly){ std::vector<cv::Point> out; out.reserve(poly.size()); for (auto p: poly){ out.emplace_back(p.x - roi.x, p.y - roi.y);} return out; };
                auto scale_poly = [&](const std::vector<cv::Point>& poly, float sc){ std::vector<cv::Point> out; out.reserve(poly.size()); for (auto p: poly){ out.emplace_back((int)std::round(p.x*sc), (int)std::round(p.y*sc)); } return out; };
                FaceRegions fr_roi; fr_roi.face_oval = shift_poly(fr.face_oval);
                fr_roi.lips_outer = shift_poly(fr.lips_outer);
                fr_roi.lips_inner = shift_poly(fr.lips_inner);
                fr_roi.left_eye   = shift_poly(fr.left_eye);
                fr_roi.right_eye  = shift_poly(fr.right_eye);
                float sc = std::clamp(fx_adv_scale, 0.5f, 1.0f);
                FaceRegions fr_small; fr_small.face_oval = scale_poly(fr_roi.face_oval, sc);
                fr_small.lips_outer = scale_poly(fr_roi.lips_outer, sc);
                fr_small.lips_inner = scale_poly(fr_roi.lips_inner, sc);
                fr_small.left_eye   = scale_poly(fr_roi.left_eye, sc);
                fr_small.right_eye  = scale_poly(fr_roi.right_eye, sc);
                // Landmarks: re-normalize to ROI then safe to pass with any scale
                mediapipe::NormalizedLandmarkList lms_roi;
                if (lmsp) {
                  lms_roi = *lmsp;
                  for (int i=0;i<lms_roi.landmark_size();++i) {
                    auto* p = lms_roi.mutable_landmark(i);
                    float px = p->x() * frame_bgr.cols;
                    float py = p->y() * frame_bgr.rows;
                    float xr = (px - roi.x) / (float)roi.width;
                    float yr = (py - roi.y) / (float)roi.height;
                    p->set_x(xr); p->set_y(yr);
                  }
                }
                // Resize ROI to small, process, then upsample back and paste
                cv::Mat roi_bgr = frame_bgr(roi);
                cv::Mat small; cv::resize(roi_bgr, small, cv::Size(), sc, sc, cv::INTER_AREA);
                const mediapipe::NormalizedLandmarkList* lmsp_small = lmsp ? &lms_roi : nullptr;
                ApplySkinSmoothingAdvBGR(small, fr_small, fx_skin_amount, fx_skin_radius*sc, fx_skin_tex, fx_skin_edge*sc, lmsp_small,
                                         sboost, qboost, fx_skin_forehead_boost, fx_skin_wrinkle_gain,
                                         fx_wrinkle_suppress_lower, fx_wrinkle_lower_ratio, fx_wrinkle_ignore_glasses, fx_wrinkle_glasses_margin*sc,
                                         fx_wrinkle_keep_ratio,
                                         fx_wrinkle_custom_scales ? fx_wrinkle_min_px*sc : -1.0f,
                                         fx_wrinkle_custom_scales ? fx_wrinkle_max_px*sc : -1.0f,
                                         10.0f*sc,
                                         fx_wrinkle_preview,
                                         fx_wrinkle_baseline,
                                         fx_wrinkle_use_skin_gate,
                                         fx_wrinkle_mask_gain,
                                         fx_wrinkle_neg_cap);
                cv::Mat up; cv::resize(small, up, roi.size(), 0, 0, cv::INTER_LINEAR);
                up.copyTo(roi_bgr);
              } else {
                // Fallback to full-res processing if ROI tiny
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
              }
            } else {
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
            }
          } else {
            ApplySkinSmoothingBGR(frame_bgr, fr, fx_skin_strength, use_opencl);
          }
          auto t1 = std::chrono::steady_clock::now();
          t_smooth_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
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
      auto t0 = std::chrono::steady_clock::now();
      display_rgb = CompositeBlurBackgroundBGR(frame_bgr, mask8_resized, blur_strength, feather_px);
      auto t1 = std::chrono::steady_clock::now();
      t_bg_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
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

    // Perf accumulation + periodic log
    auto t_frame_end = std::chrono::steady_clock::now();
    double t_frame_ms = std::chrono::duration<double, std::milli>(t_frame_end - t_frame_start).count();
    perf_sum_frame_ms += t_frame_ms; perf_sum_smooth_ms += t_smooth_ms; perf_sum_bg_ms += t_bg_ms; perf_sum_frames++;
    if (perf_log && !perf_logged_caps) {
      std::cout << "[PERF] OpenCL build: " << (opencl_available?"available":"not available")
                << ", useOpenCL=" << (cv::ocl::useOpenCL()?"on":"off");
      if (opencl_available) {
        try {
          cv::ocl::Context ctx = cv::ocl::Context::getDefault();
          cv::ocl::Device dev = cv::ocl::Device::getDefault();
          std::cout << ", device='" << dev.name() << "' vendor='" << dev.vendorName() << "'";
        } catch (...) {}
      }
      std::cout << std::endl;
      perf_logged_caps = true;
    }
    uint32_t now_ms_perf = SDL_GetTicks();
    if (perf_log && now_ms_perf - perf_last_log_ms >= (uint32_t)perf_log_interval_ms && perf_sum_frames > 0) {
      double avg_frame = perf_sum_frame_ms / (double)perf_sum_frames;
      double avg_smooth = perf_sum_smooth_ms / (double)perf_sum_frames;
      double avg_bg = perf_sum_bg_ms / (double)perf_sum_frames;
      double est_fps = 1000.0 / std::max(1e-3, avg_frame);
      std::cout << "[PERF] frames=" << perf_sum_frames
                << " avg_ms frame=" << avg_frame
                << " smooth=" << avg_smooth
                << " bg=" << avg_bg
                << " est_fps=" << est_fps
                << " (bg_mode=" << bg_mode << ", adv=" << (int)fx_skin_adv << ", smooth_on=" << (int)fx_skin
                << ", ocl=" << (cv::ocl::useOpenCL()?"on":"off") << ")"
                << std::endl;
      perf_sum_frame_ms = perf_sum_smooth_ms = perf_sum_bg_ms = 0.0; perf_sum_frames = 0; perf_last_log_ms = now_ms_perf;
    }

    // If virtual cam enabled, push this frame into loopback (YUYV)
    if (vcam_running && vcam_fd >= 0 && !display_rgb.empty()) {
      // Ensure format matches; if not, try to reconfigure
      if (display_rgb.cols != vcam_w || display_rgb.rows != vcam_h) {
        open_vcam(vcam_list.empty()?std::string():vcam_list[ui_vcam_idx].path, display_rgb.cols, display_rgb.rows);
      }
      cv::Mat disp_bgr; cv::cvtColor(display_rgb, disp_bgr, cv::COLOR_RGB2BGR);
      std::vector<uint8_t> yuyv; bgr_to_yuyv(disp_bgr, yuyv);
      ssize_t need = (ssize_t)yuyv.size();
      ssize_t wr = ::write(vcam_fd, yuyv.data(), need);
      (void)wr; // best-effort
    }

    // Start new ImGui frame and build UI windows
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
    int dw, dh; SDL_GL_GetDrawableSize(window, &dw, &dh);
    glViewport(0,0,dw,dh); glClearColor(0.06f,0.06f,0.07f,1.0f); glClear(GL_COLOR_BUFFER_BIT);
    // Preview window (kept on top by z-ordering of ImGui windows)
    ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
    ImGui::SetWindowPos(ImVec2(0,0), ImGuiCond_Always); ImGui::SetWindowSize(ImVec2((float)dw,(float)dh), ImGuiCond_Always);
    if (tex) {
      float aspect = tex_h > 0 ? (float)tex_w/(float)tex_h : 1.0f;
      float w = (float)dw, h = w/aspect; if (h > dh) { h = (float)dh; w = h*aspect; }
      ImVec2 center((float)dw*0.5f, (float)dh*0.5f); ImVec2 size(w,h); ImVec2 pos(center.x - size.x*0.5f, center.y - size.y*0.5f);
      ImGui::SetCursorPos(ImVec2(pos.x,pos.y));
      ImGui::Image((ImTextureID)(intptr_t)tex, size, ImVec2(0,0), ImVec2(1,1));
    } else {
      ImGui::SetCursorPos(ImVec2(20,20)); ImGui::Text("Waiting for camera frame... (tex=0)");
    }
    ImGui::End();
    // Controls (allow user to move/resize; set defaults only once)
    ImGui::SetNextWindowPos(ImVec2(16,16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse)) {
      // Camera controls
      if (!cam_list.empty()) {
        ImGui::Text("Camera");
        std::vector<std::string> labels; labels.reserve(cam_list.size());
        for (const auto& c : cam_list) {
          char buf[256]; std::snprintf(buf, sizeof(buf), "%s (%s)", c.name.c_str(), c.path.c_str());
          labels.emplace_back(buf);
        }
        std::vector<const char*> items; items.reserve(labels.size());
        for (auto& s : labels) items.push_back(s.c_str());
        ImGui::Combo("Device", &ui_cam_idx, items.data(), (int)items.size());
        const auto& rlist = cam_list[ui_cam_idx].resolutions;
        if (!rlist.empty()) {
          if (ui_res_idx >= (int)rlist.size()) ui_res_idx = (int)rlist.size()-1;
          std::vector<std::string> rlabels; rlabels.reserve(rlist.size());
          for (auto& r : rlist) { char b[32]; std::snprintf(b,sizeof(b),"%dx%d", r.first, r.second); rlabels.emplace_back(b);} 
          std::vector<const char*> ritems; for (auto& s : rlabels) ritems.push_back(s.c_str());
          ImGui::Combo("Resolution", &ui_res_idx, ritems.data(), (int)ritems.size());
          // FPS combo for selected resolution
          if (ImGui::IsItemEdited() || ui_fps_opts.empty()) {
            auto wh2 = rlist[ui_res_idx];
            current_cam_path = cam_list[ui_cam_idx].path;
            ui_fps_opts = EnumerateFPS(current_cam_path, wh2.first, wh2.second);
            ui_fps_idx = ui_fps_opts.empty()?0:(int)ui_fps_opts.size()-1;
          }
          if (!ui_fps_opts.empty()) {
            std::vector<std::string> flabels; for (int f: ui_fps_opts){ char b[16]; std::snprintf(b,sizeof(b),"%d fps", f); flabels.emplace_back(b);} 
            std::vector<const char*> fitems; for (auto& s: flabels) fitems.push_back(s.c_str());
            ImGui::Combo("FPS", &ui_fps_idx, fitems.data(), (int)fitems.size());
          } else {
            ImGui::TextDisabled("FPS: unknown (driver)");
          }
          ImGui::SameLine();
          if (ImGui::Button("Apply##cam")) {
            int new_idx = cam_list[ui_cam_idx].index;
            auto wh = rlist[ui_res_idx];
            cap.release();
            cap = open_capture(new_idx, wh.first, wh.second);
            if (!cap.isOpened()) {
              cap.open(new_idx);
            }
            current_cam_path = cam_list[ui_cam_idx].path;
            if (!ui_fps_opts.empty()) {
              double fps = (double)ui_fps_opts[ui_fps_idx];
              cap.set(cv::CAP_PROP_FPS, fps);
            }
            refresh_ctrls();
            apply_default_ctrls();
          }
        } else {
          ImGui::TextDisabled("Resolutions: unknown (driver)");
        }
        ImGui::Separator();
      }
      // Profiles
      {
        ImGui::Text("Profile");
        // List existing profiles
        if (profile_names.empty()) {
          ImGui::TextDisabled("No profiles yet");
        } else {
          std::vector<const char*> items; for (auto& n: profile_names) items.push_back(n.c_str());
          if (ui_profile_idx < -1 || ui_profile_idx >= (int)profile_names.size()) ui_profile_idx = -1;
          ImGui::Combo("Select", &ui_profile_idx, items.data(), (int)items.size()); ImGui::SameLine();
          if (ImGui::Button("Load##prof") && ui_profile_idx>=0) { load_profile(profile_names[ui_profile_idx]); }
        }
        ImGui::InputText("Name", profile_name_buf, sizeof(profile_name_buf));
        if (ImGui::Button("Save##prof")) {
          std::string nm(profile_name_buf); if (!nm.empty()) { if (save_profile(nm)) { profile_names = list_profiles(); auto it = std::find(profile_names.begin(), profile_names.end(), nm); ui_profile_idx = (it==profile_names.end()?-1:(int)std::distance(profile_names.begin(), it)); } }
        }
        ImGui::SameLine();
        if (ImGui::Button("Set Default##prof")) { std::ofstream fout(default_profile_path); if (fout) fout << profile_name_buf; }
        ImGui::Separator();
      }
      // Virtual Webcam (v4l2loopback)
      {
        ImGui::Text("Virtual Webcam");
        if (vcam_list.empty()) {
          ImGui::TextDisabled("No v4l2 output devices found. modprobe v4l2loopback?");
        } else {
          std::vector<std::string> labels; labels.reserve(vcam_list.size());
          for (const auto& d : vcam_list) { char b[192]; std::snprintf(b,sizeof(b),"%s (%s)", d.name.c_str(), d.path.c_str()); labels.emplace_back(b); }
          std::vector<const char*> items; for (auto& s: labels) items.push_back(s.c_str());
          ImGui::Combo("Output", &ui_vcam_idx, items.data(), (int)items.size()); ImGui::SameLine();
          if (!vcam_running) {
            if (ImGui::Button("Start##vcam")) {
              int W = (tex_w>0?tex_w:init_w>0?init_w:640);
              int H = (tex_h>0?tex_h:init_h>0?init_h:480);
              if (ui_vcam_idx >=0 && ui_vcam_idx < (int)vcam_list.size()) open_vcam(vcam_list[ui_vcam_idx].path, W, H);
            }
          } else {
            if (ImGui::Button("Stop##vcam")) { close_vcam(); }
          }
        }
        ImGui::Separator();
      }
      // Camera controls (V4L2)
      if (!current_cam_path.empty()) {
        ImGui::Text("Camera Controls");
        auto slider_ctrl = [&](const char* label, CtrlRange& r, uint32_t id){
          if (!r.available) return; int v = r.val; int minv=r.min, maxv=r.max; int step= std::max(1, r.step);
          if (ImGui::SliderInt(label, &v, minv, maxv)) {
            // round to step
            int rs = minv + ((v - minv)/step)*step; r.val = rs; SetCtrl(current_cam_path, id, r.val);
          }
        };
        auto checkbox_ctrl = [&](const char* label, CtrlRange& r, uint32_t id){
          if (!r.available) return; int v = r.val != 0; if (ImGui::Checkbox(label, (bool*)&v)) { r.val = v?1:0; SetCtrl(current_cam_path, id, r.val);} };
        auto checkbox_exposure_auto = [&](const char* label){
          if (!r_autoexposure.available) return;
          // Treat any non-manual as enabled
          int mode = r_autoexposure.val;
          bool enabled = (mode != V4L2_EXPOSURE_MANUAL);
          if (ImGui::Checkbox(label, &enabled)) {
            if (!enabled) {
              // Turn OFF -> MANUAL
              if (SetCtrl(current_cam_path, V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL)) {
                r_autoexposure.val = V4L2_EXPOSURE_MANUAL;
              } else {
                std::cout << "Failed to set EXPOSURE_AUTO to MANUAL" << std::endl;
              }
            } else {
              // Turn ON: try supported non-manual modes in order of likelihood of success for UVC cams
              const int candidates[] = {
                (int)V4L2_EXPOSURE_APERTURE_PRIORITY,
                (int)V4L2_EXPOSURE_AUTO,
                (int)V4L2_EXPOSURE_SHUTTER_PRIORITY
              };
              bool ok = false;
              for (int c : candidates) {
                if (c < r_autoexposure.min || c > r_autoexposure.max || c == (int)V4L2_EXPOSURE_MANUAL) continue;
                if (SetCtrl(current_cam_path, V4L2_CID_EXPOSURE_AUTO, c)) {
                  r_autoexposure.val = c; ok = true; break;
                }
              }
              if (!ok) {
                // Last resort: try leaving as-is if it was already some auto mode
                if (mode != V4L2_EXPOSURE_MANUAL) {
                  r_autoexposure.val = mode;
                } else {
                  std::cout << "Failed to enable auto exposure: no supported mode accepted" << std::endl;
                }
              }
            }
          }
        };
        slider_ctrl("Brightness", r_brightness, V4L2_CID_BRIGHTNESS);
        slider_ctrl("Contrast",   r_contrast,   V4L2_CID_CONTRAST);
        slider_ctrl("Saturation", r_saturation, V4L2_CID_SATURATION);
        if (r_autogain.available) {
          checkbox_ctrl("Auto gain", r_autogain, V4L2_CID_AUTOGAIN);
        } else if (r_autoexposure.available) {
          // Fallback label when AUTOGAIN not provided by driver
          checkbox_exposure_auto("Auto exposure");
        }
        // Hide Gain/Exposure sliders completely when Auto Exposure is ON
        bool ae_on_global = (r_autoexposure.available && r_autoexposure.val != V4L2_EXPOSURE_MANUAL);
        if (!ae_on_global) {
          // Gain: disable only if explicit AUTOGAIN is enabled
          if (r_autogain.available && r_autogain.val) ImGui::BeginDisabled();
          slider_ctrl("Gain",       r_gain,       V4L2_CID_GAIN);
          if (r_autogain.available && r_autogain.val) ImGui::EndDisabled();
        }
        // Exposure controls and helpers
        if (r_autoexposure.available) {
          bool ae_on = (r_autoexposure.val != V4L2_EXPOSURE_MANUAL);
          if (r_exposure_abs.available && !ae_on) {
            slider_ctrl("Exposure",   r_exposure_abs, V4L2_CID_EXPOSURE_ABSOLUTE);
          }
          if (r_expo_dynfps.available) {
            checkbox_ctrl("Exposure dynamic framerate", r_expo_dynfps, V4L2_CID_EXPOSURE_AUTO_PRIORITY);
          }
        }
        if (r_backlight.available) {
          if (r_backlight.min == 0 && r_backlight.max == 1 && r_backlight.step == 1) {
            checkbox_ctrl("Backlight compensation", r_backlight, V4L2_CID_BACKLIGHT_COMPENSATION);
          } else {
            slider_ctrl("Backlight compensation", r_backlight, V4L2_CID_BACKLIGHT_COMPENSATION);
          }
        }
        slider_ctrl("Sharpness",  r_sharpness,  V4L2_CID_SHARPNESS);
        slider_ctrl("Zoom",       r_zoom,       V4L2_CID_ZOOM_ABSOLUTE);
        checkbox_ctrl("Auto focus", r_autofocus, V4L2_CID_FOCUS_AUTO);
        slider_ctrl("Focus",      r_focus,      V4L2_CID_FOCUS_ABSOLUTE);
        // White balance (AWB + temperature)
        if (r_awb.available) {
          checkbox_ctrl("Auto white balance", r_awb, V4L2_CID_AUTO_WHITE_BALANCE);
          if (r_wb_temp.available) {
            if (r_awb.val) ImGui::BeginDisabled();
            slider_ctrl("White balance (temp)", r_wb_temp, V4L2_CID_WHITE_BALANCE_TEMPERATURE);
            if (r_awb.val) ImGui::EndDisabled();
          }
        }
        ImGui::Separator();
      }
      ImGui::Checkbox("Show Segmentation (mask)", &show_mask);
      const char* modes[] = {"None", "Blur", "Image", "Solid Color"};
      ImGui::Combo("Background", &bg_mode, modes, IM_ARRAYSIZE(modes));
      if (bg_mode == 1) {
        ImGui::SliderInt("Blur Strength", &blur_strength, 1, 61);
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

        // Overlay/Debug controls (collapsed by default)
        ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Overlay & Debug")) {
          ImGui::Checkbox("Show Face Landmarks", &show_landmarks);
          if (show_landmarks) {
            ImGui::Checkbox("LM ROI coords", &lm_roi_mode); ImGui::SameLine();
            ImGui::Checkbox("Apply rotation", &lm_apply_rot);
            ImGui::Checkbox("Show grid", &show_mesh); ImGui::SameLine();
            ImGui::Checkbox("Dense", &show_mesh_dense);
            ImGui::Checkbox("Flip X", &lm_flip_x); ImGui::SameLine();
            ImGui::Checkbox("Flip Y", &lm_flip_y); ImGui::SameLine();
            ImGui::Checkbox("Swap XY", &lm_swap_xy);
          }
        }

        // Skin Smoothing (main section)
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Skin Smoothing")) {
          if (opencl_available) {
            bool prev = use_opencl;
            ImGui::Checkbox("OpenCL accel (T-API)", &use_opencl);
            if (use_opencl != prev) cv::ocl::setUseOpenCL(use_opencl);
            ImGui::SameLine(); ImGui::TextDisabled("%s", use_opencl?"on":"off");
          } else {
            ImGui::TextDisabled("OpenCL not available");
          }
          ImGui::SameLine();
          ImGui::Checkbox("Perf log", &perf_log);
          if (perf_log) { ImGui::SameLine(); ImGui::SliderInt("Interval (ms)", &perf_log_interval_ms, 500, 10000); }
          ImGui::Checkbox("Enable##skin", &fx_skin); ImGui::SameLine();
          ImGui::Checkbox("Advanced", &fx_skin_adv);
          if (fx_skin) {
            if (!fx_skin_adv) {
              ImGui::SliderFloat("Strength", &fx_skin_strength, 0.0f, 1.0f);
            } else {
              ImGui::SliderFloat("Amount##skin", &fx_skin_amount, 0.0f, 1.0f);
              ImGui::SliderFloat("Radius (px)", &fx_skin_radius, 1.0f, 20.0f);
              ImGui::SliderFloat("Texture keep (0..1)", &fx_skin_tex, 0.05f, 1.0f);
              ImGui::SliderFloat("Edge feather (px)", &fx_skin_edge, 2.0f, 40.0f);
              ImGui::SliderFloat("Processing scale", &fx_adv_scale, 0.5f, 1.0f);

              ImGui::Separator();
              ImGui::Checkbox("Wrinkle-aware", &fx_skin_wrinkle);
              if (fx_skin_wrinkle) {
                ImGui::Indent();
                // Sensitivity & boosts
                ImGui::SliderFloat("Wrinkle gain", &fx_skin_wrinkle_gain, 0.0f, 8.0f);
                ImGui::SliderFloat("Smile boost", &fx_skin_smile_boost, 0.0f, 1.0f);
                ImGui::SliderFloat("Squint boost", &fx_skin_squint_boost, 0.0f, 1.0f);
                ImGui::SliderFloat("Forehead boost", &fx_skin_forehead_boost, 0.0f, 2.0f);

                ImGui::Separator();
                // Region controls
                ImGui::Checkbox("Suppress chin/stubble", &fx_wrinkle_suppress_lower);
                if (fx_wrinkle_suppress_lower) {
                  ImGui::SliderFloat("Lower-face ratio", &fx_wrinkle_lower_ratio, 0.25f, 0.65f);
                }
                ImGui::Checkbox("Ignore glasses", &fx_wrinkle_ignore_glasses);
                if (fx_wrinkle_ignore_glasses) {
                  // No extra indent: align with other sliders in this section
                  ImGui::SliderFloat("Glasses margin (px)", &fx_wrinkle_glasses_margin, 0.0f, 30.0f);
                }

                ImGui::Separator();
                // Advanced masking
                ImGui::SliderFloat("Wrinkle sensitivity", &fx_wrinkle_keep_ratio, 0.05f, 10.60f);
                ImGui::Checkbox("Custom line width", &fx_wrinkle_custom_scales);
                if (fx_wrinkle_custom_scales) {
                  ImGui::SliderFloat("Min width (px)", &fx_wrinkle_min_px, 1.0f, 10.0f);
                  ImGui::SliderFloat("Max width (px)", &fx_wrinkle_max_px, 2.0f, 16.0f);
                  if (fx_wrinkle_max_px < fx_wrinkle_min_px) fx_wrinkle_max_px = fx_wrinkle_min_px;
                }
                ImGui::Checkbox("Skin gate (YCbCr)", &fx_wrinkle_use_skin_gate);
                if (fx_wrinkle_use_skin_gate) {
                  ImGui::Indent();
                  ImGui::SliderFloat("Mask gain", &fx_wrinkle_mask_gain, 0.5f, 3.0f);
                  ImGui::Unindent();
                }
                ImGui::SliderFloat("Baseline boost", &fx_wrinkle_baseline, 0.0f, 1.0f);
                ImGui::SliderFloat("Neg atten cap", &fx_wrinkle_neg_cap, 0.6f, 1.0f);

                ImGui::Separator();
                ImGui::Checkbox("Wrinkle-only preview", &fx_wrinkle_preview);
                ImGui::Checkbox("Show wrinkle mask", &dbg_wrinkle_mask); ImGui::SameLine();
                ImGui::Checkbox("Show stats", &dbg_wrinkle_stats);
                ImGui::Unindent();
              }
            }
          }
        }

        // Lips
        ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Lips")) {
          ImGui::Checkbox("Lip refiner", &fx_lipstick);
          if (fx_lipstick) {
            ImGui::ColorEdit3("Lip color", fx_lip_color);
            ImGui::SliderFloat("Amount##lip", &fx_lip_alpha, 0.0f, 1.0f);
            ImGui::SliderFloat("Feather (px)", &fx_lip_feather, 0.0f, 20.0f);
            ImGui::SliderFloat("Band grow (px)", &fx_lip_band, 0.0f, 12.0f);
            ImGui::SliderFloat("Lightness", &fx_lip_light, -1.0f, 1.0f);
          }
        }

        // Teeth
        ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
        if (ImGui::CollapsingHeader("Teeth Whitening")) {
          ImGui::Checkbox("Enable##teeth", &fx_teeth);
          if (fx_teeth) {
            ImGui::SliderFloat("Amount##teeth", &fx_teeth_strength, 0.0f, 1.0f);
            ImGui::SliderFloat("Avoid lips (px)", &fx_teeth_margin, 0.0f, 12.0f);
          }
        }
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

  // Ensure virtual webcam is properly closed on exit
  if (vcam_running) { close_vcam(); }
  ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext();
  if (tex) glDeleteTextures(1, &tex);
  SDL_GL_DeleteContext(gl_context); SDL_DestroyWindow(window); SDL_Quit();

  { auto st = graph.CloseInputStream("input_video"); if (!st.ok()) std::fprintf(stderr, "CloseInputStream failed: %s\n", st.message().data()); }
  { auto st = graph.WaitUntilDone(); if (!st.ok()) std::fprintf(stderr, "WaitUntilDone failed: %s\n", st.message().data()); }
  return 0;
}
