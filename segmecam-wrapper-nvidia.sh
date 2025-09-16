#!/bin/bash

echo "🎬 Starting SegmeCam (NVIDIA GPU Mode)..."
echo "🖥️  Using display: ${DISPLAY:-:1}"
echo "🎮 GPU: NVIDIA RTX 3080 Ti"

# Choose graph based on arguments (like the native script)
if [[ "$*" == *"--face"* ]]; then
    echo "🎭 Face landmarks mode enabled"
    echo "📊 Using face_and_seg_gpu_mask_cpu.pbtxt graph (classic FaceMesh)..."
    GRAPH_PATH="/app/share/segmecam/mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt"
    # Remove --face from arguments
    set -- "${@/--face/}"
    # Filter out any empty arguments that might remain
    set -- $(printf '%s\n' "$@")
else
    echo "📊 Using selfie_seg_gpu_mask_cpu.pbtxt graph..."
    GRAPH_PATH="/app/share/segmecam/mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt"
fi

# Set up NVIDIA environment for EGL access
export __GLX_VENDOR_LIBRARY_NAME=nvidia
export NVIDIA_VISIBLE_DEVICES=all
export NVIDIA_DRIVER_CAPABILITIES=all

# Use the exact working EGL path from original release
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu/GL/nvidia-580-82-07/lib:/usr/lib/x86_64-linux-gnu"

# Execute the binary with graph path as first argument (like native script)
# Pattern: segmecam_gui_gpu GRAPH_PATH BIN_RUNFILES camera_id
exec /app/bin/segmecam_gui_gpu "$GRAPH_PATH" "/app/mediapipe_runfiles" 0 "$@"