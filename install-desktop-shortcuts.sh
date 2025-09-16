#!/bin/bash
# SegmeCam Desktop Integration Setup

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APPS_DIR="$HOME/.local/share/applications"

echo "🖥️  Installing SegmeCam desktop shortcuts..."

# Create applications directory
mkdir -p "$APPS_DIR"

# Copy desktop files
echo "📁 Copying desktop files to $APPS_DIR"
cp "$SCRIPT_DIR/SegmeCam.desktop" "$APPS_DIR/"
cp "$SCRIPT_DIR/SegmeCam-Face.desktop" "$APPS_DIR/"

# Make them executable
chmod +x "$APPS_DIR/SegmeCam.desktop"
chmod +x "$APPS_DIR/SegmeCam-Face.desktop"

# Update desktop database
echo "🔄 Updating desktop database..."
update-desktop-database "$APPS_DIR" 2>/dev/null || true

echo "✅ Desktop shortcuts installed successfully!"
echo ""
echo "You can now find SegmeCam in your application menu under:"
echo "  📸 AudioVideo → Photography"
echo ""
echo "Available shortcuts:"
echo "  • SegmeCam - Basic selfie segmentation"
echo "  • SegmeCam (Face Landmarks) - With face detection"
echo ""
echo "To uninstall, run: rm ~/.local/share/applications/SegmeCam*.desktop"