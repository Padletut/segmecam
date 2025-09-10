# SegmeCam

SegmeCam is a Linux desktop webcam app that uses MediaPipe Selfie Segmentation on the GPU and renders a real‑time preview via SDL2, OpenGL 3.3, and Dear ImGui. Current focus is high‑quality background effects with a simple, fast UI.

---

## Features
- Selfie segmentation (GPU) with CPU mask output
- Background modes: None, Blur, Image, Solid Color
- Realtime controls (blur strength, color, image loader)
- OpenCV camera capture with V4L2 fallback

---

## Tech Stack
- MediaPipe (GPU calculators + graphs)
- OpenCV (capture, image processing)
- SDL2 + OpenGL 3.3 + Dear ImGui (UI/rendering)
- Bazel (build inside the MediaPipe repo)

---

## Project Layout
- `src/segmecam_gui_gpu/`: App sources and BUILD file
  - `segmecam_gui_gpu.cpp`: UI, camera, graph wiring
  - `segmecam_composite.{h,cc}`: CPU mask decode + compositing
- `mediapipe_graphs/`: Local graphs (e.g. `selfie_seg_gpu_mask_cpu.pbtxt`)
- `scripts/`: Utilities
  - `mediapipe_build_selfie_seg_gpu.sh`: Clone MediaPipe and build GUI example
  - `run_segmecam_gui_gpu.sh`: Run the app (ensures runfiles/model)
  - `clean.sh`: Remove local build artifacts
- `external/`: External checkouts (e.g. `mediapipe/`) created by scripts
- `BUILD` / `WORKSPACE` / `MODULE.bazel`: Minimal scaffolding for tooling

---

## Build & Run
> Tested on Ubuntu 22.04+ with system OpenGL drivers installed.

### Prerequisites (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y build-essential git curl pkg-config bazel \
  libopencv-dev libsdl2-dev libgl1-mesa-dev
```

### One‑time setup
1) Clone this repository and enter it:
```bash
git clone https://github.com/Padletut/SegmeCam.git
cd SegmeCam
```

2) (Optional) Place the model locally so builds/runs don’t fetch it:
```bash
# Expected by our scripts; copied into MediaPipe tree when present
mkdir -p models
# Put the file at: models/selfie_segmenter.tflite
```

### Build
```bash
scripts/mediapipe_build_selfie_seg_gpu.sh
```
This script:
- Clones `external/mediapipe` if missing
- Ensures OpenCV is discoverable via `pkg-config`
- Copies `models/selfie_segmenter.tflite` into MediaPipe if present
- Builds the GPU selfie segmentation example and SegmeCam GUI target

### Link example into MediaPipe (first time only)
If the example symlink does not exist yet, create it and rebuild:
```bash
ln -s "$PWD/src/segmecam_gui_gpu" external/mediapipe/mediapipe/examples/desktop/segmecam_gui_gpu
scripts/mediapipe_build_selfie_seg_gpu.sh
```

### Run
```bash
scripts/run_segmecam_gui_gpu.sh            # uses our default graph
scripts/run_segmecam_gui_gpu.sh --rebuild  # force a clean rebuild/run
```
The app accepts optional CLI args when run by Bazel:
- `graph_path`: defaults to `mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt`
- `resource_root_dir`: pass MediaPipe repo root if needed (default: `.`)
- `camera_index`: default `0`

Controls available in the UI:
- Toggle mask visualization
- Background mode: None / Blur / Image / Solid Color
- Blur kernel size, color picker, image path loader
- FPS and texture size overlay

Face effects (optional):
- Build/run with the combined graph to enable landmark-based effects:
  - Graph: `mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt`
  - Adds `multi_face_landmarks` stream used for lipstick, skin smoothing, teeth whitening
  - If not present, the UI shows a notice and hides controls

---

## Cleanup
```bash
scripts/clean.sh
```
Removes local binaries and Bazel outputs (`bazel-*`). External checkouts remain.

---

## Troubleshooting
- OpenCV not found: ensure `opencv4.pc` is visible to `pkg-config`. Set `PKG_CONFIG_PATH` (e.g. `/usr/local/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig`).
- Camera won’t open: the app tries V4L2 first, then default backend. Check device permissions and index.
- Blank/black preview: confirm GPU/GL drivers and that SDL2 + OpenGL packages are installed.
- Model missing: place `models/selfie_segmenter.tflite` so the scripts can copy it to MediaPipe.

---

## Roadmap
- [x] Real‑time selfie segmentation (GPU mask → CPU)
- [x] Background blur / image / solid color compositing
- [x] Face landmarks for beauty effects,anti-wrinkles,Lipstick effect
- [x] Skin smoothing, teeth whitening, makeup
- [ ] Profiles (save/load settings)
- [x] Virtual webcam via v4l2loopback
- [ ] Flatpak packaging

---

## Credits
SegmeCam builds on:
- TensorFlow Lite / MediaPipe
- SDL2
- Dear ImGui
- OpenGL

## License
SegmeCam is licensed under the Apache-2.0 License.
