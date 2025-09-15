#!/usr/bin/env bash
set -euo pipefail

# Clone MediaPipe from google-ai-edge and build the selfie_segmentation GPU desktop example.
# Requires system deps (OpenGL/EGL, protobuf, etc.). This script uses Bazel.

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
MP_DIR="$ROOT_DIR/mediapipe"

# Save SegmeCam files before cloning (if they exist)
SEGMECAM_BACKUP_DIR="/tmp/segmecam_backup_$$"
TRACKED_SEGMECAM_DIR="$ROOT_DIR/mediapipe/examples/desktop/segmecam"
if [[ -d "$TRACKED_SEGMECAM_DIR" ]]; then
  echo "Backing up existing SegmeCam source files..."
  mkdir -p "$SEGMECAM_BACKUP_DIR"
  cp -rf "$TRACKED_SEGMECAM_DIR" "$SEGMECAM_BACKUP_DIR/"
fi

if [[ ! -d "$MP_DIR/.git" ]]; then
  echo "Cloning MediaPipe from google-ai-edge..."
  git clone --depth 1 https://github.com/google-ai-edge/mediapipe.git "$MP_DIR"
else
  echo "MediaPipe already cloned at $MP_DIR"
fi

# Restore SegmeCam files after cloning
if [[ -d "$SEGMECAM_BACKUP_DIR/segmecam" ]]; then
  echo "Restoring SegmeCam source files..."
  mkdir -p "$MP_DIR/mediapipe/examples/desktop"
  cp -rf "$SEGMECAM_BACKUP_DIR/segmecam" "$MP_DIR/mediapipe/examples/desktop/"
  rm -rf "$SEGMECAM_BACKUP_DIR"
  echo "Restored SegmeCam source files"
fi

cd "$MP_DIR"

echo "Checking OpenCV via pkg-config..."
if ! pkg-config --exists opencv4; then
  echo "Error: pkg-config cannot find opencv4. Ensure OpenCV is installed and opencv4.pc is discoverable." >&2
  echo "Hint: set PKG_CONFIG_PATH to include your opencv4.pc directory (e.g. /usr/local/lib/pkgconfig)." >&2
  exit 2
fi
echo "OpenCV version: $(pkg-config --modversion opencv4)"
echo "OpenCV cflags:  $(pkg-config --cflags opencv4)"
echo "OpenCV libs:    $(pkg-config --libs opencv4)"

# Ensure Bazel sandbox sees custom pkg-config locations (e.g., /usr/local/lib/pkgconfig)
DEFAULT_PKG_PATH="/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig"
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}:${DEFAULT_PKG_PATH}"
echo "Using PKG_CONFIG_PATH=${PKG_CONFIG_PATH}"

# Place local model (if present) at the path expected by MediaPipe graphs
LOCAL_MODEL="$ROOT_DIR/models/selfie_segmenter.tflite"
MP_MODEL_DIR="$MP_DIR/mediapipe/modules/selfie_segmentation"
MP_MODEL_PATH="$MP_MODEL_DIR/selfie_segmentation.tflite"
echo
echo "The model is at $LOCAL_MODEL"
if [[ -f "$LOCAL_MODEL" ]]; then
  mkdir -p "$MP_MODEL_DIR"
  # Copy instead of symlink to satisfy Bazel runfiles and avoid symlink-out-of-workspace issues
  cp -f "$LOCAL_MODEL" "$MP_MODEL_PATH"
  echo "Copied model to $MP_MODEL_PATH"
else
  echo "Note: $LOCAL_MODEL not found; graphs will try to download or you can place it manually." >&2
fi

# Copy SegmeCam graphs
SEGMECAM_GRAPHS_DIR="$ROOT_DIR/mediapipe_graphs"
MP_GRAPHS_DIR="$MP_DIR/mediapipe_graphs"
if [[ -d "$SEGMECAM_GRAPHS_DIR" ]]; then
  echo "Copying SegmeCam graph files..."
  mkdir -p "$MP_GRAPHS_DIR"
  cp -rf "$SEGMECAM_GRAPHS_DIR"/* "$MP_GRAPHS_DIR/"
  echo "Copied SegmeCam graphs to $MP_GRAPHS_DIR"
fi

echo "Preparing Bazelisk (to honor .bazelversion)..."
BAZELISK="$MP_DIR/.bazelisk"
if [[ ! -x "$BAZELISK" ]]; then
  curl -L --fail -o "$BAZELISK" https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-amd64
  chmod +x "$BAZELISK"
fi

echo "Building selfie_segmentation GPU example with Bazelisk..."
"$BAZELISK" build -c opt --define MEDIAPIPE_DISABLE_GPU=0 \
  --action_env=PKG_CONFIG_PATH="$PKG_CONFIG_PATH" \
  --repo_env=PKG_CONFIG_PATH="$PKG_CONFIG_PATH" \
  --cxxopt=-I/usr/include/opencv4 \
  mediapipe/examples/desktop/selfie_segmentation:selfie_segmentation_gpu

echo "Building SegmeCam GUI GPU demo (CPU mask output)..."
"$BAZELISK" build -c opt --define MEDIAPIPE_DISABLE_GPU=0 \
  --action_env=PKG_CONFIG_PATH="$PKG_CONFIG_PATH" \
  --repo_env=PKG_CONFIG_PATH="$PKG_CONFIG_PATH" \
  --cxxopt=-I/usr/include/opencv4 \
  mediapipe/examples/desktop/segmecam_gui_gpu:segmecam_gui_gpu

echo "Built: bazel-bin/mediapipe/examples/desktop/selfie_segmentation/selfie_segmentation_gpu"
echo "Built: bazel-bin/mediapipe/examples/desktop/segmecam_gui_gpu/segmecam_gui_gpu"
