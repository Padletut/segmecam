# SegmeCam

🎥 **SegmeCam** is an AI-powered Linux desktop webcam app that combines **Selfie Segmentation** and **Face Landmark Detection** for professional-grade real-time effects.  
Built with **TensorFlow Lite** (TFLite), **SDL2**, **OpenGL 3.3**, and **Dear ImGui**, SegmeCam provides natural background blur, custom backgrounds, and precise beauty enhancements such as skin smoothing, makeup, and teeth whitening.

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
- **Build System**: **Bazel**
- **Performance**: XNNPACK delegate (fast CPU inference), optional GPU delegate
- **Computer Vision**: OpenCV (camera input, pre/post-processing)
- **UI / Rendering**: SDL2 + OpenGL 3.3 + Dear ImGui
- **Packaging**: AppImage & Flatpak for Linux distribution

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

## 📦 Building from Source
> **Note:** SegmeCam is in active development. Pre-built binaries (AppImage/Flatpak) will be released later.

### Prerequisites (Ubuntu example)
```bash
sudo apt update
sudo apt install build-essential bazel libopencv-dev libsdl2-dev libgl1-mesa-dev
```
### Clone and Build
```bash
git clone https://github.com/<your-username>/SegmeCam.git
cd SegmeCam
bazel build //:segmecam
```

### Run
```bash
bazel-bin/segmecam
```

## 🎯 Goals

- SegmeCam aims to become the first open-source, professional-grade Linux camera app with:
- Native AI-powered background segmentation
- Facial landmark-based beauty filters (skin smoothing, teeth whitening, makeup)
- Virtual camera support for streaming and video calls
- Smooth, customizable controls via ImGui

## 🙏 Credits

SegmeCam builds on:

- [TensorFlow Lite](https://ai.google.dev/edge/litert)
- [MediaPipe Modles](https://ai.google.dev/edge/mediapipe/solutions/guide)
- [SDL2](https://www.libsdl.org/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [OpenGL](https://www.opengl.org/)

## 📜 License

SegmeCam is licensed under the Apache-2.0 License.
