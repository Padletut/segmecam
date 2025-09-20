#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Building SegmeCam Flatpak (build natively, copy to project root)..."

# Build the native SegmeCam binary first
echo "Building native SegmeCam binary..."
export ALLOW_BAZEL=1
bazel build -c opt --action_env=PKG_CONFIG_PATH --repo_env=PKG_CONFIG_PATH --cxxopt=-I/usr/include/opencv4 //mediapipe/examples/desktop/segmecam:segmecam

# Copy the binary to project root for easy access
echo "Copying binary to project root..."
rm -f ./segmecam  # Remove existing binary if it exists
cp bazel-bin/mediapipe/examples/desktop/segmecam/segmecam ./segmecam
chmod +w ./segmecam  # Make writable for Flatpak stripping
echo "✅ Binary available at: ./segmecam"

# Build and install exactly as we did before
echo "Building Flatpak package..."
#flatpak-builder --repo=repo --force-clean --disable-cache build-dir-final org.segmecam.SegmeCam.final.yml
flatpak-builder --repo=repo --force-clean build-dir-final org.segmecam.SegmeCam.final.yml


echo "Uninstalling existing package..."
flatpak uninstall --user org.segmecam.SegmeCam -y || true

echo "Installing new package..."
flatpak install --user ./repo org.segmecam.SegmeCam -y

echo "✅ Build complete! Run with:"
echo "flatpak run org.segmecam.SegmeCam"
#flatpak install --user segmecam-local org.segmecam.SegmeCam -y

echo "Installing debug symbols..."
flatpak install --user --reinstall --include-sdk --include-debug ./repo org.segmecam.SegmeCam -y || true

echo "✅ SegmeCam Flatpak built and installed!"
echo ""
echo "🧪 Test commands:"
echo "  Basic segmentation: flatpak run org.segmecam.SegmeCam"
echo "  Face landmarks:     flatpak run org.segmecam.SegmeCam --face"
echo "  Verbose output:     flatpak run --verbose org.segmecam.SegmeCam --face"
