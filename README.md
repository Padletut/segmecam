# SegmeCam

🎥 **SegmeCam** is an AI-powered Linux desktop web camera app that combines **Selfie Segmentation** and **Face Landmark Detection** for professional-grade real-time effects.  
Built with **TensorFlow Lite (TFLite)** and **Bazel**, SegmeCam provides natural background blur, custom backgrounds, and precise beauty enhancements such as skin smoothing, makeup, and teeth whitening.

---

## ✨ Features
- 🤖 **Selfie Segmentation** – Accurate person/background separation in real-time
- 📍 **Face Landmark Detection** – 100+ facial keypoints for precise filters and effects
- 🖼️ **Background Control** – Blur or replace your background with custom images
- 💄 **Beauty Filters** – Skin smoothing, lighting adjustment, makeup overlays
- 😁 **Teeth Whitening** – Subtle LAB-based whitening using landmark-based masks
- 🎚️ **Realtime Controls** – Adjust filter intensity and effects on the fly
- 🎥 **Virtual Webcam Output** – Use in Discord, OBS, Zoom, Teams, and more
- ⚡ **Optimized for Linux** – Built on TFLite + XNNPACK for fast inference on CPU, optional GPU delegates

---

## 🛠️ Tech Stack
- **Core AI**:  
  - [MediaPipe Selfie Segmentation (TFLite)](https://developers.google.com/mediapipe)  
  - [MediaPipe Face Landmark Model (TFLite)](https://developers.google.com/mediapipe/solutions/vision/face_landmarker)
- **Build System**: Bazel (lightweight build for TFLite + delegates)
- **Performance**: XNNPACK delegate (fast CPU inference), GPU acceleration where available
- **Computer Vision**: OpenCV (mask post-processing, blending, LAB whitening)
- **UI / Pipeline**: gpupixel-based rendering + ImGui controls
- **Packaging**: AppImage & Flatpak for easy Linux distribution

---

## 🚀 Roadmap
- [x] Real-time selfie segmentation (TFLite + XNNPACK)
- [x] Face landmark detection for detailed effects
- [ ] Background blur & replacement with custom images
- [ ] Skin smoothing with detail preservation
- [ ] Teeth whitening via LAB color correction
- [ ] Makeup effects (lips, blush, eyeliner)
- [ ] Face reshaping (jawline, nose, cheekbones)
- [ ] Profile system for saving favorite settings
- [ ] Flatpak release on Flathub

---

## 📦 Installation
> **Note:** SegmeCam is in active development. Instructions will be updated as soon as pre-built packages are available.

For now, build from source with **Bazel**:

```bash
git clone https://github.com/<your-username>/SegmeCam.git
cd SegmeCam
# Build TensorFlow Lite + delegates
bazel build -c opt //tensorflow/lite:libtensorflowlite.so //tensorflow/lite/delegates/xnnpack:xnnpack_delegate
# Build SegmeCam
bazel build //segmecam:app
