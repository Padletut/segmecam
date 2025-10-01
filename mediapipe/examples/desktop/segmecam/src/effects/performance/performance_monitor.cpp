#include "effects/performance/performance_monitor.h"
#include <iostream>
#include <algorithm>

namespace segmecam {

PerformanceMonitor::PerformanceMonitor()
    : perf_sum_frame_ms_(0.0),
      perf_sum_smooth_ms_(0.0),
      perf_sum_bg_ms_(0.0),
      perf_sum_frames_(0),
      auto_scale_enabled_(false),
      target_fps_(29.0f),
      current_fps_(0.0f) {
    last_perf_log_time_ = std::chrono::steady_clock::now();
    last_fps_update_ = std::chrono::steady_clock::now();
    last_scale_adjustment_ = std::chrono::steady_clock::now();
}

PerformanceMonitor::~PerformanceMonitor() {
    // No cleanup needed
}

void PerformanceMonitor::UpdatePerformanceTracking(const std::chrono::steady_clock::time_point& start_time,
                                                  double& last_smoothing_time_ms,
                                                  double& last_background_time_ms,
                                                  double& total_processing_time_ms,
                                                  int& frames_processed) {
    auto end_time = std::chrono::steady_clock::now();
    total_processing_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    perf_sum_frame_ms_ += total_processing_time_ms;
    frames_processed++;
    perf_sum_frames_++;

    // Note: Logging is handled separately by the caller
}

void PerformanceMonitor::LogPerformanceStats(double avg_frame_time, double avg_smooth_time,
                                           double avg_bg_time, uint32_t total_frames, bool opencl_enabled) {
    if (total_frames == 0) return;

    std::cout << "📊 Effects Performance [" << total_frames << " frames]:" << std::endl;
    std::cout << "  Total: " << avg_frame_time << "ms" << std::endl;
    std::cout << "  Smoothing: " << avg_smooth_time << "ms" << std::endl;
    std::cout << "  Background: " << avg_bg_time << "ms" << std::endl;
    if (opencl_enabled) {
        std::cout << "  OpenCL: enabled" << std::endl;
    }

    // Reset for next interval
    ResetPerformanceStats();
}

bool PerformanceMonitor::ShouldLogPerformance() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_perf_log_time_).count();
    return elapsed >= 5000; // Default 5 second interval
}

double PerformanceMonitor::GetAverageProcessingTime() const {
    if (perf_sum_frames_ == 0) return 0.0;
    return perf_sum_frame_ms_ / perf_sum_frames_;
}

void PerformanceMonitor::ResetPerformanceStats() {
    perf_sum_frame_ms_ = 0.0;
    perf_sum_smooth_ms_ = 0.0;
    perf_sum_bg_ms_ = 0.0;
    perf_sum_frames_ = 0;
    last_perf_log_time_ = std::chrono::steady_clock::now();
}

// Auto processing scale methods
void PerformanceMonitor::SetAutoProcessingScaleEnabled(bool enabled) {
    if (auto_scale_enabled_ == enabled) {
        return; // No change, don't reset timers
    }

    auto_scale_enabled_ = enabled;
    if (enabled) {
        fps_history_.clear();
        last_fps_update_ = std::chrono::steady_clock::now();
        last_scale_adjustment_ = std::chrono::steady_clock::now();
        std::cout << "[AutoScale] Enabled with target FPS: " << target_fps_ << std::endl;
    }
}

void PerformanceMonitor::SetTargetFPS(float target_fps) {
    float new_target = std::clamp(target_fps, 5.0f, 30.0f);
    if (std::abs(target_fps_ - new_target) < 0.1f) {
        return; // No significant change
    }
    target_fps_ = new_target;
}

void PerformanceMonitor::UpdateTargetFPSFromCamera(float camera_fps) {
    float target_fps;
    if (camera_fps >= 15.0f) {
        target_fps = 30.0f; // Target 14 FPS for high frame rate cameras
    } else {
        target_fps = camera_fps - 1.0f; // Target camera_fps - 1 for lower frame rates
    }

    SetTargetFPS(target_fps);
}

void PerformanceMonitor::UpdateAutoProcessingScale(float current_fps) {
    if (!auto_scale_enabled_) {
        return;
    }

    current_fps_ = current_fps;
    auto now = std::chrono::steady_clock::now();

    UpdateFPSHistory(current_fps);

    if (!HasEnoughFPSSamples() || !ShouldAdjustScale(now)) {
        return;
    }

    float avg_fps = CalculateAverageFPS();
    float scale_adjustment = CalculateScaleAdjustment(avg_fps);

    if (scale_adjustment != 0.0f) {
        ApplyScaleAdjustment(scale_adjustment, now);
    }
}

void PerformanceMonitor::UpdateFPSHistory(float current_fps) {
    fps_history_.push_back(current_fps);
    if (fps_history_.size() > FPS_HISTORY_SIZE) {
        fps_history_.erase(fps_history_.begin());
    }
}

bool PerformanceMonitor::HasEnoughFPSSamples() const {
    return fps_history_.size() >= 10;
}

bool PerformanceMonitor::ShouldAdjustScale(const std::chrono::steady_clock::time_point& now) const {
    auto time_since_adjustment = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_scale_adjustment_).count();
    return time_since_adjustment >= 5000;
}

float PerformanceMonitor::CalculateAverageFPS() const {
    float avg_fps = 0.0f;
    for (float fps : fps_history_) {
        avg_fps += fps;
    }
    return avg_fps / fps_history_.size();
}

float PerformanceMonitor::CalculateScaleAdjustment(float avg_fps) const {
    float fps_diff = target_fps_ - avg_fps;

    if (std::abs(fps_diff) <= 2.0f) {
        return 0.0f;
    }

    if (fps_diff > 3.0f) {
        return fps_diff > 6.0f ? -0.002f : -0.001f;
    } else if (fps_diff < -3.0f) {
        return fps_diff < -6.0f ? 0.002f : 0.001f;
    }

    return 0.0f;
}

void PerformanceMonitor::ApplyScaleAdjustment(float scale_adjustment, const std::chrono::steady_clock::time_point& now) {
    // Actually update the processing scale via callback if set
    if (set_processing_scale_cb_) {
        float current_scale = GetProcessingScale();
        float new_scale = std::clamp(current_scale + scale_adjustment, 0.4f, 1.0f);
        if (std::abs(new_scale - current_scale) > 0.0005f) {
            set_processing_scale_cb_(new_scale);
        }
    }
    last_scale_adjustment_ = now;
    TrimFPSHistoryForStability();
}

void PerformanceMonitor::TrimFPSHistoryForStability() {
    if (fps_history_.size() > 10) {
        fps_history_.erase(fps_history_.begin(), fps_history_.begin() + fps_history_.size()/2);
    }
}

bool PerformanceMonitor::IsAutoProcessingScaleEnabled() const {
    return auto_scale_enabled_;
}

float PerformanceMonitor::GetCurrentFPS() const {
    return current_fps_;
}

float PerformanceMonitor::GetTargetFPS() const {
    return target_fps_;
}

float PerformanceMonitor::GetProcessingScale() const {
    if (get_processing_scale_cb_) {
        return get_processing_scale_cb_();
    }
    return 1.0f; // Default fallback
}

} // namespace segmecam