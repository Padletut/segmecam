# 🤖 AGENT.md – AI Contribution Guide

This file is intended for AI coding assistants (GitHub Copilot, ChatGPT, etc.)  
It describes **how the AI should contribute to this repository**.

---

## 🎯 Project Overview
SegmeCam is a **Linux-native AI webcam application** that combines:
- **Selfie Segmentation** (TensorFlow Lite / MediaPipe models)
- **Face Landmark Detection** (TensorFlow Lite / MediaPipe models)
- **OpenGL Shader Pipeline** (background blur, skin smoothing, teeth whitening)
- **SDL2 + Dear ImGui** UI
- **v4l2loopback** virtual webcam output

The goal: **real-time beauty and background effects** for Linux video calls & streaming.

---

## 📐 Code Style
- **Language:** C++17
- **Build System:** **Bazel**
- **Project Structure:**
  - `engine/` → AI inference (TFLite models, mask processing)
  - `render/` → OpenGL passes (blur, smoothing, composition)
  - `ui/` → Dear ImGui panels & controls
  - `io/` → camera input (OpenCV/V4L2) + v4l2loopback output
- **Formatting:** Follow default `clang-format` (LLVM style)
- **Headers:** `.hpp` for headers, `.cpp` for implementation
- **Naming:** `snake_case` for functions/vars, `CamelCase` for classes
- **Principles:** Code must be **clean, modular, and maintainable**.  
  - Each module should have a single responsibility.  
  - No “god classes” or huge functions.  
  - Separate logic, rendering, and UI clearly.  

---

## 🧠 AI Agent Rules
When suggesting code or docs, the AI should:
1. **Keep code clean and modular** – small, reusable functions and classes.
2. **Stay layered** – `engine`, `render`, `ui`, and `io` must not mix responsibilities.
3. **Prefer clarity** – readable, maintainable code over “clever hacks”.
4. **Use modern C++** – RAII, smart pointers where ownership matters.
5. **Be explicit** – avoid hidden magic; make OpenGL state changes clear.
6. **Target Linux first** – prioritize Linux compatibility over cross-platform.

---

## 🚧 Things to Avoid
- Do **not** switch to CMake or other build systems unless explicitly requested.
- Do **not** add Windows/Mac-specific APIs unless explicitly requested.
- Do **not** introduce GPL dependencies (keep licensing Apache-2.0 compatible).
- Do **not** assume CUDA/Vulkan unless discussed (stick to TFLite + OpenGL).

---

## ✅ Good Contribution Examples
- Adding a new shader pass (Gaussian blur, LUT filter)
- Improving TFLite inference wrapper (async, batching)
- Expanding ImGui panel with new sliders
- Writing unit tests for engine utilities
- Improving README/AGENT.md for clarity

---

## 🔮 Future Directions (AI can assist with)
- Background replacement with custom images
- Multi-face support
- Real-time profile system
- Flatpak release for easy distribution

---

By following these guidelines, AI coding assistants will help keep SegmeCam **clean, modular, and Linux-first** 🚀
