# 🏗️ SegmeCam Modular Refactoring Plan

## Overview
The current `segmecam_gui_gpu.cpp` is a monolithic 1534-line file that handles all application responsibilities. This document outlines a step-by-step refactoring plan to create a modular, maintainable architecture following the project's design principles.

## Current Architecture Issues
- **Monolithic main()**: Single function handling GPU detection, MediaPipe setup, camera management, rendering, UI, and effects
- **Mixed responsibilities**: No clear separation between different concerns
- **Hard to test**: Monolithic structure prevents unit testing of individual components
- **Difficult to extend**: Adding new features requires modifying the massive main function
- **Poor maintainability**: Bug fixes and improvements are hard to isolate

## Reference code
Always use mediapipe/examples/desktop/segmecam/segmecam_gui_gpu.cpp.backup code as reference

## Target Modular Architecture

### Directory Structure (within MediaPipe examples)
```
mediapipe/examples/desktop/segmecam/
├── BUILD                           # Bazel build configuration
├── include/                        # Headers directory
│   ├── application/
│   │   └── application.h
│   ├── engine/
│   │   ├── mediapipe_manager.h
│   │   └── effects_processor.h
│   ├── io/
│   │   └── camera_manager.h
│   ├── render/
│   │   └── render_manager.h
│   └── ui/
│       ├── ui_manager.h
│       ├── camera_panel.h
│       ├── effects_panel.h
│       └── settings_panel.h
├── src/                           # Implementation files
│   ├── application/
│   │   └── application.cpp
│   ├── engine/                     # AI inference and MediaPipe management  
│   │   ├── mediapipe_manager.cpp
│   │   └── effects_processor.cpp
│   ├── io/                        # Camera input/output management
│   │   └── camera_manager.cpp
│   ├── render/                    # OpenGL/SDL2 rendering pipeline
│   │   └── render_manager.cpp
│   └── ui/                       # Dear ImGui interface components
│       ├── ui_manager.cpp
│       ├── camera_panel.cpp
│       ├── effects_panel.cpp
│       └── settings_panel.cpp
├── segmecam_gui_gpu.cpp          # Minimal main() that coordinates components
├── segmecam_composite.h          # Existing files
├── segmecam_composite.cc
├── segmecam_face_effects.h
├── segmecam_face_effects.cc
├── presets.h
├── presets.cc
├── vcam.h
├── vcam.cc
├── cam_enum.h
├── cam_enum.cc
└── gpu_detector.h
```

## Step-by-Step Refactoring Plan

### Phase 1: Application Foundation
**Goal**: Extract core application lifecycle management

**Files to Create**:
- `mediapipe/examples/desktop/segmecam/include/application/application.h`
- `mediapipe/examples/desktop/segmecam/src/application/application.cpp`

**Responsibilities**:
- Command line argument parsing
- Application initialization and cleanup
- Main event loop coordination
- Global state management

**Testing**: Verify app starts, runs, and exits cleanly

### Phase 2: MediaPipe Engine
**Goal**: Isolate AI inference and MediaPipe graph management

**Files to Create**:
- `mediapipe/examples/desktop/segmecam/include/engine/mediapipe_manager.h`
- `mediapipe/examples/desktop/segmecam/src/engine/mediapipe_manager.cpp`

**Current Implementation**: 
- Using `include/mediapipe_manager/mediapipe_manager.h` and `src/mediapipe_manager/mediapipe_manager.cpp` as interim structure
- Will be moved to `engine/` directory structure in future phases

**Responsibilities**:
- GPU detection and capability assessment
- MediaPipe graph selection (GPU vs CPU)
- Graph initialization and resource management
- Stream polling and packet processing
- Segmentation mask and face landmark extraction

**Testing**: Verify segmentation and face detection work identically

**Build Command**: 
```bash
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:mediapipe_manager_test
```

### Phase 3: Camera I/O
**Goal**: Centralize camera input and virtual camera output

**Files to Create**:
- `mediapipe/examples/desktop/segmecam/include/io/camera_manager.h`
- `mediapipe/examples/desktop/segmecam/src/io/camera_manager.cpp`

**Responsibilities**:
- Camera device enumeration and selection
- V4L2 configuration and controls (exposure, gain, etc.)
- Frame capture and format conversion
- Virtual camera (v4l2loopback) output
- Camera capability detection

**Testing**: Verify camera selection, controls, and vcam output work

### Phase 4: Render Pipeline
**Goal**: Isolate OpenGL/SDL2 rendering concerns

**Files to Create**:
- `mediapipe/examples/desktop/segmecam/include/render/render_manager.h`
- `mediapipe/examples/desktop/segmecam/src/render/render_manager.cpp`

**Responsibilities**:
- SDL2 window and OpenGL context setup
- Texture management and GPU memory handling
- Frame presentation and display
- OpenGL state management
- Performance optimization

**Testing**: Verify rendering performance and visual output

### Phase 5: Effects System ✅ COMPLETED
**Goal**: Extract beauty effects, background processing, and face effects

**Files Created**:
- `mediapipe/examples/desktop/segmecam/include/effects/effects_manager.h`
- `mediapipe/examples/desktop/segmecam/src/effects/effects_manager.cpp`
- `mediapipe/examples/desktop/segmecam/effects_manager_test.cpp`

**Responsibilities**:
- Beauty effects management (skin smoothing, wrinkle-aware processing)
- Background effects (blur, image replacement, solid color)
- Face effects (lipstick, teeth whitening)
- Beauty preset management and application
- OpenCL acceleration support
- Performance monitoring and optimization

**Key Features Implemented**:
- Comprehensive EffectsConfig and EffectsState structures
- 50+ beauty effect parameters with advanced controls
- Background blur with feathering and customizable strength
- Background image replacement with automatic scaling
- Solid color background compositing
- Advanced skin smoothing with wrinkle detection
- Lipstick application with color blending and feathering
- Teeth whitening with margin controls
- Beauty preset system (Natural, Light, Medium, Heavy)
- OpenCL acceleration toggle
- Performance tracking and logging

**Testing**: ✅ All tests pass - Effects Manager validates background effects, skin effects, lip/teeth effects, presets, performance monitoring, and OpenCL support

### Phase 6: UI Management ✅ COMPLETED
**Goal**: Modularize Dear ImGui interface components

**Files Created**:
- `mediapipe/examples/desktop/segmecam/include/ui/ui_panels.h` (117 lines)
- `mediapipe/examples/desktop/segmecam/src/ui/camera_panel.cpp` (290 lines)
- `mediapipe/examples/desktop/segmecam/src/ui/background_panel.cpp` (127 lines)
- `mediapipe/examples/desktop/segmecam/src/ui/beauty_panel.cpp` (261 lines)
- `mediapipe/examples/desktop/segmecam/src/ui/profile_debug_panels.cpp` (279 lines)
- `mediapipe/examples/desktop/segmecam/include/ui/ui_manager_enhanced.h` (84 lines)
- `mediapipe/examples/desktop/segmecam/src/ui/ui_manager_enhanced.cpp` (285 lines)
- `mediapipe/examples/desktop/segmecam/ui_manager_test.cpp` (test binary)
- `mediapipe/examples/desktop/segmecam/app_state.h` and `app_state.cpp` (shared state)

**Responsibilities Implemented**:
- Complete SDL2/OpenGL/ImGui setup and window management
- Modular panel system with base UIPanel class and specialized panels:
  - **CameraPanel**: Camera selection, resolution settings, V4L2 controls, virtual camera management
  - **BackgroundPanel**: Background mode selection, blur/image/color controls  
  - **BeautyPanel**: Comprehensive beauty effects with presets, skin smoothing, wrinkle controls, lip effects, teeth whitening
  - **ProfilePanel**: Profile management with save/load/export/import capabilities
  - **DebugPanel**: Debug visualization and performance monitoring
  - **StatusPanel**: Application status and performance information
- Enhanced UI Manager coordinating all panels with texture management
- Event processing and frame rendering coordination
- Panel visibility management and VSync control

**Testing**: ✅ All tests pass - UI Manager validates initialization, panel system, texture management, VSync control, window settings, and event handling

**Build Commands**:
```bash
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:ui_panels
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:ui_manager_enhanced
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:ui_manager_test
./bazel-bin/mediapipe/examples/desktop/segmecam/ui_manager_test
```

### Phase 7: Configuration System ✅ **COMPLETED**
**Goal**: Extract configuration management and settings persistence

**Files Created**:
- `mediapipe/examples/desktop/segmecam/src/config/config_manager.h` (168 lines)
- `mediapipe/examples/desktop/segmecam/src/config/config_manager.cpp` (582 lines)
- `mediapipe/examples/desktop/segmecam/config_manager_test.cpp` (test binary)
- `mediapipe/examples/desktop/segmecam/src/config/BUILD` (Bazel configuration)

**Responsibilities Implemented**:
- Complete configuration management with OpenCV FileStorage YAML persistence
- Profile directory management with automatic creation at `~/.config/segmecam/`
- Comprehensive profile save/load operations with error handling
- Default profile management with persistence and validation
- Configuration validation with type checking and range validation
- Beauty preset integration with existing presets.h
- Structured ConfigData with logical groupings (camera, display, background, landmarks, beauty, performance, debug)
- Profile enumeration and management operations
- Robust error handling for edge cases and invalid operations

**Testing**: ✅ All tests pass - Configuration Manager validates directory management, profile operations, default management, validation, preset integration, and error handling (6/6 test suites pass)

**Build Commands**:
```bash
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam/src/config:config_manager
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:config_manager_test
./bazel-bin/mediapipe/examples/desktop/segmecam/config_manager_test
```

### Phase 8: Integration & Final Testing
**Goal**: Comprehensive validation of refactored system

**Testing Areas**:
- All camera modes and resolutions
- GPU vs CPU fallback behavior
- Face effects and background replacement
- Profile save/load functionality
- Virtual camera output
- Performance characteristics
- Memory usage patterns

## Build Instructions

### Standard Build Commands
For building the modular architecture components during refactoring:

```bash
## Project Status

### Completed Phases ✅
- **Phase 1: Application Foundation** - Application lifecycle, GPU detection, configuration management
- **Phase 2: MediaPipe Manager** - Graph loading, GPU resources, processing pipeline  
- **Phase 3: Rendering System** - SDL2/OpenGL/ImGui separation with texture management
- **Phase 4: Camera System** - V4L2 controls, enumeration, OpenCV capture management
- **Phase 5: Effects System** - Beauty effects, background processing, face effects with comprehensive testing
- **Phase 6: UI Management** - Complete modular UI system with panels, enhanced manager, and full test validation

### Current Status
✅ **7/8 phases completed** with full testing and validation  
🚀 **Ready for Phase 8: Integration & Testing** - Final system integration and comprehensive validation

### Build Commands
```bash
# Build specific manager (example)
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:effects_manager

# Build and test manager (example)
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:effects_manager_test
./bazel-bin/mediapipe/examples/desktop/segmecam/effects_manager_test

# Build and test UI system
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:ui_manager_test
./bazel-bin/mediapipe/examples/desktop/segmecam/ui_manager_test

# Full Application Test (should work after each phase)
./scripts/run_segmecam_gui_gpu.sh --face
```

### Build Troubleshooting
```

### Build Flag Explanations
- `-c opt`: Optimized build mode for performance testing
- `--action_env=PKG_CONFIG_PATH`: Ensure pkg-config environment is available during build
- `--repo_env=PKG_CONFIG_PATH`: Pass pkg-config environment to repository rules  
- `--cxxopt=-I/usr/include/opencv4`: Include OpenCV4 headers from system installation

### Testing Commands
For each phase, use these commands to test the refactored components:

```bash
# Phase 1: Application Foundation Test
./bazel-bin/mediapipe/examples/desktop/segmecam/segmecam_gui_gpu_test

# Phase 2: MediaPipe Manager Test  
./bazel-bin/mediapipe/examples/desktop/segmecam/mediapipe_manager_test

# Phase 6: UI Management Test
./bazel-bin/mediapipe/examples/desktop/segmecam/ui_manager_test

# Full Application Test (should work after each phase)
./scripts/run_segmecam_gui_gpu.sh --face
```

### Build Troubleshooting
- If OpenCV headers are not found, verify installation: `pkg-config --cflags opencv4`
- For missing calculators, ensure all MediaPipe dependencies are included in BUILD files
- Clean build cache if needed: `bazel clean`

## Refactoring Guidelines

### Code Style
- Follow existing C++17 and clang-format (LLVM style)
- Use `.h` for headers (matching project convention), `.cpp` for implementation
- `snake_case` for functions/variables, `CamelCase` for classes
- Maintain existing error handling patterns

### Dependencies
- Keep existing MediaPipe, OpenCV, SDL2, ImGui dependencies
- No new external dependencies without approval
- Maintain Apache 2.0 license compatibility

### Testing Strategy
- Test after each phase to ensure no regression
- Compare performance metrics before/after each step
- Validate all user-facing functionality remains identical
- Test in both native and flatpak environments

### Rollback Plan
- Keep git commits small and focused per phase
- Each phase should be independently testable
- Maintain ability to revert any phase if issues arise

## Success Criteria
- ✅ Identical functionality to original monolithic version
- ✅ Same or better performance characteristics
- ✅ All camera, effects, and UI features working
- ✅ Clean modular architecture with clear responsibilities
- ✅ Easier to maintain, test, and extend
- ✅ No new dependencies or build complexity

## Next Steps
1. Start with Phase 1 (Application Foundation)
2. Create branch: `refactor/modular-architecture`
3. Implement one phase at a time with testing
4. Use this document to track progress and decisions