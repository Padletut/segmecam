#!/usr/bin/env bash
set -euo pipefail

# Copy all .tflite models from Bazel runfiles, models/, and mediapipe/modules/ into mediapipe_runfiles/mediapipe/modules/ for Flatpak

SRC_ROOT="$(pwd)"
DEST_ROOT="mediapipe_runfiles/mediapipe/modules"

# Ensure destination exists
mkdir -p "$DEST_ROOT"

# 1. Copy from Bazel runfiles if present (preferred, preserves structure)
BAZEL_RUNFILES="bazel-bin/mediapipe/examples/desktop/segmecam/segmecam_gui_gpu.runfiles/mediapipe/modules"
if [ -d "$BAZEL_RUNFILES" ]; then
  echo "Copying .tflite models from Bazel runfiles..."
  find "$BAZEL_RUNFILES" -type f -name '*.tflite' | while read -r model; do
    relpath="${model#$BAZEL_RUNFILES/}"
    # Don't copy selfie_segmenter.tflite from runfiles, always use models/ version
    if [[ "$relpath" == *"selfie_segmentation/selfie_segmentation.tflite" ]]; then
      continue
    fi
    destpath="$DEST_ROOT/$relpath"
    mkdir -p "$(dirname "$destpath")"
    cp -v "$model" "$destpath"
  done
else
  echo "No Bazel runfiles found at $BAZEL_RUNFILES, falling back to models/ and mediapipe/modules/"
fi

# 2. Always copy selfie_segmenter.tflite from models/ to selfie_segmentation/selfie_segmentation.tflite
if [ -f "$SRC_ROOT/models/selfie_segmenter.tflite" ]; then
  mkdir -p "$DEST_ROOT/selfie_segmentation"
  cp -v "$SRC_ROOT/models/selfie_segmenter.tflite" "$DEST_ROOT/selfie_segmentation/selfie_segmentation.tflite"
fi

# 3. Copy all .tflite files from models/ to the correct module subdir if possible (for manual/extra models)
find "$SRC_ROOT/models" -type f -name '*.tflite' 2>/dev/null | while read -r model; do
  fname=$(basename "$model")
  case "$fname" in
    selfie_segmenter.tflite)
      # already handled above
      continue ;;
    face_detection_short_range.tflite)
      moddir="$DEST_ROOT/face_detection"; modname="face_detection_short_range.tflite" ;;
    face_landmark_with_attention.tflite)
      moddir="$DEST_ROOT/face_landmark"; modname="face_landmark_with_attention.tflite" ;;
    *)
      moddir="$DEST_ROOT"; modname="$fname" ;;
  esac
  mkdir -p "$moddir"
  cp -v "$model" "$moddir/$modname"
done

# 4. Recursively copy all .tflite files from mediapipe/modules/ to mediapipe_runfiles/mediapipe/modules/ (preserving structure)
if [ -d "$SRC_ROOT/mediapipe/modules" ]; then
  find "$SRC_ROOT/mediapipe/modules" -type f -name '*.tflite' | while read -r model; do
    relpath="${model#$SRC_ROOT/mediapipe/modules/}"
    destpath="$DEST_ROOT/$relpath"
    mkdir -p "$(dirname "$destpath")"
    cp -v "$model" "$destpath"
  done
fi

echo "✅ All .tflite models copied to $DEST_ROOT/"
