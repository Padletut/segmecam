#!/usr/bin/env bash
set -euo pipefail

# SegmeCam Flatpak full build & bundle script
# Builds native, prepares models/libs, builds Flatpak, exports, and bundles

# 1. Build native binary and dependencies
./scripts/run_segmecam_gui_gpu.sh --face

# 2. Copy all required libraries and models
./ldd-copy-all.sh
./copy-mediapipe-models.sh

# 3. Build Flatpak
flatpak-builder --force-clean build-dir org.segmecam.SegmeCam.final.yml

# 4. Export Flatpak repo
flatpak build-export repo build-dir

# 5. Create distributable bundle
flatpak build-bundle repo segmecam.flatpak org.segmecam.SegmeCam

echo "\n✅ SegmeCam Flatpak bundle created: segmecam.flatpak"
echo "Install with: flatpak install --user segmecam.flatpak"
echo "Run with: flatpak run org.segmecam.SegmeCam"
