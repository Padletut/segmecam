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
#include <vector>

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
    
    // Check if face landmarks are available (multi_face_landmarks is required, face_rects is optional)
    bool has_landmarks = (multi_face_landmarks_poller != nullptr);
    if (has_landmarks) {
        std::cout << "✅ Face landmarks pollers available for processing" << std::endl;
    } else {
        std::cout << "ℹ️  Face landmarks not enabled for this session" << std::endl;
    }
    
    // Initialize UIManager Enhanced for comprehensive panels
    UIManager ui_manager;
    if (!ui_manager.Initialize(window)) {
        std::cerr << "❌ Failed to initialize UIManager Enhanced" << std::endl;
        return -1;
    }
    
    // Initialize UI panels with dependencies
    ui_manager.InitializePanels(app_state, *managers.camera, *managers.effects, managers.config.get());
    std::cout << "✅ UIManager Enhanced initialized successfully" << std::endl;
    
    // Initialize application state
    bool running = true;
    int64_t frame_id = 0;
    cv::Mat last_mask_u8;
    cv::Mat last_display_rgb;
    
    // FPS tracking
    double fps = 0.0;
    uint64_t fps_frames = 0;
    uint32_t fps_last_ms = SDL_GetTicks();
    
    std::cout << "✅ Main loop initialized, starting frame processing..." << std::endl;
    
    // Debug: Check camera manager state
    if (!managers.camera) {
        std::cerr << "❌ Camera manager is null!" << std::endl;
        return -1;
    }
    std::cout << "✅ Camera manager is valid" << std::endl;
    
    int frame_count = 0;
    
    // Main application loop
    while (running) {
        if (frame_count <= 5) {
            std::cout << "🔄 Starting frame " << frame_count + 1 << " processing..." << std::endl;
        }
        
        try {
            frame_count++;
            
            if (frame_count <= 5) {
                std::cout << "📸 Attempting to capture frame " << frame_count << "..." << std::endl;
            }
            
            // Capture frame from camera using CameraManager
            cv::Mat frame_bgr;
            if (frame_count <= 5) {
                std::cout << "📸 Calling CaptureFrame()..." << std::endl;
            }
            if (!managers.camera->CaptureFrame(frame_bgr) || frame_bgr.empty()) {
                if (frame_count < 10) {  // Only log first few failures
                    std::cout << "⚠️  Frame capture failed or empty on frame " << frame_count << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
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
        
            // Update FPS tracking
            UpdateFPSTracking(fps, fps_frames, fps_last_ms);
            
            // Update app state with current frame and camera information
        app_state.fps = fps;
        app_state.camera_width = managers.camera->GetCurrentWidth();
        app_state.camera_height = managers.camera->GetCurrentHeight();
        app_state.camera_fps = managers.camera->GetCurrentFPS();
        
        // Auto-update target FPS based on camera settings
        static float last_camera_fps = -1.0f;
        if (std::abs(app_state.camera_fps - last_camera_fps) > 0.1f) {
            managers.effects->UpdateTargetFPSFromCamera(app_state.camera_fps);
            app_state.target_fps = managers.effects->GetTargetFPS();
            last_camera_fps = app_state.camera_fps;
        }
        
        // Update auto processing scale if enabled
        if (app_state.auto_processing_scale && fps > 0.0) {
            managers.effects->UpdateAutoProcessingScale(fps);
            // Update app state with current values for UI display
            app_state.current_fps = managers.effects->GetCurrentFPS();
            app_state.fx_adv_scale = managers.effects->GetProcessingScale();
        }
        
        // Send frame to MediaPipe graph
        {
            if (frame_count <= 5) {
                std::cout << "📤 Sending frame " << frame_count << " to MediaPipe..." << std::endl;
            }
            std::unique_ptr<mediapipe::ImageFrame> frame;
            MatToImageFrame(frame_bgr, frame);
            auto ts = mediapipe::Timestamp(frame_id++);
            auto st = mediapipe_graph->AddPacketToInputStream("input_video", mediapipe::Adopt(frame.release()).At(ts));
            if (!st.ok()) {
                std::cerr << "❌ AddPacket failed: " << st.message() << std::endl;
                break;
            }
            if (frame_count <= 5) {
                std::cout << "✅ Frame " << frame_count << " sent to MediaPipe successfully" << std::endl;
            }
            
            if (frame_count <= 5) {
                std::cout << "✅ Frame " << frame_count << " sent, waiting before polling..." << std::endl;
            }
        }
        
        // Poll for mask output (non-blocking)
        mediapipe::Packet pkt;
        while (mask_poller->QueueSize() > 0 && mask_poller->Next(&pkt)) {
            const auto& mask = pkt.Get<mediapipe::ImageFrame>();
            
            // Use the proper mask decoding function (same as original implementation)
            static bool first_mask_info = false;
            last_mask_u8 = DecodeMaskToU8(mask, &first_mask_info);
            
            // Update app state with mask
            app_state.last_mask_u8 = last_mask_u8.clone();
            
            if (frame_count <= 5) {
                double min_val, max_val;
                cv::minMaxLoc(last_mask_u8, &min_val, &max_val);
                std::cout << "✅ Mask received: " << mask.Width() << "x" << mask.Height() 
                          << " (format: " << mask.NumberOfChannels() << "ch, " << mask.ByteDepth() << "bd -> 8UC1: " << min_val << "-" << max_val << ")" << std::endl;
            }
        }
        
        // Poll latest face landmarks (non-blocking) - WITH DEFENSIVE ERROR HANDLING
        mediapipe::NormalizedLandmarkList latest_lms;
        bool have_lms = false;
        std::vector<mediapipe::NormalizedRect> latest_rects;
        if (has_landmarks && multi_face_landmarks_poller) {
            try {
                mediapipe::Packet lp;
                // Use non-blocking polling with queue size check
                int queue_size = multi_face_landmarks_poller->QueueSize();
                if (frame_count <= 5) {
                    std::cout << "📍 Landmarks queue size: " << queue_size << std::endl;
                }
                
                while (queue_size > 0 && multi_face_landmarks_poller->Next(&lp)) {
                    try {
                        // Expecting vector<NormalizedLandmarkList>
                        const auto& v = lp.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
                        if (!v.empty()) { 
                            latest_lms = v[0]; 
                            have_lms = true;
                            if (frame_count <= 5) {
                                std::cout << "✅ Got landmarks with " << latest_lms.landmark_size() << " points" << std::endl;
                            }
                        }
                        queue_size = multi_face_landmarks_poller->QueueSize(); // Update queue size
                    } catch (const std::exception& e) {
                        std::cerr << "❌ Error processing landmarks packet: " << e.what() << std::endl;
                        break;
                    }
                }
                
                // Process face rects if available
                if (face_rects_poller) {
                    mediapipe::Packet rp;
                    while (face_rects_poller->QueueSize() > 0 && face_rects_poller->Next(&rp)) {
                        latest_rects = rp.Get<std::vector<mediapipe::NormalizedRect>>();
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "❌ Exception during landmarks polling: " << e.what() << std::endl;
                // Don't exit, just continue without landmarks for this frame
            }
        }
        
        // Process frame for display
        cv::Mat display_rgb;
        cv::Mat processed_frame = frame_bgr;
        
        // Apply effects if EffectsManager is available
        if (managers.effects) {
            // Sync app_state settings to EffectsManager before processing
            SyncSettingsToEffectsManager(*managers.effects, app_state);
            
            // Only process if we have a mask or face landmarks
            if (!last_mask_u8.empty() || have_lms) {
                // Use EffectsManager to process the frame with segmentation mask and face landmarks
                const mediapipe::NormalizedLandmarkList* landmarks_ptr = (have_lms) ? &latest_lms : nullptr;
                processed_frame = managers.effects->ProcessFrame(frame_bgr, last_mask_u8, landmarks_ptr);
            } else {
                // No processing needed - use original frame
            }
        }
        
        // EffectsManager now returns RGB directly, no conversion needed
        display_rgb = processed_frame.clone();
        
        // Update app state with current frame
        app_state.last_display_rgb = display_rgb.clone();
        
        // Virtual Camera Output - write to v4l2loopback device
        if (app_state.vcam.IsOpen() && !display_rgb.empty()) {
            // Check if frame size matches vcam, reopen if needed
            if (display_rgb.cols != app_state.vcam.Width() || display_rgb.rows != app_state.vcam.Height()) {
                // Get virtual camera list and reopen with correct size
                auto vcam_list = managers.camera->GetVCamList();
                if (app_state.ui_vcam_idx >= 0 && app_state.ui_vcam_idx < (int)vcam_list.size()) {
                    app_state.vcam.Open(vcam_list[app_state.ui_vcam_idx].path, display_rgb.cols, display_rgb.rows);
                }
            }
            
            // Convert RGB to BGR and write to virtual camera
            cv::Mat display_bgr;
            cv::cvtColor(display_rgb, display_bgr, cv::COLOR_RGB2BGR);
            app_state.vcam.WriteBGR(display_bgr);
        }
        
        // Let UIManager handle events first
        if (!ui_manager.ProcessEvents(running)) {
            std::cout << "🛑 UIManager ProcessEvents returned false, exiting..." << std::endl;
            break;
        }
        
        // Handle any dropped files
        auto dropped_files = ui_manager.GetDroppedFiles();
        for (const auto& file_path : dropped_files) {
            std::cout << "🖼️  Processing dropped file: " << file_path << std::endl;
            cv::Mat img;
            std::string resolved_path;
            if (LoadBackgroundImageWithPortal(file_path, img, resolved_path)) {
                app_state.bg_image = img.clone();
                app_state.bg_mode = 2; // Automatically switch to Image mode (0=None, 1=Blur, 2=Image, 3=Solid)
                // Update the background path for profile persistence
                strncpy(app_state.bg_path_buf, resolved_path.c_str(), sizeof(app_state.bg_path_buf) - 1);
                app_state.bg_path_buf[sizeof(app_state.bg_path_buf) - 1] = '\0';
                std::cout << "✅ Background image loaded: " << img.cols << "x" << img.rows
                          << " (auto-switched to Image mode)" << std::endl;
                std::cout << "🔖 Background path saved: " << app_state.bg_path_buf << std::endl;
            } else {
                std::cout << "❌ Failed to load dropped file as image after portal fallback." << std::endl;
            }
        }
        
        if (!running) {
            std::cout << "🛑 Running flag set to false by event handler, exiting..." << std::endl;
            break;
        }
        
        if (frame_count <= 5) {
            std::cout << "✅ UI events processed successfully for frame " << frame_count << std::endl;
        }
        
        // Get window size for rendering
        int dw, dh;
        SDL_GL_GetDrawableSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        
        // Clear screen with dark background (matching original)
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
        
        if (frame_count <= 5) {
            std::cout << "✅ Frame " << frame_count << " completed successfully" << std::endl;
        }
        
        // Check if running flag is still true after each iteration
        if (!running) {
            std::cout << "🛑 Running flag became false at end of frame " << frame_count << std::endl;
            break;
        }
        
        // Log when we're about to continue to the next iteration
        if (frame_count <= 5) {
            std::cout << "🔄 Frame " << frame_count << " loop completed, continuing..." << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception in main loop: " << e.what() << std::endl;
        break;
    } catch (...) {
        std::cerr << "❌ Unknown exception in main loop" << std::endl;
        break;
    }
}
    
    std::cout << "🛑 Main loop ended" << std::endl;
    return 0;
}

} // namespace segmecam
