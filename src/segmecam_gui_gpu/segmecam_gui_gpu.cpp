// SDL2 + OpenGL + ImGui rendering of MediaPipe GPU segmentation (mask on CPU, composite on CPU)

#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <opencv2/opencv.hpp>
#include <cstdio>
#include <string>
#include <iostream>

#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"
#include "segmecam_composite.h"

namespace mp = mediapipe;

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
  std::string graph_path = "mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt";  // from project root
  std::string resource_root_dir = ".";  // MediaPipe repo root
  int cam_index = 0;
  if (argc > 1) graph_path = argv[1];
  if (argc > 2) resource_root_dir = argv[2];
  if (argc > 3) cam_index = std::atoi(argv[3]);

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
  ImGui::SetNextWindowPos(ImVec2(16,16), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(360,100), ImGuiCond_Always);
  if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
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

    // Compose display frame
    cv::Mat display_rgb;
    // Ensure mask matches frame size
    cv::Mat mask8_resized;
    if (!last_mask_u8.empty() && (last_mask_u8.cols != frame_bgr.cols || last_mask_u8.rows != frame_bgr.rows)) {
      cv::resize(last_mask_u8, mask8_resized, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
    } else if (!last_mask_u8.empty()) {
      mask8_resized = last_mask_u8;
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
    // Controls pinned top-left
    ImGui::SetNextWindowPos(ImVec2(16,16), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(360, 160), ImGuiCond_Always);
    if (ImGui::Begin("SegmeCam", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
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
