#include "mediapipe/examples/desktop/segmecam/include/application/application_run.h"
#include "mediapipe/examples/desktop/segmecam/include/effects/effects_manager.h"
#include "mediapipe/examples/desktop/segmecam/segmecam_face_effects.h"

// Include MediaPipe for output stream polling
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"

// Include ImGui for GUI
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h" 
#include "third_party/imgui/backends/imgui_impl_opengl3.h"

// Include UIManager Enhanced for comprehensive panels
#include "include/ui/ui_manager_enhanced.h"

// Include OpenCV for camera capture and image processing
#include <opencv2/opencv.hpp>

// Include for precision formatting
#include <iomanip>

// Include segmentation composite functions for proper mask decoding
#include "segmecam_composite.h"

#include "include/application/portal_utils.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <vector> // NOLINT - Standard library header resolved by build system

namespace segmecam {

// Helper function to sync app_state settings to EffectsManager
void ApplicationRun::SyncSettingsToEffectsManager(EffectsManager& effects_manager, const AppState& app_state) {
    // Background effects settings
    effects_manager.SetBackgroundMode(app_state.bg_mode);
    effects_manager.SetBlurStrength(app_state.blur_strength);
    effects_manager.SetFeatherAmount(app_state.feather_px);
    effects_manager.SetSolidBackgroundColor(app_state.solid_color[0], app_state.solid_color[1], app_state.solid_color[2]);
    effects_manager.SetShowMask(app_state.show_mask);
    effects_manager.SetShowLandmarks(app_state.show_landmarks);
    
    // Sync background image if available
    if (!app_state.bg_image.empty()) {
        effects_manager.SetBackgroundImage(app_state.bg_image);
    }
    
    // Beauty effects settings
    effects_manager.SetSkinSmoothingEnabled(app_state.fx_skin);
    effects_manager.SetSkinSmoothingStrength(app_state.fx_skin_strength);
    effects_manager.SetSkinSmoothingAdvanced(app_state.fx_skin_adv);
    effects_manager.SetSkinSmoothingAmount(app_state.fx_skin_amount);
    effects_manager.SetSkinSmoothingRadius(app_state.fx_skin_radius);
    effects_manager.SetSkinTexturePreservation(app_state.fx_skin_tex);
    effects_manager.SetSkinEdgeFeather(app_state.fx_skin_edge);
    
    // Wrinkle-aware settings
    effects_manager.SetWrinkleAwareEnabled(app_state.fx_skin_wrinkle);
    effects_manager.SetWrinkleGain(app_state.fx_skin_wrinkle_gain);
    effects_manager.SetSmileBoost(app_state.fx_skin_smile_boost);
    effects_manager.SetSquintBoost(app_state.fx_skin_squint_boost);
    effects_manager.SetForeheadBoost(app_state.fx_skin_forehead_boost);
    effects_manager.SetSuppressLowerFace(app_state.fx_wrinkle_suppress_lower);
    effects_manager.SetLowerFaceRatio(app_state.fx_wrinkle_lower_ratio);
    effects_manager.SetIgnoreGlasses(app_state.fx_wrinkle_ignore_glasses);
    effects_manager.SetGlassesMargin(app_state.fx_wrinkle_glasses_margin);
    effects_manager.SetWrinkleSensitivity(app_state.fx_wrinkle_keep_ratio);
    effects_manager.SetCustomWrinkleScales(app_state.fx_wrinkle_custom_scales);
    effects_manager.SetWrinkleMinWidth(app_state.fx_wrinkle_min_px);
    effects_manager.SetWrinkleMaxWidth(app_state.fx_wrinkle_max_px);
    effects_manager.SetWrinkleSkinGate(app_state.fx_wrinkle_use_skin_gate);
    effects_manager.SetWrinkleMaskGain(app_state.fx_wrinkle_mask_gain);
    effects_manager.SetWrinkleBaselineBoost(app_state.fx_wrinkle_baseline);
    effects_manager.SetWrinkleNegativeCap(app_state.fx_wrinkle_neg_cap);
    effects_manager.SetWrinklePreview(app_state.fx_wrinkle_preview);
    
    // Processing scale settings
    effects_manager.SetProcessingScale(app_state.fx_adv_scale);
    effects_manager.SetDetailPreservation(app_state.fx_adv_detail_preserve);
    
    // Auto processing scale settings
    effects_manager.SetAutoProcessingScaleEnabled(app_state.auto_processing_scale);
    effects_manager.SetTargetFPS(app_state.target_fps);
    
    // Lip effects settings
    effects_manager.SetLipstickEnabled(app_state.fx_lipstick);
    effects_manager.SetLipAlpha(app_state.fx_lip_alpha);
    effects_manager.SetLipFeather(app_state.fx_lip_feather);
    effects_manager.SetLipLightness(app_state.fx_lip_light);
    effects_manager.SetLipBandGrow(app_state.fx_lip_band);
    effects_manager.SetLipColor(app_state.fx_lip_color[0], app_state.fx_lip_color[1], app_state.fx_lip_color[2]);
    
    // Teeth whitening settings
    effects_manager.SetTeethWhiteningEnabled(app_state.fx_teeth);
    effects_manager.SetTeethWhiteningStrength(app_state.fx_teeth_strength);
    effects_manager.SetTeethMargin(app_state.fx_teeth_margin);
}

// Helper function to sync status FROM EffectsManager back TO app_state
void ApplicationRun::SyncStatusFromEffectsManager(const EffectsManager& effects_manager, AppState& app_state) {
    // Sync OpenCL availability status (detected by EffectsManager)
    bool prev_opencl_available = app_state.opencl_available;
    app_state.opencl_available = effects_manager.IsOpenCLAvailable();
    
    // Enable OpenCL by default when first detected as available
    if (!prev_opencl_available && app_state.opencl_available) {
        app_state.use_opencl = true;
        std::cout << "OpenCL detected and enabled by default for acceleration" << std::endl;
    }
}

// Helper function to convert MediaPipe ImageFrame to OpenCV Mat
void ApplicationRun::MatToImageFrame(const cv::Mat& mat_bgr, std::unique_ptr<mediapipe::ImageFrame>& frame) {
    frame = std::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, mat_bgr.cols, mat_bgr.rows);
    cv::Mat frame_rgb;
    cv::cvtColor(mat_bgr, frame_rgb, cv::COLOR_BGR2RGB);
    frame_rgb.copyTo(cv::Mat(frame->Height(), frame->Width(), CV_8UC3, frame->MutablePixelData()));
}

bool ApplicationRun::ProcessEvents(bool& running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_QUIT) {
            running = false;
            return false;
        }
    }
    return true;
}

void ApplicationRun::UpdateFPSTracking(double& fps, uint64_t& fps_frames, uint32_t& fps_last_ms) {
    fps_frames++;
    uint32_t now_ms = SDL_GetTicks();
    if (now_ms - fps_last_ms >= 500) {
        fps = (double)fps_frames * 1000.0 / (double)(now_ms - fps_last_ms);
        fps_frames = 0;
        fps_last_ms = now_ms;
    }
}

GLuint ApplicationRun::CreateVideoTexture(const cv::Mat& display_rgb) {
    GLuint video_texture = 0;
    if (!display_rgb.empty()) {
        // Debug output for texture upload
        static int texture_debug_count = 0;
        texture_debug_count++;
        if (texture_debug_count <= 3) {
            cv::Vec3b center_pixel = display_rgb.at<cv::Vec3b>(display_rgb.rows/2, display_rgb.cols/2);
            std::cout << "🔍 TEXTURE UPLOAD " << texture_debug_count << " - format: " << display_rgb.type() 
                      << " center pixel RGB: [" << (int)center_pixel[0] << "," << (int)center_pixel[1] << "," << (int)center_pixel[2] << "]" << std::endl;
        }
        
        // Ensure GL interprets tightly packed RGB rows (like original)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        
        glGenTextures(1, &video_texture);
        glBindTexture(GL_TEXTURE_2D, video_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, display_rgb.cols, display_rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, display_rgb.data);
    }
    return video_texture;
}

void ApplicationRun::RenderVideoBackground(const cv::Mat& display_rgb, int window_width, int window_height) {
    // Calculate aspect-preserving dimensions to fill window
    float video_aspect = (float)display_rgb.cols / (float)display_rgb.rows;
    float window_aspect = (float)window_width / (float)window_height;
    
    float quad_w, quad_h, quad_x, quad_y;
    if (window_aspect > video_aspect) {
        // Window is wider - scale to height, center horizontally
        quad_h = window_height;
        quad_w = quad_h * video_aspect;
        quad_x = (window_width - quad_w) * 0.5f;
        quad_y = 0;
    } else {
        // Window is taller - scale to width, center vertically  
        quad_w = window_width;
        quad_h = quad_w / video_aspect;
        quad_x = 0;
        quad_y = (window_height - quad_h) * 0.5f;
    }
    
    // Set up orthographic projection for fullscreen quad (like original)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_width, window_height, 0, -1, 1);  // Y-flipped for texture coordinates
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Enable texturing and proper blending
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set white color (full brightness)
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Draw fullscreen textured quad (matching original coordinate system)
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(quad_x, quad_y);
    glTexCoord2f(1, 0); glVertex2f(quad_x + quad_w, quad_y);
    glTexCoord2f(1, 1); glVertex2f(quad_x + quad_w, quad_y + quad_h);
    glTexCoord2f(0, 1); glVertex2f(quad_x, quad_y + quad_h);
    glEnd();
    
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void ApplicationRun::RenderComprehensiveUI(ManagerCoordination::Managers& managers,
                                          const AppState& app_state, 
                                          UIManager& ui_manager,
                                          bool& running) {
    // Use UIManager Enhanced for comprehensive panels
    ui_manager.ProcessEvents(running);
    ui_manager.BeginFrame();
    ui_manager.RenderUI();
    ui_manager.EndFrame();
}

int ApplicationRun::ExecuteMainLoop(
    ManagerCoordination::Managers& managers,
    std::unique_ptr<mediapipe::CalculatorGraph>& mediapipe_graph,
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    SDL_Window* window,
    AppState& app_state
) {
    std::cout << "🎥 Starting main application loop..." << std::endl;
    
    // Check if face landmarks are available
    bool has_landmarks = (multi_face_landmarks_poller != nullptr);
    if (has_landmarks) {
        std::cout << "✅ Face landmarks pollers available for processing" << std::endl;
    } else {
        std::cout << "ℹ️  Face landmarks not enabled for this session" << std::endl;
    }
    
    // Initialize UIManager Enhanced
    UIManager ui_manager;
    if (!InitializeApplication(managers, window, app_state, ui_manager)) {
        return -1;
    }
    
    // Initialize application state
    bool running = true;
    int64_t frame_id = 0;
    
    // FPS tracking
    double fps = 0.0;
    uint64_t fps_frames = 0;
    uint32_t fps_last_ms = SDL_GetTicks();
    
    std::cout << "✅ Main loop initialized, starting frame processing..." << std::endl;
    
    // Run the main processing loop
    MainLoopParams main_params = {
        managers, mediapipe_graph, mask_poller, multi_face_landmarks_poller, 
        face_rects_poller, window, app_state, ui_manager, running, frame_id, 
        fps, fps_frames, fps_last_ms, has_landmarks
    };
    RunMainLoop(main_params);
    
    std::cout << "🛑 Main loop ended" << std::endl;
    return 0;
}

void ApplicationRun::RunMainLoop(MainLoopParams& params) {
    int frame_count = 0;
    
    // Main application loop
    while (params.running) {
        frame_count++;
        
        try {
            FrameProcessingParams frame_params = {
                params.managers, params.mediapipe_graph, params.mask_poller, params.multi_face_landmarks_poller, 
                params.face_rects_poller, params.window, params.app_state, params.ui_manager, params.frame_id, params.fps, 
                params.fps_frames, params.fps_last_ms, frame_count, params.has_landmarks, params.running
            };
            
            if (!ProcessFrame(frame_params)) {
                break; // Exit on error
            }
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Exception in main loop: " << e.what() << std::endl;
            break;
        } catch (...) {
            std::cerr << "❌ Unknown exception in main loop" << std::endl;
            break;
        }
    }
}

bool ApplicationRun::ProcessFrameCapture(CameraManager& camera_mgr, cv::Mat& frame_bgr, int frame_count) {
    if (frame_count <= 5) {
        std::cout << "📸 Calling CaptureFrame()..." << std::endl;
    }
    if (!camera_mgr.CaptureFrame(frame_bgr) || frame_bgr.empty()) {
        if (frame_count < 10) {  // Only log first few failures
            std::cout << "⚠️  Frame capture failed or empty on frame " << frame_count << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return false;
    }
    
    if (frame_count <= 5) {
        std::cout << "✅ Frame " << frame_count << " captured successfully: " << frame_bgr.cols << "x" << frame_bgr.rows << std::endl;
        // Debug: Show camera frame format and sample pixel values
        if (!frame_bgr.empty()) {
            cv::Vec3b center_pixel = frame_bgr.at<cv::Vec3b>(frame_bgr.rows/2, frame_bgr.cols/2);
            cv::Vec3b corner_pixel = frame_bgr.at<cv::Vec3b>(10, 10);
            std::cout << "🔍 CAMERA DEBUG - Frame format: " << frame_bgr.type() << " (CV_8UC3=" << CV_8UC3 << ")" << std::endl;
            std::cout << "🔍 CAMERA DEBUG - Center pixel BGR(" << frame_bgr.rows/2 << "," << frame_bgr.cols/2 << "): [" 
                      << (int)center_pixel[0] << "," << (int)center_pixel[1] << "," << (int)center_pixel[2] << "]" << std::endl;
            std::cout << "🔍 CAMERA DEBUG - Corner pixel BGR(10,10): [" 
                      << (int)corner_pixel[0] << "," << (int)corner_pixel[1] << "," << (int)corner_pixel[2] << "]" << std::endl;
        }
    }
    return true;
}

void ApplicationRun::ProcessMediaPipeOutputs(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    AppState& app_state,
    int frame_count,
    bool has_landmarks
) {
    // Process mask output
    ProcessMaskOutput(output_data, mask_poller, app_state, frame_count);
    
    // Process face landmarks if available
    if (has_landmarks && multi_face_landmarks_poller) {
        ProcessFaceLandmarks(output_data, multi_face_landmarks_poller, face_rects_poller, frame_count);
    }
}

void ApplicationRun::ProcessMaskOutput(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& mask_poller,
    AppState& app_state,
    int frame_count
) {
    mediapipe::Packet pkt;
    while (mask_poller->QueueSize() > 0 && mask_poller->Next(&pkt)) {
        const auto& mask = pkt.Get<mediapipe::ImageFrame>();
        static bool first_mask_info = false;
        output_data.last_mask_u8 = DecodeMaskToU8(mask, &first_mask_info);
        app_state.last_mask_u8 = output_data.last_mask_u8.clone();
        
        if (frame_count <= 5) {
            double min_val, max_val;
            cv::minMaxLoc(output_data.last_mask_u8, &min_val, &max_val);
            std::cout << "✅ Mask received: " << mask.Width() << "x" << mask.Height() 
                      << " (format: " << mask.NumberOfChannels() << "ch, " << mask.ByteDepth() << "bd -> 8UC1: " << min_val << "-" << max_val << ")" << std::endl;
        }
    }
}

void ApplicationRun::ProcessFaceLandmarks(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    int frame_count
) {
    try {
        ProcessFaceLandmarksData(output_data, multi_face_landmarks_poller, face_rects_poller, frame_count);
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception during landmarks polling: " << e.what() << std::endl;
    }
}

void ApplicationRun::ProcessFaceLandmarksData(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller,
    int frame_count
) {
    // Process landmark packets
    ProcessLandmarkPackets(output_data, multi_face_landmarks_poller, frame_count);

    // Process face rects if available
    if (face_rects_poller) {
        ProcessFaceRects(output_data, face_rects_poller);
    }
}

void ApplicationRun::ProcessLandmarkPackets(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& multi_face_landmarks_poller,
    int frame_count
) {
    mediapipe::Packet lp;
    int queue_size = multi_face_landmarks_poller->QueueSize();
    if (frame_count <= 5) {
        std::cout << "📍 Landmarks queue size: " << queue_size << std::endl;
    }

    while (queue_size > 0 && multi_face_landmarks_poller->Next(&lp)) {
        try {
            const auto& v = lp.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
            if (!v.empty()) {
                output_data.latest_lms = v[0];
                output_data.have_lms = true;
                if (frame_count <= 5) {
                    std::cout << "✅ Got landmarks with " << output_data.latest_lms.landmark_size() << " points" << std::endl;
                }
            }
            queue_size = multi_face_landmarks_poller->QueueSize();
        } catch (const std::exception& e) {
            std::cerr << "❌ Error processing landmarks packet: " << e.what() << std::endl;
            break;
        }
    }
}

void ApplicationRun::ProcessFaceRects(
    MediaPipeOutputData& output_data,
    std::unique_ptr<mediapipe::OutputStreamPoller>& face_rects_poller
) {
    mediapipe::Packet rp;
    while (face_rects_poller->QueueSize() > 0 && face_rects_poller->Next(&rp)) {
        output_data.latest_rects = rp.Get<std::vector<mediapipe::NormalizedRect>>();
    }
}

cv::Mat ApplicationRun::ProcessAndDisplayFrame(
    const cv::Mat& frame_bgr,
    const cv::Mat& last_mask_u8,
    const mediapipe::NormalizedLandmarkList* landmarks_ptr,
    EffectsManager& effects_mgr,
    AppState& app_state,
    bool have_lms) {
    
    cv::Mat processed_frame = frame_bgr;
    
    // Apply effects if EffectsManager is available
    if (!last_mask_u8.empty() || have_lms) {
        // Use EffectsManager to process the frame with segmentation mask and face landmarks
        processed_frame = effects_mgr.ProcessFrame(frame_bgr, last_mask_u8, landmarks_ptr);
    }
    
    // EffectsManager now returns RGB directly, no conversion needed
    cv::Mat display_rgb = processed_frame.clone();
    
    // Update app state with current frame
    app_state.last_display_rgb = display_rgb.clone();
    
    return display_rgb;
}

void ApplicationRun::HandleVirtualCameraOutput(
    CameraManager& camera_mgr,
    AppState& app_state,
    const cv::Mat& display_rgb) {
    
    if (!app_state.vcam.IsOpen() || display_rgb.empty()) {
        return;
    }
    
    // Check if frame size matches vcam, reopen if needed
    if (display_rgb.cols != app_state.vcam.Width() || display_rgb.rows != app_state.vcam.Height()) {
        // Get virtual camera list and reopen with correct size
        auto vcam_list = camera_mgr.GetVCamList();
        if (app_state.ui_vcam_idx >= 0 && app_state.ui_vcam_idx < (int)vcam_list.size()) {
            app_state.vcam.Open(vcam_list[app_state.ui_vcam_idx].path, display_rgb.cols, display_rgb.rows);
        }
    }
    
    // Convert RGB to BGR and write to virtual camera
    cv::Mat display_bgr;
    cv::cvtColor(display_rgb, display_bgr, cv::COLOR_RGB2BGR);
    app_state.vcam.WriteBGR(display_bgr);
}

void ApplicationRun::HandleDroppedFiles(
    UIManager& ui_manager,
    AppState& app_state) {
    
    auto dropped_files = ui_manager.GetDroppedFiles();
    for (const auto& file_path : dropped_files) {
        std::cout << "🖼️  Processing dropped file: " << file_path << std::endl;
        cv::Mat img;
        std::string resolved_path;
        if (LoadBackgroundImageWithPortal(file_path, img, resolved_path)) {
            app_state.bg_image = img.clone();
            app_state.bg_mode = 2; // Automatically switch to Image mode (0=None, 1=Blur, 2=Image, 3=Solid)
            // Update the background path for profile persistence
            // Safe string copy with null-termination guarantee
            if (!resolved_path.empty()) {
                std::size_t copy_len = std::min(resolved_path.length(), sizeof(app_state.bg_path_buf) - 1);
                std::memcpy(app_state.bg_path_buf, resolved_path.c_str(), copy_len);
                app_state.bg_path_buf[copy_len] = '\0';
            } else {
                app_state.bg_path_buf[0] = '\0'; // Ensure null-termination even for empty string
            }
            std::cout << "✅ Background image loaded: " << img.cols << "x" << img.rows
                      << " (auto-switched to Image mode)" << std::endl;
            std::cout << "🔖 Background path saved: " << app_state.bg_path_buf << std::endl;
        } else {
            std::cout << "❌ Failed to load dropped file as image after portal fallback." << std::endl;
        }
    }
}

void ApplicationRun::RenderFrame(
    UIManager& ui_manager,
    const cv::Mat& display_rgb,
    SDL_Window* window,
    int frame_count,
    bool& running
) {
    // Get window size for rendering
    int dw, dh;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);
    
    // Clear screen with dark background
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Create OpenGL texture from processed frame for background display
    GLuint video_texture = CreateVideoTexture(display_rgb);
    
    // Render camera feed as fullscreen background
    if (video_texture && !display_rgb.empty()) {
        glBindTexture(GL_TEXTURE_2D, video_texture);
        RenderVideoBackground(display_rgb, dw, dh);
    }
    
    // Begin ImGui frame and render UI panels on top
    ui_manager.BeginFrame();
    ui_manager.RenderUI();
    ui_manager.EndFrame();
    
    if (frame_count <= 5) {
        std::cout << "✅ UI rendered successfully for frame " << frame_count << std::endl;
    }
    
    // Clean up texture
    if (video_texture) {
        glDeleteTextures(1, &video_texture);
    }
}

bool ApplicationRun::InitializeApplication(
    ManagerCoordination::Managers& managers,
    SDL_Window* window,
    AppState& app_state,
    UIManager& ui_manager
) {
    // Initialize UIManager Enhanced for comprehensive panels
    if (!ui_manager.Initialize(window)) {
        std::cerr << "❌ Failed to initialize UIManager Enhanced" << std::endl;
        return false;
    }
    
    // Initialize UI panels with dependencies
    ui_manager.InitializePanels(app_state, *managers.camera, *managers.effects, managers.config.get());
    std::cout << "✅ UIManager Enhanced initialized successfully" << std::endl;
    
    // Debug: Check camera manager state
    if (!managers.camera) {
        std::cerr << "❌ Camera manager is null!" << std::endl;
        return false;
    }
    std::cout << "✅ Camera manager is valid" << std::endl;
    
    return true;
}

bool ApplicationRun::ProcessFrame(FrameProcessingParams& params) {
    cv::Mat frame_bgr;
    cv::Mat display_rgb;

    // Process frame capture and initial updates
    if (!ProcessFrameCaptureAndUpdates(params, frame_bgr)) {
        return true; // Continue to next frame
    }

    // Process MediaPipe and effects
    if (!ProcessFrameMediaPipeAndEffects(params, frame_bgr, display_rgb)) {
        return false; // Exit on error
    }

    // Handle UI events and rendering
    return ProcessFrameUIAndRender(params, display_rgb);
}

bool ApplicationRun::ProcessFrameCaptureAndUpdates(FrameProcessingParams& params, cv::Mat& frame_bgr) {
    // Process frame capture
    if (!ProcessFrameCapture(*params.managers.camera, frame_bgr, params.frame_count)) {
        return false; // Continue to next frame
    }

    // Update FPS tracking
    UpdateFPSTracking(params.fps, params.fps_frames, params.fps_last_ms);

    // Update camera information in app state
    UpdateCameraInfo(params);

    // Update auto FPS settings if camera changed
    UpdateAutoFPS(params);

    // Update auto processing scale if enabled
    UpdateAutoProcessingScale(params);

    return true;
}

bool ApplicationRun::ProcessFrameMediaPipeAndEffects(FrameProcessingParams& params, const cv::Mat& frame_bgr, cv::Mat& display_rgb) {
    // Send frame to MediaPipe graph
    if (!SendFrameToMediaPipe(frame_bgr, params)) {
        return false; // Exit on error
    }

    // Process MediaPipe outputs
    MediaPipeOutputData output_data;
    ProcessMediaPipeOutputs(output_data, params.mask_poller, params.multi_face_landmarks_poller, params.face_rects_poller,
                           params.app_state, params.frame_count, params.has_landmarks);

    // Process frame effects and prepare for display
    const mediapipe::NormalizedLandmarkList* landmarks_ptr = (output_data.have_lms) ? &output_data.latest_lms : nullptr;
    display_rgb = ProcessAndDisplayFrame(frame_bgr, output_data.last_mask_u8, landmarks_ptr,
                                        *params.managers.effects, params.app_state, output_data.have_lms);

    // Handle virtual camera output
    HandleVirtualCameraOutput(*params.managers.camera, params.app_state, display_rgb);

    return true;
}

bool ApplicationRun::ProcessFrameUIAndRender(FrameProcessingParams& params, const cv::Mat& display_rgb) {
    // Let UIManager handle events first
    if (!params.ui_manager.ProcessEvents(params.running)) {
        std::cout << "🛑 UIManager ProcessEvents returned false, exiting..." << std::endl;
        return false;
    }

    // Handle any dropped files
    HandleDroppedFiles(params.ui_manager, params.app_state);

    if (!params.running) {
        std::cout << "🛑 Running flag set to false by event handler, exiting..." << std::endl;
        return false;
    }

    // Render complete frame
    RenderFrame(params.ui_manager, display_rgb, params.window, params.frame_count, params.running);

    return true;
}

void ApplicationRun::UpdateCameraInfo(FrameProcessingParams& params) {
    params.app_state.fps = params.fps;
    params.app_state.camera_width = params.managers.camera->GetCurrentWidth();
    params.app_state.camera_height = params.managers.camera->GetCurrentHeight();
    params.app_state.camera_fps = params.managers.camera->GetCurrentFPS();
}

void ApplicationRun::UpdateAutoFPS(FrameProcessingParams& params) {
    static float last_camera_fps = -1.0f;
    if (std::abs(params.app_state.camera_fps - last_camera_fps) > 0.1f) {
        params.managers.effects->UpdateTargetFPSFromCamera(params.app_state.camera_fps);
        params.app_state.target_fps = params.managers.effects->GetTargetFPS();
        last_camera_fps = params.app_state.camera_fps;
    }
}

void ApplicationRun::UpdateAutoProcessingScale(FrameProcessingParams& params) {
    if (params.app_state.auto_processing_scale && params.fps > 0.0) {
        params.managers.effects->UpdateAutoProcessingScale(params.fps);
        params.app_state.current_fps = params.managers.effects->GetCurrentFPS();
        params.app_state.fx_adv_scale = params.managers.effects->GetProcessingScale();
    }
}

bool ApplicationRun::SendFrameToMediaPipe(const cv::Mat& frame_bgr, FrameProcessingParams& params) {
    std::unique_ptr<mediapipe::ImageFrame> frame;
    MatToImageFrame(frame_bgr, frame);
    auto ts = mediapipe::Timestamp(params.frame_id++);
    auto st = params.mediapipe_graph->AddPacketToInputStream("input_video", mediapipe::Adopt(frame.release()).At(ts));
    if (!st.ok()) {
        std::cerr << "❌ AddPacket failed: " << st.message() << std::endl;
        return false;
    }
    return true;
}

} // namespace segmecam
