# SegmeCam

[![Build](https://github.com/Padletut/SegmeCam/actions/workflows/build.yml/badge.svg)](https://github.com/Padletut/SegmeCam/actions)
[![Release](https://img.shields.io/github/v/release/Padletut/SegmeCam?color=brightgreen&logo=github)](https://github.com/Padletut/SegmeCam/releases)
[![License](https://img.shields.io/github/license/Padletut/SegmeCam?color=blue)](LICENSE)


🎥 **SegmeCam** — the first AI-powered Linux webcam app that combines **Selfie Segmentation** and **Face Landmark Detection** for real-time professional effects.  
Built with **TensorFlow Lite**, **SDL2 + OpenGL 3.3**, and **Dear ImGui**, SegmeCam brings studio-grade features to your Linux desktop.

---

## ✨ Features
- 🤖 **Selfie Segmentation** – Accurate separation of person and background
- 📍 **Face Landmark Detection** – 100+ keypoints for precise effects
- 🖼️ **Background Control** – Blur, color, or custom image backgrounds
- 💄 **Beauty Filters** – Skin smoothing, wrinkle-aware, makeup overlays
- 😁 **Teeth Whitening** – Landmark-driven whitening masks
- 👄 **Lip Refinement** – Subtle reshaping and coloring
- 🎚️ **Realtime Controls** – Sliders and toggles powered by Dear ImGui
- 🎥 **Virtual Webcam Output** – Works in Discord, OBS, Zoom, Teams
- ⚡ **Optimized for Linux** – GPU-accelerated pipeline with TFLite XNNPACK

---

## 🔍 Why SegmeCam?

Unlike other Linux webcam apps, SegmeCam is a **complete AI camera suite**.  
Here’s how it compares:

| Feature | **SegmeCam** | OBS + BackgroundRemoval | Webcamoid | Zoom / Meet (Linux) |
|---------|--------------|--------------------------|-----------|----------------------|
| AI Selfie Segmentation | ✅ Yes (TFLite GPU) | ✅ Yes (plugin) | ❌ No | ✅ Yes (built-in) |
| Background Blur/Replace | ✅ Blur, Color, Custom Image | ✅ Blur/Replace | ❌ No | ✅ Blur/Replace |
| Face Landmark Detection | ✅ 100+ points | ❌ No | ❌ No | ❌ No |
| Skin Smoothing | ✅ Wrinkle-aware filter | ❌ No | ❌ No | ❌ No |
| Lip/Makeup Effects | ✅ Lip refiner, blush | ❌ No | ❌ No | ❌ No |
| Teeth Whitening | ✅ Yes | ❌ No | ❌ No | ❌ No |
| Virtual Webcam Output | ✅ v4l2loopback | ✅ OBS VirtualCam | ❌ No | ❌ No |
| Open Source | ✅ Apache-2.0 | ✅ GPL | ✅ GPL | ❌ No |

👉 SegmeCam is the **first all-in-one AI beauty + background app for Linux** 🚀

---

## 🛠️ Tech Stack
- **Core AI**: MediaPipe Selfie Segmentation + Face Landmarker (TFLite)
- **Build System**: Bazel (TFLite, dependencies) + C++ project build
- **Performance**: XNNPACK delegate, optional GPU delegate
- **Computer Vision**: OpenCV (camera I/O, pre/post-processing)
- **UI / Rendering**: SDL2 + OpenGL 3.3 + Dear ImGui
- **Packaging**: AppImage & Flatpak

---

## 🗺️ Roadmap
- [x] ✅ Selfie segmentation with background blur/replace
- [x] ✅ Face landmark detection (100+ keypoints)
- [x] ✅ Teeth whitening via LAB masks
- [x] ✅ Lip refinement / makeup overlay
- [x] ✅ Wrinkle-aware skin smoothing
- [ ] 🎭 Fun filters (masks, sunglasses, hats)
- [x] ✅ Profile system for saving favorite presets
- [x] ✅ Virtual webcam integration (v4l2loopback)
- [ ] 📦 Flatpak release on Flathub
- [ ] 🌐 Backend mode for streaming segmentation results

---

## 🚀 Quick Start
1. **Clone repo**:  
   ```bash
   git clone https://github.com/Padletut/SegmeCam.git
   cd SegmeCam
   ```
2. **Build with Bazel**:  
   ```bash
   bazel build //...
   ```
3. **Run**:  
   ```bash
   ./bazel-bin/segmecam
   ```

> ⚠️ Requires GLIBC 2.38+ (Ubuntu 24.04+, Fedora 40+, Arch latest).  
> Install `v4l2loopback-dkms` for virtual webcam output.

---

## 🎯 Goals
- Native **AI-powered background segmentation**
- **Face landmark-based beauty filters** (skin smoothing, whitening, makeup)
- Professional Linux alternative to Windows-only beauty camera apps
- Optimized GPU-driven pipeline for **low CPU usage**

---

## 🙏 Credits
Built with inspiration from [MediaPipe](https://ai.google.dev/edge/mediapipe).  
Apache-2.0 License.