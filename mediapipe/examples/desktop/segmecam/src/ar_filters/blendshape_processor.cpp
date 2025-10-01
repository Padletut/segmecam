#include "include/ar_filters/blendshape_processor.h"
#include <cmath>
#include <iostream>

namespace segmecam {

// Static blendshape names for debugging
std::string BlendshapeData::GetName(BlendshapeIndex idx) {
    static const char* names[] = {
        "browDownLeft", "browDownRight", "browInnerUp", "browOuterUpLeft", "browOuterUpRight",
        "cheekPuff", "cheekSquintLeft", "cheekSquintRight",
        "eyeBlinkLeft", "eyeBlinkRight", "eyeLookDownLeft", "eyeLookDownRight",
        "eyeLookInLeft", "eyeLookInRight", "eyeLookOutLeft", "eyeLookOutRight",
        "eyeLookUpLeft", "eyeLookUpRight", "eyeSquintLeft", "eyeSquintRight",
        "eyeWideLeft", "eyeWideRight",
        "jawForward", "jawLeft", "jawOpen", "jawRight",
        "mouthClose", "mouthDimpleLeft", "mouthDimpleRight",
        "mouthFrownLeft", "mouthFrownRight", "mouthFunnel",
        "mouthLeft", "mouthLowerDownLeft", "mouthLowerDownRight",
        "mouthPressLeft", "mouthPressRight", "mouthPucker", "mouthRight",
        "mouthRollLower", "mouthRollUpper", "mouthShrugLower", "mouthShrugUpper",
        "mouthSmileLeft", "mouthSmileRight", "mouthStretchLeft", "mouthStretchRight",
        "mouthUpperUpLeft", "mouthUpperUpRight",
        "noseSneerLeft", "noseSneerRight",
        "tongueOut"
    };
    size_t index = static_cast<size_t>(idx);
    if (index < 52) {
        return names[index];
    }
    return "unknown";
}

BlendshapeProcessor::BlendshapeProcessor() 
    : smoothing_alpha_(0.3f) {
}

BlendshapeProcessor::~BlendshapeProcessor() {
}

void BlendshapeProcessor::Update(const mediapipe::ClassificationList& blendshapes) {
    // MediaPipe FaceLandmarker outputs 52 blendshape classifications
    if (blendshapes.classification_size() != 52) {
        std::cerr << "⚠️  Expected 52 blendshapes, got " << blendshapes.classification_size() << std::endl;
        return;
    }
    
    // Extract raw blendshape values
    for (int i = 0; i < 52; ++i) {
        const auto& classification = blendshapes.classification(i);
        float score = classification.score();
        
        // Clamp to [0.0, 1.0] range
        score = std::max(0.0f, std::min(1.0f, score));
        
        current_blendshapes_.coefficients[i] = score;
    }
    
    // Timestamp will be set externally when calling Update()
    // (ClassificationList doesn't contain timestamp information)
    
    // Apply smoothing
    ApplySmoothing();
}

void BlendshapeProcessor::ApplySmoothing() {
    // Exponential moving average: new_smooth = alpha * new_raw + (1 - alpha) * old_smooth
    // alpha closer to 1.0 = more responsive (less smooth)
    // alpha closer to 0.0 = more smooth (less responsive)
    
    for (size_t i = 0; i < 52; ++i) {
        float raw_value = current_blendshapes_.coefficients[i];
        float smooth_value = smoothed_blendshapes_.coefficients[i];
        
        // Exponential smoothing
        smooth_value = smoothing_alpha_ * raw_value + (1.0f - smoothing_alpha_) * smooth_value;
        
        smoothed_blendshapes_.coefficients[i] = smooth_value;
    }
    
    smoothed_blendshapes_.timestamp_us = current_blendshapes_.timestamp_us;
}

void BlendshapeProcessor::SetSmoothingFactor(float alpha) {
    smoothing_alpha_ = std::max(0.0f, std::min(1.0f, alpha));
}

// ========== High-Level Expression Queries ==========

bool BlendshapeProcessor::IsSmiling(float threshold) const {
    float smile_left = smoothed_blendshapes_.Get(BlendshapeIndex::MOUTH_SMILE_LEFT);
    float smile_right = smoothed_blendshapes_.Get(BlendshapeIndex::MOUTH_SMILE_RIGHT);
    float avg_smile = (smile_left + smile_right) / 2.0f;
    return avg_smile > threshold;
}

bool BlendshapeProcessor::IsBlinking(float threshold) const {
    float blink_left = smoothed_blendshapes_.Get(BlendshapeIndex::EYE_BLINK_LEFT);
    float blink_right = smoothed_blendshapes_.Get(BlendshapeIndex::EYE_BLINK_RIGHT);
    return (blink_left > threshold) || (blink_right > threshold);
}

bool BlendshapeProcessor::IsWinking(float close_threshold, float open_threshold) const {
    float blink_left = smoothed_blendshapes_.Get(BlendshapeIndex::EYE_BLINK_LEFT);
    float blink_right = smoothed_blendshapes_.Get(BlendshapeIndex::EYE_BLINK_RIGHT);
    
    // Left eye closed, right eye open
    bool left_wink = (blink_left > close_threshold) && (blink_right < open_threshold);
    
    // Right eye closed, left eye open
    bool right_wink = (blink_right > close_threshold) && (blink_left < open_threshold);
    
    return left_wink || right_wink;
}

float BlendshapeProcessor::GetJawOpenness() const {
    return smoothed_blendshapes_.Get(BlendshapeIndex::JAW_OPEN);
}

float BlendshapeProcessor::GetSmileIntensity() const {
    float smile_left = smoothed_blendshapes_.Get(BlendshapeIndex::MOUTH_SMILE_LEFT);
    float smile_right = smoothed_blendshapes_.Get(BlendshapeIndex::MOUTH_SMILE_RIGHT);
    return (smile_left + smile_right) / 2.0f;
}

float BlendshapeProcessor::GetSquintIntensity() const {
    float squint_left = smoothed_blendshapes_.Get(BlendshapeIndex::CHEEK_SQUINT_LEFT);
    float squint_right = smoothed_blendshapes_.Get(BlendshapeIndex::CHEEK_SQUINT_RIGHT);
    return (squint_left + squint_right) / 2.0f;
}

float BlendshapeProcessor::GetEyebrowRaiseIntensity() const {
    float inner_up = smoothed_blendshapes_.Get(BlendshapeIndex::BROW_INNER_UP);
    float outer_left = smoothed_blendshapes_.Get(BlendshapeIndex::BROW_OUTER_UP_LEFT);
    float outer_right = smoothed_blendshapes_.Get(BlendshapeIndex::BROW_OUTER_UP_RIGHT);
    return (inner_up + outer_left + outer_right) / 3.0f;
}

bool BlendshapeProcessor::IsMouthWideOpen(float threshold) const {
    return GetJawOpenness() > threshold;
}

bool BlendshapeProcessor::IsSurprised(float eye_threshold, float brow_threshold) const {
    float eye_wide_left = smoothed_blendshapes_.Get(BlendshapeIndex::EYE_WIDE_LEFT);
    float eye_wide_right = smoothed_blendshapes_.Get(BlendshapeIndex::EYE_WIDE_RIGHT);
    float avg_eye_wide = (eye_wide_left + eye_wide_right) / 2.0f;
    
    float brow_raise = GetEyebrowRaiseIntensity();
    
    return (avg_eye_wide > eye_threshold) && (brow_raise > brow_threshold);
}

void BlendshapeProcessor::Reset() {
    current_blendshapes_.coefficients.fill(0.0f);
    smoothed_blendshapes_.coefficients.fill(0.0f);
    current_blendshapes_.timestamp_us = 0;
    smoothed_blendshapes_.timestamp_us = 0;
}

} // namespace segmecam
