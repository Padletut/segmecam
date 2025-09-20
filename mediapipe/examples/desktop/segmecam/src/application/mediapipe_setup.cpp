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
    // If given path exists (absolute or relative), use it
    if (std::filesystem::exists(graph_path)) {
        return graph_path;
    }

    // Build list of candidate roots to try
    std::vector<std::string> roots;
    const char* flatpak_id = std::getenv("FLATPAK_ID");
    bool is_flatpak = flatpak_id != nullptr || std::filesystem::exists("/.flatpak-info");

    // Highest priority: explicit resource root (ignore "." which is default)
    if (!resource_root_dir.empty() && resource_root_dir != ".") {
        roots.push_back(resource_root_dir);
    }

    // Common install locations
    if (is_flatpak) {
        roots.push_back("/app");
        roots.push_back("/app/share/segmecam");
        roots.push_back("/app/mediapipe_graphs");
        roots.push_back("/app/share/segmecam/mediapipe_graphs");
    }

    // Helper to try roots + relative
    auto try_with_roots = [&](const std::string& rel) -> std::string {
        for (const auto& r : roots) {
            std::string candidate = r + "/" + rel;
            if (std::filesystem::exists(candidate)) {
                return candidate;
            }
        }
        return std::string();
    };

    // 1) Try root + graph_path as-is (e.g., includes mediapipe_graphs/...)
    if (!graph_path.empty() && graph_path[0] != '/') {
        std::string found = try_with_roots(graph_path);
        if (!found.empty()) return found;
    }

    // 2) Try just the basename under common graph directories
    std::string basename = graph_path;
    auto pos = graph_path.find_last_of('/');
    if (pos != std::string::npos) basename = graph_path.substr(pos + 1);

    std::vector<std::string> graph_dirs = {
        "mediapipe_graphs",
        "/app/mediapipe_graphs",
        "/app/share/segmecam/mediapipe_graphs"
    };
    for (const auto& dir : graph_dirs) {
        std::string candidate = dir + std::string("/") + basename;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    // Not found; return original (will fail later and print error)
    return graph_path;
}

void MediaPipeSetup::SetupResourceDirectory(const std::string& user_resource_root_dir) {
    // Setup MediaPipe resource root directory for model files (critical for graph initialization)
    const char* runfiles_dir = std::getenv("RUNFILES_DIR");
    const char* flatpak_id = std::getenv("FLATPAK_ID");
    bool is_flatpak = flatpak_id != nullptr || std::filesystem::exists("/.flatpak-info");

    if (runfiles_dir && *runfiles_dir) {
        // Running under Bazel - use runfiles directory
        absl::SetFlag(&FLAGS_resource_root_dir, std::string(runfiles_dir));
        std::cout << "🔧 Set MediaPipe resource root to Bazel runfiles: " << runfiles_dir << std::endl;
        return;
    }

    // Treat "." as no explicit override so we can choose better defaults (e.g. Flatpak)
    bool has_explicit_user_dir = !user_resource_root_dir.empty() && user_resource_root_dir != ".";
    if (has_explicit_user_dir) {
        std::string abs_path = std::filesystem::absolute(user_resource_root_dir).string();
        absl::SetFlag(&FLAGS_resource_root_dir, abs_path);
        std::cout << "🔧 Set MediaPipe resource root to user directory: " << abs_path << std::endl;
        return;
    }

    if (is_flatpak) {
        // Default inside Flatpak: runtime installs models to this directory
        std::string flatpak_resources = "/app/mediapipe_runfiles";
        absl::SetFlag(&FLAGS_resource_root_dir, flatpak_resources);
        std::cout << "🔧 Set MediaPipe resource root for Flatpak: " << flatpak_resources << std::endl;
        return;
    }

    // Fallback to current working directory
    std::string cwd = std::filesystem::current_path().string();
    absl::SetFlag(&FLAGS_resource_root_dir, cwd);
    std::cout << "🔧 Set MediaPipe resource root to: " << cwd << std::endl;
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
