#!/usr/bin/env bash

set -euo pipefail

# Ensure all binary dependencies are copied before building Flatpak
echo "🔄 Running ldd-copy-all.sh to gather dependencies..."
./ldd-copy-all.sh
echo "✅ Dependencies copied."

# Ensure all model files are copied before building Flatpak
echo "🔄 Running copy-mediapipe-models.sh to gather model files..."
./copy-mediapipe-models.sh
echo "✅ Model files copied."

echo "📦 Creating Flatpak bundle for distribution..."

# Build the Flatpak if not already built
if [ ! -d "build-dir-final" ]; then
    echo "Building Flatpak first..."
    flatpak-builder --user --install --force-clean build-dir-final org.segmecam.SegmeCam.final.yml
fi

# Create a repository and export the app
echo "Creating repository..."
flatpak-builder --repo=repo --force-clean build-dir-final org.segmecam.SegmeCam.final.yml

# Create a bundle file for distribution
echo "Creating .flatpak bundle..."
flatpak build-bundle repo segmecam.flatpak org.segmecam.SegmeCam

echo "✅ Bundle created: segmecam.flatpak"
echo "📤 Upload this file to GitHub Releases"
echo ""
echo "Users can install with:"
echo "  flatpak install segmecam.flatpak"
echo ""
echo "Bundle size:"
ls -lh segmecam.flatpak