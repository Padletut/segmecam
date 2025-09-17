---
mode: agent
---
Define the task to achieve, including specific requirements, constraints, and success criteria.

# Comparison of the monolithic code with the modular architecture
The current `segmecam_gui_gpu.cpp` is a monolithic file that combines all functionalities including application setup, camera management, MediaPipe processing, effects application, rendering, and UI handling. This results in a large, complex file that is difficult to maintain and extend.

# Refactoring Task

The current `segmecam_gui_gpu.cpp` is a monolithic 1534-line file that handles all application responsibilities. This document outlines a step-by-step refactoring plan to create a modular, maintainable architecture following the project's design principles.

# Target Modular Architecture

### Directory Structure (within MediaPipe examples)
```
mediapipe/examples/desktop/segmecam/
├── BUILD                           # Bazel build configuration
├── include/                        # Headers directory
│   ├── application/
│   │   └── application.h        # Main application class
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

### Build Commands
```bash
# Build specific manager (example)
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4

./scripts/run_segmecam_gui_gpu.sh --face

## Success Criteria

- ✅ Identical functionality to original monolithic version
- ✅ Same or better performance characteristics
- ✅ All camera, effects, and UI features working
- ✅ Clean modular architecture with clear responsibilities
- ✅ Easier to maintain, test, and extend
- ✅ No new dependencies or build complexity