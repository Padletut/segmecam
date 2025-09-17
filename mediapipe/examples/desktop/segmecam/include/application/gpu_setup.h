#ifndef SEGMECAM_GPU_SETUP_H
#define SEGMECAM_GPU_SETUP_H

#include <cstdlib>
#include "gpu_detector.h"

namespace segmecam {

struct GPUSetupState {
    bool force_cpu = false;
    bool force_no_nvidia = false;
    bool force_no_mesa = false;
    bool use_gpu = false;
    GPUCapabilities gpu_caps;
};

class GPUSetup {
public:
    static GPUSetupState DetectAndSetupGPU();
    static void PrintGPUInfo(const GPUSetupState& state);
    
private:
    static GPUSetupState CheckEnvironmentVariables();
    static GPUCapabilities DetectCapabilities(bool force_no_nvidia, bool force_no_mesa);
};

} // namespace segmecam

#endif // SEGMECAM_GPU_SETUP_H