#!/usr/bin/env bash
set -euo pipefail
GRAPH="${1:-/opt/segmecam/graphs/face_and_seg_gpu_mask_cpu.pbtxt}"
RUNFILES="/opt/segmecam/runfiles"
CAM="${2:-0}"
exec /opt/segmecam/bin/SegmeCam "$GRAPH" "$RUNFILES" "$CAM"