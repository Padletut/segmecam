#!/usr/bin/env bash
export HERMETIC_PYTHON_VERSION=3.12
set -euo pipefail

# Allow bazel calls from this script (bypasses tools/bazel shim)
export ALLOW_BAZEL=1

# Runs the SegmeCam Modular Application
# This replaces the monolithic version with the new Phase 1-7 modular architecture.

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
# Flags:
# --rebuild           Force a clean rebuild
# --face              Use combined face+seg graph (default: selfie segmentation only)
# --graph <path>      Use a specific graph path
# --tasks             Use Tasks Face Landmarker + Seg graph
REBUILD=false
USE_FACE=false
USE_TASKS=false
CUSTOM_GRAPH=""
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --rebuild) REBUILD=true; shift ;;
    --face) USE_FACE=true; shift ;;
    --graph) CUSTOM_GRAPH="$2"; shift 2 ;;
    --tasks) USE_TASKS=true; shift ;;
    *) ARGS+=("$1"); shift ;;
  esac
done
MP_DIR="$ROOT_DIR"

if [[ ! -d "$MP_DIR/mediapipe" ]]; then
  echo "MediaPipe repo not found at $MP_DIR" >&2
  echo "Please run scripts/mediapipe_build_selfie_seg_gpu.sh first." >&2
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

# Resolve graph path
DEFAULT_GRAPH="$ROOT_DIR/mediapipe_graphs/selfie_seg_gpu_mask_cpu.pbtxt"
FACE_GRAPH="$ROOT_DIR/mediapipe_graphs/face_and_seg_gpu_mask_cpu.pbtxt"
TASKS_GRAPH="$ROOT_DIR/mediapipe_graphs/face_tasks_and_seg_gpu_mask_cpu.pbtxt"
if [[ -n "$CUSTOM_GRAPH" ]]; then
  GRAPH_PATH="$CUSTOM_GRAPH"
elif [[ "$USE_TASKS" == true ]]; then
  GRAPH_PATH="$TASKS_GRAPH"
elif [[ "$USE_FACE" == true ]]; then
  GRAPH_PATH="$FACE_GRAPH"
else
  GRAPH_PATH="$DEFAULT_GRAPH"
fi

# Choose which application to run - now only the modular version
APP_TARGET="mediapipe/examples/desktop/segmecam:segmecam"
APP_NAME="SegmeCam Beta v1.0.0"

# Ensure OpenCV is discoverable inside Bazel sandbox
DEFAULT_PKG_PATH="/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig"
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}:${DEFAULT_PKG_PATH}"

echo "Running $APP_NAME via: $BAZEL run ..."
echo "Graph: $GRAPH_PATH"
if [[ "$REBUILD" == true ]]; then
  echo "Forcing Bazel clean --expunge before run (this may take a while)..."
  "$BAZEL" clean --expunge || true
fi

# Compute runfiles dir path for resource loading  
BIN_RUNFILES="$MP_DIR/bazel-bin/mediapipe/examples/desktop/segmecam/segmecam.runfiles"

# Step 1: Run Bazel to fetch dependencies (if needed)
#echo "Running Bazel fetch to ensure dependencies are downloaded..."
#"$BAZEL" fetch "$APP_TARGET" || true

# Step 2: Run Bazel build and execute the modular application
"$BAZEL" run -c opt \
  --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH \
  --cxxopt=-I/usr/include/opencv4 \
  "$APP_TARGET" -- \
  --graph_path="$GRAPH_PATH" \
  --resource_root_dir="$BIN_RUNFILES" \
  --camera_id=0
