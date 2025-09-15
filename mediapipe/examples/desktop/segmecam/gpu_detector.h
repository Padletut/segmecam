#pragma once

#include <string>
#include <vector>

enum class GPUBackend {
    NONE,
    NVIDIA_EGL,
    MESA_EGL,     // Covers AMD, Intel, and other Mesa-supported GPUs
    AMD_RADEON,   // Specific AMD detection (theoretical - not tested)
    INTEL_GPU,    // Specific Intel detection (theoretical - not tested)
    CPU_ONLY
};

enum class RuntimeEnvironment {
    NATIVE,
    FLATPAK,
    DOCKER,
    UNKNOWN
};

struct GPUCapabilities {
    GPUBackend backend = GPUBackend::NONE;
    RuntimeEnvironment environment = RuntimeEnvironment::UNKNOWN;
    std::string vendor;
    std::string renderer;
    std::string version;
    bool egl_available = false;
    bool opengl_available = false;
    std::vector<std::string> egl_library_paths;
};

class GPUDetector {
public:
    static GPUCapabilities DetectGPUCapabilities();
    static RuntimeEnvironment DetectEnvironment();
    static bool SetupOptimalEGLPath(const GPUCapabilities& caps);
    static GPUBackend GetBestAvailableBackend();
    
    // Testing utilities
    static GPUCapabilities DetectGPUCapabilitiesForTesting(bool force_no_nvidia = false, bool force_no_mesa = false);
    static void SetTestingMode(bool enable) { testing_mode = enable; }

private:
    static bool TestNvidiaEGL();
    static bool TestMesaEGL();
    static bool TestAMDRadeon();    // Theoretical AMD detection
    static bool TestIntelGPU();     // Theoretical Intel detection
    static std::vector<std::string> FindEGLLibraries();
    static bool IsInFlatpak();
    static bool IsInDocker();
    
    static bool testing_mode;
};