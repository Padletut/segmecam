

# 🧑‍💻 Copilot Instructions for SegmeCam

## Project Overview
SegmeCam is a Linux-native AI webcam app built in C++17, using Bazel for builds. It combines real-time selfie segmentation and face landmark detection (TensorFlow Lite/MediaPipe), OpenGL/SDL2 rendering, and Dear ImGui for UI. The goal is professional beauty and background effects for Linux video calls and streaming.

## Architecture & Major Components
- `engine/` – AI inference (TFLite models, mask processing)
- `render/` – OpenGL passes (blur, smoothing, composition)
- `ui/` – Dear ImGui panels & controls
- `io/` – camera input (OpenCV/V4L2) + v4l2loopback output
- Bazel `WORKSPACE` and `BUILD` files manage all builds and dependencies


## Developer Workflows
- **Build & Run (Recommended):**
  ```bash
  ./scripts/run_segmecam_gui_gpu.sh --face
  ```
  This script handles Bazel builds and launches the SegmeCam GUI with face segmentation enabled.
- **Manual Bazel Build (Advanced):**
  ```bash
  bazel build //...
  ./bazel-bin/segmecam
  ```
- **Add dependencies:**
  Use Bazel repository rules in `WORKSPACE` and update `BUILD` files. Do not switch to CMake or other build systems unless explicitly requested.
- **Testing:**
  Unit tests for engine utilities are encouraged; follow modular patterns.

## Project-Specific Conventions
- **Language:** C++17, default `clang-format` (LLVM style)
- **Headers:** `.hpp` for headers, `.cpp` for implementation
- **Naming:** `snake_case` for functions/vars, `CamelCase` for classes
- **Modularity:** Each module has a single responsibility; avoid “god classes”
- **Layering:** Keep `engine`, `render`, `ui`, and `io` responsibilities separate
- **Linux-first:** Prioritize Linux compatibility; avoid Windows/Mac APIs unless requested
- **Licensing:** Only Apache-2.0 compatible dependencies; avoid GPL

## Integration Points
- **TensorFlow Lite:** Integrated via Bazel; models loaded at runtime
- **OpenCV:** For image processing and blending
- **Dear ImGui:** For GUI controls and rendering pipeline
- **v4l2loopback:** For virtual webcam output

## Example: Adding a New Effect
1. Add C++ source in the appropriate module directory and header in `include/`
2. Update Bazel `BUILD` file to include new files and dependencies
3. If using a new library, add its Bazel rule in `WORKSPACE`
4. Document usage in README

## References
- See `README.md` for feature roadmap, tech stack, and build instructions
- Key files: `engine/`, `render/`, `ui/`, `io/`, `WORKSPACE`, `BUILD`, `README.md`

---

For unclear conventions or missing instructions, check README or AGENT.md, or ask for clarification. Suggest improvements if you find outdated or missing guidance.
