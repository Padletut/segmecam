#!/bin/bash

# SegmeCam Flatpak Wrapper Script
# Force software rendering to avoid EGL issues in Flatpak sandbox

echo "🎬 Starting SegmeCam (Software Rendering Mode)..."

# Force software rendering - disable all GPU acceleration
export LIBGL_ALWAYS_SOFTWARE=1
export LIBGL_ALWAYS_INDIRECT=1
export MESA_GL_VERSION_OVERRIDE=3.3
export MESA_GLSL_VERSION_OVERRIDE=330

# Disable GPU-specific features
export MESA_LOADER_DRIVER_OVERRIDE=swrast
export GALLIUM_DRIVER=llvmpipe

# Docker-proven EGL/GLX fixes (but forcing software)
export QT_X11_NO_MITSHM=1
export XDG_RUNTIME_DIR=/tmp/xdg

# Try different display approaches
if [ -z "$DISPLAY" ]; then
    export DISPLAY=:0
fi

# Force software EGL
export EGL_PLATFORM=surfaceless
export __EGL_VENDOR_LIBRARY_DIRS=""
export LIBGL_DRI3_DISABLE=1

# Create XDG runtime directory
mkdir -p /tmp/xdg

echo "🖥️  Using display: $DISPLAY"
echo "📁 Library path: $LD_LIBRARY_PATH"
echo "💻 Software rendering: ENABLED"
echo "🔧 Qt X11 no MITSHM: $QT_X11_NO_MITSHM"
echo "📦 XDG runtime dir: $XDG_RUNTIME_DIR"
echo "🎮 Mesa driver: $MESA_LOADER_DRIVER_OVERRIDE"

# Try to run with explicit graph first
if [ ! -f "mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt" ]; then
    echo "📁 Graph file not found, running without arguments..."
    exec /app/bin/segmecam_gui_gpu "$@"
else
    echo "📊 Using selfie_seg_gpu_mask_cpu.pbtxt graph..."
    exec /app/bin/segmecam_gui_gpu mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt "$@"
fi