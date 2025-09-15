#!/usr/bin/env bash
set -euo pipefail

echo "🚀 Building SegmeCam Flatpak..."

# Install Flatpak and flatpak-builder if not present
if ! command -v flatpak-builder >/dev/null 2>&1; then
    echo "Installing flatpak-builder..."
    sudo apt install -y flatpak-builder
fi

# Add Flathub repo if not present
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# Install SDK and runtime
flatpak install -y flathub org.freedesktop.Platform//23.08 org.freedesktop.Sdk//23.08

# Build the Flatpak
echo "Building Flatpak package..."
flatpak-builder --force-clean build-dir org.segmecam.SegmeCam.final.yml

# Install locally for testing
echo "Installing locally for testing..."
flatpak-builder --user --install --force-clean build-dir org.segmecam.SegmeCam.final.yml

# Install with debug symbols
echo "Installing debug symbols..."
flatpak install --user --include-debug org.segmecam.SegmeCam

echo "✅ SegmeCam Flatpak built and installed!"
echo "Run with: flatpak run org.segmecam.SegmeCam"