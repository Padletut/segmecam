

# 🧑‍💻 SegmeCam AI Coding Instructions

## Core Architecture - Phase 1-8 Modular System
SegmeCam uses a **modular manager architecture** at `mediapipe/examples/desktop/segmecam/` replacing the original monolithic design:

- **Main Application** (`src/application/application.cpp`) - Main lifecycle, coordinates all managers
- **CameraManager** (`src/camera/camera_manager.cpp`) - V4L2 capture, device enumeration, vcam output
- **MediaPipeManager** (`src/mediapipe_manager/mediapipe_manager.cpp`) - AI inference graphs, GPU detection
- **RenderManager** (`src/render/render_manager.cpp`) - SDL2/OpenGL context, texture management
- **EffectsManager** (`src/effects/effects_manager.cpp`) - Beauty effects, background processing
- **UIManager** (`src/ui/ui_manager_enhanced.cpp`) - ImGui panels, event handling
- **ConfigManager** (`src/config/config_manager.cpp`) - Profile persistence, settings validation

## Critical Build Commands ⚠️ ALWAYS USE THE BUILD SCRIPT ⚠️
```bash
# 🚨 PRIMARY BUILD & RUN WORKFLOW - USE THIS 100% OF THE TIME! 🚨
./scripts/run_segmecam_gui_gpu.sh --face

# 🚨 NEVER use manual bazel commands for building the main app! 🚨
# 🚨 ALWAYS use the build script above! 🚨

# Individual manager testing ONLY (Phase 1-7 validation)
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:effects_manager_test

# Essential flags for ALL builds
--action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4
```

## ⚠️ CRITICAL BUILD RULE ⚠️
**NEVER use manual `bazel build` or `bazel run` commands for the main application!**
**ALWAYS use `./scripts/run_segmecam_gui_gpu.sh --face`**
**This script handles all proper paths, arguments, and configurations automatically!**

## Project-Specific Patterns
- **Header/Source**: `.h` in `include/`, `.cpp` in `src/` with matching subdirectories
- **Manager Pattern**: Each manager is self-contained with `Initialize()`, lifecycle methods, and clear responsibilities
- **Forward Declarations**: Use `std::unique_ptr<class ManagerName>` in headers to avoid circular includes
- **Bazel Targets**: Each manager has corresponding `cc_library` in BUILD with specific deps


## MediaPipe Integration Specifics
- **Graph Selection**: `segmecam_gui_gpu.cpp.backup` contains reference monolithic implementation
- **GPU Detection**: `gpu_detector.h` handles NVIDIA/Mesa capability assessment
- **Graph Paths**: Use `mediapipe_graphs/` with fallback from GPU→CPU graphs
- **Calculator Dependencies**: Include specific MediaPipe calculators in BUILD files, not generic deps

## Manager Dependencies & Known Issues
- **OpenCV Conflicts**: MediaPipe's `opencv_core_inc.h` conflicts with system OpenCV headers
  - **Workaround**: Use MediaPipe's OpenCV port (`//mediapipe/framework/port:opencv_core`) not system OpenCV
- **Missing Protobuf Types**: Some MediaPipe calculators require additional deps in BUILD files
  - **Example**: `ConstantSidePacketCalculatorOptions` needs proper calculator dependencies
  - **Solution**: Add missing calculator targets to binary deps in BUILD file
- **Manager Testing**: Each manager has dedicated test binary (e.g., `effects_manager_test`)
- **Partial Integration**: Some managers temporarily disabled in `application.cpp` - see comments

## AppState & Data Flow
- **Central State**: `app_state.h` contains 50+ beauty/background parameters shared across managers
- **Profile System**: `ConfigManager` handles YAML persistence to `~/.config/segmecam/`
- **Beauty Presets**: `presets.h` defines Natural/Light/Medium/Heavy effect combinations
- **Virtual Camera**: `vcam.h` manages v4l2loopback output for video calls

## Development Anti-Patterns
- ❌ Don't add system OpenCV deps - use MediaPipe ports
- ❌ Don't modify monolithic `segmecam_gui_gpu.cpp` - it's reference only
- ❌ Don't create "god classes" - follow manager single-responsibility pattern
- ❌ Don't skip `--action_env=PKG_CONFIG_PATH` flags in builds

## Project-Specific Conventions
- **Language:** C++17, default `clang-format` (LLVM style)
- **Headers:** `.h` for headers, `.cpp` for implementation
- **Naming:** `snake_case` for functions/vars, `CamelCase` for classes
- **Modularity:** Each module has a single responsibility; avoid "god classes"
- **Layering:** Keep `application`, `engine`, `render`, `ui`, and `io` responsibilities separate
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

## Refactoring Guidelines
- **Modular Architecture:** Follow the step-by-step plan in `AGENTS.md` for breaking down monolithic code
- **Phase-by-Phase:** Complete one refactoring phase at a time with thorough testing before proceeding
- **Testing Strategy:** After each phase, verify identical functionality, performance, and user experience
- **Rollback Safety:** Keep git commits small and focused per phase to enable easy rollback if needed
- **Component Isolation:** Each module should have clear responsibilities and minimal dependencies
- **Performance Preservation:** Maintain or improve performance characteristics during refactoring

## References
- See `README.md` for feature roadmap, tech stack, and build instructions
- See `AGENTS.md` for detailed modular architecture refactoring plan
- Key files: `application/`, `camera/`, `render/`, `ui/`, `effects/`, `WORKSPACE`, `BUILD`, `README.md`

---

For unclear conventions or missing instructions, check README, AGENTS.md, or AGENT.md, or ask for clarification. Suggest improvements if you find outdated or missing guidance.
