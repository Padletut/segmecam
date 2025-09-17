# SegmeCam Flatpak Build & Bundle Guide

This guide explains how to build, export, and bundle SegmeCam as a Flatpak for easy distribution.

## Prerequisites
- All dependencies built (native binary, models, libraries)
- Flatpak and flatpak-builder installed
- Sufficient disk space (5GB+ recommended)

## Step-by-step Instructions

1. **Build SegmeCam and dependencies**
   ```bash
   ./scripts/run_segmecam_gui_gpu.sh --face
   # Or your preferred build script
   ```

2. **Prepare models and libraries**
   ```bash
   ./ldd-copy-all.sh
   ./copy-mediapipe-models.sh
   # Ensure all required .tflite models are in /models
   ```

3. **Build the Flatpak**
   ```bash
   flatpak-builder --force-clean build-dir org.segmecam.SegmeCam.final.yml
   ```

4. **Export the Flatpak repo**
   ```bash
   flatpak build-export repo build-dir
   ```

5. **Create the distributable bundle**
   ```bash
   flatpak build-bundle repo segmecam.flatpak org.segmecam.SegmeCam
   ```

6. **Install and test locally**
   ```bash
   flatpak install --user segmecam.flatpak
   flatpak run org.segmecam.SegmeCam
   ```

## Notes
- If you add new models, update the sources and build-commands in `org.segmecam.SegmeCam.final.yml`.
- For troubleshooting, check Flatpak logs and ensure all model files are present in `/app/mediapipe_runfiles/mediapipe/modules/` inside the Flatpak.

---

## One-command build
See `build-flatpak-bundle.sh` for a fully automated workflow.
