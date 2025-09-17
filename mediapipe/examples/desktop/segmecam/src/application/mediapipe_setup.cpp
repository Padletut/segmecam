#include "include/application/mediapipe_setup.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <thread>
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"
#include "absl/flags/flag.h"
#include "absl/flags/declare.h"

// Declare the resource root dir flag (defined in MediaPipe resource_util)
ABSL_DECLARE_FLAG(std::string, resource_root_dir);

namespace segmecam {

// Helper function to load text file
static mediapipe::StatusOr<std::string> LoadTextFile(const std::string& path) {
    std::string contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(path, &contents));
    return contents;
}

std::string MediaPipeSetup::SelectGraphPath(const ApplicationConfig& config, const GPUCapabilities& gpu_caps) {
    std::string final_graph_path;
    
    // Use CPU graph if forced or no GPU backend
    if ((std::getenv("SEGMECAM_FORCE_CPU") != nullptr) || (gpu_caps.backend == GPUBackend::CPU_ONLY)) {
        final_graph_path = config.cpu_graph_path;
        std::cout << "💻 Using CPU graph" << std::endl;
    } else {
        final_graph_path = config.graph_path;
        std::cout << "🚀 Using GPU graph" << std::endl;
    }
    
    // Resolve full path
    bool force_cpu = (std::getenv("SEGMECAM_FORCE_CPU") != nullptr);
    final_graph_path = ResolveGraphPath(final_graph_path, config.resource_root_dir, force_cpu);
    
    std::cout << "📊 Using graph: " << final_graph_path << std::endl;
    return final_graph_path;
}

int MediaPipeSetup::InitializeGraph(std::unique_ptr<mediapipe::CalculatorGraph>& graph, 
                                  const std::string& graph_path, 
                                  const GPUCapabilities& gpu_caps,
                                  const ApplicationConfig& config) {
    std::cout << "📊 Loading graph config from: " << graph_path << std::endl;
    
    // CRITICAL: Setup MediaPipe resource root directory for model files
    SetupResourceDirectory(config.resource_root_dir);
    
    // Load and parse graph configuration
    std::string cfg_text;
    auto status = mediapipe::file::GetContents(graph_path, &cfg_text);
    if (!status.ok()) { 
        std::fprintf(stderr, "Failed to read graph: %s\n", graph_path.c_str()); 
        return 1; 
    }
    std::cout << "✅ Graph config loaded successfully" << std::endl;
    
    std::cout << "🔧 Parsing graph configuration..." << std::endl;
    mediapipe::CalculatorGraphConfig graph_config = mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(cfg_text);
    std::cout << "✅ Graph config parsed successfully" << std::endl;
    
    // Configure MediaPipe threading for better performance
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads > 1) {
        graph_config.set_num_threads(num_threads);
        std::cout << "🧵 MediaPipe threading configured with " << num_threads << " threads" << std::endl;
    }
    
    // Create graph instance
    graph = std::make_unique<mediapipe::CalculatorGraph>();
    
    std::cout << "🔧 Initializing MediaPipe graph..." << std::endl;
    auto st = graph->Initialize(graph_config);
    if (!st.ok()) { 
        std::fprintf(stderr, "Initialize failed: %s\n", st.message().data()); 
        return 2; 
    }
    
    // Provide GPU resources BEFORE StartRun (only if using GPU graph)
    bool use_gpu = (gpu_caps.backend != GPUBackend::CPU_ONLY);
    if (use_gpu) {
        std::cout << "🚀 Setting up GPU acceleration..." << std::endl;
        
        auto or_gpu = mediapipe::GpuResources::Create();
        if (!or_gpu.ok()) { 
            std::fprintf(stderr, "⚠️  GpuResources::Create failed: %s\n", or_gpu.status().message().data());
            std::fprintf(stderr, "❌ Cannot fallback after graph initialization. Please restart.\n");
            return 2;
        }
        auto gpu_st = graph->SetGpuResources(std::move(or_gpu.value()));
        if (!gpu_st.ok()) { 
            std::fprintf(stderr, "⚠️  SetGpuResources failed: %s\n", gpu_st.message().data());
            std::fprintf(stderr, "❌ Cannot fallback after graph initialization. Please restart.\n");
            return 2;
        }
        std::cout << "✅ GPU acceleration enabled successfully!" << std::endl;
    } else {
        std::cout << "💻 Running in CPU-only mode" << std::endl;
    }
    
    return 0; // Don't call StartGraph automatically - let main application do it after setting up pollers
}

std::string MediaPipeSetup::ResolveGraphPath(const std::string& graph_path, 
                                           const std::string& resource_root_dir,
                                           bool force_cpu) {
    // Check if the graph path exists as-is
    if (std::filesystem::exists(graph_path)) {
        return graph_path;
    }
    
    // Try with different path prefixes for CPU graphs
    if (force_cpu) {
        std::vector<std::string> possible_paths = {
            "/home/padletut/segmecam/" + graph_path,
            resource_root_dir + "/" + graph_path,
            graph_path
        };
        
        for (const auto& path : possible_paths) {
            if (std::filesystem::exists(path)) {
                return path;
            }
        }
    }
    
    // Return original path if no resolution found
    return graph_path;
}

void MediaPipeSetup::SetupResourceDirectory(const std::string& user_resource_root_dir) {
    // Setup MediaPipe resource root directory for model files (critical for graph initialization)
    const char* runfiles_dir = std::getenv("RUNFILES_DIR");
    if (runfiles_dir && *runfiles_dir) {
        // Running under Bazel - use runfiles directory
        absl::SetFlag(&FLAGS_resource_root_dir, std::string(runfiles_dir));
        std::cout << "🔧 Set MediaPipe resource root to Bazel runfiles: " << runfiles_dir << std::endl;
    } else if (!user_resource_root_dir.empty()) {
        // Use user-specified resource root directory
        std::string abs_path = std::filesystem::absolute(user_resource_root_dir).string();
        absl::SetFlag(&FLAGS_resource_root_dir, abs_path);
        std::cout << "🔧 Set MediaPipe resource root to user directory: " << abs_path << std::endl;
    } else {
        // Not running under Bazel - use current working directory
        std::string cwd = std::filesystem::current_path().string();
        absl::SetFlag(&FLAGS_resource_root_dir, cwd);
        std::cout << "🔧 Set MediaPipe resource root to: " << cwd << std::endl;
    }
}

int MediaPipeSetup::StartGraph(std::unique_ptr<mediapipe::CalculatorGraph>& graph) {
    // Start the graph
    auto st = graph->StartRun({});
    if (!st.ok()) { 
        std::fprintf(stderr, "StartRun failed: %s\n", st.message().data()); 
        return 4; 
    }
    
    std::cout << "✅ MediaPipe graph started successfully!" << std::endl;
    return 0;
}

} // namespace segmecam