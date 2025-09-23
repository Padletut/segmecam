# 🧑‍💻 SegmeCam AI Coding Instructions

## Core Architecture - Phase 1-8 Modular System ✅ 7/8 PHASES COMPLETE

SegmeCam uses a **modular manager architecture** at `mediapipe/examples/desktop/segmecam/` replacing the original monolithic design:

- **Application** (`src/application/`) - Main lifecycle, coordinates all managers via `application.cpp`
- **CameraManager** (`src/camera/camera_manager.cpp`) - V4L2 capture, device enumeration, vcam output
- **MediaPipeManager** (`src/mediapipe_manager/mediapipe_manager.cpp`) - AI inference graphs, GPU detection
- **RenderManager** (`src/render/render_manager.cpp`) - SDL2/OpenGL context, texture management
- **EffectsManager** (`src/effects/effects_manager.cpp`) - Beauty effects, background processing
- **UIManager** (`src/ui/ui_manager_enhanced.cpp`) - ImGui panels, event handling
- **ConfigManager** (`src/config/config_manager.cpp`) - Profile persistence, settings validation

## Critical Build Commands ⚠️ ALWAYS USE THE BUILD SCRIPT ⚠️

```bash
# 🚨 PRIMARY BUILD & RUN WORKFLOW - USE THIS 100% OF THE TIME! 🚨
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:segmecam
./bazel-bin/mediapipe/examples/desktop/segmecam/segmecam

# 🚨 NEVER use manual bazel commands for building the main app! 🚨
# 🚨 Build script handles all proper paths, arguments, and configurations automatically! 🚨

# Individual manager testing ONLY (Phase 1-7 validation)
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:effects_manager_test
./bazel-bin/mediapipe/examples/desktop/segmecam/effects_manager_test

# Essential flags for ALL builds
--action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4
```

## ⚠️ CRITICAL BUILD RULE ⚠️

**NEVER use manual `bazel build` or `bazel run` commands for the main application!**
**ALWAYS build the `:segmecam` target and run the binary directly**
**This ensures all proper paths, arguments, and configurations are set automatically!**

## MCP Tool Usage ⚠️ ALWAYS USE CORRECT PARAMETERS ⚠️

**When using the MCP tool `codacy_cli_analyze`, always include this input unless I specify another path:**

```json
{
  "rootPath": "/home/padletut/segmecam"
}
```

## Project-Specific Patterns

- **Header/Source Structure**: `.h` in `include/`, `.cpp` in `src/` with matching subdirectories
- **Manager Pattern**: Each manager is self-contained with `Initialize()`, `Update()`, `Render()`, `Cleanup()` lifecycle methods
- **Forward Declarations**: Use `std::unique_ptr<class ManagerName>` in headers to avoid circular includes
- **Bazel Targets**: Each manager has corresponding `cc_library` in BUILD with specific MediaPipe calculator deps
- **AppState Pattern**: `app_state.h` contains 50+ shared parameters synced across managers via `SyncSettingsToEffectsManager()`

## MediaPipe Integration Specifics

- **Graph Selection**: `segmecam_gui_gpu.cpp.backup` contains reference monolithic implementation
- **GPU Detection**: `gpu_detector.h` handles NVIDIA/Mesa capability assessment with EGL/GL probing
- **Graph Paths**: Use `mediapipe_graphs/` with fallback from GPU→CPU graphs based on `gpu_detector` results
- **Calculator Dependencies**: Include specific MediaPipe calculators in BUILD files, not generic deps
- **Stream Processing**: Poll `CalculatorGraph` outputs in main loop, process segmentation masks and face landmarks

## Manager Dependencies & Known Issues

- **OpenCV Conflicts**: MediaPipe's `opencv_core_inc.h` conflicts with system OpenCV headers
  - **Workaround**: Use MediaPipe's OpenCV port (`//mediapipe/framework/port:opencv_core`) not system OpenCV
- **Missing Protobuf Types**: Some MediaPipe calculators require additional deps in BUILD files
  - **Example**: `ConstantSidePacketCalculatorOptions` needs `//mediapipe/calculators/core:constant_side_packet_calculator_cc_proto`
  - **Solution**: Add missing calculator targets to binary deps in BUILD file
- **Manager Testing**: Each manager has dedicated test binary (e.g., `effects_manager_test`)
- **Partial Integration**: Some managers temporarily disabled in `application.cpp` - check comments

## AppState & Data Flow

- **Central State**: `app_state.h` contains 50+ beauty/background parameters shared across managers
- **Profile System**: `ConfigManager` handles YAML persistence to `~/.config/segmecam/` with OpenCV FileStorage
- **Beauty Presets**: `presets.h` defines Natural/Light/Medium/Heavy effect combinations with 20+ parameters each
- **Virtual Camera**: `vcam.h` manages v4l2loopback output for video calls via OpenCV VideoWriter
- **Settings Sync**: `ApplicationRun::SyncSettingsToEffectsManager()` propagates app_state changes to EffectsManager

## Development Anti-Patterns

- ❌ Don't add system OpenCV deps - use MediaPipe ports (`//mediapipe/framework/port:opencv_core`)
- ❌ Don't modify monolithic `segmecam_gui_gpu.cpp.backup` - it's reference only, not active code
- ❌ Don't create "god classes" - follow manager single-responsibility pattern
- ❌ Don't skip `--action_env=PKG_CONFIG_PATH` flags in builds
- ❌ Don't mix SDL2/OpenGL/ImGui initialization - keep in separate managers
- ❌ Don't hardcode paths - use configurable options via ConfigManager

## Project-Specific Conventions

- **Language:** C++17, default `clang-format` (LLVM style)
- **Headers:** `.h` for headers, `.cpp` for implementation
- **Naming:** `snake_case` for functions/vars, `CamelCase` for classes/structs
- **Modularity:** Each module has a single responsibility; avoid "god classes"
- **Layering:** Keep `application`, `mediapipe_manager`, `render`, `ui`, and `camera` responsibilities separate
- **Linux-first:** Prioritize Linux compatibility; avoid Windows/Mac APIs unless requested
- **Licensing:** Only Apache-2.0 compatible dependencies; avoid GPL
- **Error Handling:** Use absl::Status for MediaPipe operations, exceptions for application logic

## Integration Points

- **TensorFlow Lite:** Integrated via Bazel; models loaded at runtime from `//mediapipe/modules/`
- **OpenCV:** For image processing and blending (use MediaPipe ports, not system headers)
- **Dear ImGui:** For GUI controls and rendering pipeline (SDL2 + OpenGL3 backend)
- **v4l2loopback:** For virtual webcam output (managed via `vcam.h`)
- **SDL2:** Window management and input handling
- **EGL/GL:** GPU detection and OpenGL context management

## Example: Adding a New Effect

1. Add C++ source in `src/effects/` and header in `include/effects/`
2. Update EffectsManager class with new methods and state tracking
3. Add parameters to `app_state.h` and EffectsConfig/EffectsState structs
4. Update `SyncSettingsToEffectsManager()` to propagate new settings
5. Add UI controls in `UIManager` panels (CameraPanel, BeautyPanel, etc.)
6. Update Bazel BUILD file with any new dependencies
7. Add to ConfigManager YAML persistence if needed
8. Test with `effects_manager_test`

## Refactoring Guidelines

- **Modular Architecture:** Follow the step-by-step plan in `AGENTS.md` for breaking down monolithic code
- **Phase-by-Phase:** Complete one refactoring phase at a time with thorough testing before proceeding
- **Testing Strategy:** After each phase, verify identical functionality, performance, and user experience
- **Rollback Safety:** Keep git commits small and focused per phase to enable easy rollback if needed
- **Component Isolation:** Each module should have clear responsibilities and minimal dependencies
- **Performance Preservation:** Maintain or improve performance characteristics during refactoring

## Key Files & Directories

- `src/application/application.cpp` - Main coordinator, manager lifecycle
- `src/mediapipe_manager/` - AI inference, GPU graphs, stream processing
- `src/effects/` - Beauty effects, background processing, face effects
- `src/ui/` - ImGui panels, event handling, texture management
- `src/camera/` - V4L2 capture, device enumeration, virtual camera
- `src/render/` - SDL2/OpenGL setup, texture management
- `src/config/` - Profile persistence, YAML configuration
- `app_state.h` - Shared state across all managers (50+ parameters)
- `BUILD` - Bazel targets, dependencies, calculator includes
- `WORKSPACE` - External dependencies, MediaPipe integration

---

For unclear conventions or missing instructions, check README.md, AGENTS.md, or AGENT.md, or ask for clarification. Suggest improvements if you find outdated or missing guidance.
