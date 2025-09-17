#!/bin/bash

echo "🎬 Starting SegmeCam (CPU-Only Mode)..."
echo "🖥️  Using display: ${DISPLAY}"
echo "📁 Library path: /app/lib"
echo "🧠 Processing: CPU-only (no GPU required)"
echo "🔧 Qt X11 no MITSHM: 1"
echo "📦 XDG runtime dir: ${XDG_RUNTIME_DIR}"

# CPU-only environment - disable GPU acceleration
export MESA_GL_VERSION_OVERRIDE=3.3
export MESA_GLSL_VERSION_OVERRIDE=330
export QT_X11_NO_MITSHM=1

# Force software rendering
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe

# Use CPU-only MediaPipe graph
echo "📊 Using selfie_seg_cpu_min.pbtxt graph..."

# Launch with CPU graph (updated for modular architecture)
exec /app/bin/segmecam --graph=/app/share/mediapipe_graphs/selfie_seg_cpu_min.pbtxt "$@"