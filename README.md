# SegmeCam

🎥 **SegmeCam** is an AI-powered Linux desktop web camera app that combines **Selfie Segmentation** and **Face Landmark Detection** for professional-grade real-time effects.  
Built with **TensorFlow Lite** (TFLite) and a custom **SDL2 + OpenGL + Dear ImGui pipeline**, SegmeCam provides natural background blur, custom backgrounds, and precise beauty enhancements such as skin smoothing, makeup, and teeth whitening.

---

## ✨ Features
- 🤖 **Selfie Segmentation** – Accurate person/background separation in real-time
- 📍 **Face Landmark Detection** – 100+ facial keypoints for detailed effects
- 🖼️ **Background Control** – Blur or replace your background with custom images
- 💄 **Beauty Filters** – Skin smoothing, lighting adjustment, makeup overlays
- 😁 **Teeth Whitening** – Subtle LAB-based whitening using landmark-based masks
- 🎚️ **Realtime Controls** – Sliders and toggles powered by Dear ImGui
- 🎥 **Virtual Webcam Output** – Use in Discord, OBS, Zoom, Teams, and more
- ⚡ **Optimized for Linux** – SDL2 + OpenGL rendering, TFLite with XNNPACK delegate

---

## 🛠️ Tech Stack
- **Core AI**:  
  - [MediaPipe Selfie Segmentation (TFLite)](https://developers.google.com/mediapipe)  
  - [MediaPipe Face Landmark Model (TFLite)](https://developers.google.com/mediapipe/solutions/vision/face_landmarker)
- **Build System**: CMake (with Bazel for building TFLite if needed)
- **Performance**: XNNPACK delegate (fast CPU inference), optional GPU delegate
- **Computer Vision**: OpenCV (camera input, pre/post-processing)
- **UI / Rendering**: SDL2 + OpenGL 3.3 + Dear ImGui
- **Packaging**: AppImage & Flatpak for easy Linux distribution

---

## 🚀 Roadmap
- [x] Real-time selfie segmentation (TFLite + XNNPACK)
- [x] Face landmark detection for detailed effects
- [ ] Background blur & replacement with custom images
- [ ] Skin smoothing with detail preservation
- [ ] Teeth whitening via LAB color correction
- [ ] Makeup effects (lips, blush, eyeliner)
- [ ] Profile system for saving favorite settings
- [ ] Virtual webcam integration via v4l2loopback
- [ ] Flatpak release on Flathub

---

## 📦 Installation
> **Note:** SegmeCam is in active development. Instructions will be updated as soon as pre-built packages are available.

For now, build from source:

```bash
git clone https://github.com/<your-username>/SegmeCam.git
cd SegmeCam
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 🎯 Goals

- SegmeCam aims to become the first open-source, professional-grade Linux camera app with:
- Native AI-powered background segmentation
- Facial landmark-based beauty filters (skin smoothing, teeth whitening, makeup)
- Virtual camera support for streaming and video calls
- Smooth, customizable controls via ImGui