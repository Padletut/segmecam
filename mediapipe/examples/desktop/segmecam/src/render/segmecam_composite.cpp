#include "include/render/segmecam_composite.h"
#include "include/compositing_utils.h"

#include <opencv2/core/ocl.hpp>

// Persistently remembered preferred channel when mask comes as 4xU8 (SRGBA).
// This avoids per-frame channel switches that can look like flicker.
static int g_rgba_mask_channel = -1; // 0=B,1=G,2=R,3=A

// Helper function to perform compositing with optional upscaling
cv::Mat PerformCompositingWithUpscale(const cv::Mat& small_frame, const cv::Mat& small_mask, 
                                     const cv::Mat& small_bg, const cv::Size& original_size) {
  // GPU compositing if OpenCL is enabled and all inputs are UMat
  static thread_local bool use_ocl = cv::ocl::useOpenCL();
  if (use_ocl) {
    cv::UMat frame_u, bg_u, mask_u;
    small_frame.copyTo(frame_u);
    small_bg.copyTo(bg_u);
    small_mask.copyTo(mask_u);
    cv::UMat frame_f, bg_f, mask_f;
    frame_u.convertTo(frame_f, CV_32FC3, 1.0/255.0);
    bg_u.convertTo(bg_f, CV_32FC3, 1.0/255.0);
    mask_u.convertTo(mask_f, CV_32FC1, 1.0/255.0);
    std::vector<cv::UMat> fch(3), bch(3), out(3);
    cv::split(frame_f, fch);
    cv::split(bg_f, bch);
    cv::UMat one(mask_f.size(), mask_f.type()); one.setTo(1.0f);
    cv::UMat inv; cv::subtract(one, mask_f, inv);
    for (int i=0;i<3;++i) {
      cv::UMat a,b; cv::multiply(fch[i], mask_f, a); cv::multiply(bch[i], inv, b); cv::add(a,b,out[i]);
    }
    cv::UMat comp_f; cv::merge(out, comp_f);
    cv::UMat comp_u8; comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
    cv::Mat comp_bgr;
    comp_u8.copyTo(comp_bgr);
    // Upscale result if needed
    cv::Mat final_comp;
    if (comp_bgr.size() != original_size) {
      cv::resize(comp_bgr, final_comp, original_size, 0, 0, cv::INTER_LINEAR);
    } else {
      final_comp = comp_bgr;
    }
    return final_comp;
  } else {
    // CPU compositing (original code)
    cv::Mat frame_f, bg_f;
    small_frame.convertTo(frame_f, CV_32FC3, 1.0/255.0);
    small_bg.convertTo(bg_f, CV_32FC3, 1.0/255.0);
    cv::Mat mask_f;
    small_mask.convertTo(mask_f, CV_32FC1, 1.0/255.0);
    std::vector<cv::Mat> fch, bch, cch;
    cv::split(frame_f, fch);
    cv::split(bg_f, bch);
    cch.resize(3);
    for (int i = 0; i < 3; ++i) {
      cch[i] = fch[i].mul(mask_f) + bch[i].mul(1.0 - mask_f);
    }
    cv::Mat comp_f;
    cv::merge(cch, comp_f);
    cv::Mat comp_u8;
    comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
    // Upscale result if needed
    cv::Mat final_comp;
    if (comp_u8.size() != original_size) {
      cv::resize(comp_u8, final_comp, original_size, 0, 0, cv::INTER_LINEAR);
    } else {
      final_comp = comp_u8;
    }
    return final_comp;
  }
}

// Helper function to decode single channel uint8 mask
cv::Mat DecodeSingleChannelU8(const mediapipe::ImageFrame& mask) {
  cv::Mat m(mask.Height(), mask.Width(), CV_8UC1,
            const_cast<uint8_t*>(mask.PixelData()), mask.WidthStep());
  return m.clone();
}

// Helper function to decode single channel float mask
cv::Mat DecodeSingleChannelFloat(const mediapipe::ImageFrame& mask) {
  cv::Mat mf(mask.Height(), mask.Width(), CV_32FC1,
             const_cast<uint8_t*>(mask.PixelData()), mask.WidthStep());
  cv::Mat out;
  mf.convertTo(out, CV_8UC1, 255.0);
  return out;
}

// Helper function to find best RGBA channel for mask data
int FindBestRGBAChannel(const std::vector<cv::Mat>& channels) {
  cv::Scalar mb = cv::mean(channels[0]);
  cv::Scalar mg = cv::mean(channels[1]);
  cv::Scalar mr = cv::mean(channels[2]);
  cv::Scalar ma = cv::mean(channels[3]);
  
  // Find the channel with the highest mean value (most likely to contain segmentation data)
  // Prefer non-alpha channels over alpha channel to avoid blue tint issues
  int best = 0;
  double bestv = mb[0];
  if (mg[0] > bestv) { best = 1; bestv = mg[0]; }
  if (mr[0] > bestv) { best = 2; bestv = mr[0]; }
  // Only use alpha if it's significantly better than color channels
  if (ma[0] > bestv + 10.0) { best = 3; bestv = ma[0]; }
  return best;
}

// Helper function to decode RGBA mask channels
cv::Mat DecodeRGBAChannels(const mediapipe::ImageFrame& mask, bool* logged_once) {
  cv::Mat rgba(mask.Height(), mask.Width(), CV_8UC4,
               const_cast<uint8_t*>(mask.PixelData()), mask.WidthStep());
  std::vector<cv::Mat> chm;
  cv::split(rgba, chm);
  
  int best = g_rgba_mask_channel;
  if (best < 0) {
    best = FindBestRGBAChannel(chm);
    g_rgba_mask_channel = best;
  }
  
  cv::Mat out = chm[best].clone();
  
  if (logged_once && !*logged_once) {
    cv::Scalar mb = cv::mean(chm[0]);
    cv::Scalar mg = cv::mean(chm[1]);
    cv::Scalar mr = cv::mean(chm[2]);
    cv::Scalar ma = cv::mean(chm[3]);
    std::cout << "Mask channels=4 byteDepth=1 means[B,G,R,A]="
              << mb[0] << "," << mg[0] << "," << mr[0] << "," << ma[0]
              << " chosen=" << (g_rgba_mask_channel==0?"B":g_rgba_mask_channel==1?"G":g_rgba_mask_channel==2?"R":"A")
              << std::endl;
    *logged_once = true;
  }
  
  return out;
}

cv::Mat DecodeMaskToU8(const mediapipe::ImageFrame& mask, bool* logged_once) {
  const int ch = mask.NumberOfChannels();
  const int bd = mask.ByteDepth();
  
  if (ch == 1 && bd == 1) {
    return DecodeSingleChannelU8(mask);
  } else if (ch == 1 && bd == 4) {
    return DecodeSingleChannelFloat(mask);
  } else if (ch == 4 && bd == 1) {
    return DecodeRGBAChannels(mask, logged_once);
  } else {
    // Fallback: treat as float
    return DecodeSingleChannelFloat(mask);
  }
}

cv::Mat ResizeMaskToFrame(const cv::Mat& mask_u8, const cv::Size& frame_size) {
  if (mask_u8.empty()) return mask_u8;
  if (mask_u8.size() == frame_size) return mask_u8;
  cv::Mat r; cv::resize(mask_u8, r, frame_size, 0, 0, cv::INTER_LINEAR);
  return r;
}

cv::Mat VisualizeMaskRGB(const cv::Mat& mask_u8) {
  cv::Mat rgb; cv::cvtColor(mask_u8, rgb, cv::COLOR_GRAY2RGB);
  return rgb;
}

static cv::Mat normalizedMaskedBlurChannel(const cv::Mat& ch, const cv::Mat& bg_mask, int k) {
  const float eps = 1e-6f;
  cv::Mat num = ch.mul(bg_mask);
  cv::GaussianBlur(num, num, cv::Size(k,k), 0);
  cv::Mat den; cv::GaussianBlur(bg_mask, den, cv::Size(k,k), 0);
  den = den + eps;
  return num / den;
}

cv::Mat CompositeBlurBackgroundBGR(const cv::Mat& frame_bgr,
                                   const cv::Mat& mask_u8,
                                   int blur_strength,
                                   float feather_px) {
  cv::Mat result = frame_bgr.clone();
  
  // Ensure mask is single-channel
  cv::Mat mask_single;
  if (mask_u8.channels() > 1) {
    std::cout << "🔍 WARNING: Mask has " << mask_u8.channels() << " channels, extracting first channel" << std::endl;
    std::vector<cv::Mat> channels;
    cv::split(mask_u8, channels);
    mask_single = channels[0].clone();
  } else {
    mask_single = mask_u8;
  }
  
  cv::Mat mask_f; mask_single.convertTo(mask_f, CV_32FC1, 1.0/255.0);

  // Apply feathering if requested
  if (feather_px > 0.5f) {
    int fks = (int)std::max(1.0f, feather_px) * 2 + 1;
    cv::GaussianBlur(mask_f, mask_f, cv::Size(fks, fks), 0);
  }

  // Use the shared utility for blurred background compositing
  segmecam::CompositeWithBlurredBackground(result, frame_bgr, mask_f, blur_strength, false);
  return result;
}

cv::Mat CompositeBlurBackgroundBGR_Accel(const cv::Mat& frame_bgr,
                                         const cv::Mat& mask_u8,
                                         int blur_strength,
                                         float feather_px,
                                         bool use_ocl,
                                         float scale) {
  // Debug: Log OpenCL/GPU status if requested
  // (OpenCL debug output removed)
  scale = std::clamp(scale, 0.4f, 1.0f);

  // Timing: start total
  auto t_start = std::chrono::high_resolution_clock::now();

  // Allow OpenCL acceleration if requested (do not force CPU path)

  // Timing: downscale
  auto t_downscale_start = std::chrono::high_resolution_clock::now();
  cv::Mat small_src;
  if (std::abs(scale - 1.0f) < 1e-3f) {
    small_src = frame_bgr;
  } else {
    cv::resize(frame_bgr, small_src, cv::Size(), scale, scale, (scale >= 0.85f)?cv::INTER_LINEAR:cv::INTER_AREA);
  }
  auto t_downscale_end = std::chrono::high_resolution_clock::now();

  // Compute kernel size based on blur_strength and scale
  int k = std::max(1, int((blur_strength | 1) * scale));
  if ((k % 2) == 0) ++k; // ensure odd
  if (k < 3) k = 3;
  // (BG-ACCEL debug output removed)

  // Timing: blur
  auto t_blur_start = std::chrono::high_resolution_clock::now();

  if (!use_ocl) {
    // CPU path but with background computed at reduced res
    cv::Mat small_blur; cv::blur(small_src, small_blur, cv::Size(k,k));
    auto t_blur_end = std::chrono::high_resolution_clock::now();

    // Timing: upscale
    auto t_upscale_start = std::chrono::high_resolution_clock::now();
    cv::Mat blurred;
    if (small_blur.size() != frame_bgr.size()) cv::resize(small_blur, blurred, frame_bgr.size(), 0,0, cv::INTER_LINEAR);
    else blurred = small_blur;
    auto t_upscale_end = std::chrono::high_resolution_clock::now();

    // Timing: blend prep
    auto t_blendprep_start = std::chrono::high_resolution_clock::now();
    cv::Mat frame_f, blurred_f; frame_bgr.convertTo(frame_f, CV_32FC3, 1.0/255.0); blurred.convertTo(blurred_f, CV_32FC3, 1.0/255.0);
    cv::Mat mask_f; mask_u8.convertTo(mask_f, CV_32FC1, 1.0/255.0);
    int fks = (int)std::max(1.0f, feather_px) * 2 + 1;
    auto t_blendprep_end = std::chrono::high_resolution_clock::now();

    // Timing: feather
    auto t_feather_start = std::chrono::high_resolution_clock::now();
    if (feather_px > 0.5f) cv::GaussianBlur(mask_f, mask_f, cv::Size(fks,fks), 0);
    auto t_feather_end = std::chrono::high_resolution_clock::now();

  // Timing: blend
  auto t_blend_start = std::chrono::high_resolution_clock::now();
  cv::Mat comp_u8, rgb;
  if (use_ocl) {
    // Use UMat for all steps to enable OpenCL acceleration
    cv::UMat frame_fU, blurred_fU, mask_fU, inv_mask_fU, mask3U, inv_mask3U, comp_fU, comp_u8U, rgbU;
    frame_f.copyTo(frame_fU);
    blurred_f.copyTo(blurred_fU);
    mask_f.copyTo(mask_fU);
    // Expand mask to 3 channels
    std::vector<cv::UMat> mask_channelsU(3, mask_fU);
    cv::merge(mask_channelsU, mask3U);
  cv::subtract(1.0f, mask_fU, inv_mask_fU);
    std::vector<cv::UMat> inv_mask_channelsU(3, inv_mask_fU);
    cv::merge(inv_mask_channelsU, inv_mask3U);
  // Blend on GPU
  cv::add(frame_fU.mul(mask3U), blurred_fU.mul(inv_mask3U), comp_fU);
    comp_fU.convertTo(comp_u8U, CV_8UC3, 255.0);
    cv::cvtColor(comp_u8U, rgbU, cv::COLOR_BGR2RGB);
    comp_u8U.copyTo(comp_u8);
    rgbU.copyTo(rgb);
  } else {
    // CPU path (as before)
    cv::Mat mask3, inv_mask3;
    std::vector<cv::Mat> mask_channels(3, mask_f);
    cv::merge(mask_channels, mask3);
    cv::Mat inv_mask_f = 1.0f - mask_f;
    std::vector<cv::Mat> inv_mask_channels(3, inv_mask_f);
    cv::merge(inv_mask_channels, inv_mask3);
    cv::Mat comp_f = frame_f.mul(mask3) + blurred_f.mul(inv_mask3);
    comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
    cv::cvtColor(comp_u8, rgb, cv::COLOR_BGR2RGB);
  }
  auto t_blend_end = std::chrono::high_resolution_clock::now();

  // Timing: total
  auto t_end = std::chrono::high_resolution_clock::now();

  // Print timing
  auto ms = [](auto start, auto end) { return std::chrono::duration_cast<std::chrono::microseconds>(end-start).count()/1000.0; };
  std::cout << "[BG-ACCEL-TIME] downscale=" << ms(t_downscale_start, t_downscale_end)
        << "ms, blur=" << ms(t_blur_start, t_blur_end)
        << "ms, upscale=" << ms(t_upscale_start, t_upscale_end)
        << "ms, blendprep=" << ms(t_blendprep_start, t_blendprep_end)
        << "ms, feather=" << ms(t_feather_start, t_feather_end)
        << "ms, blend=" << ms(t_blend_start, t_blend_end)
        << "ms, total=" << ms(t_start, t_end) << "ms" << std::endl;

  // Debug output for blur composite
  static int blur_debug_count = 0;
  blur_debug_count++;
  if (blur_debug_count <= 2 && !comp_u8.empty() && !rgb.empty()) {
    cv::Vec3b bgr_pixel = comp_u8.at<cv::Vec3b>(comp_u8.rows/2, comp_u8.cols/2);
    cv::Vec3b rgb_pixel = rgb.at<cv::Vec3b>(rgb.rows/2, rgb.cols/2);
    std::cout << "🔍 BLUR COMPOSITE " << blur_debug_count << " - BGR result: [" 
          << (int)bgr_pixel[0] << "," << (int)bgr_pixel[1] << "," << (int)bgr_pixel[2] 
          << "] -> RGB output: [" << (int)rgb_pixel[0] << "," << (int)rgb_pixel[1] << "," << (int)rgb_pixel[2] << "]" << std::endl;
  }

  return comp_u8;
  }
  // OpenCL path via UMat
  cv::UMat src_u; small_src.copyTo(src_u);
  cv::UMat blur_u; cv::blur(src_u, blur_u, cv::Size(k,k));
  cv::UMat blurred_u;
  if (blur_u.size() != frame_bgr.size()) cv::resize(blur_u, blurred_u, frame_bgr.size(), 0,0, cv::INTER_LINEAR);
  else blurred_u = blur_u;
  cv::UMat frame_u; frame_bgr.copyTo(frame_u);
  cv::UMat mask_u; mask_u8.copyTo(mask_u);
  cv::UMat frame_f, blurred_f, mask_f; frame_u.convertTo(frame_f, CV_32FC3, 1.0/255.0); blurred_u.convertTo(blurred_f, CV_32FC3, 1.0/255.0);
  mask_u.convertTo(mask_f, CV_32FC1, 1.0/255.0);
  if (feather_px > 0.5f) {
    int fks = (int)std::max(1.0f, feather_px) * 2 + 1; cv::GaussianBlur(mask_f, mask_f, cv::Size(fks,fks), 0);
  }
  std::vector<cv::UMat> ff(3), bf(3), out(3); cv::split(frame_f, ff); cv::split(blurred_f, bf);
  cv::UMat one(mask_f.size(), mask_f.type()); one.setTo(1.0f);
  cv::UMat inv; cv::subtract(one, mask_f, inv);
  for (int i=0;i<3;++i) {
    cv::UMat a,b; cv::multiply(ff[i], mask_f, a); cv::multiply(bf[i], inv, b); cv::add(a,b,out[i]);
  }
  cv::UMat comp_f; cv::merge(out, comp_f); cv::UMat comp_u8; comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
  cv::Mat comp_bgr; comp_u8.copyTo(comp_bgr);
  cv::Mat rgb; cv::cvtColor(comp_bgr, rgb, cv::COLOR_BGR2RGB);
  return comp_bgr;
}

// Helper functions to reduce complexity in composite functions

// Handle scale optimization for compositing
struct ScaleOptimizationResult {
  cv::Mat small_frame;
  cv::Mat small_mask;
  cv::Mat small_bg;
  bool needs_upscale;
};

ScaleOptimizationResult ApplyScaleOptimization(const cv::Mat& frame_bgr,
                                               const cv::Mat& mask_u8,
                                               const cv::Mat& bg,
                                               float scale) {
  ScaleOptimizationResult result;
  result.needs_upscale = std::abs(scale - 1.0f) >= 1e-3f;

  if (!result.needs_upscale) {
    result.small_frame = frame_bgr;
    result.small_mask = mask_u8;
    result.small_bg = bg;
  } else {
    cv::resize(frame_bgr, result.small_frame, cv::Size(), scale, scale,
               (scale >= 0.85f) ? cv::INTER_LINEAR : cv::INTER_AREA);
    cv::resize(mask_u8, result.small_mask, result.small_frame.size(), 0, 0, cv::INTER_LINEAR);
    cv::resize(bg, result.small_bg, result.small_frame.size(), 0, 0, cv::INTER_LINEAR);
  }

  return result;
}

// CPU path for blur background compositing
cv::Mat CompositeBlurBackgroundCPU(const cv::Mat& frame_bgr,
                                   const cv::Mat& mask_u8,
                                   int blur_strength,
                                   float feather_px,
                                   const cv::Mat& blurred_bg) {
  // Normalized blend (same as baseline)
  cv::Mat frame_f, blurred_f;
  frame_bgr.convertTo(frame_f, CV_32FC3, 1.0/255.0);
  blurred_bg.convertTo(blurred_f, CV_32FC3, 1.0/255.0);
  cv::Mat mask_f;
  mask_u8.convertTo(mask_f, CV_32FC1, 1.0/255.0);

  if (feather_px > 0.5f) {
    int fks = (int)std::max(1.0f, feather_px) * 2 + 1;
    cv::GaussianBlur(mask_f, mask_f, cv::Size(fks,fks), 0);
  }

  std::vector<cv::Mat> fch(3), bch(3), out(3);
  cv::split(frame_f, fch);
  cv::split(blurred_f, bch);
  for (int i=0;i<3;++i) {
    out[i] = fch[i].mul(mask_f) + bch[i].mul(1.0f - mask_f);
  }

  cv::Mat comp_f;
  cv::merge(out, comp_f);
  cv::Mat comp_u8;
  comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
  return comp_u8;
}

// OpenCL path for blur background compositing
cv::Mat CompositeBlurBackgroundOpenCL(const cv::Mat& frame_bgr,
                                      const cv::Mat& mask_u8,
                                      float feather_px,
                                      const cv::Mat& blurred_bg) {
  cv::UMat frame_u, blurred_u, mask_u;
  frame_bgr.copyTo(frame_u);
  blurred_bg.copyTo(blurred_u);
  mask_u8.copyTo(mask_u);

  cv::UMat frame_f, blurred_f, mask_f;
  frame_u.convertTo(frame_f, CV_32FC3, 1.0/255.0);
  blurred_u.convertTo(blurred_f, CV_32FC3, 1.0/255.0);
  mask_u.convertTo(mask_f, CV_32FC1, 1.0/255.0);

  if (feather_px > 0.5f) {
    int fks = (int)std::max(1.0f, feather_px) * 2 + 1;
    cv::GaussianBlur(mask_f, mask_f, cv::Size(fks,fks), 0);
  }

  std::vector<cv::UMat> ff(3), bf(3), out(3);
  cv::split(frame_f, ff);
  cv::split(blurred_f, bf);
  cv::UMat one(mask_f.size(), mask_f.type());
  one.setTo(1.0f);
  cv::UMat inv;
  cv::subtract(one, mask_f, inv);

  for (int i=0;i<3;++i) {
    cv::UMat a,b;
    cv::multiply(ff[i], mask_f, a);
    cv::multiply(bf[i], inv, b);
    cv::add(a,b,out[i]);
  }

  cv::UMat comp_f;
  cv::merge(out, comp_f);
  cv::UMat comp_u8;
  comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
  cv::Mat comp_bgr;
  comp_u8.copyTo(comp_bgr);
  return comp_bgr;
}

// Debug output for blur composite
void DebugBlurCompositeOutput(const cv::Mat& comp_u8, const cv::Mat& rgb) {
  static int blur_debug_count = 0;
  blur_debug_count++;
  if (blur_debug_count <= 2 && !comp_u8.empty() && !rgb.empty()) {
    cv::Vec3b bgr_pixel = comp_u8.at<cv::Vec3b>(comp_u8.rows/2, comp_u8.cols/2);
    cv::Vec3b rgb_pixel = rgb.at<cv::Vec3b>(rgb.rows/2, rgb.cols/2);
    std::cout << "🔍 BLUR COMPOSITE " << blur_debug_count << " - BGR result: ["
              << (int)bgr_pixel[0] << "," << (int)bgr_pixel[1] << "," << (int)bgr_pixel[2]
              << "] -> RGB output: [" << (int)rgb_pixel[0] << "," << (int)rgb_pixel[1] << "," << (int)rgb_pixel[2] << "]" << std::endl;
  }
}

cv::Mat CompositeImageBackgroundBGR(const cv::Mat& frame_bgr,
                                    const cv::Mat& mask_u8,
                                    const cv::Mat& bg_bgr) {
  cv::Mat result = frame_bgr.clone();
  cv::Mat mask_f; mask_u8.convertTo(mask_f, CV_32FC1, 1.0/255.0);
  segmecam::CompositeWithMask(result, bg_bgr, mask_f, false);
  return result;
}

cv::Mat CompositeSolidBackgroundBGR(const cv::Mat& frame_bgr,
                                    const cv::Mat& mask_u8,
                                    const cv::Scalar& bgr) {
  cv::Mat result = frame_bgr.clone();
  cv::Mat mask_f; mask_u8.convertTo(mask_f, CV_32FC1, 1.0/255.0);
  segmecam::CompositeWithSolidColor(result, bgr, mask_f, false);
  return result;
}

// Optimized image background composite with scale optimization and caching
cv::Mat CompositeImageBackgroundBGR_Accel(const cv::Mat& frame_bgr,
                                          const cv::Mat& mask_u8,
                                          const cv::Mat& bg_bgr,
                                          bool use_ocl,
                                          float scale) {
  // Static cache for resized background images
  static cv::Mat cached_bg_resized;
  static cv::Size cached_frame_size;
  static cv::Mat cached_bg_original;
  static bool cache_valid = false;
  static int debug_call_count = 0;
  
  debug_call_count++;
  
  scale = std::clamp(scale, 0.4f, 1.0f);
  
  // Debug first few calls to show optimization status
  if (debug_call_count <= 3) {
    std::cout << "🚀 IMAGE BG ACCEL " << debug_call_count << " - scale=" << scale 
              << " opencl=" << use_ocl << " frame=" << frame_bgr.cols << "x" << frame_bgr.rows << std::endl;
  }
  
  // Check if we can use cached background
  bool need_resize = false;
  if (!cache_valid || 
      cached_frame_size != frame_bgr.size() ||
      cached_bg_original.data != bg_bgr.data ||
      cached_bg_original.size() != bg_bgr.size()) {
    need_resize = true;
    cache_valid = true;
    cached_frame_size = frame_bgr.size();
    cached_bg_original = bg_bgr.clone(); // Store copy for comparison
    if (debug_call_count <= 3) {
      std::cout << "🔄 IMAGE BG CACHE MISS - resizing background" << std::endl;
    }
  } else if (debug_call_count <= 3) {
    std::cout << "✅ IMAGE BG CACHE HIT - using cached background" << std::endl;
  }
  
  // Resize background if needed
  if (need_resize) {
    cv::resize(bg_bgr, cached_bg_resized, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  }
  
  // If scale optimization is disabled or OpenCL not available, use standard path
  if (!use_ocl && std::abs(scale - 1.0f) < 1e-3f) {
    return CompositeImageBackgroundBGR(frame_bgr, mask_u8, cached_bg_resized);
  }
  
  // Scale optimization: work at reduced resolution for compositing
  cv::Mat small_frame, small_mask, small_bg;
  if (std::abs(scale - 1.0f) < 1e-3f) {
    small_frame = frame_bgr;
    small_mask = mask_u8;
    small_bg = cached_bg_resized;
  } else {
    cv::resize(frame_bgr, small_frame, cv::Size(), scale, scale, 
               (scale >= 0.85f) ? cv::INTER_LINEAR : cv::INTER_AREA);
    cv::resize(mask_u8, small_mask, small_frame.size(), 0, 0, cv::INTER_LINEAR);
    cv::resize(cached_bg_resized, small_bg, small_frame.size(), 0, 0, cv::INTER_LINEAR);
  }
  
  return PerformCompositingWithUpscale(small_frame, small_mask, small_bg, frame_bgr.size());
}

// Optimized solid color background composite with scale optimization and caching
cv::Mat CompositeSolidBackgroundBGR_Accel(const cv::Mat& frame_bgr,
                                          const cv::Mat& mask_u8,
                                          const cv::Scalar& bgr,
                                          bool use_ocl,
                                          float scale) {
  // Static cache for solid color matrices
  static cv::Mat cached_solid_bg;
  static cv::Size cached_size;
  static cv::Scalar cached_color;
  static bool cache_valid = false;
  static int debug_call_count = 0;
  
  debug_call_count++;
  
  scale = std::clamp(scale, 0.4f, 1.0f);
  
  // Debug first few calls to show optimization status
  if (debug_call_count <= 3) {
    std::cout << "🚀 SOLID BG ACCEL " << debug_call_count << " - scale=" << scale 
              << " opencl=" << use_ocl << " frame=" << frame_bgr.cols << "x" << frame_bgr.rows << std::endl;
  }
  
  // Check if we can use cached solid background
  bool need_create = false;
  if (!cache_valid || 
      cached_size != frame_bgr.size() ||
      cached_color != bgr) {
    need_create = true;
    cache_valid = true;
    cached_size = frame_bgr.size();
    cached_color = bgr;
    if (debug_call_count <= 3) {
      std::cout << "🔄 SOLID BG CACHE MISS - creating solid matrix" << std::endl;
    }
  } else if (debug_call_count <= 3) {
    std::cout << "✅ SOLID BG CACHE HIT - using cached matrix" << std::endl;
  }
  
  // Create solid background if needed
  if (need_create) {
    cached_solid_bg = cv::Mat(frame_bgr.size(), CV_8UC3, bgr);
  }
  
  // If scale optimization is disabled or OpenCL not available, use standard path
  if (!use_ocl && std::abs(scale - 1.0f) < 1e-3f) {
    return CompositeSolidBackgroundBGR(frame_bgr, mask_u8, bgr);
  }
  
  // Scale optimization: work at reduced resolution for compositing
  cv::Mat small_frame, small_mask, small_bg;
  if (std::abs(scale - 1.0f) < 1e-3f) {
    small_frame = frame_bgr;
    small_mask = mask_u8;
    small_bg = cached_solid_bg;
  } else {
    cv::resize(frame_bgr, small_frame, cv::Size(), scale, scale, 
               (scale >= 0.85f) ? cv::INTER_LINEAR : cv::INTER_AREA);
    cv::resize(mask_u8, small_mask, small_frame.size(), 0, 0, cv::INTER_LINEAR);
    small_bg = cv::Mat(small_frame.size(), CV_8UC3, bgr);
  // (BG-ACCEL debug output removed)
  }
  
  return PerformCompositingWithUpscale(small_frame, small_mask, small_bg, frame_bgr.size());
}
