#!/bin/bash

echo "🎬 Starting SegmeCam (NVIDIA GPU Mode)..."
echo "🖥️  Using display: ${DISPLAY:-:1}"
echo "🎮 GPU: NVIDIA RTX 3080 Ti"
echo "📊 Using selfie_seg_gpu_mask_cpu.pbtxt graph..."

# Use the exact working approach
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu/GL/nvidia-580-82-07/lib:/usr/lib/x86_64-linux-gnu"
exec /app/bin/segmecam_gui_gpu "$@"