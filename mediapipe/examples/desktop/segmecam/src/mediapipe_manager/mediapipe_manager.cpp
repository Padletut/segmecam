#include "include/mediapipe_manager/mediapipe_manager.h"

#include <iostream>
#include <filesystem>
#include <cstdio>

#include "absl/flags/flag.h"
#include "absl/flags/declare.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"

ABSL_DECLARE_FLAG(std::string, resource_root_dir);

namespace segmecam {

MediaPipeManager::MediaPipeManager() 
    : graph_(std::make_unique<mediapipe::CalculatorGraph>()) {
}

MediaPipeManager::~MediaPipeManager() {
    Cleanup();
}

int MediaPipeManager::Initialize(const MediaPipeConfig& config) {
    config_ = config;
    state_ = MediaPipeState{}; // Reset state
    
    std::cout << "🧠 Initializing MediaPipe Manager..." << std::endl;
    std::cout << "📊 Using graph: " << config_.graph_path << std::endl;
    
    if (int result = LoadGraphConfiguration(); result != 0) {
        return result;
    }
    
    if (int result = SetupGPUResources(); result != 0) {
        return result;
    }
    
    if (int result = InitializeGraph(); result != 0) {
        return result;
    }
    
    state_.is_initialized = true;
    state_.loaded_graph_path = config_.graph_path;
    
    std::cout << "✅ MediaPipe Manager initialized successfully!" << std::endl;
    return 0;
}

int MediaPipeManager::SetupOutputPollers(bool use_face_landmarks) {
    if (!state_.is_initialized) {
        std::cerr << "❌ MediaPipe Manager not initialized!" << std::endl;
        return 1;
    }
    
    if (state_.is_running) {
        std::cout << "⚠️  Graph already running - pollers should be set up before Start()" << std::endl;
        return 0;
    }
    
    std::cout << "🔗 Setting up output stream pollers..." << std::endl;
    
    // Set up face landmarks pollers if requested
    if (use_face_landmarks) {
        std::cout << "👤 Setting up face landmarks pollers..." << std::endl;
        
        // Set up multi_face_landmarks poller
        auto multi_face_landmarks_status = graph_->ObserveOutputStream(
            "multi_face_landmarks", 
            [&](const mediapipe::Packet& packet) -> mediapipe::Status {
                return mediapipe::OkStatus();
            });
        
        if (!multi_face_landmarks_status.ok()) {
            std::cerr << "❌ Failed to set up multi_face_landmarks poller: " 
                      << multi_face_landmarks_status.message() << std::endl;
            return 3;
        }
        
        // Set up face_rects poller
        auto face_rects_status = graph_->ObserveOutputStream(
            "face_rects", 
            [&](const mediapipe::Packet& packet) -> mediapipe::Status {
                return mediapipe::OkStatus();
            });
        
        if (!face_rects_status.ok()) {
            std::cerr << "❌ Failed to set up face_rects poller: " 
                      << face_rects_status.message() << std::endl;
            return 4;
        }
        
        std::cout << "✅ Face landmarks pollers set up successfully!" << std::endl;
    }
    
    std::cout << "✅ Output stream pollers configured!" << std::endl;
    return 0;
}

int MediaPipeManager::Start() {
    if (!state_.is_initialized) {
        std::cerr << "❌ MediaPipe Manager not initialized!" << std::endl;
        return 1;
    }
    
    if (state_.is_running) {
        std::cout << "⚠️  MediaPipe graph already running" << std::endl;
        return 0;
    }
    
    std::cout << "🚀 Starting MediaPipe graph..." << std::endl;
    
    auto status = graph_->StartRun({});
    if (!status.ok()) { 
        std::fprintf(stderr, "❌ StartRun failed: %s\n", status.message().data()); 
        return 2; 
    }
    
    state_.is_running = true;
    std::cout << "✅ MediaPipe graph started successfully!" << std::endl;
    return 0;
}

int MediaPipeManager::Stop() {
    if (!state_.is_running) {
        return 0; // Already stopped
    }
    
    std::cout << "⏹️  Stopping MediaPipe graph..." << std::endl;
    
    auto status = graph_->CloseInputStream("");
    if (!status.ok()) {
        std::cerr << "⚠️  Failed to close input stream: " << status.message() << std::endl;
    }
    
    status = graph_->WaitUntilDone();
    if (!status.ok()) {
        std::cerr << "⚠️  Failed to finish graph: " << status.message() << std::endl;
    }
    
    state_.is_running = false;
    std::cout << "✅ MediaPipe graph stopped" << std::endl;
    return 0;
}

void MediaPipeManager::Cleanup() {
    std::cout << "🧹 Cleaning up MediaPipe Manager..." << std::endl;
    
    if (state_.is_running) {
        Stop();
    }
    
    // Reset graph
    if (graph_) {
        graph_.reset();
        graph_ = std::make_unique<mediapipe::CalculatorGraph>();
    }
    
    state_ = MediaPipeState{}; // Reset state
    std::cout << "✅ MediaPipe Manager cleanup completed" << std::endl;
}

mediapipe::Status MediaPipeManager::ProcessFrame(const cv::Mat& input_frame, int64_t timestamp_us) {
    if (!IsReady()) {
        return mediapipe::InternalError("MediaPipe Manager not ready for processing");
    }
    
    // TODO: Implement frame processing logic
    // This will be expanded in later phases when we integrate with the camera system
    // For now, return success to validate the architecture
    
    return mediapipe::OkStatus();
}

int MediaPipeManager::LoadGraphConfiguration() {
    // Setup MediaPipe resource directory
    const char* rf = std::getenv("RUNFILES_DIR");
    if (rf && *rf) {
        absl::SetFlag(&FLAGS_resource_root_dir, std::string(rf));
    } else if (!config_.resource_root_dir.empty()) {
        absl::SetFlag(&FLAGS_resource_root_dir, config_.resource_root_dir);
    }

    // Load and parse graph configuration
    auto cfg_text_or = LoadTextFile(config_.graph_path);
    if (!cfg_text_or.ok()) { 
        std::fprintf(stderr, "❌ Failed to read graph: %s\n", config_.graph_path.c_str()); 
        return 1; 
    }
    
    std::cout << "📖 Loading MediaPipe graph configuration..." << std::endl;
    
    try {
        mediapipe::CalculatorGraphConfig graph_config = 
            mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(cfg_text_or.value());
        
        // Store the parsed config for later use
        // Note: The actual initialization happens in InitializeGraph()
        std::cout << "✅ Graph configuration loaded successfully" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "❌ Failed to parse graph config: %s\n", e.what());
        return 2;
    }
}

int MediaPipeManager::SetupGPUResources() {
    if (!config_.use_gpu || config_.force_cpu) {
        std::cout << "💻 MediaPipe configured for CPU-only mode" << std::endl;
        state_.gpu_resources_available = false;
        return 0;
    }
    
    std::cout << "🚀 Setting up MediaPipe GPU resources..." << std::endl;
    
    // Setup optimal EGL path for GPU based on detected capabilities
    if (config_.gpu_capabilities.backend != GPUBackend::CPU_ONLY) {
        GPUDetector::SetupOptimalEGLPath(config_.gpu_capabilities);
        std::cout << "🔧 Optimal EGL path configured for GPU backend" << std::endl;
    }
    
    // GPU resources will be created and assigned during graph initialization
    state_.gpu_resources_available = true;
    std::cout << "✅ GPU resources setup completed" << std::endl;
    return 0;
}

int MediaPipeManager::InitializeGraph() {
    std::cout << "🔧 Initializing MediaPipe calculator graph..." << std::endl;
    
    // Load graph configuration again for actual initialization
    auto cfg_text_or = LoadTextFile(config_.graph_path);
    if (!cfg_text_or.ok()) { 
        std::fprintf(stderr, "❌ Failed to read graph for initialization: %s\n", config_.graph_path.c_str()); 
        return 1; 
    }
    
    mediapipe::CalculatorGraphConfig graph_config = 
        mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(cfg_text_or.value());

    // Initialize the graph
    auto status = graph_->Initialize(graph_config);
    if (!status.ok()) { 
        std::fprintf(stderr, "❌ Graph Initialize failed: %s\n", status.message().data()); 
        return 2; 
    }
    
    // Setup GPU resources if available and needed
    if (state_.gpu_resources_available && config_.use_gpu) {
        std::cout << "🎮 Configuring GPU acceleration for MediaPipe..." << std::endl;
        
        auto gpu_resources_or = mediapipe::GpuResources::Create();
        if (!gpu_resources_or.ok()) { 
            std::fprintf(stderr, "⚠️  GpuResources::Create failed: %s\n", gpu_resources_or.status().message().data());
            std::fprintf(stderr, "❌ Cannot fallback after graph initialization. Please restart.\n");
            return 3;
        }
        
        auto gpu_status = graph_->SetGpuResources(std::move(gpu_resources_or.value()));
        if (!gpu_status.ok()) { 
            std::fprintf(stderr, "⚠️  SetGpuResources failed: %s\n", gpu_status.message().data());
            std::fprintf(stderr, "❌ Cannot fallback after graph initialization. Please restart.\n");
            return 3;
        }
        
        std::cout << "✅ GPU acceleration enabled for MediaPipe!" << std::endl;
    } else {
        std::cout << "💻 MediaPipe running in CPU-only mode" << std::endl;
    }
    
    std::cout << "✅ MediaPipe calculator graph initialized successfully!" << std::endl;
    return 0;
}

mediapipe::StatusOr<std::string> MediaPipeManager::LoadTextFile(const std::string& path) {
    std::string contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(path, &contents));
    return contents;
}

} // namespace segmecam