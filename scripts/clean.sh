#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)

echo "Cleaning local build artifacts..."
rm -f "$ROOT_DIR/app" "$ROOT_DIR/segmecam_gui" || true
rm -rf "$ROOT_DIR/bazel-*" || true

echo "Optionally remove fetched externals (uncomment if desired):"
echo "  rm -rf '$ROOT_DIR/external/imgui'"
echo "  rm -rf '$ROOT_DIR/external/mediapipe'  # large clone"
echo "  rm -rf '$ROOT_DIR/third_party/tflite_c/lib'"
echo "Done."

