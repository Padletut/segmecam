# SegmeCam

This will create a fresh image named `segmecam:latest` for use with the run commands below.

🎥 **SegmeCam** is an AI-powered Linux desktop webcam app that combines **Selfie Segmentation** and **Face Landmark Detection** for professional-grade real-time effects.  
Built with **TensorFlow Lite** (TFLite), **SDL2**, **OpenGL 3.3**, and **Dear ImGui**, SegmeCam provides natural background blur, custom backgrounds, and precise beauty enhancements such as skin smoothing, makeup, and teeth whitening.

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
2. **Build & Run (Recommended)**:  
   ```bash
   ./scripts/run_segmecam_gui_gpu.sh --face
   ```
   This script handles Bazel builds and launches the SegmeCam GUI with face segmentation enabled.

> ⚠️ Requires GLIBC 2.38+ (Ubuntu 24.04+, Fedora 40+, Arch latest).  
> Install `v4l2loopback-dkms` for virtual webcam output.

## Building the Docker Image
## Docker Permissions: Using the docker Group

To run Docker commands without sudo, add your user to the docker group:

```bash
sudo usermod -aG docker $USER
```

After running this command, log out and log back in, or run:

```bash
newgrp docker
```

This reloads your group membership so you can use Docker without sudo.

To build the SegmeCam Docker image, run the following command in the project root (where the Dockerfile is located):

```bash
docker compose build
```

## Running SegmeCam in Docker with GPU Support

### NVIDIA GPU (nvidia-docker2 required)
```bash
   docker run --rm -it --gpus all \
   --device /dev/video0:/dev/video0 \
   --device /dev/video1:/dev/video1 \
   -e DISPLAY=$DISPLAY \
   -v /tmp/.X11-unix:/tmp/.X11-unix \
   -v "$(pwd)":/workspace \
   segmecam:latest
```

### Intel/AMD GPU (Mesa, DRI)
```bash
   docker run --rm -it \
   --device /dev/video0:/dev/video0 \
   --device /dev/video1:/dev/video1 \
   --device /dev/dri:/dev/dri \
   -e DISPLAY=$DISPLAY \
   -v /tmp/.X11-unix:/tmp/.X11-unix \
   -v "$(pwd)":/workspace \
   segmecam:latest
```

> For NVIDIA, install [nvidia-docker2](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html) and ensure host drivers are up to date.
> For Intel/AMD, ensure Mesa and DRI devices are available on the host.

---

## 🎯 Goals
- Native **AI-powered background segmentation**
- **Face landmark-based beauty filters** (skin smoothing, whitening, makeup)
- Professional Linux alternative to Windows-only beauty camera apps
- Optimized GPU-driven pipeline for **low CPU usage**

---

## 🙏 Credits

SegmeCam builds on:

- [TensorFlow Lite](https://ai.google.dev/edge/litert)
- [MediaPipe Modles](https://ai.google.dev/edge/mediapipe/solutions/guide)
- [SDL2](https://www.libsdl.org/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [OpenGL](https://www.opengl.org/)

## 📜 License

SegmeCam is licensed under the Apache-2.0 License.

docker run --rm -it --gpus all   -e DISPLAY=$DISPLAY -e QT_X11_NO_MITSHM=1 -e XDG_RUNTIME_DIR=/tmp/xdg   -v /tmp/.X11-unix:/tmp/.X11-unix:ro   -v segmecam_profiles:/root/.config/segmecam   --device /dev/dri:/dev/dri --device /dev/video0:/dev/video0 --device /dev/video2:/dev/video2   --entrypoint /opt/segmecam/bin/SegmeCam   segmecam:prod   /opt/segmecam/graphs/face_and_seg_gpu_mask_cpu.pbtxt   /opt/segmecam/runfiles/mediapipe   0

# To run the app directly after it has compiled with run_segmecam_gui_gpu.sh

external/mediapipe$ bazel-bin/mediapipe/examples/desktop/segmecam_gui_gpu/segmecam_gui_gpu $HOME/segmecam/mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt bazel-bin/mediapipe/examples/desktop/segmecam_gui_gpu $HOME/segmecam/bazel-bin/mediapipe/examples/desktop/segmecam_gui_gpu/segmecam_gui_gpu.runfiles 0