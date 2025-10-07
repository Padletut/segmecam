#!/bin/bash

# SegmeCam Flatpak Wrapper Script
# This script helps with EGL/GPU initialization in the Flatpak sandbox

echo "🎬 Starting SegmeCam..."

# Set up environment for better GPU access
export LIBGL_ALWAYS_INDIRECT=0
export LIBGL_ALWAYS_SOFTWARE=0
export MESA_GL_VERSION_OVERRIDE=3.3
export MESA_GLSL_VERSION_OVERRIDE=330

# Docker-proven EGL/GLX fixes
export QT_X11_NO_MITSHM=1
export XDG_RUNTIME_DIR=/tmp/xdg
export __GLX_VENDOR_LIBRARY_NAME=nvidia

# Try different display approaches
if [ -z "$DISPLAY" ]; then
    export DISPLAY=:0
fi

# EGL specific environment variables
export EGL_PLATFORM=x11
export __EGL_VENDOR_LIBRARY_DIRS=/app/lib:/usr/share/glvnd/egl_vendor.d
export __EGL_VENDOR_LIBRARY_FILENAMES=""

# Create XDG runtime directory
mkdir -p /tmp/xdg

echo "🖥️  Using display: $DISPLAY"
echo "📁 Library path: $LD_LIBRARY_PATH"
echo "🎮 EGL platform: $EGL_PLATFORM"
echo "🔧 Qt X11 no MITSHM: $QT_X11_NO_MITSHM"
echo "📦 XDG runtime dir: $XDG_RUNTIME_DIR"
echo "🎯 GLX vendor: $__GLX_VENDOR_LIBRARY_NAME"

# Try to run with explicit graph first
if [ ! -f "mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt" ]; then
    echo "� Graph file not found, running without arguments..."
    exec /app/bin/segmecam "$@"
else
    echo "📊 Using selfie_seg_gpu_mask_cpu.pbtxt graph..."
    exec /app/bin/segmecam mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt "$@"
fi