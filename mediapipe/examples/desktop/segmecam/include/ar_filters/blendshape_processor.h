#ifndef SEGMECAM_BLENDSHAPE_PROCESSOR_H
#define SEGMECAM_BLENDSHAPE_PROCESSOR_H

#include <array>
#include <string>
#include "mediapipe/framework/formats/classification.pb.h"

namespace segmecam {

/**
 * Blendshape indices matching MediaPipe FaceLandmarker output
 * Total: 52 blendshape coefficients (0.0 - 1.0)
 * Reference: https://storage.googleapis.com/mediapipe-assets/Model%20Card%20Blendshape%20V2.pdf
 */
enum class BlendshapeIndex {
    // Brows (5)
    BROW_DOWN_LEFT = 0,
    BROW_DOWN_RIGHT = 1,
    BROW_INNER_UP = 2,
    BROW_OUTER_UP_LEFT = 3,
    BROW_OUTER_UP_RIGHT = 4,
    
    // Cheeks (3)
    CHEEK_PUFF = 5,
    CHEEK_SQUINT_LEFT = 6,
    CHEEK_SQUINT_RIGHT = 7,
    
    // Eyes (14)
    EYE_BLINK_LEFT = 8,
    EYE_BLINK_RIGHT = 9,
    EYE_LOOK_DOWN_LEFT = 10,
    EYE_LOOK_DOWN_RIGHT = 11,
    EYE_LOOK_IN_LEFT = 12,
    EYE_LOOK_IN_RIGHT = 13,
    EYE_LOOK_OUT_LEFT = 14,
    EYE_LOOK_OUT_RIGHT = 15,
    EYE_LOOK_UP_LEFT = 16,
    EYE_LOOK_UP_RIGHT = 17,
    EYE_SQUINT_LEFT = 18,
    EYE_SQUINT_RIGHT = 19,
    EYE_WIDE_LEFT = 20,
    EYE_WIDE_RIGHT = 21,
    
    // Jaw (4)
    JAW_FORWARD = 22,
    JAW_LEFT = 23,
    JAW_OPEN = 24,
    JAW_RIGHT = 25,
    
    // Mouth (24)
    MOUTH_CLOSE = 26,
    MOUTH_DIMPLE_LEFT = 27,
    MOUTH_DIMPLE_RIGHT = 28,
    MOUTH_FROWN_LEFT = 29,
    MOUTH_FROWN_RIGHT = 30,
    MOUTH_FUNNEL = 31,
    MOUTH_LEFT = 32,
    MOUTH_LOWER_DOWN_LEFT = 33,
    MOUTH_LOWER_DOWN_RIGHT = 34,
    MOUTH_PRESS_LEFT = 35,
    MOUTH_PRESS_RIGHT = 36,
    MOUTH_PUCKER = 37,
    MOUTH_RIGHT = 38,
    MOUTH_ROLL_LOWER = 39,
    MOUTH_ROLL_UPPER = 40,
    MOUTH_SHRUG_LOWER = 41,
    MOUTH_SHRUG_UPPER = 42,
    MOUTH_SMILE_LEFT = 43,
    MOUTH_SMILE_RIGHT = 44,
    MOUTH_STRETCH_LEFT = 45,
    MOUTH_STRETCH_RIGHT = 46,
    MOUTH_UPPER_UP_LEFT = 47,
    MOUTH_UPPER_UP_RIGHT = 48,
    
    // Nose (2)
    NOSE_SNEER_LEFT = 49,
    NOSE_SNEER_RIGHT = 50,
    
    // Tongue (1)
    TONGUE_OUT = 51
};

/**
 * Container for 52 blendshape coefficients with timestamp
 */
struct BlendshapeData {
    std::array<float, 52> coefficients;  // 0.0 - 1.0 values
    int64_t timestamp_us;
    
    BlendshapeData() : coefficients{}, timestamp_us(0) {
        coefficients.fill(0.0f);
    }
    
    /**
     * Get blendshape value by index
     */
    float Get(BlendshapeIndex idx) const { 
        return coefficients[static_cast<size_t>(idx)]; 
    }
    
    /**
     * Set blendshape value by index
     */
    void Set(BlendshapeIndex idx, float value) { 
        coefficients[static_cast<size_t>(idx)] = value; 
    }
    
    /**
     * Get blendshape name for debugging
     */
    static std::string GetName(BlendshapeIndex idx);
};

/**
 * Blendshape Processor
 * 
 * Processes MediaPipe FaceLandmarker blendshape output into usable facial
 * expression data. Provides smoothing to reduce jitter and high-level
 * expression queries for common use cases.
 * 
 * Use cases:
 * - AR filter behaviors (blink → shake, smile → color change)
 * - Enhanced wrinkle detection (precise smile/squint values)
 * - Expression-triggered events (wink, jaw open, etc.)
 */
class BlendshapeProcessor {
public:
    BlendshapeProcessor();
    ~BlendshapeProcessor();
    
    /**
     * Update with new blendshape data from MediaPipe
     * @param blendshapes MediaPipe classification list (52 elements expected)
     */
    void Update(const mediapipe::ClassificationList& blendshapes);
    
    /**
     * Get current raw blendshape data (no smoothing)
     */
    const BlendshapeData& GetBlendshapes() const { return current_blendshapes_; }
    
    /**
     * Get smoothed blendshape data (reduced jitter)
     */
    const BlendshapeData& GetSmoothedBlendshapes() const { return smoothed_blendshapes_; }
    
    /**
     * Set smoothing factor (0.0 = no smoothing, 1.0 = heavy smoothing)
     * Default: 0.3
     */
    void SetSmoothingFactor(float alpha);
    
    /**
     * Get current smoothing factor
     */
    float GetSmoothingFactor() const { return smoothing_alpha_; }
    
    // ========== High-Level Expression Queries ==========
    
    /**
     * Check if user is smiling (mouthSmileLeft/Right > threshold)
     * @param threshold Smile detection threshold (default: 0.5)
     */
    bool IsSmiling(float threshold = 0.5f) const;
    
    /**
     * Check if user is blinking (either eye blink > threshold)
     * @param threshold Blink detection threshold (default: 0.7)
     */
    bool IsBlinking(float threshold = 0.7f) const;
    
    /**
     * Check if user is winking (one eye closed, other open)
     * @param close_threshold Eye closed threshold (default: 0.8)
     * @param open_threshold Eye open threshold (default: 0.2)
     */
    bool IsWinking(float close_threshold = 0.8f, float open_threshold = 0.2f) const;
    
    /**
     * Get jaw openness (0.0 = closed, 1.0 = fully open)
     */
    float GetJawOpenness() const;
    
    /**
     * Get smile intensity (average of left/right smile)
     */
    float GetSmileIntensity() const;
    
    /**
     * Get squint intensity (average of left/right cheek squint)
     */
    float GetSquintIntensity() const;
    
    /**
     * Get eyebrow raise intensity (average of inner + outer brows)
     */
    float GetEyebrowRaiseIntensity() const;
    
    /**
     * Check if mouth is wide open (jawOpen > threshold)
     * @param threshold Jaw open threshold (default: 0.75)
     */
    bool IsMouthWideOpen(float threshold = 0.75f) const;
    
    /**
     * Check if user looks surprised (eyes wide + eyebrows up)
     * @param eye_threshold Eye wide threshold (default: 0.6)
     * @param brow_threshold Eyebrow up threshold (default: 0.5)
     */
    bool IsSurprised(float eye_threshold = 0.6f, float brow_threshold = 0.5f) const;
    
    /**
     * Reset all blendshape data to zero
     */
    void Reset();
    
private:
    BlendshapeData current_blendshapes_;   // Raw, unsmoothed data
    BlendshapeData smoothed_blendshapes_;  // Smoothed data (reduced jitter)
    float smoothing_alpha_;                // Smoothing factor (0.0-1.0)
    
    /**
     * Apply exponential smoothing to reduce jitter
     * new_smooth = alpha * new_raw + (1 - alpha) * old_smooth
     */
    void ApplySmoothing();
};

} // namespace segmecam

#endif // SEGMECAM_BLENDSHAPE_PROCESSOR_H
