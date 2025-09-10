
# Copilot Instructions for SegmeCam

## Project Overview
SegmeCam is an AI-powered Linux desktop web camera app built in C++ with Bazel. It combines real-time selfie segmentation and face landmark detection using TensorFlow Lite (TFLite) models, and provides advanced effects via OpenCV and custom rendering pipelines. The GUI is planned to use ImGui for controls and rendering.

## Architecture & Key Components
- **src/**: Main C++ source files. Entry point is `src/main.cpp`.
- **include/**: Header files for modularization.
- **WORKSPACE & BUILD**: Bazel build configuration. All builds and dependencies are managed here.
- **AI Models**: TFLite models for segmentation and landmark detection (see README for details).
- **Rendering/UI**: ImGui (planned) for real-time controls and effects.

## Developer Workflows
- **Build the app:**
  ```bash
  bazel build //segmecam:app
  ```
- **Build TFLite and delegates:**
  ```bash
  bazel build -c opt //tensorflow/lite:libtensorflowlite.so //tensorflow/lite/delegates/xnnpack:xnnpack_delegate
  ```
- **Run the app:**
  ```bash
  ./bazel-bin/segmecam/app
  ```
- **Add dependencies:**
  Use Bazel's WORKSPACE and BUILD files. For external libraries (OpenCV, ImGui, etc.), prefer Bazel rules or repository rules.

## Patterns & Conventions
- All source code is in `src/`, headers in `include/`.
- Bazel targets are defined in the top-level `BUILD` file.
- External dependencies should be added via Bazel repository rules in `WORKSPACE`.
- AI models are referenced as external files or downloaded via scripts (see README for future instructions).
- Use C++17 or newer for all code.

## Integration Points
- **TensorFlow Lite**: Integrated via Bazel build; models loaded at runtime.
- **OpenCV**: Used for image processing and blending.
- **ImGui**: For GUI controls and rendering pipeline.
- **Virtual Webcam Output**: Planned feature for integration with Linux webcam stack.

## Example: Adding a New Effect
1. Add C++ source in `src/` and header in `include/`.
2. Update `BUILD` to include new files and dependencies.
3. If using a new library, add its Bazel rule in `WORKSPACE`.
4. Document usage in README.

## References
- See `README.md` for feature roadmap, tech stack, and build instructions.
- Key files: `src/main.cpp`, `WORKSPACE`, `BUILD`, `README.md`

---

For questions or unclear conventions, check README or ask for clarification. Please suggest improvements if you find missing or outdated instructions.
