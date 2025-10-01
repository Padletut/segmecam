#pragma once

#include <chrono>
#include <vector>
#include <functional>

namespace segmecam {

/**
 * Performance Monitor Module
 *
 * Handles performance tracking, FPS monitoring, and automatic processing scale adjustment.
 */
class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();

    // Performance tracking
    void UpdatePerformanceTracking(const std::chrono::steady_clock::time_point& start_time,
                                  double& last_smoothing_time_ms,
                                  double& last_background_time_ms,
                                  double& total_processing_time_ms,
                                  int& frames_processed);

    void LogPerformanceStats(double avg_frame_time, double avg_smooth_time, double avg_bg_time,
                           uint32_t total_frames, bool opencl_enabled);
    bool ShouldLogPerformance();

    double GetAverageProcessingTime() const;
    void ResetPerformanceStats();

    // Auto processing scale functionality
    void SetAutoProcessingScaleEnabled(bool enabled);
    void SetTargetFPS(float target_fps);
    void UpdateTargetFPSFromCamera(float camera_fps);
    void UpdateAutoProcessingScale(float current_fps);
    bool IsAutoProcessingScaleEnabled() const;
    float GetCurrentFPS() const;
    float GetTargetFPS() const;
    float GetProcessingScale() const;
    void SetProcessingScaleGetter(std::function<float()> cb) { get_processing_scale_cb_ = std::move(cb); }

    // Callback setter for processing scale
    void SetProcessingScaleCallback(std::function<void(float)> cb) { set_processing_scale_cb_ = std::move(cb); }

    // Performance tracking members
    std::chrono::steady_clock::time_point last_perf_log_time_;
    double perf_sum_frame_ms_;
    double perf_sum_smooth_ms_;
    double perf_sum_bg_ms_;
    uint32_t perf_sum_frames_;

    // Auto processing scale members
    bool auto_scale_enabled_;
    float target_fps_;
    float current_fps_;
    std::chrono::steady_clock::time_point last_fps_update_;
    std::chrono::steady_clock::time_point last_scale_adjustment_;
    std::vector<float> fps_history_;

    static constexpr size_t FPS_HISTORY_SIZE = 8;

    // Helper methods
    void UpdateFPSHistory(float current_fps);
    bool HasEnoughFPSSamples() const;
    bool ShouldAdjustScale(const std::chrono::steady_clock::time_point& now) const;
    float CalculateAverageFPS() const;
    float CalculateScaleAdjustment(float avg_fps) const;
    void ApplyScaleAdjustment(float scale_adjustment, const std::chrono::steady_clock::time_point& now);
    void TrimFPSHistoryForStability();

    // Callback to update processing scale in EffectsManager
    std::function<void(float)> set_processing_scale_cb_;
    // Callback to get current processing scale from EffectsManager
    std::function<float()> get_processing_scale_cb_;
};

} // namespace segmecam