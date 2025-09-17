#include "segmecam_composite.h"

// Persistently remembered preferred channel when mask comes as 4xU8 (SRGBA).
// This avoids per-frame channel switches that can look like flicker.
static int g_rgba_mask_channel = -1; // 0=B,1=G,2=R,3=A

cv::Mat DecodeMaskToU8(const mediapipe::ImageFrame& mask, bool* logged_once) {
  const int ch = mask.NumberOfChannels();
  const int bd = mask.ByteDepth();
  cv::Mat out;
  if (ch == 1 && bd == 1) {
    cv::Mat m(mask.Height(), mask.Width(), CV_8UC1,
              const_cast<uint8_t*>(mask.PixelData()), mask.WidthStep());
    out = m.clone();
  } else if (ch == 1 && bd == 4) {
    cv::Mat mf(mask.Height(), mask.Width(), CV_32FC1,
               const_cast<uint8_t*>(mask.PixelData()), mask.WidthStep());
    mf.convertTo(out, CV_8UC1, 255.0);
  } else if (ch == 4 && bd == 1) {
    cv::Mat rgba(mask.Height(), mask.Width(), CV_8UC4,
                 const_cast<uint8_t*>(mask.PixelData()), mask.WidthStep());
    std::vector<cv::Mat> chm; cv::split(rgba, chm);
    cv::Scalar mb = cv::mean(chm[0]);
    cv::Scalar mg = cv::mean(chm[1]);
    cv::Scalar mr = cv::mean(chm[2]);
    cv::Scalar ma = cv::mean(chm[3]);
    int best = g_rgba_mask_channel;
    if (best < 0) {
      // Find the channel with the highest mean value (most likely to contain segmentation data)
      // Prefer non-alpha channels over alpha channel to avoid blue tint issues
      best = 0; double bestv = mb[0];
      if (mg[0] > bestv) { best=1; bestv=mg[0]; }
      if (mr[0] > bestv) { best=2; bestv=mr[0]; }
      // Only use alpha if it's significantly better than color channels
      if (ma[0] > bestv + 10.0) { best=3; bestv=ma[0]; }
      g_rgba_mask_channel = best;
    }
    out = chm[best].clone();
    if (logged_once && !*logged_once) {
      std::cout << "Mask channels=4 byteDepth=1 means[B,G,R,A]="
                << mb[0] << "," << mg[0] << "," << mr[0] << "," << ma[0]
                << " chosen=" << (g_rgba_mask_channel==0?"B":g_rgba_mask_channel==1?"G":g_rgba_mask_channel==2?"R":"A")
                << std::endl;
      *logged_once = true;
    }
  } else {
    // Fallback treat as float
    cv::Mat mf(mask.Height(), mask.Width(), CV_32FC1,
               const_cast<uint8_t*>(mask.PixelData()), mask.WidthStep());
    mf.convertTo(out, CV_8UC1, 255.0);
  }
  return out;
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
  cv::Mat frame_f; frame_bgr.convertTo(frame_f, CV_32FC3, 1.0/255.0);
  cv::Mat mask_f; mask_u8.convertTo(mask_f, CV_32FC1, 1.0/255.0);
  // Feather
  int fks = (int)std::max(1.0f, feather_px) * 2 + 1;
  if (feather_px > 0.5f) cv::GaussianBlur(mask_f, mask_f, cv::Size(fks,fks), 0);
  cv::Mat bg_mask = 1.0f - mask_f;
  int k = blur_strength | 1;
  std::vector<cv::Mat> fch, out_ch(3);
  cv::split(frame_f, fch);
  for (int i=0;i<3;++i) {
    cv::Mat bg_only = normalizedMaskedBlurChannel(fch[i], bg_mask, k);
    out_ch[i] = fch[i].mul(mask_f) + bg_only.mul(1.0f - mask_f);
  }
  cv::Mat comp_f; cv::merge(out_ch, comp_f);
  cv::Mat comp_u8; comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
  cv::Mat rgb; cv::cvtColor(comp_u8, rgb, cv::COLOR_BGR2RGB);
  return rgb;
}

cv::Mat CompositeBlurBackgroundBGR_Accel(const cv::Mat& frame_bgr,
                                         const cv::Mat& mask_u8,
                                         int blur_strength,
                                         float feather_px,
                                         bool use_ocl,
                                         float scale) {
  scale = std::clamp(scale, 0.4f, 1.0f);
  if (!use_ocl && std::abs(scale - 1.0f) < 1e-3f) {
    return CompositeBlurBackgroundBGR(frame_bgr, mask_u8, blur_strength, feather_px);
  }
  int k = blur_strength | 1;
  // Optional downscale for speed
  cv::Mat small_src;
  if (std::abs(scale - 1.0f) < 1e-3f) small_src = frame_bgr;
  else cv::resize(frame_bgr, small_src, cv::Size(), scale, scale, (scale >= 0.85f)?cv::INTER_LINEAR:cv::INTER_AREA);

  if (!use_ocl) {
    // CPU path but with background computed at reduced res
    cv::Mat small_blur; cv::GaussianBlur(small_src, small_blur, cv::Size(k,k), 0);
    cv::Mat blurred;
    if (small_blur.size() != frame_bgr.size()) cv::resize(small_blur, blurred, frame_bgr.size(), 0,0, cv::INTER_LINEAR);
    else blurred = small_blur;
    // Normalized blend (same as baseline)
    cv::Mat frame_f, blurred_f; frame_bgr.convertTo(frame_f, CV_32FC3, 1.0/255.0); blurred.convertTo(blurred_f, CV_32FC3, 1.0/255.0);
    cv::Mat mask_f; mask_u8.convertTo(mask_f, CV_32FC1, 1.0/255.0);
    int fks = (int)std::max(1.0f, feather_px) * 2 + 1; if (feather_px > 0.5f) cv::GaussianBlur(mask_f, mask_f, cv::Size(fks,fks), 0);
    std::vector<cv::Mat> fch(3), bch(3), out(3); cv::split(frame_f, fch); cv::split(blurred_f, bch);
    for (int i=0;i<3;++i) out[i] = fch[i].mul(mask_f) + bch[i].mul(1.0f - mask_f);
    cv::Mat comp_f; cv::merge(out, comp_f); cv::Mat comp_u8; comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
    cv::Mat rgb; cv::cvtColor(comp_u8, rgb, cv::COLOR_BGR2RGB);
    
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
    
    return rgb;
  }
  // OpenCL path via UMat
  cv::UMat src_u; small_src.copyTo(src_u);
  cv::UMat blur_u; cv::GaussianBlur(src_u, blur_u, cv::Size(k,k), 0);
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
  return rgb;
}


cv::Mat CompositeImageBackgroundBGR(const cv::Mat& frame_bgr,
                                    const cv::Mat& mask_u8,
                                    const cv::Mat& bg_bgr) {
  cv::Mat bg_resized; cv::resize(bg_bgr, bg_resized, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  cv::Mat frame_f, bg_f; frame_bgr.convertTo(frame_f, CV_32FC3, 1.0/255.0); bg_resized.convertTo(bg_f, CV_32FC3, 1.0/255.0);
  cv::Mat mask_f; mask_u8.convertTo(mask_f, CV_32FC1, 1.0/255.0);
  std::vector<cv::Mat> fch, bch, cch; cv::split(frame_f, fch); cv::split(bg_f, bch);
  cch.resize(3);
  for (int i=0;i<3;++i) cch[i] = fch[i].mul(mask_f) + bch[i].mul(1.0 - mask_f);
  cv::Mat comp_f; cv::merge(cch, comp_f);
  cv::Mat comp_u8; comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
  cv::Mat rgb; cv::cvtColor(comp_u8, rgb, cv::COLOR_BGR2RGB);
  return rgb;
}

cv::Mat CompositeSolidBackgroundBGR(const cv::Mat& frame_bgr,
                                    const cv::Mat& mask_u8,
                                    const cv::Scalar& bgr) {
  cv::Mat bg(frame_bgr.size(), CV_8UC3, bgr);
  cv::Mat frame_f, bg_f; frame_bgr.convertTo(frame_f, CV_32FC3, 1.0/255.0); bg.convertTo(bg_f, CV_32FC3, 1.0/255.0);
  cv::Mat mask_f; mask_u8.convertTo(mask_f, CV_32FC1, 1.0/255.0);
  std::vector<cv::Mat> fch, bch, cch; cv::split(frame_f, fch); cv::split(bg_f, bch);
  cch.resize(3);
  for (int i=0;i<3;++i) cch[i] = fch[i].mul(mask_f) + bch[i].mul(1.0 - mask_f);
  cv::Mat comp_f; cv::merge(cch, comp_f);
  cv::Mat comp_u8; comp_f.convertTo(comp_u8, CV_8UC3, 255.0);
  cv::Mat rgb; cv::cvtColor(comp_u8, rgb, cv::COLOR_BGR2RGB);
  return rgb;
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
  
  // Perform compositing at reduced scale
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
  if (comp_u8.size() != frame_bgr.size()) {
    cv::resize(comp_u8, final_comp, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  } else {
    final_comp = comp_u8;
  }
  
  cv::Mat rgb;
  cv::cvtColor(final_comp, rgb, cv::COLOR_BGR2RGB);
  return rgb;
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
  }
  
  // Perform compositing at reduced scale
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
  if (comp_u8.size() != frame_bgr.size()) {
    cv::resize(comp_u8, final_comp, frame_bgr.size(), 0, 0, cv::INTER_LINEAR);
  } else {
    final_comp = comp_u8;
  }
  
  cv::Mat rgb;
  cv::cvtColor(final_comp, rgb, cv::COLOR_BGR2RGB);
  return rgb;
}
