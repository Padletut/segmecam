#!/usr/bin/env bash
set -euo pipefail

# Runs the SegmeCam GUI (GPU) from inside the MediaPipe repo using `bazel run`.
# This guarantees runfiles are set up so the model is found.

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
# Allow optional --rebuild flag to force a fresh build
REBUILD=false
for arg in "$@"; do
  if [[ "$arg" == "--rebuild" ]]; then
    REBUILD=true
  fi
done
MP_DIR="$ROOT_DIR/external/mediapipe"

if [[ ! -d "$MP_DIR/.git" ]]; then
  echo "MediaPipe repo not found at $MP_DIR" >&2
  exit 1
fi

# Ensure model is placed where MP graphs expect it (copy, not symlink)
LOCAL_MODEL="$ROOT_DIR/models/selfie_segmenter.tflite"
MP_MODEL_DIR="$MP_DIR/mediapipe/modules/selfie_segmentation"
MP_MODEL_PATH="$MP_MODEL_DIR/selfie_segmentation.tflite"
if [[ -f "$LOCAL_MODEL" ]]; then
  mkdir -p "$MP_MODEL_DIR"
  if ! cmp -s "$LOCAL_MODEL" "$MP_MODEL_PATH" 2>/dev/null; then
    echo "Copying model to $MP_MODEL_PATH"
    cp -f "$LOCAL_MODEL" "$MP_MODEL_PATH"
  fi
fi

cd "$MP_DIR"

# Choose Bazel launcher: prefer local Bazelisk, then system bazelisk, then bazel
BAZEL="$MP_DIR/.bazelisk"
if [[ ! -x "$BAZEL" ]]; then
  if command -v bazelisk >/dev/null 2>&1; then
    BAZEL=$(command -v bazelisk)
  else
    BAZEL=$(command -v bazel)
  fi
fi

# Use absolute path for the graph so it resolves regardless of Bazel runfiles CWD
GRAPH_PATH="$ROOT_DIR/mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt"

# Ensure OpenCV is discoverable inside Bazel sandbox
DEFAULT_PKG_PATH="/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig"
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}:${DEFAULT_PKG_PATH}"

echo "Running SegmeCam GUI via: $BAZEL run (single build) ..."
if [[ "$REBUILD" == true ]]; then
  echo "Forcing Bazel clean --expunge before run (this may take a while)..."
  "$BAZEL" clean --expunge || true
fi

"$BAZEL" run -c opt \
  --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH \
  --cxxopt=-I/usr/include/opencv4 \
  mediapipe/examples/desktop/segmecam_gui_gpu:segmecam_gui_gpu -- \
  "$GRAPH_PATH" . 0
