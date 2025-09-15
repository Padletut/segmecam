#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Building Simple SegmeCam Flatpak (pre-built binaries approach)..."

# Save current branch
CURRENT_BRANCH=$(git branch --show-current)

# First, let's build SegmeCam normally to get the binaries
echo "Step 1: Building SegmeCam locally..."
git stash || true
git checkout main

# Build MediaPipe and SegmeCam
./scripts/mediapipe_build_selfie_seg_gpu.sh
bazel build //...

echo "✅ Local build complete!"

# Switch back to flatpak branch
git checkout $CURRENT_BRANCH
git stash pop || true

# Check if we have the binaries
if [ ! -f "bazel-bin/segmecam_gui" ]; then
    echo "❌ segmecam_gui binary not found!"
    echo "Make sure the build completed successfully."
    exit 1
fi

echo "Step 2: Setting up Flatpak build environment..."

# Install Flatpak and flatpak-builder if not present
if ! command -v flatpak-builder >/dev/null 2>&1; then
    echo "Installing flatpak-builder..."
    sudo apt install -y flatpak-builder
fi

# Add Flathub repo if not present
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# Install SDK and runtime
flatpak install -y flathub org.freedesktop.Platform//23.08 org.freedesktop.Sdk//23.08

echo "Step 3: Building Flatpak with pre-built binaries..."

# Build the Flatpak using pre-built binaries
flatpak-builder --force-clean build-dir org.segmecam.SegmeCam.prebuilt.yml

echo "Step 4: Installing locally for testing..."

# Install locally for testing  
flatpak-builder --user --install --force-clean build-dir org.segmecam.SegmeCam.prebuilt.yml

echo "✅ SegmeCam Flatpak built and installed successfully!"
echo ""
echo "🎉 Test it with:"
echo "  flatpak run org.segmecam.SegmeCam"
echo ""
echo "📋 To export for distribution:"
echo "  flatpak build-export repo build-dir"
echo "  flatpak build-bundle repo segmecam.flatpak org.segmecam.SegmeCam"