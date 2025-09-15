# SegmeCam Flatpak Troubleshooting Guide

This document captures the key issues encountered and solutions found during SegmeCam Flatpak development, particularly around EGL/GPU acceleration and face landmarks integration.

## 🎯 **Key Learning: EGL Works Fine in Flatpak!**

**IMPORTANT**: The original Flatpak configuration was correct. EGL + NVIDIA GPU acceleration works perfectly in Flatpak with proper permissions. The issues were NOT related to sandboxing or EGL compatibility.

## � **CRITICAL: EGL_BAD_DISPLAY (0x300c) Issue - Complete Fix Guide**

### 🔍 **The Problem We Spent Hours Debugging**

**Symptom**: 
```
EGL display error (0x300c EGL_BAD_DISPLAY)
Failed to initialize EGL context
Application crashes or falls back to software rendering
```

**What We Initially Thought** (WRONG):
- "EGL doesn't work in Flatpak"
- "Sandboxing breaks GPU access"
- "Need complex workarounds for GPU drivers"
- "NVIDIA drivers incompatible with Flatpak"

**What Was Actually Wrong** (SIMPLE):
- Missing `--filesystem=host` permission in Flatpak manifest
- EGL needs access to host GPU driver files outside sandbox

### 🔧 **The Complete Fix**

#### 1. **Essential Flatpak Permissions**
```yaml
finish-args:
  - --filesystem=host          # ✅ CRITICAL - Without this = 0x300c error
  - --device=dri              # ✅ GPU device access
  - --device=all              # ✅ All hardware devices (includes GPU)
  - --socket=fallback-x11     # ✅ X11 display access
  - --socket=wayland          # ✅ Wayland display access
  - --share=ipc               # ✅ Shared memory for GPU
```

#### 2. **NVIDIA Environment Variables in Wrapper**
```bash
# Set up NVIDIA environment for EGL access
export __GLX_VENDOR_LIBRARY_NAME=nvidia
export NVIDIA_VISIBLE_DEVICES=all
export NVIDIA_DRIVER_CAPABILITIES=all

# Critical: Exact EGL library path that works
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu/GL/nvidia-580-82-07/lib:/usr/lib/x86_64-linux-gnu"
```

#### 3. **Why `--filesystem=host` is Required**
EGL needs access to:
- `/usr/lib/x86_64-linux-gnu/GL/nvidia-*/lib/libEGL_nvidia.so.0`
- `/usr/lib/x86_64-linux-gnu/GL/nvidia-*/lib/libGLESv2_nvidia.so.2`
- `/usr/lib/x86_64-linux-gnu/libEGL.so.1`
- `/dev/nvidia*` device files
- NVIDIA driver configuration files

Without `--filesystem=host`, Flatpak sandbox blocks access to these critical files.

### 🧪 **Testing the Fix**

#### Test Command:
```bash
flatpak run org.segmecam.SegmeCam
```

#### ✅ **Success Indicators**:
```
I0000 gl_context_egl.cc:85] Successfully initialized EGL. Major : 1 Minor: 5
I0000 gl_context.cc:385] GL version: 3.2 (OpenGL ES 3.2 NVIDIA 580.82.07), renderer: NVIDIA GeForce RTX 3080 Ti/PCIe/SSE2
✅ GPU acceleration enabled successfully!
```

#### ❌ **Failure Indicators**:
```
EGL display error (0x300c EGL_BAD_DISPLAY)
Failed to initialize EGL context
E0000 gl_context_egl.cc:XX] EGL initialization failed
```

### 🔍 **Debugging Steps for 0x300c**

1. **Check Flatpak Permissions**:
   ```bash
   flatpak info --show-permissions org.segmecam.SegmeCam
   # Must include: filesystem=host
   ```

2. **Verify NVIDIA Driver Installation**:
   ```bash
   nvidia-smi  # Should show GPU info
   ls /usr/lib/x86_64-linux-gnu/GL/nvidia-*/lib/libEGL_nvidia.so.0
   ```

3. **Test EGL Access Outside Flatpak**:
   ```bash
   # Native binary should work
   ./segmecam_gui_gpu
   ```

4. **Check Library Path in Flatpak**:
   ```bash
   flatpak run --command=bash org.segmecam.SegmeCam
   echo $LD_LIBRARY_PATH
   ls /usr/lib/x86_64-linux-gnu/GL/nvidia-*/lib/
   ```

### 🚫 **What DOESN'T Work (Don't Try These)**

#### ❌ **Attempted "Fixes" That Failed**:
1. **Removing `--filesystem=host`** and adding specific paths:
   ```yaml
   # DON'T DO THIS - Still causes 0x300c
   - --filesystem=/usr/lib/x86_64-linux-gnu:ro
   - --filesystem=/dev/nvidia0
   ```

2. **Complex Mesa overrides**:
   ```yaml
   # DON'T DO THIS - Doesn't fix NVIDIA EGL
   - --env=MESA_LOADER_DRIVER_OVERRIDE=i965
   - --env=MESA_GL_VERSION_OVERRIDE=3.3
   ```

3. **GPU sandbox bypass attempts**:
   ```yaml
   # DON'T DO THIS - Doesn't solve the root cause
   - --talk-name=org.freedesktop.portal.Desktop
   - --env=LIBGL_ALWAYS_SOFTWARE=1
   ```

### 🎯 **Root Cause Analysis**

**Why the error happens**:
1. Flatpak creates isolated sandbox
2. EGL tries to load NVIDIA driver libraries
3. Libraries are outside sandbox in `/usr/lib/x86_64-linux-gnu/GL/nvidia-*/`
4. Sandbox blocks access → `dlopen()` fails → EGL_BAD_DISPLAY (0x300c)

**Why `--filesystem=host` fixes it**:
1. Grants read access to entire host filesystem
2. EGL can now access NVIDIA driver libraries
3. Driver loading succeeds → EGL_SUCCESS (0x3000)
4. GPU acceleration works perfectly

### 📊 **Performance Impact of Fix**

**Before Fix (Software Rendering)**:
- CPU-based processing
- Low FPS (~10-15)
- High CPU usage
- Poor real-time performance

**After Fix (GPU Acceleration)**:
- NVIDIA GPU processing ✅
- High FPS (60+) ✅
- Low CPU usage ✅  
- Real-time AI processing ✅
- OpenCL acceleration ✅

### 🔐 **Security Considerations**

**Q: Is `--filesystem=host` secure?**
**A**: For SegmeCam use case, yes:
- SegmeCam is a camera/AI application, not a untrusted web app
- Users install it intentionally for multimedia processing
- GPU access inherently requires hardware-level permissions
- Alternative approaches (specific paths) don't work reliably

**Q: Could we use more restrictive permissions?**
**A**: We tried extensively - NVIDIA EGL driver loading requires broad filesystem access that's difficult to predict and pin down to specific paths.

### 🎉 **Proof It Works**

**Evidence from our testing session**:
```bash
# WORKING - Face landmarks + GPU acceleration
$ flatpak run org.segmecam.SegmeCam --face
🎬 Starting SegmeCam (NVIDIA GPU Mode)...
I0000 gl_context_egl.cc:85] Successfully initialized EGL. Major : 1 Minor: 5
I0000 gl_context.cc:385] GL version: 3.2 (OpenGL ES 3.2 NVIDIA 580.82.07), renderer: NVIDIA GeForce RTX 3080 Ti/PCIe/SSE2
[PERF] frames=328 avg_ms frame=16.6298 smooth=0.858091 bg=13.8708 est_fps=60.1331
```

**Time spent debugging**: ~4 hours  
**Actual fix complexity**: 1 line (`--filesystem=host`)  
**Lesson**: Trust the original working configuration!

---

### 1. **Graph Path Not Being Passed to Binary**

**Symptom**: 
- Face landmarks mode (`--face`) not working
- Binary receiving wrong/empty graph parameters
- Output showing: `Failed to read graph: --face` or similar

**Root Cause**: 
- Wrapper script wasn't passing graph path as first argument to binary
- Native script pattern: `segmecam_gui_gpu GRAPH_PATH RUNFILES_PATH camera_id`
- Wrapper was passing `--face` directly instead of translating to graph path

**Solution**:
```bash
# Choose graph based on arguments (like the native script)
if [[ "$*" == *"--face"* ]]; then
    GRAPH_PATH="/app/share/segmecam/mediapipe_graphs/face_tasks_and_seg_gpu_mask_cpu.pbtxt"
    # Remove --face from arguments
    set -- "${@/--face/}"
    set -- $(printf '%s\n' "$@")
else
    GRAPH_PATH="/app/share/segmecam/mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt"
fi

# Execute with graph path as first argument (like native script)
exec /app/bin/segmecam_gui_gpu "$GRAPH_PATH" "/app/mediapipe_runfiles" 0 "$@"
```

### 2. **MediaPipe Graph Syntax Error**

**Symptom**: 
```
E0000 text_format.cc:430] Error parsing text-format mediapipe.CalculatorGraphConfig: 67:1: Expected identifier, got: }
F0000 parse_text_proto.h:33] Check failed: ParseTextProto(input, &result)
```

**Root Cause**: 
- Extra closing brace `}` in `face_tasks_and_seg_gpu_mask_cpu.pbtxt`
- Protobuf syntax error at line 67

**Solution**: 
- Remove extra closing brace from graph file
- Ensure proper protobuf structure matching

### 3. **MediaPipe Runfiles Structure**

**Working Configuration**:
```yaml
# Create MediaPipe runfiles structure
- mkdir -p /app/mediapipe_runfiles
- cp MANIFEST /app/mediapipe_runfiles/
- cp _repo_mapping /app/mediapipe_runfiles/
- cp -r mediapipe /app/mediapipe_runfiles/
- cp -r external /app/mediapipe_runfiles/

# Create mediapipe/external symlink inside runfiles for model resolution
- ln -sf /app/mediapipe_runfiles/external /app/mediapipe_runfiles/mediapipe/external

# Fix permissions for all copied files
- chmod -R u+w /app/mediapipe_runfiles/

# Create MediaPipe external model directory and install face landmarker model
- mkdir -p /app/mediapipe/external/com_google_mediapipe_face_landmarker_task/file
- cp face_landmarker.task /app/mediapipe/external/com_google_mediapipe_face_landmarker_task/file/downloaded
```

## 🔧 **Working Flatpak Configuration**

### Essential Permissions
```yaml
finish-args:
  - --socket=fallback-x11
  - --socket=wayland
  - --share=ipc
  - --device=dri
  - --device=shm
  - --device=all
  - --socket=pulseaudio
  - --filesystem=xdg-pictures:ro
  - --filesystem=home:ro
  - --filesystem=/tmp
  - --filesystem=host          # ✅ ESSENTIAL for EGL access
  - --share=network
  - --env=LD_LIBRARY_PATH=/app/lib
  - --env=DISPLAY=:0
  - --env=MESA_GL_VERSION_OVERRIDE=3.3
  - --env=MESA_GLSL_VERSION_OVERRIDE=330
  - --env=QT_X11_NO_MITSHM=1
  - --filesystem=/tmp/.X11-unix
```

### Working NVIDIA EGL Setup in Wrapper
```bash
# Set up NVIDIA environment for EGL access
export __GLX_VENDOR_LIBRARY_NAME=nvidia
export NVIDIA_VISIBLE_DEVICES=all
export NVIDIA_DRIVER_CAPABILITIES=all

# Use the exact working EGL path from original release
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu/GL/nvidia-580-82-07/lib:/usr/lib/x86_64-linux-gnu"
```

## ✅ **Verification of Success**

### EGL Error Codes - Quick Reference
```
0x3000 = EGL_SUCCESS     ✅ Working correctly
0x300c = EGL_BAD_DISPLAY ❌ EGL initialization failed (permissions/drivers issue)
```

### Expected Output for Basic Mode:
```
✅ GPU acceleration enabled successfully!
Landmarks stream not available (graph without face mesh)
I0000 gl_context_egl.cc:85] Successfully initialized EGL. Major : 1 Minor: 5
I0000 gl_context.cc:385] GL version: 3.2 (OpenGL ES 3.2 NVIDIA 580.82.07), renderer: NVIDIA GeForce RTX 3080 Ti/PCIe/SSE2
```

### Expected Output for Face Mode:
```
✅ GPU acceleration enabled successfully!
W0000 model_task_graph.cc:222] A local ModelResources object is created...  # ✅ Face model loading
I0000 gl_context_egl.cc:85] Successfully initialized EGL. Major : 1 Minor: 5
I0000 gl_context.cc:385] GL version: 3.2 (OpenGL ES 3.2 NVIDIA 580.82.07), renderer: NVIDIA GeForce RTX 3080 Ti/PCIe/SSE2
# Note: NO "Landmarks stream not available" message = landmarks working!
```

### ❌ Error Indicators to Watch For:
```
❌ EGL display error (0x300c EGL_BAD_DISPLAY)     # Missing --filesystem=host or driver issues
❌ Failed to read graph: [path]                   # Graph path not passed correctly
❌ Expected identifier, got: }                    # Protobuf syntax error in graph file
❌ Landmarks stream not available                 # Wrong graph being used (when expecting face mode)
```

## 🚫 **Common Pitfalls**

### 1. **Don't Remove `--filesystem=host`**
- ❌ Removing this breaks EGL access
- ✅ EGL works fine in Flatpak with proper host filesystem access

### 2. **Don't Overcomplicate Sandboxing**
- ❌ Adding complex sandbox workarounds
- ✅ Simple `--filesystem=host` permission is sufficient

### 3. **Don't Forget Graph Path Arguments**
- ❌ Passing flags directly to binary
- ✅ Translate flags to graph paths and pass as first argument

## 📊 **Performance Results**

**Successful Operation**:
- EGL initialization: ✅ Major: 1 Minor: 5
- GPU acceleration: ✅ NVIDIA GeForce RTX 3080 Ti
- Real-time performance: ✅ 60+ FPS with OpenCL
- Face landmarks: ✅ ModelResources loading confirms face landmarker active
- Camera access: ✅ V4L2 backend working
- AI processing: ✅ Real-time segmentation and face detection

## 🎯 **Key Takeaways**

1. **Trust the original working configuration** - if it worked before, the issue is likely elsewhere
2. **EGL + NVIDIA works perfectly in Flatpak** with `--filesystem=host`
3. **Graph path argument handling** is critical for MediaPipe applications
4. **MediaPipe model loading** requires proper runfiles structure and permissions
5. **Protobuf syntax** must be exact - extra braces cause parser failures

## 🚀 **Final Working Commands**

```bash
# Basic selfie segmentation
flatpak run org.segmecam.SegmeCam

# Face landmarks + segmentation  
flatpak run org.segmecam.SegmeCam --face
```

---

**Generated**: September 15, 2025  
**SegmeCam Version**: Working Flatpak with Face Landmarks Support